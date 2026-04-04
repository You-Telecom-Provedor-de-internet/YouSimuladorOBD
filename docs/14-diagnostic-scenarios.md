# 14 - Cenarios Diagnosticos Compostos

## Objetivo

Esta camada evolui o `YouSimuladorOBD` de um gerador basico de PIDs para um gerador de cenarios diagnosticos compostos, progressivos e plausiveis.

O `dynamic_engine` continua sendo o responsavel pela fisica base do veiculo em movimento. A nova camada diagnostica roda no mesmo tick de `100 ms`, depois do estado base ser calculado e antes de o mutex ser liberado.

## Arquitetura

- `dynamic_engine.cpp`
  Aplica a fisica base para `SIM_STATIC`, `SIM_IDLE`, `SIM_URBAN`, `SIM_HIGHWAY`, `SIM_WARMUP` e `SIM_FAULT`.
- `diagnostic_scenario_engine.cpp`
  Sobrepoe falhas compostas sobre o estado base, sem reescrever o motor existente.
- `fault_catalog.cpp`
  Mantem o catalogo fixo de falhas, severidade, DTC, compartimento e causa raiz.
- `alert_engine.cpp`
  Constroi alertas, define alerta principal, calcula `health_score` local e ativa `limp_mode` quando necessario.

Regras da implementacao:

- sem nova task FreeRTOS
- sem alocacao dinamica no loop de 10 Hz
- sem `String` no loop critico
- arrays fixos e enums compactos
- serializacao rica apenas sob demanda

## Extensoes de Estado

Os novos campos foram adicionados apenas no final de `SimulationState`:

- `scenario_id`
- `health_score`
- `limp_mode`
- `active_fault_count`
- `alert_count`
- `manual_dtc_count`
- `manual_dtcs[8]`
- `active_faults[6]`
- `alerts[6]`

Helpers relevantes:

- `diagnostic_reset_fields(SimulationState&)`
- `diagnostic_reset_runtime()`
- `diagnostic_engine_rebuild_effective_dtcs(SimulationState&)`
- `diagnostic_engine_handle_mode04_clear(SimulationState&)`

## Modelo de DTC

O firmware continua expondo `dtc_count` e `dtcs[8]` como visao final efetiva para compatibilidade com CAN, K-Line, Bluetooth, UI e Torque/ELM.

Regra:

- `dtcs[] final = dtcs de cenario + manual_dtcs`
- deduplicacao por codigo
- priorizacao por severidade
- limite de 8 entradas

`Mode 04` agora:

- limpa `manual_dtcs`
- reseta o latch dos DTCs de cenario
- preserva a causa raiz
- permite que o DTC reapareca depois de nova qualificacao, se a falha persistir

## Cenarios Implementados

| ID | Base | Causa raiz | Efeitos principais | DTCs | Alertas |
|---|---|---|---|---|---|
| `urban_misfire_progressive` | `SIM_URBAN` | falha de ignicao progressiva | RPM irregular, perda de torque, `STFT/LTFT` positivos, irregularidade em carga parcial | `P0300`, `P0301`, `P0304` | motor / ignicao |
| `urban_overheat_fan_degraded` | `SIM_URBAN` | ventoinha degradada | coolant e oil sobem em baixa velocidade, alivio parcial em rodovia, `limp_mode` em limiar critico | `P0217` | arrefecimento critico |
| `idle_lean_air_leak` | `SIM_IDLE` | entrada falsa de ar | marcha lenta cacando, `MAP` alterado, `STFT/LTFT` altos, mistura pobre coerente | `P0171` | admissao / combustivel |
| `highway_catalyst_efficiency_drop` | `SIM_HIGHWAY` | eficiencia reduzida do catalisador | trims moderados, emissoes degradadas, pouca perda de dirigibilidade | `P0420` | emissoes / escape |
| `unstable_voltage_multi_module` | qualquer modo | tensao instavel de carga | tensao oscilando, jitter limitado em sensores secundarios, impacto em modulos | `P0562` quando persistente | bateria/carga e eletronica |
| `wheel_speed_abs_inconsistent` | `SIM_URBAN` ou `SIM_HIGHWAY` | inconsistencia de roda ABS | anomalia interna contextual para integracao futura | sem DTC OBD em `Mode 03` | freios / ABS |

## Progressao Temporal

Todos os cenarios usam o mesmo ciclo de vida:

- `onset`
- `aggravating`
- `intermittent`
- `persistent`
- `recovering`

Regras:

- alerta pode nascer antes do DTC
- DTC so entra quando a falha cruza o estagio minimo de qualificacao do catalogo
- a intensidade influencia severidade e `health_score`
- nao ha randomizacao solta; as variacoes sao deterministicas e coerentes com a causa raiz

