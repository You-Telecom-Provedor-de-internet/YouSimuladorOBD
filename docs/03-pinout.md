# 03 — Pinout ESP32 38 Pinos e Ligações

## Pinout do ESP32 DevKit (38 pinos)

```
                     ┌─────────────────┐
              3.3V ──┤ 3V3         GND ├── GND
               GND ──┤ GND         D23 ├── GPIO23  (MOSI / I2C_SDA)
              EN  ──┤ EN          D22 ├── GPIO22  (I2C_SCL → OLED)
           GPIO36 ──┤ VP          TX0 ├── GPIO1   (UART0 TX / USB Debug)
           GPIO39 ──┤ VN          RX0 ├── GPIO3   (UART0 RX / USB Debug)
           GPIO34 ──┤ D34          D21├── GPIO21  (I2C_SDA → OLED)
           GPIO35 ──┤ D35         D19 ├── GPIO19  (MISO)
           GPIO32 ──┤ D32         D18 ├── GPIO18  (SCK)
           GPIO33 ──┤ D33          D5 ├── GPIO5
           GPIO25 ──┤ D25         TX2 ├── GPIO17  (UART2 TX)
           GPIO26 ──┤ D26         RX2 ├── GPIO16  (UART2 RX)
           GPIO27 ──┤ D27         D4  ├── GPIO4
           GPIO14 ──┤ D14          D2 ├── GPIO2
           GPIO12 ──┤ D12         D15 ├── GPIO15
           GPIO13 ──┤ D13         GND ├── GND
               GND ──┤ GND        D13 ├── GPIO13  (duplicado na borda)
               VIN ──┤ VIN         D12├── GPIO12  (duplicado na borda)
                     └─────────────────┘
                       (centro: antena)
```

---

## Alocação de Pinos — Projeto YouSimuladorOBD

### Comunicação OBD-II

| GPIO | Pino Físico | Função | Destino |
|------|------------|--------|---------|
| GPIO4 | D4 | TWAI TX (CAN TX) | SN65HVD230 pino TXD |
| GPIO5 | D5 | TWAI RX (CAN RX) | SN65HVD230 pino RXD |
| GPIO17 | TX2 | K-Line TX | L9637D pino TXD |
| GPIO16 | RX2 | K-Line RX | L9637D pino RXD |

> **TWAI = Two-Wire Automotive Interface** — módulo CAN nativo do ESP32.
> Qualquer par de GPIOs pode ser usado para TWAI, pois o driver permite remapeamento.

### Display OLED (I2C)

| GPIO | Pino Físico | Função | Destino |
|------|------------|--------|---------|
| GPIO21 | D21 | I2C SDA | OLED pino SDA |
| GPIO22 | D22 | I2C SCL | OLED pino SCL |

### Interface de Usuário

| GPIO | Pino Físico | Função |
|------|------------|--------|
| GPIO32 | D32 | Botão PREV (parâmetro anterior) |
| GPIO33 | D33 | Botão NEXT (próximo parâmetro) |
| GPIO25 | D25 | Botão UP (incrementar valor) |
| GPIO26 | D26 | Botão DOWN (decrementar valor) |
| GPIO27 | D27 | Botão SELECT |
| GPIO14 | D14 | Botão PROTOCOL (trocar protocolo) |
| GPIO12 | D12 | Encoder — Fase A (CLK) |
| GPIO13 | D13 | Encoder — Fase B (DT) |
| GPIO15 | D15 | Encoder — Botão (SW) |

### Seleção de Protocolo (DIP Switch)

| GPIO | Pino Físico | Função |
|------|------------|--------|
| GPIO34 | D34 | DIP Switch Bit 0 (LSB) |
| GPIO35 | D35 | DIP Switch Bit 1 |
| GPIO36 | VP | DIP Switch Bit 2 (MSB) |

```
Tabela DIP Switch → Protocolo:
SW3 SW2 SW1 | Protocolo
 0   0   0  | ISO 15765-4 CAN 11-bit 500k
 0   0   1  | ISO 15765-4 CAN 11-bit 250k
 0   1   0  | ISO 15765-4 CAN 29-bit 500k
 0   1   1  | ISO 15765-4 CAN 29-bit 250k
 1   0   0  | ISO 9141-2
 1   0   1  | ISO 14230-4 KWP2000 5-baud
 1   1   0  | ISO 14230-4 KWP2000 Fast Init
 1   1   1  | (reservado)
```

