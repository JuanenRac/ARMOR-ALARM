<p align="center">
  <img src="images/ARMOR_BANNER.svg" alt="ARMOR-ALARM banner" width="100%">
</p>

# 🚨 ARMOR-ALARM

<p align="center">
  <a href="README.md">🇺🇸 English</a> |
  <a href="README_spa.md">🇪🇸 Español</a> |
  <a href="README_fra.md">🇫🇷 Français</a> |
  <a href="README_ita.md">🇮🇹 Italiano</a> |
  <a href="README_deu.md">🇩🇪 Deutsch</a> |
  <a href="README_zho.md">🇨🇳 简体中文</a> |
  <a href="README_jpn.md">🇯🇵 日本語</a> |
  🇵🇹 <b>Português</b>
</p>


### Nó e central de alarme: as zonas, a armação, os atrasos, a sirene e o PIN do alarme da casa, com o firmware do nó e as regras testadas num computador (nada correu numa placa)

<p align="center">
  <img src="https://img.shields.io/badge/License-GPL%203.0-blue.svg" alt="GPL 3.0">
  <img src="https://img.shields.io/badge/Language-C%2B%2B17-00599c.svg" alt="Language">
  <img src="https://img.shields.io/badge/Zones-wired%20%2B%20Zigbee-ffb020.svg" alt="Zones">
  <img src="https://img.shields.io/badge/Checks-492%2C368-2ea44f.svg" alt="Checks">
  <img src="https://img.shields.io/badge/Maturity-scaffolding-ff9800.svg" alt="Maturity">
</p>

---

**Verificação de honestidade - o que funciona hoje:** **Maturidade: andaime.** O que existe é o **núcleo do alarme** (o que é uma zona, como um laço com fio lido pelo ADC se torna um estado, as regras da central: armar, atrasos, o alarme, a sirene, o PIN com bloqueio) e o **firmware do nó** construído sobre ele (definições, painel web, mensagens com o servidor, a ponte do rádio Zigbee e os relés), que compila para as duas placas; o núcleo, as definições, as mensagens e o painel num navegador real contra um nó substituto estão testados num computador com 496 185 verificações. **Nada correu numa placa nem numa instalação real, nenhuma zona foi ligada nem nenhuma sirene conectada, a própria conversa Zigbee do nó com um coordenador (tomar o rádio ao servidor) está escrita e testada contra um coordenador substituto, mas nunca correu contra um coordenador real nem um sensor real, e isto não é um sistema de alarme certificado.**

---

## 🎯 Visão geral

* **Zonas:** quatro tipos - *instantânea* (uma porta, uma janela), *de entrada* (a porta por onde entra: inicia o atraso de entrada), *interior* (movimento: só em «ausente») e *sempre* (fumo, gás, pânico: armada ou não). Uma zona pode ser um laço com fio ou um sensor Zigbee.
* **O laço com fio:** um contacto com uma resistência de fim de linha, lido como tensão, dá quatro estados num só pino - normal, disparado, e violação por um fio em curto-circuito ou cortado. Os limites são definições; um filtro de repique impede que um contacto que salta dispare o alarme.
* **A central:** armação *ausente* e *em casa*, um atraso de saída que não trata como intruso quem sai, um atraso de entrada, um alarme que **fica retido** até um desarme correto, uma sirene que pára sozinha passado um tempo, desvio de zonas abertas quando forçado (nunca um detetor de fumo a soar), e um registo dos últimos cem eventos.
* **O PIN:** armar não precisa de nenhum, desarmar e silenciar sim; os PIN errados contam-se e, passados alguns, tudo se recusa durante um tempo, mesmo o PIN certo.
* **O plano:** uma ponte na placa do nó elétrico (um rádio Zigbee, zonas com fio, um relé de sirene) que funciona com o ARMOR-SERVER e **continua a vigiar quando o servidor está ausente**: uma central toma o rádio ao servidor através da ponte Zigbee do ARMOR-ELECTRICAL. Veja [o desenho](docs/DESIGN.md) e [a continuidade](docs/FAILOVER.md).
* **O nó:** firmware para o ESP32-S3 (Ethernet PoE ou Wi-Fi) com até oito zonas com fio, a sirene num relé, o painel web de cada nó em sete idiomas, o rádio Zigbee emprestado ao Zigbee2MQTT, e as mensagens `armor/alarm/<node>/state|command|result`; o servidor só pode armar e desarmar quando três interruptores independentes o permitem. Veja [o firmware](docs/NODE_FIRMWARE.md), [as mensagens](docs/ALARM_MESSAGES.md) e [o hardware](docs/HARDWARE.md).
* **Tomar o rádio:** depois de o Zigbee2MQTT do servidor ter estado ausente durante um tempo definido, o nó levanta ele próprio o coordenador (ZNP), lê os sensores das zonas que o nomeiam (IAS Zone) e comanda sirenes Zigbee (IAS WD), e larga-o assim que o cliente volta. **Ainda não:** um coordenador ou sensor real, o hardware e qualquer teste numa instalação real.
* **Um reinício não desarma a casa:** o modo, um alarme em curso e as zonas deixadas de fora guardam-se na flash e retomam-se no arranque; um laço cortado ou em curto-circuito nunca se deixa de fora; as mensagens passam por uma fila (sem bloqueio mútuo com o broker) e o cão de guarda reinicia a placa se a tarefa que lê as zonas parar.

