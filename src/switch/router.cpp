#include "router.h"
#include "../common/logger.h"
#include "../switch/circuit_breaker.h"
#include "../database/database_manager.h"
#include <iostream>

using namespace std;

Router& Router::get_instance() {
    static Router instance;
    return instance;
}

void Router::setRoutingLogic(RoutingLogic logic) {
    routing_logic_ = logic;
}

void Router::registerIssuerHandler(const string& handler_type, RouteHandler handler) {
    lock_guard<mutex> lock(mtx_);
    issuer_handlers_[handler_type] = handler;
}

string Router::route(const IsoMessage& msg) {
    if (!routing_logic_) {
        LOG_ERROR("Lógica de roteamento não foi configurada no Router!");
        return "96"; // System Malfunction
    }

    if (!msg.has_field(2)) {
        LOG_WARN("Mensagem sem PAN (DE2) para extrair BIN.");
        return "14"; // Invalid Card Number
    }

    string pan = msg.get_field(2);
    if (pan.length() < 6) {
        LOG_WARN("PAN muito curto para extrair BIN: {}", pan);
        return "14"; // Invalid Card Number
    }

    // 1. Chama a lógica de roteamento (que consulta o DatabaseManager)
    auto route_info_opt = routing_logic_(msg);

    if (!route_info_opt) {
        string bin = pan.substr(0, 6);
        LOG_WARN("Nenhuma rota de BIN configurada no banco de dados para o BIN: {}", bin);
        return "12"; // Invalid Transaction (Sem rota)
    }

    const auto& route_info = *route_info_opt;

    // Proteção com Circuit Breaker baseada no issuer_id
    if (!CircuitBreaker::get().can_request(route_info.issuer_id)) {
        LOG_WARN("Circuit breaker ABERTO para emissor {}", route_info.issuer_id);
        return "91"; // Issuer inoperative
    }

    // 2. Busca a configuração detalhada do emissor no banco
    auto issuer_config_opt = DatabaseManager::getInstance().lookupIssuerConfig(route_info.issuer_id);

    string handler_type = "IssuerLocal"; // Fallback de segurança
    string endpoint_config_str = "";

    if (issuer_config_opt) {
        handler_type = issuer_config_opt->handler_type;
        if (!issuer_config_opt->endpoint_config.empty()) {
            endpoint_config_str = issuer_config_opt->endpoint_config.dump();
        }
    } else {
        LOG_WARN("Configuração do emissor '{}' não encontrada. Usando handler padrão '{}'.", route_info.issuer_id, handler_type);
    }

    // 3. Procura o handler de processamento associado ao handler_type
    RouteHandler handler;
    {
        lock_guard<mutex> lock(mtx_);
        auto it = issuer_handlers_.find(handler_type);
        if (it != issuer_handlers_.end()) {
            handler = it->second;
        }
    }

    if (handler) {
        LOG_INFO("Roteando para Issuer ID: '{}' | Usando Handler Type: '{}'", route_info.issuer_id, handler_type);
        try {
            // Executa a lógica do emissor (passando a mensagem e as configs do DB em JSON)
            string res = handler(msg, endpoint_config_str);

            if (res == "91") {
                CircuitBreaker::get().record_failure(route_info.issuer_id);
            } else {
                CircuitBreaker::get().record_success(route_info.issuer_id);
            }
            return res;
        } catch (const exception& e) {
            LOG_ERROR("Exceção ao executar handler para emissor '{}': {}", route_info.issuer_id, e.what());
            CircuitBreaker::get().record_failure(route_info.issuer_id);
            return "91"; // System Error
        }
    }

    LOG_ERROR("Handler não registrado em código para o tipo de roteamento: '{}'", handler_type);
    return "91"; // Issuer inoperative
}
