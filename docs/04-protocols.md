# 04 — Protocolos OBD-II

## Visão Geral dos 7 Protocolos

```
┌───┬─────────────────────────────┬──────────┬────────────┬──────────────┐
│ # │ Protocolo                   │ Vel.     │ ID         │ Física       │
├───┼─────────────────────────────┼──────────┼────────────┼──────────────┤
│ 1 │ ISO 15765-4 CAN 11-bit      │ 500 kbps │ 0x7DF/7E8  │ CAN Bus      │
│ 2 │ ISO 15765-4 CAN 11-bit      │ 250 kbps │ 0x7DF/7E8  │ CAN Bus      │
│ 3 │ ISO 15765-4 CAN 29-bit      │ 500 kbps │ 18DB33F1   │ CAN Bus      │
│ 4 │ ISO 15765-4 CAN 29-bit      │ 250 kbps │ 18DB33F1   │ CAN Bus      │
│ 5 │ ISO 9141-2                  │ 10400 bd │ —          │ K-Line UART  │
│ 6 │ ISO 14230-4 KWP2000 5-baud  │ 10400 bd │ —          │ K-Line UART  │
│ 7 │ ISO 14230-4 KWP2000 Fast    │ 10400 bd │ —          │ K-Line UART  │
└───┴─────────────────────────────┴──────────┴────────────┴──────────────┘
```

---

## Protocolos 1–4: ISO 15765-4 (CAN / ISO-TP)

### Camada Física CAN

- **Standard (11-bit):** IDs de 0 a 0x7FF
- **Extended (29-bit):** IDs de 0 a 0x1FFFFFFF
- **Velocidades:** 500 kbps (padrão pós-2008) ou 250 kbps (veículos anteriores)
- **Payload máximo por frame:** 8 bytes
- **Mensagens OBD longas:** fragmentadas via ISO-TP (ISO 15765-2)

### Identificadores CAN para OBD-II

#### Modo 11-bit (Standard)

| Direção | ID | Descrição |
|---------|----|-----------|
| Requisição (scanner → ECU) | 0x7DF | Endereço funcional (broadcast — todos os ECUs) |
| Requisição (scanner → ECU) | 0x7E0 | Endereço físico ECU 1 (motor) |
| Resposta (ECU → scanner) | 0x7E8 | Resposta ECU 1 (motor) |
| Resposta (ECU → scanner) | 0x7E9 | Resposta ECU 2 |

#### Modo 29-bit (Extended)

| Direção | ID | Descrição |
|---------|----|-----------|
| Requisição (scanner → ECU) | 0x18DB33F1 | Endereço funcional (broadcast) |
| Requisição (scanner → ECU) | 0x18DA10F1 | Endereço físico ECU motor |
| Resposta (ECU → scanner) | 0x18DAF110 | Resposta ECU motor |

> **Nota:** No 29-bit, o byte mais significativo indica prioridade + tipo. `0x18DA` = prioridade 6, tipo physical. `F1` = ID do testador (scanner). `10` = ID do ECU motor.

### Formato de Frame ISO-TP (ISO 15765-2)

#### Single Frame (SF) — payload ≤ 7 bytes

```
Byte 0: [N_PCI = 0x0N]  onde N = tamanho dos dados (1–7)
Bytes 1–7: dados OBD-II

Exemplo: Requisitar RPM (Mode 01, PID 0x0C)
CAN ID: 0x7DF
Data:  02 01 0C 00 00 00 00 00
       ── ── ──
       │  │  └── PID 0x0C
       │  └───── Mode 01 (dados correntes)
       └──────── N_PCI: Single Frame, 2 bytes de dados
```

#### Resposta Single Frame

```
Exemplo: Resposta RPM = 2000 RPM
CAN ID: 0x7E8
Data:  04 41 0C 0F A0 00 00 00
       ── ── ── ─────
       │  │  │  └── Valor: 0x0FA0 = 4000 → 4000/4 = 1000 RPM
       │  │  └───── PID 0x0C
       │  └──────── Mode 01 + 0x40 = 0x41 (resposta positiva)
       └─────────── N_PCI: Single Frame, 4 bytes
```

#### First Frame (FF) + Consecutive Frame (CF) — payload > 7 bytes

```
First Frame:
Byte 0: [0x1N] bits 11–8 do tamanho total
Byte 1: bits 7–0 do tamanho total
Bytes 2–7: primeiros 6 bytes de dados

Flow Control (scanner → ECU após FF):
Byte 0: 0x30  (FC, ContinueToSend)
Byte 1: 0x00  (BlockSize = 0 → sem limite)
Byte 2: 0x00  (STmin = 0ms)

Consecutive Frame:
Byte 0: [0x2N] onde N = número de sequência (1–F, depois volta a 0)
Bytes 1–7: próximos 7 bytes de dados
```

