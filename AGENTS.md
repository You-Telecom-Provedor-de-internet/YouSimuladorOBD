# AGENTS.md

## Escopo

Este arquivo existe para reduzir drift operacional quando o `cwd` estiver em `C:\www\YouSimuladorOBD`.

Fontes principais deste repo:

- `README.md`
- `docs/`
- `firmware/README.md`
- `hardware/README.md`

## Regras minimas

- Trabalhar em portugues do Brasil.
- Tratar `README.md` e a documentacao de `docs/` como ponto de entrada antes de editar firmware, hardware ou OTA.
- Se o trabalho cruzar app Android, web, OTA publica ou plugin, usar coordenacao explicita entre repos.
- Se o usuario mencionar `@you-obd-lab`, preferir o workflow real multi-agente com `you-orchestrator` antes de especialistas.

## Fechamento natural da rodada

Antes de declarar conclusao, verificar:

1. Houve mudanca que exige `git commit` e `git push` neste repo?
2. Houve mudanca que tambem exige sincronizacao em `C:\www\YouAutoCarvAPP2`?
3. Houve alteracao de versao, `manifest`, bins ou OTA publica?

Se o escopo autorizado exigir publicacao completa:

- nao fechar a rodada com firmware local pronto e rollout externo pendente sem explicitar isso
- sincronizar os repos afetados de forma coerente quando houver autorizacao do Owner
- listar claramente o que ficou local, o que foi publicado e o que ainda depende de deploy/reflash

## OTA e rollout

Quando a mudanca tocar versao, `manifest`, firmware, `LittleFS` ou URLs publicas:

- validar firmware e filesystem
- validar `manifest`
- se o pacote publico morar em outro repo, nao tratar este repo isoladamente
- descrever o estado final de bancada, OTA publica e hardware real separadamente

## Cross-repo

Se a rodada envolver:

- `C:\www\YouAutoCarvAPP2`
- `C:\www\you-obd-lab-plugin`

congelar ownership, contratos e handoffs antes de editar para nao deixar drift entre simulador, app e laboratorio.
