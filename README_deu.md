<p align="center">
  <img src="images/ARMOR_BANNER.svg" alt="ARMOR-ALARM banner" width="100%">
</p>

# 🚨 ARMOR-ALARM

<p align="center">
  <a href="README.md">🇺🇸 English</a> |
  <a href="README_spa.md">🇪🇸 Español</a> |
  <a href="README_fra.md">🇫🇷 Français</a> |
  <a href="README_ita.md">🇮🇹 Italiano</a> |
  🇩🇪 <b>Deutsch</b> |
  <a href="README_zho.md">🇨🇳 简体中文</a> |
  <a href="README_jpn.md">🇯🇵 日本語</a> |
  <a href="README_por.md">🇵🇹 Português</a>
</p>

### Alarmknoten und Alarmzentrale: die Zonen, das Scharfschalten, die Verzögerungen, die Sirene und die PIN der Hausalarmanlage, mit der Firmware des Knotens und den auf einem Rechner getesteten Regeln (nichts lief auf einer Platine)

<p align="center">
  <img src="https://img.shields.io/badge/License-GPL%203.0-blue.svg" alt="GPL 3.0">
  <img src="https://img.shields.io/badge/Language-C%2B%2B17-00599c.svg" alt="Language">
  <img src="https://img.shields.io/badge/Zones-wired%20%2B%20Zigbee-ffb020.svg" alt="Zones">
  <img src="https://img.shields.io/badge/Checks-521%2C307-2ea44f.svg" alt="Checks">
  <img src="https://img.shields.io/badge/Version-0.3.0-0f6cbd.svg" alt="Version">
  <img src="https://img.shields.io/badge/Maturity-functional-00E5FF.svg" alt="Maturity">
</p>

---

**Ehrlichkeitsprüfung - was heute läuft:** Es existiert der **Alarmkern** (was eine Zone ist, wie eine vom ADC gelesene verdrahtete Schleife zum Zustand wird, die Regeln der Zentrale: Scharfschalten, Verzögerungen, der Alarm, die Sirene, die PIN mit Sperre) und die darauf gebaute **Firmware des Knotens** (Einstellungen, Web-Panel, Nachrichten mit dem Server, die Brücke des Zigbee-Funkmoduls, ein Zigbee-Koordinator im Netzwerk, Relais, Bluetooth, signierte Updates), die für beide Platinen baut; der Kern, die Einstellungen, die Nachrichten und das Panel in einem echten Browser gegen einen Ersatzknoten werden auf einem Computer mit 521.307 Prüfungen in 6 Testprogrammen getestet. **Nichts lief auf einer Platine oder einer echten Anlage, keine Zone wurde verdrahtet und keine Sirene angeschlossen, die Zigbee-Kommunikation des Knotens mit einem Koordinator (lokal oder im Netzwerk) ist geschrieben und gegen einen Ersatzkoordinator getestet, lief aber nie gegen einen echten Koordinator oder Sensor, und dies ist keine zertifizierte Alarmanlage.**

---

## 📋 Auf einen Blick

| | |
|---|---|
| Version | `0.3.0` |
| Reifegrad | Funktionsfähig |
| Tests | 521,307 checks in 6 programs (`ctest`) |
| Technik | C++17 host-tested core; ESP-IDF 5.5 firmware for the ESP32-S3 (PoE Ethernet and Wi-Fi boards) |
| Auf echter Hardware | Noch keiner: keine Platine, Zone, Sirene oder kein Zigbee-Koordinator angeschlossen (nur Computertests, der Build und das Panel im Browser) |

## 🎯 Überblick

