# 12 - Auditoria do Firmware e Bancada

## Objetivo

Registrar os achados principais da auditoria do `YouSimuladorOBD`, as correcoes ja aplicadas no firmware e o procedimento de ligacao do modulo `SN65HVD230` na bancada atual.

Data da rodada: `2026-04-02`

## Achados Criticos

### 1. Troca de protocolo nao reinicializava o hardware

Antes desta auditoria, mudar o protocolo na UI/web alterava `active_protocol`, mas o transporte fisico continuava no protocolo iniciado no boot. Na pratica, a tela podia mostrar `K-Line` enquanto o TWAI ainda estava ativo, ou vice-versa.

Status: corrigido.

O firmware agora sobe os workers de CAN e K-Line juntos e ativa/desativa cada barramento dinamicamente conforme `active_protocol`.

Arquivos:

- `firmware/src/protocols/protocol_init.cpp`
- `firmware/src/protocols/can_protocol.cpp`
- `firmware/src/protocols/kline_protocol.cpp`

### 2. Mode 03 / 04 estava mal encapsulado no transporte fisico

As requisicoes `03` e `04` podem vir sem PID. A implementacao antiga tratava isso mal em CAN e K-Line:

- CAN recusava `Single Frame` com apenas o byte do servico
- K-Line podia interpretar checksum como PID
- respostas negativas e respostas de `03/04` usavam framing incorreto

Status: corrigido.

O firmware agora:

- aceita servico sem PID quando faz sentido
- responde `7F <servico> <NRC>` corretamente
- nao injeta PID em respostas `03/04`
- limpa `dtcs[]` e `dtc_count` corretamente no `Mode 04`

### 3. CAN fisico nao conseguia responder payload longo

VIN (`Mode 09`) e uma lista maior de DTCs podem ultrapassar os 7 bytes de payload do `Single Frame` CAN. A implementacao antiga descartava qualquer resposta multi-frame.

Status: corrigido para transmissao.

O firmware agora transmite:

- `Single Frame` quando o payload cabe em 7 bytes
- `First Frame + Flow Control + Consecutive Frames` quando a resposta e maior

Observacao:

- recepcao multi-frame ainda nao e necessaria para o escopo atual, porque as requisicoes OBD suportadas pelo simulador seguem curtas

### 4. O simulador podia responder a trafego CAN que nao era OBD dele

O parser antigo aceitava praticamente qualquer frame recebido no TWAI.

Status: corrigido.

Agora o firmware filtra os IDs OBD esperados:

- `0x7DF` e `0x7E0` para CAN 11-bit
- `0x18DB33F1` e `0x18DA10F1` para CAN 29-bit

## Validacao Feita

- `pio run`: OK
- `pio run -t upload`: OK em `COM3`
- `GET http://192.168.1.5/ping`: `pong`
- transceiver `SN65HVD230` montado e operando na perfboard
- `ELM327 + Torque Pro` lendo PIDs em tempo real com sucesso
- `Mode 03` validado com leitura de DTCs
- `Mode 04` validado com limpeza de DTCs
- `Mode 09` validado com leitura de VIN
- troca de protocolo em runtime validada nos 4 protocolos CAN

Protocolos CAN confirmados em bancada:

- `CAN 11b 500k`
- `CAN 11b 250k`
- `CAN 29b 500k`
- `CAN 29b 250k`

Versao de firmware desta rodada:

- `APP_VERSION = 2026.04.02.1`

## Ligacao do SN65HVD230

### Mapeamento eletrico

Breakout efetivamente usado nesta bancada:

```text
3V3
GND
CTX
CRX
CANH
CANL
```

| Breakout SN65HVD230 | ESP32 / OBD | Funcao |
|---|---|---|
| `3V3` | `3V3` do ESP32 | alimentacao logica |
| `GND` | `GND` comum | referencia comum |
| `CTX` | `GPIO4` | TWAI TX |
| `CRX` | `GPIO5` | TWAI RX |
| `CANH` | OBD pino `6` | CAN High |
| `CANL` | OBD pino `14` | CAN Low |

### Sequencia recomendada de bancada

1. Soldar `3V3`, `GND`, `CTX` e `CRX`.
2. Confirmar que o ESP32 continua ligando e responde em rede.
3. Soldar `CANH` e `CANL` no chicote OBD.
4. Validar com scanner externo em `CAN 11-bit 500k` primeiro.
5. Testar depois `11-bit 250k`, `29-bit 500k` e `29-bit 250k`.

### Terminacao

- Se o simulador estiver sozinho com o scanner na bancada, normalmente precisa de uma terminacao total adequada no barramento.
- O breakout usado nesta rodada aparenta possuir terminacao onboard, entao evitar somar outra `120R` sem necessidade.
- Evitar duplicar terminacao sem saber o que o outro lado ja possui.

## Pendencias para Encerramento do Produto

### Firmware

- validar a trilha K-Line quando a interface fisica 12V estiver pronta
- revisar consumo de flash OTA, que segue muito perto do limite
- revisar warnings de API depreciada do ArduinoJson em `web_server.cpp`

### Hardware

- montar a etapa K-Line definitiva com transistor/dispositivo de interface 12V
- organizar alimentacao e distribuicao de GND na perfboard
- decidir a terminacao CAN da versao final de bancada

### Produto

- subir a proxima release OTA com `firmware.bin` novo e, se necessario, `littlefs.bin`
- trocar credenciais padrao antes de uso em campo
- manter hostname unico por unidade quando houver mais de um ESP32 na mesma rede

## Observacoes de Risco

- A flash esta muito no limite do slot OTA (`~98.5%` de uso). Novas features devem ser adicionadas com cuidado.
- OTA remota esta funcional, mas ainda depende de operacao autenticada na rede local ou do uso da interface web do proprio equipamento.
