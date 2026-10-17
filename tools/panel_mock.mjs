#!/usr/bin/env node
/**
 * ARMOR-ALARM - a stand-in for an alarm node, to work on the web panel without a board.
 * Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
 *
 *   node tools/panel_mock.mjs [port]          (default 8090)     open http://127.0.0.1:8090/
 *
 * It serves the panel from panel/ (text.js is joined in front of app.js, as tools/pack_panel.py does) and answers /api/v1 like the firmware
 * does, from memory: a fresh mock is in set-up (code TESTCODE); `--user admin:adminpass123` starts it with an administrator. It is a development
 * tool: it checks far less than the firmware, and its numbers are made up. The alarm is a small simulation: arming starts the exit delay, an open zone alarms, the PIN is 1234.
 */
import { createServer } from "node:http";
import { readFileSync } from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const here = path.dirname(fileURLToPath(import.meta.url));
const panel = path.join(here, "..", "panel");
const port = Number(process.argv.find(a => /^\d+$/.test(a)) ?? 8090);
// --board s3-eth: the mock plays the Waveshare ESP32-S3-ETH (a cable, the W5500 pins reserved); the default is the s3-wifi board
const board = process.argv.includes("--board") ? process.argv[process.argv.indexOf("--board") + 1] : "s3-wifi";
const wired = board === "s3-eth";
const seeded = process.argv.includes("--user") ? process.argv[process.argv.indexOf("--user") + 1] : "";

const users = new Map();
if (seeded) { const [name, password] = seeded.split(":"); users.set(name, { password, role: "admin" }); }
const sessions = new Map();
const started = Date.now();
let rebootAt = 0;
let logText = "I (1200) armor-node: A.R.M.O.R. node armor-a1b2c3, firmware 0.2.3\nI (1500) armor-net: link up\nI (2600) armor-net: address 192.168.0.181, gateway 192.168.0.1, netmask 255.255.255.0 (wire)\nW (9000) armor-radar: radar 2: no data (RX not connected, no power or the wrong pin) (0 bytes, 0 frames, 0 bad)\n";

const config = {
  v: 1, node: { id: "alarm-a1b2c3", name: "House panel", hostname: "" },
  uplink: wired ? "ethernet" : "wifi", ip: { dhcp: true, address: "", netmask: "255.255.255.0", gateway: "", dns1: "", dns2: "" },
  ap: { enabled: true, ssid: "ARMOR-ALARM-A1B2C3", security: "wpa2", password_set: true, channel: 0, hidden: false, max_clients: 8, tx_power_dbm: 15, bandwidth_mhz: 20, country: "ES" },
  sta: { enabled: true, ssid: "HomeRouter", password_set: true, backup: [] },
  mqtt: { enabled: true, uri: "mqtt://192.168.0.180:18883", username: "alarm-node-alarm-a1b2c3", password_set: true, heartbeat_s: 10, ntp: "pool.ntp.org", backup: [] },
  zones: Array.from({ length: 8 }, (_, i) => ({ enabled: i < 4, id: ["front-door", "window", "hall-motion", "smoke"][i] ?? `zone${i + 1}`, name: ["Front door", "Living room window", "Hall motion", "Kitchen smoke"][i] ?? "", pin: [1, 2, 4, 5, 6, 7, 3, -1][i], input: "loop", contact: "nc", role: "contact", kind: ["entry", "instant", "interior", "always"][i] ?? "instant", hold_ms: 200, zigbee: "" })),
  buttons: Array.from({ length: 4 }, () => ({ enabled: false, action: "arm_away", pin: -1 })),
  loop: { short_below_mv: 400, normal_below_mv: 1200, triggered_below_mv: 2400 },
  panel: { exit_delay_s: 10, entry_delay_s: 10, siren_max_s: 60, lockout_after: 3, lockout_s: 30, siren_relay: "siren", pin_set: true },
  server: { commands: false },
  zigbee_local: { after_s: 0, sirens: [] },
  zigbee: { enabled: true, rx: 18, tx: 17, reset: 21, bootloader: 47, baud: 115200, port: 8888, allowed_client: "192.168.0.180", fallback_client: "", fallback_after_s: 30 },
  remote: { relays: false, link_timeout_s: 0, http: false, token_set: false },
  relays: Array.from({ length: 8 }, (_, i) => ({ enabled: i < 2, name: ["siren", "lights"][i] ?? `relay${i + 1}`, label: ["Siren", "Hall lights"][i] ?? "", pin: wired ? [38, 39, 40, 41, 42, 43, 44, 48][i] : [38, 39, 40, 41, 42, 11, 12, 13][i], active_low: i === 1 })),
  time: { ntp_enabled: true, ntp: "pool.ntp.org", zone: "CET-1CEST,M3.5.0,M10.5.0/3" },
  system: { auto_restart_hours: 0 },
  web: { mode: "both" },
  ble: { mode: "setup" },
  ui: { language: "en" },
};
// the relays the node drives: every one starts open
const relayOn = new Map();

