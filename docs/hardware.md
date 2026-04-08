# Hardware Consolidado

Documento canonico de consolidacao eletrica do `YouSimuladorOBD`.

Objetivo:

- reunir em um unico lugar o que o firmware realmente exige do hardware
- separar o que esta confirmado no codigo do que vem de documentacao e EDA existente
- registrar explicitamente o que ainda e inferido ou depende de confirmacao antes do esquematico final

Fontes principais usadas nesta consolidacao:

- `firmware/include/config.h`
- `firmware/src/main.cpp`
- `firmware/src/protocols/can_protocol.cpp`
- `firmware/src/protocols/kline_protocol.cpp`
- `firmware/src/protocols/protocol_init.cpp`
- `firmware/src/web/ui_init.cpp`
- `firmware/src/web/web_server.cpp`
- `firmware/platformio.ini`
- `docs/pinout.md`
- `docs/kline-l9637d-montagem.md`
- `docs/diagrama-eletrico-pcb.md`
- `docs/engenharia-pcb.md`
- `docs/reva-carrier-esp32-devkit.md`
- `docs/mapa-gpio-reva.md`
- `hardware/pcb-handoff/bom-rev-a.csv`
- `hardware/pcb-handoff/netlist-rev-a.csv`
- `hardware/kicad/revA/README.md`
- `hardware/kicad/revA/erc-report.txt`
- `hardware/kicad/revA/*.kicad_sch`
- `wokwi/diagram.json`

## Visao geral funcional da placa

O `YouSimuladorOBD` e uma placa de bancada para emular uma ECU OBD-II usando um `ESP32 DevKit` 38 pinos como MCU principal. A placa precisa responder a scanners reais nos barramentos:

- `CAN` usando o controlador `TWAI` nativo do ESP32 e um transceiver `SN65HVD230`
- `K-Line` usando `UART1` do ESP32 e um transceiver `L9637D`

Tambem precisa oferecer:

- interface local com `OLED SH1107 128x128`
- `6 botoes`
- `encoder KY-040`
- `DIP switch` de 3 bits para selecao rapida de protocolo
- `LED_TX` externo opcional e o LED onboard do DevKit
- `Wi-Fi`, `Web UI`, `WebSocket`, `API REST`, `mDNS` e `OTA` pela rede

Arquitetura alvo desta rodada:

- `RevA` congelada como `carrier board para ESP32 DevKit 38 pinos`
- sem migracao para `ESP32-WROOM` integrado nesta rodada

## Blocos funcionais

| Bloco | Descricao | Classificacao dominante |
|---|---|---|
| MCU | `ESP32 DevKit` 38 pinos com FreeRTOS/Arduino, TWAI, UART1, I2C, Wi-Fi e LittleFS | confirmado no codigo + confirmado na documentacao/EDA existente |
| Entrada automotiva | `OBD-II J1962` com `+12V`, `GND`, `CANH`, `CANL`, `K-Line` | confirmado na documentacao/EDA existente |
| Fonte primaria | `+12V OBD -> fusivel -> buck 12V para 5V` | confirmado na documentacao/EDA existente |
| Fonte logica | `5V` para `VIN` do DevKit e `+3V3_AUX` dedicado para perifericos | confirmado na documentacao/EDA existente |
| CAN | `SN65HVD230` em `+3V3_AUX` ligado ao TWAI nativo do ESP32 | confirmado no codigo + confirmado na documentacao/EDA existente |
| K-Line | `L9637D` com `VS` em `+12V`, logica em `+3V3_AUX`, `K` no OBD e bias passivo dedicado | confirmado no codigo + confirmado na documentacao/EDA existente |
| UI local | `OLED SH1107`, `KY-040`, `6 botoes`, `DIP`, `LED_TX` opcional | confirmado no codigo + confirmado na documentacao/EDA existente |
| Rede e web | `Wi-Fi AP/STA`, `mDNS`, `ESPAsyncWebServer`, `LittleFS`, `OTA` | confirmado no codigo |
| EDA baseline | handoff em CSV e bootstrap em `KiCad RevA` com `0` erros e `0` warnings de ERC | confirmado na documentacao/EDA existente |

## Entradas

