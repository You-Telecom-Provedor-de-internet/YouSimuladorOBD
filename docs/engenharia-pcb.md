# Guia Executivo para Engenharia de PCB

Este documento orienta o engenheiro de hardware/PCB sobre quais arquivos usar, qual e o baseline real da `RevA` e o que ainda precisa ser confirmado antes do esquematico final.

## Escopo da RevA

A `RevA` desta rodada esta congelada como:

- `carrier board para ESP32 DevKit 38 pinos`
- `SN65HVD230` para CAN
- `L9637D` para K-Line
- `OLED SH1107` via header
- `KY-040` via header
- `LM2596` ou buck equivalente de baixo risco para `12V -> 5V`

## Fase atual do projeto

O objetivo desta fase ja nao e mais validar se a arquitetura funciona em bancada. Esse marco foi atingido com o prototipo fisico atual.

Estado consolidado:

- prototipo montado e testado fisicamente
- `CAN`, `ISO 9141-2`, `KWP 5-baud` e `KWP Fast` tratados como validados
- foco imediato deslocado para `PCB RevA` e preparacao para produto

Consequencia pratica para a engenharia:

- partir do principio de que a topologia central esta aceita
- concentrar esforco em layout, protecao, repetibilidade, teste e industrializacao leve

Fora de escopo nesta rodada:

- integrar `ESP32-WROOM` diretamente na PCB
- alterar pinagem do firmware
- alterar protocolos, barramentos ou logica OBD

## Baseline real a considerar

Use como baseline combinado:

1. firmware atual
   - `firmware/include/config.h`
   - `firmware/src/main.cpp`
   - `firmware/src/protocols/*.cpp`
   - `firmware/src/web/ui_init.cpp`
   - `firmware/src/web/web_server.cpp`

2. documentacao consolidada desta rodada
   - [hardware.md](hardware.md)
   - [pinout.md](pinout.md)
   - [arquitetura.md](arquitetura.md)
   - [../hardware/schematic/esquematico-conceitual.md](../hardware/schematic/esquematico-conceitual.md)
   - [../hardware/schematic/netlist-humana.md](../hardware/schematic/netlist-humana.md)
   - [../hardware/schematic/bom-preliminar.csv](../hardware/schematic/bom-preliminar.csv)
   - [../hardware/pcb/requisitos-layout.md](../hardware/pcb/requisitos-layout.md)

3. handoff e EDA existentes
   - [../hardware/pcb-handoff/README.md](../hardware/pcb-handoff/README.md)
   - [../hardware/pcb-handoff/netlist-rev-a.csv](../hardware/pcb-handoff/netlist-rev-a.csv)
   - [../hardware/pcb-handoff/bom-rev-a.csv](../hardware/pcb-handoff/bom-rev-a.csv)
   - [../hardware/kicad/revA/README.md](../hardware/kicad/revA/README.md)
   - [../hardware/kicad/revA/YouSimuladorOBD_RevA.kicad_sch](../hardware/kicad/revA/YouSimuladorOBD_RevA.kicad_sch)

## O que esta confirmado

### Confirmado no codigo

- mapa de GPIOs do projeto
- uso de `TWAI` nativo para CAN
- uso de `UART1` para K-Line a `10400 baud`
- uso de `OLED SH1107` via `I2C`
- uso de `6 botoes`, `encoder`, `DIP` e `3 LEDs`
- uso de `Wi-Fi`, `mDNS`, `LittleFS`, `Web UI` e `OTA`

### Confirmado na documentacao/EDA existente

- `SN65HVD230` como frontend CAN
- `L9637D` como frontend K-Line
- `LM2596` como baseline de buck
- mapeamento OBD `pin 6`, `7`, `14`, `16`, `4`, `5`
- `RK1 510R`, `RLI1 10k`, `CK1 100nF`, `CK2 100nF`
- `RevA` como `carrier para ESP32 DevKit 38 pinos`
- bootstrap `KiCad RevA` dividido em blocos

## O que ainda precisa ser confirmado antes do esquematico final

- modelo comercial exato do `ESP32 DevKit 38 pinos`
- footprint final do modulo `OLED SH1107`
- footprint final do modulo `KY-040`
- validacao final da TVS de entrada `SMCJ24A` contra o ambiente real de surto do produto
- escolha final dos MOSFETs externos do `LM74502-Q1`
- estrategia final de ESD no conector OBD com `PESD2CAN24LT-Q` no CAN e `ESDLIN1524BJ` na `K-Line`
- forma final do conector OBD na placa: borda, chicote ou solucao hibrida
- orcamento de corrente se o buck deixar de ser modulo pronto

## Fechamento recomendado de protecao para a RevA

Como recomendacao de engenharia para a primeira PCB robusta, usar:

- `F1 1A` na entrada `+12V_OBD`
- `TVS1 = SMCJ24A` unidirecional entre `+12V_PROT` e `GND`
- `D_ESD_CAN1 = PESD2CAN24LT-Q` para `CANH/CANL`
- `D_ESD_K1 = ESDLIN1524BJ` para `K-Line`
- `UREV1 = LM74502-Q1` com `QREV1/QREV2` externos para reversao de polaridade e chaveamento de entrada

Classificacao dessas escolhas:

- `F1 1A`: confirmado na documentacao/EDA existente
- demais itens acima: `inferido / hipotese fortemente recomendada`

Esses componentes entram como baseline proposto para a PCB final, mas nao devem ser descritos como "confirmados no codigo".

## Conflitos e divergencias que nao devem ser resolvidos silenciosamente

1. Narrativas antigas de UI local foram removidas desta rodada.
   - consequencia: para decisao de hardware usar a pinagem real do firmware, nao a narrativa antiga de menus

2. Narrativas antigas de selecao por serial USB foram removidas.
   - consequencia: tratar como historico; o fluxo real atual e `NVS + DIP + UI + web`

3. O firmware nao prova sozinho a existencia fisica de todos os componentes da carrier.
   - consequencia: para liberar esquematico final cruzar sempre firmware com `docs/diagrama-eletrico-pcb.md`, `docs/engenharia-pcb.md`, `docs/reva-carrier-esp32-devkit.md`, handoff CSV e `KiCad RevA`

4. Protecoes automotivas aparecem como recomendacao de engenharia, nao como implementacao fechada da bancada.
   - consequencia: a placa final deve incorporar essas protecoes, mas elas nao podem ser marcadas como "confirmadas no codigo"

## Sequencia recomendada para fechar o esquematico real

1. Use [hardware.md](hardware.md) e [pinout.md](pinout.md) como fonte canonica de requisitos.
2. Use [../hardware/schematic/esquematico-conceitual.md](../hardware/schematic/esquematico-conceitual.md) para estruturar as folhas do projeto.
3. Use [../hardware/schematic/netlist-humana.md](../hardware/schematic/netlist-humana.md) e [../hardware/schematic/bom-preliminar.csv](../hardware/schematic/bom-preliminar.csv) para capturar a conectividade e a lista de componentes.
4. Consulte [../hardware/pcb-handoff/](../hardware/pcb-handoff/) para comparar com o handoff congelado.
5. Continue a captura nativa a partir de [../hardware/kicad/revA/](../hardware/kicad/revA/).

## Observacao operacional importante

O firmware atual contem `SSID`, `hostname` e credenciais default hardcoded em `config.h`. Isso nao foi alterado nesta rodada. Tratar como risco operacional/documental, nao como decisao eletrica de PCB.
