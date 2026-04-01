# 02 — Hardware e Componentes

## Bill of Materials (BOM)

### Componentes Principais

| Qtd | Componente | Especificação | Função |
|-----|-----------|---------------|--------|
| 1 | ESP32 38-pin | ESP32-WROOM-32D / DevKitC | Microcontrolador principal |
| 1 | SN65HVD230 | CAN Transceiver 3.3V | Interface física CAN bus |
| 1 | L9637D | K-Line Driver | Interface física K-Line ISO9141/KWP2000 |
| 1 | OBD-II Conector | Fêmea 16 pinos | Ponto de conexão com scanner |
| 1 | Display OLED | SSD1306 128x64 I2C | Exibição de status e valores |
| 1 | Encoder Rotativo | KY-040 ou similar | Ajuste de valores |
| 3–4 | Botões tácteis | 6x6mm | Navegação / seleção |
| 3 | LED 3mm | Verde, Amarelo, Vermelho | Status visual |
| 3 | DIP Switch | 3 posições | Seleção de protocolo |
| 1 | Regulador 3.3V | AMS1117-3.3 ou LDO similar | Alimentação do ESP32 |
| 1 | Conector USB | Micro-USB ou USB-C | Alimentação e debug serial |

### Componentes Passivos

| Qtd | Componente | Valor | Localização |
|-----|-----------|-------|-------------|
| 2 | Resistor | 120 Ω | Terminação CAN (CANH-CANL) |
| 2 | Resistor | 10 kΩ | Pull-up botões |
| 1 | Resistor | 1 kΩ | Pull-up K-Line (para 12V) |
| 1 | Transistor NPN | 2N2222 / BC547 | Driver K-Line (alternativo ao L9637D) |
| 2 | Diodo | 1N4148 | Proteção K-Line |
| 4 | Capacitor | 100 nF | Desacoplamento VCC |
| 2 | Capacitor | 10 µF | Filtro alimentação |

---

## Circuito Interface CAN (ISO 15765-4)

### Transceiver SN65HVD230

```
ESP32              SN65HVD230              OBD-II Conector
                  ┌───────────┐
GPIO_TX_CAN ─────►│ TXD   CANH├──────────── Pino 6 (CAN High)
                  │           │
GPIO_RX_CAN ◄─────│ RXD   CANL├──────────── Pino 14 (CAN Low)
                  │           │
3.3V ─────────────│ VCC    GND├──────────── GND
                  │           │
GND ──────────────│ GND    RS ├── GND (modo normal/alta velocidade)
                  └───────────┘

Resistores de terminação:
Pino 6 ──[120Ω]── Pino 14  (opcional, se for ponta do barramento)
```

### Notas CAN:
- O SN65HVD230 opera em 3.3V, compatível direto com ESP32 (sem level shifter)
- Pino RS (slope control): GND = modo rápido (até 1Mbps) | 3.3V via resistor = modo silencioso
- Resistor de terminação 120Ω: instalar apenas se o emulador for o último nó do barramento
- Para emular ECU isolada (sem barramento real): usar ambos os 120Ω

---

## Circuito Interface K-Line (ISO 9141-2 / KWP2000)

### IC Dedicado: L9637D (Recomendado)

```
ESP32              L9637D                  OBD-II Conector
                  ┌───────────┐
GPIO_TX_KLINE ───►│ TXD     K ├──────────── Pino 7 (K-Line)
                  │           │
GPIO_RX_KLINE ◄───│ RXD   VBB├──────────── Pino 16 (+12V)
                  │           │
3.3V ─────────────│ VCC    GND├──────────── Pino 4/5 (GND)
                  └───────────┘
```

### Alternativa com Transistor Discreto (sem L9637D)

```
                                    +12V (OBD Pino 16)
                                        │
                                    [1kΩ pull-up]
                                        │
GPIO_RX_KLINE ◄────────────────────────┤◄──── K-Line (OBD Pino 7)
                                        │
                                   [Diodo 1N4148]
                                        │
GPIO_TX_KLINE ──[1kΩ]──── Base ─── NPN (2N2222)
                               Emissor ─── GND
                              Coletor ─── K-Line
```

> **Atenção:** A K-Line opera em 12V. O nível lógico HIGH na K-Line é ~12V. O ESP32 é 3.3V. O L9637D faz a conversão de nível automaticamente. No circuito discreto, o pull-up para 12V combinado com o transistor NPN realiza a conversão.

