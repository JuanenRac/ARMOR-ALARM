<p align="center">
  <img src="images/ARMOR_BANNER.svg" alt="ARMOR-NODE-ALARM banner" width="100%">
</p>

# 🚨 ARMOR-NODE-ALARM

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
  <img src="https://img.shields.io/badge/Checks-482%2C535-2ea44f.svg" alt="Checks">
  <img src="https://img.shields.io/badge/Version-0.4.4-0f6cbd.svg" alt="Version">
  <img src="https://img.shields.io/badge/Maturity-functional-00E5FF.svg" alt="Maturity">
</p>

---

**Verificação de honestidade - o que funciona hoje:** O que existe é o **núcleo do alarme** (o que é uma zona, como um laço com fios lido pelo ADC se torna um estado, as regras da central: armar, atrasos, o alarme, a sirene, o PIN com bloqueio) e o **firmware do nó** construído sobre ele (definições, painel web, mensagens com o servidor, a ponte do rádio Zigbee, um coordenador Zigbee na rede, relés, Bluetooth, atualizações assinadas), que compila para as duas placas; o núcleo, as definições, as mensagens e o painel num navegador real contra um nó de substituição são testados num computador com 477 588 verificações em 6 programas de teste. **Nada correu numa placa nem numa instalação real, nenhuma zona foi ligada com fios nem sirene ligada, a conversa Zigbee do próprio nó com um coordenador (local ou na rede) está escrita e testada contra um coordenador de substituição mas nunca correu contra um coordenador ou sensor reais, e isto não é um sistema de alarme certificado.**

---

## 📋 Num relance

| | |
|---|---|
| Versão | `0.4.4` |
| Maturidade | Funcional |
| Testes | 486 690 verificações em 14 programas (`ctest`) |
| Tecnologia | C++17 host-tested core; ESP-IDF 5.5.5 firmware for the ESP32-S3 (PoE Ethernet and Wi-Fi boards) |
| Em hardware real | Nenhum ainda: nenhuma placa, zona, sirene ou coordenador Zigbee ligados (só testes no computador, a compilação e o painel num navegador) |

## 🎯 Visão geral

