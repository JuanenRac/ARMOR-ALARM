// ARMOR-ALARM - the alarm at work: the task that reads the wired zones and runs core/alarm_runner.hpp. Nothing here has run on a board.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// What the panel decides (the states of a loop, the delays, the PIN, the siren) is in core/ and is tested on a computer; this file only reads the ADC and the digital pins (the contacts and
// the buttons: switches to ground, the pins pulled up), gives the runner the clock, puts the
// siren's level on its relay and talks to the broker. Only ADC1 is used (the converter shared with the Wi-Fi radio is never touched), 0 to about 3.1 V at the pin (a cut wire reads about 3.1 V, so the loop limits stop at 3 V): a zone's loop must never put
// more than 3.3 V there. A pin that cannot be read says nothing - it is never taken for a tamper, because a fault of the node must not sound the siren.
#include "alarm_manager.hpp"

#include <memory>
#include <mutex>
extern "C" {
#include "driver/gpio.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_task_wdt.h"
#include "nvs.h"
#include "nvs_flash.h"
}
#include "clock_sync.hpp"
#include "core/local_zigbee.hpp"
#include "entropy.hpp"
#include "mqtt_link.hpp"
#include "node_control.hpp"
#include "relay_outputs.hpp"
#include "zigbee_bridge.hpp"