| Entrada | Origem | Observacao | Classificacao |
|---|---|---|---|
| `+12V_OBD` | `J1 pin 16` | alimenta o buck e o `VS` do `L9637D` | confirmado na documentacao/EDA existente |
| `GND` | `J1 pin 4` e `J1 pin 5` | referencia comum do sistema | confirmado na documentacao/EDA existente |
| `CANH/CANL` | `J1 pin 6` e `J1 pin 14` | trafego OBD sobre CAN | confirmado na documentacao/EDA existente |
| `K-Line` | `J1 pin 7` | trafego OBD em `ISO 9141-2`, `KWP 5-baud` e `KWP Fast` | confirmado no codigo + confirmado na documentacao/EDA existente |
| botoes locais | `GPIO32`, `33`, `25`, `26`, `27`, `18` | entradas com `INPUT_PULLUP`, botoes para `GND` | confirmado no codigo |
| encoder | `GPIO14`, `13`, `19` | `A`, `B` e `SW` | confirmado no codigo |
| DIP de protocolo | `GPIO34`, `35`, `36` | somente entrada; exigem pull-up externo para `+3V3_AUX` | confirmado no codigo + confirmado na documentacao/EDA existente |
| USB do DevKit | conector do modulo comercial | usado para gravacao e serial debug | confirmado na documentacao/EDA existente |

## Saidas

| Saida | Destino | Observacao | Classificacao |
|---|---|---|---|
| resposta OBD em CAN | scanner no `J1 pin 6/14` | via `SN65HVD230` | confirmado no codigo + confirmado na documentacao/EDA existente |
| resposta OBD em K-Line | scanner no `J1 pin 7` | via `L9637D` | confirmado no codigo + confirmado na documentacao/EDA existente |
| display local | modulo `OLED SH1107` | I2C em `GPIO21/22` | confirmado no codigo + confirmado na documentacao/EDA existente |
| LED de status externo | `GPIO23` | `LED_TX` opcional para trafego | confirmado no codigo + confirmado na documentacao/EDA existente |
| LED heartbeat | `GPIO2` | LED onboard do DevKit, nao detalhado na carrier | confirmado no codigo |
| Wi-Fi/web | AP/STA com web UI, API e OTA | interface sem fio do ESP32 | confirmado no codigo |

## Interfaces externas

| Interface | Implementacao | Status |
|---|---|---|
| `OBD-II J1962` | `J1` com pinos `4/5/6/7/14/16` usados | confirmado na documentacao/EDA existente |
| `CAN` | `TWAI` do ESP32 + `SN65HVD230` | confirmado no codigo + confirmado na documentacao/EDA existente |
| `K-Line` | `UART1` do ESP32 + `L9637D` | confirmado no codigo + confirmado na documentacao/EDA existente |
| `I2C` | `GPIO21 SDA` e `GPIO22 SCL` para o OLED | confirmado no codigo |
| `USB serial/programacao` | USB do `ESP32 DevKit` | confirmado na documentacao/EDA existente |
| `Wi-Fi 2.4 GHz` | AP/STA do ESP32 | confirmado no codigo |
| `mDNS` | hostname configuravel, default `youobd.local` | confirmado no codigo |
| `BLE/Bluetooth embarcado` | nao usado nesta revisao | confirmado no codigo por ausencia de uso ativo no build e no firmware atual |

## Alimentacao

### Topologia consolidada

```text
J1 pin 16 (+12V OBD)
  -> F1
  -> +12V_PROT
  -> U1 buck 12V -> 5V
  -> U3 L9637D pin 7 (VS)
  -> RK1 510R para a linha K
  -> RLI1 10k para o pino LI do L9637D

J1 pin 4 / pin 5 (GND)
  -> GND comum

U1 OUT+
  -> +5V_SYS
  -> VIN / 5V do ESP32 DevKit
  -> U4 regulador 3.3V dedicado

U4 OUT
  -> +3V3_AUX
  -> U2 SN65HVD230
  -> U3 L9637D VCC logico
  -> OLED SH1107
  -> ENC1 KY-040
  -> pull-ups do DIP
```

### Observacoes

