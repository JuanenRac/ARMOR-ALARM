<p align="center">
  <img src="images/ARMOR_BANNER.svg" alt="ARMOR-ALARM banner" width="100%">
</p>

# 🚨 ARMOR-ALARM

<p align="center">
  <a href="README.md">🇺🇸 English</a> |
  <a href="README_spa.md">🇪🇸 Español</a> |
  <a href="README_fra.md">🇫🇷 Français</a> |
  🇮🇹 <b>Italiano</b> |
  <a href="README_deu.md">🇩🇪 Deutsch</a> |
  <a href="README_zho.md">🇨🇳 简体中文</a> |
  <a href="README_jpn.md">🇯🇵 日本語</a> |
  <a href="README_por.md">🇵🇹 Português</a>
</p>

### Nodo e centrale d'allarme: le zone, l'inserimento, i ritardi, la sirena e il PIN dell'allarme di casa, con il firmware del nodo e le regole provate su un computer (nulla ha girato su una scheda)

<p align="center">
  <img src="https://img.shields.io/badge/License-GPL%203.0-blue.svg" alt="GPL 3.0">
  <img src="https://img.shields.io/badge/Language-C%2B%2B17-00599c.svg" alt="Language">
  <img src="https://img.shields.io/badge/Zones-wired%20%2B%20Zigbee-ffb020.svg" alt="Zones">
  <img src="https://img.shields.io/badge/Checks-492%2C368-2ea44f.svg" alt="Checks">
  <img src="https://img.shields.io/badge/Maturity-scaffolding-ff9800.svg" alt="Maturity">
</p>

---

**Controllo di onestà - cosa funziona oggi:** **Maturità: scaffolding.** Esistono il **nucleo dell'allarme** (cos'è una zona, come un anello cablato letto dall'ADC diventa uno stato, le regole della centrale: inserimento, ritardi, l'allarme, la sirena, il PIN con blocco) e il **firmware del nodo** costruito su di esso (impostazioni, pannello web, messaggi con il server, ponte della radio Zigbee e relè), che compila per entrambe le schede; il nucleo, le impostazioni, i messaggi e il pannello in un vero browser contro un nodo simulato sono provati su un computer con 496.185 controlli. **Nulla ha girato su una scheda né su un impianto reale, nessuna zona è stata cablata né alcuna sirena collegata, la conversazione Zigbee del nodo stesso con un coordinatore (prendere la radio al server) è scritta e provata contro un coordinatore simulato, ma mai con un coordinatore o sensori reali, e non è una centrale d'allarme certificata.**

---

## 🎯 Panoramica

* **Zone:** quattro tipi - *istantanea* (una porta, una finestra), *d'ingresso* (la porta da cui si entra: avvia il ritardo d'ingresso), *interna* (movimento: solo in "fuori casa") e *sempre* (fumo, gas, panico: inserita o no). Una zona può essere un anello cablato o un sensore Zigbee.
* **L'anello cablato:** un contatto con resistenza di fine linea, letto come tensione, dà quattro stati su un solo pin - normale, scattato e manomissione per un filo in cortocircuito o tagliato. I limiti sono impostazioni; un filtro antirimbalzo impedisce che un contatto che rimbalza faccia scattare l'allarme.
* **La centrale:** inserimento *fuori casa* e *in casa*, un ritardo di uscita che non tratta da intruso chi esce, un ritardo d'ingresso, un allarme che **resta memorizzato** fino a un disinserimento corretto, una sirena che si ferma da sola dopo un po', l'esclusione delle zone aperte se si forza (mai un rilevatore di fumo che suona) e un registro degli ultimi cento eventi.
* **Il PIN:** l'inserimento non lo chiede, il disinserimento e il silenziamento sì; i PIN errati vengono contati e, dopo alcuni, tutto viene rifiutato per un po', anche il PIN giusto.
* **Il piano:** un gateway sulla scheda del nodo elettrico (radio Zigbee, zone cablate, relè della sirena) che lavora con ARMOR-SERVER e **continua a sorvegliare quando il server manca**: una centrale prende la radio al server tramite il ponte Zigbee di ARMOR-ELECTRICAL. Vedi [il progetto](docs/DESIGN.md) e [il passaggio di consegne](docs/FAILOVER.md).
* **Il nodo:** firmware per l'ESP32-S3 (Ethernet PoE o Wi-Fi) con fino a otto zone cablate, la sirena su un relè, il pannello web di tutti i nodi in sette lingue, la radio Zigbee prestata a Zigbee2MQTT e i messaggi `armor/alarm/<nodo>/state|command|result`; il server può inserire e disinserire solo se tre interruttori indipendenti lo permettono. Vedi [il firmware](docs/NODE_FIRMWARE.md), [i messaggi](docs/ALARM_MESSAGES.md) e [l'hardware](docs/HARDWARE.md).
* **Prendere la radio:** quando Zigbee2MQTT del server è assente da un certo tempo, il nodo avvia da sé il coordinatore (ZNP), legge i sensori delle zone che ne nominano uno (IAS Zone) e comanda sirene Zigbee (IAS WD), e la rilascia appena il client torna. **Non ancora:** un coordinatore o sensore reali, l'hardware e qualsiasi prova su un impianto reale.
* **Un riavvio non disinserisce la casa:** la modalità, un allarme in corso e le zone escluse vengono conservati nella flash e ripresi all'avvio; un anello tagliato o in cortocircuito non viene mai escluso; i messaggi passano da una coda (nessuno stallo con il broker) e il watchdog riavvia la scheda se si ferma il task che legge le zone.

## 📂 Struttura del repository