const catalog = [];
for (let gpio = 0; gpio <= 48; ++gpio) {
  if (gpio >= 22 && gpio <= 25) continue;
  let use = "free", reason = "", note = "", header = true;
  if (gpio >= 26 && gpio <= 32) { use = "reserved"; reason = "flash"; header = false; }
  else if (wired && gpio >= 9 && gpio <= 14) { use = "reserved"; reason = "ethernet"; header = false; }
  else if (wired && gpio === 8) { use = "reserved"; reason = "camera"; header = false; }
  else if (wired && gpio >= 4 && gpio <= 7) { use = "caution"; note = "sdcard"; }
  else if (gpio >= 33 && gpio <= 37) { use = "reserved"; reason = "psram"; header = gpio >= 35; }
  else if (gpio === 19 || gpio === 20) { use = "reserved"; reason = "usb"; }
  else if (gpio === 0) { use = "caution"; note = "boot"; }
  else if ([3, 45, 46].includes(gpio)) { use = "caution"; note = "strapping"; }
  else if (gpio === 43 || gpio === 44) { use = "caution"; note = "usb_serial"; }
  else if (gpio === 48) { use = "caution"; note = "led"; }
  catalog.push({ gpio, use, reason, note, on_header: header });
}
// The alarm: a small simulation of the panel (the PIN is 1234). The zones are all closed except what the test opens through /api/v1/zones/open (not a firmware route).
const PIN = "1234";
const panelState = { phase: "disarmed", mode: "disarmed", siren: false, since: Date.now(), wrong: 0, lockedUntil: 0, events: [], open: new Set() };
const note = (kind, zone) => { panelState.events.push({ at: Date.now(), kind, ...(zone ? { zone } : {}) }); if (panelState.events.length > 10) panelState.events.shift(); };
const tickPanel = () => {
  const seconds = (Date.now() - panelState.since) / 1000;
  if (panelState.phase === "exit_delay" && seconds >= config.panel.exit_delay_s) { panelState.phase = "armed"; panelState.since = Date.now(); note("armed"); }
  if (panelState.phase === "entry_delay" && seconds >= config.panel.entry_delay_s) { panelState.phase = "alarm"; panelState.siren = true; panelState.since = Date.now(); note("alarm", "front-door"); }
  if (panelState.phase === "alarm" && panelState.siren && seconds >= config.panel.siren_max_s) { panelState.siren = false; note("siren_timed_out"); }
};
const enabledZones = () => config.zones.filter(z => z.enabled);
const zoneState = z => (panelState.open.has(z.id) ? "triggered" : "normal");
const openZonesFor = mode => enabledZones().filter(z => zoneState(z) === "triggered" && !(z.kind === "interior" && mode === "stay")).map(z => z.id);
const alarmMessage = () => {
  tickPanel();
  return { kind: "alarm", node_id: config.node.id, timestamp_ms: Date.now(), phase: panelState.phase, mode: panelState.mode, siren: panelState.siren, locked_out: Date.now() < panelState.lockedUntil, commands_enabled: config.server.commands,
    zones: enabledZones().map(z => ({ id: z.id, ...(z.name ? { name: z.name } : {}), kind: z.kind, state: zoneState(z), bypassed: false })), open_zones: openZonesFor(panelState.mode === "stay" ? "stay" : "away"),
    events: panelState.events.map(e => ({ ago_s: Math.round((Date.now() - e.at) / 1000), kind: e.kind, ...(e.zone ? { zone: e.zone } : {}) })) };
};
const zonesData = () => ({ alarm: alarmMessage(), catalog, zones: config.zones.map((z, i) => ({ zone: i + 1, enabled: z.enabled, id: z.id, name: z.name, kind: z.kind, state: z.enabled ? zoneState(z) : "normal", bypassed: false, has_reading: z.enabled && !z.zigbee, millivolts: z.enabled && !z.zigbee && z.input !== "digital" ? (panelState.open.has(z.id) ? 1800 : 800) : 0, digital: z.input === "digital", pin_high: z.input === "digital" && panelState.open.has(z.id), zigbee: z.zigbee ?? "", battery_low: false })) });
const io = () => {
  const tick = Math.floor((Date.now() - started) / 2000);
  const z = config.zigbee;
  return {
    zigbee: { enabled: z.enabled, state: z.enabled ? "connected" : "disabled", error: "", port: z.port, baud: z.baud, allowed_client: z.allowed_client, client: z.enabled ? z.allowed_client : "", role: z.enabled ? "primary" : "", fallback_client: z.fallback_client, fallback_after_s: z.fallback_after_s, takeovers: 0, to_radio: 120 + tick * 9, from_radio: 4096 + tick * 61, connections: 1, refused: 0, dropped: 0, has_reset: z.reset >= 0, has_bootloader: z.bootloader >= 0 },
    remote: { relays: config.remote.relays },
    relays: config.relays.map((r, i) => ({ relay: i + 1, enabled: r.enabled, name: r.name, label: r.label, pin: r.pin, on: r.enabled && relayOn.get(i) === true })),
  };
};