> GPIOs 34, 35, 36, 39 são **somente entrada** no ESP32. Ideais para DIP switches.

### LEDs de Status

| GPIO | Pino Físico | Função | Cor |
|------|------------|--------|-----|
| GPIO2 | D2 | LED interno / Status geral | Azul (interno) |
| GPIO19 | D19 | LED CAN ativo | Verde |
| GPIO18 | D18 | LED K-Line ativo | Amarelo |
| GPIO23 | D23 | LED comunicação/TX | Vermelho |

### Debug Serial (USB)

| GPIO | Pino Físico | Função |
|------|------------|--------|
| GPIO1 | TX0 | UART0 TX → USB-Serial |
| GPIO3 | RX0 | UART0 RX → USB-Serial |

---

## Diagrama de Conexão Completo

```
ESP32 DevKit 38-pin
┌──────────────────────────────────────────────────────────────┐
│                                                              │
│  GPIO4  ──────────────────────► SN65HVD230 [TXD]           │
│  GPIO5  ◄────────────────────── SN65HVD230 [RXD]           │
│                                       │                      │
│                              [CANH] ──┼── OBD Pino 6        │
│                              [CANL] ──┼── OBD Pino 14       │
│                                       │                      │
│  GPIO17 ──────────────────────► L9637D [TXD]               │
│  GPIO16 ◄────────────────────── L9637D [RXD]               │
│                                       │                      │
│                              [K-Line] ── OBD Pino 7         │
│                              [12V]    ── OBD Pino 16        │
│                                                              │
│  GPIO21 ──────── SDA ──► OLED SSD1306                       │
│  GPIO22 ──────── SCL ──► OLED SSD1306                       │
│                                                              │
│  GPIO32 ──[10kΩ pull-up]──► BTN_PREV                       │
│  GPIO33 ──[10kΩ pull-up]──► BTN_NEXT                       │
│  GPIO25 ──[10kΩ pull-up]──► BTN_UP                         │
│  GPIO26 ──[10kΩ pull-up]──► BTN_DOWN                       │
│  GPIO27 ──[10kΩ pull-up]──► BTN_SELECT                     │
│  GPIO14 ──[10kΩ pull-up]──► BTN_PROTOCOL                   │
│                                                              │
│  GPIO12 ──► ENCODER_A (CLK)                                 │
│  GPIO13 ──► ENCODER_B (DT)                                  │
│  GPIO15 ──► ENCODER_SW                                      │
│                                                              │
│  GPIO34 ──► DIP_SW[0]   (pull-down para GND quando OFF)    │
│  GPIO35 ──► DIP_SW[1]                                       │
│  GPIO36 ──► DIP_SW[2]                                       │
│                                                              │
│  GPIO19 ──[330Ω]──► LED Verde (CAN)                        │
│  GPIO18 ──[330Ω]──► LED Amarelo (K-Line)                   │
│  GPIO23 ──[330Ω]──► LED Vermelho (TX activity)             │
│                                                              │
│  3.3V / GND ── Alimentação de todos os ICs                 │
│  VIN ── 5V via USB (desenvolvimento) ou regulador do 12V   │
└──────────────────────────────────────────────────────────────┘
```

---

## Restrições e Cuidados

| GPIO | Restrição |
|------|-----------|
| GPIO0 | Boot mode — não usar para I/O geral |
| GPIO1, GPIO3 | UART0 — ocupados pelo USB/Serial |
| GPIO6–11 | Flash SPI interno — **nunca usar** |
| GPIO34–39 | Somente entrada — sem pull-up/down interno |
| GPIO12 | Afeta boot se HIGH na energização — cuidado com pull-up externo |

> **Recomendação:** GPIOs 34–39 usados para DIP switches devem ter resistor **10kΩ de cada GPIO ao 3V3** (pull-up externo). Quando a chave estiver **aberta** → leitura HIGH (1). Quando **fechada para GND** → leitura LOW (0). O firmware interpreta LOW = bit ativo.
