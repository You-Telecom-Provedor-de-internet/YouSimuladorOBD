# 09 — Perfis de Veículo

## Visão Geral

O YouSimuladorOBD suporta **perfis de veículo** — conjuntos de parâmetros OBD pré-configurados
que simulam o comportamento real de um veículo específico em marcha lenta.

Cada perfil define:
- Marca, modelo e ano
- Protocolo OBD padrão do veículo
- 12 parâmetros realistas (RPM, temperatura, MAF, MAP, TPS, etc.)
- VIN com WMI (World Manufacturer Identifier) correto

---

## Estrutura de um Perfil (`VehicleProfile`)

```cpp
struct VehicleProfile {
    const char* id;           // "fiat_uno_10"
    const char* brand;        // "Fiat"
    const char* model;        // "Uno 1.0 Fire"
    const char* year;         // "2004-2013"
    uint8_t  protocol;        // PROTO_CAN_11B_500K, PROTO_ISO9141, etc.
    uint16_t rpm;             // RPM em marcha lenta
    uint8_t  speed_kmh;       // sempre 0 (parado)
    int16_t  coolant_temp_c;  // temperatura do motor aquecido
    int16_t  intake_temp_c;   // temperatura do ar de admissão
    float    maf_gs;          // fluxo de ar (g/s)
    uint8_t  map_kpa;         // pressão do coletor
    uint8_t  throttle_pct;    // posição do acelerador
    float    ignition_adv;    // avanço de ignição (graus)
    uint8_t  engine_load_pct; // carga do motor (%)
    uint8_t  fuel_level_pct;  // nível de combustível (%)
    const char* vin;          // VIN de 17 caracteres
};
```

---

## Catálogo de Perfis (22 veículos)

Total atual da revisao: `24` perfis, incluindo `peugeot_308_16thp` e `kia_carens_20`.

### Fiat

| ID | Modelo | Ano | Protocolo | RPM | Coolant |
|----|--------|-----|-----------|-----|---------|
| `fiat_uno_10` | Uno 1.0 Fire | 2004-2013 | ISO 9141-2 | 850 | 88°C |
| `fiat_palio_14` | Palio 1.4 Evo | 2013-2017 | CAN 11b 500k | 800 | 90°C |
| `fiat_strada_14` | Strada 1.4 | 2014-2020 | CAN 11b 500k | 820 | 89°C |
| `fiat_argo_13` | Argo 1.3 | 2017-2024 | CAN 11b 500k | 780 | 91°C |
| `fiat_cronos_13` | Cronos 1.3 | 2018-2024 | CAN 11b 500k | 760 | 90°C |

> **Nota:** O Uno 1.0 Fire (pré-2013) usa ISO 9141-2 pois é anterior à obrigatoriedade do CAN no Brasil.

### Volkswagen

| ID | Modelo | Ano | Protocolo | RPM | Coolant |
|----|--------|-----|-----------|-----|---------|
| `vw_gol_10` | Gol 1.0 MPI | 2012-2019 | CAN 11b 500k | 700 | 87°C |
| `vw_polo_16` | Polo 1.6 MSI | 2018-2022 | CAN 11b 500k | 750 | 91°C |
| `vw_virtus_16` | Virtus 1.6 MSI | 2018-2024 | CAN 11b 500k | 720 | 90°C |
| `vw_t_cross_15t` | T-Cross 1.4 TSI | 2019-2024 | CAN 11b 500k | 680 | 92°C |

### Chevrolet

| ID | Modelo | Ano | Protocolo | RPM | Coolant |
|----|--------|-----|-----------|-----|---------|
| `gm_celta_10` | Celta 1.0 VHC-E | 2006-2015 | KWP Fast | 900 | 86°C |
| `gm_onix_10t` | Onix 1.0 Turbo | 2020-2024 | CAN 11b 500k | 650 | 93°C |
| `gm_tracker_12t` | Tracker 1.2T | 2021-2024 | CAN 11b 500k | 680 | 94°C |

### Ford

| ID | Modelo | Ano | Protocolo | RPM | Coolant |
|----|--------|-----|-----------|-----|---------|
| `ford_ka_10` | Ka 1.0 SE Plus | 2015-2020 | CAN 11b 500k | 780 | 90°C |
| `ford_ecosport_15` | EcoSport 1.5 | 2013-2022 | CAN 11b 500k | 820 | 91°C |