```text
ARMOR-ALARM/
├── core/    alarm_zone, alarm_panel (the rules), alarm_config, alarm_message, alarm_runner (the node), plus the settings every node shares
├── main/    the ESP-IDF firmware: alarm_manager, the web server, the Zigbee bridge, the relays
├── panel/   the web panel (seven languages)
├── tests/   test_alarm (the rules), test_node (settings, messages, runner), test_release (update from GitHub), emit_samples + check_samples (the messages against the contract), test_zigbee (ZNP, IAS Zone and WD, the node's own conversation with a coordinator)
├── tools/   the stand-in node and the browser test of the panel
├── docs/    DESIGN, FAILOVER, SAFETY, NODE_FIRMWARE, ALARM_MESSAGES, HARDWARE, BASE_BOARD, ZIGBEE, BLE_PROVISIONING
└── images/  brand assets
```

## 🛠️ Ambiente di sviluppo

```bash
cmake -S tests -B build/host && cmake --build build/host && ctest --test-dir build/host   # 496.185 checks: the rules, the settings, the messages, the runner and the update from GitHub
```

Vedi [il progetto](docs/DESIGN.md), [come la centrale subentra al server](docs/FAILOVER.md) e le [note di sicurezza](docs/SAFETY.md).

## 🔗 Progetti correlati

**A.R.M.O.R.** (Autonomous Radar & Multimodal Observation Range) è un sistema di sicurezza perimetrale fatto di repository indipendenti. Ognuno ha la propria versione, i propri test e il proprio README; ecco la famiglia:

* **[ARMOR-COMMON](https://github.com/JuanenRac/ARMOR-COMMON)** - Contratti dei messaggi, validatori, vettori di conformità e tipi generati
* **[ARMOR-RADAR](https://github.com/JuanenRac/ARMOR-RADAR)** - Firmware del nodo di campo per ESP32-S3 con tre radar e un proprio pannello web
* **[ARMOR-SOLAR](https://github.com/JuanenRac/ARMOR-SOLAR)** - Protocolli di inverter e batterie solari e messaggi di un nodo gateway
* **[ARMOR-ELECTRICAL](https://github.com/JuanenRac/ARMOR-ELECTRICAL)** - Nodo elettrico: contatori, il messaggio delle letture della rete e le regole di manovra
* **ARMOR-ALARM** (questo repository) - Nodo e centrale d'allarme: zone, inserimento, ritardi, sirena e PIN, con il server o senza
* **[ARMOR-ALARM](https://github.com/JuanenRac/ARMOR-ALARM)** - Nodo e centrale d'allarme: zone, inserimento, ritardi, sirena e PIN, con il server o senza
* **[ARMOR-HMI](https://github.com/JuanenRac/ARMOR-HMI)** - Pannello touch: lo stato del sistema su uno schermo a parete, attivare e riconoscere gli allarmi, e la casa dell'assistente vocale
* **[ARMOR-NETWORK](https://github.com/JuanenRac/ARMOR-NETWORK)** - La rete locale: i suoi dispositivi, internet e ciò che cambia
* **[ARMOR-SERVER](https://github.com/JuanenRac/ARMOR-SERVER)** - Coordinatore centrale: telemetria, allarmi, dispositivi, letture solari e telecamere
* **[ARMOR-STUDIO](https://github.com/JuanenRac/ARMOR-STUDIO)** - Console web: telecamere, radar, allarmi, energia solare e progettista del sito 2D/3D
* **[ARMOR-ANDROID-CONTROL](https://github.com/JuanenRac/ARMOR-ANDROID-CONTROL)** - Client Android dell'operatore con radar 2D/3D in tempo reale
* **[ARMOR-SERVER-AI](https://github.com/JuanenRac/ARMOR-SERVER-AI)** - Politica di inferenza visiva che spiega le sue decisioni e non agisce mai
* **[ARMOR-VISION](https://github.com/JuanenRac/ARMOR-VISION)** - Zone delle telecamere, maschere e regole di rilevamento per la Jetson Orin NX
* **[ARMOR-VOICE-AI](https://github.com/JuanenRac/ARMOR-VOICE-AI)** - Intenti vocali offline con una conferma impossibile da falsificare
* **[ARMOR-HARDWARE](https://github.com/JuanenRac/ARMOR-HARDWARE)** - Contenitori, elettronica e matrice di accettazione da banco
* **[ARMOR-DEVOPS](https://github.com/JuanenRac/ARMOR-DEVOPS)** - Distribuzione, banco di prova CM5, backup e TLS
* **[ARMOR-SIMULATOR](https://github.com/JuanenRac/ARMOR-SIMULATOR)** - Simulatore di telemetria offline con guasti ripetibili
* **[ARMOR-UPDATER](https://github.com/JuanenRac/ARMOR-UPDATER)** - Rileva, installa e aggiorna i repository stessi dell'ecosistema
* **[ARMOR-DOCS](https://github.com/JuanenRac/ARMOR-DOCS)** - Architettura, base di sicurezza e matrice delle capacità

## 📚 Documentazione e comunità

Dove leggere di più:

* [Matrice delle capacità: cosa è provato e cosa no](https://github.com/JuanenRac/ARMOR-DOCS/blob/main/docs/CAPABILITY_MATRIX.md)
* [Catalogo dei progetti: versioni e dipendenze tra i repository](https://github.com/JuanenRac/ARMOR-DOCS/blob/main/docs/PROJECT_CATALOG.md)
* [Cronologia delle modifiche di questo repository](CHANGELOG.md)
* [Licenza (GPL-3.0-or-later)](LICENSE)
* Domande, idee e segnalazioni: electrohobby3d@gmail.com

## 👤 AUTORE

**JuanenRac (Electro Hobby 3D)** · electrohobby3d@gmail.com

## 📜 LICENZA

GPL-3.0-or-later - vedi [LICENSE](LICENSE).
