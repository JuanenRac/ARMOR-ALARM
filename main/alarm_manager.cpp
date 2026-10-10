// ARMOR-ALARM - the alarm at work: the task that reads the wired zones and runs core/alarm_runner.hpp. Nothing here has run on a board.
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
//
// What the panel decides (the states of a loop, the delays, the PIN, the siren) is in core/ and is tested on a computer; this file only reads the ADC, gives the runner the clock, puts the
// siren's level on its relay and talks to the broker. Only ADC1 is used (the converter shared with the Wi-Fi radio is never touched), 0 to about 3.1 V at the pin: a zone's loop must never put
// more than 3.3 V there. A pin that cannot be read says nothing - it is never taken for a tamper, because a fault of the node must not sound the siren.
#include "alarm_manager.hpp"

#include <memory>
#include <mutex>
extern "C" {
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
}
#include "mqtt_link.hpp"
#include "relay_outputs.hpp"

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

std::uint64_t now_ms() { return static_cast<std::uint64_t>(esp_timer_get_time()) / 1000ULL; }
std::uint64_t stamp_ms() { return mqtt_link::clock_is_set() ? mqtt_link::wall_clock_ms() : now_ms(); }

// What changes the message: the panel's phase, mode and siren and the state of each zone. Called with the lock held.
std::string signature() {
  std::string text = std::string(alarm::phase_text(g_runner->panel().phase())) + alarm::mode_text(g_runner->panel().mode()) + (g_runner->siren() ? "S" : "s") + (g_runner->locked_out(now_ms()) ? "L" : "l");
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

// The siren is on while the panel says so; the level goes to its relay only when it changes.
void drive_siren_locked() {
  const bool wanted = g_runner->siren();
  if (wanted == g_siren_on) return;
  g_siren_on = wanted;
  if (g_siren_relay >= 0) relay_outputs::set(static_cast<std::size_t>(g_siren_relay), wanted);
  ESP_LOGW(kTag, "the siren is %s", wanted ? "ON" : "off");
}

void on_command(const std::string&, const std::string& payload) {
  std::lock_guard<std::mutex> guard(g_lock);
  if (!g_runner) return;
  const alarm::Reply reply = g_runner->handle(payload, now_ms());
  if (!reply.understood) { ESP_LOGW(kTag, "a message on the command topic was ignored: it is not the contract's command"); return; }
  ESP_LOGW(kTag, "the server said %s: %s", reply.command.action == alarm::Action::kArm ? "arm" : "disarm", alarm::refusal_text(reply.refusal));
  if (g_publish) g_publish(alarm::topic(g_settings.node_id, "result"), g_runner->result_json(stamp_ms(), reply));
  drive_siren_locked();
  publish_state_locked(true);
}

void alarm_task(void*) {
  adc_oneshot_unit_handle_t unit = nullptr;
  adc_oneshot_unit_init_cfg_t unit_config{};
  unit_config.unit_id = ADC_UNIT_1;
  std::array<adc_cali_handle_t, config::kZoneCount> calibration{};
  std::array<bool, config::kZoneCount> ready{};
  bool any_zone = false;
  for (const config::ZoneSetting& zone : g_settings.zones) any_zone = any_zone || zone.enabled;
  if (any_zone) {
    if (adc_oneshot_new_unit(&unit_config, &unit) != ESP_OK) {
      ESP_LOGE(kTag, "the converter could not be started: the zones are not read");
      std::lock_guard<std::mutex> guard(g_lock);
      g_error = "adc";
      unit = nullptr;
    } else {
      for (std::size_t i = 0; i < config::kZoneCount; ++i) {
        const config::ZoneSetting& zone = g_settings.zones[i];
        if (!zone.enabled) continue;
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
  for (;;) {
    vTaskDelay(pdMS_TO_TICKS(kStepMs));
    const std::uint64_t now = now_ms();
    std::array<double, config::kZoneCount> millivolts{};
    std::array<bool, config::kZoneCount> read{};
    if (unit != nullptr) {
      for (std::size_t i = 0; i < config::kZoneCount; ++i) {
        if (!g_settings.zones[i].enabled || !ready[i]) continue;
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
    std::lock_guard<std::mutex> guard(g_lock);
    for (std::size_t i = 0; i < config::kZoneCount; ++i) if (read[i]) g_runner->feed(i, millivolts[i], now);
    g_runner->tick(now);
    drive_siren_locked();
    publish_state_locked(false);
  }
}
}  // namespace

void start(const config::Settings& settings, Publish publish) {
  g_settings = settings;
  g_publish = std::move(publish);
  g_runner = std::make_unique<alarm::Runner>(settings);
  g_siren_relay = settings.panel.siren_relay.empty() ? -1 : relay_outputs::index_of(settings.panel.siren_relay);
  if (!settings.panel.siren_relay.empty() && g_siren_relay < 0) ESP_LOGE(kTag, "the siren relay \"%s\" is not an enabled relay: there is no siren", settings.panel.siren_relay.c_str());
  if (settings.server.commands) {
    mqtt_link::subscribe(alarm::topic(settings.node_id, "command"), on_command);
    ESP_LOGI(kTag, "the server may arm and disarm the panel (armor/alarm/%s/command)", settings.node_id.c_str());
  }
  xTaskCreate(alarm_task, "alarm", 6144, nullptr, 5, nullptr);
}

std::string state_json() {
  std::lock_guard<std::mutex> guard(g_lock);
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

std::string error() { std::lock_guard<std::mutex> guard(g_lock); return g_error; }

}  // namespace armor::manager
