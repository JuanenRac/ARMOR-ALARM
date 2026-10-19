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
  <img src="https://img.shields.io/badge/Checks-482%2C535-2ea44f.svg" alt="Checks">
  <img src="https://img.shields.io/badge/Version-0.4.2-0f6cbd.svg" alt="Version">
  <img src="https://img.shields.io/badge/Maturity-functional-00E5FF.svg" alt="Maturity">
</p>

---

**Comprobación de honestidad - qué funciona hoy:** Lo que existe es el **núcleo de la alarma** (qué es una zona, cómo un bucle cableado leído por el ADC se convierte en un estado, las reglas de la central: armado, retardos, la alarma, la sirena, el PIN con bloqueo) y el **firmware del nodo** construido sobre él (ajustes, panel web, mensajes con el servidor, el puente de la radio Zigbee, un coordinador Zigbee en la red, relés, Bluetooth, actualizaciones firmadas), que compila para las dos placas; el núcleo, los ajustes, los mensajes y el panel en un navegador real contra un nodo de pega se prueban en un ordenador con 477.588 comprobaciones en 6 programas de test. **Nada ha corrido en una placa ni en una instalación real, no se ha cableado ninguna zona ni conectado ninguna sirena, la conversación Zigbee del propio nodo con un coordinador (local o en la red) está escrita y probada contra un coordinador de pega pero nunca ha corrido contra un coordinador ni un sensor reales, y esto no es un sistema de alarma certificado.**

---

## 📋 De un vistazo

| | |
|---|---|
| Versión | `0.4.2` |
| Madurez | Funcional |
| Tests | 486,098 checks in 13 programs (`ctest`) |
| Tecnología | C++17 host-tested core; ESP-IDF 5.4 firmware for the ESP32-S3 (PoE Ethernet and Wi-Fi boards) |
| En hardware real | Ninguno todavía: ninguna placa, zona, sirena ni coordinador Zigbee conectados (solo tests en ordenador, la compilación y el panel en un navegador) |

## 🎯 Descripción general

