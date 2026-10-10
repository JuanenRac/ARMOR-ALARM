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
  <a href="README_jpn.md">🇯🇵 日本語</a>
</p>

### Alarmknoten und Alarmzentrale: die Zonen, das Scharfschalten, die Verzögerungen, die Sirene und die PIN der Hausalarmanlage, als Regeln, die auf einem Rechner getestet sind (noch keine Firmware; nichts lief auf einer Platine)

<p align="center">
  <img src="https://img.shields.io/badge/License-GPL%203.0-blue.svg" alt="GPL 3.0">
  <img src="https://img.shields.io/badge/Language-C%2B%2B17-00599c.svg" alt="Language">
  <img src="https://img.shields.io/badge/Zones-wired%20%2B%20Zigbee-ffb020.svg" alt="Zones">
  <img src="https://img.shields.io/badge/Checks-492%2C806-2ea44f.svg" alt="Checks">
  <img src="https://img.shields.io/badge/Maturity-scaffolding-ff9800.svg" alt="Maturity">
</p>

---

**Ehrlichkeitsprüfung - was heute läuft:** **Reifegrad: Scaffolding.** Der Kern (die Rahmen der Zähler PZEM-004T v3 und PZEM-017, der Bus, der sie abfragt, die Nachricht des Knotens und die Regeln fürs Schalten: 53 Prüfungen der Zähler und 135.736 der Schaltregeln) wird auf einem Rechner mit nachgebildeten Zählern und Schützen getestet, und die Nachrichten, die er erzeugt, akzeptiert ARMOR-COMMON. Die Firmware des Knotens (die Einstellungen, die serielle Leitung und die Seiten Zähler und Messwerte ihres Panels: 71 weitere Prüfungen) baut im ESP-IDF-5.4.2-Container für beide Platinen. **Sie lief noch nie auf einer Platine, nichts wurde an einen Zähler, ein Schütz, ein Relais oder das Funkmodul angeschlossen, und hier schaltet nichts von selbst irgendetwas.**

---

## 🎯 Überblick

* **Zonen:** vier Arten - *sofort* (eine Tür, ein Fenster), *Eingang* (die Tür, durch die man hereinkommt: sie startet die Eingangsverzögerung), *Innenraum* (Bewegung: nur bei „abwesend“) und *immer* (Rauch, Gas, Panik: scharf oder nicht). Eine Zone kann eine verdrahtete Schleife oder ein Zigbee-Sensor sein.
* **Die verdrahtete Schleife:** ein Kontakt mit Abschlusswiderstand, als Spannung gelesen, ergibt vier Zustände an einem Pin - normal, ausgelöst und Sabotage bei kurzgeschlossenem oder durchtrenntem Draht. Die Grenzwerte sind Einstellungen; ein Entpreller verhindert, dass ein prellender Kontakt den Alarm auslöst.
* **Die Zentrale:** Scharfschalten *abwesend* und *anwesend*, eine Ausgangsverzögerung, die den Gehenden nicht als Eindringling behandelt, eine Eingangsverzögerung, ein Alarm, der **bis zum richtigen Unscharfschalten bleibt**, eine Sirene, die nach einer Zeit von selbst aufhört, das Überbrücken offener Zonen bei Erzwingen (nie ein ertönender Rauchmelder) und ein Protokoll der letzten hundert Ereignisse.
* **Die PIN:** zum Scharfschalten ist keine nötig, zum Unscharfschalten und Stummschalten schon; falsche PINs werden gezählt, und nach einigen wird alles eine Weile abgelehnt, auch die richtige PIN.
* **Der Plan:** ein Gateway auf der Platine des Elektroknotens (Zigbee-Funk, verdrahtete Zonen, Sirenenrelais), das mit ARMOR-SERVER arbeitet und **weiter überwacht, wenn der Server fehlt**: eine Zentrale übernimmt das Funkmodul vom Server über die Zigbee-Brücke von ARMOR-ELECTRICAL. Siehe [den Entwurf](docs/DESIGN.md) und [die Übernahme](docs/FAILOVER.md).
* **Noch nicht:** die Firmware, der Zigbee-Stack, den die Zentrale zum Übernehmen des Funkmoduls braucht, die Nachrichten mit dem Server, die Hardware und jeder Test an einer echten Anlage.

## 📂 Struktur des Repositorys

```text
ARMOR-ALARM/
├── core/    alarm_zone (what a zone is, the wired loop and its end-of-line resistor, the debouncer), alarm_panel (arming, delays, alarm, siren, PIN)
├── tests/   test_alarm (scripted scenarios and a long random run of the rules that must always hold)
├── docs/    DESIGN, FAILOVER, SAFETY
└── images/  brand assets
```

## 🛠️ Entwicklungsumgebung

```bash
cmake -S tests -B build/host && cmake --build build/host && ctest --test-dir build/host   # 492,806 checks: the zones, the loop, the debouncer and the panel's rules
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