## Health Score Local

O firmware calcula um score auxiliar embarcado:

- base `100`
- penalidade por falha ativa:
  `base_penalty * stage_factor * (intensity_pct / 100.0)`

Fatores:

- `onset = 0.25`
- `aggravating = 0.55`
- `intermittent = 0.70`
- `persistent = 1.0`
- `recovering = 0.35`

Faixa final:

- clamp `5..100`

Este score e auxiliar. O score oficial do produto continua pertencendo ao backend do ecossistema You.

## Endpoints e WebSocket

Payload leve:

- `GET /api/status`
- broadcast WebSocket

Campos adicionais leves:

- `scenario_id`
- `health_score`
- `limp_mode`
- `active_faults_count`
- `alert_count`
- `primary_alert`

Endpoints ricos:

- `GET /api/scenarios`
- `POST /api/scenario`
- `GET /api/diagnostics`

Comando via WebSocket:

```json
{"type":"scenario","id":"urban_misfire_progressive"}
```

Limpar:

```json
{"type":"scenario","id":""}
```

## Reflexo em OBD Classico

A camada diagnostica agora alimenta tambem o fluxo OBD classico, e nao apenas o payload rico local:

- `Mode 02` usa o snapshot de `freeze_frame` capturado quando o primeiro DTC efetivo qualifica
- `Mode 04` limpa o snapshot e permite recaptura quando a falha reaparece
- `Mode 06` expoe monitores sinteticos coerentes com os cenarios ativos

Regra atual de armazenamento:

- `freeze_frame` = snapshot ativo usado pelo `Mode 02` classico
- `freeze_frames[]` = historico local dos snapshots capturados, separado por DTC e por origem
- o protocolo OBD classico continua com apenas `1` frame ativo nesta tranche
- o historico separado fica disponivel em `GET /api/diagnostics`

Monitores principais desta tranche:

- `TID 01 / CID 01` catalyst
- `TID 03 / CID 01` O2 sensor B1S1
- `TID 21 / CID 01` O2 heater B1S1
- `TID 41 / CID 01` misfire cyl 1
- `TID 44 / CID 01` misfire cyl 4
- `TID 81 / CID 01` fuel system bank 1

Validacao de aceite feita em `2026-04-04`:

- `Freeze Frame (Mode 02)` validado no app `YouAutoCar` com `OBDLink MX+`
- estado vazio tratado como ausencia de snapshot, sem exibir `42 02 00 00` bruto
- estado com snapshot renderizado com DTC associado e parametros parseados
- decoder do app corrigido para reconstruir o DTC do `Mode 02` no formato OBD correto
- `Monitor Tests (Mode 06)` renderizado no `Centro OBD` com monitores e status coerentes

## Payload Rico de Integracao Futura

Exemplo de retorno de `GET /api/diagnostics`:

```json
{
  "scenario_id": "urban_misfire_progressive",
  "drive_context": "urban",
  "health_score": 41,
  "limp_mode": false,
  "vehicle": {
    "vin": "9YSMULMS0T1234567",
    "profile_id": "hb20_16_manual",
    "protocol": "CAN 11b 500k",
    "protocol_id": 0
  },
  "sensors": {
    "rpm": 912,
    "speed": 18,
    "coolant_temp": 94,
    "intake_temp": 31,
    "maf": 3.8,
    "map": 42,
    "throttle": 16,
    "engine_load": 30,
    "battery_voltage": 13.8,
    "oil_temp": 90,
    "stft": 18.5,
    "ltft": 9.2
  },
  "dtcs": ["P0300", "P0301", "P0304"],
  "active_faults": [
    {
      "id": "misfire_multi",
      "label": "Falha de ignicao progressiva",
      "dtc": "P0300",
      "system": "ignicao",
      "compartment": "powertrain",
      "severity": "high",
      "stage": "persistent",
      "intensity_pct": 82
    }
  ],
  "alerts": [
    {
      "type": "performance_loss",
      "label": "Perda de desempenho",
      "severity": "high",
      "system": "motor",
      "compartment": "powertrain",
      "fault_id": "misfire_multi",
      "primary": true
    }
  ],
  "anomalies": [
    {
      "metric": "fuel_trim_positive",
      "severity": "high",
      "description": "STFT/LTFT positivos e perda de torque sob carga parcial"
    }
  ],
  "probable_root_cause": {
    "id": "misfire_multi",
    "label": "Falha de ignicao progressiva",
    "system": "ignicao",
    "compartment": "powertrain"
  },
  "derived_faults": ["misfire_cyl_1", "misfire_cyl_4"],
  "freeze_frame": {
    "fault_id": "misfire_multi",
    "dtc": "P0300",
    "health_score": 41,
    "sensors": {
      "rpm": 912,
      "speed": 18,
      "throttle": 16,
      "coolant_temp": 94,
      "intake_temp": 31,
      "oil_temp": 90,
      "maf": 3.8,
      "map": 42,
      "battery_voltage": 13.8,
      "stft": 18.5,
      "ltft": 9.2
    }
  },
  "freeze_frames": [
    {
      "active": true,
      "fault_id": "misfire_multi",
      "scenario_id": "urban_misfire_progressive",
      "source": "scenario",
      "sequence": 1,
      "dtc": "P0300"
    }
  ],
  "mode06_like": {
    "failing_groups": [
      {
        "group": "misfire_multi_monitor",
        "context": "urban",
        "causes": [
          {
            "percentage": 78,
            "cause": "Falha de ignicao progressiva"
          }
        ]
      }
    ]
  }
}
```