namespace armor::manager {
namespace {
constexpr char kTag[] = "armor-alarm";
constexpr unsigned kStepMs = 100;                 // a turn of the loop
constexpr std::uint64_t kPublishEveryMs = 5000;   // the state is said again this often, and at every change
constexpr int kSamples = 8;                       // readings averaged for one value of a zone

std::mutex g_lock;
std::unique_ptr<alarm::Runner> g_runner;
config::Settings g_settings;
Publish g_publish;
std::string g_error;
int g_siren_relay = -1;
bool g_siren_on = false;
std::string g_last_signature;
std::uint64_t g_last_publish_ms = 0;

// What a restart must not forget (see alarm::Panel::saved): the mode, a running alarm and the zones left out are kept in flash whenever they change and taken back at start, so that a
// power cut, a reset by the watchdog or an update leaves the house guarded, not open. The text in flash is the one line core/alarm_panel.hpp writes and reads.
constexpr char kStateNamespace[] = "alarm_state";
constexpr char kStateKey[] = "panel";
std::string g_saved_text;   // what is in flash now
constexpr char kEventsKey[] = "events";
constexpr char kProfileKey[] = "profile";   // the arming profile the panel is armed with (JSON, as in the arm command), or "" for none
std::string g_saved_profile;               // what is in flash now
bool g_forgotten = false;   // a factory reset has erased the saved state: nothing is saved again before the restart
std::uint64_t g_last_events_write_ms = 0;
constexpr std::uint64_t kEventsWriteEveryMs = 30000;
std::string g_saved_events;   // the events in flash now (the last few that happened while the node had real time, so a restart does not forget what happened)

// The wall-clock second, or 0 while the node has no real time.
std::int64_t wall_epoch() { return clocksync::is_set() ? clocksync::info().epoch : 0; }

bool read_events(std::string& out) {
  nvs_handle_t handle;
  if (nvs_open(kStateNamespace, NVS_READONLY, &handle) != ESP_OK) return false;
  std::size_t size = 0;
  bool ok = false;
  if (nvs_get_str(handle, kEventsKey, nullptr, &size) == ESP_OK && size > 0 && size <= 1024) {
    out.resize(size);
    ok = nvs_get_str(handle, kEventsKey, out.data(), &size) == ESP_OK;
    if (ok) out.resize(size - 1);
  }
  nvs_close(handle);
  return ok;
}

bool write_events(const std::string& text) {
  nvs_handle_t handle;
  if (nvs_open(kStateNamespace, NVS_READWRITE, &handle) != ESP_OK) return false;
  const bool ok = nvs_set_str(handle, kEventsKey, text.c_str()) == ESP_OK && nvs_commit(handle) == ESP_OK;
  nvs_close(handle);
  return ok;
}

bool read_saved(std::string& out) {
  nvs_handle_t handle;
  if (nvs_open(kStateNamespace, NVS_READONLY, &handle) != ESP_OK) return false;
  std::size_t size = 0;
  bool ok = false;
  if (nvs_get_str(handle, kStateKey, nullptr, &size) == ESP_OK && size > 0 && size <= 512) {
    out.resize(size);
    ok = nvs_get_str(handle, kStateKey, out.data(), &size) == ESP_OK;
    if (ok) out.resize(size - 1);
  }
  nvs_close(handle);
  return ok;
}

bool read_profile(std::string& out) {
  nvs_handle_t handle;
  if (nvs_open(kStateNamespace, NVS_READONLY, &handle) != ESP_OK) return false;
  std::size_t size = 0;
  bool ok = false;
  if (nvs_get_str(handle, kProfileKey, nullptr, &size) == ESP_OK && size > 0 && size <= 2048) {
    out.resize(size);
    ok = nvs_get_str(handle, kProfileKey, out.data(), &size) == ESP_OK;
    if (ok) out.resize(size - 1);
  }
  nvs_close(handle);
  return ok;
}

bool write_profile(const std::string& text) {
  nvs_handle_t handle;
  if (nvs_open(kStateNamespace, NVS_READWRITE, &handle) != ESP_OK) return false;
  const bool ok = nvs_set_str(handle, kProfileKey, text.c_str()) == ESP_OK && nvs_commit(handle) == ESP_OK;
  nvs_close(handle);
  return ok;
}

bool write_saved(const std::string& text) {
  nvs_handle_t handle;
  if (nvs_open(kStateNamespace, NVS_READWRITE, &handle) != ESP_OK) return false;
  const bool ok = nvs_set_str(handle, kStateKey, text.c_str()) == ESP_OK && nvs_commit(handle) == ESP_OK;
  nvs_close(handle);
  return ok;
}

// The node speaking Zigbee itself while the radio is its own (see zigbee_bridge.hpp): the bridge's task drives it; the alarm task takes what the sensors said out of it and tells it when the siren
// is on. It has its own lock because those are two tasks.
class ZigbeeHost : public zigbee_bridge::LocalHost {
 public:
  explicit ZigbeeHost(const config::Settings& settings) : z_(sensors_of(settings), sirens_of(settings)) {}
  void begin(std::uint64_t now_ms) override { std::lock_guard<std::mutex> guard(m_); active_ = true; pending_.clear(); z_.begin(now_ms); }
  void end() override { std::lock_guard<std::mutex> guard(m_); active_ = false; pending_.clear(); z_.end(); }
  void feed(const std::uint8_t* bytes, std::size_t count, std::uint64_t now_ms) override { std::lock_guard<std::mutex> guard(m_); z_.feed(bytes, count, now_ms); }
  void tick(std::uint64_t now_ms) override { std::lock_guard<std::mutex> guard(m_); z_.tick(now_ms); }
  std::size_t take_output(std::uint8_t* out, std::size_t capacity) override {
    std::lock_guard<std::mutex> guard(m_);
    const std::vector<std::uint8_t> more = z_.take_output();
    pending_.insert(pending_.end(), more.begin(), more.end());
    const std::size_t count = pending_.size() < capacity ? pending_.size() : capacity;
    for (std::size_t i = 0; i < count; ++i) out[i] = pending_[i];
    pending_.erase(pending_.begin(), pending_.begin() + static_cast<std::ptrdiff_t>(count));
    return count;
  }
  std::vector<zigbee::ZoneUpdate> updates() { std::lock_guard<std::mutex> guard(m_); return z_.take_updates(); }
  bool ready() { std::lock_guard<std::mutex> guard(m_); return active_ && z_.ready(); }
  void siren(bool on, unsigned seconds) { std::lock_guard<std::mutex> guard(m_); if (active_ && z_.ready()) z_.siren(on, static_cast<std::uint16_t>(seconds > 65535 ? 65535 : seconds)); }

