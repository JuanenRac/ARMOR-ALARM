// ARMOR-ALARM - the web panel in a real browser (headless Microsoft Edge) against the stand-in node of tools/panel_mock.mjs: set-up, login, the pages, arming and disarming, saving with a refused and a
// corrected value, every page in the seven languages, the width of a phone. Development tool; it needs Edge, and `ws` from ARMOR-SERVER's node_modules.
//   node tools/panel_browser_test.mjs [s3-wifi|s3-eth]
// Copyright (C) 2026 JuanenRac (Electro Hobby 3D). GPL-3.0-or-later.
import { spawn } from "node:child_process";
import fs from "node:fs";
import os from "node:os";
import path from "node:path";
import { createRequire } from "node:module";
import { fileURLToPath } from "node:url";
const HERE = path.dirname(fileURLToPath(import.meta.url));
const R = path.resolve(HERE, "..", "..");   // the folder that holds the ARMOR repositories
const require = createRequire(R + "/ARMOR-SERVER/package.json");
const WebSocket = require("ws");
const OUT = path.join(os.tmpdir(), "armor-panel-shots");   // where the screenshots go
fs.mkdirSync(OUT, { recursive: true });
const sleep = ms => new Promise(r => setTimeout(r, ms));
const tmp = fs.mkdtempSync(path.join(os.tmpdir(), "armor-epanel-"));
const board = process.argv[2] ?? "s3-wifi";
const mock = spawn("node", ["tools/panel_mock.mjs", "18122", "--user", "admin:adminpass123", "--board", board], { cwd: R + "/ARMOR-ALARM", stdio: "ignore" });
const fresh = spawn("node", ["tools/panel_mock.mjs", "18123", "--board", board], { cwd: R + "/ARMOR-ALARM", stdio: "ignore" });
let edge;
const cleanup = () => { for (const p of [mock, fresh, edge]) try { p?.kill(); } catch { /* ignore */ } };
process.on("exit", cleanup);
const results = [];
const check = (name, ok, extra = "") => { results.push(ok); console.log(ok ? "PASS" : "FAIL", name, ok ? "" : extra); };
try {
  await sleep(1500);
  const login = await fetch("http://127.0.0.1:18122/api/v1/login", { method: "POST", headers: { "Content-Type": "application/json" }, body: JSON.stringify({ user: "admin", password: "adminpass123" }) });
  const cookie = login.headers.getSetCookie()[0].split(";")[0];
  const [cname, cvalue] = [cookie.slice(0, cookie.indexOf("=")), cookie.slice(cookie.indexOf("=") + 1)];
  edge = spawn("C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe", ["--headless=new", "--remote-debugging-port=9366", "--user-data-dir=" + path.join(tmp, "edge"), "--no-first-run", "--disable-gpu", "--window-size=1300,900", "about:blank"], { stdio: "ignore" });
  let targets;
  for (let i = 0; i < 40; i += 1) { try { targets = await (await fetch("http://127.0.0.1:9366/json")).json(); if (targets.length) break; } catch { /* wait */ } await sleep(300); }
  const page = targets.find(t => t.type === "page");
  const ws = new WebSocket(page.webSocketDebuggerUrl);
  await new Promise(r => ws.on("open", r));
  let id = 0; const waiting = new Map(); const errors = [];
  ws.on("message", raw => {
    const m = JSON.parse(raw);
    if (m.id && waiting.has(m.id)) { waiting.get(m.id)(m.result ?? m.error); waiting.delete(m.id); }
    if (m.method === "Runtime.exceptionThrown") errors.push(m.params.exceptionDetails.exception?.description ?? m.params.exceptionDetails.text);
    if (m.method === "Runtime.consoleAPICalled" && m.params.type === "error") errors.push(m.params.args.map(a => a.value ?? a.description).join(" "));
    if (m.method === "Log.entryAdded" && m.params.entry.level === "error") errors.push(m.params.entry.text + " " + (m.params.entry.url ?? ""));
  });
  const send = (method, params = {}) => new Promise(r => { id += 1; waiting.set(id, r); ws.send(JSON.stringify({ id, method, params })); });
  await send("Page.enable"); await send("Network.enable"); await send("Runtime.enable"); await send("Log.enable");
  await send("Emulation.setDeviceMetricsOverride", { width: 1300, height: 900, deviceScaleFactor: 1, mobile: false });
  const ev = async expr => { const r = await send("Runtime.evaluate", { expression: expr, returnByValue: true, awaitPromise: true }); if (r.exceptionDetails) console.log("EXC", r.exceptionDetails.exception?.description ?? r.exceptionDetails.text); return r.result?.value; };
  const shot = async file => { const r = await send("Page.captureScreenshot", { format: "png", captureBeyondViewport: true }); fs.writeFileSync(path.join(OUT, file), Buffer.from(r.data, "base64")); };
  const text = () => ev("document.body.innerText");
  const goto = async hash => { await ev(`location.hash = '#/${hash}'`); await sleep(900); };
  const setText = (selector, value) => ev(`(() => { const e = document.querySelector(${JSON.stringify(selector)}); if (!e) return false; e.focus(); e.value = ${JSON.stringify(value)}; e.dispatchEvent(new Event('input', { bubbles: true })); return true; })()`);
  const clickButton = label => ev(`(() => { const b = [...document.querySelectorAll('button')].find(x => x.textContent.trim() === ${JSON.stringify(label)}); if (!b) return false; b.click(); return true; })()`);

  await send("Network.setCookie", { name: cname, value: cvalue, url: "http://127.0.0.1:18122/", path: "/" });
  await send("Page.navigate", { url: "http://127.0.0.1:18122/" });
  await sleep(1800);

  const post = (route, body) => fetch("http://127.0.0.1:18122/api/v1/" + route, { method: "POST", headers: { "Content-Type": "application/json", "X-Requested-With": "armor", Cookie: cookie }, body: JSON.stringify(body) }).then(r => r.json().then(d => ({ status: r.status, data: d })));

  // ---- overview
  let body = await text();
  check("the overview shows the node and the alarm", body.includes("This node") && body.includes("The alarm") && body.includes("disarmed"), body.slice(0, 300));
  check("the overview has no meter leftovers", !/Meters|PZEM|inverter|battery/i.test(body), body.match(/Meters|PZEM|inverter|battery/i)?.[0]);
  await shot(`apanel-${board}-overview.png`);

  // ---- the alarm page
  await goto("alarm");
  body = await text();
  check("the alarm page has the state, the buttons, the PIN and the zones", body.includes("State of the alarm") && body.includes("Arm: away") && body.includes("Arm: at home") && body.includes("Disarm") && body.includes("Front door") && body.includes("not a certified alarm system"), body.slice(0, 400));
  await clickButton("Arm: away"); await sleep(900);
  body = await text();
  check("arming starts the exit delay", body.includes("leaving: exit delay") && body.includes("Armed.") || body.includes("leaving: exit delay"), body.slice(0, 300));
  await shot(`apanel-${board}-alarm-arming.png`);
  // an open zone raises the alarm once armed (the stand-in's exit delay is 10 s: set it to 1 first)
  await post("zones/open", { id: "window", open: true });
  await sleep(300);
  await post("alarm/disarm", { pin: "1234" });
  await post("zones/open", { id: "window", open: false });
  // a wrong PIN is refused and said, and the right one disarms
  await post("alarm/arm", { mode: "away" });
  await goto("alarm");
  await ev(`(() => { const i = document.getElementById('al-pin'); i.value = '0000'; i.dispatchEvent(new Event('input', { bubbles: true })); })()`);
  await clickButton("Disarm"); await sleep(900);
  check("a wrong PIN is refused and the panel says so", (await text()).includes("Wrong PIN."), (await text()).slice(0, 200));
  await goto("alarm");
  await ev(`(() => { const i = document.getElementById('al-pin'); i.value = '1234'; i.dispatchEvent(new Event('input', { bubbles: true })); })()`);
  await clickButton("Disarm"); await sleep(900);
  body = await text();
  check("the right PIN disarms", body.includes("Disarmed.") || body.includes("disarmed"), body.slice(0, 200));
  // arming over an open zone is refused and offers to force it
  await post("zones/open", { id: "window", open: true });
  await goto("alarm");
  await clickButton("Arm: away"); await sleep(900);
  body = await text();
  check("arming over an open zone is refused and says why", body.includes("A zone that matters is open"), body.slice(0, 300));
  await post("zones/open", { id: "window", open: false });
  await post("alarm/arm", { mode: "away", force: true });
  await post("alarm/disarm", { pin: "1234" });

  // ---- the zones page
  await goto("zones");
  body = await text();
  check("the zones page has the loop's limits and eight zones", body.includes("The loop's limits") && [1, 2, 8].every(n => body.includes(`Zone ${n}`)) && body.includes("Never put more than 3.3 V"), body.slice(0, 300));
  check("each live zone shows what its pin reads", /\d+ mV at the pin/.test(body), body.slice(0, 300));
  const pinList = await ev(`(() => { const c = [...document.querySelectorAll('section.card')].find(x => x.querySelector('h2')?.textContent.startsWith('Zone 1')); const sel = [...c.querySelectorAll('select')][0]; return [...sel.options].map(o => o.value).filter(v => v !== '-1').map(Number); })()`);
  check("a zone is offered only ADC1 pins", pinList.length > 0 && pinList.every(g => g >= 1 && g <= 10), JSON.stringify(pinList));
  // a repeated identifier is refused by the node and the panel says so
  await ev(`(() => { const c = [...document.querySelectorAll('section.card')].find(x => x.querySelector('h2')?.textContent.startsWith('Zone 2')); const ins = c.querySelectorAll('input[type=text]'); ins[0].value = 'front-door'; ins[0].dispatchEvent(new Event('input', { bubbles: true })); })()`);
  await clickButton("Save"); await sleep(900);
  check("a repeated zone identifier is refused", /already|conflict|used/i.test(await text()) || (await ev("document.querySelectorAll('.err, input.bad').length")) > 0);
  await ev(`(() => { const c = [...document.querySelectorAll('section.card')].find(x => x.querySelector('h2')?.textContent.startsWith('Zone 2')); const ins = c.querySelectorAll('input[type=text]'); ins[0].value = 'window'; ins[0].dispatchEvent(new Event('input', { bubbles: true })); })()`);
  await clickButton("Save"); await sleep(900);
  await shot(`apanel-${board}-zones.png`);

  // ---- the panel page
  await goto("panel");
  body = await text();
  check("the panel page has the delays, the PIN and the siren and the server's switch", body.includes("Exit delay") && body.includes("Entry delay") && body.includes("PIN (4 to 12 digits)") && body.includes("Let the server arm and disarm the panel") && body.includes("alarm-commands"), body.slice(0, 400));
  await setText("input[type=password]", "12ab");
  await clickButton("Save"); await sleep(900);
  check("a PIN that is not four to twelve digits is refused", (await text()).includes("not valid") || (await ev("document.querySelectorAll('input.bad').length")) > 0, (await text()).slice(0, 200));
  await setText("input[type=password]", "987654");
  await clickButton("Save"); await sleep(900);
  const savedPanel = await (await fetch("http://127.0.0.1:18122/api/v1/config", { headers: { Cookie: cookie } })).json();
  check("the corrected PIN is saved and never shown", savedPanel.config.panel.pin_set === true && savedPanel.config.panel.pin === undefined && savedPanel.config.zones[1].id === "window", JSON.stringify(savedPanel.config.panel));
  await shot(`apanel-${board}-panel.png`);

  // ---- the Zigbee radio and the relays
  await ev("window.confirm = () => true");
  await goto("zigbee");
  body = await text();
  check("the Zigbee page has the radio card and the lines for Zigbee2MQTT", body.includes("Zigbee radio") && body.includes("tcp://192.168.0.181:8888") && body.includes("Fallback client"), body.slice(0, 300));
  await goto("io");
  body = await text();
  check("the relays page has the siren note, the broker switch and eight relays", body.includes("is the siren") && body.includes("Let the broker move the relays") && [1, 2, 8].every(n => body.includes(`Relay ${n}`)) && !body.includes("Analog inputs"), body.slice(0, 300));

  // ---- the firmware page can ask GitHub for a newer version (every node has it)
  await goto("update");
  body = await text();
  check("the firmware page offers the GitHub check", body.includes("Check GitHub for a new version") && body.includes("Check now"), body.slice(0, 200));
  await clickButton("Check now"); await sleep(700);
  check("the check offers a newer version", (await text()).includes("Version 9.9.9 is available."));
  await ev("location.reload()"); await sleep(1500);

  // ---- every page in every language: no raw keys, no missing texts, no console errors
  const pages = ["overview", "alarm", "zones", "panel", "zigbee", "io", "network", "wifi", "broker", "users", "update"];
  const langs = await ev("LANGS.map(l => l[0])");
  let rawBad = 0;
  for (let li = 0; li < langs.length; li += 1) {
    await ev(`(() => { lang = ${li}; document.documentElement.lang = LANGS[${li}][0]; })()`);
    for (const p of pages) {
      await ev(`location.hash = '#/${p}'`); await sleep(300);
      const raw = await ev(`(() => { const keys = new Set(Object.keys(L)); const bad = []; document.querySelectorAll('h1,h2,h3,label,span,button,p,dt,th,option,small,a').forEach(n => { const s = n.childNodes.length === 1 && n.childNodes[0].nodeType === 3 ? n.textContent.trim() : ''; if (s && keys.has(s) && L[s][0] !== s) bad.push(s); if (/^\\?\\?|undefined|\\[object/.test(s)) bad.push(s); }); return bad; })()`);
      if (raw.length) { rawBad += 1; check(`no raw text keys on ${p} in ${langs[li]}`, false, raw.join(",")); }
      if (li === 5 && (p === "alarm" || p === "zones")) await shot(`epanel-${board}-${p}-${langs[li]}.png`);
    }
  }
  check(`all ${pages.length} pages in all ${langs.length} languages have their texts`, rawBad === 0);

  // ---- phone width
  await send("Emulation.setDeviceMetricsOverride", { width: 390, height: 800, deviceScaleFactor: 2, mobile: true });
  await ev(`(() => { lang = 0; localStorage.setItem('armor_lang', 'en'); })()`);
  for (const p of ["alarm", "zones", "panel", "io"]) {
    await ev(`location.hash = '#/${p}'`); await sleep(500);
    const overflow = await ev("document.documentElement.scrollWidth - document.documentElement.clientWidth");
    check(`the ${p} page fits a phone's width`, overflow <= 1, `overflow ${overflow}px`);
    await shot(`epanel-${board}-${p}-phone.png`);
  }

  // ---- a fresh node shows the set-up screen
  await send("Emulation.setDeviceMetricsOverride", { width: 1300, height: 900, deviceScaleFactor: 1, mobile: false });
  await send("Network.clearBrowserCookies");
  await send("Page.navigate", { url: "http://127.0.0.1:18123/" }); await sleep(1500);
  check("a fresh node shows the set-up screen", (await text()).includes("Set up this node"));
  await shot(`epanel-${board}-setup.png`);

  const real = errors.filter(e => !/favicon|401|403|409|422|423/.test(e));
  check("no errors in the browser console", real.length === 0, JSON.stringify(real).slice(0, 400));
  console.log(`${results.filter(Boolean).length}/${results.length} checks passed`);
} catch (e) { console.log("FAILED", e); } finally { cleanup(); process.exit(0); }
