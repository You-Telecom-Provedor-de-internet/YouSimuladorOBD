# 05 — PIDs OBD-II e Formato de Dados

## Modo 01 — Dados Correntes (Current Data)

### PID 0x00 — PIDs Suportados [01–20]

Obrigatório. O scanner sempre consulta este PID primeiro para saber quais PIDs a ECU suporta.

```
Resposta: 4 bytes = bitmask de PIDs 01–20
Bit 31 (MSB) = PID 0x01, ..., Bit 0 (LSB) = PID 0x20

PIDs que implementamos: 0x04, 0x05, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11
Bitmask: PIDs 01–20:
  PID04 = bit 28 → 0x10000000
  PID05 = bit 27 → 0x08000000
  PID0B = bit 21 → 0x00200000
  PID0C = bit 20 → 0x00100000
  PID0D = bit 19 → 0x00080000
  PID0E = bit 18 → 0x00040000
  PID0F = bit 17 → 0x00020000
  PID10 = bit 16 → 0x00010000
  PID11 = bit 15 → 0x00008000
  PID20 = bit  0 → 0x00000001 (indica que há PIDs 21–40)

Valor: 0x10000000 | 0x08000000 | 0x00200000 | 0x00100000 |
       0x00080000 | 0x00040000 | 0x00020000 | 0x00010000 |
       0x00008000 | 0x00000001
     = 0x183F8001

Resposta:  04 41 00 18 3F 80 01
```

### PID 0x20 — PIDs Suportados [21–40]

```
PIDs que implementamos neste range: 0x2F
  PID2F = bit 17 (de 0x21 = bit31 até 0x40 = bit0)
  Posição relativa: 0x2F - 0x21 = 14 → bit 31-14 = bit 17 → 0x00020000

Valor: 0x00020000 | 0x00000001 (indica suporte a range 41-60, mesmo que vazio)
     = 0x00020001

Resposta: 06 41 20 00 02 00 01
```

---

## Tabela de PIDs Implementados

### PID 0x04 — Carga Calculada do Motor

| Campo | Valor |
|-------|-------|
| Bytes de resposta | 1 |
| Fórmula | Valor = A × 100 / 255 (%) |
| Fórmula inversa (codificar) | A = valor_percent × 255 / 100 |
| Range | 0–100% |
| Default simulação | 25% (A = 0x40) |

```
Exemplo: 30% carga
A = 30 × 255 / 100 = 76 = 0x4C
Resposta: 03 41 04 4C
```

### PID 0x05 — Temperatura do Líquido Refrigerante

| Campo | Valor |
|-------|-------|
| Bytes de resposta | 1 |
| Fórmula | Temp(°C) = A − 40 |
| Fórmula inversa | A = temp + 40 |
| Range | −40 a +215°C |
| Default simulação | 90°C (A = 0x82) |

```
Exemplo: 90°C
A = 90 + 40 = 130 = 0x82
Resposta: 03 41 05 82
```

### PID 0x0B — Pressão do Coletor de Admissão (MAP)

| Campo | Valor |
|-------|-------|
| Bytes de resposta | 1 |
| Fórmula | Pressão(kPa) = A |
| Range | 0–255 kPa |
| Default simulação | 35 kPa em idle (A = 0x23) |

```
Exemplo: 35 kPa
Resposta: 03 41 0B 23
```

### PID 0x0C — Rotação do Motor (RPM)

| Campo | Valor |
|-------|-------|
| Bytes de resposta | 2 |
| Fórmula | RPM = (A × 256 + B) / 4 |
| Fórmula inversa | raw = RPM × 4; A = raw >> 8; B = raw & 0xFF |
| Range | 0–16383.75 RPM |
| Default simulação | 800 RPM (idle) |

```
Exemplo: 1500 RPM
raw = 1500 × 4 = 6000 = 0x1770
A = 0x17, B = 0x70
Resposta: 04 41 0C 17 70

Exemplo: 800 RPM
raw = 3200 = 0x0C80
A = 0x0C, B = 0x80
Resposta: 04 41 0C 0C 80
```

