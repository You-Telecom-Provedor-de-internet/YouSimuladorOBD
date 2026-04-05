# 10 — Modo de Simulacao Dinamica

## Visao Geral

O simulador opera em dois modelos:

1. **STATIC (Manual)** — valores fixos, ajustados pelo usuario via botoes ou WebUI
2. **Dinamico** — motor de fisica atualiza os 16 parametros a cada 100 ms, simulando comportamento real de um motor 1.6L 4 cilindros NA com cambio de 5 marchas

Perfis turbo (`TSI`, `Turbo`, `THP`, `TDI`) agora adicionam spool/boost, MAP acima de 100 kPa, MAF mais alto e retardo de ignicao sob carga.

O modo e selecionado via WebUI (painel "Modo de Simulacao") ou WebSocket.

```
┌─────────────────────────────────────────────────────┐
│              Modos de Simulacao                      │
├──────────┬──────────────────────────────────────────┤
│ STATIC   │ Valores manuais (botoes / sliders)       │
│ IDLE     │ Marcha lenta aquecido                    │
│ URBAN    │ Stop & go urbano 0-60 km/h              │
│ HIGHWAY  │ Cruzeiro 110 km/h com ultrapassagens     │
│ WARMUP   │ Cold start: 20°C → 90°C em ~3 min       │
│ FAULT    │ Falha de ignicao P0300 + P0301 + P0304   │
└──────────┴──────────────────────────────────────────┘
```

---

## Motor de Referencia

Todos os valores foram calibrados a partir de dados reais de motores 4 cilindros 1.6L aspirados naturalmente:

| Parametro | Motor de referencia |
|-----------|---------------------|
| Cilindros | 4 em linha |
| Cilindrada | 1.6L (1598 cc) |
| Aspiracao | Natural (NA) |
| Cambio | 5 marchas manuais |
| Relacao final | ~3.94 |

Acima esta o motor base. Perfis turbo reaproveitam essa base e acrescentam camada de boost, aquecimento de admissao e retardo de ignicao conforme carga.

### Relacao RPM por km/h em cada marcha

| Marcha | RPM/km/h | Faixa de uso |
|--------|----------|--------------|
| 1a | 105 | 0-15 km/h |
| 2a | 62 | 15-35 km/h |
| 3a | 42 | 35-60 km/h |
| 4a | 30 | 60-85 km/h |
| 5a | 22 | 85+ km/h |

---

## Valores Calibrados por Modo

### IDLE — Marcha Lenta (motor aquecido, parado)

| Parametro | Valor tipico | Variacao |
|-----------|-------------|----------|
| RPM | 800 | ±15 (senoide lenta ~16s) |
| Velocidade | 0 km/h | — |
| Temp. refrigerante | 92°C | ±0.2°C (termostatado) |
| Temp. oleo | 87°C | ±0.15°C |
| Temp. admissao | 31°C | — |
| MAF | 4.0 g/s | ±0.3 |
| MAP | 29 kPa | ±0.5 |
| TPS | 3% | ±0.5 |
| Carga motor | 18% | ±1.5 |
| Avanco ignicao | 11° | — |
| STFT | 0% | ±0.6 (oscilacao do O2) |
| LTFT | 0% | estavel |
| Bateria | 14.1 V | ±0.06 |
| Combustivel | diminui ~0.03%/min | — |

### URBAN — Transito urbano (stop & go)

Maquina de 4 estados com transicoes temporais:

```
┌──────────┐  semaforo  ┌──────────┐  atingiu  ┌──────────┐  sinal   ┌──────────┐
│ PARADO   ├───────────►│ ACELERAC ├──────────►│ CRUZEIRO ├────────►│ FRENAGEM │
│ 3-8s     │  verde     │ ~6s      │  vel.     │ 8-25s    │  fecha  │ ~4s      │
└─────▲────┘            └──────────┘           └──────────┘         └────┬─────┘
      └──────────────────────────────────────────────────────────────────┘
                              volta a 0 km/h
```

