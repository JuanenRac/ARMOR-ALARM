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
  <a href="README_jpn.md">🇯🇵 日本語</a> |
  <a href="README_por.md">🇵🇹 Português</a>
</p>

### Nœud et centrale d'alarme : les zones, l'armement, les temporisations, la sirène et le code PIN de l'alarme de la maison, avec le firmware du nœud et les règles testées sur un ordinateur (rien n'a tourné sur une carte)

<p align="center">
  <img src="https://img.shields.io/badge/License-GPL%203.0-blue.svg" alt="GPL 3.0">
  <img src="https://img.shields.io/badge/Language-C%2B%2B17-00599c.svg" alt="Language">
  <img src="https://img.shields.io/badge/Zones-wired%20%2B%20Zigbee-ffb020.svg" alt="Zones">
  <img src="https://img.shields.io/badge/Checks-482%2C535-2ea44f.svg" alt="Checks">
  <img src="https://img.shields.io/badge/Version-0.4.2-0f6cbd.svg" alt="Version">
  <img src="https://img.shields.io/badge/Maturity-functional-00E5FF.svg" alt="Maturity">
</p>

---

**Vérification d'honnêteté - ce qui fonctionne aujourd'hui:** Ce qui existe, c'est le **cœur de l'alarme** (ce qu'est une zone, comment une boucle câblée lue par l'ADC devient un état, les règles de la centrale : armement, temporisations, l'alarme, la sirène, le PIN avec verrouillage) et le **firmware du nœud** construit dessus (réglages, panneau web, messages avec le serveur, le pont de la radio Zigbee, un coordinateur Zigbee sur le réseau, relais, Bluetooth, mises à jour signées), qui se compile pour les deux cartes ; le cœur, les réglages, les messages et le panneau dans un vrai navigateur contre un nœud factice sont testés sur un ordinateur avec 477 588 vérifications dans 6 programmes de test. **Rien n'a tourné sur une carte ni sur une installation réelle, aucune zone n'a été câblée ni aucune sirène connectée, la conversation Zigbee du nœud avec un coordinateur (local ou sur le réseau) est écrite et testée contre un coordinateur factice mais n'a jamais tourné contre un vrai coordinateur ni un vrai capteur, et ce n'est pas un système d'alarme certifié.**

---

## 📋 En un coup d'œil

| | |
|---|---|
| Version | `0.4.2` |
| Maturité | Fonctionnel |
| Tests | 486,098 checks in 13 programs (`ctest`) |
| Technologie | C++17 host-tested core; ESP-IDF 5.4 firmware for the ESP32-S3 (PoE Ethernet and Wi-Fi boards) |
| Sur matériel réel | Aucun pour l'instant : ni carte, ni zone, ni sirène, ni coordinateur Zigbee connectés (seulement des tests sur ordinateur, la compilation et le panneau dans un navigateur) |

## 🎯 Présentation

* **Zones :** quatre types - *instantanée* (une porte, une fenêtre), *d'entrée* (la porte par laquelle on entre : elle lance la temporisation d'entrée), *intérieure* (mouvement : seulement en « absent ») et *toujours* (fumée, gaz, panique : armée ou non). Une zone peut être une boucle câblée ou un capteur Zigbee.
* **La boucle câblée :** un contact avec résistance de fin de ligne, lu comme une tension, donne quatre états sur une seule broche - normal, déclenché, et sabotage pour un fil en court-circuit ou coupé. Les seuils sont des réglages ; un filtre anti-rebond évite qu'un contact qui rebondit déclenche l'alarme.
* **La centrale :** armement *absent* et *présent*, une temporisation de sortie qui ne traite pas comme un intrus celui qui part, une temporisation d'entrée, une alarme qui **reste verrouillée** jusqu'à un désarmement correct, une sirène qui s'arrête seule au bout d'un moment, l'inhibition des zones ouvertes si on force (jamais un détecteur de fumée qui sonne) et un journal des cent derniers événements.
* **Le PIN :** l'armement n'en demande pas, le désarmement et l'arrêt de l'alarme si ; les PIN faux sont comptés et, après quelques-uns, tout est refusé un moment, même le bon PIN.
* **Couper la sirène sans désarmer :** tant que la sirène sonne, un PIN correct l'arrête aussitôt alors que le système reste armé - l'alarme reste verrouillée et enregistrée, une zone qui n'a pas encore sonné peut la relancer, et un PIN faux compte pour le verrouillage comme un faux PIN de désarmement.
* **Le plan :** une passerelle sur la carte du nœud électrique (radio Zigbee, zones câblées, relais de sirène) qui travaille avec ARMOR-SERVER et **continue de surveiller quand le serveur est absent** : une centrale reprend la radio au serveur par le pont Zigbee d'ARMOR-ELECTRICAL. Voir [la conception](docs/DESIGN.md) et [la reprise](docs/FAILOVER.md).
* **Le nœud :** firmware pour l'ESP32-S3 (Ethernet PoE ou Wi-Fi) avec jusqu'à huit zones câblées, la sirène sur un relais, le panneau web de tous les nœuds en huit langues, la radio Zigbee prêtée à Zigbee2MQTT et les messages `armor/alarm/<nœud>/state|command|result` ; le serveur ne peut armer et désarmer que si trois interrupteurs indépendants l'autorisent. Voir [le firmware](docs/NODE_FIRMWARE.md), [les messages](docs/ALARM_MESSAGES.md) et [le matériel](docs/HARDWARE.md).
* **Prendre la radio :** quand Zigbee2MQTT du serveur est absent depuis un temps donné, le nœud démarre lui-même le coordinateur (ZNP), lit les capteurs des zones qui en nomment un (IAS Zone) et commande des sirènes Zigbee (IAS WD), et la rend dès que le client revient. **Pas encore :** un vrai coordinateur ou capteur, le matériel et tout essai sur une installation réelle.
* **Un redémarrage ne désarme pas la maison :** le mode, une alarme en cours et les zones exclues sont gardés en flash et repris au démarrage ; une boucle coupée ou en court-circuit n'est jamais exclue ; les messages passent par une file (pas d'interblocage avec le broker) et le chien de garde redémarre la carte si la tâche qui lit les zones s'arrête.
* **Mises à jour par radio signées :** le nœud n'installe depuis GitHub qu'une image dont le SHA-256 correspond et qui porte une signature Ed25519 valide du responsable (la clé publique est dans le firmware, la privée ne quitte jamais le responsable) ; un fichier envoyé à la main par un administrateur dans le panneau reste accepté. Les pages et messages du panneau sont en huit langues.
* **Appairage Bluetooth avec un PIN :** le PIN à six chiffres est dérivé du code de configuration, imprimé sur la console USB et montré à un administrateur dans le panneau ; le téléphone doit le saisir avant de pouvoir écrire les réglages du nœud.
* **Sirène et redémarrages :** le relais de la sirène appartient à l'alarme seule (ni le broker, ni la centrale, ni le panneau ne peuvent le bouger, et il ne s'ouvre pas à la perte du broker) ; une zone qui n'avait pas encore sonné refait sonner la sirène après son délai ; après un redémarrage reviennent une alarme de fumée ou de panique système désarmé, une alarme silencieuse et un blocage ; une réinitialisation à la main est refusée alarme armée ; un capteur Zigbee qui ne parle jamais compte comme perdu.
* **Seule la maison peut utiliser le panneau :** le panneau répond par défaut au réseau privé (ou aux réseaux que son propriétaire indique, comme un VPN), refuse une liste qui exclurait celui qui l'enregistre, compte les mauvais mots de passe par personne et par adresse, et fonctionne derrière le proxy du serveur pour être atteint depuis l'extérieur sans publier le nœud.**Une zone illisible n'est pas affichée comme fermée :** si le convertisseur ne démarre pas ou qu'une boucle ne donne aucune lecture pendant cinq tours de suite, la zone passe en *défaut* (trouble) - elle ne surveille pas, ne fait jamais sonner la sirène, empêche l'armement (sauf en le forçant, et elle est alors exclue et notée) et le serveur avertit.
* **Liaison chiffrée facultative avec le broker (TLS) :** désactivée par défaut, le nœud parle donc MQTT non chiffré comme avant. Un interrupteur et une zone pour le certificat de l'autorité propre à l'installation (PEM, 2 Ko au plus) dans la page Broker lui font se connecter en `mqtts://`, ne faire confiance qu'à cette autorité et vérifier que le certificat du broker a été établi pour l'adresse atteinte ; ce qui n'est pas un seul certificat PEM est refusé. Couvert par des tests sur ordinateur ; la poignée de main n'a pas encore été exécutée sur une carte.
* **Profils d'armement :** une commande d'armer peut porter un profil (Absent, Présent, Nuit, le vôtre) qui choisit, pour cet armement, quelles zones surveillent, lesquelles déclenchent l'alarme tout de suite ou après la temporisation d'entrée, les temporisations de sortie et d'entrée et si la sirène sonne (une alarme silencieuse reste verrouillée, notée et signalée). Il ne touche jamais les zones fumée, panique ou sabotage, le PIN, le verrouillage, le silence, un armement forcé ni une zone en défaut ; le profil survit à un redémarrage et un armement sans profil se comporte exactement comme avant. Testé contre les anciennes règles par une exécution aléatoire et contre le contrat partagé.
* **Rapport de santé :** environ une fois par minute le nœud publie depuis combien de temps il tourne et pourquoi il a redémarré la dernière fois, sa mémoire libre, sa liaison et son signal Wi-Fi, combien de fois il s'est connecté au broker et la dernière erreur notée (`armor/health/<nœud>`, le contrat `node_health` d'ARMOR-COMMON). Le code qui le construit est partagé par tous les nœuds et testé sur un ordinateur ; il n'a pas tourné sur une carte.

