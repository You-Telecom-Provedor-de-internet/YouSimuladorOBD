# Esquematico Conceitual da RevA

Documento textual por blocos para orientar a captura do esquematico real da `RevA`.

Fontes base:

- `docs/hardware.md`
- `docs/pinout.md`
- `docs/15-diagrama-eletrico-pcb.md`
- `hardware/pcb-handoff/netlist-rev-a.csv`
- `hardware/pcb-handoff/bom-rev-a.csv`
- `hardware/kicad/revA/*.kicad_sch`

Legenda de confianca:

- `confirmado no codigo`
- `confirmado na documentacao/EDA existente`
- `inferido / hipotese`

## 1. Alimentacao e protecao de entrada

### Componentes envolvidos

- `J1` conector OBD-II
- `F1` fusivel de entrada
- `TVS1` opcional/recomendada para PCB final
- elemento de protecao contra inversao de polaridade a definir

### Ligacoes principais

```text
J1 pin 16 -> F1 -> +12V_PROT
J1 pin 4  -> GND
J1 pin 5  -> GND
```

### Observacoes de projeto

- a placa deve nascer do `+12V` do OBD, nao de `USB`
- `J1 pin 16` tambem precisa alimentar o `VS` do `L9637D`
- a protecao automotiva precisa ficar na boca da placa, antes do buck e antes da distribuicao do `+12V`

### Cuidados de layout

- posicionar fusivel e TVS o mais perto possivel do conector OBD
- manter o loop de entrada `J1 -> F1 -> TVS/buck -> GND` curto
- prever trilhas e cobre compativeis com o pico de corrente de partida do buck

### Nivel de confianca

- `J1` e `F1`: confirmado na documentacao/EDA existente
- `TVS1` e protecao contra inversao: inferido / hipotese fortemente recomendada

## 2. Reguladores

### Componentes envolvidos

- `U1` buck `12V -> 5V`, baseline `LM2596`
- regulador `3V3` do proprio `ESP32 DevKit`

### Ligacoes principais

```text
+12V_PROT -> U1 IN+
GND       -> U1 IN-
U1 OUT+   -> +5V_SYS -> MCU1 VIN
U1 OUT-   -> GND
MCU1 3V3  -> +3V3_SYS
```

### Observacoes de projeto

- a `RevA` foi congelada para aceitar `LM2596` modulo ou equivalente de baixo risco
- a carrier depende do `3V3` gerado pelo DevKit para alimentar `SN65HVD230`, `L9637D VCC`, `OLED` e `pull-ups` do DIP

### Cuidados de layout

- isolar a regiao do buck da regiao da antena do ESP32
- reservar area termica para o buck e para o resistor `RK1`
- manter retorno de corrente do buck curto e com plano de GND consistente

### Nivel de confianca

- topologia `12V -> 5V -> VIN do DevKit`: confirmado na documentacao/EDA existente
- corrente maxima e implementacao final do buck: inferido / hipotese ate fechar o componente final

## 3. MCU

### Componentes envolvidos

- `MCU1` `ESP32 DevKit 38 pinos`
- `JMCU1`, `JMCU2` headers femea `1x19`

### Ligacoes principais

```text
+5V_SYS -> MCU1 VIN
GND     -> MCU1 GND
MCU1 3V3 -> +3V3_SYS
GPIO4/5  -> CAN
GPIO17/16 -> K-Line
GPIO21/22 -> OLED
GPIO32/33/25/26/27/14 -> botoes
GPIO12/13/15 -> encoder
GPIO34/35/36 -> DIP
GPIO19/18/23 -> LEDs externos
GPIO2 -> LED onboard do DevKit
```

### Observacoes de projeto

- a `RevA` e carrier, nao placa integrada do modulo ESP32
- o USB do DevKit precisa ficar acessivel na borda do conjunto
- o LED onboard em `GPIO2` existe no modulo comercial, nao precisa necessariamente de item dedicado na carrier

### Cuidados de layout

- respeitar keepout de antena sem cobre e sem componentes
- confirmar o DevKit comercial exato antes de travar footprints
- manter acesso mecanico ao USB e espaco para cabo

### Nivel de confianca

- pinagem funcional: confirmado no codigo
- geometria final do modulo comercial: inferido / hipotese ate confirmar o part number

## 4. Interface OBD

