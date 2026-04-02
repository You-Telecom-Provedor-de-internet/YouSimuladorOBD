/**
 * @file dtc_catalog.h
 * @brief Catálogo completo de DTCs e Cenários de Falha para o YouSimuladorOBD
 *
 * Define:
 *  - DtcEntry    : struct de um código de falha OBD2 com descrição em português
 *  - DtcScenario : conjunto de DTCs que simulam uma falha específica do veículo
 *  - DTC_CATALOG[]    : 70 códigos reais organizados por sistema
 *  - DTC_SCENARIOS[]  : 12 cenários prontos para uso imediato
 *
 * Como usar nos demais módulos:
 *   #include "dtc_catalog.h"
 *
 * Para aplicar um cenário ao estado de simulação (ver simulation_state.h):
 *   applyDtcScenario(g_sim_state, DTC_SCENARIOS[4]);
 *
 * Para buscar a descrição de um código:
 *   const DtcEntry* e = findDtc(0x0171);
 *   if (e) Serial.println(e->description);
 *
 * Branch : feature/dtc-scenarios
 */

#pragma once
#include <stdint.h>
#include <stdio.h>   // snprintf

// ============================================================
//  ENUMERAÇÕES
// ============================================================

/** Sistema do veículo ao qual o DTC pertence */
enum class DtcSystem : uint8_t {
    POWERTRAIN_FUEL_AIR  = 0,  ///< P01xx – Combustível e Ar
    POWERTRAIN_INJECTION = 1,  ///< P02xx – Injetores
    POWERTRAIN_IGNITION  = 2,  ///< P03xx – Ignição / Combustão
    POWERTRAIN_EMISSIONS = 3,  ///< P04xx – Emissões (EGR, EVAP, Catalisador)
    POWERTRAIN_SPEED     = 4,  ///< P05xx – Velocidade e Controles Elétricos
    POWERTRAIN_ECU       = 5,  ///< P06xx – ECU / Computador de bordo
    POWERTRAIN_TRANS     = 6,  ///< P07xx – Transmissão automática
    BODY                 = 7,  ///< Bxxxx – Carroceria (Airbag, SRS)
    CHASSIS              = 8,  ///< Cxxxx – Chassi (ABS, ESP)
    NETWORK              = 9,  ///< Uxxxx – Rede CAN / comunicação
};

/** Nível de gravidade da falha */
enum class DtcSeverity : uint8_t {
    INFO     = 0,  ///< Apenas informativo
    LEVE     = 1,  ///< Não compromete funcionamento imediato
    MODERADO = 2,  ///< Afeta performance ou emissões
    GRAVE    = 3,  ///< Risco de dano ou falha de sistema crítico
    CRITICO  = 4,  ///< Veículo pode não funcionar ou há risco de segurança
};

// ============================================================
//  STRUCTS
// ============================================================

/** Representa um único código de falha OBD2 */
struct DtcEntry {
    uint16_t    code;         ///< Código: 0x0171 = P0171, 0xC031 = C0031
    char        prefix;       ///< 'P', 'C', 'B' ou 'U'
    DtcSystem   system;
    DtcSeverity severity;
    const char* description;  ///< Descrição completa em português
    const char* cause;        ///< Causa mais comum
};

/**
 * Cenário de falha: agrupa DTCs relacionados e define como os
 * parâmetros do motor devem reagir durante a simulação.
 */
struct DtcScenario {
    uint8_t     id;
    const char* name;              ///< Nome curto (display OLED / web)
    const char* description;       ///< Descrição detalhada do problema
    uint8_t     dtc_count;         ///< Número de DTCs (máx 16)
    uint16_t    dtcs[16];
    int16_t     rpm_delta;         ///< Variação de RPM (negativo = motor falhando)
    int8_t      temp_delta;        ///< Variação de temperatura em °C
    int8_t      throttle_delta;    ///< Variação na posição do acelerador em %
    bool        mil_on;            ///< true = acende check engine
};