- O firmware depende implicitamente de o `ESP32 DevKit` receber `5V` em `VIN` e do rail `+3V3_AUX` alimentar os perifericos de forma estavel.
- `+3V3_AUX` deve permanecer separado do pino `3V3` de saida do `ESP32 DevKit`.
- O codigo nao define nenhuma medicao de tensao de entrada nem supervisao analogica da fonte. Logo, a robustez da entrada `+12V` fica a cargo da eletrica da placa.
- A corrente total do sistema nao esta formalizada em documento eletrico fechado; isso ainda precisa ser confirmado antes do esquematico final se houver mudanca de buck ou integracao futura do MCU.

## Protecao eletrica

| Item | Estado | Origem | Observacao |
|---|---|---|---|
| `F1` fusivel de entrada `1A` | confirmado como baseline de handoff | docs + `hardware/pcb-handoff` + KiCad bootstrap | nao ha part number final congelado |
| TVS na entrada `+12V` | recomendado e registrado | `docs/diagrama-eletrico-pcb.md` e `docs/engenharia-pcb.md` | nao confirmado como implementado na bancada base |
| protecao contra inversao de polaridade | recomendada | `docs/diagrama-eletrico-pcb.md` | topologia final nao fechada |
| ESD no conector OBD | nao encontrado como item fechado | inferido / hipotese | recomendavel para PCB automotiva, principalmente nas linhas externas |
| terminacao CAN `120R` por jumper ou DNP | recomendada | docs + `hardware/pcb-handoff/bom-rev-a.csv` | nao ha prova no codigo; no breakout de bancada a terminacao pode ja existir |
| desacoplamento local do `L9637D` | confirmado | docs + `hardware/pcb-handoff` + KiCad bootstrap | `CK1` e `CK2` de `100nF` obrigatorios |
| desacoplamento local do `SN65HVD230` | confirmado como baseline de handoff | `hardware/pcb-handoff/bom-rev-a.csv` | `CCAN1 100nF` |

### Fechamento recomendado de protecao para a RevA

Os itens abaixo nao estao confirmados no codigo. Eles sao recomendacoes de engenharia para transformar a `RevA` em uma PCB mais robusta e mais objetiva para compra e montagem.

| Funcao | Componente recomendado | Classificacao | Observacao pratica |
|---|---|---|---|
| fusivel de entrada | `F1 1A` | confirmado na documentacao/EDA existente + recomendacao de fechamento | manter como primeira barreira da linha `+12V_OBD` |
| TVS da entrada `+12V` | `SMCJ24A` unidirecional | inferido / hipotese fortemente recomendada | encapsulamento `DO-214AB`; fica antes do buck e do `VS` do `L9637D` |
| ESD para `CANH/CANL` | `PESD2CAN24LT-Q` | inferido / hipotese fortemente recomendada | componente automotivo de 2 linhas; colocar junto de `J1` |
| ESD/surge para `K-Line` | `ESDLIN1524BJ` | inferido / hipotese fortemente recomendada | otimizado para barramentos tipo `LIN/CXPI`; serve bem como referencia para a linha `K` |
| reversao de polaridade / load disconnect | `LM74502-Q1` + `2 x MOSFET N` back-to-back | inferido / hipotese fortemente recomendada | solucao de baixa perda para uso automotivo; os MOSFETs ainda precisam ser fechados pela corrente alvo |

Observacoes:

- Para prototipo rapido de bancada, um diodo Schottky em serie pode funcionar como protecao simples de reversao, mas nao e a recomendacao principal para a PCB final por perda termica e queda de tensao.
- As protecoes acima devem ser tratadas como adicao recomendada sobre o baseline atual de bancada/handoff, nao como itens ja comprovados pelo firmware.

## Componentes provaveis

