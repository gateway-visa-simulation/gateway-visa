#pragma once
#include <string>
#include <optional>
#include <nlohmann/json.hpp> // Para JSONB

// Estrutura para o resultado de uma transação anterior para idempotência
struct PreviousTransactionResult {
    std::string response_code;
    std::string status;
};

// Estrutura para visa.visanet_config
struct GlobalConfig {
    int global_latency_ms = 0;
    bool simulate_network_instability = false;
    double error_probability = 0.0; // 0.00 a 1.00
    int cache_refresh_interval_sec = 30;
};

// Estrutura unificada para informações de roteamento.
// Contém os dados do JOIN entre visanet_bin_table e visanet_issuers.
struct BinRoute {
    std::string bin_prefix;
    std::string issuer_id;
    std::string handler_type; // Ex: "IssuerKafka", "IssuerLocal"
    std::string description;
};

// Estrutura para visa.visanet_simulated_cards
struct SimulatedCard {
    std::string pan;
    std::string issuer_id;
    bool is_active = true;
    double balance = 0.0;
    double daily_limit = 0.0;
    double daily_spent = 0.0;
    std::optional<std::string> force_response_code; // RC '05', '51', etc.
    std::optional<std::string> pin_block_key; // Chave para simular PIN Block
    int simulation_delay_ms = 0;
};

// Estrutura para visa.visanet_issuers
struct IssuerConfig {
    std::string issuer_id;
    std::string handler_type;
    nlohmann::json endpoint_config; // JSONB para configurações específicas
};
