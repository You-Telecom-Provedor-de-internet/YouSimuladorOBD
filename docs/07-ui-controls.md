# 07 — Interface de Controle (Botões / UI)

## Visão Geral da Interface

O simulador possui interface física para ajuste em tempo real dos 12 parâmetros sem necessidade de computador:

```
┌──────────────────────────────────────────────┐
│          OLED 128×64 — Tela Principal        │
│  ┌────────────────────────────────────────┐  │
│  │ Proto: CAN 11b 500k    [●CAN]          │  │
│  │ ► RPM:      1500 rpm                   │  │
│  │   Vel:        60 km/h                  │  │
│  │   Temp.R:     90 °C                    │  │
│  └────────────────────────────────────────┘  │
│                                              │
│  [◄]  [►]  [▲]  [▼]  [OK]  [PROTO]         │
│                           ┌──┐               │
│                      ENC: │  │               │
│                           └──┘               │
└──────────────────────────────────────────────┘
```

---

## Telas do Sistema

### Tela 1 — Status / Monitor

Exibe os 4 primeiros parâmetros visíveis e o protocolo ativo:

```
┌──────────────────────┐
│Proto: ISO9141        │
│►RPM:      800 rpm    │
│ Vel:        0 km/h   │
│ Temp.R:    90 °C     │
└──────────────────────┘
▲▼ = scroll | OK = editar
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

| Parâmetro | Passo Normal | Passo Rápido |
|-----------|-------------|-------------|
| RPM | 50 rpm | 250 rpm |
| Velocidade | 5 km/h | 20 km/h |
| Temp. refrigerante | 5°C | 20°C |
| Temp. ar admissão | 1°C | 5°C |
| MAF | 0.5 g/s | 2.0 g/s |
| MAP | 5 kPa | 20 kPa |
| TPS | 5% | 20% |
| Avanço ignição | 1° | 5° |
| Carga motor | 5% | 20% |
| Nível combustível | 5% | 20% |
| Encoder (giro) | 1 unidade mínima | — |

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

## Display OLED — Layout de Pixels

```
Resolução: 128 × 64 pixels

┌─ 0,0 ─────────────────────── 127,0 ─┐
│ Linha header (fonte 6×8, 16 chars)   │ y: 0–8
├──────────────────────────────────────┤
│ Linha 1 parâmetro (fonte 6×8)        │ y: 10–18
│ Linha 2 parâmetro                    │ y: 20–28
│ Linha 3 parâmetro                    │ y: 30–38
│ Linha 4 parâmetro                    │ y: 40–48
├──────────────────────────────────────┤
│ Footer: status / dica de tecla       │ y: 54–62
└─ 0,63 ──────────────────────── 127,63┘

Cursor (parâmetro selecionado): caractere ► na coluna 0
Parâmetro em edição: invertido (fundo branco, texto preto)
LED de atividade TX: pisca 50ms a cada frame enviado
```

---

## LEDs de Status

| LED | Cor | Comportamento |
|-----|-----|---------------|
| LED_CAN | Verde | Aceso = protocolo CAN ativo; apagado = K-Line ativo |
| LED_KLINE | Amarelo | Aceso = protocolo K-Line ativo |
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
