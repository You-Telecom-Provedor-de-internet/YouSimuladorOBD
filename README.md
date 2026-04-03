# YouSimuladorOBD - Emulador OBD-II com ESP32

Emulador/simulador OBD-II baseado em ESP32 (38 pinos), capaz de responder a scanners OBD-II reais simulando parametros de veiculo em tempo real nos 7 protocolos do padrao OBD-II.

## Protocolos Suportados

| # | Protocolo | Velocidade | Interface Fisica |
|---|-----------|------------|------------------|
| 1 | ISO 15765-4 CAN 11-bit | 500 kbps | CAN Bus |
| 2 | ISO 15765-4 CAN 11-bit | 250 kbps | CAN Bus |
| 3 | ISO 15765-4 CAN 29-bit | 500 kbps | CAN Bus |
| 4 | ISO 15765-4 CAN 29-bit | 250 kbps | CAN Bus |
| 5 | ISO 9141-2 | 10400 baud | K-Line |
| 6 | ISO 14230-4 KWP2000 | 5 baud | K-Line |
| 7 | ISO 14230-4 KWP2000 | Init rapido | K-Line |

## Parametros Simulados

| PID | Parametro | Modo OBD |
|-----|-----------|----------|
| 0x0C | Rotacao do motor (RPM) | Mode 01 |
| 0x0D | Velocidade do veiculo (km/h) | Mode 01 |
| 0x05 | Temperatura do liquido refrigerante | Mode 01 |
| 0x0F | Temperatura do ar de admissao | Mode 01 |
| 0x10 | MAF | Mode 01 |
| 0x0B | MAP | Mode 01 |
| 0x11 | Posicao do acelerador | Mode 01 |
| 0x0E | Avanco de ignicao | Mode 01 |
| 0x04 | Carga do motor | Mode 01 |
| 0x2F | Nivel de combustivel | Mode 01 |
| - | DTCs | Mode 03 |
| - | VIN | Mode 09 |

## Conectividade Web Atual

- STA com DHCP e mDNS em `http://youobd.local`
- AP fallback `OBD-Simulator` em `http://192.168.4.1`
- interface web protegida por autenticacao
- WebSocket para atualizacao em tempo real
- API REST para controle e configuracao
- OTA web para firmware e filesystem em `/ota.html`
- configuracao persistente em NVS para hostname, manifest OTA e credenciais web/OTA
- checagem automatica do manifest OTA sem atualizar sozinha

Credenciais web/OTA de fabrica:

```text
Usuario: admin
Senha: obd12345
```

## OTA

O projeto suporta OTA web no proprio ESP32, tanto por upload manual quanto por download remoto via manifest.

No modo comercial/campo, a pagina `/ota.html` tambem permite salvar:

- hostname mDNS do equipamento
- URL padrao do `manifest.json`
- usuario e senha da interface web/OTA
- intervalo da checagem automatica de update

Essas configuracoes ficam persistidas na NVS e sobrevivem a reboot.

Importante:

- este repositorio documenta apenas o OTA do `YouSimuladorOBD`
- o `YouAutoTester` tera trilha OTA separada e nao deve compartilhar `manifest.json` ou binarios com este projeto

Pagina de update:

```text
http://youobd.local/ota.html
http://<ip-do-esp>/ota.html
```

Arquivos usados no modo manual:

- firmware: `.pio/build/esp32dev/firmware.bin`
- filesystem: `.pio/build/esp32dev/littlefs.bin`

Fluxo online:

- manifest padrao do firmware: `https://app2.youtelecom.com.br/updates/yousimuladorobd/manifest.json`
- informe uma URL de `manifest.json` na pagina `/ota.html`
- use `Verificar online` para consultar a release
- use `Baixar firmware` ou `Baixar filesystem` para o ESP32 buscar o `.bin` sozinho
- se a checagem automatica estiver ligada, o ESP32 consulta esse manifest periodicamente, mas nao atualiza sem comando

