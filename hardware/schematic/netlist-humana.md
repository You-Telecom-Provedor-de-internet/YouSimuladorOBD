# Netlist Humana

Fonte primaria:

- `hardware/pcb-handoff/netlist-rev-a.csv`

Fontes de cruzamento:

- `docs/hardware.md`
- `docs/pinout.md`
- `firmware/include/config.h`
- `docs/13-kline-l9637d-montagem.md`
- `docs/15-diagrama-eletrico-pcb.md`

Legenda curta:

- `[CC]` confirmado no codigo
- `[DE]` confirmado na documentacao/EDA existente
- `[IH]` inferido / hipotese

- `+12V_OBD -> J1-16` `[DE]`
- `+12V_PROT -> F1-2, U1-IN+, U3-7(VS), RK1-1, RLI1-1` `[DE]`
- `GND -> J1-4, J1-5, U1-IN-, U1-OUT-, MCU1-GND, U2-2(GND), U2-8(RS), U3-5(GND), CK1-2, CK2-2, DISP1-GND, ENC1-GND, SW2-SW7-retorno, SW1-comum, D1-D3-catodo` `[DE]`
- `+5V_SYS -> U1-OUT+, MCU1-VIN` `[DE]`
- `+3V3_SYS -> MCU1-3V3, U2-3(VCC), U3-3(VCC), DISP1-VCC, RDIP1-1, RDIP2-1, RDIP3-1` `[DE]`
- `CAN_TX_LOGIC -> MCU1-GPIO4, U2-1(TXD)` `[CC/DE]`
- `CAN_RX_LOGIC -> U2-4(RXD), MCU1-GPIO5` `[CC/DE]`
- `CANH -> U2-7(CANH), J1-6` `[DE]`
- `CANL -> U2-6(CANL), J1-14` `[DE]`
- `CAN_TERM_OPTIONAL -> RCAN1-1 entre CANH, RCAN1-2 entre CANL` `[DE]`
- `K_TX_LOGIC -> MCU1-GPIO17, U3-4(TX)` `[CC/DE]`
- `K_RX_LOGIC -> U3-1(RX), MCU1-GPIO16` `[CC/DE]`
- `KLINE -> U3-6(K), J1-7, RK1-2` `[DE]`
- `LI_BIAS -> U3-8(LI), RLI1-2` `[DE]`
- `CK1_VCC -> U3-3(VCC), CK1-1` `[DE]`
- `CK2_VS -> U3-7(VS), CK2-1` `[DE]`
- `I2C_SDA -> MCU1-GPIO21, DISP1-SDA` `[CC/DE]`
- `I2C_SCL -> MCU1-GPIO22, DISP1-SCL` `[CC/DE]`
- `ENC_A -> MCU1-GPIO12, ENC1-CLK` `[CC/DE]`
- `ENC_B -> MCU1-GPIO13, ENC1-DT` `[CC/DE]`
- `ENC_SW -> MCU1-GPIO15, ENC1-SW` `[CC/DE]`
- `BTN_PREV -> MCU1-GPIO32, SW2` `[CC/DE]`
- `BTN_NEXT -> MCU1-GPIO33, SW3` `[CC/DE]`
- `BTN_UP -> MCU1-GPIO25, SW4` `[CC/DE]`
- `BTN_DOWN -> MCU1-GPIO26, SW5` `[CC/DE]`
- `BTN_SELECT -> MCU1-GPIO27, SW6` `[CC/DE]`
- `BTN_PROTOCOL -> MCU1-GPIO14, SW7` `[CC/DE]`
- `DIP_BIT0 -> MCU1-GPIO34, RDIP1-2, SW1-bit0` `[CC/DE]`
- `DIP_BIT1 -> MCU1-GPIO35, RDIP2-2, SW1-bit1` `[CC/DE]`
- `DIP_BIT2 -> MCU1-GPIO36, RDIP3-2, SW1-bit2` `[CC/DE]`
- `LED_CAN -> MCU1-GPIO19, RLED1-1` `[CC/DE]`
- `LED_KLINE -> MCU1-GPIO18, RLED2-1` `[CC/DE]`
- `LED_TX -> MCU1-GPIO23, RLED3-1` `[CC/DE]`
- `LED_BUILTIN_ONBOARD -> MCU1-GPIO2, LED onboard do DevKit` `[CC/IH]`
- `USB_DEVKIT -> conector USB do modulo MCU1, UART0/gravacao` `[DE]`

## Notas de leitura

- A maior parte das nets de carrier vem do handoff CSV e da documentacao de `RevA`.
- As nets `CAN_*`, `K_*`, `I2C_*`, `BTN_*`, `ENC_*`, `DIP_*` e `LED_*` tambem estao confirmadas diretamente pelo firmware.
- `LED_BUILTIN_ONBOARD` nao aparece no handoff de PCB porque pertence ao modulo comercial do DevKit.