## 📂 Estrutura do repositório

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

## 🛠️ Ambiente de desenvolvimento

```bash
cmake -S tests -B build/host && cmake --build build/host && ctest --test-dir build/host   # 496,185 checks: the rules, the settings, the messages, the runner and the update from GitHub
```

Veja o [desenho](docs/DESIGN.md), [como a central toma o lugar do servidor](docs/FAILOVER.md) e as [notas de segurança](docs/SAFETY.md).

## 🔗 Projetos relacionados

O **A.R.M.O.R.** (Autonomous Radar & Multimodal Observation Range) é um sistema de segurança perimetral feito de repositórios independentes. Cada um tem a sua própria versão, os seus próprios testes e o seu próprio README; esta é a família:

* **[ARMOR-COMMON](https://github.com/JuanenRac/ARMOR-COMMON)** - Contratos de mensagens, validadores, vetores de conformidade e tipos gerados
* **[ARMOR-RADAR](https://github.com/JuanenRac/ARMOR-RADAR)** - Firmware de nó de campo para ESP32-S3 com três radares e o seu próprio painel web
* **[ARMOR-SOLAR](https://github.com/JuanenRac/ARMOR-SOLAR)** - Protocolos de inversores e baterias solares e as mensagens de um nó de ligação
* **[ARMOR-ELECTRICAL](https://github.com/JuanenRac/ARMOR-ELECTRICAL)** - Nó elétrico: contadores, a mensagem das leituras da rede e as regras de comutação
* **ARMOR-ALARM** (este repositório) - Nó e central de alarme: zonas, armar, atrasos, sirene e PIN, com o servidor ou sem ele
* **[ARMOR-ALARM](https://github.com/JuanenRac/ARMOR-ALARM)** - Nó e central de alarme: zonas, armar, atrasos, sirene e PIN, com o servidor ou sem ele
* **[ARMOR-HMI](https://github.com/JuanenRac/ARMOR-HMI)** - Painel tátil: o estado do sistema num ecrã de parede, armar e reconhecer, e a casa do assistente de voz
* **[ARMOR-NETWORK](https://github.com/JuanenRac/ARMOR-NETWORK)** - A rede local: os seus dispositivos, a internet e o que muda
* **[ARMOR-SERVER](https://github.com/JuanenRac/ARMOR-SERVER)** - Coordenador central: telemetria, alarmes, dispositivos, leituras solares e câmaras
* **[ARMOR-STUDIO](https://github.com/JuanenRac/ARMOR-STUDIO)** - Consola web: câmaras, radar, alarmes, energia solar e o desenhador de local 2D/3D
* **[ARMOR-ANDROID-CONTROL](https://github.com/JuanenRac/ARMOR-ANDROID-CONTROL)** - Cliente Android de operador com um radar 2D/3D em direto
* **[ARMOR-SERVER-AI](https://github.com/JuanenRac/ARMOR-SERVER-AI)** - Política de inferência visual que explica as suas decisões e nunca atua
* **[ARMOR-VISION](https://github.com/JuanenRac/ARMOR-VISION)** - Zonas de câmara, máscaras e regras de deteção para o Jetson Orin NX
* **[ARMOR-VOICE-AI](https://github.com/JuanenRac/ARMOR-VOICE-AI)** - Intenções de voz offline com uma confirmação que não se pode falsificar
* **[ARMOR-HARDWARE](https://github.com/JuanenRac/ARMOR-HARDWARE)** - Caixas, eletrónica e a matriz de aceitação de bancada
* **[ARMOR-DEVOPS](https://github.com/JuanenRac/ARMOR-DEVOPS)** - Implementação, a bancada de testes CM5, cópias de segurança e TLS
* **[ARMOR-SIMULATOR](https://github.com/JuanenRac/ARMOR-SIMULATOR)** - Simulador de telemetria offline com falhas repetíveis
* **[ARMOR-UPDATER](https://github.com/JuanenRac/ARMOR-UPDATER)** - Deteta, instala e atualiza os próprios repositórios do ecossistema
* **[ARMOR-DOCS](https://github.com/JuanenRac/ARMOR-DOCS)** - Arquitetura, linha de base de segurança e a matriz de capacidades

## 📚 Documentação e comunidade

Onde ler mais:

* [Matriz de capacidades: o que está provado e o que não](https://github.com/JuanenRac/ARMOR-DOCS/blob/main/docs/CAPABILITY_MATRIX.md)
* [Catálogo de projetos: versões e como os repositórios dependem uns dos outros](https://github.com/JuanenRac/ARMOR-DOCS/blob/main/docs/PROJECT_CATALOG.md)
* [Registo de alterações deste repositório](CHANGELOG.md)
* [Licença (GPL-3.0-or-later)](LICENSE)
* Perguntas, ideias e relatórios: electrohobby3d@gmail.com

## 👤 AUTOR

**JuanenRac (Electro Hobby 3D)** · electrohobby3d@gmail.com

## 📜 LICENÇA

GPL-3.0-or-later - veja [LICENSE](LICENSE).
