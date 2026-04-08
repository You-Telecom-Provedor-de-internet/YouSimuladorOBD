# 07 — Interface de Controle (Botões / UI)

## Visão Geral da Interface

O simulador possui interface física para ajuste em tempo real dos 16 parâmetros sem necessidade de computador:

```
┌──────────────────────────────────────────────┐
│        OLED SH1107 128×128 — Tela Principal  │
│  ┌────────────────────────────────────────┐  │
│  │ CAN 11b 500k                           │  │
│  │ ────────────────────────────────────── │  │
│  │ ► RPM           2000                   │  │
│  │   Vel km/h        60                   │  │
│  │   T.Motor C       92                   │  │
│  │   T.Admis C       35                   │  │
│  │   MAF g/s       12.0                   │  │
│  │   MAP kPa         55                   │  │
│  │   TPS %           35                   │  │
│  │   Avanco gr     15.0                   │  │
│  │ ──────────────────────────────────     │  │
│  │ OK=edit  p.1/2                         │  │
│  └────────────────────────────────────────┘  │
│                                              │
│  [◄]  [►]  [▲]  [▼]  [OK]  [PROTO]         │
│                           ┌──┐               │
│                      ENC: │  │               │
│                           └──┘               │
└──────────────────────────────────────────────┘
```

### Foto do display em funcionamento

![SH1107 128x128 funcionando](img/oled_sh1107_funcionando.jpg)

---

## Telas do Sistema

### Tela 1 — Página 1 (parâmetros 1–8)

Exibe os 8 primeiros parâmetros com protocolo ativo no header:

```
┌──────────────────────┐
│CAN 11b 500k          │  ← header
│──────────────────────│  ← separador
│► RPM          2000   │
│  Vel km/h       60   │
│  T.Motor C      92   │
│  T.Admis C      35   │
│  MAF g/s      12.0   │
│  MAP kPa        55   │
│  TPS %          35   │
│  Avanco gr    15.0   │
│──────────────────────│
│OK=edit  p.1/2        │  ← footer
└──────────────────────┘
```

### Tela 2 — Página 2 (parâmetros 9–16)

```
┌──────────────────────┐
│CAN 11b 500k          │
│──────────────────────│
│  Carga %        45   │
│  Comb. %        60   │
│  Bat. V       14.0   │
│  Oleo C         90   │
│  STFT %        0.0   │
│  LTFT %        0.0   │
│  DTC          0 DTC  │
│  VIN    YOUSIM...001 │
│──────────────────────│
│OK=edit  p.2/2        │
└──────────────────────┘
```

### Tela 2 — Edição de Parâmetro

Ao pressionar OK em um parâmetro:

```
┌──────────────────────┐
│  [EDITAR]            │
│  RPM do Motor        │
│                      │
│  ◄  1500 rpm  ►      │
│     (▲ / ▼ / enc)    │
└──────────────────────┘
```

### Tela 3 — Gerenciar DTCs

Acessível via menu:

```
┌──────────────────────┐
│  DTCs Ativos: 1      │
│  ► P0300 Ign. Rand.  │
│    [ADD] [REMOVE]    │
│    [CLEAR ALL]       │
└──────────────────────┘
```

### Tela 4 — Seleção de Protocolo

Ao pressionar PROTO:

```
┌──────────────────────┐
│  Protocolo OBD:      │
│  ► CAN 11b 500k      │
│    CAN 11b 250k      │
│    CAN 29b 500k      │
│    ISO 9141-2        │
└──────────────────────┘
▲▼ = navegar | OK = selec.
```

### Tela 5 — Informações do Sistema

```
┌──────────────────────┐
│  YouSimuladorOBD     │
│  v1.0.0              │
│  VIN: YOUSIM...001   │
│  Proto: CAN 11b 500k │
└──────────────────────┘
```

---

## Mapeamento de Botões por Contexto

### Contexto: Tela Principal (Monitor)

| Botão | Ação |
|-------|------|
| ▲ (UP) | Sobe parâmetro selecionado (cursor) |
| ▼ (DOWN) | Desce parâmetro selecionado |
| ◄ (PREV) | Página anterior (se scroll) |
| ► (NEXT) | Próxima página |
| OK (SELECT) | Entra no modo edição do parâmetro |
| PROTO | Vai para tela de seleção de protocolo |
| Encoder (giro) | Scroll na lista de parâmetros |
| Encoder (press) | Igual ao OK |

### Contexto: Tela Edição de Parâmetro

| Botão | Ação |
|-------|------|
| ▲ (UP) | Incrementa valor (passo normal) |
| ▼ (DOWN) | Decrementa valor |
| ◄ (PREV) | Decrementa valor rápido (×5) |
| ► (NEXT) | Incrementa valor rápido (×5) |
| OK (SELECT) | Confirma e sai para monitor |
| PROTO | Cancela edição (volta ao valor anterior) |
| Encoder (giro) | Ajuste fino (±1 unidade mínima) |
| Encoder (press) | Confirma e sai |