 private:
  static std::vector<zigbee::SensorConfig> sensors_of(const config::Settings& s) {
    std::vector<zigbee::SensorConfig> out;
    for (const config::ZoneSetting& zone : s.zones) {
      std::uint64_t ieee = 0;
      if (zone.enabled && zone.is_zigbee() && zigbee::parse_ieee(zone.zigbee, ieee)) out.push_back({zone.id, ieee});
    }
    return out;
  }
  static std::vector<zigbee::SirenConfig> sirens_of(const config::Settings& s) {
    std::vector<zigbee::SirenConfig> out;
    for (const std::string& text : s.zigbee_local.sirens) {
      std::uint64_t ieee = 0;
      if (!text.empty() && zigbee::parse_ieee(text, ieee)) out.push_back({ieee});
    }
    return out;
  }
  std::mutex m_;
  zigbee::LocalZigbee z_;
  std::vector<std::uint8_t> pending_;
  bool active_ = false;
};
std::unique_ptr<ZigbeeHost> g_host;
bool g_zigbee_siren_told = false;

std::uint64_t now_ms() { return static_cast<std::uint64_t>(esp_timer_get_time()) / 1000ULL; }
std::uint64_t stamp_ms() { return mqtt_link::clock_is_set() ? mqtt_link::wall_clock_ms() : now_ms(); }

// What changes the message: the panel's phase, mode and siren and the state of each zone. Called with the lock held.
std::string signature() {
  std::string text = std::string(alarm::phase_text(g_runner->panel().phase())) + alarm::mode_text(g_runner->panel().mode()) + (g_runner->siren() ? "S" : "s") + (g_runner->locked_out(now_ms()) ? "L" : "l");
  if (g_runner->profile() != nullptr) text += "P" + g_runner->profile()->id;
  for (const alarm::ZoneView& zone : g_runner->zones()) if (zone.enabled) text += std::string(alarm::state_text(zone.state)) + (zone.bypassed ? "b" : "-");
  return text;
}

void publish_state_locked(bool force) {
  if (!g_publish) return;
  const std::uint64_t now = now_ms();
  const std::string now_signature = signature();
  if (!force && now_signature == g_last_signature && now - g_last_publish_ms < kPublishEveryMs) return;
  g_last_signature = now_signature;
  g_last_publish_ms = now;
  g_publish(alarm::topic(g_settings.node_id, "state"), g_runner->state_json(stamp_ms(), now));
}

// The siren is on while the panel says so. The relay is the alarm's alone (see relay_outputs::reserve_for_siren) and its pin is made to hold the level on every turn, so a disturbed pin
// is put back; the log hears of changes only.
void drive_siren_locked() {
  const bool wanted = g_runner->siren();
  if (g_siren_relay >= 0) relay_outputs::siren(wanted);
  if (wanted == g_siren_on) return;
  g_siren_on = wanted;
  ESP_LOGW(kTag, "the siren is %s", wanted ? "ON" : "off");
}

void on_command(const std::string&, const std::string& payload) {
  if (!node_control::working()) { ESP_LOGW(kTag, "a command of the server was ignored: the node's work is off (paused or in standby), the house is NOT protected"); return; }
  std::lock_guard<std::mutex> guard(g_lock);
  if (!g_runner) return;
  const alarm::Reply reply = g_runner->handle(payload, now_ms());
  if (!reply.understood) { ESP_LOGW(kTag, "a message on the command topic was ignored: it is not the contract's command"); return; }
  ESP_LOGW(kTag, "the server said %s%s%s: %s", reply.command.action == alarm::Action::kArm ? "arm" : reply.command.action == alarm::Action::kSilence ? "silence" : "disarm",
           reply.command.has_profile ? " with the profile " : "", reply.command.has_profile ? reply.command.profile.id.c_str() : "", alarm::refusal_text(reply.refusal));
  if (g_publish) g_publish(alarm::topic(g_settings.node_id, "result"), g_runner->result_json(stamp_ms(), reply));
  drive_siren_locked();
  publish_state_locked(true);
}

// The server tells the panel how its Zigbee zones are while it holds the radio (information, never a command: it is read even when the server may not arm the panel).
void on_zone_feed(const std::string&, const std::string& payload) {
  std::lock_guard<std::mutex> guard(g_lock);
  if (g_runner) g_runner->handle_zone_feed(payload, now_ms());
}

void alarm_task(void*) {
  adc_oneshot_unit_handle_t unit = nullptr;
  adc_oneshot_unit_init_cfg_t unit_config{};
  unit_config.unit_id = ADC_UNIT_1;
  std::array<adc_cali_handle_t, config::kZoneCount> calibration{};
  std::array<bool, config::kZoneCount> ready{};
  bool any_zone = false;
  for (const config::ZoneSetting& zone : g_settings.zones) any_zone = any_zone || (zone.enabled && !zone.is_zigbee() && !zone.is_digital());   // a Zigbee zone has no pin, a digital one is not read by the ADC
  // the digital pins: inputs with the internal pull-up (a closed contact pulls the pin to ground)
  const auto input_with_pullup = [](int pin, const char* what) {
    gpio_config_t io{};
    io.pin_bit_mask = 1ULL << pin;
    io.mode = GPIO_MODE_INPUT;
    io.pull_up_en = GPIO_PULLUP_ENABLE;
    if (gpio_config(&io) != ESP_OK) { ESP_LOGE(kTag, "GPIO %d could not be set up as the input of %s", pin, what); return false; }
    return true;
  };
  std::array<bool, config::kZoneCount> digital_ready{};
  std::array<bool, config::kButtonCount> button_ready{};
  for (std::size_t i = 0; i < config::kZoneCount; ++i) if (g_settings.zones[i].enabled && g_settings.zones[i].is_digital()) digital_ready[i] = input_with_pullup(g_settings.zones[i].pin, "a zone");
  for (std::size_t i = 0; i < config::kButtonCount; ++i) if (g_settings.buttons[i].enabled) button_ready[i] = input_with_pullup(g_settings.buttons[i].pin, "a button");
  if (any_zone) {
    if (adc_oneshot_new_unit(&unit_config, &unit) != ESP_OK) {
      ESP_LOGE(kTag, "the converter could not be started: the zones are not read");
      std::lock_guard<std::mutex> guard(g_lock);
      g_error = "adc";
      unit = nullptr;
    } else {
      for (std::size_t i = 0; i < config::kZoneCount; ++i) {
        const config::ZoneSetting& zone = g_settings.zones[i];
        if (!zone.enabled || zone.is_zigbee() || zone.is_digital()) continue;
        const adc_channel_t channel = static_cast<adc_channel_t>(board::adc1_channel(zone.pin));
        adc_oneshot_chan_cfg_t channel_config{};
        channel_config.atten = ADC_ATTEN_DB_12;
        channel_config.bitwidth = ADC_BITWIDTH_DEFAULT;
        adc_cali_curve_fitting_config_t cali{};
        cali.unit_id = ADC_UNIT_1; cali.chan = channel; cali.atten = ADC_ATTEN_DB_12; cali.bitwidth = ADC_BITWIDTH_DEFAULT;
        if (adc_oneshot_config_channel(unit, channel, &channel_config) == ESP_OK && adc_cali_create_scheme_curve_fitting(&cali, &calibration[i]) == ESP_OK) ready[i] = true;
        else { ESP_LOGE(kTag, "GPIO %d could not be set up as a zone's input", zone.pin); std::lock_guard<std::mutex> guard(g_lock); g_error = "adc"; }
      }
    }
  }
  // A task that stops turning (a lock that is never given back, a driver that hangs) must not leave the house unguarded for ever: the watchdog restarts the board, and the saved state
  // brings it back armed.
  esp_task_wdt_add(nullptr);
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(kStepMs));
    esp_task_wdt_reset();
    if (!node_control::working()) {
      // Paused or in standby by the server's order: the zones are not read, nothing is evaluated or armed, no state is published and no siren sounds (the relay is held open by
      // relay_outputs::hold_open and a Zigbee siren is told to stop). The panel's state stays as it was and is taken up again at the resume. The house is NOT protected meanwhile: the
      // node's health report says paused or standby.
      std::lock_guard<std::mutex> guard(g_lock);
      if (g_siren_relay >= 0) relay_outputs::siren(false);
      if (g_siren_on) { g_siren_on = false; ESP_LOGW(kTag, "the siren is off: the node's work is off"); }
      if (g_host && g_host->ready() && g_zigbee_siren_told) { g_host->siren(false, g_settings.panel.timing.siren_max_s); g_zigbee_siren_told = false; }
      continue;
    }
    const std::uint64_t now = now_ms();
    std::array<double, config::kZoneCount> millivolts{};
    std::array<bool, config::kZoneCount> read{};
    if (unit != nullptr) {
      std::lock_guard<std::mutex> adc_guard(armor::adc_lock());   // nobody else touches the converter while the zones are read (see entropy.cpp)
      for (std::size_t i = 0; i < config::kZoneCount; ++i) {
        if (!g_settings.zones[i].enabled || g_settings.zones[i].is_zigbee() || g_settings.zones[i].is_digital() || !ready[i]) continue;
        const adc_channel_t channel = static_cast<adc_channel_t>(board::adc1_channel(g_settings.zones[i].pin));
        long sum = 0;
        int taken = 0;
        for (int k = 0; k < kSamples; ++k) {
          int raw = 0, mv = 0;
          if (adc_oneshot_read(unit, channel, &raw) == ESP_OK && adc_cali_raw_to_voltage(calibration[i], raw, &mv) == ESP_OK) { sum += mv; ++taken; }
        }
        if (taken > 0) { millivolts[i] = static_cast<double>(sum) / taken; read[i] = true; }
      }
    }
    std::string to_save, events_to_save, profile_text;
    bool profile_changed = false;
    {
    std::lock_guard<std::mutex> guard(g_lock);
    g_runner->set_epoch(wall_epoch());
    for (std::size_t i = 0; i < config::kZoneCount; ++i) {
      if (read[i]) g_runner->feed(i, millivolts[i], now);
      else if (g_settings.zones[i].enabled && !g_settings.zones[i].is_zigbee() && !g_settings.zones[i].is_digital()) g_runner->feed_unreadable(i, now);   // a loop nobody can read is not a closed one
    }
    for (std::size_t i = 0; i < config::kZoneCount; ++i) if (digital_ready[i]) g_runner->feed_digital(i, gpio_get_level(static_cast<gpio_num_t>(g_settings.zones[i].pin)) != 0, now);
    bool pressed = false;
    for (std::size_t i = 0; i < config::kButtonCount; ++i) {
      if (!button_ready[i]) continue;
      const alarm::Runner::ButtonPress press = g_runner->feed_button(i, gpio_get_level(static_cast<gpio_num_t>(g_settings.buttons[i].pin)) != 0, now);
      if (!press.fired) continue;
      pressed = true;
      ESP_LOGW(kTag, "button %u: armed %s: %s", static_cast<unsigned>(i + 1), alarm::mode_text(press.mode), alarm::refusal_text(press.refusal));
    }
    if (pressed) publish_state_locked(true);
    if (g_host) {
      for (const zigbee::ZoneUpdate& update : g_host->updates()) g_runner->feed_zigbee(update.zone_id, update.state, update.battery_low, now);
      if (!g_host->ready()) g_zigbee_siren_told = false;                    // a radio that is not ours cannot be told anything; it is told again when it is
      else if (g_siren_on != g_zigbee_siren_told) { g_host->siren(g_siren_on, g_settings.panel.timing.siren_max_s); g_zigbee_siren_told = g_siren_on; }
    }
    g_runner->radio_held(g_host && g_host->ready(), now);
    g_runner->tick(now);
    drive_siren_locked();
    publish_state_locked(false);
    const std::string now_saved = alarm::saved_text(g_runner->saved());
    if (!g_forgotten && now_saved != g_saved_text) to_save = now_saved;
    profile_text = g_runner->profile() != nullptr && g_runner->panel().mode() != alarm::Mode::kDisarmed ? alarm::profile_json(*g_runner->profile()) : std::string();
    profile_changed = !g_forgotten && profile_text != g_saved_profile;
    const std::string now_events = alarm::events_text(g_runner->panel().events());
    // Flash wears with every write: a wrong PIN or a bouncing sensor is written at most every half minute; what changes the state of the house (arming, alarm, disarming, lockout) at once.
    const std::vector<alarm::Event>& log = g_runner->panel().events();
    const bool important = !log.empty() && (log.back().kind == alarm::EventKind::kAlarm || log.back().kind == alarm::EventKind::kArmed || log.back().kind == alarm::EventKind::kDisarmed || log.back().kind == alarm::EventKind::kLockedOut || log.back().kind == alarm::EventKind::kSirenSilenced);
    if (!g_forgotten && !now_events.empty() && now_events != g_saved_events && (important || now - g_last_events_write_ms >= kEventsWriteEveryMs)) events_to_save = now_events;
    }
    if (!events_to_save.empty() && write_events(events_to_save)) { std::lock_guard<std::mutex> guard(g_lock); g_saved_events = events_to_save; g_last_events_write_ms = now; }
    // flash is written outside the lock: a write can take tens of milliseconds
    if (profile_changed) {
      if (write_profile(profile_text)) { std::lock_guard<std::mutex> guard(g_lock); g_saved_profile = profile_text; }
      else ESP_LOGE(kTag, "the arming profile could not be saved: a restart would arm the house without it");
    }
    if (!to_save.empty()) {
      if (write_saved(to_save)) { std::lock_guard<std::mutex> guard(g_lock); g_saved_text = to_save; }
      else ESP_LOGE(kTag, "the state of the panel could not be saved: a restart would leave it disarmed");
    }
  }
}
}  // namespace

