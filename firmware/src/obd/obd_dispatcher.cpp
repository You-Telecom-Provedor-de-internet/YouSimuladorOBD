#include "obd_dispatcher.h"
#include <Arduino.h>
#include <cstring>
#include "mode01_handler.h"
#include "mode03_handler.h"
#include "mode09_handler.h"

// ════════════════════════════════════════════════════════════
//  OBD Dispatcher — roteia requisição para o handler correto
//  e adiciona byte de modo na resposta
// ════════════════════════════════════════════════════════════

static Mode01Handler mode01;
static Mode03Handler mode03;
static Mode09Handler mode09;

OBDResponse OBDDispatcher::dispatch(const OBDRequest& req, const SimulationState& state) {
    OBDResponse resp;

    switch (req.mode) {
        case 0x01: {
            resp = mode01.handle(req, state);
            if (!resp.negative) prependModeByte(resp, 0x41);
            break;
        }
        case 0x03: {
            resp = mode03.handle(state);
            if (!resp.negative) prependModeByte(resp, 0x43);
            break;
        }
        case 0x04: {
            // Limpar DTCs — sem parâmetros adicionais
            resp.data[resp.len++] = 0x44;
            break;
        }
        case 0x09: {
            resp = mode09.handle(req, state);
            if (!resp.negative) prependModeByte(resp, 0x49);
            break;
        }
        default:
            resp.negative = true;
            resp.nrc = 0x11; // serviceNotSupported
            break;
    }
    return resp;
}

void OBDDispatcher::prependModeByte(OBDResponse& r, uint8_t modeByte) {
    // Insere mode byte no início: [ModeByte][dados existentes...]
    // PID é adicionado pelo protocol layer (CAN/K-Line)
    if (r.len >= sizeof(r.data)) return; // sem espaço — protege overflow
    memmove(r.data + 1, r.data, r.len);
    r.data[0] = modeByte;
    r.len++;
}