// ============================================================
//  CATÁLOGO DE DTCs
// ============================================================

static const DtcEntry DTC_CATALOG[] = {

    // --------------------------------------------------------
    // P01xx – Combustível, Ar e Sensores
    // --------------------------------------------------------
    {0x0100,'P',DtcSystem::POWERTRAIN_FUEL_AIR, DtcSeverity::MODERADO,
     "Sensor MAF - Circuito Aberto",
     "Sensor MAF defeituoso ou fiacao interrompida"},

    {0x0101,'P',DtcSystem::POWERTRAIN_FUEL_AIR, DtcSeverity::MODERADO,
     "Sensor MAF - Faixa/Performance",
     "MAF contaminado ou vazamento de admissao"},

    {0x0102,'P',DtcSystem::POWERTRAIN_FUEL_AIR, DtcSeverity::MODERADO,
     "Sensor MAF - Sinal Baixo",
     "Curto no circuito do sensor MAF"},

    {0x0103,'P',DtcSystem::POWERTRAIN_FUEL_AIR, DtcSeverity::MODERADO,
     "Sensor MAF - Sinal Alto",
     "Tensao elevada no circuito do MAF"},

    {0x0105,'P',DtcSystem::POWERTRAIN_FUEL_AIR, DtcSeverity::MODERADO,
     "Sensor MAP - Circuito Aberto/Faixa",
     "Sensor de pressao do coletor com falha"},

    {0x0106,'P',DtcSystem::POWERTRAIN_FUEL_AIR, DtcSeverity::MODERADO,
     "Sensor MAP - Performance",
     "Vazamento no coletor ou sensor MAP defeituoso"},

    {0x0111,'P',DtcSystem::POWERTRAIN_FUEL_AIR, DtcSeverity::LEVE,
     "Sensor TPS - Faixa/Performance",
     "Sensor de posicao do acelerador descalibrado"},

    {0x0112,'P',DtcSystem::POWERTRAIN_FUEL_AIR, DtcSeverity::MODERADO,
     "Sensor TPS - Sinal Baixo",
     "Curto no circuito do sensor de acelerador"},

    {0x0113,'P',DtcSystem::POWERTRAIN_FUEL_AIR, DtcSeverity::MODERADO,
     "Sensor TPS - Sinal Alto",
     "Circuito aberto no sensor de acelerador"},

    {0x0115,'P',DtcSystem::POWERTRAIN_FUEL_AIR, DtcSeverity::MODERADO,
     "Sensor ECT - Circuito",
     "Sensor de temperatura do motor com falha"},

    {0x0116,'P',DtcSystem::POWERTRAIN_FUEL_AIR, DtcSeverity::MODERADO,
     "Sensor ECT - Faixa/Performance",
     "Temperatura fora do padrao esperado"},

    {0x0117,'P',DtcSystem::POWERTRAIN_FUEL_AIR, DtcSeverity::MODERADO,
     "Sensor ECT - Sinal Baixo",
     "Curto a massa no circuito do sensor de temperatura"},

    {0x0118,'P',DtcSystem::POWERTRAIN_FUEL_AIR, DtcSeverity::MODERADO,
     "Sensor ECT - Sinal Alto",
     "Circuito aberto no sensor de temperatura"},

    {0x0125,'P',DtcSystem::POWERTRAIN_FUEL_AIR, DtcSeverity::MODERADO,
     "Temperatura Insuficiente para Lambda Fechada",
     "Termostato preso aberto ou sensor ECT com erro"},

    // Sensor Lambda / O2
    {0x0130,'P',DtcSystem::POWERTRAIN_FUEL_AIR, DtcSeverity::MODERADO,
     "Sensor Lambda B1S1 - Circuito",
     "Sensor O2 pre-catalisador com falha eletrica"},

    {0x0131,'P',DtcSystem::POWERTRAIN_FUEL_AIR, DtcSeverity::MODERADO,
     "Sensor Lambda B1S1 - Sinal Baixo",
     "Sensor lambda saturado ou mistura enriquecida"},

    {0x0132,'P',DtcSystem::POWERTRAIN_FUEL_AIR, DtcSeverity::MODERADO,
     "Sensor Lambda B1S1 - Sinal Alto",
     "Sensor lambda com tensao elevada"},

    {0x0133,'P',DtcSystem::POWERTRAIN_FUEL_AIR, DtcSeverity::MODERADO,
     "Sensor Lambda B1S1 - Resposta Lenta",
     "Sensor lambda envelhecido ou contaminado"},

    {0x0134,'P',DtcSystem::POWERTRAIN_FUEL_AIR, DtcSeverity::MODERADO,
     "Sensor Lambda B1S1 - Sem Atividade",
     "Sensor lambda defeituoso ou fio cortado"},

    {0x0136,'P',DtcSystem::POWERTRAIN_FUEL_AIR, DtcSeverity::MODERADO,
     "Sensor Lambda B1S2 - Circuito",
     "Sensor O2 pos-catalisador com falha"},

    {0x0137,'P',DtcSystem::POWERTRAIN_FUEL_AIR, DtcSeverity::MODERADO,
     "Sensor Lambda B1S2 - Sinal Baixo",
     "Sensor lambda downstream com leitura baixa"},

    {0x0138,'P',DtcSystem::POWERTRAIN_FUEL_AIR, DtcSeverity::MODERADO,
     "Sensor Lambda B1S2 - Sinal Alto",
     "Sensor lambda downstream com leitura alta"},

    // Mistura Rica / Pobre
    {0x0171,'P',DtcSystem::POWERTRAIN_FUEL_AIR, DtcSeverity::MODERADO,
     "Mistura Empobrecida - Banco 1",
     "Vazamento de ar, injetor entupido ou MAF sujo"},

    {0x0172,'P',DtcSystem::POWERTRAIN_FUEL_AIR, DtcSeverity::MODERADO,
     "Mistura Enriquecida - Banco 1",
     "Injetor vazando, pressao de combustivel alta ou lambda lento"},

    {0x0174,'P',DtcSystem::POWERTRAIN_FUEL_AIR, DtcSeverity::MODERADO,
     "Mistura Empobrecida - Banco 2",
     "Vazamento de ar no banco 2 ou injetor entupido"},

    {0x0175,'P',DtcSystem::POWERTRAIN_FUEL_AIR, DtcSeverity::MODERADO,
     "Mistura Enriquecida - Banco 2",
     "Injetor com gotejamento no banco 2"},

    // --------------------------------------------------------
    // P02xx – Injetores de Combustível
    // --------------------------------------------------------
    {0x0200,'P',DtcSystem::POWERTRAIN_INJECTION, DtcSeverity::GRAVE,
     "Injetor - Falha Geral no Circuito",
     "Problema no circuito de injecao de combustivel"},

    {0x0201,'P',DtcSystem::POWERTRAIN_INJECTION, DtcSeverity::GRAVE,
     "Injetor Cilindro 1 - Circuito Aberto",
     "Injetor 1 defeituoso ou fiacao interrompida"},

    {0x0202,'P',DtcSystem::POWERTRAIN_INJECTION, DtcSeverity::GRAVE,
     "Injetor Cilindro 2 - Circuito Aberto",
     "Injetor 2 defeituoso ou fiacao interrompida"},

    {0x0203,'P',DtcSystem::POWERTRAIN_INJECTION, DtcSeverity::GRAVE,
     "Injetor Cilindro 3 - Circuito Aberto",
     "Injetor 3 defeituoso ou fiacao interrompida"},

    {0x0204,'P',DtcSystem::POWERTRAIN_INJECTION, DtcSeverity::GRAVE,
     "Injetor Cilindro 4 - Circuito Aberto",
     "Injetor 4 defeituoso ou fiacao interrompida"},

    {0x0261,'P',DtcSystem::POWERTRAIN_INJECTION, DtcSeverity::GRAVE,
     "Injetor Cil.1 - Curto a Massa",
     "Curto a massa no acionamento do injetor 1"},

    {0x0263,'P',DtcSystem::POWERTRAIN_INJECTION, DtcSeverity::GRAVE,
     "Injetor Cil.2 - Curto a Massa",
     "Curto a massa no acionamento do injetor 2"},

    // --------------------------------------------------------
    // P03xx – Ignição e Falhas de Combustão
    // --------------------------------------------------------
    {0x0300,'P',DtcSystem::POWERTRAIN_IGNITION, DtcSeverity::GRAVE,
     "Falha de Combustao - Multiplos Cilindros",
     "Velas, cabos ou bobinas defeituosas, compressao baixa"},

    {0x0301,'P',DtcSystem::POWERTRAIN_IGNITION, DtcSeverity::GRAVE,
     "Falha de Combustao - Cilindro 1",
     "Vela, cabo ou bobina do cilindro 1 com defeito"},

    {0x0302,'P',DtcSystem::POWERTRAIN_IGNITION, DtcSeverity::GRAVE,
     "Falha de Combustao - Cilindro 2",
     "Vela, cabo ou bobina do cilindro 2 com defeito"},

    {0x0303,'P',DtcSystem::POWERTRAIN_IGNITION, DtcSeverity::GRAVE,
     "Falha de Combustao - Cilindro 3",
     "Vela, cabo ou bobina do cilindro 3 com defeito"},

    {0x0304,'P',DtcSystem::POWERTRAIN_IGNITION, DtcSeverity::GRAVE,
     "Falha de Combustao - Cilindro 4",
     "Vela, cabo ou bobina do cilindro 4 com defeito"},

    {0x0320,'P',DtcSystem::POWERTRAIN_IGNITION, DtcSeverity::GRAVE,
     "Sensor CKP - Circuito",
     "Sensor de posicao do virabrequim com falha eletrica"},

    {0x0321,'P',DtcSystem::POWERTRAIN_IGNITION, DtcSeverity::GRAVE,
     "Sensor CKP - Faixa/Performance",
     "Reluctor danificado ou folga excessiva no sensor"},

    {0x0322,'P',DtcSystem::POWERTRAIN_IGNITION, DtcSeverity::CRITICO,
     "Sensor CKP - Sem Sinal",
     "Sensor de rotacao defeituoso - motor pode nao ligar"},

    {0x0340,'P',DtcSystem::POWERTRAIN_IGNITION, DtcSeverity::GRAVE,
     "Sensor CMP (Fase) - Circuito",
     "Sensor do comando de valvulas com falha"},

    {0x0341,'P',DtcSystem::POWERTRAIN_IGNITION, DtcSeverity::GRAVE,
     "Sensor CMP - Faixa/Performance",
     "Correia dentada com passo avancado ou atrasado"},

    {0x0351,'P',DtcSystem::POWERTRAIN_IGNITION, DtcSeverity::GRAVE,
     "Bobina de Ignicao Cil.1",
     "Bobina 1 com enrolamento aberto ou curto"},

    {0x0352,'P',DtcSystem::POWERTRAIN_IGNITION, DtcSeverity::GRAVE,
     "Bobina de Ignicao Cil.2",
     "Bobina 2 com enrolamento aberto ou curto"},

    {0x0353,'P',DtcSystem::POWERTRAIN_IGNITION, DtcSeverity::GRAVE,
     "Bobina de Ignicao Cil.3",
     "Bobina 3 com enrolamento aberto ou curto"},

    {0x0354,'P',DtcSystem::POWERTRAIN_IGNITION, DtcSeverity::GRAVE,
     "Bobina de Ignicao Cil.4",
     "Bobina 4 com enrolamento aberto ou curto"},

    // --------------------------------------------------------
    // P04xx – Emissões
    // --------------------------------------------------------
    {0x0400,'P',DtcSystem::POWERTRAIN_EMISSIONS, DtcSeverity::MODERADO,
     "Sistema EGR - Falha Geral",
     "Valvula EGR travada ou solenoide com falha"},

    {0x0401,'P',DtcSystem::POWERTRAIN_EMISSIONS, DtcSeverity::MODERADO,
     "EGR - Fluxo Insuficiente",
     "EGR entupida com carbono ou tubulacao obstruida"},

    {0x0402,'P',DtcSystem::POWERTRAIN_EMISSIONS, DtcSeverity::MODERADO,
     "EGR - Fluxo Excessivo",
     "EGR travada aberta"},

    {0x0420,'P',DtcSystem::POWERTRAIN_EMISSIONS, DtcSeverity::MODERADO,
     "Eficiencia do Catalisador Abaixo do Limiar - B1",
     "Catalisador saturado, danificado ou lambda lento"},

    {0x0421,'P',DtcSystem::POWERTRAIN_EMISSIONS, DtcSeverity::MODERADO,
     "Eficiencia do Catalisador Abaixo do Limiar - B2",
     "Catalisador banco 2 com eficiencia reduzida"},

    {0x0440,'P',DtcSystem::POWERTRAIN_EMISSIONS, DtcSeverity::LEVE,
     "Sistema EVAP - Falha Geral",
     "Vazamento no sistema de vapores de combustivel"},

    {0x0442,'P',DtcSystem::POWERTRAIN_EMISSIONS, DtcSeverity::LEVE,
     "Sistema EVAP - Pequeno Vazamento",
     "Tampa do combustivel mal fechada ou vedacao danificada"},

    {0x0446,'P',DtcSystem::POWERTRAIN_EMISSIONS, DtcSeverity::LEVE,
     "Sistema EVAP - Circuito de Ventilacao",
     "Solenoide de ventilacao do canister com falha"},

    {0x0455,'P',DtcSystem::POWERTRAIN_EMISSIONS, DtcSeverity::MODERADO,
     "Sistema EVAP - Grande Vazamento",
     "Tampa do combustivel ausente ou mangueira solta"},

    // --------------------------------------------------------
    // P05xx – Velocidade e Controles Elétricos
    // --------------------------------------------------------
    {0x0500,'P',DtcSystem::POWERTRAIN_SPEED, DtcSeverity::MODERADO,
     "Sensor VSS - Circuito",
     "Sensor de velocidade do veiculo com falha"},

    {0x0505,'P',DtcSystem::POWERTRAIN_SPEED, DtcSeverity::MODERADO,
     "Sistema IAC - Circuito",
     "Atuador de marcha lenta com falha ou obstruido"},

    {0x0506,'P',DtcSystem::POWERTRAIN_SPEED, DtcSeverity::LEVE,
     "RPM de Marcha Lenta Abaixo do Esperado",
     "IAC entupido ou vazamento no sistema de admissao"},

    {0x0507,'P',DtcSystem::POWERTRAIN_SPEED, DtcSeverity::LEVE,
     "RPM de Marcha Lenta Acima do Esperado",
     "Corpo de borboleta sujo ou IAC travado aberto"},

    {0x0562,'P',DtcSystem::POWERTRAIN_SPEED, DtcSeverity::GRAVE,
     "Tensao do Sistema - Baixa",
     "Bateria fraca, alternador com defeito ou mau contato"},

    {0x0563,'P',DtcSystem::POWERTRAIN_SPEED, DtcSeverity::MODERADO,
     "Tensao do Sistema - Alta",
     "Regulador de tensao do alternador com defeito"},

    // --------------------------------------------------------
    // P06xx – ECU / Computador de Bordo
    // --------------------------------------------------------
    {0x0600,'P',DtcSystem::POWERTRAIN_ECU, DtcSeverity::CRITICO,
     "Comunicacao Serial Interna da ECU",
     "Falha interna na ECU ou problema de comunicacao"},

    {0x0601,'P',DtcSystem::POWERTRAIN_ECU, DtcSeverity::CRITICO,
     "Erro de Memoria - ECU",
     "Memoria da ECU corrompida"},

    {0x0605,'P',DtcSystem::POWERTRAIN_ECU, DtcSeverity::CRITICO,
     "Erro ROM Interna - ECU",
     "Firmware da ECU corrompido"},

    {0x0606,'P',DtcSystem::POWERTRAIN_ECU, DtcSeverity::CRITICO,
     "Falha no Processador da ECU",
     "ECU com defeito interno - necessita substituicao"},

    // --------------------------------------------------------
    // P07xx – Transmissão Automática
    // --------------------------------------------------------
    {0x0700,'P',DtcSystem::POWERTRAIN_TRANS, DtcSeverity::GRAVE,
     "Transmissao - Falha Geral",
     "Problema na TCM (modulo da transmissao)"},

    {0x0706,'P',DtcSystem::POWERTRAIN_TRANS, DtcSeverity::MODERADO,
     "Sensor de Posicao do Cambio (TR) - Faixa",
     "Sensor do seletor de marchas com defeito"},

    {0x0715,'P',DtcSystem::POWERTRAIN_TRANS, DtcSeverity::GRAVE,
     "Sensor de Rotacao Eixo de Entrada",
     "Sensor de rotacao da transmissao automatica com falha"},

    {0x0730,'P',DtcSystem::POWERTRAIN_TRANS, DtcSeverity::GRAVE,
     "Relacao de Transmissao Incorreta",
     "Problema mecanico interno na caixa de cambio"},

    {0x0740,'P',DtcSystem::POWERTRAIN_TRANS, DtcSeverity::GRAVE,
     "Embreagem do Conversor de Torque - Travamento",
     "Embreagem do conversor com falha"},

    {0x0741,'P',DtcSystem::POWERTRAIN_TRANS, DtcSeverity::GRAVE,
     "Embreagem do Conversor - Patinagem",
     "Desgaste interno no conversor de torque"},

    {0x0750,'P',DtcSystem::POWERTRAIN_TRANS, DtcSeverity::GRAVE,
     "Solenoide de Cambio A - Circuito",
     "Solenoide de troca de marcha com defeito eletrico"},

    // --------------------------------------------------------
    // Bxxxx – Carroceria (Airbag / SRS)
    // --------------------------------------------------------
    {0x0001,'B',DtcSystem::BODY, DtcSeverity::CRITICO,
     "Airbag Motorista - Resistencia Fora do Padrao",
     "Airbag do motorista com circuito aberto"},

    {0x0002,'B',DtcSystem::BODY, DtcSeverity::CRITICO,
     "Airbag Passageiro - Resistencia Fora do Padrao",
     "Airbag do passageiro com falha no circuito"},

    {0x0010,'B',DtcSystem::BODY, DtcSeverity::CRITICO,
     "Modulo SRS/Airbag - Falha Interna",
     "Modulo de airbag com defeito interno"},

    {0x0011,'B',DtcSystem::BODY, DtcSeverity::GRAVE,
     "Pre-Tensionador do Cinto - Motorista",
     "Pre-tensionador do cinto do motorista com falha"},

    {0x0012,'B',DtcSystem::BODY, DtcSeverity::GRAVE,
     "Pre-Tensionador do Cinto - Passageiro",
     "Pre-tensionador do cinto do passageiro com falha"},

    // --------------------------------------------------------
    // Cxxxx – Chassi (ABS / ESP)
    // --------------------------------------------------------
    {0x0031,'C',DtcSystem::CHASSIS, DtcSeverity::GRAVE,
     "Sensor ABS Roda Dianteira Esquerda - Circuito",
     "Sensor ABS da roda DE com falha ou fiacao danificada"},

    {0x0034,'C',DtcSystem::CHASSIS, DtcSeverity::GRAVE,
     "Sensor ABS Roda Dianteira Direita - Circuito",
     "Sensor ABS da roda DD com falha ou fiacao danificada"},

    {0x0037,'C',DtcSystem::CHASSIS, DtcSeverity::GRAVE,
     "Sensor ABS Roda Traseira Esquerda - Circuito",
     "Sensor ABS da roda TE com falha ou fiacao danificada"},

    {0x0040,'C',DtcSystem::CHASSIS, DtcSeverity::GRAVE,
     "Sensor ABS Roda Traseira Direita - Circuito",
     "Sensor ABS da roda TD com falha ou fiacao danificada"},

    {0x0045,'C',DtcSystem::CHASSIS, DtcSeverity::GRAVE,
     "Modulo ABS - Falha Interna",
     "Modulo de controle do ABS com defeito"},

    {0x0110,'C',DtcSystem::CHASSIS, DtcSeverity::GRAVE,
     "Bomba do ABS - Circuito",
     "Motor da bomba hidraulica do ABS com falha"},

    // --------------------------------------------------------
    // Uxxxx – Rede CAN / Comunicação
    // --------------------------------------------------------
    {0x0001,'U',DtcSystem::NETWORK, DtcSeverity::GRAVE,
     "Barramento CAN - Falha de Comunicacao",
     "CAN bus interrompido, terminador ausente ou modulo com falha"},

    {0x0100,'U',DtcSystem::NETWORK, DtcSeverity::GRAVE,
     "Perda de Comunicacao com ECM/PCM",
     "ECU principal nao responde na rede CAN"},

    {0x0101,'U',DtcSystem::NETWORK, DtcSeverity::GRAVE,
     "Perda de Comunicacao com TCM (Cambio)",
     "Modulo do cambio automatico nao responde"},

    {0x0121,'U',DtcSystem::NETWORK, DtcSeverity::MODERADO,
     "Perda de Comunicacao com Modulo ABS/ESC",
     "Modulo ABS/ESP nao responde na rede CAN"},

    {0x0140,'U',DtcSystem::NETWORK, DtcSeverity::MODERADO,
     "Perda de Comunicacao com BCM",
     "Modulo de carroceria sem comunicacao"},

    {0x0155,'U',DtcSystem::NETWORK, DtcSeverity::LEVE,
     "Perda de Comunicacao com Painel de Instrumentos",
     "Cluster de instrumentos sem resposta na rede CAN"},
};