## Mapeamento Futuro com `YouAutoCarvAPP2`

### `analisar-dtc`

Pode consumir:

- `dtcs`
- `vehicle`
- `freeze_frame`
- `freeze_frames`
- `mode06_like`
- `drive_context`

### `compute-vehicle-health`

Pode consumir:

- `alerts`
- `anomalies`
- severidade
- `active_faults`
- `health_score` local como sinal auxiliar

### `generate_mechanic_copilot_response`

Pode consumir:

- `active_faults`
- `probable_root_cause`
- `derived_faults`
- `alerts`
- `freeze_frame`
- `freeze_frames`

### `ai-diagnostic-copilot`

Pode consumir:

- `vehicle`
- `sensors`
- `drive_context`
- `alerts`
- `anomalies`
- `dtcs`

## Observacoes de Memoria

Medicoes desta tranche:

- `sizeof(ActiveFault) = 8`
- `sizeof(DiagnosticAlert) = 6`
- `sizeof(SimulationState)` antes: `104`
- `sizeof(SimulationState)` depois: `220`

Comparacao das tranches mais recentes:

- camada diagnostica composta:
  - `text`: `1651600 -> 1668492`
  - `data`: `306824 -> 312712`
  - `bss`: `36249 -> 36249`
- camada de odometria + reducao final:
  - `text`: `1668492 -> 1647404`
  - `data`: `312712 -> 292824`
  - `bss`: `36249 -> 36201`

Impacto:

- a camada diagnostica aumentou o firmware
- a rodada de simplificacao OTA e debug recuperou folga relevante de flash e RAM

Tamanho final observado:

- `flash usada`: `1917945 / 1966080`
- `firmware.bin`: `1924528 bytes`

Observacao operacional:

- durante a rodada de aceite houve um travamento real da placa porque um binario intermediario passou do slot OTA
- a recuperacao foi feita reduzindo apenas strings humanas do catalogo e das descricoes
- a logica dos cenarios foi preservada
- a validacao final foi feita na build estavel acima

## Aceite de Bancada em 2026-04-03

Validado no ESP32 real:

- `GET /api/scenarios`, `POST /api/scenario` e `GET /api/diagnostics`
- ativacao de cenario por WebSocket
- `urban_misfire_progressive`
- `urban_overheat_fan_degraded`
- `idle_lean_air_leak`
- `highway_catalyst_efficiency_drop`
- `unstable_voltage_multi_module`
- `wheel_speed_abs_inconsistent`
- limpeza e requalificacao de DTC
- limpeza de cenario por preset
- preservacao de `SIM_FAULT` legado

Validado com scanner real:

- leitura de PIDs via `OBDLink MX+ + Torque`
- odometro total persistente exposto na UI e na API local
- `PID 21`, `PID 31` e `PID A6` adicionados na mesma trilha de aceite

Observacao honesta desta rodada:

- a camada diagnostica foi validada na placa real por API e WebSocket
- `Mode 03`, `Mode 04` e `Mode 09` ja tinham sido validados anteriormente em bancada CAN/Torque
- nesta ultima rodada de aceite, o scanner externo confirmou novamente os PIDs vivos com `OBDLink MX+`
- a OTA local por arquivo foi removida depois disso para recuperar folga no slot OTA
- o fluxo final de campo ficou online-only por manifest

## Observacao sobre OLED e `memcmp`

O OLED usa comparacao binaria do estado:

```cpp
memcmp(&snap, &prev_snap, sizeof(SimulationState))
```

Nao houve indicio estrutural novo de regressao por causa disso, porque:

- os novos campos ficaram somente no final de `SimulationState`
- os novos tipos sao PODs compactos
- os novos campos sao resetados e atualizados de forma deterministica

Observacao honesta:

- nesta rodada nao foi feito um teste instrumental especifico de flicker do OLED
- a conclusao aqui e estrutural e baseada na bancada geral, nao em medicao dedicada de redraw