* **Zonen:** vier Arten - *sofort* (eine Tür, ein Fenster), *Eingang* (die Tür, durch die man hereinkommt: sie startet die Eingangsverzögerung), *Innenraum* (Bewegung: nur bei „abwesend“) und *immer* (Rauch, Gas, Panik: scharf oder nicht). Eine Zone kann eine verdrahtete Schleife oder ein Zigbee-Sensor sein.
* **Die verdrahtete Schleife:** ein Kontakt mit Abschlusswiderstand, als Spannung gelesen, ergibt vier Zustände an einem Pin - normal, ausgelöst und Sabotage bei kurzgeschlossenem oder durchtrenntem Draht. Die Grenzwerte sind Einstellungen; ein Entpreller verhindert, dass ein prellender Kontakt den Alarm auslöst.
* **Die Zentrale:** Scharfschalten *abwesend* und *anwesend*, eine Ausgangsverzögerung, die den Gehenden nicht als Eindringling behandelt, eine Eingangsverzögerung, ein Alarm, der **bis zum richtigen Unscharfschalten bleibt**, eine Sirene, die nach einer Zeit von selbst aufhört, das Überbrücken offener Zonen bei Erzwingen (nie ein ertönender Rauchmelder) und ein Protokoll der letzten hundert Ereignisse.
* **Die PIN:** zum Scharfschalten ist keine nötig, zum Unscharfschalten und Stummschalten schon; falsche PINs werden gezählt, und nach einigen wird alles eine Weile abgelehnt, auch die richtige PIN.
* **Der Plan:** ein Gateway auf der Platine des Elektroknotens (Zigbee-Funk, verdrahtete Zonen, Sirenenrelais), das mit ARMOR-SERVER arbeitet und **weiter überwacht, wenn der Server fehlt**: eine Zentrale übernimmt das Funkmodul vom Server über die Zigbee-Brücke von ARMOR-ELECTRICAL. Siehe [den Entwurf](docs/DESIGN.md) und [die Übernahme](docs/FAILOVER.md).
* **Der Knoten:** Firmware für den ESP32-S3 (PoE-Ethernet oder WLAN) mit bis zu acht verdrahteten Zonen, der Sirene an einem Relais, dem Webpanel aller Knoten in acht Sprachen, dem an Zigbee2MQTT verliehenen Zigbee-Funk und den Nachrichten `armor/alarm/<Knoten>/state|command|result`; der Server darf nur scharf- und unscharfschalten, wenn drei unabhängige Schalter es erlauben. Siehe [die Firmware](docs/NODE_FIRMWARE.md), [die Nachrichten](docs/ALARM_MESSAGES.md) und [die Hardware](docs/HARDWARE.md).
* **Funkmodul übernehmen:** ist Zigbee2MQTT des Servers eine festgelegte Zeit weg, startet der Knoten den Koordinator selbst (ZNP), liest die Sensoren der Zonen, die einen nennen (IAS Zone), und steuert Zigbee-Sirenen (IAS WD); er gibt es frei, sobald der Client zurück ist. **Noch nicht:** ein echter Koordinator oder Sensor, die Hardware und jeder Test an einer echten Anlage.
* **Ein Neustart schaltet das Haus nicht unscharf:** Modus, ein laufender Alarm und die ausgeschlossenen Zonen werden im Flash gehalten und beim Start zurückgeholt; eine unterbrochene oder kurzgeschlossene Schleife wird nie ausgeschlossen; Nachrichten laufen über eine Warteschlange (kein Deadlock mit dem Broker), und der Watchdog startet die Platine neu, wenn die Aufgabe, die die Zonen liest, stehen bleibt.
* **Signierte Over-the-Air-Updates:** der Knoten installiert von GitHub nur ein Abbild, dessen SHA-256 stimmt und das eine gültige Ed25519-Signatur des Betreuers trägt (der öffentliche Schlüssel steckt in der Firmware, der private verlässt den Betreuer nie); eine von einem Administrator im Panel von Hand hochgeladene Datei wird weiterhin akzeptiert. Seiten und Meldungen des Panels sind in acht Sprachen.
* **Bluetooth-Kopplung mit PIN:** die sechsstellige PIN wird aus dem Einrichtungscode abgeleitet, auf der USB-Konsole ausgegeben und einem Administrator im Panel angezeigt; das Telefon muss sie eingeben, bevor es die Einstellungen des Knotens schreiben darf.

## 🖼️ Screenshots

Aufgenommen mit 1920×1080 auf einem Prüfstand, mit Beispieldaten und ohne eine echte Anlage dahinter.