const json = (response, code, body, headers = {}) => { response.writeHead(code, { "Content-Type": "application/json", "Cache-Control": "no-store", ...headers }); response.end(JSON.stringify(body)); };
const bodyOf = request => new Promise(resolve => { const chunks = []; request.on("data", c => chunks.push(c)); request.on("end", () => resolve(Buffer.concat(chunks))); });
const tokenOf = request => /armor_session=([0-9a-f]+)/.exec(request.headers.cookie ?? "")?.[1] ?? "";

function status() {
  return {
    node_id: config.node.id, name: config.node.name, version: "0.0.3", uptime_s: Math.floor((Date.now() - started) / 1000) + 5400, reset_reason: "power_on", heap_free: 182000, heap_min: 151000, psram_free: 7400000, partition: "ota_0",
    network: { board, ethernet_available: wired, ethernet_ok: true, layout: wired ? "ethernet+ap" : "wifi-station+ap", link_up: true, has_ip: true, ip: "192.168.0.181", netmask: "255.255.255.0", gateway: "192.168.0.1", dns: "192.168.0.1", mac: "34:85:18:a1:b2:c3",
      ap_active: config.ap.enabled, ap_setup: users.size === 0, ap_ssid: users.size === 0 ? "ARMOR-SETUP-A1B2C3" : config.ap.ssid, ap_channel: 6, ap_clients: 1, sta_connected: true, sta_ssid: config.sta.ssid, sta_rssi: -52 },
    mqtt: { enabled: config.mqtt.enabled, connected: true, clock_set: true, published: 400 + Math.floor((Date.now() - started) / 5000), dropped: 0 },
    web: { mode: config.web.mode, https: config.web.mode !== "http", cert_sha256: "a3f1c07d9e2b4c58a7106f3de9b2c4815d6e7f80a1b2c3d4e5f60718293a4b5c" },
    alarm: alarmMessage(),
  };
}

