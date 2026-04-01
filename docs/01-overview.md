# 01 — Visão Geral e Arquitetura do Sistema

## Objetivo

O YouSimuladorOBD é um emulador OBD-II que, conectado a um scanner automotivo ou leitor OBD (ELM327, BlueDriver, launch, etc.), responde como se fosse a ECU de um veículo real, permitindo:

- Testar scanners OBD-II sem necessidade de um carro
- Validar aplicativos de diagnóstico veicular
- Demonstrar e ensinar protocolos automotivos
- Simular condições de falha (DTCs) de forma controlada

---

## Diagrama de Contexto

```
┌─────────────────────────────────────────────────────────────┐
│                   SCANNER OBD-II / ELM327                   │
│   (celular + app, leitor dedicado, notebook com software)   │
└──────────────────────────┬──────────────────────────────────┘
                           │ OBD-II Conector (16 pinos)
                           │ Pinos 6/14 → CAN H/L
                           │ Pino 7     → K-Line
                  ┌────────▼────────┐
                  │  PCB Interface  │
                  │  CAN: SN65HVD230│
                  │  K-Line: L9637D │
                  └────────┬────────┘
                           │ UART / TWAI
                  ┌────────▼────────┐
                  │   ESP32 38-pin  │
                  │   (Firmware)    │
                  └────────┬────────┘
                           │ GPIO
              ┌────────────┼────────────┐
              │            │            │
         ┌────▼────┐  ┌────▼────┐  ┌───▼────┐
         │ Botões  │  │  OLED   │  │  LEDs  │
         │ Encoders│  │ Display │  │ Status │
         └─────────┘  └─────────┘  └────────┘
```

---

## Fluxo de Operação Geral

```
1. INICIALIZAÇÃO
   └─ ESP32 boot → detecta protocolo selecionado → inicializa interface física

2. DETECÇÃO / HANDSHAKE
   └─ Aguarda requisição do scanner
      ├─ CAN: aguarda frame 0x7DF ou 0x18DB33F1
      └─ K-Line: executa sequência de inicialização (5-baud ou fast-init)

3. LOOP PRINCIPAL
   └─ Recebe requisição OBD-II
      ├─ Decodifica: Modo + PID
      ├─ Busca valor atual do parâmetro (ajustado via UI)
      ├─ Monta frame de resposta conforme protocolo ativo
      └─ Transmite resposta

4. CONTROLE DO USUÁRIO
   └─ Botões / encoder ajustam valores dos parâmetros em tempo real
      └─ Display OLED exibe valores atuais e protocolo ativo
```

---

## Camadas da Arquitetura de Software

```
┌─────────────────────────────────────────────────────┐
│                   CAMADA DE APLICAÇÃO                │
│  SimulationManager — mantém estado dos 12 parâmetros│
│  DTCManager        — gerencia lista de DTCs ativos  │
│  VINManager        — gerencia número de quadro      │
└──────────────────────────┬──────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────┐
│                  CAMADA OBD-II (PIDs)                │
│  Mode01Handler — responde PIDs de dados correntes   │
│  Mode03Handler — responde com DTCs ativos           │
│  Mode04Handler — limpa DTCs                         │
│  Mode09Handler — informa VIN                        │
└──────────────────────────┬──────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────┐
│               CAMADA DE PROTOCOLO                    │
│  CANProtocol   — ISO 15765-4 (11/29-bit, 250/500k)  │
│  KLineProtocol — ISO 9141-2 + KWP2000               │
│  ISOTPLayer    — segmentação/remontagem ISO-TP       │
└──────────────────────────┬──────────────────────────┘
                           │
┌──────────────────────────▼──────────────────────────┐
│               CAMADA FÍSICA / HAL                    │
│  TWAI Driver   — CAN nativo ESP32                   │
│  UART Driver   — K-Line via UART + GPIO             │
└─────────────────────────────────────────────────────┘
```

---

## Seleção de Protocolo

O protocolo ativo pode ser selecionado por:

1. **DIP Switch físico** (3 chaves = 8 combinações, cobre os 7 protocolos)
2. **Botão de ciclo** — cicla entre os protocolos sequencialmente
3. **Comando via serial USB** — útil para desenvolvimento e testes

A cada mudança de protocolo, o hardware correspondente é reconfigurado:
- **Protocolos 1-4 (CAN):** TWAI ativado, K-Line desativado
- **Protocolos 5-7 (K-Line):** TWAI desativado, UART configurado

---

## Compatibilidade com Scanners

| Scanner / Ferramenta | CAN | K-Line | Testado |
|---------------------|-----|--------|---------|
| ELM327 (modo passthrough) | ✓ | ✓ | — |
| Torque Pro (Android) | ✓ | — | — |
| OBD Auto Doctor | ✓ | ✓ | — |
| Launch CRP129 | ✓ | ✓ | — |
| PyOBD (Python) | ✓ | ✓ | — |