<table>
<tr><td width="50%" valign="top"><img src="images/screenshots/overview.png" alt="Übersicht: der Zustand des Knotens auf einen Blick - Netzwerk, Broker, Firmware und was er meldet." width="100%"><br><sub>Übersicht: der Zustand des Knotens auf einen Blick - Netzwerk, Broker, Firmware und was er meldet.</sub></td><td width="50%" valign="top"><img src="images/screenshots/zones.png" alt="Zonen: jede verdrahtete oder Zigbee-Zone der Zentrale, ihre Art, Haltezeit und der Live-Zustand." width="100%"><br><sub>Zonen: jede verdrahtete oder Zigbee-Zone der Zentrale, ihre Art, Haltezeit und der Live-Zustand.</sub></td></tr>
<tr><td width="50%" valign="top"><img src="images/screenshots/zigbee.png" alt="Zigbee-Funk: die serielle Brücke zum Koordinatormodul, welcher Client sie nutzen darf, und der Zustand der Brücke." width="100%"><br><sub>Zigbee-Funk: die serielle Brücke zum Koordinatormodul, welcher Client sie nutzen darf, und der Zustand der Brücke.</sub></td><td width="50%" valign="top"><img src="images/screenshots/inputs-outputs.png" alt="Relais und Sirene: die Ausgänge der Basisplatine und was jeden ansteuert." width="100%"><br><sub>Relais und Sirene: die Ausgänge der Basisplatine und was jeden ansteuert.</sub></td></tr>
</table>

## 🧩 Rolle in A.R.M.O.R.