* **Zonas:** quatro tipos - *instantânea* (uma porta, uma janela), *de entrada* (a porta por onde entra: inicia o atraso de entrada), *interior* (movimento: só em «ausente») e *sempre* (fumo, gás, pânico: armada ou não). Uma zona pode ser um laço com fio ou um sensor Zigbee.
* **O laço com fio:** um contacto com uma resistência de fim de linha, lido como tensão, dá quatro estados num só pino - normal, disparado, e violação por um fio em curto-circuito ou cortado. Os limites são definições; um filtro de repique impede que um contacto que salta dispare o alarme.
* **A central:** armação *ausente* e *em casa*, um atraso de saída que não trata como intruso quem sai, um atraso de entrada, um alarme que **fica retido** até um desarme correto, uma sirene que pára sozinha passado um tempo, desvio de zonas abertas quando forçado (nunca um detetor de fumo a soar), e um registo dos últimos cem eventos.
* **O PIN:** armar não precisa de nenhum, desarmar e silenciar sim; os PIN errados contam-se e, passados alguns, tudo se recusa durante um tempo, mesmo o PIN certo.
* **Silenciar sem desarmar:** enquanto a sirene soa, um PIN certo pára-a logo com o sistema ainda armado - o alarme fica retido e registado, uma zona que ainda não soou pode voltar a fazê-la soar, e um PIN errado conta para o bloqueio como um errado de desarmar.
* **O plano:** uma ponte na placa do nó elétrico (um rádio Zigbee, zonas com fio, um relé de sirene) que funciona com o ARMOR-SERVER e **continua a vigiar quando o servidor está ausente**: uma central toma o rádio ao servidor através da ponte Zigbee do ARMOR-NODE-ELECTRICAL. Veja [o desenho](docs/DESIGN.md) e [a continuidade](docs/FAILOVER.md).
* **O nó:** firmware para o ESP32-S3 (Ethernet PoE ou Wi-Fi) com até oito zonas com fio, a sirene num relé, o painel web de cada nó em oito idiomas, o rádio Zigbee emprestado ao Zigbee2MQTT, e as mensagens `armor/alarm/<node>/state|command|result`; o servidor só pode armar e desarmar quando três interruptores independentes o permitem. Veja [o firmware](docs/NODE_FIRMWARE.md), [as mensagens](docs/ALARM_MESSAGES.md) e [o hardware](docs/HARDWARE.md).
* **Tomar o rádio:** depois de o Zigbee2MQTT do servidor ter estado ausente durante um tempo definido, o nó levanta ele próprio o coordenador (ZNP), lê os sensores das zonas que o nomeiam (IAS Zone) e comanda sirenes Zigbee (IAS WD), e larga-o assim que o cliente volta. **Ainda não:** um coordenador ou sensor real, o hardware e qualquer teste numa instalação real.
* **Um reinício não desarma a casa:** o modo, um alarme em curso e as zonas deixadas de fora guardam-se na flash e retomam-se no arranque; um laço cortado ou em curto-circuito nunca se deixa de fora; as mensagens passam por uma fila (sem bloqueio mútuo com o broker) e o cão de guarda reinicia a placa se a tarefa que lê as zonas parar.
* **Atualizações pelo ar assinadas:** o nó só instala do GitHub uma imagem cujo SHA-256 coincide e que leva uma assinatura Ed25519 válida do responsável (a chave pública está no firmware, a privada nunca sai do responsável); um ficheiro carregado à mão por um administrador no painel continua a ser aceite. As páginas e mensagens do painel estão em oito idiomas.
* **Emparelhamento Bluetooth com PIN:** o PIN de seis dígitos deriva do código de configuração, é impresso na consola USB e mostrado a um administrador no painel; o telemóvel tem de o introduzir antes de poder escrever as definições do nó.
* **Sirene e reinícios:** o relé da sirene pertence só ao alarme (nem o broker, nem a central, nem o painel o podem mover, e não abre ao perder o broker); uma zona que ainda não tinha tocado volta a fazer soar a sirene depois do seu tempo; após um reinício voltam um alarme de fumo ou pânico com o sistema desarmado, um alarme silenciado e um bloqueio; um reset à mão é recusado com o alarme armado; um sensor Zigbee que nunca fala conta como perdido.
* **Só a casa pode usar o painel:** o painel responde por omissão à rede privada (ou às redes que o dono indicar, como uma VPN), recusa uma lista que deixaria de fora quem a guarda, conta as palavras-passe erradas por pessoa e por endereço e funciona atrás do proxy do servidor para ser alcançado de fora de casa sem publicar o nó.**Uma zona que não pode ser lida não aparece como fechada:** se o conversor não arranca ou um laço não dá leitura cinco rondas seguidas, a zona passa a *avaria* (trouble) - não vigia, nunca faz soar a sirene, impede armar (salvo forçando, e então fica excluída e registada) e o servidor avisa.
* **Ligação cifrada opcional ao broker (TLS):** desativada por predefinição, por isso o nó fala MQTT sem cifra como antes. Um interruptor e uma caixa para o certificado da autoridade própria da instalação (PEM, no máximo 2 KB) na página Broker fazem-no ligar-se com `mqtts://`, confiar só nessa autoridade e verificar que o certificado do broker foi feito para o endereço alcançado; o que não for um único certificado PEM é recusado. Coberto por testes no computador; o handshake ainda não foi executado numa placa.
* **Perfis de armar:** um comando de armar pode levar um perfil (Ausente, Em casa, Noite, o seu) que escolhe, para esse armar, que zonas vigiam, quais disparam o alarme logo ou após o atraso de entrada, os atrasos de saída e de entrada e se a sirene toca (um alarme silencioso fica trancado, registado e avisado). Nunca toca nas zonas de fumo, pânico ou sabotagem, no PIN, no bloqueio, no silêncio, num armar forçado nem numa zona com problema; o perfil sobrevive a um reinício e um armar sem perfil comporta-se exatamente como antes. Testado contra as regras antigas com uma execução aleatória e contra o contrato partilhado.
* **Relatório de saúde:** cerca de uma vez por minuto o nó publica há quanto tempo está ligado e porque reiniciou da última vez, a memória livre, a ligação e o sinal Wi-Fi, quantas vezes se ligou ao broker e o último erro que registou (`armor/health/<nó>`, o contrato `node_health` do ARMOR-COMMON). O código que o constrói é partilhado por todos os nós e testado num computador; nunca correu numa placa.
* **Controlo a partir do servidor:** o Studio pode reiniciar o nó de forma limpa, pô-lo em pausa até uma hora (retoma sozinho), pôr o seu trabalho em espera (sem fim, mantida nos reinícios, o nó continua ligado e a responder), fazê-lo mostrar-se, pedir um instantâneo de diagnóstico e marcá-lo em manutenção (`armor/control/<nó>/...`, o contrato `node_control` do ARMOR-COMMON). Cada ordem tem dois passos com um token de uso único que pertence a essa operação, só o servidor as pode escrever no broker, e não existe reposição de fábrica, apagamento nem sono profundo. Enquanto o nó está em pausa ou em espera, as suas zonas não são lidas, nada é avaliado, armado ou tocado, nenhum estado é publicado e o relé da sirene fica aberto: a casa NÃO está protegida, e o relatório de saúde di-lo. O código é partilhado por todos os nós e testado num computador; ainda não correu numa placa.
* **Diagnóstico sem cabo:** depois de um arranque que não foi limpo (um bloqueio, um watchdog, uma quebra de alimentação, uma reserva de memória falhada), o relatório de saúde diz o motivo durante dez minutos; uma falha deixa o seu core dump para ver, descarregar ou apagar a partir do painel; as definições guardadas são copiadas antes de cada instalação de firmware e recuperadas se uma atualização as danificar; e um administrador pode pedir ao nó, através do servidor, um pacote de diagnóstico ou o seu registo com um token de utilização única.
* **As opções de segurança do servidor:** a central aplica as opções que o servidor guarda para ela (`armor/alarm/<nó>/policy`) e cada uma pode ser fixada no próprio nó: o estado armado guardado fica atado às zonas e ao PIN, as mensagens de zona Zigbee do servidor podem ser desligadas e, se assinadas, são verificadas (assinatura com uma chave escrita no painel e um número de sequência que só aumenta). Testado num computador com os valores que o contrato e o servidor calculam; não executado numa placa.
* **Selagem opcional dos segredos guardados:** desativada por predefinição (`security.seal_secrets`, ou `nodes.nvs_encryption` do servidor). Ativada, as palavras-passe, os tokens, as chaves, os PIN e os hashes de palavras-passe que o nó guarda ficam selados com ChaCha20-Poly1305 e tudo o resto continua legível; ativá-la ou desativá-la reescreve-os sem perder nada, mesmo que o nó pare a meio. Não se queima nenhum eFuse nem se cifra a flash: a chave está numa partição do mesmo chip, pelo que **não** protege contra quem tenha a placa, e um nó instalado antes dessa partição mantém os segredos como estavam até ser gravado com a nova tabela de partições.