void start(const config::Settings& settings, Publish publish) {
  g_settings = settings;
  g_publish = std::move(publish);
  g_runner = std::make_unique<alarm::Runner>(settings);
  {
    std::string text;
    alarm::Panel::Saved saved;
    g_saved_text = alarm::saved_text(g_runner->saved());   // disarmed
    // the arming profile the panel was armed with, if it was armed with one and it can be read back
    alarm::ArmProfile profile;
    std::string profile_text;
    const bool have_profile = read_profile(profile_text) && !profile_text.empty() && alarm::parse_profile_text(profile_text, profile);
    if (read_saved(text) && alarm::parse_saved(text, saved)) {
      if (g_runner->restore(saved, now_ms(), have_profile ? &profile : nullptr)) {
        ESP_LOGW(kTag, "the node restarted while the panel was %s (%s%s%s): it is armed again", alarm::phase_text(saved.phase), text.c_str(), g_runner->profile() != nullptr ? ", profile " : "", g_runner->profile() != nullptr ? g_runner->profile()->id.c_str() : "");
        g_saved_text = alarm::saved_text(g_runner->saved());
      }
    } else if (!text.empty()) ESP_LOGW(kTag, "the saved state of the panel is not readable and was ignored");
    g_saved_profile = g_runner->profile() != nullptr ? alarm::profile_json(*g_runner->profile()) : std::string();
  }
  {
    std::string text;
    if (read_events(text)) { g_saved_events = text; g_runner->restore_events(alarm::parse_events(text)); }
  }
  g_siren_relay = settings.panel.siren_relay.empty() ? -1 : relay_outputs::index_of(settings.panel.siren_relay);
  if (g_siren_relay >= 0) relay_outputs::reserve_for_siren(static_cast<std::size_t>(g_siren_relay));
  if (!settings.panel.siren_relay.empty() && g_siren_relay < 0) ESP_LOGE(kTag, "the siren relay \"%s\" is not an enabled relay: there is no siren", settings.panel.siren_relay.c_str());
  if (settings.zigbee_local.after_s != 0 && (settings.zigbee.enabled || !settings.zigbee.network_host.empty())) {   // the radio on the board or a coordinator on the network
    g_host = std::make_unique<ZigbeeHost>(settings);
    zigbee_bridge::set_local_host(g_host.get(), static_cast<unsigned>(settings.zigbee_local.after_s));
    ESP_LOGI(kTag, "the node takes the Zigbee radio itself after %d s without the server's client", settings.zigbee_local.after_s);
  }
  mqtt_link::subscribe(alarm::topic(settings.node_id, "zigbee"), on_zone_feed);
  if (settings.server.commands) {
    mqtt_link::subscribe(alarm::topic(settings.node_id, "command"), on_command);
    ESP_LOGI(kTag, "the server may arm, disarm and silence the panel (armor/alarm/%s/command)", settings.node_id.c_str());
  }
  xTaskCreate(alarm_task, "alarm", 6144, nullptr, 5, nullptr);
}

