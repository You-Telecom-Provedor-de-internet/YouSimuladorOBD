# 18 - Codex Plugin YOU OBD Lab

## Objetivo

O `YOU OBD Lab` e um plugin local do Codex criado para ajudar no desenvolvimento e validacao integrada do ecossistema:

- `YouSimuladorOBD`
- `YouAutoCarvAPP2`
- celular Android real
- adaptadores `ELM327` e `OBDLink`

Ele nao substitui o OBD real. O papel dele e organizar e ampliar a capacidade do Codex de validar o laboratorio.

## O que o plugin faz

O plugin encapsula a skill `you-obd-android-lab`, que orienta o Codex a:

- usar a API REST/WebSocket do simulador como plano de controle
- usar OBD real como prova de compatibilidade
- usar `ADB`, `logcat` e screenshots no celular
- comparar o estado interno do simulador com a resposta OBD e com a UI do app

## Onde ele vive

Nesta maquina, a instalacao ativa esta em:

- `C:\Users\haise\.codex\.tmp\plugins\plugins\you-obd-lab`

Marketplace usado pela interface do Codex:

- `C:\Users\haise\.codex\.tmp\plugins\.agents\plugins\marketplace.json`

Arquivos principais:

- `C:\Users\haise\.codex\.tmp\plugins\plugins\you-obd-lab\.codex-plugin\plugin.json`
- `C:\Users\haise\.codex\.tmp\plugins\plugins\you-obd-lab\skills\you-obd-android-lab\SKILL.md`
- `C:\Users\haise\.codex\.tmp\plugins\plugins\you-obd-lab\README.md`
- `C:\Users\haise\.codex\.tmp\plugins\plugins\you-obd-lab\CHANGELOG.md`
- `C:\Users\haise\.codex\.tmp\plugins\plugins\you-obd-lab\scripts\collect-you-obd-lab-snapshot.ps1`
- `C:\Users\haise\.codex\.tmp\plugins\plugins\you-obd-lab\scripts\watch-you-obd-status.ps1`

## Quando usar

Use o plugin quando a tarefa envolver mais de uma camada do ecossistema, por exemplo:

- firmware do simulador + app Android
- protocolo OBD + API do simulador
- validacao de tela do app + logcat + cenario do simulador
- comparacao entre `Torque`, `OBDLink`, `YouAutoCar` e o estado interno do ESP32

## Modelo de validacao

O plugin trabalha com tres verdades em paralelo:

1. `API do simulador`
2. `OBD real`
3. `UI/logs do Android`

Interpretacao:

- `API` diz o que o simulador acredita que esta acontecendo
- `OBD` diz o que um scanner real realmente enxerga
- `ADB/logcat` diz o que o app exibiu ou fez

Um teste so e considerado fechado quando essas tres camadas estao coerentes, ou quando o desvio entre elas esta claramente explicado.

## Fluxo recomendado de uso

1. Ler `GET /api/status`
2. Preparar `profile`, `protocol`, `mode`, `scenario` e `DTCs`
3. Conectar o app Android via adaptador real
4. Capturar logs e screenshots
5. Comparar com `GET /api/diagnostics`
6. Relatar:
   - setup
   - oracle da API
   - resposta OBD
   - resultado no app
   - conclusao

## Scripts auxiliares do plugin

O plugin ja possui scripts locais para acelerar bancada:

- `collect-you-obd-lab-snapshot.ps1`
  - coleta `adb devices`
  - coleta `/api/status`, `/api/diagnostics` e `/api/profiles`
  - captura screenshot do celular
  - salva `logcat`
  - gera um resumo rapido

- `watch-you-obd-status.ps1`
  - monitora `GET /api/status` em loop
  - ajuda a acompanhar perfil, protocolo, RPM, velocidade e DTCs durante o teste

## Exemplos de prompts

- `Use $you-obd-android-lab para validar o simulador com o app Android e o celular real`
- `Compare API, OBD real e UI do app em um teste de regressao`
- `Prepare um cenario de DTC e valide o fluxo no YouAutoCarvAPP2`
- `Verifique se a UI do app bate com o freeze frame entregue pela API e pelo OBD`

## O que ele nao faz

O plugin nao torna a API do simulador um substituto do OBD real.

Regra:

- para compatibilidade com scanner/app automotivo real, a validacao final continua sendo feita no barramento OBD

## Observacoes operacionais

- o plugin e local ao ambiente Codex desta maquina
- ele nao faz parte do firmware do ESP32
- ele nao precisa ser embarcado na PCB
- ele serve para acelerar desenvolvimento, depuracao e regressao no laboratorio

## Relacao com a documentacao do projeto

Use junto com:

- [06 - Arquitetura de Firmware](C:/www/YouSimuladorOBD/docs/06-architecture.md)
- [08 - Wi-Fi, Web e OTA](C:/www/YouSimuladorOBD/docs/08-wifi-webui.md)
- [14 - Cenarios Diagnosticos](C:/www/YouSimuladorOBD/docs/14-diagnostic-scenarios.md)

Esses documentos explicam o simulador. Este documento explica como o Codex usa esse simulador dentro do laboratorio.