| RefDes | Componente provavel | Papel | Classificacao |
|---|---|---|---|
| `MCU1` | `ESP32 DevKit 38p` | MCU principal | confirmado na documentacao/EDA existente |
| `U1` | modulo `LM2596` ou buck equivalente | `12V -> 5V` | confirmado na documentacao/EDA existente |
| `U4` | regulador `3.3V` dedicado | gera `+3V3_AUX` para perifericos | confirmado na documentacao/EDA existente |
| `U2` | `SN65HVD230` | transceiver CAN 3V3 | confirmado na documentacao/EDA existente |
| `U3` | `L9637D` | transceiver K-Line | confirmado na documentacao/EDA existente |
| `TVS1` | `SMCJ24A` | protecao de surto na entrada `+12V` | inferido / hipotese fortemente recomendada |
| `D_ESD_CAN1` | `PESD2CAN24LT-Q` | protecao ESD de `CANH/CANL` | inferido / hipotese fortemente recomendada |
| `D_ESD_K1` | `ESDLIN1524BJ` | protecao ESD/surge da `K-Line` | inferido / hipotese fortemente recomendada |
| `UREV1` | `LM74502-Q1` + MOSFETs externos | protecao de reversao de polaridade e chaveamento de entrada | inferido / hipotese fortemente recomendada |
| `J1` | conector `OBD-II` 16 pinos | interface automotiva externa | confirmado na documentacao/EDA existente |
| `DISP1` | modulo `OLED SH1107 128x128` | interface local | confirmado na documentacao/EDA existente |
| `ENC1` | `KY-040` | encoder da UI | confirmado na documentacao/EDA existente |
| `SW1` | DIP switch 3 posicoes | boot fallback de protocolo | confirmado na documentacao/EDA existente |
| `SW2-SW7` | botoes momentaneos | UI local | confirmado na documentacao/EDA existente |
| `D3` | LED externo opcional | `LED_TX` | confirmado na documentacao/EDA existente |
| `JMCU1/JMCU2` | headers femea `1x19` | carrier do DevKit | confirmado na documentacao/EDA existente |
| `JDISP1` | header `1x4` | modulo OLED | confirmado na documentacao/EDA existente |
| `JENC1` | header `1x5` | modulo encoder | confirmado na documentacao/EDA existente |

## Dependencias eletricas implicitas

Estas dependencias nao dependem de preferencia de software; elas sao impostas pelo hardware ou pelo proprio firmware atual.

1. `GPIO34`, `GPIO35` e `GPIO36` sao somente entrada e sem `pull-up` interno.
   - efeito: o `DIP` precisa de `pull-up` externo para `+3V3_AUX`
   - base: `config.h`, `main.cpp`, `docs/pinout.md`, `hardware/pcb-handoff/netlist-rev-a.csv`

2. A linha `K-Line` precisa de nivel de idle alto estavel em `+12V`.
   - efeito: `RK1 510R` entre `K` e `VS` e `RLI1 10k` entre `LI` e `VS` deixam de ser opcionais para reproduzir a bancada validada
   - base: `docs/kline-l9637d-montagem.md`, `docs/diagrama-eletrico-pcb.md`, `hardware/pcb-handoff`

3. O `L9637D` precisa de dois dominios de alimentacao.
   - `VCC` logico em `+3V3_AUX`
   - `VS` automotivo em `+12V`
   - base: `docs/kline-l9637d-montagem.md`, `docs/diagrama-eletrico-pcb.md`, `hardware/pcb-handoff/netlist-rev-a.csv`

4. O `SN65HVD230` precisa operar em `+3V3_AUX` e ficar com `RS` em `GND` para o modo rapido registrado no handoff.
   - base: `docs/pinout.md`, `hardware/pcb-handoff/netlist-rev-a.csv`

5. A regiao de antena do `ESP32 DevKit` exige `keepout` de cobre e componentes.
   - base: `docs/reva-carrier-esp32-devkit.md`

6. O `OLED` depende de barramento `I2C` em `400 kHz` configurado no firmware e de `+3V3_AUX` bem desacoplado.
   - base: `ui_init.cpp`
   - implicacao: manter trilhas curtas e return path limpo para evitar instabilidade visual

7. O buck e o resistor `RK1` precisam de cuidado termico.
   - base: `docs/diagrama-eletrico-pcb.md`
   - implicacao: placement e cobre devem considerar dissipacao, especialmente para `RK1`

## Itens confirmados no codigo

- `PIN_CAN_TX = GPIO4` e `PIN_CAN_RX = GPIO5`
- `PIN_KLINE_TX = GPIO17` e `PIN_KLINE_RX = GPIO16`
- `K-Line` usa `UART1` a `10400 baud`
- `TWAI` do ESP32 e usado para os protocolos CAN
- `OLED` usa `GPIO21 SDA`, `GPIO22 SCL`, endereco `0x3C`
- `6 botoes` usam `GPIO32`, `33`, `25`, `26`, `27`, `18` com `INPUT_PULLUP`
- `encoder` usa `GPIO14`, `13`, `19`
- `DIP` usa `GPIO34`, `35`, `36`
- `LED_TX` externo usa `GPIO23`
- o `LED` onboard usa `GPIO2`
- `Wi-Fi`, `mDNS`, `LittleFS`, `ESPAsyncWebServer`, `OTA` e autenticacao web/API estao ativos
- nao ha uso ativo de `BLE` ou `Bluetooth Classic` no firmware atual desta revisao