const problems = doc => {
  const list = [];
  if (doc.ap?.enabled && !String(doc.ap.ssid ?? "").trim()) list.push({ path: "ap.ssid", code: "required" });
  if (doc.ap?.enabled && doc.ap.security !== "open" && doc.ap.password !== undefined && doc.ap.password.length > 0 && doc.ap.password.length < 8) list.push({ path: "ap.password", code: "invalid_key" });
  if (doc.mqtt?.enabled && !/^mqtts?:\/\/.+/.test(doc.mqtt.uri ?? "")) list.push({ path: "mqtt.uri", code: "invalid" });
  if (doc.zigbee?.enabled) {
    if (!/^\d+\.\d+\.\d+\.\d+$/.test(doc.zigbee.allowed_client ?? "")) list.push({ path: "zigbee.allowed_client", code: doc.zigbee.allowed_client ? "invalid" : "required" });
    if (!(doc.zigbee.port >= 1024 && doc.zigbee.port <= 65535)) list.push({ path: "zigbee.port", code: "range" });
  }
  if (doc.remote?.relays && !(doc.mqtt?.enabled ?? config.mqtt.enabled)) list.push({ path: "remote.relays", code: "needs_broker" });
  if (doc.remote && doc.remote.link_timeout_s !== 0 && doc.remote.link_timeout_s !== undefined && doc.remote.link_timeout_s < 10) list.push({ path: "remote.link_timeout_s", code: "range" });
  const seen = new Set(), zigbeeSeen = new Set();
  (doc.zones ?? []).forEach((z, i) => {
    if (!z.enabled) return;
    const base = `zones.${i}.`;
    if (!/^[a-z0-9][a-z0-9_-]{0,31}$/.test(z.id ?? "")) list.push({ path: base + "id", code: z.id ? "invalid" : "required" });
    else if (seen.has(z.id)) list.push({ path: base + "id", code: "conflict" }); else seen.add(z.id);
    if (z.zigbee) {
      if (!/^[0-9a-fA-F]{16}$/.test(z.zigbee)) list.push({ path: base + "zigbee", code: "invalid" });
      else if (zigbeeSeen.has(z.zigbee.toLowerCase())) list.push({ path: base + "zigbee", code: "conflict" }); else zigbeeSeen.add(z.zigbee.toLowerCase());
      if (!((doc.zigbee_local ?? config.zigbee_local).after_s > 0)) list.push({ path: base + "zigbee", code: "needs_local" });
    } else if (z.input === "digital") { if (!(z.pin >= 0)) list.push({ path: base + "pin", code: "required" }); }
    else if (!(z.pin >= 1 && z.pin <= 10)) list.push({ path: base + "pin", code: z.pin < 0 ? "required" : "not_analog" });
  });
  if (doc.zigbee_local) {
    const after = doc.zigbee_local.after_s;
    if (after !== undefined && after !== 0 && (after < 10 || !(doc.zigbee?.enabled ?? config.zigbee.enabled))) list.push({ path: "zigbee_local.after_s", code: (doc.zigbee?.enabled ?? config.zigbee.enabled) ? "range" : "needs_zigbee" });
    (doc.zigbee_local.sirens ?? []).forEach((a, i) => { if (!/^[0-9a-fA-F]{16}$/.test(a)) list.push({ path: `zigbee_local.sirens.${i}`, code: "invalid" }); });
  }
  if ((doc.zones ?? config.zones).some(z => z.enabled) && doc.panel && doc.panel.pin !== undefined && doc.panel.pin !== "" && !/^\d{4,12}$/.test(doc.panel.pin)) list.push({ path: "panel.pin", code: "invalid" });
  if (doc.server?.commands && !(doc.mqtt?.enabled ?? config.mqtt.enabled)) list.push({ path: "server.commands", code: "needs_broker" });
  return list;
};

