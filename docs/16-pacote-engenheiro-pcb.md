# 16 - Pacote para o Engenheiro de PCB

## Envie estes arquivos

### Pacote eletrico principal

- [docs/15-diagrama-eletrico-pcb.md](15-diagrama-eletrico-pcb.md)
- [hardware/pcb-handoff/README.md](../hardware/pcb-handoff/README.md)
- [hardware/pcb-handoff/netlist-rev-a.csv](../hardware/pcb-handoff/netlist-rev-a.csv)
- [hardware/pcb-handoff/bom-rev-a.csv](../hardware/pcb-handoff/bom-rev-a.csv)

### Captura nativa inicial no KiCad

- [hardware/kicad/revA/README.md](../hardware/kicad/revA/README.md)
- [hardware/kicad/revA/YouSimuladorOBD_RevA.kicad_sch](../hardware/kicad/revA/YouSimuladorOBD_RevA.kicad_sch)
- [hardware/kicad/revA/power_input.kicad_sch](../hardware/kicad/revA/power_input.kicad_sch)
- [hardware/kicad/revA/esp32_core.kicad_sch](../hardware/kicad/revA/esp32_core.kicad_sch)
- [hardware/kicad/revA/obd_transceivers.kicad_sch](../hardware/kicad/revA/obd_transceivers.kicad_sch)
- [hardware/kicad/revA/ui_local.kicad_sch](../hardware/kicad/revA/ui_local.kicad_sch)
- [hardware/kicad/revA/esp32-devkit38-carrier-reference.csv](../hardware/kicad/revA/esp32-devkit38-carrier-reference.csv)
- [hardware/kicad/revA/erc-report.txt](../hardware/kicad/revA/erc-report.txt)

### Referencia mecanica da UI

- [hardware/kicad/revA/ui-panel-reference.csv](../hardware/kicad/revA/ui-panel-reference.csv)
- [hardware/cad/freecad/bench_case_revA_lid_ui.FCStd](../hardware/cad/freecad/bench_case_revA_lid_ui.FCStd)
- [hardware/cad/renders/bench_case_revA_lid_ui.png](../hardware/cad/renders/bench_case_revA_lid_ui.png)

### Decisao de arquitetura da RevA

- [docs/17-revA-carrier-esp32-devkit.md](17-revA-carrier-esp32-devkit.md)

## O que esse pacote ja define

- `RevA` congelada como `carrier para ESP32 DevKit 38 pinos`
- topologia validada de `CAN`, `ISO 9141-2`, `KWP 5-baud` e `KWP Fast`
- fonte de entrada `OBD +12V` com fusivel e buck
- `SN65HVD230` para CAN
- `L9637D` para K-Line
- `OLED SH1107 128x128`
- `KY-040`
- 6 botoes da UI local
- DIP de protocolo
- LEDs de status

## Marco atual do projeto

O projeto ja saiu da fase de bring-up puro de bancada.

Estado atual:

- prototipo fisico montado e testado
- barramentos `CAN` e `K-Line` operacionais
- `KWP 5-baud` agora tratado como validado

Direcao desta rodada:

- transformar a topologia validada em `PCB RevA`
- preparar a placa para repetibilidade de montagem e evolucao para produto

## O que ainda fica a cargo do engenheiro

- fechar a captura completa do esquema dentro do `KiCad`
- escolher o `DevKit 38 pinos` comercial exato e confirmar o footprint final dos headers
- definir footprint final do modulo OLED e do encoder
- fechar outline da PCB
- ajustar a mecanica final da tampa com base nas posicoes definitivas da placa
- revisar a placa com criterio de produto: protecao, teste, montagem, manutencao e robustez