std::string state_json() {
  std::lock_guard<std::mutex> guard(g_lock);
  if (g_runner) g_runner->set_epoch(wall_epoch());
  return g_runner ? g_runner->state_json(stamp_ms(), now_ms()) : "{}";
}

std::array<alarm::ZoneView, config::kZoneCount> zones() {
  std::lock_guard<std::mutex> guard(g_lock);
  return g_runner ? g_runner->zones() : std::array<alarm::ZoneView, config::kZoneCount>{};
}

alarm::Refusal arm(alarm::Mode mode, bool force) {
  std::lock_guard<std::mutex> guard(g_lock);
  if (!g_runner) return alarm::Refusal::kNotArmed;
  const alarm::Refusal result = g_runner->arm(mode, force, now_ms());
  ESP_LOGW(kTag, "armed %s from the panel: %s", alarm::mode_text(mode), alarm::refusal_text(result));
  drive_siren_locked();
  publish_state_locked(true);
  return result;
}

alarm::Refusal disarm(const std::string& pin) {
  std::lock_guard<std::mutex> guard(g_lock);
  if (!g_runner) return alarm::Refusal::kNotArmed;
  const alarm::Refusal result = g_runner->disarm(pin, now_ms());
  ESP_LOGW(kTag, "disarm from the panel: %s", alarm::refusal_text(result));
  drive_siren_locked();
  publish_state_locked(true);
  return result;
}

alarm::Refusal silence(const std::string& pin) {
  std::lock_guard<std::mutex> guard(g_lock);
  if (!g_runner) return alarm::Refusal::kNotArmed;
  const alarm::Refusal result = g_runner->silence(pin, now_ms());
  ESP_LOGW(kTag, "silence from the panel: %s", alarm::refusal_text(result));
  drive_siren_locked();
  publish_state_locked(true);
  return result;
}

std::string error() { std::lock_guard<std::mutex> guard(g_lock); return g_error; }

bool guarding() {
  std::lock_guard<std::mutex> guard(g_lock);
  return g_runner && g_runner->panel().phase() != alarm::Phase::kDisarmed;
}

void forget() {
  std::lock_guard<std::mutex> guard(g_lock);
  g_forgotten = true;
  nvs_handle_t handle;
  if (nvs_open(kStateNamespace, NVS_READWRITE, &handle) != ESP_OK) return;
  if (nvs_erase_all(handle) == ESP_OK) nvs_commit(handle);
  nvs_close(handle);
}

}  // namespace armor::manager