| Fase | RPM | Velocidade | TPS | Carga | MAF |
|------|-----|-----------|-----|-------|-----|
| Parado | 780-820 | 0 | 3% | 18% | 4 g/s |
| Aceleracao | 800-3500 | 0→50 | 18-45% | 45-70% | 5-25 g/s |
| Cruzeiro | 1400-2200 | 30-55 | 12-20% | 28-44% | 10-16 g/s |
| Frenagem | 2200→800 | 50→0 | 0% | 8% | 3-10 g/s |

Taxas: aceleracao ~0.8 km/h por 100ms (0-50 em ~6s), frenagem ~1.2 km/h por 100ms (50-0 em ~4s).

### HIGHWAY — Rodovia (cruzeiro + ultrapassagens)

| Fase | RPM | Velocidade | TPS | Carga | Duracao |
|------|-----|-----------|-----|-------|---------|
| Cruzeiro | 2200 (5a) | 110 km/h | 22% | 38% | 60-120s |
| Ultrapassagem | 2600-2900 | 125-135 | 55% | 65% | 8-15s |
| Retorno | 2400→2200 | 130→110 | 22% | 38% | 5-10s |

STFT levemente negativo em cruzeiro (-1.2%) indica operacao lean para economia.

### WARMUP — Arranque a frio

Curva de aquecimento comprimida para ~3 minutos (real: 5-10 min), mantendo o formato realista em 4 fases:

```
Temp (°C)
  95 ┤                                        ┄┄┄┄┄  Fase 4: regulado
  90 ┤                                   ╱───────     pelo termostato
  80 ┤                             ╱─────             Fase 3: radiador
  70 ┤                        ╱───╱                   ativo (~0.17°C/s)
  60 ┤                   ╱───╱
  55 ┤               ╱──╱                             Fase 2: termostato
  50 ┤           ╱──╱                                 abrindo (~0.42°C/s)
  40 ┤       ╱──╱
  30 ┤   ╱──╱                                        Fase 1: termostato
  20 ┤──╱                                            fechado (~0.58°C/s)
     └──┬──────┬──────┬──────┬──────┬──────┬──►
        0     30     60     90    120    150   tempo (s)
```

| Fase | Coolant | Taxa | Oil | RPM | STFT |
|------|---------|------|-----|-----|------|
| 1 (0-60s) | 20→55°C | 0.58°C/s | 20→30°C | 1100→950 | +12% (open loop) |
| 2 (60-120s) | 55→80°C | 0.42°C/s | 30→50°C | 950→850 | +8%→+4% |
| 3 (120-180s) | 80→90°C | 0.17°C/s | 50→70°C | 850→800 | +2%→0% |
| 4 (180s+) | 90-93°C | regulado | 70→87°C | 800 | ±2% (closed loop) |

Comportamento do oil: lags coolant em ~12°C, sobe a ~65% da taxa do coolant.

STFT alto (+12%) nos primeiros 20s representa open loop (sensor O2 nao pronto), depois diminui gradualmente ate normalizar em closed loop.

### FAULT — Falha de Ignicao

DTCs ativados **imediatamente** ao entrar no modo:

| DTC | Codigo OBD | Descricao |
|-----|-----------|-----------|
| P0300 | 0x0300 | Falha de ignicao — multiplos cilindros |
| P0301 | 0x0301 | Falha de ignicao — cilindro 1 |
| P0304 | 0x0304 | Falha de ignicao — cilindro 4 |

Comportamento ciclico entre falha intensa e falha leve:

```
     Falha intensa (15-30s)      Falha leve (10-20s)
    ┌───────────────────────┐   ┌─────────────────┐
    │ RPM: ±200-400 jitter  │   │ RPM: ±50 jitter │
    │ STFT: +15-18%         │   │ STFT: +8%       │
    │ Quedas ate -400 RPM   │   │ Mais estavel    │
    │ Carga +8% (inefic.)   │   │ Carga normal    │
    └───────────┬───────────┘   └────────┬────────┘
                └──────────►────────────►┘  (DTCs sempre presentes)
```