## 🖼️ Captures d'écran

Capturées en 1920×1080 sur un banc d'essai, avec des données d'exemple et sans installation réelle derrière.

<table>
<tr><td width="50%" valign="top"><img src="images/screenshots/overview.png" alt="Résumé : l'état du nœud en un coup d'œil - réseau, broker, firmware et ce qu'il remonte." width="100%"><br><sub>Résumé : l'état du nœud en un coup d'œil - réseau, broker, firmware et ce qu'il remonte.</sub></td><td width="50%" valign="top"><img src="images/screenshots/zones.png" alt="Zones : chaque zone filaire ou Zigbee de la centrale, son type, sa durée de maintien et son état en direct." width="100%"><br><sub>Zones : chaque zone filaire ou Zigbee de la centrale, son type, sa durée de maintien et son état en direct.</sub></td></tr>
<tr><td width="50%" valign="top"><img src="images/screenshots/zigbee.png" alt="Radio Zigbee : le pont série vers le module coordinateur, quel client peut l'utiliser et l'état du pont." width="100%"><br><sub>Radio Zigbee : le pont série vers le module coordinateur, quel client peut l'utiliser et l'état du pont.</sub></td><td width="50%" valign="top"><img src="images/screenshots/inputs-outputs.png" alt="Relais et sirène : les sorties de la carte de base et ce qui pilote chacune." width="100%"><br><sub>Relais et sirène : les sorties de la carte de base et ce qui pilote chacune.</sub></td></tr>
</table>

