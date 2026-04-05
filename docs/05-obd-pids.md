# 05 - PIDs OBD-II e Formato de Dados

## Visao Geral

O `YouSimuladorOBD` responde PIDs genericos de `Mode 01`, DTCs em `Mode 03`, limpeza em `Mode 04` e VIN em `Mode 09`.

Nesta versao, o firmware tambem expoe odometria e contadores de servico:

- `PID 0x21`: distancia com MIL ativa
- `PID 0x31`: distancia desde o ultimo clear de DTC
- `PID 0xA6`: odometria total, quando o perfil suporta esse PID

## PIDs Suportados por Range

### PID 0x00 - PIDs suportados [01-20]

PIDs atuais neste range:

- `0x04` carga do motor
- `0x05` temperatura do motor
- `0x06` STFT
- `0x07` LTFT
- `0x0B` MAP
- `0x0C` RPM
- `0x0D` velocidade
- `0x0E` avanco de ignicao
- `0x0F` temperatura de admissao
- `0x10` MAF
- `0x11` TPS

### PID 0x20 - PIDs suportados [21-40]

PIDs atuais neste range:

- `0x21` distancia com MIL ativa
- `0x2F` nivel de combustivel
- `0x31` distancia desde clear

O firmware anuncia `PID 0x40` para o range seguinte.

### PID 0x40 - PIDs suportados [41-60]

PIDs atuais neste range:

- `0x42` tensao do modulo de controle
- `0x5C` temperatura do oleo

Se o perfil suportar `PID A6`, o firmware tambem anuncia `PID 0x60` para os ranges estendidos.

### PID 0x60, 0x80 e 0xA0

Quando `PID A6` esta habilitado para o perfil atual, o firmware anuncia os ranges estendidos em cascata:

- `0x60` anuncia `0x80`
- `0x80` anuncia `0xA0`
- `0xA0` anuncia `0xA6`

Quando o perfil nao suporta odometro por OBD generico, esses ranges nao sao anunciados.

## Tabela de PIDs Implementados

### PID 0x04 - Carga do motor

- Formula: `A * 100 / 255`
- Unidade: `%`
- Default: `25%`

### PID 0x05 - Temperatura do motor

- Formula: `A - 40`
- Unidade: `C`
- Default: `90 C`

### PID 0x06 - STFT banco 1

- Formula: `(A - 128) * 100 / 128`
- Unidade: `%`
- Default: `0.0%`

### PID 0x07 - LTFT banco 1

- Formula: `(A - 128) * 100 / 128`
- Unidade: `%`
- Default: `0.0%`

### PID 0x0B - MAP

- Formula: `A`
- Unidade: `kPa`
- Default: `35 kPa`
- Perfis turbo podem reportar acima de `100 kPa` absolutos, atÃ© cerca de `170 kPa`

### PID 0x0C - RPM

- Formula: `(A * 256 + B) / 4`
- Unidade: `rpm`
- Default: `800 rpm`

### PID 0x0D - Velocidade

- Formula: `A`
- Unidade: `km/h`
- Default: `0 km/h`

### PID 0x0E - Avanco de ignicao

- Formula: `A / 2 - 64`
- Unidade: `graus`
- Default: `10 graus`

### PID 0x0F - Temperatura de admissao

- Formula: `A - 40`
- Unidade: `C`
- Default: `30 C`

### PID 0x10 - MAF

- Formula: `(A * 256 + B) / 100`
- Unidade: `g/s`
- Default: `3.0 g/s`

### PID 0x11 - TPS

- Formula: `A * 100 / 255`
- Unidade: `%`
- Default: `15%`

### PID 0x21 - Distancia com MIL ativa

- Formula: `A * 256 + B`
- Unidade: `km`
- Fonte: `distance_mil_on_km`
- Semantica: acumula apenas enquanto ha DTC ativo

Exemplo:

```text
12 km com MIL ativa
Resposta: 04 41 21 00 0C
```

### PID 0x2F - Nivel de combustivel

- Formula: `A * 100 / 255`
- Unidade: `%`
- Default: `75%`

### PID 0x31 - Distancia desde clear de DTC

- Formula: `A * 256 + B`
- Unidade: `km`
- Fonte: `distance_since_clear_km`
- Semantica: zera em `Mode 04`

Exemplo:

```text
257 km desde o clear
Resposta: 04 41 31 01 01
```

### PID 0x42 - Tensao do modulo de controle

- Formula: `(A * 256 + B) / 1000`
- Unidade: `V`
- Default: `14.2 V`

### PID 0x5C - Temperatura do oleo

- Formula: `A - 40`
- Unidade: `C`
- Default: `85 C`

### PID 0xA6 - Odometria total

- Formula: inteiro de 32 bits em unidade de `0.1 km`
- Fonte: `odometer_total_km_x10`
- Persistencia: NVS
- Disponibilidade: apenas em perfis compativeis

Exemplo:

```text
1.9 km
raw = 19 = 0x00000013
Resposta: 06 41 A6 00 00 00 13
```

Observacoes:

- o simulador usa o `cluster/painel` como fonte principal do odometro
- `BCM`, `ABS` e `ECM` sao espelhos sincronizados expostos na API local
- `Mode 04` nunca zera o odometro total

## Mode 03 - DTCs

O simulador continua usando `dtc_count` e `dtcs[8]` como visao efetiva final.

Exemplo sem DTC:

```text
02 43 00
```

Exemplo com `P0300` e `P0171`:

```text
06 43 02 03 00 01 71
```

## Mode 04 - Limpar DTCs

Ao receber `Mode 04`, o firmware:

1. limpa DTCs ativos
2. limpa o latch dos DTCs de cenario
3. zera `distance_since_clear_km`
4. preserva `odometer_total_km_x10`

Resposta positiva:

```text
01 44
```

## Mode 09 - VIN

O simulador continua respondendo `Mode 09 PID 02` com VIN multi-frame.

VIN padrao:

```text
9YSMULMS0T1234567
```

## Resumo Rapido

| PID | Parametro | Unidade | Observacao |
|---|---|---|---|
| `0x04` | Carga do motor | `%` | generico |
| `0x05` | Temp. motor | `C` | generico |
| `0x06` | STFT | `%` | generico |
| `0x07` | LTFT | `%` | generico |
| `0x0B` | MAP | `kPa` | generico |
| `0x0C` | RPM | `rpm` | generico |
| `0x0D` | Velocidade | `km/h` | generico |
| `0x0E` | Avanco de ignicao | `graus` | generico |
| `0x0F` | Temp. admissao | `C` | generico |
| `0x10` | MAF | `g/s` | generico |
| `0x11` | TPS | `%` | generico |
| `0x21` | Dist. com MIL | `km` | cresce so com DTC ativo |
| `0x2F` | Combustivel | `%` | generico |
| `0x31` | Dist. desde clear | `km` | zera em `Mode 04` |
| `0x42` | Tensao | `V` | generico |
| `0x5C` | Temp. oleo | `C` | generico |
| `0xA6` | Odometria total | `0.1 km` | depende do perfil |