### Sequência de Comunicação CAN (Exemplo: requisitar RPM)

```
SCANNER                              ESP32 (ECU emulado)
   │                                        │
   │─── CAN 0x7DF: 02 01 0C 00 00 00 00 00 ──►│  Requisição RPM
   │                                        │  (processa, busca valor)
   │◄── CAN 0x7E8: 04 41 0C 0F A0 00 00 00 ───│  Resposta: RPM 1000
   │                                        │
```

### Configuração TWAI (ESP32)

```c
// Exemplo de configuração para CAN 11-bit 500kbps
twai_general_config_t g_config = {
    .mode = TWAI_MODE_NORMAL,
    .tx_io = GPIO_NUM_4,
    .rx_io = GPIO_NUM_5,
    .clkout_io = TWAI_IO_UNUSED,
    .bus_off_io = TWAI_IO_UNUSED,
    .tx_queue_len = 10,
    .rx_queue_len = 10,
    .alerts_enabled = TWAI_ALERT_NONE,
    .clkout_divider = 0
};

twai_timing_config_t t_config = TWAI_TIMING_CONFIG_500KBITS();
// ou: TWAI_TIMING_CONFIG_250KBITS()

twai_filter_config_t f_config = {
    .acceptance_code = (0x7DF << 21),  // aceita 0x7DF e 0x7E0
    .acceptance_mask = ~(0x7FF << 21), // máscara 11-bit
    .single_filter = true
};
```

---

## Protocolo 5: ISO 9141-2

### Características

- **Velocidade:** 10400 baud
- **Física:** K-Line (pino 7 OBD-II), half-duplex, nível 12V
- **Init:** Sequência de 5 baud (endereço 0x33 a 5 baud)
- **ECU Address:** 0x10 (motor), 0x11 (caixa), 0x01 (sensor lambda)
- **Padrão:** Veículos europeus e alguns asiáticos pré-2005

### Sequência de Inicialização ISO 9141-2

```
Passo 1: K-Line IDLE (HIGH / 12V) por no mínimo 300ms

Passo 2: Enviar endereço 0x33 a 5 baud (bit time = 200ms)
  Trama 5-baud do byte 0x33 = 0b00110011:
  ─HIGH─ [IDLE 300ms]
  ─LOW──────200ms── [Start bit]
  ─HIGH─────200ms── [bit 0 = 1]
  ─HIGH─────200ms── [bit 1 = 1]
  ─LOW──────200ms── [bit 2 = 0]
  ─LOW──────200ms── [bit 3 = 0]
  ─HIGH─────200ms── [bit 4 = 1]
  ─HIGH─────200ms── [bit 5 = 1]
  ─LOW──────200ms── [bit 6 = 0]
  ─LOW──────200ms── [bit 7 = 0]
  ─HIGH─────200ms── [Stop bit]

Passo 3: Aguardar 15–20ms

Passo 4: ECU responde sync byte 0x55 a 10400 baud

Passo 5: ECU envia Key Byte 1 (KB1) e Key Byte 2 (KB2)
  Tipicamente KB1=0x08, KB2=0x08 (ISO 9141-2)

Passo 6: Scanner envia complemento de KB2 (inverso de KB2)
  Se KB2 = 0x08 → envia 0xF7 (~0x08)

Passo 7: ECU responde complemento de endereço 0x33 → 0xCC (~0x33)

Passo 8: Link estabelecido. Comunicação a 10400 baud.
```

### Formato de Mensagem ISO 9141-2

```
┌──────────┬──────────┬──────────┬──────────────┬──────────┐
│  HEADER  │  TARGET  │  SOURCE  │   DATA[1–7]  │ CHECKSUM │
│  0x68    │  0x6A    │  0xF1    │              │   XOR    │
└──────────┴──────────┴──────────┴──────────────┴──────────┘

Exemplo requisitar RPM (Mode 01, PID 0x0C):
68 6A F1 01 0C CC
── ── ── ── ── ──
│  │  │  │  │  └── Checksum = soma dos bytes anteriores & 0xFF
│  │  │  │  └───── PID 0x0C
│  │  │  └──────── Mode 01
│  │  └─────────── Source: 0xF1 (testador / scanner)
│  └────────────── Target: 0x6A (ECU motor broadcast)
└───────────────── Header: 0x68 (3 bytes, sem comprimento)

Resposta (ECU → scanner):
48 6B F1 41 0C 0F A0 CE
── ── ── ── ── ───── ──
│  │  │  │  │  └── Valor RPM: 0x0FA0/4 = 1000 RPM
│  │  │  │  └───── PID
│  │  │  └──────── Mode 0x41 (resposta positiva Mode 01)
│  │  └─────────── Source: 0xF1
│  └────────────── Target: 0x6B (resposta ECU)
└───────────────── Header: 0x48
```

