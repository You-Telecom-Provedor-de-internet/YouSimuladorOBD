# YouSimuladorOBD — Emulador OBD-II com ESP32

Emulador/simulador OBD-II baseado em ESP32 (38 pinos) capaz de responder a scanners OBD-II reais simulando parâmetros de veículo em tempo real através dos 7 protocolos do padrão OBD-II.

## Protocolos Suportados

| # | Protocolo | Velocidade | Interface Física |
|---|-----------|-----------|-----------------|
| 1 | ISO 15765-4 CAN 11-bit | 500 kbps | CAN Bus |
| 2 | ISO 15765-4 CAN 11-bit | 250 kbps | CAN Bus |
| 3 | ISO 15765-4 CAN 29-bit | 500 kbps | CAN Bus |
| 4 | ISO 15765-4 CAN 29-bit | 250 kbps | CAN Bus |
| 5 | ISO 9141-2 | 10400 baud | K-Line |
| 6 | ISO 14230-4 KWP2000 | 5 baud (init lento) | K-Line |
| 7 | ISO 14230-4 KWP2000 | Init rápido | K-Line |

## Parâmetros Simulados (12 PIDs)

| PID | Parâmetro | Modo OBD |
|-----|-----------|----------|
| 0x0C | Rotação do motor (RPM) | Mode 01 |
| 0x0D | Velocidade do veículo (km/h) | Mode 01 |
| 0x05 | Temperatura do líquido refrigerante (°C) | Mode 01 |
| 0x0F | Temperatura do ar de admissão (°C) | Mode 01 |
| 0x10 | Fluxo de ar de admissão — MAF (g/s) | Mode 01 |
| 0x0B | Pressão do coletor de admissão — MAP (kPa) | Mode 01 |
| 0x11 | Posição absoluta do acelerador (%) | Mode 01 |
| 0x0E | Ângulo de avanço de ignição (°) | Mode 01 |
| 0x04 | Carga do motor (%) | Mode 01 |
| 0x2F | Nível de combustível restante (%) | Mode 01 |
| —   | Código de falha (DTC) | Mode 03 |
| —   | Número de quadro (VIN) | Mode 09 |

## Conectividade Wi-Fi e Web

- **Modo AP:** ESP32 cria rede `OBD-Simulator` → acessa `http://192.168.4.1`
- **Modo STA:** conecta ao roteador → acessa `http://obdsim.local`
- **Interface web:** sliders em tempo real para todos os 12 parâmetros
- **WebSocket:** atualização bidirecional a cada 500ms
- **API REST:** controle completo via JSON (`/api/params`, `/api/protocol`, `/api/dtcs`)

## Documentação

- [01 - Visão Geral e Arquitetura](docs/01-overview.md)
- [02 - Hardware e Componentes](docs/02-hardware.md)
- [03 - Pinout ESP32 e Ligações](docs/03-pinout.md)
- [04 - Protocolos OBD-II](docs/04-protocols.md)
- [05 - PIDs OBD-II e Formato de Dados](docs/05-obd-pids.md)
- [06 - Arquitetura de Software / Firmware](docs/06-architecture.md)
- [07 - Interface de Controle (Botões/UI)](docs/07-ui-controls.md)
- [08 - Wi-Fi e Interface Web](docs/08-wifi-webui.md)

## Estrutura do Projeto

```
YouSimuladorOBD/
├── README.md
├── docs/                  # Documentação técnica
├── firmware/              # Código ESP32 (Arduino/ESP-IDF)
│   ├── src/
│   │   ├── main.cpp
│   │   ├── protocols/     # Implementação dos protocolos
│   │   ├── obd/           # Handlers de PIDs/Modos
│   │   ├── simulation/    # Estado dos parâmetros
│   │   ├── ui/            # Botões, encoder, OLED
│   │   └── web/           # Wi-Fi, AsyncWebServer, WebSocket
│   ├── data/              # LittleFS: index.html, config.json
│   ├── include/
│   └── platformio.ini
└── hardware/              # Esquemáticos e PCB
    ├── schematics/
    └── bom/               # Bill of Materials
```