### Componentes envolvidos

- `J1` conector OBD-II

### Ligacoes principais

```text
J1 pin 4  -> GND
J1 pin 5  -> GND
J1 pin 6  -> CANH
J1 pin 7  -> KLINE
J1 pin 14 -> CANL
J1 pin 15 -> NC
J1 pin 16 -> +12V_OBD
```

### Observacoes de projeto

- nesta revisao `L-Line` permanece `NC`
- o conector final pode ser de placa ou chicote, mas a pinagem logica do projeto nao muda

### Cuidados de layout

- deixar acesso facil a pontos de teste de `CANH`, `CANL`, `KLINE`, `+12V` e `GND`
- proteger as linhas externas ja na entrada

### Nivel de confianca

- mapeamento funcional dos pinos usados: confirmado na documentacao/EDA existente

## 5. Interface CAN

### Componentes envolvidos

- `U2` `SN65HVD230`
- `CCAN1` `100nF`
- `RCAN1` `120R` opcional/jumper

### Ligacoes principais

```text
GPIO4         -> U2 TXD
GPIO5         <- U2 RXD
+3V3_SYS      -> U2 VCC
GND           -> U2 GND
GND           -> U2 RS
U2 CANH       -> J1 pin 6
U2 CANL       -> J1 pin 14
RCAN1 120R    -> entre CANH e CANL (opcional/jumper)
CCAN1 100nF   -> entre U2 VCC e GND
```

### Observacoes de projeto

- o firmware usa o `TWAI` nativo do ESP32; nao existe controlador CAN externo no projeto
- a terminacao CAN precisa ser explicitada na PCB mesmo que algum breakout de bancada ja a tivesse

### Cuidados de layout

- manter `CANH` e `CANL` paralelos, curtos e com impedancia o mais controlada possivel dentro da realidade da placa
- colocar `U2` perto do conector OBD
- colocar `CCAN1` colado no CI

### Nivel de confianca

- uso de CAN e pinagem logica: confirmado no codigo
- transceiver, terminacao e desacoplamento: confirmado na documentacao/EDA existente

## 6. Interface K-Line

### Componentes envolvidos

- `U3` `L9637D`
- `RK1` `510R`
- `RLI1` `10k`
- `CK1` `100nF`
- `CK2` `100nF`

### Ligacoes principais

```text
GPIO17      -> U3 pin 4 (TX)
GPIO16      <- U3 pin 1 (RX)
+3V3_SYS    -> U3 pin 3 (VCC)
GND         -> U3 pin 5 (GND)
KLINE       -> U3 pin 6 (K) -> J1 pin 7
+12V_PROT   -> U3 pin 7 (VS)
RLI1 10k    -> U3 pin 8 (LI) para U3 pin 7 (VS)
RK1 510R    -> U3 pin 6 (K) para U3 pin 7 (VS)
CK1 100nF   -> U3 pin 3 para GND
CK2 100nF   -> U3 pin 7 para GND
U3 pin 2    -> NC
```

### Observacoes de projeto

- este e o bloco mais sensivel do projeto porque a bancada validada depende explicitamente de `RK1`, `RLI1`, `CK1` e `CK2`
- o firmware suporta `ISO 9141-2`, `KWP 5-baud` e `KWP Fast`
- a placa precisa preservar o comportamento de idle alto da linha `K`

### Cuidados de layout

- colocar `U3` perto do conector OBD
- colocar `CK1` e `CK2` encostados no CI
- dar folga termica a `RK1`
- deixar pontos de teste em `K`, `TX_K`, `RX_K`, `VS` e `GND`

### Nivel de confianca

- pinagem logica, UART1 e comportamento de protocolo: confirmado no codigo
- topologia eletrica do frontend K-Line: confirmado na documentacao/EDA existente

## 7. Programacao e debug

### Componentes envolvidos

- conector `USB` do `ESP32 DevKit`
- `Serial` do modulo

### Ligacoes principais

```text
USB do DevKit -> gravacao de firmware
USB do DevKit -> monitor serial em 115200
```

### Observacoes de projeto

- a carrier deve prever acesso mecanico ao USB
- nesta revisao nao e necessario adicionar interface extra de programacao se o DevKit mantiver USB acessivel

### Cuidados de layout

- preservar janela mecanica para o cabo
- nao bloquear o DevKit com componentes altos ao redor do conector USB

