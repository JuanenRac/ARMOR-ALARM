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
  <a href="README_jpn.md">🇯🇵 日本語</a>
</p>

### Nodo y central de alarma: las zonas, el armado, los retardos, la sirena y el PIN de la alarma de la casa, como reglas probadas en un ordenador (aún sin firmware; nada ha corrido en una placa)

<p align="center">
  <img src="https://img.shields.io/badge/License-GPL%203.0-blue.svg" alt="GPL 3.0">
  <img src="https://img.shields.io/badge/Language-C%2B%2B17-00599c.svg" alt="Language">
  <img src="https://img.shields.io/badge/Zones-wired%20%2B%20Zigbee-ffb020.svg" alt="Zones">
  <img src="https://img.shields.io/badge/Checks-492%2C806-2ea44f.svg" alt="Checks">
  <img src="https://img.shields.io/badge/Maturity-scaffolding-ff9800.svg" alt="Maturity">
</p>

---

**Comprobación de honestidad - qué funciona hoy:** **Madurez: scaffolding.** Lo que existe es el **núcleo de la alarma**, aparte de cualquier hardware: qué es una zona, cómo un bucle cableado leído por el ADC se convierte en un estado y las reglas de la central (armado, retardos de salida y entrada, la alarma, la sirena, el PIN con bloqueo), probadas en un ordenador con 492.806 comprobaciones. **Todavía no hay firmware, nada ha corrido en una placa ni en una instalación real, y esto no es una central de alarma certificada.**

---

## 🎯 Descripción general

* **Zonas:** cuatro tipos - *instantánea* (una puerta, una ventana), *de entrada* (la puerta por la que entras: inicia el retardo de entrada), *interior* (movimiento: solo en "fuera") y *siempre* (humo, gas, pánico: armada o no). Una zona puede ser un bucle cableado o un sensor Zigbee.
* **El bucle cableado:** un contacto con resistencia de fin de línea, leído como tensión, da cuatro estados en un solo pin - normal, disparada y sabotaje por un cable en cortocircuito o cortado. Los límites son ajustes; un filtro antirrebote evita que un contacto que rebota dispare la alarma.
* **La central:** armado *fuera* y *en casa*, un retardo de salida que no trata como intruso a quien se va, un retardo de entrada, una alarma que **se mantiene** hasta un desarmado correcto, una sirena que se para sola pasado un tiempo, anulación de zonas abiertas si se fuerza (nunca un detector de humo sonando) y un registro de los últimos cien eventos.
* **El PIN:** armar no lo pide, desarmar y silenciar sí; los PIN erróneos se cuentan y, tras unos pocos, se rechaza todo durante un tiempo, incluso el PIN correcto.
* **El plan:** una pasarela en la placa del nodo eléctrico (radio Zigbee, zonas cableadas, relé de sirena) que trabaja con ARMOR-SERVER y **sigue vigilando cuando el servidor falta**: una central toma la radio al servidor por el puente Zigbee de ARMOR-ELECTRICAL. Véanse [el diseño](docs/DESIGN.md) y [el relevo](docs/FAILOVER.md).
* **Todavía no:** el firmware, la pila Zigbee que la central necesita para tomar la radio, los mensajes con el servidor, el hardware y cualquier prueba en una instalación real.

## 📂 Estructura del repositorio

```text
ARMOR-ALARM/
├── core/    alarm_zone (what a zone is, the wired loop and its end-of-line resistor, the debouncer), alarm_panel (arming, delays, alarm, siren, PIN)
├── tests/   test_alarm (scripted scenarios and a long random run of the rules that must always hold)
├── docs/    DESIGN, FAILOVER, SAFETY
└── images/  brand assets
```

## 🛠️ Entorno de desarrollo

```bash
cmake -S tests -B build/host && cmake --build build/host && ctest --test-dir build/host   # 492,806 checks: the zones, the loop, the debouncer and the panel's rules
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