static const uint8_t DTC_CATALOG_SIZE =
    (uint8_t)(sizeof(DTC_CATALOG) / sizeof(DTC_CATALOG[0]));

// ============================================================
//  CENÁRIOS DE FALHA (12 prontos para uso)
// ============================================================

static const DtcScenario DTC_SCENARIOS[] = {

    {0x01, "Lambda Defeituoso",
     "Sensor O2 B1S1 com resposta lenta e circuito com falha. Motor oscila entre mistura rica e pobre.",
     3, {0x0130, 0x0133, 0x0171}, -50, 0, 0, true},

    {0x02, "Falha Combustao Total",
     "Motor falhando em todos os 4 cilindros. Tipico de bobina central defeituosa ou combustivel contaminado.",
     5, {0x0300, 0x0301, 0x0302, 0x0303, 0x0304}, -250, +5, -5, true},

    {0x03, "Catalisador Saturado",
     "Catalisador com eficiencia abaixo do limiar. Comum apos falhas de combustao nao corrigidas.",
     2, {0x0420, 0x0136}, 0, 0, 0, true},

    {0x04, "ABS - 4 Rodas",
     "Todos os sensores de velocidade de roda com falha simultanea. Indica problema no modulo ABS.",
     4, {0x4031, 0x4034, 0x4037, 0x4040}, 0, 0, 0, true},

    {0x05, "Cambio Automatico",
     "Transmissao com falha de comunicacao e relacao incorreta. Caixa pode travar em limp mode.",
     4, {0x0700, 0x0715, 0x0730, 0x0740}, -100, 0, 0, true},

    {0x06, "Airbag SRS Critico",
     "Sistema de airbag inoperante. Ambos os sacos com resistencia fora do padrao e modulo SRS com falha.",
     3, {0x8001, 0x8002, 0x8010}, 0, 0, 0, true},

    {0x07, "Rede CAN Offline",
     "Barramento CAN interrompido. ECU principal e modulo de cambio sem comunicacao.",
     3, {0xC001, 0xC100, 0xC101}, 0, 0, 0, true},

    {0x08, "MAF Defeituoso",
     "Fluximetro com circuito aberto causando mistura enriquecida e catalisador saturado.",
     3, {0x0100, 0x0172, 0x0420}, 0, 0, +8, true},

    {0x09, "Injetores 1 e 3",
     "Injetores dos cilindros 1 e 3 com circuito aberto. Motor perde potencia e vibra muito.",
     4, {0x0201, 0x0203, 0x0301, 0x0303}, -300, +5, -5, true},

    {0x0A, "Bateria/Alternador",
     "Tensao do sistema abaixo de 11V. Alternador nao carrega a bateria. Risco de desligamento.",
     2, {0x0562, 0xC001}, -100, 0, 0, true},

    {0x0B, "Vazamento EVAP",
     "Vazamento detectado no sistema de vapores de combustivel. Tampa do tanque mal fechada.",
     3, {0x0440, 0x0442, 0x0455}, 0, 0, 0, true},

    {0x0C, "ECU Critica",
     "Modulo de controle do motor com erros internos criticos. Motor pode ser desligado por seguranca.",
     3, {0x0600, 0x0605, 0x0606}, -500, 0, 0, true},
};