### PID 0x0D — Velocidade do Veículo

| Campo | Valor |
|-------|-------|
| Bytes de resposta | 1 |
| Fórmula | Vel(km/h) = A |
| Range | 0–255 km/h |
| Default simulação | 0 km/h |

```
Exemplo: 60 km/h
A = 0x3C
Resposta: 03 41 0D 3C
```

### PID 0x0E — Ângulo de Avanço de Ignição

| Campo | Valor |
|-------|-------|
| Bytes de resposta | 1 |
| Fórmula | Ângulo(°) = A / 2 − 64 |
| Fórmula inversa | A = (ângulo + 64) × 2 |
| Range | −64 a +63.5° antes do PMS |
| Default simulação | 10° (A = 0x94) |

```
Exemplo: 10°
A = (10 + 64) × 2 = 148 = 0x94
Resposta: 03 41 0E 94
```

### PID 0x0F — Temperatura do Ar de Admissão (IAT)

| Campo | Valor |
|-------|-------|
| Bytes de resposta | 1 |
| Fórmula | Temp(°C) = A − 40 |
| Fórmula inversa | A = temp + 40 |
| Range | −40 a +215°C |
| Default simulação | 30°C (A = 0x46) |

```
Exemplo: 30°C
A = 30 + 40 = 70 = 0x46
Resposta: 03 41 0F 46
```

### PID 0x10 — Fluxo de Ar de Admissão (MAF)

| Campo | Valor |
|-------|-------|
| Bytes de resposta | 2 |
| Fórmula | MAF(g/s) = (A × 256 + B) / 100 |
| Fórmula inversa | raw = MAF × 100; A = raw >> 8; B = raw & 0xFF |
| Range | 0–655.35 g/s |
| Default simulação | 3.0 g/s idle |

```
Exemplo: 3.0 g/s
raw = 300 = 0x012C
A = 0x01, B = 0x2C
Resposta: 04 41 10 01 2C
```

### PID 0x11 — Posição Absoluta do Acelerador (TPS)

| Campo | Valor |
|-------|-------|
| Bytes de resposta | 1 |
| Fórmula | TPS(%) = A × 100 / 255 |
| Fórmula inversa | A = tps_percent × 255 / 100 |
| Range | 0–100% |
| Default simulação | 15% (A = 0x26) |

```
Exemplo: 15%
A = 15 × 255 / 100 = 38 = 0x26
Resposta: 03 41 11 26
```

### PID 0x2F — Nível de Combustível

| Campo | Valor |
|-------|-------|
| Bytes de resposta | 1 |
| Fórmula | Nível(%) = A × 100 / 255 |
| Fórmula inversa | A = nivel_percent × 255 / 100 |
| Range | 0–100% |
| Default simulação | 75% |

```
Exemplo: 75%
A = 75 × 255 / 100 = 191 = 0xBF
Resposta: 03 41 2F BF
```

---

## Modo 03 — Códigos de Falha (DTCs)

### Formato DTC

Cada DTC ocupa 2 bytes:

```
Byte 1:  Bits 7–6 = Categoria
           00 = P (Powertrain)
           01 = C (Chassis)
           10 = B (Body)
           11 = U (Network)
         Bits 5–4 = Sub-tipo
         Bits 3–0 = Dígito 1 (hexadecimal)
Byte 2:  Dígitos 2 e 3 (hex)

Exemplo DTC P0300 (falha de ignição aleatória):
P = 00, sub = 03, código = 00
Byte1 = 0000 0011 = 0x03
Byte2 = 0x00
Frame: 03 00

DTC P0171 (mistura pobre banco 1):
Byte1 = 0x01, Byte2 = 0x71
Frame: 01 71
```

### Resposta Modo 03