### Toyota

| ID | Modelo | Ano | Protocolo | RPM | Coolant |
|----|--------|-----|-----------|-----|---------|
| `toyota_corolla_20` | Corolla 2.0 XEi | 2020-2024 | CAN 11b 500k | 650 | 93°C |
| `toyota_hilux_28` | Hilux 2.8 TDI | 2016-2024 | CAN 11b 500k | 700 | 88°C |

### Honda

| ID | Modelo | Ano | Protocolo | RPM | Coolant |
|----|--------|-----|-----------|-----|---------|
| `honda_civic_20` | Civic 2.0 EXL | 2017-2021 | CAN 11b 500k | 700 | 92°C |
| `honda_hrv_15` | HR-V 1.5 EX | 2015-2021 | CAN 11b 500k | 720 | 91°C |

### Hyundai

| ID | Modelo | Ano | Protocolo | RPM | Coolant |
|----|--------|-----|-----------|-----|---------|
| `hyundai_hb20_10` | HB20 1.0 Sense | 2020-2024 | CAN 11b 500k | 750 | 90°C |
| `hyundai_creta_16` | Creta 1.6 Pulse | 2017-2021 | CAN 11b 500k | 730 | 91°C |

### Kia

| ID | Modelo | Ano | Protocolo | RPM | Coolant |
|----|--------|-----|-----------|-----|---------|
| `kia_carens_20` | Carens EX 2.0L | 2008 | CAN 11b 500k | 720 | 93Â°C |

### Peugeot

| ID | Modelo | Ano | Protocolo | RPM | Coolant |
|----|--------|-----|-----------|-----|---------|
| `peugeot_308_16thp` | 308 1.6 THP | 2013-2019 | CAN 11b 500k | 720 | 93 C |

### Renault

| ID | Modelo | Ano | Protocolo | RPM | Coolant |
|----|--------|-----|-----------|-----|---------|
| `renault_kwid_10` | Kwid 1.0 Zen | 2017-2024 | CAN 11b 500k | 850 | 88°C |
| `renault_sandero_16` | Sandero 1.6 Stepway | 2015-2020 | CAN 11b 500k | 800 | 89°C |

---

## API REST

### Listar todos os perfis
```
GET /api/profiles
```
Resposta:
```json
[
  {"id":"fiat_uno_10","brand":"Fiat","model":"Uno 1.0 Fire","year":"2004-2013","protocol":4,"turbo":false},
  {"id":"fiat_palio_14","brand":"Fiat","model":"Palio 1.4 Evo","year":"2013-2017","protocol":0,"turbo":false},
  ...
]
```

Perfis turbo agora saem explicitamente com `\"turbo\": true` na listagem da API, por exemplo nos modelos `TSI`, `Turbo`, `THP` e `TDI`.

### Aplicar um perfil
```
POST /api/profile
Content-Type: application/json

{"id": "fiat_uno_10"}
```
Resposta:
```json
{"ok":true,"brand":"Fiat","model":"Uno 1.0 Fire"}
```

### Via WebSocket
```json
{"type": "profile", "id": "fiat_uno_10"}
```

---

## VIN — World Manufacturer Identifier (WMI)

| WMI | Fabricante |
|-----|-----------|
| `ZFA` | Fiat (Itália / Brasil) |
| `9BW` | Volkswagen do Brasil |
| `9BG` | General Motors do Brasil |
| `9BF` | Ford do Brasil |
| `9BR` | Toyota do Brasil |
| `2HG` | Honda Canada |
| `9BH` | Hyundai |
| `VF1` | Renault (França) |

---

## Adicionando Novos Perfis

Editar `firmware/include/vehicle_profiles.h` e adicionar uma entrada no array `VEHICLE_PROFILES[]`:

```cpp
{ "meu_carro_20",
  "Marca", "Modelo X 2.0", "2020-2024",
  PROTO_CAN_11B_500K,
  750,   // rpm idle
  0,     // speed
  91,    // coolant °C
  30,    // intake °C
  4.5f,  // maf g/s
  38,    // map kPa
  14,    // tps %
  14.5f, // ignition advance °
  22,    // engine load %
  70,    // fuel level %
  "WMI000000A0000000" },  // VIN 17 chars
```

Após editar, recompilar e reflashear:
```bash
cd firmware
pio run -t upload -t uploadfs
```