const server = createServer(async (request, response) => {
  const url = new URL(request.url, "http://x");
  if (request.method === "GET" && ["/", "/index.html"].includes(url.pathname)) { response.writeHead(200, { "Content-Type": "text/html" }); return response.end(readFileSync(path.join(panel, "index.html"))); }
  if (request.method === "GET" && url.pathname === "/style.css") { response.writeHead(200, { "Content-Type": "text/css" }); return response.end(readFileSync(path.join(panel, "style.css"))); }
  if (request.method === "GET" && url.pathname === "/app.js") { response.writeHead(200, { "Content-Type": "text/javascript" }); return response.end(readFileSync(path.join(panel, "text.js"), "utf8") + "\n" + readFileSync(path.join(panel, "app.js"), "utf8")); }
  if (!url.pathname.startsWith("/api/v1/")) return json(response, 404, { error: "not_found" });

  const route = url.pathname.slice(8), method = request.method;
  const raw = await bodyOf(request);
  let body = {};
  if (raw.length && !url.pathname.endsWith("/ota")) { try { body = JSON.parse(raw.toString("utf8")); } catch { return json(response, 400, { error: "not_json" }); } }
  const session = sessions.get(tokenOf(request));
  const setup = users.size === 0;

  if (method === "GET" && route === "session") return json(response, 200, { setup, authenticated: !!session, user: session?.user ?? "", role: session?.role ?? "", node_id: config.node.id, language: config.ui.language, version: "0.2.3", setup_ssid: setup ? "ARMOR-SETUP-A1B2C3" : "", mac: "34:85:18:a1:b2:c3", board, ethernet: wired });
  if (method === "POST" && route === "setup") {
    if (!setup) return json(response, 403, { error: "forbidden" });
    if (body.code !== "TESTCODE") return json(response, 403, { error: "wrong_code" });
    if (!/^[a-z0-9_.-]{3,32}$/.test(body.user ?? "")) return json(response, 422, { error: "invalid_name" });
    if ((body.password ?? "").length < 8) return json(response, 422, { error: "weak_password" });
    users.set(body.user, { password: body.password, role: "admin" });
    return json(response, 200, { ok: true, restart_required: true });
  }
  if (method === "POST" && route === "login") {
    const user = users.get(body.user);
    if (!user || user.password !== body.password) return json(response, 401, { error: "wrong_credentials" });
    const token = [...Array(48)].map(() => "0123456789abcdef"[Math.floor(Math.random() * 16)]).join("");
    sessions.set(token, { user: body.user, role: user.role });
    return json(response, 200, { ok: true, restart_required: false }, { "Set-Cookie": `armor_session=${token}; Path=/; HttpOnly; SameSite=Strict` });
  }
  if (method === "POST" && route === "logout") { sessions.delete(tokenOf(request)); return json(response, 200, { ok: true }); }
  if (setup) return json(response, 403, { error: "setup_required" });
  if (!session) return json(response, 401, { error: "unauthorized" });
  if (method !== "GET" && request.headers["x-requested-with"] !== "armor") return json(response, 403, { error: "forbidden" });
  const admin = session.role === "admin";
  const needAdmin = () => { if (!admin) { json(response, 403, { error: "forbidden" }); return false; } return true; };

  if (method === "GET" && route === "status") return json(response, 200, status());
  if (method === "GET" && route === "wifi/scan") { if (!needAdmin()) return; return json(response, 200, { networks: [{ ssid: "HomeRouter", rssi: -48, channel: 6, security: "wpa2" }, { ssid: "Neighbour", rssi: -71, channel: 11, security: "wpa2wpa3" }, { ssid: "CafeOpen", rssi: -80, channel: 1, security: "open" }] }); }
  if (method === "GET" && route === "config") return json(response, 200, { config, channel_auto: 6, firmware: "0.2.3" });
  if (method === "PUT" && route === "config") {
    if (!needAdmin()) return;
    const list = problems(body);
    if (list.length) return json(response, 422, { error: "invalid", problems: list });
    for (const key of Object.keys(body)) {
      const value = body[key];
      if ((key === "zones" || key === "relays") && Array.isArray(value)) value.forEach((m, i) => Object.assign(config[key][i], m));
      else if (value && typeof value === "object" && !Array.isArray(value) && config[key] && typeof config[key] === "object") Object.assign(config[key], value);
      else config[key] = value;
    }
    if (typeof config.panel.pin === "string") { if (config.panel.pin) config.panel.pin_set = true; delete config.panel.pin; }
    if (typeof config.remote.token === "string") { if (config.remote.token) config.remote.token_set = true; delete config.remote.token; }
    for (const section of [config.ap, config.sta, config.mqtt]) if (typeof section.password === "string" && section.password) { section.password_set = true; delete section.password; } else delete section.password;
    return json(response, 200, { ok: true, restart_required: true });
  }
  if (method === "GET" && route === "zones") return json(response, 200, zonesData());
  if (method === "POST" && route === "alarm/arm") {
    if (!needAdmin()) return;
    if (!["away", "stay"].includes(body.mode)) return json(response, 422, { error: "invalid_mode" });
    tickPanel();
    if (panelState.phase !== "disarmed") return json(response, 409, { error: "not_disarmed" });
    if (openZonesFor(body.mode).length && !body.force) return json(response, 409, { error: "zones_open" });
    panelState.mode = body.mode; panelState.phase = "exit_delay"; panelState.since = Date.now(); note("exit_delay_started");
    return json(response, 200, { ok: true, restart_required: false });
  }
  if (method === "POST" && route === "alarm/disarm") {
    if (!needAdmin()) return;
    if (Date.now() < panelState.lockedUntil) return json(response, 423, { error: "locked_out" });
    if (body.pin !== PIN) { note("bad_pin"); if (++panelState.wrong >= config.panel.lockout_after) { panelState.wrong = 0; panelState.lockedUntil = Date.now() + config.panel.lockout_s * 1000; note("locked_out"); } return json(response, 403, { error: "bad_pin" }); }
    panelState.wrong = 0; panelState.phase = "disarmed"; panelState.mode = "disarmed"; panelState.siren = false; note("disarmed");
    return json(response, 200, { ok: true, restart_required: false });
  }
  if (method === "POST" && route === "alarm/silence") {
    if (!needAdmin()) return;
    if (Date.now() < panelState.lockedUntil) return json(response, 423, { error: "locked_out" });
    if (body.pin !== PIN) { note("bad_pin"); if (++panelState.wrong >= config.panel.lockout_after) { panelState.wrong = 0; panelState.lockedUntil = Date.now() + config.panel.lockout_s * 1000; note("locked_out"); } return json(response, 403, { error: "bad_pin" }); }
    panelState.wrong = 0;
    tickPanel();
    if (panelState.phase !== "alarm") return json(response, 409, { error: "refused" });
    if (panelState.siren) { panelState.siren = false; note("siren_silenced"); }
    return json(response, 200, { ok: true, restart_required: false });
  }
  if (method === "POST" && route === "zones/open") {   // a development route: opens or closes a zone of the stand-in
    if (!needAdmin()) return;
    if (body.open) panelState.open.add(body.id); else panelState.open.delete(body.id);
    if (body.open) {
      const zone = config.zones.find(z => z.id === body.id);
      if (zone && panelState.phase === "armed") { if (zone.kind === "entry") { panelState.phase = "entry_delay"; panelState.since = Date.now(); note("entry_delay_started", zone.id); } else if (zone.kind === "instant" || (zone.kind === "interior" && panelState.mode === "away")) { panelState.phase = "alarm"; panelState.siren = true; panelState.since = Date.now(); note("alarm", zone.id); } }
      if (zone && zone.kind === "always") { panelState.phase = "alarm"; panelState.siren = true; panelState.since = Date.now(); note("alarm", zone.id); }
    }
    return json(response, 200, { ok: true });
  }
  if (method === "GET" && route === "io") return json(response, 200, io());
  if (method === "POST" && route === "zigbee/radio") {
    if (!needAdmin()) return;
    if (!["reset", "bootloader"].includes(body.action)) return json(response, 422, { error: "invalid" });
    if (!config.zigbee.enabled || config.zigbee.reset < 0 || (body.action === "bootloader" && config.zigbee.bootloader < 0)) return json(response, 409, { error: "radio_not_available" });
    return json(response, 200, { ok: true, restart_required: false });
  }
  if (method === "POST" && route.startsWith("relays/")) {
    if (!needAdmin()) return;
    const index = Number(route.slice(7)) - 1;
    if (typeof body.on !== "boolean" || !(index >= 0 && index < 8)) return json(response, 422, { error: "invalid" });
    if (!config.relays[index].enabled) return json(response, 409, { error: "relay_not_available" });
    relayOn.set(index, body.on);
    return json(response, 200, { ok: true, restart_required: false });
  }
  if (method === "GET" && route === "users") { if (!needAdmin()) return; return json(response, 200, [...users].map(([name, u]) => ({ name, role: u.role }))); }
  if (method === "POST" && route === "users") {
    if (!needAdmin()) return;
    if (!/^[a-z0-9_.-]{3,32}$/.test(body.name ?? "")) return json(response, 422, { error: "invalid_name" });
    if ((body.password ?? "").length < 8) return json(response, 422, { error: "weak_password" });
    if (users.has(body.name)) return json(response, 409, { error: "exists" });
    users.set(body.name, { password: body.password, role: body.role === "admin" ? "admin" : "viewer" });
    return json(response, 200, { ok: true, restart_required: false });
  }
  if (method === "PUT" && route.startsWith("users/")) {
    if (!needAdmin()) return;
    const user = users.get(route.slice(6));
    if (!user) return json(response, 404, { error: "not_found" });
    if (body.role) user.role = body.role;
    if (body.password) user.password = body.password;
    return json(response, 200, { ok: true, restart_required: false });
  }
  if (method === "DELETE" && route.startsWith("users/")) {
    if (!needAdmin()) return;
    const name = route.slice(6);
    if (users.get(name)?.role === "admin" && [...users.values()].filter(u => u.role === "admin").length === 1) return json(response, 409, { error: "last_admin" });
    users.delete(name);
    return json(response, users.has(name) ? 500 : 200, { ok: true, restart_required: false });
  }
  if (method === "PUT" && route === "account") {
    const user = users.get(session.user);
    if (user.password !== body.current) return json(response, 403, { error: "wrong_password" });
    if ((body.password ?? "").length < 8) return json(response, 422, { error: "weak_password" });
    user.password = body.password;
    return json(response, 200, { ok: true, restart_required: false });
  }
  if (method === "GET" && route === "log") return json(response, 200, { next: logText.length, text: logText.slice(Number(url.searchParams.get("from") ?? 0)) });
  if (method === "GET" && route === "ota/check") { if (!needAdmin()) return; return json(response, 200, { ok: true, current_version: "0.2.3", latest_version: "9.9.9", update_available: true }); }
  if (method === "POST" && route === "ota/install") { if (!needAdmin()) return; return json(response, 200, { ok: true, started: true }); }
  if (method === "GET" && route === "ota/progress") { if (!needAdmin()) return; return json(response, 200, { state: "downloading", got: 300000, total: 1200000 }); }
  if (method === "POST" && route === "ota/switch") { if (!needAdmin()) return; rebootAt = Date.now(); return json(response, 200, { ok: true, restart_required: true, slot: "ota_1", version: "0.0.0" }); }
  if (method === "POST" && route === "reboot") { if (!needAdmin()) return; rebootAt = Date.now(); return json(response, 200, { ok: true, restart_required: false }); }
  if (method === "POST" && route === "factory-reset") { if (!needAdmin()) return; if (body.confirm !== "RESET") return json(response, 422, { error: "confirm_required" }); users.clear(); return json(response, 200, { ok: true, restart_required: true }); }
  if (method === "POST" && route === "ota") { if (!needAdmin()) return; return json(response, 200, { ok: true, restart_required: true, version: "0.2.4", bytes: raw.length, sha256: "0".repeat(64) }); }
  return json(response, 404, { error: "not_found" });
});
server.listen(port, "127.0.0.1", () => console.log(`ARMOR node panel mock on http://127.0.0.1:${port}/ (${users.size ? "signed-in users: " + [...users.keys()].join(", ") : "set-up code TESTCODE"})`));
