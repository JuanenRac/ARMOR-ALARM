<p align="center">
  <img src="images/ARMOR_BANNER.svg" alt="ARMOR-ALARM banner" width="100%">
</p>

# 🚨 ARMOR-ALARM

<p align="center">
  <a href="README.md">🇺🇸 English</a> |
  <a href="README_spa.md">🇪🇸 Español</a> |
  🇫🇷 <b>Français</b> |
  <a href="README_ita.md">🇮🇹 Italiano</a> |
  <a href="README_deu.md">🇩🇪 Deutsch</a> |
  <a href="README_zho.md">🇨🇳 简体中文</a> |
  <a href="README_jpn.md">🇯🇵 日本語</a>
</p>

### Nœud et centrale d'alarme : les zones, l'armement, les temporisations, la sirène et le code PIN de l'alarme de la maison, avec le firmware du nœud et les règles testées sur un ordinateur (rien n'a tourné sur une carte)

<p align="center">
  <img src="https://img.shields.io/badge/License-GPL%203.0-blue.svg" alt="GPL 3.0">
  <img src="https://img.shields.io/badge/Language-C%2B%2B17-00599c.svg" alt="Language">
  <img src="https://img.shields.io/badge/Zones-wired%20%2B%20Zigbee-ffb020.svg" alt="Zones">
  <img src="https://img.shields.io/badge/Checks-492%2C368-2ea44f.svg" alt="Checks">
  <img src="https://img.shields.io/badge/Maturity-scaffolding-ff9800.svg" alt="Maturity">
</p>

---

**Vérification d'honnêteté - ce qui fonctionne aujourd'hui :** **Maturité : scaffolding.** Ce qui existe est le **cœur de l'alarme** (ce qu'est une zone, comment une boucle câblée lue par le CAN devient un état, les règles de la centrale : armement, temporisations, l'alarme, la sirène, le PIN avec verrouillage) et le **firmware du nœud** construit dessus (réglages, panneau web, messages avec le serveur, pont de la radio Zigbee et relais), qui se compile pour les deux cartes ; le cœur, les réglages, les messages et le panneau dans un vrai navigateur face à un nœud simulé sont testés sur un ordinateur par 492 368 contrôles. **Rien n'a tourné sur une carte ni sur une installation réelle, aucune zone n'a été câblée ni aucune sirène branchée, la pile Zigbee dont la centrale a besoin pour reprendre la radio au serveur n'est pas écrite, et ce n'est pas une centrale d'alarme certifiée.**

---

## 🎯 Présentation

* **Zones :** quatre types - *instantanée* (une porte, une fenêtre), *d'entrée* (la porte par laquelle on entre : elle lance la temporisation d'entrée), *intérieure* (mouvement : seulement en « absent ») et *toujours* (fumée, gaz, panique : armée ou non). Une zone peut être une boucle câblée ou un capteur Zigbee.
* **La boucle câblée :** un contact avec résistance de fin de ligne, lu comme une tension, donne quatre états sur une seule broche - normal, déclenché, et sabotage pour un fil en court-circuit ou coupé. Les seuils sont des réglages ; un filtre anti-rebond évite qu'un contact qui rebondit déclenche l'alarme.
* **La centrale :** armement *absent* et *présent*, une temporisation de sortie qui ne traite pas comme un intrus celui qui part, une temporisation d'entrée, une alarme qui **reste verrouillée** jusqu'à un désarmement correct, une sirène qui s'arrête seule au bout d'un moment, l'inhibition des zones ouvertes si on force (jamais un détecteur de fumée qui sonne) et un journal des cent derniers événements.
* **Le PIN :** l'armement n'en demande pas, le désarmement et l'arrêt de l'alarme si ; les PIN faux sont comptés et, après quelques-uns, tout est refusé un moment, même le bon PIN.
* **Le plan :** une passerelle sur la carte du nœud électrique (radio Zigbee, zones câblées, relais de sirène) qui travaille avec ARMOR-SERVER et **continue de surveiller quand le serveur est absent** : une centrale reprend la radio au serveur par le pont Zigbee d'ARMOR-ELECTRICAL. Voir [la conception](docs/DESIGN.md) et [la reprise](docs/FAILOVER.md).
* **Le nœud :** firmware pour l'ESP32-S3 (Ethernet PoE ou Wi-Fi) avec jusqu'à huit zones câblées, la sirène sur un relais, le panneau web de tous les nœuds en sept langues, la radio Zigbee prêtée à Zigbee2MQTT et les messages `armor/alarm/<nœud>/state|command|result` ; le serveur ne peut armer et désarmer que si trois interrupteurs indépendants l'autorisent. Voir [le firmware](docs/NODE_FIRMWARE.md), [les messages](docs/ALARM_MESSAGES.md) et [le matériel](docs/HARDWARE.md).
* **Pas encore :** la pile Zigbee dont la centrale a besoin pour reprendre la radio au serveur, le matériel et tout essai sur une installation réelle.

## 📂 Structure du dépôt