static const uint8_t DTC_SCENARIO_COUNT =
    (uint8_t)(sizeof(DTC_SCENARIOS) / sizeof(DTC_SCENARIOS[0]));

// ============================================================
//  FUNÇÕES UTILITÁRIAS
// ============================================================

/** Converte DtcEntry para o código wire OBD2 (bits 15-14 = prefixo). */
inline uint16_t dtcWireCode(const DtcEntry& e) {
    uint16_t w = e.code;
    switch (e.prefix) {
        case 'C': w |= 0x4000; break;
        case 'B': w |= 0x8000; break;
        case 'U': w |= 0xC000; break;
    }
    return w;
}

/** Busca DTC pelo código wire (com prefixo). Retorna nullptr se não encontrado. */
inline const DtcEntry* findDtc(uint16_t wireCode) {
    for (uint8_t i = 0; i < DTC_CATALOG_SIZE; i++)
        if (dtcWireCode(DTC_CATALOG[i]) == wireCode) return &DTC_CATALOG[i];
    return nullptr;
}

/** Busca cenário pelo ID. Retorna nullptr se não encontrado. */
inline const DtcScenario* findScenario(uint8_t id) {
    for (uint8_t i = 0; i < DTC_SCENARIO_COUNT; i++)
        if (DTC_SCENARIOS[i].id == id) return &DTC_SCENARIOS[i];
    return nullptr;
}

/**
 * Converte DtcEntry para string OBD2 padrão (ex: P0171).
 * @param entry  DtcEntry válido
 * @param buf    Buffer destino — mínimo 7 bytes
 */
inline void dtcToString(const DtcEntry& entry, char* buf) {
    snprintf(buf, 7, "%c%02X%02X",
             entry.prefix,
             (entry.code >> 8) & 0xFF,
              entry.code        & 0xFF);
}
