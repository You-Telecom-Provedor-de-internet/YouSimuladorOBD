# BOM — Status dos Componentes

Atualizado: 2026-03-31 — pedidos realizados no Mercado Livre.

## ✅ Comprado / A Caminho

| # | Componente | Produto | Status | Observações |
|---|-----------|---------|--------|-------------|
| 1 | **ESP32 38-pin** | NodeMCU ESP32 Espressif 4MB Flash WiFi+BT | Em trânsito | Wi-Fi nativo incluso |
| 2 | **Placa expansão c/ bornes** | Adaptador expansão terminal borne ESP32S 38pin GPIO | Em trânsito | Conexões por parafuso, sem solda |
| 3 | **Módulo CAN SN65HVD230** | Módulo CAN Bus SN65HVD230 | Em trânsito | **USE ESTE para CAN.** 3.3V nativo |
| 4 | **Módulo CAN MCP2515+TJA1050** | Módulo CAN Bus MCP2515 TJA1050 | Em trânsito | Backup — TJA1050 é 5V |
| 5 | **Conversor DC-DC LM2596** | Step-Down Buck LM2596 3A | Em trânsito | 12V OBD → 5V ESP32 |
| 6 | **Kit 120 resistores 1/4W 1%** | Kit Resistores Metal Film | Em trânsito | 120Ω CAN, 10kΩ pull-ups, 1kΩ |
| 7 | **Conector OBD-II fêmea 16p** | Plug Fêmea OBD2 16 pinos + chicote | Em trânsito | Ponto de conexão com scanner |
| 8 | **Diodo 1N4148** | Pacote 100 peças Semtech | Pendente FULL | Para circuito K-Line discreto |
| 9 | **Transistor 2N2222 NPN** | 20 peças TO-92 | Chega 3–4 abr | Para circuito K-Line discreto |
| 10 | **L9637D SOIC8** | Circuito Integrado L9637D SOIC8 | Pendente | Driver K-Line — usar com adaptador abaixo |
| 11 | **Adaptador SOIC8→DIP** | 10x Adaptador SMD SOP8/SOIC8/DIP8 | Chega quinta-feira | Solda o L9637D nele → usa no protoboard |

## ✅ L9637D SOIC8 + Adaptador — Solução Completa

Adaptador **10x SOP8/SOIC8→DIP** comprado. Chega quinta-feira.

### Como usar o L9637D no protoboard:
```
1. Alinhe o L9637D no pad SOIC8 do adaptador
   (pino 1 marcado com ponto no CI e no adaptador)
2. Solda com ferro de ponta fina — aplica fluxo antes
3. Solda pinos macho (header 4+4) nos furos do adaptador
4. Encaixa no protoboard normalmente
```

### Pinout L9637D SOIC8 → ligação com ESP32:
```
L9637D Pino 1 (TXD) ← GPIO17 (TX K-Line do ESP32)
L9637D Pino 2 (RXD) → GPIO16 (RX K-Line do ESP32)
L9637D Pino 3 (GND) → GND
L9637D Pino 4 (VCC) → 3.3V do ESP32
L9637D Pino 5 (K)   ↔ OBD-II Pino 7 (K-Line, 12V)
L9637D Pino 6 (VBB) ← OBD-II Pino 16 (+12V)
Pinos 7,8            → não conectar
```

## ❌ Ainda Faltando

| Componente | Função | Alternativa / Observação |
|-----------|--------|--------------------------|
| **L9637D** (ou MC33290) | Driver K-Line (ISO 9141-2 / KWP2000) — conversão 3.3V↔12V | Circuito discreto com NPN 2N2222 + resistores (tem no kit) serve como alternativa |
| **Display OLED SSD1306** 128x64 I2C | Interface visual do simulador | Opcional — sem ele só Wi-Fi funciona |
| **Botões tácteis 6x6mm** (×6) | PREV, NEXT, UP, DOWN, SELECT, PROTOCOL | Opcional se usar só Wi-Fi |
| **Encoder Rotativo KY-040** | Ajuste fino de valores | Opcional se usar só Wi-Fi |
| **DIP Switch 3 posições** | Seleção de protocolo por chave física | Opcional — protocolo pode ser trocado pela Web UI |
| **LEDs 3mm** (verde, amarelo, vermelho) | Status visual | Opcional |
| **Capacitores 100nF** (×4) | Desacoplamento VCC | Recomendado para estabilidade |
| **Capacitor 10µF** (×2) | Filtro alimentação | Recomendado |
| **Diodo 1N4148** (×2) | Proteção K-Line (circuito discreto) | Só se não usar L9637D |

---

## ⚠️ Atenção — MCP2515 + TJA1050 (5V vs 3.3V)

Você tem **dois** módulos CAN:

### SN65HVD230 → USE ESTE para o projeto
- Opera em **3.3V** — conecta direto nos pinos do ESP32
- Usa o **TWAI nativo** do ESP32 (GPIO4 TX, GPIO5 RX)
- Sem necessidade de level shifter
- Desempenho máximo: até 1 Mbps

### MCP2515 + TJA1050 → Guarda como backup ou ignore
- Chip MCP2515 precisa de **VCC 5V** para funcionar corretamente
- TJA1050 transceiver emite **saída 5V** no pino RX → pode danificar GPIO ESP32 (3.3V)
- Se quiser usar: precisa de **level shifter** (BSS138 ou similar) em MISO + circuito de alimentação 5V separado
- **Recomendação:** use o SN65HVD230 e deixe o MCP2515 de reserva

---

## Circuito K-Line Discreto (sem L9637D)

Se não quiser comprar o L9637D, use os componentes que já tem no kit:

```
+12V (OBD Pino 16)
        │
    [1kΩ]         ← tem no kit de resistores
        │
        ├────────────────────── GPIO16 (RX K-Line) do ESP32
        │                       (via divisor 1kΩ/2kΩ para 3.3V)
    [Diodo 1N4148]  ← comprar
        │
    Coletor NPN (2N2222) ← comprar ~R$1
        │
    Emissor → GND
        │
    Base → [1kΩ] → GPIO17 (TX K-Line)
```

> **Custo total da alternativa:** ~R$3 (1x 2N2222 + 2x 1N4148)
> Comprar o L9637D (~R$8) é mais robusto e fácil de montar.

---

## Alimentação — Como Usar o LM2596

```
OBD Pino 16 (+12V) ──► [LM2596 ajustado para 5V] ──► VIN do ESP32
OBD Pino 4/5 (GND) ──► GND do LM2596 ──► GND do ESP32

Ajuste do LM2596:
- Ligue sem carregar
- Mida na saída com multímetro
- Gire trimpot até 5.0V
- Conecte ao ESP32
```

> O ESP32 tem regulador interno 3.3V. Alimente o pino VIN (5V) e tudo funciona.
> Limite de corrente: LM2596 3A é mais do que suficiente (ESP32 pico ~500mA + LEDs ~50mA).
