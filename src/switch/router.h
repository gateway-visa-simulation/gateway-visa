#pragma once
#include "../iso8583/iso_message.h"
#include <string>
#include <map>
#include <functional>
#include <mutex>
#include <optional>
#include "../database/db_structures.h" // Inclui as estruturas de dados do DB

// O handler agora recebe a mensagem e a string de configuração do endpoint (JSON)
using RouteHandler = std::function<std::string(const IsoMessage&, const std::string& endpoint_config)>;

// A lógica de roteamento agora retorna um optional de BinRoute
using RoutingLogic = std::function<std::optional<BinRoute>(const IsoMessage&)>;

class Router {
public:
    static Router& get_instance();

    // Define a função lambda que será usada para buscar a rota (BIN -> Issuer)
    void setRoutingLogic(RoutingLogic logic);

    // Registra um handler para um tipo de emissor (ex: "KAFKA_BANK", "LOCAL_BANK")
    void registerIssuerHandler(const std::string& handler_type, RouteHandler handler);

    // Roteia a mensagem principal consultando o DB e chamando o handler
    std::string route(const IsoMessage& msg);

private:
    Router() = default;

    RoutingLogic routing_logic_; // Função para buscar a rota no DB
    std::map<std::string, RouteHandler> issuer_handlers_; // Mapeia handler_type para a função de processamento
    std::mutex mtx_;
};