---

## Protocolo 6: ISO 14230-4 KWP2000 — Init 5-baud

### Diferenças em relação ao ISO 9141-2

| Aspecto | ISO 9141-2 | KWP2000 5-baud |
|---------|-----------|----------------|
| Key Bytes | 0x08, 0x08 | 0x8F, 0x65 ou 0xEF, 0x8F |
| Formato mensagem | Fixo (cabeçalho 3 bytes) | Variável (com byte de comprimento) |
| Velocidade pós-init | 10400 baud | 10400 baud |
| Init | 5-baud 0x33 | 5-baud 0x33 |

### Formato de Mensagem KWP2000

```
┌──────────────┬────────┬────────┬────────────┬──────────┐
│   HEADER     │  LEN   │ TARGET │   DATA     │ CHECKSUM │
│  0x80 ou     │ N bytes│  ADDR  │            │          │
│  0xC0/0x81   │        │        │            │          │
└──────────────┴────────┴────────┴────────────┴──────────┘

Formato sem comprimento no header (modo curto):
80 F1 10 02 01 0C CS
── ── ── ── ── ── ──
│  │  │  │  │  └── PID
│  │  │  │  └───── Mode
│  │  │  └──────── Comprimento de dados
│  │  └─────────── Source ECU address (0x10 = motor)
│  └────────────── Target (0xF1 = testador)
└───────────────── Header format byte

Resposta KWP2000:
80 F1 10 03 41 0C 0F CS
(estrutura similar)
```

---

## Protocolo 7: ISO 14230-4 KWP2000 — Fast Init

### Sequência de Fast Init

```
Passo 1: K-Line IDLE (HIGH) por no mínimo 300ms

Passo 2: Fast Init Pattern (25ms LOW + 25ms HIGH):
  ─HIGH─ [IDLE ≥300ms]
  ─LOW──────────────── 25ms ────────────┐
  ─HIGH─────────────── 25ms ────────────┘

Passo 3: Enviar StartCommunication a 10400 baud:
  Trama: 81 F1 10 81 03
         ── ── ── ── ──
         │  │  │  └── Checksum
         │  │  └───── Header (StartComm service byte 0x81)
         │  └──────── Source ECU (0x10)
         └─────────── Target (0xF1)

Passo 4: ECU responde StartCommunication Response:
  C1 57 8F E9 0A
  (inclui key bytes 0x57, 0x8F)

Passo 5: Link estabelecido. Comunicação normal a 10400 baud.
```

> **Vantagem do Fast Init:** Muito mais rápido que o 5-baud init (< 100ms vs ~3 segundos). Padrão em veículos pós-2001.

---

## Tempos e Timing (P1–P4)

### Parâmetros de Timing ISO 9141-2 / KWP2000

| Parâmetro | Descrição | Valor |
|-----------|-----------|-------|
| P1 | Tempo mínimo inter-byte (ECU → tester) | 0–20ms |
| P2 | Tempo máximo resposta ECU | 25ms típico, ≤50ms |
| P3 | Tempo entre fim de resposta e próxima requisição | 55ms ≤ P3 ≤ 5s |
| P4 | Tempo inter-byte (tester → ECU) | 5–20ms |
| W1 | Tempo após 5-baud init antes de sync byte | 15–20ms |
| W2 | Tempo entre sync e key bytes | ≤ 300ms |
| W3 | Tempo entre KB2 e complemento | ≤ 200ms |
| W4 | Tempo entre complemento KB2 e complemento endereço | 25–300ms |

### Timing CAN (ISO 15765-4)

| Parâmetro | Valor |
|-----------|-------|
| Tempo máx. de resposta (single frame) | 50ms |
| Tempo máx. entre First Frame e Flow Control | 1000ms |
| Separação entre Consecutive Frames (STmin) | 0–127ms |

---

## Lógica de Seleção de Protocolo no Firmware

```
Ao inicializar:
1. Lê DIP Switch (3 bits) → código 0–7
2. Ativa interface:
   - Código 0–3: inicia TWAI, configura bitrate e ID mode
   - Código 4–6: inicia UART1 10400 baud, K-Line driver
3. Aguarda primeira requisição:
   - CAN: ISR TWAI_EVENT_RX_DATA
   - K-Line: ISR UART_DATA com parser de frame

Detecção automática (modo AUTO):
1. Tenta CAN 500k 11-bit por 200ms → se receber frame, confirma
2. Tenta CAN 250k 11-bit por 200ms → idem
3. Tenta CAN 500k 29-bit → idem
4. Tenta CAN 250k 29-bit → idem
5. Tenta K-Line (aguarda 5-baud init por 5s)
6. Se nada detectado → volta ao passo 1
```
