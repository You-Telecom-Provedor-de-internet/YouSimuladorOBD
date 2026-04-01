# 02 — Hardware e Componentes

## Bill of Materials (BOM)

### Componentes Principais

| Qtd | Componente | Especificação | Função |
|-----|-----------|---------------|--------|
| 1 | ESP32 38-pin | ESP32-WROOM-32D / DevKitC | Microcontrolador principal |
| 1 | Placa expansão ESP32 | 38-pin com bornes/terminais | Facilita conexão dos cabos sem solda |
| 1 | SN65HVD230 | CAN Transceiver 3.3V | Interface física CAN bus |
| 1 | Transistor NPN | 2N2222 / BC547 | Driver K-Line (interface 12V↔3.3V) |
| 1 | OBD-II Conector | Fêmea 16 pinos + chicote | Ponto de conexão com scanner |
| 1 | Display OLED | SH1107 128×128 I2C | Exibição de 16 parâmetros (2 páginas de 8) |
| 1 | Encoder Rotativo | KY-040 | Ajuste de valores + botão (SW) |
| 6 | Botões tácteis | 6×6mm | PREV / NEXT / UP / DOWN / SELECT / PROTOCOL |
| 3 | LED 3mm | Verde, Amarelo, Vermelho | Status CAN / K-Line / TX |
| 1 | LED interno | GPIO2 (azul, já no ESP32) | Heartbeat do sistema |
| 3 | DIP Switch | 3 posições | Seleção rápida de protocolo |
| 1 | Regulador Step-Down | LM2596 3A (módulo buck) | 12V OBD → 5V ESP32 |
| 5 | Placa fenolite | Dupla face ilhada 6×8cm | Montagem dos circuitos auxiliares |

### Componentes Passivos

| Qtd | Componente | Valor | Localização |
|-----|-----------|-------|-------------|
| 1 | Resistor | 120 Ω | Terminação CAN (CANH–CANL) |
| 1 | Resistor | 510 Ω | Pull-up K-Line para 12V |
| 1 | Resistor | 1 kΩ | Base transistor K-Line |
| 4 | Resistor | 330 Ω | Limitador de corrente LEDs status |
| 2 | Resistor | 10 kΩ | Pull-up encoder / divisor |
| 1 | Diodo | 1N4148 | Proteção K-Line |
| 4 | Capacitor | 100 nF | Desacoplamento VCC |
| 2 | Capacitor | 10 µF | Filtro alimentação LM2596 |
| 1 | Fusível | 1A | Proteção linha 12V OBD |

> Kit utilizado: **120 resistores metal film 1/4W 1%** — cobre todos os valores acima com folga.

---

## Distribuição nas Placas Fenolite (6×8cm)

| Placa | Conteúdo | Tamanho estimado |
|-------|----------|-----------------|
| **#1 — Fonte** | LM2596 + capacitores + fusível | ~3×4 cm |
| **#2 — CAN** | SN65HVD230 + resistor 120Ω + conector | ~4×4 cm |
| **#3 — K-Line** | Transistor NPN + resistores 510Ω/1kΩ + diodo | ~3×3 cm |
| **#4 — Painel** | 6 botões + encoder KY-040 + cabeamento OLED | ~6×7 cm |
| **#5 — Status** | LEDs (3×) + resistores 330Ω + DIP switch 3pos | ~3×4 cm |

---

## Circuito Interface CAN (ISO 15765-4)

### Transceiver SN65HVD230

```
ESP32              SN65HVD230              OBD-II Conector
                  ┌───────────┐
GPIO4 (TX) ──────►│ TXD   CANH├──────────── Pino 6 (CAN High)
                  │           │     [120Ω]   (terminação opcional)
GPIO5 (RX) ◄──────│ RXD   CANL├──────────── Pino 14 (CAN Low)
                  │           │
3.3V ─────────────│ VCC    GND├──────────── GND
                  │           │
GND ──────────────│ GND    RS ├── GND (modo alta velocidade)
                  └───────────┘
```

### Notas CAN:
- O SN65HVD230 opera em 3.3V — compatível direto com ESP32 (sem level shifter)
- Pino RS: GND = modo rápido (até 1 Mbps) | resistor 10kΩ para 3.3V = modo silencioso
- Resistor 120Ω: instalar se o simulador for o único nó ou ponta do barramento

---

## Circuito Interface K-Line (ISO 9141-2 / KWP2000)

### Circuito com Transistor Discreto (2N2222 / BC547)

```
                            +12V (OBD Pino 16)
                                │
                            [510Ω pull-up]
                                │
GPIO16 (RX) ◄──────────────────┤◄──────── K-Line (OBD Pino 7)
                                │
                           [Diodo 1N4148]
                                │
GPIO17 (TX) ──[1kΩ]──── Base ──┤
                          Emissor ── GND
                          Coletor ── K-Line (OBD Pino 7)
```