* **Zonas:** cuatro tipos - *instantánea* (una puerta, una ventana), *de entrada* (la puerta por la que entras: inicia el retardo de entrada), *interior* (movimiento: solo en "fuera") y *siempre* (humo, gas, pánico: armada o no). Una zona puede ser un bucle cableado o un sensor Zigbee.
* **El bucle cableado:** un contacto con resistencia de fin de línea, leído como tensión, da cuatro estados en un solo pin - normal, disparada y sabotaje por un cable en cortocircuito o cortado. Los límites son ajustes; un filtro antirrebote evita que un contacto que rebota dispare la alarma.
* **La central:** armado *fuera* y *en casa*, un retardo de salida que no trata como intruso a quien se va, un retardo de entrada, una alarma que **se mantiene** hasta un desarmado correcto, una sirena que se para sola pasado un tiempo, anulación de zonas abiertas si se fuerza (nunca un detector de humo sonando) y un registro de los últimos cien eventos.
* **El PIN:** armar no lo pide, desarmar y silenciar sí; los PIN erróneos se cuentan y, tras unos pocos, se rechaza todo durante un tiempo, incluso el PIN correcto.
* **Silenciar sin desarmar:** mientras suena la sirena, un PIN correcto la para al instante con el sistema aún armado: la alarma sigue enclavada y registrada, una zona que aún no ha sonado puede volver a hacerla sonar, y un PIN erróneo cuenta para el bloqueo como uno erróneo de desarmar.
* **El plan:** una pasarela en la placa del nodo eléctrico (radio Zigbee, zonas cableadas, relé de sirena) que trabaja con ARMOR-SERVER y **sigue vigilando cuando el servidor falta**: una central toma la radio al servidor por el puente Zigbee de ARMOR-ELECTRICAL. Véanse [el diseño](docs/DESIGN.md) y [el relevo](docs/FAILOVER.md).
* **El nodo:** firmware para el ESP32-S3 (Ethernet PoE o Wi-Fi) con hasta ocho zonas cableadas, la sirena en un relé, el panel web de todos los nodos en ocho idiomas, la radio Zigbee prestada a Zigbee2MQTT y los mensajes `armor/alarm/<nodo>/state|command|result`; el servidor solo puede armar y desarmar si tres interruptores independientes lo permiten. Véanse [el firmware](docs/NODE_FIRMWARE.md), [los mensajes](docs/ALARM_MESSAGES.md) y [el hardware](docs/HARDWARE.md).
* **Tomar la radio:** cuando Zigbee2MQTT del servidor lleva un tiempo ausente, el nodo levanta el coordinador por sí mismo (ZNP), lee los sensores de las zonas que nombran uno (IAS Zone) y manda a sirenas Zigbee (IAS WD), y lo suelta en cuanto vuelve el cliente. **Todavía no:** un coordinador o sensor reales, el hardware y cualquier prueba en una instalación real.
* **Un reinicio no desarma la casa:** el modo, una alarma en marcha y las zonas excluidas se guardan en la flash y se recuperan al arrancar; un lazo cortado o en cortocircuito nunca se excluye; los mensajes pasan por una cola (sin bloqueo mutuo con el broker) y el perro guardián reinicia la placa si se para la tarea que lee las zonas.
* **Actualizaciones por aire firmadas:** el nodo solo instala desde GitHub una imagen cuyo SHA-256 coincide y que lleva una firma Ed25519 válida del responsable (la clave pública está en el firmware, la privada nunca sale del responsable); un archivo subido a mano por un administrador en el panel se sigue aceptando. Las páginas y mensajes del panel están en ocho idiomas.
* **Emparejamiento Bluetooth con PIN:** el PIN de seis cifras se deriva del código de configuración, se imprime en la consola USB y se muestra a un administrador en el panel; el móvil tiene que escribirlo antes de poder cambiar los ajustes del nodo.
* **Sirena y reinicios:** el relé de la sirena es solo de la alarma (ni el broker, ni la central, ni el panel pueden moverlo, y no se abre al perder el broker); una zona que aún no había sonado vuelve a hacer sonar la sirena pasado su tiempo; tras un reinicio vuelven una alarma de humo o pánico con el sistema desarmado, una alarma silenciada y un bloqueo; un reset a mano se rechaza con la alarma armada; un sensor Zigbee que nunca habla cuenta como perdido.
* **Solo la casa puede usar el panel:** el panel responde por defecto a la red privada (o a las redes que su dueño indique, como una VPN), rechaza una lista que dejaría fuera a quien la guarda, cuenta las contraseñas erróneas por persona y por dirección, y funciona detrás del proxy del servidor para llegar a él desde fuera de casa sin publicar el nodo.**Una zona que no se puede leer no se muestra como cerrada:** si el conversor no arranca o un lazo no da lectura cinco rondas seguidas, la zona pasa a *avería* (trouble): no vigila, nunca hace sonar la sirena, impide armar (salvo forzando, y entonces queda excluida y anotada) y el servidor avisa.
* **Enlace cifrado opcional con el broker (TLS):** desactivado por defecto, así que el nodo habla MQTT sin cifrar como antes. Un interruptor y una caja para el certificado de la autoridad propia de la instalación (PEM, 2 KB como máximo) en la página Broker hacen que se conecte con `mqtts://`, confíe solo en esa autoridad y compruebe que el certificado del broker se hizo para la dirección a la que llegó; lo que no sea un único certificado PEM se rechaza. Cubierto con pruebas en el ordenador; el handshake aún no se ha ejecutado en una placa.
* **Perfiles de armado:** un comando de armar puede llevar un perfil (Fuera, En casa, Noche, el tuyo) que elige, para ese armado, qué zonas vigilan, cuáles disparan la alarma al instante o tras el retardo de entrada, los retardos de salida y entrada y si suena la sirena (una alarma silenciosa se queda enganchada, anotada y avisada). Nunca toca las zonas de humo, pánico o sabotaje, el PIN, el bloqueo, el silencio, un armado forzado ni una zona con problema; el perfil sobrevive a un reinicio y un armado sin perfil se comporta exactamente como antes. Probado contra las reglas antiguas con una ejecución aleatoria y contra el contrato compartido.
* **Informe de salud:** cada minuto más o menos el nodo publica cuánto lleva encendido y por qué se reinició la última vez, su memoria libre, su enlace y su señal Wi-Fi, cuántas veces se conectó al broker y el último error que anotó (`armor/health/<nodo>`, el contrato `node_health` de ARMOR-COMMON). El código que lo construye lo comparten todos los nodos y está probado en un ordenador; no se ha ejecutado en una placa.
* **Control desde el servidor:** Studio puede reiniciar el nodo limpiamente, pausarlo hasta una hora (se reanuda solo), poner su trabajo en espera (sin fin, se conserva al reiniciar, el nodo sigue conectado y respondiendo), hacer que se muestre, pedirle una instantánea de diagnóstico y marcarlo en mantenimiento (`armor/control/<nodo>/...`, el contrato `node_control` de ARMOR-COMMON). Cada orden son dos pasos con un token de un solo uso que pertenece a esa operación, solo el servidor puede escribirlas en el broker, y no existe restablecimiento de fábrica, borrado ni sueño profundo. Mientras el nodo está en pausa o en espera no se leen sus zonas, no se evalúa, arma ni hace sonar nada, no se publica estado y el relé de la sirena se mantiene abierto: la casa NO está protegida, y el informe de salud lo dice. El código es compartido por todos los nodos y se prueba en un ordenador; no ha funcionado en una placa.