| Parametro | Falha intensa | Falha leve | Normal (ref.) |
|-----------|--------------|------------|---------------|
| RPM | 780 ±200-400 | 780 ±50 | 780 ±15 |
| STFT | +15 a +18% | +8% | ±2% |
| LTFT | sobe ate +8% | +6% | ±5% |
| Carga | +8% adicional | normal | 18% |
| Bateria | 13.8 V | 14.0 V | 14.1 V |

15% de chance a cada tick de queda abrupta de RPM (-200 a -400) simulando misfire intermitente.

---

## Modelo de Ruido

Todos os modos usam um sistema de ruido suave em vez de valores aleatorios por tick (que causariam flickering):

### Wander (Random Walk com Reversao a Media)

```cpp
struct Wander {
    float v = 0;
    void step(float pull, float amp) {
        v = v * (1.0 - pull) + random(-amp, amp);
    }
};
```

| Canal | pull | amp | Resultado |
|-------|------|-----|-----------|
| RPM | 0.08 | 6.0 | ±15 RPM, oscilacao lenta |
| TPS | 0.15 | 0.4 | ±0.7% |
| Carga | 0.10 | 0.8 | ±1.8% |
| STFT | 0.06 | 0.2 | ±0.6% (oscilacao do O2) |
| Coolant | 0.25 | 0.15 | ±0.2°C |
| Oil | 0.25 | 0.10 | ±0.15°C |
| Bateria | 0.12 | 0.03 | ±0.06 V |

### EMA (Exponential Moving Average)

Todas as transicoes de parametros usam suavizacao EMA:

```
valor = valor + alpha × (alvo - valor)
```

Alpha baixo (~0.02-0.04) para transicoes lentas (temperatura), alpha alto (~0.10-0.15) para respostas rapidas (TPS durante aceleracao).

---

## Formulas de Fisica

### MAF (Mass Air Flow)

```
MAF(g/s) = 1.8 + (RPM / 1000) × (Load% / 100) × 13.0
```

| Condicao | RPM | Load | MAF calculado | Real tipico |
|----------|-----|------|---------------|-------------|
| Idle | 800 | 18% | 3.7 g/s | 3-5 g/s |
| Cruzeiro 60 | 2500 | 35% | 13.2 g/s | 12-16 g/s |
| Cruzeiro 110 | 2200 | 38% | 12.7 g/s | 15-22 g/s |
| Aceleracao | 3500 | 65% | 31.5 g/s | 25-40 g/s |

### MAP (Manifold Absolute Pressure)

```
MAP(kPa) = 27 + TPS% × 0.71
```

Nos perfis turbo, a base acima recebe `boost_kPa` dinÃ¢mico e pode subir atÃ© aproximadamente `170 kPa` absolutos em carga alta.

| TPS | MAP calculado | Real tipico |
|-----|---------------|-------------|
| 3% (idle) | 29 kPa | 25-35 kPa |
| 22% (cruise) | 43 kPa | 40-55 kPa |
| 50% (accel) | 63 kPa | 55-80 kPa |
| 100% (WOT) | 98 kPa | 90-105 kPa |

### Avanco de Ignicao

```
base = 10 + (RPM - 800) / 400
retardo = Load% × 0.06
ignicao(°) = base - retardo    [clamped 5°-32°]
```

Mais avanco em baixa carga (eficiencia), retarda sob carga alta (previne detonacao).

### Temperatura de Admissao (IAT)

```
IAT(°C) = 25 + (coolant - 20) × 0.07 + Load% × 0.06
```

Calor do motor (heat soak) + calor da compressao no corpo de borboleta.

---

## API

### REST — POST /api/mode

