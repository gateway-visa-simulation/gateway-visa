#pragma once
#include <string>
#include <memory>
#include <optional>
#include <unordered_map>
#include <shared_mutex>
#include <mutex>
#include <pqxx/pqxx>
#include <thread> // Para thread
#include <atomic> // Para atomic
#include <chrono> // Para chrono

#include "db_structures.h" // Inclui as estruturas de dados do arquivo separado
#include "../iso8583/iso_message.h" // Para logTransaction
#include "../common/logger.h" // Para LOG_ERROR, LOG_INFO, etc.

using namespace std;

struct BinRouteInfo {
    string bin_prefix;
    string issuer_id;
    string handler_type; // Mapeia para o registro de Handlers no main.cpp
    bool is_online;
};

class DatabaseManager {
public:
    static DatabaseManager& getInstance() {
        static DatabaseManager instance;
        return instance;
    }

    // Inicializa a conexão com o banco e carrega o cache inicial
    void init(const string& conninfo);

    // Fecha a conexão explicitamente
    void close();

    // Inicia a thread de refresh do cache periodicamente
    void startCacheRefreshThread();

    // --- Métodos de Consulta ---

    // Retorna a configuração global (do cache em memória)
    GlobalConfig getGlobalConfig();

    // Busca a rota do BIN (do cache em memória)
    optional<BinRoute> lookupRoute(const string& bin); // Agora retorna BinRoute

    // Busca o cartão (Lazy Loading: cache -> banco de dados)
    optional<SimulatedCard> lookupCard(const string& pan);

    // Busca a configuração de um emissor (do cache em memória)
    optional<IssuerConfig> lookupIssuerConfig(const string& issuer_id);

    // Busca o resultado de uma transação anterior (Idempotência)
    optional<PreviousTransactionResult> getPreviousTransactionResult(const string& unique_key);

    // --- Métodos de Atualização ---
    bool updateCardBalance(const string& pan, double amount);
    bool refundCardBalance(const string& pan, double amount);

    // --- Logging ---
    // Loga a transação usando consultas parametrizadas para evitar SQL Injection
    void logTransaction(const IsoMessage& msg, const string& response_code, const string& status, const string& issuer_id = "UNKNOWN");

    // Recarrega todas as configurações e tabelas de cache
    void refreshCache();

private:
    DatabaseManager();
    ~DatabaseManager();
    DatabaseManager(const DatabaseManager&) = delete;
    DatabaseManager& operator=(const DatabaseManager&) = delete;

    void connect_if_needed();

    unique_ptr<pqxx::connection> conn_;
    string conninfo_;
    atomic<bool> is_initialized_ = false;
    atomic<bool> connected_ = false;

    // Mutex global para o banco de dados (protege a conexão e operações críticas)
    mutex db_mutex_;

    // --- Caches e Mutexes Específicos ---
    GlobalConfig global_config_cache_;
    shared_mutex config_cache_mutex_;

    unordered_map<string, BinRoute> bin_route_cache_; // Agora armazena BinRoute
    shared_mutex bin_cache_mutex_;

    unordered_map<string, SimulatedCard> card_cache_; // Cartões são lazy-loaded
    shared_mutex card_cache_mutex_;

    unordered_map<string, IssuerConfig> issuer_config_cache_;
    shared_mutex issuer_config_mutex_;

    // Thread de refresh do cache
    atomic<bool> cache_thread_running_ = false;
    thread cache_refresh_thread_;

    // Métodos privados para carregar dados do DB
    void loadGlobalConfigFromDb();
    void loadAllBinRoutesFromDb();
    void loadAllIssuerConfigsFromDb();
    optional<SimulatedCard> loadSimulatedCardFromDb(const string& pan);
};
