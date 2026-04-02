# 11 — Bluetooth SPP (Emulador ELM327)

## Visao Geral

O ESP32 expoe um servico Bluetooth Classic SPP (Serial Port Profile) que emula um adaptador ELM327 v1.5. Apps como Torque Pro, OBD Auto Doctor e Car Scanner podem conectar via Bluetooth e ler dados do simulador como se fosse um carro real.

O Bluetooth roda **em paralelo** com o protocolo fisico (CAN/K-Line) e a interface Web. Todos compartilham o mesmo `SimulationState` via mutex FreeRTOS.

```
 Celular (Torque Pro)          Scanner fisico         Navegador
      |                              |                     |
  Bluetooth SPP                OBD-II Conector         Wi-Fi HTTP
      |                              |                     |
 ┌────v─────┐                ┌───────v──────┐      ┌──────v──────┐
 │ ELM327   │                │ CAN / K-Line │      │ WebServer   │
 │ Parser   │                │  Protocol    │      │ + WebSocket │
 └────┬─────┘                └───────┬──────┘      └──────┬──────┘
      |                              |                     |
      └──────────────┬───────────────┘─────────────────────┘
                     v
             ┌───────────────┐
             │ SimulationState│
             │   (mutex)      │
             └───────────────┘
```

---

## Conexao

| Parametro | Valor |
|-----------|-------|
| Nome Bluetooth | `OBD-Simulator` |
| PIN | `1234` |
| Perfil | SPP (Serial Port Profile) |
| Protocolo | Texto ASCII (AT commands + hex OBD) |

### Passos para conectar com Torque Pro

1. No celular: **Configuracoes → Bluetooth → Parear novo dispositivo**
2. Selecionar **"OBD-Simulator"**, PIN **1234**
3. Abrir **Torque Pro → Configuracoes → Conexao OBD**
4. Selecionar **Bluetooth → OBD-Simulator**
5. Voltar e verificar que Torque mostra dados (RPM, velocidade, etc.)

---

## Comandos AT Suportados

O emulador implementa o subconjunto de comandos AT necessario para compatibilidade com os principais apps OBD:

### Comandos de Identificacao

| Comando | Resposta | Descricao |
|---------|----------|-----------|
| AT Z | `ELM327 v1.5` | Reset do dispositivo |
| AT I | `ELM327 v1.5` | Versao do firmware |
| AT @1 | `YouSimuladorOBD` | Descricao do dispositivo |
| AT RV | `14.2V` | Tensao da bateria (do simulador) |
| AT DPN | `6` | Numero do protocolo (CAN 500k) |
| AT DP | `ISO 15765-4 (CAN 11/500)` | Descricao do protocolo |

### Comandos de Configuracao

| Comando | Descricao | Default |
|---------|-----------|---------|
| AT E0 / E1 | Echo off / on | on |
| AT H0 / H1 | Headers off / on | off |
| AT L0 / L1 | Linefeeds off / on | on |
| AT S0 / S1 | Espacos off / on | on |
| AT SP h | Selecionar protocolo (aceita qualquer) | — |
| AT ST hh | Timeout (aceita, ignora) | — |
| AT AT 0/1/2 | Adaptive timing (aceita, ignora) | — |
| AT D0/D1 | DLC display (aceita, ignora) | — |
| AT BI | Bypass init (aceita, ignora) | — |

---

## Formato de Comunicacao OBD

### Requisicao (app → ESP32)

O app envia hex puro terminado com `\r`:

```
010C\r          → Mode 01, PID 0x0C (RPM)
0100\r          → Mode 01, PID 0x00 (PIDs suportados)
03\r            → Mode 03 (ler DTCs)
04\r            → Mode 04 (limpar DTCs)
0902\r          → Mode 09, PID 02 (VIN)
```

### Resposta (ESP32 → app)

Formato depende das configuracoes AT H/S/L:

**Com headers (H1) e espacos (S1) — padrao Torque:**
```
7E8 04 41 0C 0C 80\r\r>
```

**Sem headers (H0), sem espacos (S0):**
```
410C0C80\r\r>
```

**Estrutura do frame CAN (com headers):**
```
7E8  04    41   0C   0C 80
───  ──    ──   ──   ─────
 │    │     │    │     └── dados (RPM×4 = 0x0C80 = 3200 → 800 RPM)
 │    │     │    └──────── PID
 │    │     └───────────── modo resposta (0x41 = Mode 01 + 0x40)
 │    └─────────────────── N_PCI (4 bytes de dados)
 └──────────────────────── CAN Response ID (ECU 1)
```

---

## WebUI — Indicador Bluetooth

O header da WebUI mostra um badge "BT" que indica o estado da conexao Bluetooth:

- **Cinza**: Bluetooth ativo, sem cliente conectado
- **Azul**: Cliente Bluetooth conectado (Torque, etc.)

O estado `bt_connected` e transmitido via WebSocket no JSON de broadcast.

---

## Implementacao

### Arquivos

| Arquivo | Funcao |
|---------|--------|
| `firmware/src/bluetooth/elm327_bt.h` | Header: `elm327_bt_init()`, `elm327_bt_connected()` |
| `firmware/src/bluetooth/elm327_bt.cpp` | Parser AT + OBD hex, task FreeRTOS |

### Task FreeRTOS

| Parametro | Valor |
|-----------|-------|
| Nome | `elm327_bt` |
| Core | 1 |
| Prioridade | 2 |
| Stack | 8192 bytes |
| Frequencia | ~50 Hz (20ms delay no loop) |

### Fluxo interno

```
1. BluetoothSerial.begin("OBD-Simulator")
2. Loop:
   a. Se nao ha cliente → espera 200ms
   b. Le bytes ate \r
   c. Remove espacos, uppercase
   d. Se "AT..." → handle_at_command()
   e. Senao → parse hex → OBDRequest → lock mutex → dispatch → unlock
   f. Formata resposta hex com headers opcionais
   g. Envia + prompt ">"
```

### Dependencia

Usa a biblioteca `BluetoothSerial` nativa do arduino-esp32 (nenhuma dependencia externa).

---

## Impacto em Recursos

| Recurso | Antes | Com BT | Variacao |
|---------|-------|--------|----------|
| Flash | 984 KB (50.1%) | 1753 KB (89.2%) | +770 KB |
| RAM | 47 KB (14.5%) | 61 KB (18.6%) | +14 KB |

O aumento de flash e significativo (~770 KB) porque o stack Bluetooth Classic do ESP-IDF e grande. Ainda restam ~210 KB livres para futuras funcionalidades.

---

## Compatibilidade

| App | Plataforma | Status |
|-----|-----------|--------|
| Torque Pro | Android | Esperado funcionar |
| OBD Auto Doctor | Android/iOS | Esperado funcionar |
| Car Scanner | Android | Esperado funcionar |
| DashCommand | Android/iOS | Esperado funcionar |

> Nota: iOS usa BLE (Bluetooth Low Energy) e NAO suporta SPP Classic. Apps iOS que precisam de BLE requerem implementacao separada (futuro).