```json
// Request
{ "mode": 2 }   // 0=STATIC, 1=IDLE, 2=URBAN, 3=HIGHWAY, 4=WARMUP, 5=FAULT

// Response
{ "ok": true }
```

### WebSocket — Tipo "mode"

```json
// Envio (cliente → ESP32)
{ "type": "mode", "id": 2 }

// No broadcast de estado (ESP32 → clientes)
{ "sim_mode": 2, "rpm": 2400, ... }
```

### OLED

O header do display mostra o modo ativo:

```
┌─────────────────────┐
│ CAN 11b 500k  IDLE  │   ← modo indicado no canto
│ ─────────────────── │
│ ...                  │
```

Abreviacoes: `STAT`, `IDLE`, `URB`, `HWY`, `WARM`, `FAIL`

---

## Banco de DTCs OBD-II

A WebUI inclui um banco de 140 DTCs com descricoes em portugues, organizados em 10 categorias:

| Categoria | Quantidade | Descricao |
|-----------|-----------|-----------|
| P01xx | 39 | Sensores de combustivel e ar (MAF, MAP, O2, TPS, temp.) |
| P02xx | 8 | Injecao de combustivel (injetores, bomba) |
| P03xx | 21 | Ignicao e falhas de ignicao (misfire, CKP, CMP, bobinas) |
| P04xx | 17 | Emissoes (EGR, EVAP, catalisador) |
| P05xx | 15 | Velocidade, marcha lenta e sistema eletrico |
| P06xx | 8 | Computador / ECU (memoria, processador, comunicacao) |
| P07xx | 13 | Transmissao automatica (solenoides, relacoes) |
| Bxxxx | 6 | Carroceria (airbag, BCM, transponder) |
| Cxxxx | 7 | Chassi e ABS (sensores de roda, modulo ABS) |
| Uxxxx | 6 | Rede de comunicacao (barramento CAN, modulos) |

### Codificacao OBD-II de DTCs

Cada DTC ocupa 2 bytes. Os bits 15-14 codificam o sistema:

```
Bits 15-14 do uint16:
  00 = P (Powertrain)    → 0x0000-0x3FFF
  01 = C (Chassis)       → 0x4000-0x7FFF
  10 = B (Body)          → 0x8000-0xBFFF
  11 = U (Network)       → 0xC000-0xFFFF

Exemplos:
  P0300 → 0x0300
  B0001 → 0x8001
  C0035 → 0x4035
  U0100 → 0xC100
```

### Funcionalidades da UI

- Painel expansivel com busca em tempo real por codigo ou descricao
- Agrupamento por categoria quando navegando, lista plana ao buscar
- Botao "+" com feedback visual (check verde por 1.2s)
- Bloqueio automatico ao atingir limite de 8 DTCs ativos
- Validacao aceita prefixos P, C, B e U

---

## Implementacao (Arquivos)

| Arquivo | Funcao |
|---------|--------|
| `firmware/include/simulation_state.h` | Enum `SimMode` (6 modos) + campo `sim_mode` |
| `firmware/include/dynamic_engine.h` | Declaracao `dynamic_init()` |
| `firmware/src/simulation/dynamic_engine.cpp` | Motor de fisica completo (~480 linhas) |
| `firmware/src/main.cpp` | Chama `dynamic_init()` na inicializacao |
| `firmware/src/web/web_server.cpp` | `POST /api/mode`, WS type "mode", `sim_mode` no JSON |
| `firmware/src/web/ui_init.cpp` | Modo curto no header OLED |
| `firmware/data/index.html` | Painel de modo + banco de 140 DTCs |

### Task FreeRTOS

```
Task: dyn_engine
Core: 1
Prioridade: 1 (baixa)
Stack: 4096 bytes
Periodo: 100 ms (10 Hz)
```

Quando `sim_mode == SIM_STATIC`, a task nao altera o estado (passthrough para controle manual).