---

## Circuito OBD-II Conector (Referência de Pinos)

```
OBD-II Fêmea (vista frontal do conector):

 ┌──────────────────────────────────┐
 │  1   2   3   4   5   6   7   8  │
 │    9  10  11  12  13  14  15  16 │
 └──────────────────────────────────┘

Pino  1 → Fabricante (não usado)
Pino  2 → SAE J1850 Bus+ (não implementado neste projeto)
Pino  3 → Fabricante (não usado)
Pino  4 → Chassis Ground (GND do carro)          ◄── conectar ao GND
Pino  5 → Signal Ground                          ◄── conectar ao GND
Pino  6 → CAN High (ISO 15765-4)                 ◄── CANH do SN65HVD230
Pino  7 → K-Line (ISO 9141-2 / KWP2000)         ◄── K do L9637D
Pino  8 → Fabricante (não usado)
Pino  9 → Fabricante (não usado)
Pino 10 → SAE J1850 Bus- (não implementado)
Pino 11 → Fabricante (não usado)
Pino 12 → Fabricante (não usado)
Pino 13 → Fabricante (não usado)
Pino 14 → CAN Low (ISO 15765-4)                  ◄── CANL do SN65HVD230
Pino 15 → L-Line ISO 9141-2 (opcional)           ◄── pode conectar ao K
Pino 16 → +12V (alimentação do scanner)          ◄── entrada 12V → regulador
```

---

## Alimentação do Sistema

Componente utilizado: **LM2596** (step-down/buck converter) — converte 12V OBD → 5V para o ESP32.

```
OBD Pino 16 (+12V)
        │
    [Fusível 1A]          ⚠️ Ajustar potenciômetro do LM2596
        │                    para 5.0V na saída ANTES de ligar
   ┌────▼──────────┐         ao ESP32
   │   LM2596      │
   │  IN+   OUT+ ──┼──────── ESP32 VIN (5V)
   │  IN-   OUT- ──┼──────── GND
   └───────────────┘

ESP32 gera internamente 3.3V a partir do VIN (5V).
L9637D (K-Line driver) também alimentado com 5V (VS + EN).
SN65HVD230, OLED, KY-040 → 3.3V (saída interna do ESP32).
```

> **Alimentação dupla:** Durante desenvolvimento, o ESP32 pode ser alimentado pelo USB (5V). Em operação com o veículo/scanner, usar o LM2596 ligado ao pino 16 do OBD-II. Não é necessário diodo — o USB e o VIN do ESP32 têm proteção interna.

---

## Interface de Usuário — Botões e Display

```
                ┌─────────────────────────────┐
                │       OLED 128x64 (I2C)     │
                │  Protocolo: CAN 11b 500k    │
                │  RPM:  1500    Vel: 60 km/h │
                │  Temp: 90°C    TPS: 25%     │
                └─────────────────────────────┘

   [◄ PREV]  [NEXT ►]  [▲ UP]  [▼ DOWN]  [SELECT]  [PROTOCOL]

   Encoder Rotativo: ajusta valor do parâmetro selecionado
   DIP Switch 3 pos: seleção rápida de protocolo
```

### Mapeamento de Botões

| Botão | Função |
|-------|--------|
| PREV / NEXT | Navega entre os 12 parâmetros |
| UP / DOWN | Incrementa / decrementa valor do parâmetro ativo |
| SELECT | Confirma / edita parâmetro selecionado |
| PROTOCOL | Cicla pelo protocolo ativo |
| Encoder (rotação) | Ajuste fino do valor |
| Encoder (pressão) | Confirma seleção |

---

## Verificação de Compatibilidade do ESP32

O ESP32-WROOM-32 (38 pinos) possui:

| Recurso | Disponibilidade | Uso no Projeto |
|---------|----------------|----------------|
| TWAI (CAN) | Nativo (1x) | Protocolos ISO 15765-4 |
| UART0 | Nativo | Debug / USB Serial |
| UART1 | Nativo | K-Line TX/RX |
| UART2 | Nativo | Reserva |
| I2C | 2x (qualquer GPIO) | Display OLED |
| GPIO | 34 disponíveis | Botões, LEDs, DIP switch |
| Flash | 4MB | Firmware |
| RAM | 520KB | Buffers e estado |