```
Se não há DTCs:
02 43 00
── ── ──
│  │  └── 0 DTCs
│  └───── Modo 03 resposta (0x43)
└──────── Tamanho: 2 bytes

Se há 2 DTCs (P0300 e P0171):
06 43 02 03 00 01 71
── ── ── ───── ─────
│  │  │  └DTC2 └DTC1
│  │  └── 2 DTCs
│  └───── Modo 03 resposta
└──────── 6 bytes de dados
```

### DTCs Disponíveis para Simulação

| Código | Descrição |
|--------|-----------|
| P0100 | Circuito MAF — falha |
| P0171 | Mistura muito pobre — Banco 1 |
| P0300 | Falha de ignição aleatória |
| P0301 | Falha cilindro 1 |
| P0420 | Eficiência catalisador abaixo do limiar |
| P0500 | Sensor de velocidade — falha |
| P0505 | Sistema de controle de marcha lenta |
| P0700 | Falha no sistema de transmissão |

---

## Modo 04 — Limpar DTCs

```
Requisição: 01 04
Resposta:   01 44  (confirmação positiva, sem dados adicionais)
```

Ao receber Mode 04, o firmware deve:
1. Limpar a lista de DTCs ativos
2. Resetar contadores de falha
3. Responder 0x44

---

## Modo 09 — Informações do Veículo (VIN)

### PID 0x02 — VIN (Vehicle Identification Number)

O VIN tem 17 caracteres ASCII. Requer frame multi-parte (ISO-TP):

```
Requisição: 02 09 02
Resposta (17 bytes = multi-frame):

First Frame:
CAN ID: 0x7E8
10 14 49 02 01 31 47 31  (primeiro byte 01 = numero de mensagem)
── ── ── ── ── ── ── ──
│  │  │  │  │  └─────── Primeiros 3 chars do VIN
│  │  │  │  └────────── Message count (sempre 0x01 para VIN)
│  │  │  └────────────── PID 0x02
│  │  └───────────────── Mode 09 resposta = 0x49
│  └──────────────────── Total de 20 bytes (0x14)
└─────────────────────── First Frame (0x1X)

Flow Control (scanner → ECU):
30 00 00

Consecutive Frame 1:
21 4A 34 47 38 35 42 34   (próximos 7 chars)

Consecutive Frame 2:
22 32 34 35 36 37 00 00   (últimos chars + padding)

VIN completo: "1G1JG8524B245" (exemplo 13 chars visíveis — VIN real tem 17)
```

### VIN Padrão do Simulador

```
VIN: YOUSIM00000000001
     ────────────────
     Y=Y, O=O, U=U (fabricante fictício "YOU")
     SIM = Tipo simulador
     000000001 = número de série
```

---

## Resumo de Ranges e Defaults dos 12 Parâmetros

| # | Parâmetro | PID | Mín | Máx | Default | Passo |
|---|-----------|-----|-----|-----|---------|-------|
| 1 | RPM | 0x0C | 0 | 16000 | 800 | 50 RPM |
| 2 | Velocidade | 0x0D | 0 | 255 | 0 | 5 km/h |
| 3 | Temp. refrigerante | 0x05 | -40 | 215 | 90 | 5°C |
| 4 | Temp. ar admissão | 0x0F | -40 | 80 | 30 | 1°C |
| 5 | Fluxo MAF | 0x10 | 0.0 | 655.0 | 3.0 | 0.5 g/s |
| 6 | Pressão MAP | 0x0B | 0 | 255 | 35 | 5 kPa |
| 7 | Pos. acelerador | 0x11 | 0 | 100 | 15 | 5% |
| 8 | Avanço ignição | 0x0E | -64 | 63.5 | 10 | 1° |
| 9 | Carga motor | 0x04 | 0 | 100 | 25 | 5% |
| 10 | Nível combustível | 0x2F | 0 | 100 | 75 | 5% |
| 11 | DTC ativo | — | 0 | 8 DTCs | nenhum | — |
| 12 | VIN | — | — | — | YOUSIM…001 | — |
