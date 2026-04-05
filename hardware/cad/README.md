# CAD e Impressao 3D

Este diretorio concentra os primeiros ativos mecanicos do projeto.

## Primeira peca teste

`bench_case_revA_base`

Objetivo:

- servir como primeira peca de validacao na `Creality K1`
- testar tolerancias, rigidez e volume util para a futura placa do `YouSimuladorOBD`
- oferecer uma base aberta tipo `bench tray`, mais segura para a fase atual do hardware do que um gabinete fechado definitivo

Arquivos gerados:

- `freecad/bench_case_revA_base.FCStd`
- `freecad/bench_case_revA_base.step`
- `freecad/bench_case_revA_base.stl`
- `renders/bench_case_revA_base.png`

## Segunda peca teste

`bench_case_revA_lid_ui`

Objetivo:

- criar a tampa inicial para a bandeja `revA`
- aproximar o frontal do hardware real da UI local
- testar leitura do `OLED SH1107 128x128` atraves de uma janela quadrada
- testar a disposicao real de botoes dedicados e do encoder rotativo

Arquivos gerados:

- `freecad/bench_case_revA_lid_ui.FCStd`
- `freecad/bench_case_revA_lid_ui.step`
- `freecad/bench_case_revA_lid_ui.stl`
- `renders/bench_case_revA_lid_ui.png`

Caracteristicas:

- tampa tipo `cap` com labio interno de encaixe
- janela quadrada central para `OLED SH1107 128x128`
- rebaixo frontal ao redor do display
- fileira de 6 botoes: `PREV`, `NEXT`, `UP`, `DOWN`, `OK`, `PROTO`
- furo separado para o encoder rotativo `KY-040`
- geometria ainda parametrica, mas agora alinhada com a UI implementada no firmware

## Compatibilidade com o projeto real

O handoff eletrico da PCB ja inclui o display e o encoder:

- `hardware/pcb-handoff/bom-rev-a.csv`
  - `DISP1 = OLED SH1107 128x128`
  - `ENC1 = Rotary encoder KY-040`
- `hardware/pcb-handoff/netlist-rev-a.csv`
  - `ENC_A`, `ENC_B`, `ENC_SW`
  - `I2C_SDA`, `I2C_SCL`

Ou seja, esta tampa passa a refletir melhor o que a placa real devera acomodar.

## Assuncoes desta revisao

Como a PCB final ainda esta em captura, esta primeira peca usa um envelope mecanico conservador:

- area util de placa: `120 x 85 mm`
- folga interna lateral: `3 mm`
- espessura da base: `3 mm`
- espessura de parede: `3 mm`
- altura de parede: `22 mm`
- standoffs internos para parafuso `M3`

Isso torna a peca util como:

- bandeja de bancada para a eletronica atual
- base de teste de impressao
- ponto de partida para o gabinete final da PCB

## Ferramentas usadas

- `FreeCAD 1.1`
- `Blender 5.1` para preview
