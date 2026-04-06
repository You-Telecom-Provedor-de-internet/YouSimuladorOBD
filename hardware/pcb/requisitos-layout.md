# Requisitos de Layout da PCB

Documento orientado a layout para a `RevA` do `YouSimuladorOBD`.

## Objetivo

Guiar o layout de uma placa:

- fiel a bancada validada
- robusta para ambiente automotivo/OBD
- de baixo risco para a primeira fabricacao

## Escopo da RevA

- `carrier` para `ESP32 DevKit 38 pinos`
- `SN65HVD230` para CAN
- `L9637D` para K-Line
- `OLED SH1107` via header
- `KY-040` via header
- buck `12V -> 5V` baseado em `LM2596` ou equivalente

## 1. Protecao contra surto

- colocar `TVS` diretamente na entrada `+12V` vinda de `J1 pin 16`
- colocar a `TVS` fisicamente antes da distribuicao para buck e `L9637D VS`
- manter o retorno da `TVS` ao `GND` com caminho curto e de baixa impedancia
- evitar que o surto passe pela placa inteira antes de encontrar o clamp

Status:

- recomendado por engenharia
- nao confirmado como item fechado de bancada

## 2. ESD

- prever protecao ESD nas linhas externas acessiveis do OBD, principalmente `CANH`, `CANL` e `K`
- colocar a protecao o mais perto possivel do conector externo
- manter caminho curto entre o dispositivo de protecao e o plano de `GND`

Status:

- inferido / hipotese fortemente recomendada

## 3. Inversao de polaridade

- incluir protecao de polaridade reversa na entrada `+12V`
- preferir topologia com baixa perda se o envelope termico permitir
- decidir entre diodo simples e MOSFET ideal somente quando o part number final do buck e a corrente alvo estiverem fechados

Status:

- recomendado por engenharia
- topologia final ainda em aberto

## 4. Ruido e dominios de alimentacao

Separar claramente no esquematico e no layout os dominios:

- `+12V_OBD`
- `+12V_PROT`
- `+5V_SYS`
- `+3V3_SYS`
- `GND`

Regras:

- manter `+12V_PROT` longe da regiao de antena do ESP32
- manter o buck e suas correntes pulsantes longe de `CAN`, `K-Line`, `I2C` e da antena
- usar plano de `GND` continuo, com poucos estrangulamentos
- evitar rotas longas de retorno para os desacoplamentos

## 5. Posicionamento do fusivel, TVS e buck

- `F1` deve ficar na entrada da placa, logo apos `J1 pin 16`
- `TVS1` deve ficar entre `+12V_PROT` e `GND`, o mais proximo da entrada
- `U1` buck deve ficar na zona de potencia, antes da area de logica
- manter a malha quente do buck minima
- nao colocar o buck sob a antena do `ESP32 DevKit`

## 6. Keepout da antena do ESP32 DevKit

Obrigatorio para a RevA:

- sem cobre sob a antena
- sem trilhas de alta comutacao sob a antena
- sem transceiver, buck, TVS ou componentes altos encostados na antena

Referencia base:

- `docs/17-revA-carrier-esp32-devkit.md`

## 7. Posicionamento de `SN65HVD230` e `L9637D`

### `U2 SN65HVD230`

- colocar perto do conector OBD
- manter `CANH` e `CANL` curtos ate `J1`
- colocar `CCAN1` junto ao `VCC/GND` do CI
- manter `RS` curto ate `GND`

### `U3 L9637D`

- colocar perto do conector OBD
- manter `K` curto ate `J1 pin 7`
- manter `VS` curto e robusto ate `+12V_PROT`
- colocar `CK1` e `CK2` encostados no CI
- dar area termica e ventilacao minima para `RK1 510R`

## 8. Roteamento CANH/CANL

- rotear `CANH` e `CANL` como par, lado a lado
- manter comprimento semelhante e poucas mudancas bruscas
- evitar stubs desnecessarios
- deixar `RCAN1` proximo da saida do barramento ou em posicao que faca sentido para populacao opcional
- manter afastamento razoavel do buck e de trilhas ruidosas

## 9. Roteamento da K-Line

- manter `KLINE` curta, direta e protegida
- separar `KLINE` do laco de comutacao do buck
- manter `TX_K` e `RX_K` logicos longe da regiao de potencia
- minimizar a area de loop entre `U3`, `RK1`, `RLI1`, `CK2` e o conector OBD

## 10. Desacoplamento

- todo CI deve ter desacoplamento local
- `CK1` e `CK2` sao obrigatorios para o `L9637D`
- `CCAN1` deve ficar junto do `SN65HVD230`
- o `ESP32 DevKit` ja traz parte do desacoplamento no proprio modulo, mas a carrier nao deve degradar esse ambiente com retorno ruim

## 11. Retorno de corrente e GND

- usar plano de `GND` continuo sempre que possivel
- evitar recortes que forcem retorno longo entre transceivers e o conector OBD
- conectar `J1 pin 4` e `J1 pin 5` ao `GND` comum com baixa impedancia
- tratar o encontro entre correntes do buck e logica de forma cuidadosa para nao injetar ruido nos barramentos

## 12. Pontos de teste minimos

Minimos recomendados:

- `TP_+12V`
- `TP_+5V`
- `TP_+3V3`
- `TP_GND`
- `TP_CANH`
- `TP_CANL`
- `TP_K`
- `TP_TX_K`
- `TP_RX_K`

Se houver espaco:

- `TP_VS_U3`
- `TP_TWDIAG` ou referencia equivalente de debug local

## 13. Regras de painel e conectividade local

- alinhar `OLED`, `encoder` e botoes com a mecanica prevista em `hardware/cad/`
- confirmar o modulo real do `OLED` e do `KY-040` antes de congelar os furos e headers
- manter a leitura visual dos LEDs e o acesso ao `USB` do DevKit

## 14. Recomendacoes especificas para a RevA

- preservar a arquitetura de baixo risco validada em bancada
- nao integrar `ESP32-WROOM` nesta revisao
- nao introduzir segunda fonte de `3V3` se o objetivo da placa continuar sendo carrier do DevKit
- preferir populacao opcional para itens ainda em avaliacao, como `RCAN1`

## 15. Evolucao futura

Para uma revisao posterior, considerar:

- `ESP32-WROOM` integrado
- protecao automotiva mais completa e fechada por componente
- conector OBD e filtragem EMC fechados com requisitos de produto
- eventual integracao de medicao de tensao/corrente

## 16. Pendencias que devem ser resolvidas antes do layout final

- part number exato do `ESP32 DevKit`
- modulo final do `OLED`
- modulo final do `KY-040`
- TVS exata
- estrategia final de reversao de polaridade
- forma final do conector OBD
- corrente alvo do buck se deixar de ser modulo pronto
