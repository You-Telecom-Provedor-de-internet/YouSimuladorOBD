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