## Itens confirmados na documentacao/EDA existente

- `RevA` congelada como `carrier para ESP32 DevKit 38 pinos`
- `SN65HVD230` como transceiver CAN
- `L9637D` como transceiver K-Line
- `LM2596` como baseline de buck `12V -> 5V`
- regulador `3.3V` dedicado como fonte de `+3V3_AUX` para perifericos
- conector `OBD-II` com uso dos pinos `4`, `5`, `6`, `7`, `14`, `16`
- `RK1 510R`, `RLI1 10k`, `CK1 100nF`, `CK2 100nF` como conjunto obrigatorio do `L9637D`
- `RCAN1 120R` como terminacao CAN opcional por jumper ou `DNP`
- `OLED SH1107 128x128` via header
- `KY-040` via header
- `2 x 1x19` como carrier do DevKit
- bootstrap `KiCad RevA` com folhas `Power_Input`, `ESP32_Core`, `OBD_Transceivers` e `UI_Local`
- `ERC report` com `0` erros e `0` warnings

## Itens inferidos / hipotese

- TVS exata de entrada, encapsulamento e potencia ainda nao estao congelados
- a familia escolhida para a TVS de entrada foi fechada como recomendacao em `SMCJ24A`, mas a validacao final contra o ambiente real do produto ainda precisa ser feita
- a topologia recomendada para reversao foi fechada em `LM74502-Q1` com MOSFETs externos, mas os MOSFETs e os resistores de apoio ainda precisam ser definidos pela corrente alvo
- o part number exato do `ESP32 DevKit 38 pinos` comercial ainda precisa ser confirmado antes de liberar footprint final
- o part number exato do regulador dedicado de `+3V3_AUX` ainda precisa ser congelado
- footprint final do `OLED SH1107` e do `KY-040` depende do modulo real escolhido para a RevA
- o ponto exato de amarracao entre `signal GND` e `chassis GND` no layout final ainda nao esta descrito em detalhe
- a terminacao CAN pode ter existido no breakout de bancada; na PCB final ela deve ser explicitada por componente selecionavel
- nao ha, nas fontes lidas, definicao eletrica final para `L-Line` alem de `NC`

## Conflitos e divergencias identificadas

1. Narrativas antigas de UI local foram removidas desta rodada.
   - efeito: usar `ui_init.cpp` como fonte primaria de comportamento real da UI local

2. Narrativas antigas de selecao por serial USB foram removidas desta rodada.
   - efeito: no estado atual o firmware usa `NVS`, `DIP` e mudanca por UI/web

3. O firmware confirma pinagem e barramentos, mas nao confirma sozinho todos os componentes fisicos do carrier RevA.
   - efeito: para esquematico/PCB usar sempre a combinacao `codigo + docs/diagrama-eletrico-pcb.md + docs/engenharia-pcb.md + docs/reva-carrier-esp32-devkit.md + pcb-handoff + KiCad RevA`

4. Protecoes automotivas adicionais aparecem como recomendacao de engenharia, nao como baseline eletrico comprovado pelo firmware.
   - efeito: registrar essas protecoes como obrigatorias para PCB robusta, mas nao como "confirmadas no codigo"

## Riscos e lacunas tecnicas

- definir TVS de entrada, protecao contra inversao de polaridade e estrategia de ESD antes do esquematico final
- confirmar o `ESP32 DevKit` comercial exato antes de congelar footprints e keepouts
- confirmar o modulo real do `OLED SH1107` e do `KY-040` antes do layout final
- formalizar corrente maxima esperada do sistema se o buck deixar de ser modulo pronto
- revisar dissipacao de `RK1 510R` e posicionamento termico
- decidir se a RevA tera conector OBD de placa, chicote ou ambos
- documentar a estrategia final de ponto de teste e depuracao de bancada
- tratar `SSID`, `hostname` e credenciais hardcoded como risco operacional/documental; nao foram alterados nesta rodada