ARMOR-ALARM ist die Alarmzentrale: Zonen, Scharfschalten, Verzögerungen, Sirene und PIN werden auf dem Knoten selbst entschieden, sodass das Haus bewacht bleibt, wenn der Server fehlt. [ARMOR-SERVER](https://github.com/JuanenRac/ARMOR-SERVER) zeigt sie, ordnet Zigbee-Sensoren ihren Zonen zu und schaltet sie, wenn Sie es erlauben, scharf und unscharf; das Menü *Alarmzentralen* und der *Alarm-Designer* von [ARMOR-STUDIO](https://github.com/JuanenRac/ARMOR-STUDIO) sowie die App [ARMOR-ANDROID-CONTROL](https://github.com/JuanenRac/ARMOR-ANDROID-CONTROL) steuern sie. Ihr Zigbee-Funkmodul kann auf der Platine sein oder ein Koordinator im Netzwerk.

## 📂 Struktur des Repositorys

```text
ARMOR-ALARM/
├── core/    alarm_zone, alarm_panel (the rules), alarm_config, alarm_message, alarm_runner (the node), plus the settings every node shares
├── main/    the ESP-IDF firmware: alarm_manager, the web server, the Zigbee bridge, the relays
├── panel/   the web panel (eight languages)
├── tests/   test_alarm (the rules), test_node (settings, messages, runner), test_release (update from GitHub), emit_samples + check_samples (the messages against the contract), test_zigbee (ZNP, IAS Zone and WD, the node's own conversation with a coordinator)
├── tools/   the stand-in node and the browser test of the panel
├── docs/    DESIGN, FAILOVER, SAFETY, NODE_FIRMWARE, ALARM_MESSAGES, HARDWARE, BASE_BOARD, ZIGBEE, BLE_PROVISIONING
└── images/  brand assets
```

## 🛠️ Entwicklungsumgebung

**Voraussetzungen:** a C++17 compiler for the host tests, the ESP-IDF 5.5 container to build the image, an ESP32-S3 board (PoE Ethernet or Wi-Fi)

```bash
cmake -S tests -B build/host && cmake --build build/host && ctest --test-dir build/host   # 521,307 checks: the rules, the settings, the messages, the runner and the update from GitHub
```

Siehe [den Entwurf](docs/DESIGN.md), [wie die Zentrale vom Server übernimmt](docs/FAILOVER.md) und die [Sicherheitshinweise](docs/SAFETY.md).

## 🔗 Verwandte Projekte

**A.R.M.O.R.** (Autonomous Radar & Multimodal Observation Range) ist ein Perimeter-Sicherheitssystem aus unabhängigen Repositorys. Jedes hat eine eigene Version, eigene Tests und ein eigenes README; hier ist die Familie:

* **[ARMOR-COMMON](https://github.com/JuanenRac/ARMOR-COMMON)** - Nachrichtenverträge, Validierer, Konformitätsvektoren und generierte Typen
* **[ARMOR-RADAR](https://github.com/JuanenRac/ARMOR-RADAR)** - Feldknoten-Firmware für ESP32-S3 mit drei Radaren und eigenem Web-Panel
* **[ARMOR-SOLAR](https://github.com/JuanenRac/ARMOR-SOLAR)** - Protokolle für Solar-Wechselrichter und -Batterien und die Nachrichten eines Gateway-Knotens
* **[ARMOR-ELECTRICAL](https://github.com/JuanenRac/ARMOR-ELECTRICAL)** - Elektroknoten: Zähler, die Nachricht der Netzmesswerte und die Regeln fürs Schalten
* **ARMOR-ALARM** (dieses Repository) - Alarmknoten und Alarmzentrale: Zonen, Scharfschalten, Verzögerungen, Sirene und PIN, mit dem Server oder ohne ihn
* **[ARMOR-HMI](https://github.com/JuanenRac/ARMOR-HMI)** - Touch-Panel: der Systemzustand auf einem Wandbildschirm, Scharf- und Quittieren sowie das Zuhause des Sprachassistenten
* **[ARMOR-NETWORK](https://github.com/JuanenRac/ARMOR-NETWORK)** - Das lokale Netzwerk: seine Geräte, das Internet und was sich ändert
* **[ARMOR-SERVER](https://github.com/JuanenRac/ARMOR-SERVER)** - Zentraler Koordinator: Telemetrie, Alarme, Geräte, Solarmesswerte und Kameras
* **[ARMOR-STUDIO](https://github.com/JuanenRac/ARMOR-STUDIO)** - Web-Konsole: Kameras, Radar, Alarme, Solarenergie und 2D/3D-Standortdesigner
* **[ARMOR-ANDROID-CONTROL](https://github.com/JuanenRac/ARMOR-ANDROID-CONTROL)** - Android-Bedienclient mit Live-Radar in 2D/3D
* **[ARMOR-SERVER-AI](https://github.com/JuanenRac/ARMOR-SERVER-AI)** - Visuelle Inferenzrichtlinie, die ihre Entscheidungen erklärt und nie handelt
* **[ARMOR-VISION](https://github.com/JuanenRac/ARMOR-VISION)** - Kamerazonen, Masken und Erkennungsregeln für den Jetson Orin NX
* **[ARMOR-VOICE-AI](https://github.com/JuanenRac/ARMOR-VOICE-AI)** - Offline-Sprachabsichten mit einer nicht fälschbaren Bestätigung
* **[ARMOR-HARDWARE](https://github.com/JuanenRac/ARMOR-HARDWARE)** - Gehäuse, Elektronik und die Abnahmematrix am Prüfstand
* **[ARMOR-DEVOPS](https://github.com/JuanenRac/ARMOR-DEVOPS)** - Bereitstellung, CM5-Prüfstand, Backup und TLS
* **[ARMOR-SIMULATOR](https://github.com/JuanenRac/ARMOR-SIMULATOR)** - Offline-Telemetriesimulator mit wiederholbaren Fehlern
* **[ARMOR-UPDATER](https://github.com/JuanenRac/ARMOR-UPDATER)** - Erkennt, installiert und aktualisiert die eigenen Repositories des Ökosystems
* **[ARMOR-DOCS](https://github.com/JuanenRac/ARMOR-DOCS)** - Architektur, Sicherheitsgrundlage und die Fähigkeitsmatrix

## 📚 Dokumentation und Community

Hier gibt es mehr zu lesen:

* [Fähigkeitsmatrix: was belegt ist und was nicht](https://github.com/JuanenRac/ARMOR-DOCS/blob/main/docs/CAPABILITY_MATRIX.md)
* [Projektkatalog: Versionen und wie die Repositorys voneinander abhängen](https://github.com/JuanenRac/ARMOR-DOCS/blob/main/docs/PROJECT_CATALOG.md)
* [Änderungsverlauf dieses Repositorys](CHANGELOG.md)
* [Lizenz (GPL-3.0-or-later)](LICENSE)
* Fragen, Ideen und Meldungen: electrohobby3d@gmail.com

## 👤 AUTOR

**JuanenRac (Electro Hobby 3D)** · electrohobby3d@gmail.com

## 📜 LIZENZ

GPL-3.0-or-later - siehe [LICENSE](LICENSE).
