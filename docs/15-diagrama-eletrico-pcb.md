# 15 - Diagrama Eletrico e Handoff PCB

## Objetivo

Este documento e a referencia eletrica consolidada para o engenheiro capturar o esquema da placa de circuito impresso.

Importante:

- a `RevA` foi congelada como `carrier para ESP32 DevKit 38 pinos`
- o objetivo deste handoff e transformar a bancada validada em uma primeira PCB de baixo risco

Arquivos complementares:

- [hardware/pcb-handoff/README.md](../hardware/pcb-handoff/README.md)
- [hardware/pcb-handoff/netlist-rev-a.csv](../hardware/pcb-handoff/netlist-rev-a.csv)
- [hardware/pcb-handoff/bom-rev-a.csv](../hardware/pcb-handoff/bom-rev-a.csv)
- [hardware/kicad/revA/README.md](../hardware/kicad/revA/README.md)

## Diagrama eletrico funcional

```mermaid
flowchart LR
    J1["J1 OBD-II J1962"]
    F1["F1 Fuse 1A"]
    U1["U1 LM2596 module\n12V -> 5V"]
    U4["U4 3.3V regulator\n+5V -> +3V3_AUX"]
    MCU["MCU1 ESP32 DevKit 38p\npluggable on carrier RevA"]
    U2["U2 SN65HVD230"]
    U3["U3 L9637D"]
    RK1["RK1 510R >=0.5W"]
    RLI1["RLI1 10k"]
    CK1["CK1 100nF"]
    CK2["CK2 100nF"]
    RCAN1["RCAN1 120R\noptional / jumper"]
    DISP1["DISP1 OLED SH1107"]
    ENC1["ENC1 KY-040"]
    BTN["SW2-SW7 buttons"]
    DIP["SW1 DIP 3 bits\nRDIP1-RDIP3 10k -> +3V3_AUX"]
    LEDTX["D3 LED_TX optional\nRLED3 330R"]

    J1 -- "pin16 +12V" --> F1
    F1 --> U1
    F1 --> U3
    F1 --> RK1
    F1 --> RLI1

    J1 -- "pin4/pin5 GND" --> GND["GND"]
    GND --> U1
    GND --> MCU
    GND --> U2
    GND --> U3
    GND --> U4
    GND --> DISP1
    GND --> ENC1
    GND --> BTN
    GND --> LEDTX
    GND --> CK1
    GND --> CK2

    U1 -- "5V" --> MCU
    U1 -- "5V" --> U4
    U4 -- "+3V3_AUX" --> U2
    U4 -- "+3V3_AUX" --> U3
    U4 -- "+3V3_AUX" --> DISP1
    U4 -- "+3V3_AUX" --> DIP
    U4 -- "+3V3_AUX" --> ENC1

    MCU -- "GPIO4 CAN_TX" --> U2
    MCU -- "GPIO5 CAN_RX" --> U2
    U2 -- "CANH" --> J1
    U2 -- "CANL" --> J1
    RCAN1 -. "across CANH/CANL" .- U2

    MCU -- "GPIO17 K_TX" --> U3
    MCU -- "GPIO16 K_RX" --> U3
    U3 -- "K" --> J1
    RK1 --> U3
    RLI1 --> U3
    CK1 --> U3
    CK2 --> U3

    MCU -- "GPIO21 SDA / GPIO22 SCL" --> DISP1
    MCU -- "GPIO14/13/19" --> ENC1
    MCU -- "GPIO32/33/25/26/27/18" --> BTN
    MCU -- "GPIO34/35/36" --> DIP
    MCU -- "GPIO23" --> LEDTX
```

## Mapa de sinais

### Conector OBD-II

| Pino | Sinal | Destino |
|---|---|---|
| `4` | chassis GND | `GND comum` |
| `5` | signal GND | `GND comum` |
| `6` | CANH | `U2 CANH` |
| `7` | K-Line | `U3 K` |
| `14` | CANL | `U2 CANL` |
| `15` | L-Line | `NC` |
| `16` | +12V bateria | `F1`, `U1 IN+`, `U3 VS` |

### CAN

| Origem | Destino |
|---|---|
| `GPIO4` | `U2 TXD` |
| `GPIO5` | `U2 RXD` |
| `U2 CANH` | `OBD pin 6` |
| `U2 CANL` | `OBD pin 14` |

### K-Line

| Origem | Destino |
|---|---|
| `GPIO17` | `U3 pin 4 (TX)` |
| `GPIO16` | `U3 pin 1 (RX)` |
| `U3 pin 6 (K)` | `OBD pin 7` |
| `U3 pin 7 (VS)` | `+12V protegido` |
| `RK1 510R` | `U3 pin 6 -> U3 pin 7` |
| `RLI1 10k` | `U3 pin 8 -> U3 pin 7` |
| `CK1 100nF` | `U3 pin 3 -> U3 pin 5` |
| `CK2 100nF` | `U3 pin 7 -> U3 pin 5` |

## Requisitos de PCB

### Obrigatorios para reproduzir a bancada

- `U2 SN65HVD230`
- `U3 L9637D`
- `RK1 510R >= 0.5W`
- `RLI1 10k`
- `CK1 100nF`
- `CK2 100nF`
- `F1` na entrada `+12V`
- `LM2596` ou buck equivalente
- regulador `3.3V` dedicado para `+3V3_AUX`

### Recomendados para a revisao da placa

- `RCAN1 120R` configuravel por jumper ou `DNP`
- TVS na entrada `+12V`
- protecao contra inversao de polaridade
- ponto de teste em:
  - `+12V`
  - `+5V`
  - `+3V3_AUX`
  - `GND`
  - `CANH`
  - `CANL`
  - `K`
  - `K_TX_LOGIC`
  - `K_RX_LOGIC`

## Observacoes para captura do esquema

### Implementacao do MCU congelada na RevA

Para esta revisao, o MCU fica congelado como:

- `ESP32 DevKit 38 pinos`
- montado por `2 x headers femea 1x19`
- com `USB` acessivel na borda
- com keepout dedicado para a antena

Referencia:

- [17 - RevA Carrier para ESP32 DevKit](17-revA-carrier-esp32-devkit.md)
- [hardware/kicad/revA/esp32-devkit38-carrier-reference.csv](../hardware/kicad/revA/esp32-devkit38-carrier-reference.csv)

### Escolha de implementacao da fonte

O hardware de bancada usa modulo `LM2596`. Para PCB:

- pode manter footprint/header para modulo buck
- ou redesenhar uma etapa buck equivalente, desde que preserve:
  - entrada `+12V`
  - saida `+5V` para `VIN`
  - corrente suficiente para `ESP32 + OLED + transceivers`

## Matriz de aceite que esta por tras deste esquema

| Barramento | Ferramenta | Resultado |
|---|---|---|
| CAN | `Torque Pro` | OK |
| CAN | `OBDLink MX+` | OK |
| ISO 9141-2 | `OBDLink app` | OK |
| ISO 9141-2 | `Torque Pro` | OK |
| ISO 9141-2 | `YouAutoCar` | OK |
| KWP Fast | `Torque Pro` | OK |

## Estado atual em EDA nativo

Ja existe um bootstrap nativo em `KiCad` em:

- `hardware/kicad/revA/YouSimuladorOBD_RevA.kicad_sch`

Esse bootstrap ainda nao e a captura completa final, mas ja organiza o projeto em folhas por bloco:

- `Power_Input`
- `ESP32_Core`
- `OBD_Transceivers`
- `UI_Local`

Assim, o engenheiro nao precisa mais partir do zero no EDA.
