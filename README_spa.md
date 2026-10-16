<p align="center">
  <img src="images/ARMOR_BANNER.svg" alt="ARMOR-ALARM banner" width="100%">
</p>

# 🚨 ARMOR-ALARM

<p align="center">
  <a href="README.md">🇺🇸 English</a> |
  🇪🇸 <b>Español</b> |
  <a href="README_fra.md">🇫🇷 Français</a> |
  <a href="README_ita.md">🇮🇹 Italiano</a> |
  <a href="README_deu.md">🇩🇪 Deutsch</a> |
  <a href="README_zho.md">🇨🇳 简体中文</a> |
  <a href="README_jpn.md">🇯🇵 日本語</a> |
  <a href="README_por.md">🇵🇹 Português</a>
</p>

### Nodo y central de alarma: las zonas, el armado, los retardos, la sirena y el PIN de la alarma de la casa, con el firmware del nodo y las reglas probadas en un ordenador (nada ha corrido en una placa)

<p align="center">
  <img src="https://img.shields.io/badge/License-GPL%203.0-blue.svg" alt="GPL 3.0">
  <img src="https://img.shields.io/badge/Language-C%2B%2B17-00599c.svg" alt="Language">
  <img src="https://img.shields.io/badge/Zones-wired%20%2B%20Zigbee-ffb020.svg" alt="Zones">
  <img src="https://img.shields.io/badge/Checks-492%2C368-2ea44f.svg" alt="Checks">
  <img src="https://img.shields.io/badge/Maturity-scaffolding-ff9800.svg" alt="Maturity">
</p>

---

**Comprobación de honestidad - qué funciona hoy:** **Madurez: scaffolding.** Lo que existe es el **núcleo de la alarma** (qué es una zona, cómo un bucle cableado leído por el ADC se convierte en un estado, las reglas de la central: armado, retardos, la alarma, la sirena, el PIN con bloqueo) y el **firmware del nodo** construido sobre él (ajustes, panel web, mensajes con el servidor, puente de la radio Zigbee y relés), que compila para las dos placas; el núcleo, los ajustes, los mensajes y el panel en un navegador real contra un nodo simulado están probados en un ordenador con 496.185 comprobaciones. **Nada ha corrido en una placa ni en una instalación real, no se ha cableado ninguna zona ni conectado ninguna sirena, la conversación Zigbee del propio nodo con un coordinador (tomar la radio al servidor) está escrita y probada contra un coordinador simulado, pero nunca con un coordinador ni con sensores reales, y esto no es una central de alarma certificada.**

---

## 🎯 Descripción general

* **Zonas:** cuatro tipos - *instantánea* (una puerta, una ventana), *de entrada* (la puerta por la que entras: inicia el retardo de entrada), *interior* (movimiento: solo en "fuera") y *siempre* (humo, gas, pánico: armada o no). Una zona puede ser un bucle cableado o un sensor Zigbee.
* **El bucle cableado:** un contacto con resistencia de fin de línea, leído como tensión, da cuatro estados en un solo pin - normal, disparada y sabotaje por un cable en cortocircuito o cortado. Los límites son ajustes; un filtro antirrebote evita que un contacto que rebota dispare la alarma.
* **La central:** armado *fuera* y *en casa*, un retardo de salida que no trata como intruso a quien se va, un retardo de entrada, una alarma que **se mantiene** hasta un desarmado correcto, una sirena que se para sola pasado un tiempo, anulación de zonas abiertas si se fuerza (nunca un detector de humo sonando) y un registro de los últimos cien eventos.
* **El PIN:** armar no lo pide, desarmar y silenciar sí; los PIN erróneos se cuentan y, tras unos pocos, se rechaza todo durante un tiempo, incluso el PIN correcto.
* **El plan:** una pasarela en la placa del nodo eléctrico (radio Zigbee, zonas cableadas, relé de sirena) que trabaja con ARMOR-SERVER y **sigue vigilando cuando el servidor falta**: una central toma la radio al servidor por el puente Zigbee de ARMOR-ELECTRICAL. Véanse [el diseño](docs/DESIGN.md) y [el relevo](docs/FAILOVER.md).
* **El nodo:** firmware para el ESP32-S3 (Ethernet PoE o Wi-Fi) con hasta ocho zonas cableadas, la sirena en un relé, el panel web de todos los nodos en siete idiomas, la radio Zigbee prestada a Zigbee2MQTT y los mensajes `armor/alarm/<nodo>/state|command|result`; el servidor solo puede armar y desarmar si tres interruptores independientes lo permiten. Véanse [el firmware](docs/NODE_FIRMWARE.md), [los mensajes](docs/ALARM_MESSAGES.md) y [el hardware](docs/HARDWARE.md).
* **Tomar la radio:** cuando Zigbee2MQTT del servidor lleva un tiempo ausente, el nodo levanta el coordinador por sí mismo (ZNP), lee los sensores de las zonas que nombran uno (IAS Zone) y manda a sirenas Zigbee (IAS WD), y lo suelta en cuanto vuelve el cliente. **Todavía no:** un coordinador o sensor reales, el hardware y cualquier prueba en una instalación real.
* **Un reinicio no desarma la casa:** el modo, una alarma en marcha y las zonas excluidas se guardan en la flash y se recuperan al arrancar; un lazo cortado o en cortocircuito nunca se excluye; los mensajes pasan por una cola (sin bloqueo mutuo con el broker) y el perro guardián reinicia la placa si se para la tarea que lee las zonas.