## 🖼️ Capturas de ecrã

Capturadas a 1920×1080 numa bancada de testes, com dados de exemplo e sem qualquer instalação real por trás.

<table>
<tr><td width="50%" valign="top"><img src="images/screenshots/overview.png" alt="Resumo: o estado do nó num relance - rede, broker, firmware e o que reporta." width="100%"><br><sub>Resumo: o estado do nó num relance - rede, broker, firmware e o que reporta.</sub></td><td width="50%" valign="top"><img src="images/screenshots/zones.png" alt="Zonas: cada zona com fios ou Zigbee do painel, o seu tipo, o tempo de retenção e o estado em direto." width="100%"><br><sub>Zonas: cada zona com fios ou Zigbee do painel, o seu tipo, o tempo de retenção e o estado em direto.</sub></td></tr>
<tr><td width="50%" valign="top"><img src="images/screenshots/zigbee.png" alt="Rádio Zigbee: a ponte série para o módulo coordenador, que cliente a pode usar e o estado da ponte." width="100%"><br><sub>Rádio Zigbee: a ponte série para o módulo coordenador, que cliente a pode usar e o estado da ponte.</sub></td><td width="50%" valign="top"><img src="images/screenshots/inputs-outputs.png" alt="Relés e sirene: as saídas da placa base e o que aciona cada uma." width="100%"><br><sub>Relés e sirene: as saídas da placa base e o que aciona cada uma.</sub></td></tr>
</table>

## 🧩 Papel no A.R.M.O.R.

