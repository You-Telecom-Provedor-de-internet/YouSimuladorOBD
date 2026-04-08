# RevA Carrier para ESP32 DevKit

## Decisao congelada da revisao

A revisao `RevA` da PCB do `YouSimuladorOBD` fica oficialmente congelada como:

- `carrier board` para `ESP32 DevKit 38 pinos`
- sem `ESP32-WROOM` soldado diretamente nesta revisao
- mantendo a topologia eletrica ja validada em bancada

Motivacao:

- reutilizar exatamente o modulo MCU que ja foi validado em firmware e bancada
- reduzir risco da primeira rodada de PCB
- facilitar rework e substituicao do MCU em caso de falha
- acelerar a passagem de protoboard para placa real

## Estado atual do programa

O prototipo fisico ja foi montado e validado em bancada com os barramentos alvo da `RevA`.

Status consolidado:

- `CAN`: validado
- `ISO 9141-2`: validado
- `KWP 5-baud`: validado
- `KWP Fast`: validado

Com isso, a `RevA` deixa de ser apenas uma hipotese de handoff e passa a ser a base imediata para a primeira `PCB` orientada a produto.

## Estrategia de integracao da RevA

### MCU

- `MCU1` = `ESP32 DevKit 38 pinos`
- montagem por `2 x headers femea 1x19` com passo `2.54 mm`
- a carrier deve aceitar um `DevKit` comercial de `38 pinos`

Importante:

- antes de fechar `Gerber`, o engenheiro deve confirmar o desenho mecanico do `DevKit` comercial escolhido
- o pinout canonico de bancada e PCB continua documentado em [Pinout Canonico](pinout.md)
- para fechar o esquematico final da `RevA`, usar como referencia primaria [Mapa Final de GPIO da RevA](mapa-gpio-reva.md)

### UI local

Na `RevA`, a estrategia de baixo risco fica:

- `DISP1` como modulo `OLED SH1107 128x128` em header
- `ENC1` como modulo `KY-040` em header
- `SW2-SW7` e `SW1` podem ficar diretamente na carrier
- `LED_TX` pode ficar diretamente na carrier como diagnostico opcional
- `LED_CAN` e `LED_KLINE` deixam de ser obrigatorios no fechamento final

### Fonte e barramentos

Na `RevA`, manter a topologia ja validada:

- entrada `OBD +12V` protegida
- buck `12V -> 5V`
- `SN65HVD230` para CAN
- `L9637D` para K-Line

## Regras mecanicas da carrier

### DevKit

Usar como referencia inicial:

- `2 x 19` pinos
- passo vertical `2.54 mm`
- espacamento nominal entre fileiras `25.40 mm` centro a centro
- keepout nominal do modulo `58 x 31 mm`

Esses valores sao referencia de engenharia da `RevA`. A liberacao final da placa deve confirmar o desenho do `DevKit` real escolhido.

### Keepout obrigatorio da antena

Reservar uma area sem cobre nem componentes na regiao da antena do modulo:

- keepout nominal: `19 x 16 mm`
- sem `GND pour` abaixo da antena
- sem trilhas de alta comutacao, buck ou transceivers nessa zona

### Acesso USB

O `USB` do `DevKit` deve ficar acessivel pela lateral da case ou pela borda da placa.

Recomendacao:

- alinhar o conector USB do `DevKit` a uma borda livre da carrier
- manter folga mecanica para cabo de programacao

## Modulos e conectores recomendados na RevA

| RefDes | Item | Estrategia RevA |
|---|---|---|
| `JMCU1` | header esquerdo do DevKit | `1x19 femea 2.54 mm` |
| `JMCU2` | header direito do DevKit | `1x19 femea 2.54 mm` |
| `JDISP1` | OLED SH1107 | `1x4 header 2.54 mm` |
| `JENC1` | KY-040 | `1x5 header 2.54 mm` |
| `J1` | OBD-II | conector ou chicote na borda |
| `MH1-MH4` | fixacao | furos `M3` |

## Regras eletricas congeladas

### CAN

- `GPIO4` -> `SN65HVD230 TXD`
- `GPIO5` <- `SN65HVD230 RXD`

### K-Line

- `GPIO17` -> `L9637D pin 4`
- `GPIO16` <- `L9637D pin 1`
- `RK1 510R` entre `K` e `VS`
- `RLI1 10k` entre `LI` e `VS`
- `CK1` e `CK2` de `100nF`

### UI

- `GPIO21/22` -> `OLED`
- `GPIO14/13/19` -> `KY-040`
- `GPIO32/33/25/26/27/18` -> botoes
- `GPIO34/35/36` -> DIP
- `GPIO23` -> `LED_TX` opcional
- `GPIO12` e `GPIO15` ficam fora da UI por risco de boot

## Placement de referencia

O placement inicial de referencia para a carrier fica em:

- [hardware/kicad/revA/esp32-devkit38-carrier-reference.csv](../hardware/kicad/revA/esp32-devkit38-carrier-reference.csv)
- [hardware/kicad/revA/ui-panel-reference.csv](../hardware/kicad/revA/ui-panel-reference.csv)

Esses arquivos nao congelam o roteamento final, mas congelam:

- envelope da placa
- keepout do `ESP32 DevKit`
- keepout da antena
- regiao de acesso USB
- regiao do `OLED`
- regiao do `KY-040`
- referencia de alinhamento da UI local

## O que a RevA nao vai fazer

Nesta revisao, nao fazer:

- `ESP32-WROOM` integrado na mesma PCB
- conector `L-Line`
- Bluetooth embarcado
- miniaturizacao agressiva

O objetivo aqui e uma `carrier robusta, de baixo risco e fiel a bancada validada`.