## 🖼️ Capturas de pantalla

Capturadas a 1920×1080 en un banco de pruebas, con datos de ejemplo y sin ninguna instalación real detrás.

<table>
<tr><td width="50%" valign="top"><img src="images/screenshots/overview.png" alt="Resumen: el estado del nodo de un vistazo - red, broker, firmware y lo que reporta." width="100%"><br><sub>Resumen: el estado del nodo de un vistazo - red, broker, firmware y lo que reporta.</sub></td><td width="50%" valign="top"><img src="images/screenshots/zones.png" alt="Zonas: cada zona cableada o Zigbee del panel, su tipo, tiempo de retención y estado en vivo." width="100%"><br><sub>Zonas: cada zona cableada o Zigbee del panel, su tipo, tiempo de retención y estado en vivo.</sub></td></tr>
<tr><td width="50%" valign="top"><img src="images/screenshots/zigbee.png" alt="Radio Zigbee: el puente serie hacia el módulo coordinador, qué cliente puede usarlo y el estado del puente." width="100%"><br><sub>Radio Zigbee: el puente serie hacia el módulo coordinador, qué cliente puede usarlo y el estado del puente.</sub></td><td width="50%" valign="top"><img src="images/screenshots/inputs-outputs.png" alt="Relés y sirena: las salidas de la placa base y qué acciona cada una." width="100%"><br><sub>Relés y sirena: las salidas de la placa base y qué acciona cada una.</sub></td></tr>
</table>

## 🧩 Papel en A.R.M.O.R.

ARMOR-ALARM es la central de alarma: las zonas, el armado, los retardos, la sirena y el PIN se deciden en el propio nodo, así que la casa sigue vigilada cuando el servidor no está. [ARMOR-SERVER](https://github.com/JuanenRac/ARMOR-SERVER) la muestra, ata sensores Zigbee a sus zonas y, cuando lo permites, la arma y desarma; el menú *Centrales de alarma* y el *Diseñador de alarma* de [ARMOR-STUDIO](https://github.com/JuanenRac/ARMOR-STUDIO) y la app [ARMOR-ANDROID-CONTROL](https://github.com/JuanenRac/ARMOR-ANDROID-CONTROL) la manejan. Su radio Zigbee puede estar en la placa o ser un coordinador en la red.

## 📂 Estructura del repositorio

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

## 🛠️ Entorno de desarrollo

**Requisitos:** a C++17 compiler for the host tests, the ESP-IDF 5.4 container to build the image, an ESP32-S3 board (PoE Ethernet or Wi-Fi)

```bash
cmake -S tests -B build/host && cmake --build build/host && ctest --test-dir build/host   # 486,098 checks: the rules, the settings, the messages, the runner and the update from GitHub
```

Véanse [el diseño](docs/DESIGN.md), [cómo la central toma el relevo del servidor](docs/FAILOVER.md) y las [notas de seguridad](docs/SAFETY.md).

## 🔗 Proyectos relacionados

**A.R.M.O.R.** (Autonomous Radar & Multimodal Observation Range) es un sistema de seguridad perimetral hecho de repositorios independientes. Cada uno tiene su propia versión, sus propias pruebas y su propio README; esta es la familia:

* **[ARMOR-COMMON](https://github.com/JuanenRac/ARMOR-COMMON)** - Contratos de mensajes, validadores, vectores de conformidad y tipos generados
* **[ARMOR-RADAR](https://github.com/JuanenRac/ARMOR-RADAR)** - Firmware del nodo de campo para ESP32-S3 con tres radares y su propio panel web
* **[ARMOR-SOLAR](https://github.com/JuanenRac/ARMOR-SOLAR)** - Protocolos de inversores y baterías solares y los mensajes de un nodo pasarela
* **[ARMOR-ELECTRICAL](https://github.com/JuanenRac/ARMOR-ELECTRICAL)** - Nodo eléctrico: contadores, el mensaje de las lecturas de la red y las reglas para maniobrar
* **ARMOR-ALARM** (este repositorio) - Nodo y central de alarma: zonas, armado, retardos, sirena y PIN, con el servidor o sin él
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
