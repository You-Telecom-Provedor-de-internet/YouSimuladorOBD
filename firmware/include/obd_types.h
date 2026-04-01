#pragma once
#include <cstdint>
#include <cstring>

// ════════════════════════════════════════════════════════════
//  Tipos compartilhados entre todas as camadas OBD-II
// ════════════════════════════════════════════════════════════

// Requisição OBD recebida do scanner
struct OBDRequest {
    uint8_t mode;       // ex: 0x01, 0x03, 0x09
    uint8_t pid;        // ex: 0x0C (RPM)
    uint8_t data[6];    // bytes adicionais (raro no OBD básico)
    uint8_t data_len;
};

// Resposta OBD a enviar para o scanner
struct OBDResponse {
    uint8_t data[20];   // payload (sem header de protocolo)
    uint8_t len;
    bool    negative;   // true = resposta negativa (NRC)
    uint8_t nrc;        // Negative Response Code

    OBDResponse() : len(0), negative(false), nrc(0) {
        memset(data, 0, sizeof(data));
    }
};

// Resultado de parsing de frame físico
enum class ParseResult {
    OK,
    INCOMPLETE,   // aguardando mais bytes
    INVALID,      // frame malformado
    UNSUPPORTED,  // protocolo/modo não suportado
};