### Contexto: Seleção de Protocolo

| Botão | Ação |
|-------|------|
| ▲ / ▼ | Navega entre protocolos |
| OK | Seleciona protocolo e reinicia interface |
| PROTO | Cancela |

---

## Passos de Incremento por Parâmetro

| Parâmetro | Passo (botão/encoder) |
|-----------|----------------------|
| RPM | 50 rpm |
| Velocidade | 5 km/h |
| Temp. refrigerante | 5°C |
| Temp. ar admissão | 1°C |
| MAF | 0.5 g/s |
| MAP | 5 kPa |
| TPS | 5% |
| Avanço ignição | 1° |
| Carga motor | 5% |
| Nível combustível | 5% |
| Tensão bateria | 0.1 V |
| Temp. óleo | 5°C |
| STFT | 0.8% |
| LTFT | 0.8% |
| DTC / VIN | — (somente via Web UI) |

---

## Fluxo de Menus

```
[Boot]
   │
   ▼
[Tela Principal / Monitor]  ◄─────────────────────────────┐
   │                                                       │
   ├── OK (no parâmetro) ──► [Edição do Parâmetro] ──► OK ┤
   │                                                       │
   ├── PROTO ────────────► [Seleção de Protocolo] ──► OK──┤
   │                                                       │
   └── OK (no DTC) ──────► [Gerenciar DTCs] ─────── OK────┘
```

---

## Lógica de Debounce e Encoder

```cpp
// Configurações de debounce
#define BTN_DEBOUNCE_MS     50      // 50ms debounce para botões
#define ENCODER_MIN_PULSE   5       // pulsos mínimos encoder por incremento
#define BTN_LONG_PRESS_MS   800     // 800ms = long press (incremento ×5)

// Detecção de long press
if (btn_held_time > BTN_LONG_PRESS_MS) {
    step = step_fast;
} else {
    step = step_normal;
}
```

---

## Display OLED SH1107 — Layout de Pixels

```
Resolução: 128 × 128 pixels  |  Driver: SH1107  |  I2C: 0x3C

┌─ 0,0 ──────────────────────── 127,0 ─┐
│ Header: protocolo ativo (21 chars)    │ y: 0–8
├───────────────────────────────────────┤ y: 9  (drawFastHLine)
│ Param 0  "► RPM          2000"        │ y: 11
│ Param 1  "  Vel km/h       60"        │ y: 24
│ Param 2  "  T.Motor C      92"        │ y: 37
│ Param 3  "  T.Admis C      35"        │ y: 50
│ Param 4  "  MAF g/s      12.0"        │ y: 63
│ Param 5  "  MAP kPa        55"        │ y: 76
│ Param 6  "  TPS %          35"        │ y: 89
│ Param 7  "  Avanco gr    15.0"        │ y: 102
├───────────────────────────────────────┤
│ Footer: dica contextual               │ y: 119
└─ 0,127 ─────────────────────── 127,127┘

Cursor: "► " (2 chars) antes do nome do parâmetro
Editando: fundo branco / texto preto (invertido)
Scroll: 2 páginas de 8 parâmetros (total: 16)
```

### Ligação I2C (SH1107 → ESP32)

| Pino OLED | Pino ESP32 | GPIO |
|-----------|-----------|------|
| VCC | 3.3V | — |
| GND | GND | — |
| SCL | GPIO22 | SCL |
| SDA | GPIO21 | SDA |

---

## LEDs de Status

| LED | Cor | Comportamento |
|-----|-----|---------------|
| LED_TX | Vermelho | Pisca 50ms a cada frame OBD enviado |
| LED interno (GPIO2) | Azul | Pisca 1Hz = sistema OK; rápido = erro |

---

## Presets de Cenários

Para facilitar demonstrações, o usuário pode carregar presets predefinidos:

| Preset | RPM | Vel | Temp | TPS | Carga | DTC |
|--------|-----|-----|------|-----|-------|-----|
| Motor desligado | 0 | 0 | 20°C | 0% | 0% | — |
| Marcha lenta | 800 | 0 | 90°C | 15% | 20% | — |
| Aceleração suave | 2000 | 60 | 92°C | 35% | 45% | — |
| Plena carga | 5000 | 120 | 98°C | 90% | 85% | — |
| Superaquecendo | 900 | 0 | 115°C | 15% | 25% | P0300 |
| Falha catalisador | 1200 | 40 | 90°C | 25% | 35% | P0420 |

Para acessar presets: manter PROTO pressionado por 2s na tela principal.
