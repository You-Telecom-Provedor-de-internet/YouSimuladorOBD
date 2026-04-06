# Material Canonico de Esquematico

Esta pasta agora e o ponto canonico dos artefatos textuais de esquematico do projeto.

## Use esta pasta para

- `esquematico-conceitual.md`
  - esquematico textual por blocos da `RevA`
- `netlist-humana.md`
  - conectividade humana consolidada para engenharia
- `bom-preliminar.csv`
  - lista preliminar de componentes para captura e layout

## Pasta `legacy/`

O conteudo em `legacy/` e historico e nao deve ser usado como fonte principal para a PCB atual.

Itens movidos para `legacy/`:

- `YouSimuladorOBD.json`
- `YouSimuladorOBD.svg`

## Fontes principais a usar junto com esta pasta

- `docs/hardware.md`
- `docs/pinout.md`
- `docs/engenharia-pcb.md`
- `hardware/pcb-handoff/`
- `hardware/kicad/revA/`

## Regra pratica

Se houver divergencia entre material historico e a consolidacao atual:

1. priorizar firmware atual para pinagem e dependencias logicas
2. priorizar `docs/hardware.md` e `docs/pinout.md` para documentacao canonica
3. usar `hardware/pcb-handoff/` e `hardware/kicad/revA/` como baseline de handoff e EDA