### Notas K-Line:
- K-Line opera em 12V lógico HIGH — o transistor faz a conversão 3.3V ↔ 12V
- Baud rate: 10.400 bps (ISO 9141-2) / até 10.400 bps (KWP 5-baud init)
- L-Line (Pino 15): pode ser ligado ao K-Line ou deixado desconectado

---

## Circuito OBD-II Conector — Pinagem Completa

```
OBD-II Fêmea (vista frontal do conector):

 ┌──────────────────────────────────┐
 │  1   2   3   4   5   6   7   8  │
 │    9  10  11  12  13  14  15  16 │
 └──────────────────────────────────┘
```

| Pino | Sinal | Uso no projeto |
|------|-------|---------------|
| 1 | Fabricante | Não usado |
| 2 | SAE J1850 Bus+ | Não implementado |
| 3 | Fabricante | Não usado |
| **4** | **Chassis GND** | **→ GND do circuito** |
| **5** | **Signal GND** | **→ GND do circuito** |
| **6** | **CAN High** | **→ CANH do SN65HVD230** |
| **7** | **K-Line** | **→ Circuito transistor K-Line** |
| 8 | Fabricante | Não usado |
| 9 | Fabricante | Não usado |
| 10 | SAE J1850 Bus- | Não implementado |
| 11 | Fabricante | Não usado |
| 12 | Fabricante | Não usado |
| 13 | Fabricante | Não usado |
| **14** | **CAN Low** | **→ CANL do SN65HVD230** |
| 15 | L-Line | Opcional — ligar ao K-Line ou deixar livre |
| **16** | **+12V Bateria** | **→ Entrada LM2596 (fonte)** |

> **Conector recebido:** Fêmea 16 pinos com chicote de fios coloridos.
> Confirmar cores com multímetro antes de soldar (variam por fabricante).
> Típico: Vermelho = Pino 16 (+12V) | Preto = Pino 4/5 (GND).

---

## Alimentação do Sistema

```
OBD Pino 16 (+12V)
        │
    [Fusível 1A]      ⚠️ Ajustar LM2596 para 5.0V na saída
        │                ANTES de conectar ao ESP32
   ┌────▼──────────┐
   │   LM2596      │
   │  IN+   OUT+ ──┼──────── ESP32 VIN (5V)
   │  IN-   OUT- ──┼──────── GND
   └───────────────┘

Distribuição de tensão:
  5.0V → ESP32 VIN, L-Line driver
  3.3V → SN65HVD230, OLED SH1107, KY-040 (gerado internamente pelo ESP32)
 12.0V → K-Line pull-up (direto do OBD pino 16)
```

> **Durante desenvolvimento:** alimentar o ESP32 pelo USB (5V). Em operação com veículo, usar o LM2596. Não há conflito — o USB e o VIN do ESP32 têm diodo de proteção interno.

---

## Interface de Usuário

```
     ┌────────────────────────────────────────┐
     │  OLED SH1107 128×128 — 8 params/página │
     │  CAN 11b 500k                          │
     │  ─────────────────────────────────     │
     │  ► RPM           2000                  │
     │    Vel km/h        60                  │
     │    T.Motor C       92                  │
     │    T.Admis C       35                  │
     │    MAF g/s       12.0                  │
     │    MAP kPa         55                  │
     │    TPS %           35                  │
     │    Avanco gr     15.0                  │
     │  ─────────────────────────────────     │
     │  OK=edit  p.1/2                        │
     └────────────────────────────────────────┘

  [◄]  [►]  [▲]  [▼]  [OK]  [PROTO]    ENC: [◉]
```

### Botões — 6 no total

| Botão | GPIO | Função |
|-------|------|--------|
| PREV (◄) | 32 | Parâmetro anterior |
| NEXT (►) | 33 | Próximo parâmetro |
| UP (▲) | 25 | Incrementa valor |
| DOWN (▼) | 26 | Decrementa valor |
| SELECT (OK) | 27 | Editar / Confirmar |
| PROTOCOL | 14 | Cicla protocolo ativo |
| Encoder SW | 15 | Igual ao SELECT |

---

## Verificação de Recursos do ESP32

| Recurso | Disponibilidade | Uso no Projeto |
|---------|----------------|----------------|
| TWAI (CAN) | Nativo (1×) | CAN ISO 15765-4 |
| UART0 | Nativo | Debug / USB Serial |
| UART1 | Nativo | K-Line TX/RX |
| UART2 | Nativo | Reserva |
| I2C | 2× (qualquer GPIO) | Display OLED SH1107 |
| GPIO Input-only | 34, 35, 36, 39 | DIP Switch (3 pinos) |
| GPIO Geral | 34 disponíveis | Botões, LEDs, Encoder |
| Flash | 4 MB | Firmware + LittleFS (Web UI) |
| RAM | 520 KB | Buffers e SimulationState |
| Wi-Fi | 802.11 b/g/n | Web UI + API REST + WebSocket |