## 📂 Estructura del repositorio

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

## 🛠️ Entorno de desarrollo

```bash
cmake -S tests -B build/host && cmake --build build/host && ctest --test-dir build/host   # 496.185 checks: the rules, the settings, the messages, the runner and the update from GitHub
```

Véanse [el diseño](docs/DESIGN.md), [cómo la central toma el relevo del servidor](docs/FAILOVER.md) y las [notas de seguridad](docs/SAFETY.md).

## 🔗 Proyectos relacionados

**A.R.M.O.R.** (Autonomous Radar & Multimodal Observation Range) es un sistema de seguridad perimetral hecho de repositorios independientes. Cada uno tiene su propia versión, sus propias pruebas y su propio README; esta es la familia:

* **[ARMOR-COMMON](https://github.com/JuanenRac/ARMOR-COMMON)** - Contratos de mensajes, validadores, vectores de conformidad y tipos generados
* **[ARMOR-RADAR](https://github.com/JuanenRac/ARMOR-RADAR)** - Firmware del nodo de campo para ESP32-S3 con tres radares y su propio panel web
* **[ARMOR-SOLAR](https://github.com/JuanenRac/ARMOR-SOLAR)** - Protocolos de inversores y baterías solares y los mensajes de un nodo pasarela
* **[ARMOR-ELECTRICAL](https://github.com/JuanenRac/ARMOR-ELECTRICAL)** - Nodo eléctrico: contadores, el mensaje de las lecturas de la red y las reglas para maniobrar
* **ARMOR-ALARM** (este repositorio) - Nodo y central de alarma: zonas, armado, retardos, sirena y PIN, con el servidor o sin él
* **[ARMOR-ALARM](https://github.com/JuanenRac/ARMOR-ALARM)** - Nodo y central de alarma: zonas, armado, retardos, sirena y PIN, con el servidor o sin él
* **[ARMOR-HMI](https://github.com/JuanenRac/ARMOR-HMI)** - Panel táctil: el estado del sistema en una pantalla de pared, armar y reconocer alarmas, y el hogar del asistente de voz
* **[ARMOR-NETWORK](https://github.com/JuanenRac/ARMOR-NETWORK)** - La red local: sus dispositivos, internet y lo que cambia
* **[ARMOR-SERVER](https://github.com/JuanenRac/ARMOR-SERVER)** - Coordinador central: telemetría, alarmas, dispositivos, lecturas solares y cámaras
* **[ARMOR-STUDIO](https://github.com/JuanenRac/ARMOR-STUDIO)** - Consola web: cámaras, radar, alarmas, energía solar y el diseñador de sitio 2D/3D
* **[ARMOR-ANDROID-CONTROL](https://github.com/JuanenRac/ARMOR-ANDROID-CONTROL)** - Cliente Android del operador con radar 2D/3D en vivo
* **[ARMOR-SERVER-AI](https://github.com/JuanenRac/ARMOR-SERVER-AI)** - Política de inferencia visual que explica sus decisiones y nunca actúa
* **[ARMOR-VISION](https://github.com/JuanenRac/ARMOR-VISION)** - Zonas de cámara, máscaras y reglas de detección para la Jetson Orin NX
* **[ARMOR-VOICE-AI](https://github.com/JuanenRac/ARMOR-VOICE-AI)** - Intenciones de voz sin conexión con una confirmación imposible de falsificar
* **[ARMOR-HARDWARE](https://github.com/JuanenRac/ARMOR-HARDWARE)** - Cajas, electrónica y la matriz de aceptación en banco
* **[ARMOR-DEVOPS](https://github.com/JuanenRac/ARMOR-DEVOPS)** - Despliegue, el banco de pruebas de la CM5, copias de seguridad y TLS
* **[ARMOR-SIMULATOR](https://github.com/JuanenRac/ARMOR-SIMULATOR)** - Simulador de telemetría sin conexión con fallos repetibles
* **[ARMOR-UPDATER](https://github.com/JuanenRac/ARMOR-UPDATER)** - Detecta, instala y actualiza los propios repositorios del ecosistema
* **[ARMOR-DOCS](https://github.com/JuanenRac/ARMOR-DOCS)** - Arquitectura, base de seguridad y la matriz de capacidades

## 📚 Documentación y comunidad

Dónde leer más:

* [Matriz de capacidades: qué está probado y qué no](https://github.com/JuanenRac/ARMOR-DOCS/blob/main/docs/CAPABILITY_MATRIX.md)
* [Catálogo de proyectos: versiones y cómo dependen unos de otros](https://github.com/JuanenRac/ARMOR-DOCS/blob/main/docs/PROJECT_CATALOG.md)
* [Historial de cambios de este repositorio](CHANGELOG.md)
* [Licencia (GPL-3.0-or-later)](LICENSE)
* Preguntas, ideas e informes: electrohobby3d@gmail.com

## 👤 AUTOR

**JuanenRac (Electro Hobby 3D)** · electrohobby3d@gmail.com

## 📜 LICENCIA

GPL-3.0-or-later - véase [LICENSE](LICENSE).