Exemplo de manifest:

- [Manifest Exemplo](docs/ota-manifest.example.json)

## Bluetooth

O emulador ELM327 via Bluetooth SPP existe no codigo, mas esta desabilitado por padrao no firmware atual para priorizar estabilidade do Wi-Fi/Web.

## Documentacao

- [01 - Visao Geral e Arquitetura](docs/01-overview.md)
- [02 - Hardware e Componentes](docs/02-hardware.md)
- [03 - Pinout ESP32 e Ligacoes](docs/03-pinout.md)
- [04 - Protocolos OBD-II](docs/04-protocols.md)
- [05 - PIDs OBD-II e Formato de Dados](docs/05-obd-pids.md)
- [06 - Arquitetura de Software / Firmware](docs/06-architecture.md)
- [07 - Interface de Controle](docs/07-ui-controls.md)
- [08 - Wi-Fi, Interface Web e OTA](docs/08-wifi-webui.md)
- [09 - Perfis de Veiculo](docs/09-vehicle-profiles.md)
- [10 - Simulacao Dinamica](docs/10-dynamic-simulation.md)
- [11 - Bluetooth SPP / ELM327](docs/11-bluetooth-elm327.md)
- [12 - Auditoria do Firmware e Bancada](docs/12-auditoria-firmware-e-bancada.md)
- [13 - Montagem K-Line com L9637D](docs/13-kline-l9637d-montagem.md)
- [14 - Cenarios Diagnosticos Compostos](docs/14-diagnostic-scenarios.md)

## Estrutura do Projeto

```text
YouSimuladorOBD/
|- README.md
|- docs/
|- firmware/
|  |- src/
|  |- data/
|  |- include/
|  `- platformio.ini
`- hardware/
```

## Build e Gravacao por Cabo

```bash
cd firmware
pio run
pio run --target upload
pio run --target uploadfs
```

## Estado Atual Validado

Validado em hardware:

- acesso web por IP e por `youobd.local`
- autenticacao funcionando
- OTA de firmware funcionando
- OTA de filesystem funcionando
- OTA online por manifest funcionando
- persistencia de hostname/manifest/credenciais via NVS
- checagem automatica de versao OTA funcionando
- camada diagnostica com cenarios compostos, progressivos e correlacionados
- endpoints `GET /api/scenarios`, `POST /api/scenario` e `GET /api/diagnostics`
- payload rico para futura integracao com health engine e mecanico online
- camada diagnostica validada por API/WebSocket na placa real
- ativacao de cenario por WebSocket validada
- requalificacao de DTC apos limpeza validada
- troca de protocolo em runtime reinicializando CAN/K-Line corretamente
- framing fisico corrigido para Mode 03 / Mode 04
- transmissao CAN multi-frame para respostas longas como VIN
- barramento CAN validado em bancada com `SN65HVD230 + ELM327 + Torque Pro`
- leitura real de PIDs tambem validada com `OBDLink MX+ + Torque`
- leitura de PIDs em tempo real validada nos 4 protocolos CAN
- leitura de DTC (`Mode 03`) validada via Torque Pro
- limpeza de DTC (`Mode 04`) validada via Torque Pro
- leitura de VIN (`Mode 09`) validada via Torque Pro
- troca em runtime validada em `CAN 11b 500k`, `CAN 11b 250k`,
  `CAN 29b 500k` e `CAN 29b 250k`
- Bluetooth desativado por padrao para manter estabilidade da rede

Observacoes importantes desta tranche:

- houve um travamento real por excesso no slot OTA durante a rodada diagnostica
- o binario foi reduzido sem mexer na logica dos cenarios
- a placa voltou a subir normalmente e foi revalidada em bancada
- build final gravada:
  `flash = 1958921 / 1966080`
  `firmware.bin = 1965504 bytes`

## Auditoria Recente

- [12 - Auditoria do Firmware e Bancada](docs/12-auditoria-firmware-e-bancada.md)
