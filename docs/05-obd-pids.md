# 05 — PIDs OBD-II e Formato de Dados

## Modo 01 — Dados Correntes (Current Data)

### PID 0x00 — PIDs Suportados [01–20]

Obrigatório. O scanner sempre consulta este PID primeiro para saber quais PIDs a ECU suporta.

```
Resposta: 4 bytes = bitmask de PIDs 01–20
Byte A: PIDs 01–08 | Byte B: PIDs 09–10 | Byte C: PIDs 11–18 | Byte D: PIDs 19–20

PIDs que implementamos: 0x04, 0x05, 0x06, 0x07, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10, 0x11
  Byte A:  PID04=bit4, PID05=bit3, PID06=bit2, PID07=bit1  → 0x1E
  Byte B:  PID0B=bit5, PID0C=bit4, PID0D=bit3, PID0E=bit2, PID0F=bit1, PID10=bit0 → 0x3F
  Byte C:  PID11=bit7                                        → 0x80
  Byte D:  PID20=bit0 (indica que há PIDs 21–40)             → 0x01

Resposta:  04 41 00 1E 3F 80 01
```

### PID 0x20 — PIDs Suportados [21–40]

```
PIDs que implementamos neste range: 0x2F
  Byte B bit 1 = PID 0x2F → 0x02
  Byte D bit 0 = PID 0x40 (indica suporte a range 41-60) → 0x01

Resposta: 06 41 20 00 02 00 01
```

### PID 0x40 — PIDs Suportados [41–60]

```
PIDs que implementamos neste range: 0x42, 0x5C
  Byte A bit 6 = PID 0x42 (tensão bateria)  → 0x40
  Byte D bit 4 = PID 0x5C (temp. óleo)      → 0x10

Resposta: 06 41 40 40 00 00 10
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

### PID 0x06 — Short Term Fuel Trim (STFT) — Banco 1

| Campo | Valor |
|-------|-------|
| Bytes de resposta | 1 |
| Fórmula | STFT(%) = (A − 128) × 100 / 128 |
| Fórmula inversa | A = (stft_pct × 128 / 100) + 128 |
| Range | −100 a +99.2% |
| Default simulação | 0.0% (A = 0x80) |

```
Exemplo: +4.7%
A = (4.7 × 128 / 100) + 128 ≈ 134 = 0x86
Resposta: 03 41 06 86

Exemplo: −3.1%
A = (−3.1 × 128 / 100) + 128 ≈ 124 = 0x7C
Resposta: 03 41 06 7C
```

> Valores |STFT| > 10% geralmente indicam problema de mistura (sensor O₂, injetor, escape).

### PID 0x07 — Long Term Fuel Trim (LTFT) — Banco 1

| Campo | Valor |
|-------|-------|
| Bytes de resposta | 1 |
| Fórmula | LTFT(%) = (A − 128) × 100 / 128 |
| Fórmula inversa | A = (ltft_pct × 128 / 100) + 128 |
| Range | −100 a +99.2% |
| Default simulação | 0.0% (A = 0x80) |

```
Exemplo: 0.0%
A = 128 = 0x80
Resposta: 03 41 07 80
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

### PID 0x42 — Tensão do Módulo de Controle (Bateria)

| Campo | Valor |
|-------|-------|
| Bytes de resposta | 2 |
| Fórmula | V = (A × 256 + B) / 1000 |
| Fórmula inversa | raw = V × 1000; A = raw >> 8; B = raw & 0xFF |
| Range | 0–65.535 V |
| Default simulação | 14.2 V (motor em marcha lenta, alternador ativo) |

```
Exemplo: 14.2 V
raw = 14200 = 0x3778
A = 0x37, B = 0x78
Resposta: 04 41 42 37 78

Valores típicos:
  Motor desligado : 12.5 V
  Marcha lenta    : 14.0–14.4 V
  Carga alta      : 13.5–14.0 V
  Alerta baixo    : < 12.0 V
  Alerta alto     : > 15.5 V
```

### PID 0x5C — Temperatura do Óleo do Motor

| Campo | Valor |
|-------|-------|
| Bytes de resposta | 1 |
| Fórmula | Temp(°C) = A − 40 |
| Fórmula inversa | A = temp + 40 |
| Range | −40 a +210°C |
| Default simulação | 85°C (A = 0x7D) |

```
Exemplo: 90°C
A = 90 + 40 = 130 = 0x82
Resposta: 03 41 5C 82

Valores típicos:
  Motor frio   : 20–40°C
  Aquecimento  : 40–80°C
  Operação     : 80–110°C
  Alerta       : > 110°C
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

## Resumo de Ranges e Defaults dos 16 Parâmetros

| # | Parâmetro | PID | Mín | Máx | Default | Alerta |
|---|-----------|-----|-----|-----|---------|--------|
| 1 | RPM | 0x0C | 0 | 16000 | 800 | > 6000 |
| 2 | Velocidade | 0x0D | 0 | 255 | 0 | — |
| 3 | Temp. refrigerante | 0x05 | -40 | 215 | 90 | > 108°C |
| 4 | Temp. ar admissão | 0x0F | -40 | 80 | 30 | — |
| 5 | Fluxo MAF | 0x10 | 0.0 | 655.0 | 3.0 | — |
| 6 | Pressão MAP | 0x0B | 0 | 255 | 35 | — |
| 7 | Pos. acelerador | 0x11 | 0 | 100 | 15 | — |
| 8 | Avanço ignição | 0x0E | -64 | 63.5 | 10 | — |
| 9 | Carga motor | 0x04 | 0 | 100 | 25 | — |
| 10 | Nível combustível | 0x2F | 0 | 100 | 75 | — |
| 11 | Tensão bateria | 0x42 | 0.0 | 65.5 | 14.2 V | < 12 V ou > 15.5 V |
| 12 | Temp. óleo | 0x5C | -40 | 210 | 85 | > 110°C |
| 13 | STFT Banco 1 | 0x06 | -30 | +30 | 0.0% | \|val\| > 10% |
| 14 | LTFT Banco 1 | 0x07 | -30 | +30 | 0.0% | \|val\| > 10% |
| 15 | DTC ativo | — | 0 | 8 DTCs | nenhum | — |
| 16 | VIN | — | — | — | YOUSIM…001 | — |
