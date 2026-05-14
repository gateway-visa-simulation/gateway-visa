#pragma once
#include "../iso8583/iso_message.h"
#include <string>
#include <map>
#include <functional>
#include <mutex>
#include <optional>
#include "../database/db_structures.h" // Inclui as estruturas de dados do DB

using namespace std;

// O handler agora recebe a mensagem e a string de configuração do endpoint (JSON)
using RouteHandler = function<string(const IsoMessage&, const string& endpoint_config)>;

// A lógica de roteamento agora retorna um optional de BinRoute
using RoutingLogic = function<optional<BinRoute>(const IsoMessage&)>;

class Router {
public:
    static Router& get_instance();

    // Define a função lambda que será usada para buscar a rota (BIN -> Issuer)
    void setRoutingLogic(RoutingLogic logic);

    // Registra um handler para um tipo de emissor (ex: "KAFKA_BANK", "LOCAL_BANK")
    void registerIssuerHandler(const string& handler_type, RouteHandler handler);

    // Roteia a mensagem principal consultando o DB e chamando o handler
    string route(const IsoMessage& msg);

private:
    Router() = default;

    RoutingLogic routing_logic_; // Função para buscar a rota no DB
    map<string, RouteHandler> issuer_handlers_; // Mapeia handler_type para a função de processamento
    mutex mtx_;
};