```text
ARMOR-ALARM/
├── core/    alarm_zone, alarm_panel (the rules), alarm_config, alarm_message, alarm_runner (the node), plus the settings every node shares
├── main/    the ESP-IDF firmware: alarm_manager, the web server, the Zigbee bridge, the relays
├── panel/   the web panel (seven languages)
├── tests/   test_alarm (the rules), test_node (settings, messages, runner), test_release (update from GitHub), emit_samples + check_samples (the messages against the contract)
├── tools/   the stand-in node and the browser test of the panel
├── docs/    DESIGN, FAILOVER, SAFETY, NODE_FIRMWARE, ALARM_MESSAGES, HARDWARE, BASE_BOARD, ZIGBEE, BLE_PROVISIONING
└── images/  brand assets
```

## 🛠️ Environnement de développement

```bash
cmake -S tests -B build/host && cmake --build build/host && ctest --test-dir build/host   # 492,368 checks: the rules, the settings, the messages, the runner and the update from GitHub
```

Voir [la conception](docs/DESIGN.md), [comment la centrale prend le relais du serveur](docs/FAILOVER.md) et les [notes de sécurité](docs/SAFETY.md).

## 🔗 Projets liés

**A.R.M.O.R.** (Autonomous Radar & Multimodal Observation Range) est un système de sécurité périmétrique composé de dépôts indépendants. Chacun a sa propre version, ses propres tests et son propre README ; voici la famille :

* **[ARMOR-COMMON](https://github.com/JuanenRac/ARMOR-COMMON)** - Contrats de messages, validateurs, vecteurs de conformité et types générés
* **[ARMOR-RADAR](https://github.com/JuanenRac/ARMOR-RADAR)** - Firmware du nœud de terrain pour ESP32-S3 avec trois radars et son propre panneau web
* **[ARMOR-SOLAR](https://github.com/JuanenRac/ARMOR-SOLAR)** - Protocoles des onduleurs et batteries solaires et messages d'un nœud passerelle
* **[ARMOR-ELECTRICAL](https://github.com/JuanenRac/ARMOR-ELECTRICAL)** - Nœud électrique : compteurs, le message des mesures du réseau et les règles de commutation
* **ARMOR-ALARM** (ce dépôt) - Nœud et centrale d'alarme : zones, armement, temporisations, sirène et PIN, avec ou sans le serveur
* **[ARMOR-ALARM](https://github.com/JuanenRac/ARMOR-ALARM)** - Nœud et centrale d'alarme : zones, armement, temporisations, sirène et PIN, avec ou sans le serveur
* **[ARMOR-HMI](https://github.com/JuanenRac/ARMOR-HMI)** - Panneau tactile : l'état du système sur un écran mural, armer et acquitter, et la maison de l'assistant vocal
* **[ARMOR-NETWORK](https://github.com/JuanenRac/ARMOR-NETWORK)** - Le réseau local : ses appareils, internet et ce qui change
* **[ARMOR-SERVER](https://github.com/JuanenRac/ARMOR-SERVER)** - Coordinateur central : télémétrie, alarmes, appareils, relevés solaires et caméras
* **[ARMOR-STUDIO](https://github.com/JuanenRac/ARMOR-STUDIO)** - Console web : caméras, radar, alarmes, énergie solaire et concepteur de site 2D/3D
* **[ARMOR-ANDROID-CONTROL](https://github.com/JuanenRac/ARMOR-ANDROID-CONTROL)** - Client Android de l'opérateur avec radar 2D/3D en direct
* **[ARMOR-SERVER-AI](https://github.com/JuanenRac/ARMOR-SERVER-AI)** - Politique d'inférence visuelle qui explique ses décisions et n'agit jamais
* **[ARMOR-VOICE-AI](https://github.com/JuanenRac/ARMOR-VOICE-AI)** - Intentions vocales hors ligne avec une confirmation impossible à falsifier
* **[ARMOR-HARDWARE](https://github.com/JuanenRac/ARMOR-HARDWARE)** - Boîtiers, électronique et matrice d'acceptation sur banc
* **[ARMOR-DEVOPS](https://github.com/JuanenRac/ARMOR-DEVOPS)** - Déploiement, banc d'essai CM5, sauvegarde et TLS
* **[ARMOR-SIMULATOR](https://github.com/JuanenRac/ARMOR-SIMULATOR)** - Simulateur de télémétrie hors ligne avec des pannes reproductibles
* **[ARMOR-UPDATER](https://github.com/JuanenRac/ARMOR-UPDATER)** - Détecte, installe et met à jour les propres dépôts de l'écosystème
* **[ARMOR-DOCS](https://github.com/JuanenRac/ARMOR-DOCS)** - Architecture, base de sécurité et matrice des capacités

## 📚 Documentation et communauté

Pour en savoir plus :

* [Matrice des capacités : ce qui est prouvé et ce qui ne l'est pas](https://github.com/JuanenRac/ARMOR-DOCS/blob/main/docs/CAPABILITY_MATRIX.md)
* [Catalogue des projets : versions et dépendances entre les dépôts](https://github.com/JuanenRac/ARMOR-DOCS/blob/main/docs/PROJECT_CATALOG.md)
* [Historique des modifications de ce dépôt](CHANGELOG.md)
* [Licence (GPL-3.0-or-later)](LICENSE)
* Questions, idées et rapports : electrohobby3d@gmail.com

## 👤 AUTEUR

**JuanenRac (Electro Hobby 3D)** · electrohobby3d@gmail.com

## 📜 LICENCE

GPL-3.0-or-later - voir [LICENSE](LICENSE).