### Nivel de confianca

- `Serial.begin(115200)` confirmado no codigo
- uso do USB do DevKit como caminho fisico: confirmado na documentacao/EDA existente

## 8. LEDs

### Componentes envolvidos

- `D1-D3`
- `RLED1-RLED3`

### Ligacoes principais

```text
GPIO19 -> RLED1 -> D1 -> GND
GPIO18 -> RLED2 -> D2 -> GND
GPIO23 -> RLED3 -> D3 -> GND
GPIO2  -> LED onboard do DevKit
```

### Observacoes de projeto

- os tres LEDs externos fazem parte da carrier
- o LED onboard depende do modulo comercial do DevKit

### Cuidados de layout

- manter polaridade clara na serigrafia
- posicionar de forma visivel ao usuario quando o conjunto estiver fechado

### Nivel de confianca

- GPIOs dos LEDs: confirmado no codigo
- resistores serie e LEDs externos: confirmado na documentacao/EDA existente

## 9. Botoes

### Componentes envolvidos

- `SW2-SW7`

### Ligacoes principais

```text
GPIO32 -> SW2 -> GND
GPIO33 -> SW3 -> GND
GPIO25 -> SW4 -> GND
GPIO26 -> SW5 -> GND
GPIO27 -> SW6 -> GND
GPIO14 -> SW7 -> GND
```

### Observacoes de projeto

- os botoes usam `INPUT_PULLUP`; nao precisam de `pull-up` externo

### Cuidados de layout

- roteamento simples, curto e com referencia de GND
- manter acessibilidade mecanica coerente com a tampa frontal

### Nivel de confianca

- confirmado no codigo + confirmado na documentacao/EDA existente

## 10. Encoder

### Componentes envolvidos

- `ENC1` `KY-040`
- `JENC1`

### Ligacoes principais

```text
GPIO12 -> ENC1 CLK
GPIO13 -> ENC1 DT
GPIO15 -> ENC1 SW
+3V3_SYS -> ENC1 +
GND -> ENC1 GND
```

### Observacoes de projeto

- o firmware usa `ESP32Encoder`
- o `KY-040` e tratado como modulo/placa filha nesta RevA

### Cuidados de layout

- respeitar a mecanica do painel frontal
- lembrar que `GPIO12` e sensivel em boot

### Nivel de confianca

- pinagem funcional: confirmado no codigo
- header/modulo na RevA: confirmado na documentacao/EDA existente

## 11. DIP

### Componentes envolvidos

- `SW1`
- `RDIP1-RDIP3`

### Ligacoes principais

```text
+3V3_SYS -> RDIP1 -> GPIO34 -> SW1 bit 0 -> GND
+3V3_SYS -> RDIP2 -> GPIO35 -> SW1 bit 1 -> GND
+3V3_SYS -> RDIP3 -> GPIO36 -> SW1 bit 2 -> GND
```

### Observacoes de projeto

- leitura no boot via `main.cpp`
- chave fechada para `GND` representa `LOW`

### Cuidados de layout

- manter trilhas simples e longe de fontes de ruido desnecessarias
- deixar orientacao do DIP clara na serigrafia

### Nivel de confianca

- confirmado no codigo + confirmado na documentacao/EDA existente

## 12. Conectores

### Componentes envolvidos

- `J1` OBD-II
- `JMCU1/JMCU2`
- `JDISP1`
- `JENC1`

### Observacoes de projeto

- a carrier precisa acomodar o DevKit, o OLED e o encoder de forma coerente com a mecanica da case
- `MH1-MH4` devem ser mantidos como furos de fixacao da placa

### Nivel de confianca

- confirmado na documentacao/EDA existente

## 13. Pontos de teste

### Pontos minimos recomendados

- `TP_+12V`
- `TP_+5V`
- `TP_+3V3`
- `TP_GND`
- `TP_CANH`
- `TP_CANL`
- `TP_K`
- `TP_TX_K`
- `TP_RX_K`

### Observacoes de projeto

- esses pontos aparecem de forma recorrente como necessidade pratica de bancada
- nao estao todos congelados como RefDes no handoff atual, mas devem entrar na PCB final

### Nivel de confianca

- necessidade funcional: confirmado na documentacao/EDA existente
- RefDes e geometria final: inferido / hipotese