## 🧩 Rôle dans A.R.M.O.R.

ARMOR-ALARM est la centrale d'alarme : les zones, l'armement, les temporisations, la sirène et le PIN se décident sur le nœud lui-même, si bien que la maison reste surveillée quand le serveur est absent. [ARMOR-SERVER](https://github.com/JuanenRac/ARMOR-SERVER) la montre, lie des capteurs Zigbee à ses zones et, quand vous l'autorisez, l'arme et la désarme ; le menu *Centrales d'alarme* et le *Concepteur d'alarme* d'[ARMOR-STUDIO](https://github.com/JuanenRac/ARMOR-STUDIO) et l'application [ARMOR-ANDROID-CONTROL](https://github.com/JuanenRac/ARMOR-ANDROID-CONTROL) la pilotent. Sa radio Zigbee peut être sur la carte ou un coordinateur sur le réseau.

## 📂 Structure du dépôt

```text
ARMOR-ALARM/
├── core/    alarm_zone, alarm_panel (the rules), alarm_config, alarm_message, alarm_runner (the node), arm_profile (the arming profiles), plus the settings every node shares
├── main/    the ESP-IDF firmware: alarm_manager, the web server, the Zigbee bridge, the relays
├── panel/   the web panel (eight languages)
├── tests/   test_alarm (the rules), test_profiles (the arming profiles), test_node (settings, messages, runner), test_mqtt_tls (the optional encrypted link to the broker), test_release (update from GitHub), emit_samples + check_samples (the messages against the contract), test_zigbee (ZNP, IAS Zone and WD, the node's own conversation with a coordinator)
├── tools/   the stand-in node and the browser test of the panel
├── docs/    DESIGN, FAILOVER, SAFETY, NODE_FIRMWARE, ALARM_MESSAGES, HARDWARE, BASE_BOARD, ZIGBEE, BLE_PROVISIONING
└── images/  brand assets
```

## 🛠️ Environnement de développement

**Prérequis:** a C++17 compiler for the host tests, the ESP-IDF 5.4 container to build the image, an ESP32-S3 board (PoE Ethernet or Wi-Fi)

```bash
cmake -S tests -B build/host && cmake --build build/host && ctest --test-dir build/host   # 486,098 checks: the rules, the settings, the messages, the runner and the update from GitHub
```

Voir [la conception](docs/DESIGN.md), [comment la centrale prend le relais du serveur](docs/FAILOVER.md) et les [notes de sécurité](docs/SAFETY.md).

## 🔗 Projets liés

**A.R.M.O.R.** (Autonomous Radar & Multimodal Observation Range) est un système de sécurité périmétrique composé de dépôts indépendants. Chacun a sa propre version, ses propres tests et son propre README ; voici la famille :

* **[ARMOR-COMMON](https://github.com/JuanenRac/ARMOR-COMMON)** - Contrats de messages, validateurs, vecteurs de conformité et types générés
* **[ARMOR-RADAR](https://github.com/JuanenRac/ARMOR-RADAR)** - Firmware du nœud de terrain pour ESP32-S3 avec trois radars et son propre panneau web
* **[ARMOR-SOLAR](https://github.com/JuanenRac/ARMOR-SOLAR)** - Protocoles des onduleurs et batteries solaires et messages d'un nœud passerelle
* **[ARMOR-ELECTRICAL](https://github.com/JuanenRac/ARMOR-ELECTRICAL)** - Nœud électrique : compteurs, le message des mesures du réseau et les règles de commutation
* **ARMOR-ALARM** (ce dépôt) - Nœud et centrale d'alarme : zones, armement, temporisations, sirène et PIN, avec ou sans le serveur
* **[ARMOR-HMI](https://github.com/JuanenRac/ARMOR-HMI)** - Panneau tactile : l'état du système sur un écran mural, armer et acquitter, et la maison de l'assistant vocal
* **[ARMOR-NETWORK](https://github.com/JuanenRac/ARMOR-NETWORK)** - Le réseau local : ses appareils, internet et ce qui change
* **[ARMOR-SERVER](https://github.com/JuanenRac/ARMOR-SERVER)** - Coordinateur central : télémétrie, alarmes, appareils, relevés solaires et caméras
* **[ARMOR-STUDIO](https://github.com/JuanenRac/ARMOR-STUDIO)** - Console web : caméras, radar, alarmes, énergie solaire et concepteur de site 2D/3D
* **[ARMOR-ANDROID-CONTROL](https://github.com/JuanenRac/ARMOR-ANDROID-CONTROL)** - Client Android de l'opérateur avec radar 2D/3D en direct
* **[ARMOR-SERVER-AI](https://github.com/JuanenRac/ARMOR-SERVER-AI)** - Politique d'inférence visuelle qui explique ses décisions et n'agit jamais
* **[ARMOR-VISION](https://github.com/JuanenRac/ARMOR-VISION)** - Zones de caméra, masques et règles de détection pour la Jetson Orin NX
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