O ARMOR-NODE-ALARM é a central de alarme: as zonas, o armar, os atrasos, a sirene e o PIN decidem-se no próprio nó, por isso a casa continua vigiada quando o servidor não está. O [ARMOR-SERVER](https://github.com/JuanenRac/ARMOR-SERVER) mostra-a, liga sensores Zigbee às suas zonas e, quando o permitir, arma-a e desarma-a; o menu *Centrais de alarme* e o *Desenhador de alarme* do [ARMOR-STUDIO](https://github.com/JuanenRac/ARMOR-STUDIO) e a aplicação [ARMOR-ANDROID-CONTROL](https://github.com/JuanenRac/ARMOR-ANDROID-CONTROL) comandam-na. O seu rádio Zigbee pode estar na placa ou ser um coordenador na rede.

## 📂 Estrutura do repositório

```text
ARMOR-NODE-ALARM/
├── core/    alarm_zone, alarm_panel (the rules), alarm_config, alarm_message, alarm_runner (the node), alarm_supervisor (what it keeps in flash and when it speaks), arm_profile (the arming profiles), plus the settings every node shares
├── main/    the ESP-IDF firmware: alarm_manager, the web server, the Zigbee bridge, the relays
├── panel/   the web panel (eight languages)
├── tests/   test_alarm (the rules), test_profiles (the arming profiles), test_node (settings, messages, runner), test_mqtt_tls (the optional encrypted link to the broker), test_release (update from GitHub), emit_samples + check_samples (the messages against the contract), test_zigbee (ZNP, IAS Zone and WD, the node's own conversation with a coordinator)
├── tools/   the stand-in node and the browser test of the panel
├── docs/    DESIGN, FAILOVER, SAFETY, NODE_FIRMWARE, ALARM_MESSAGES, HARDWARE, BASE_BOARD, ZIGBEE, BLE_PROVISIONING
└── images/  brand assets
```

## 🛠️ Ambiente de desenvolvimento

**Requisitos:** um compilador de C++17 para os testes no computador, o contentor ESP-IDF 5.5.5 para compilar a imagem, uma placa ESP32-S3 (Ethernet com PoE ou Wi-Fi)

```bash
cmake -S tests -B build/host && cmake --build build/host && ctest --test-dir build/host   # 486,690 checks: the rules, the settings, the messages, the runner and the update from GitHub
```

Veja o [desenho](docs/DESIGN.md), [como a central toma o lugar do servidor](docs/FAILOVER.md) e as [notas de segurança](docs/SAFETY.md).

## 🔗 Projetos relacionados

O **A.R.M.O.R.** (Autonomous Radar & Multimodal Observation Range) é um sistema de segurança perimetral feito de repositórios independentes. Cada um tem a sua própria versão, os seus próprios testes e o seu próprio README; esta é a família:

* **[ARMOR-COMMON](https://github.com/JuanenRac/ARMOR-COMMON)** - Contratos de mensagens, validadores, vetores de conformidade e tipos gerados
* **[ARMOR-NODE-RADAR](https://github.com/JuanenRac/ARMOR-NODE-RADAR)** - Firmware de nó de campo para ESP32-S3 com três radares e o seu próprio painel web
* **[ARMOR-NODE-SOLAR](https://github.com/JuanenRac/ARMOR-NODE-SOLAR)** - Protocolos de inversores e baterias solares e as mensagens de um nó de ligação
* **[ARMOR-NODE-ELECTRICAL](https://github.com/JuanenRac/ARMOR-NODE-ELECTRICAL)** - Nó elétrico: contadores, a mensagem das leituras da rede e as regras de comutação
* **ARMOR-NODE-ALARM** (este repositório) - Nó e central de alarme: zonas, armar, atrasos, sirene e PIN, com o servidor ou sem ele
* **[ARMOR-NODE-HMI](https://github.com/JuanenRac/ARMOR-NODE-HMI)** - Painel tátil: o estado do sistema num ecrã de parede, armar e reconhecer, e a casa do assistente de voz
* **[ARMOR-NETWORK](https://github.com/JuanenRac/ARMOR-NETWORK)** - A rede local: os seus dispositivos, a internet e o que muda
* **[ARMOR-SERVER](https://github.com/JuanenRac/ARMOR-SERVER)** - Coordenador central: telemetria, alarmes, dispositivos, leituras solares e câmaras
* **[ARMOR-STUDIO](https://github.com/JuanenRac/ARMOR-STUDIO)** - Consola web: câmaras, radar, alarmes, energia solar e o desenhador de local 2D/3D
* **[ARMOR-ANDROID-CONTROL](https://github.com/JuanenRac/ARMOR-ANDROID-CONTROL)** - Cliente Android de operador com um radar 2D/3D em direto
* **[ARMOR-IOS-CONTROL](https://github.com/JuanenRac/ARMOR-IOS-CONTROL)** - Cliente de operador para iPhone
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
