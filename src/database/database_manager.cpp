#include "database_manager.h"
#include "../common/logger.h"
#include <stdexcept>
#include <random>
#include <optional>
#include <regex>
#include <sstream> // Para std::stringstream
#include <locale>  // Para std::locale::classic()

// Construtor
DatabaseManager::DatabaseManager() {
    // Inicializa com valores default seguros
    global_config_cache_.global_latency_ms = 0;
    global_config_cache_.simulate_network_instability = false;
    global_config_cache_.error_probability = 0.0;
    global_config_cache_.cache_refresh_interval_sec = 30;
}

// Destrutor
DatabaseManager::~DatabaseManager() {
    close();
}

// Conexão inicial
void DatabaseManager::init(const std::string &conninfo) { {
        std::lock_guard<std::mutex> lock(db_mutex_);
        conninfo_ = conninfo;
        is_initialized_ = true;
        connect_if_needed();
    }
    // Carga inicial do cache
    refreshCache();
}

// Fechar conexão
void DatabaseManager::close() {
    cache_thread_running_ = false;
    if (cache_refresh_thread_.joinable()) {
        cache_refresh_thread_.join();
    }
    std::lock_guard<std::mutex> lock(db_mutex_);
    if (conn_ && conn_->is_open()) {
        conn_.reset();
        LOG_INFO("Conexão com o PostgreSQL encerrada.");
    }
    connected_ = false;
}

// Conectar se necessário
void DatabaseManager::connect_if_needed() {
    if (!conn_ || !conn_->is_open()) {
        try {
            conn_ = std::make_unique<pqxx::connection>(conninfo_);
            conn_->set_client_encoding("UTF8"); // Garante UTF8
            if (conn_->is_open()) {
                connected_ = true;
                LOG_INFO("Conexão com o PostgreSQL estabelecida com sucesso: {}", conn_->dbname());
            } else {
                LOG_ERROR("Falha ao abrir a conexão com o PostgreSQL.");
                connected_ = false;
            }
        } catch (const std::exception &e) {
            LOG_ERROR("Erro ao tentar conectar ao PostgreSQL: {}", e.what());
            connected_ = false;
        }
    }
}

// Iniciar thread de refresh do cache
void DatabaseManager::startCacheRefreshThread() {
    if (cache_thread_running_) return;

    cache_thread_running_ = true;
    cache_refresh_thread_ = std::thread([this]() {
        while (cache_thread_running_) {
            int interval = getGlobalConfig().cache_refresh_interval_sec;
            if (interval <= 0) interval = 30;

            std::this_thread::sleep_for(std::chrono::seconds(interval));
            if (cache_thread_running_) {
                LOG_INFO("Iniciando refresh periódico do cache...");
                refreshCache();
            }
        }
    });
}

// --- Logging ---
void DatabaseManager::logTransaction(const IsoMessage &msg, const std::string &response_code, const std::string &status,
                                     const std::string &issuer_id) {
    if (!is_initialized_) {
        LOG_WARN("DatabaseManager não inicializado, não é possível logar transação.");
        return;
    }

    try {
        std::lock_guard<std::mutex> db_lock(db_mutex_);
        connect_if_needed();
        if (!conn_ || !conn_->is_open()) {
            LOG_ERROR("Conexão com o banco de dados não está ativa para logTransaction.");
            return;
        }

        pqxx::work txn(*conn_);

        std::string stan = msg.has_field(11) ? msg.get_field(11) : "";
        std::string pan_masked = msg.has_field(2) ? msg.mask_pan() : "";
        std::string mti = msg.mti;

        std::optional<double> amount;
        if (msg.has_field(4)) {
            // CORREÇÃO FINAL E DEFINITIVA: O campo 4 é o valor em centavos.
            // 1. Converte a string (ex: "199" para 1.99) para double de forma locale-safe.
            std::stringstream ss(msg.get_field(4));
            ss.imbue(std::locale::classic());
            double amount_in_cents;
            ss >> amount_in_cents;
            // 2. Divide por 100.0 para obter o valor na unidade principal.
            amount = amount_in_cents / 100.0;
        }

        std::string processing_code = msg.has_field(3) ? msg.get_field(3) : "";
        std::string terminal_id = msg.has_field(41) ? msg.get_field(41) : "";
        std::string merchant_id = msg.has_field(42) ? msg.get_field(42) : "";

        std::optional<std::string> db_issuer_id;
        if (msg.mti != "0800" && issuer_id != "UNKNOWN") {
            db_issuer_id = issuer_id;
        }

        txn.exec_params(
            "INSERT INTO visa.visanet_network_log "
            "(stan, mti, pan_masked, amount, processing_code, terminal_id, merchant_id, issuer_id, response_code, status, request_timestamp) "
            "VALUES ($1, $2, $3, $4, $5, $6, $7, $8, $9, $10, NOW())",
            stan, mti, pan_masked, amount, processing_code, terminal_id, merchant_id, db_issuer_id, response_code,
            status
        );
        txn.commit();
        LOG_DEBUG("Transação salva no network_log (STAN: {})", stan);
    } catch (const std::exception &e) {
        LOG_ERROR("Falha ao persistir log da transação no PostgreSQL: {}", e.what());
    }
}

// --- Métodos de Consulta Públicos ---

GlobalConfig DatabaseManager::getGlobalConfig() {
    std::shared_lock<std::shared_mutex> lock(config_cache_mutex_);
    return global_config_cache_;
}

std::optional<SimulatedCard> DatabaseManager::lookupCard(const std::string &pan) {
    // Limpa o PAN de caracteres não numéricos (como 'f' do BCD) para a consulta
    std::string clean_pan = std::regex_replace(pan, std::regex("[^0-9]"), "");

    {
        std::shared_lock<std::shared_mutex> lock(card_cache_mutex_);
        auto it = card_cache_.find(clean_pan);
        if (it != card_cache_.end()) {
            LOG_DEBUG("Cache hit para o cartão: {}", clean_pan);
            return it->second;
        }
    }

    LOG_DEBUG("Cache miss para o cartão: {}. Consultando o banco...", clean_pan);
    auto card = loadSimulatedCardFromDb(clean_pan);
    if (card) {
        std::unique_lock<std::shared_mutex> lock(card_cache_mutex_);
        card_cache_[clean_pan] = *card;
    } else {
        string masked_pan = clean_pan;
        if (masked_pan.length() >= 10) {
            for (size_t i = 6; i < masked_pan.length() - 4; ++i) {
                masked_pan[i] = '*';
            }
        }
        LOG_WARN("Cartão não encontrado no banco: {}", masked_pan);
    }
    return card;
}

std::optional<BinRoute> DatabaseManager::lookupRoute(const std::string &bin) {
    std::shared_lock<std::shared_mutex> lock(bin_cache_mutex_);
    auto it = bin_route_cache_.find(bin);
    if (it != bin_route_cache_.end()) {
        return it->second;
    }
    LOG_WARN("BIN {} não encontrado no cache de rotas.", bin);
    return std::nullopt;
}

std::optional<IssuerConfig> DatabaseManager::lookupIssuerConfig(const std::string &issuer_id) {
    std::shared_lock<std::shared_mutex> lock(issuer_config_mutex_);
    auto it = issuer_config_cache_.find(issuer_id);
    if (it != issuer_config_cache_.end()) {
        return it->second;
    }
    return std::nullopt;
}

std::optional<PreviousTransactionResult> DatabaseManager::getPreviousTransactionResult(const std::string &unique_key) {
    try {
        std::lock_guard<std::mutex> db_lock(db_mutex_);
        connect_if_needed();
        if (!conn_ || !conn_->is_open()) {
            LOG_ERROR("Conexão com o banco de dados não está ativa para getPreviousTransactionResult.");
            return std::nullopt;
        }

        std::string stan, pan;
        size_t pos1 = unique_key.find('_');
        if (pos1 != std::string::npos) {
            stan = unique_key.substr(0, pos1);
            size_t pos2 = unique_key.find('_', pos1 + 1);
            if (pos2 != std::string::npos) {
                pan = unique_key.substr(pos1 + 1, pos2 - pos1 - 1);
            }
        }

        if (stan.empty() || pan.empty()) return std::nullopt;

        std::string masked_pan = pan;
        if (masked_pan.length() >= 10) {
            for (size_t i = 6; i < masked_pan.length() - 4; ++i) {
                masked_pan[i] = '*';
            }
        }

        pqxx::work txn(*conn_);

        pqxx::result r = txn.exec_params(
            "SELECT response_code, status FROM visa.visanet_network_log "
            "WHERE stan = $1 AND pan_masked = $2 AND processed_at >= NOW() - INTERVAL '5 minutes' "
            "ORDER BY id DESC LIMIT 1", stan, masked_pan);

        if (!r.empty()) {
            PreviousTransactionResult res;
            res.response_code = r[0]["response_code"].is_null() ? "" : r[0]["response_code"].as<std::string>();
            res.status = r[0]["status"].is_null() ? "" : r[0]["status"].as<std::string>();
            return res;
        }
    } catch (const std::exception &e) {
        LOG_ERROR("Erro ao carregar transação anterior do PostgreSQL: {}", e.what());
    }
    return std::nullopt;
}

// --- Métodos de Atualização ---

bool DatabaseManager::updateCardBalance(const std::string &pan, double amount) {
    // CORREÇÃO: Garante que o PAN usado na query UPDATE esteja limpo.
    std::string clean_pan = std::regex_replace(pan, std::regex("[^0-9]"), "");
    if (clean_pan.empty()) return false;

    try {
        std::lock_guard<std::mutex> db_lock(db_mutex_);
        connect_if_needed();
        if (!conn_ || !conn_->is_open()) {
            LOG_ERROR("Conexão com o banco de dados não está ativa para updateCardBalance.");
            return false;
        }

        pqxx::work txn(*conn_);
        pqxx::result r = txn.exec_params(
            "UPDATE visa.visanet_simulated_cards "
            "SET balance = balance - $1, daily_spent = daily_spent + $1 "
            "WHERE pan = $2 AND balance >= $1 AND is_active = TRUE",
            amount, clean_pan); // Usa o PAN limpo
        txn.commit();

        if (r.affected_rows() > 0) {
            std::unique_lock<std::shared_mutex> lock(card_cache_mutex_);
            card_cache_.erase(clean_pan); // Invalida o cache com o PAN limpo
            return true;
        }
    } catch (const std::exception &e) {
        LOG_ERROR("Erro ao atualizar saldo do cartão no PostgreSQL: {}", e.what());
    }
    return false;
}

bool DatabaseManager::refundCardBalance(const std::string &pan, double amount) {
    // CORREÇÃO: Garante que o PAN usado na query UPDATE esteja limpo.
    std::string clean_pan = std::regex_replace(pan, std::regex("[^0-9]"), "");
    if (clean_pan.empty()) return false;

    try {
        std::lock_guard<std::mutex> db_lock(db_mutex_);
        connect_if_needed();
        if (!conn_ || !conn_->is_open()) {
            LOG_ERROR("Conexão com o banco de dados não está ativa para refundCardBalance.");
            return false;
        }

        pqxx::work txn(*conn_);
        pqxx::result r = txn.exec_params(
            "UPDATE visa.visanet_simulated_cards "
            "SET balance = balance + $1, daily_spent = daily_spent - $1 "
            "WHERE pan = $2",
            amount, clean_pan); // Usa o PAN limpo
        txn.commit();

        if (r.affected_rows() > 0) {
            std::unique_lock<std::shared_mutex> lock(card_cache_mutex_);
            card_cache_.erase(clean_pan); // Invalida o cache com o PAN limpo
            return true;
        }
    } catch (const std::exception &e) {
        LOG_ERROR("Erro ao reembolsar saldo do cartão no PostgreSQL: {}", e.what());
    }
    return false;
}


// --- Métodos Privados de Cache ---

void DatabaseManager::refreshCache() {
    try {
        std::lock_guard<std::mutex> db_lock(db_mutex_);
        connect_if_needed();
        if (!conn_ || !conn_->is_open()) {
            LOG_WARN("Não foi possível atualizar o cache, banco de dados inacessível.");
            return;
        }

        pqxx::nontransaction txn(*conn_);
        pqxx::result r_config = txn.exec(
            "SELECT global_latency_ms, simulate_network_instability, error_probability, cache_refresh_interval_sec FROM visa.visanet_config ORDER BY id DESC LIMIT 1");
        if (!r_config.empty()) {
            GlobalConfig new_config;
            new_config.global_latency_ms = r_config[0]["global_latency_ms"].is_null() ? 0 : r_config[0]["global_latency_ms"].as<int>();
            new_config.simulate_network_instability = r_config[0]["simulate_network_instability"].is_null() ? false : r_config[0]["simulate_network_instability"].as<bool>();
            new_config.error_probability = r_config[0]["error_probability"].is_null() ? 0.0 : r_config[0]["error_probability"].as<double>();
            new_config.cache_refresh_interval_sec = r_config[0]["cache_refresh_interval_sec"].is_null() ? 30 : r_config[0]["cache_refresh_interval_sec"].as<int>();

            std::unique_lock<std::shared_mutex> lock(config_cache_mutex_);
            global_config_cache_ = new_config;
            LOG_INFO("Configuração global carregada do banco de dados.");
        }

        pqxx::result r_bins = txn.exec(
            "SELECT t1.bin_prefix, t1.issuer_id, t2.handler_type, t1.description FROM visa.visanet_bin_table t1 "
            "JOIN visa.visanet_issuers t2 ON t1.issuer_id = t2.issuer_id");

        std::unordered_map<std::string, BinRoute> new_bin_cache;
        for (auto row: r_bins) {
            BinRoute route;
            route.bin_prefix = row["bin_prefix"].is_null() ? "" : row["bin_prefix"].as<std::string>();
            route.issuer_id = row["issuer_id"].is_null() ? "" : row["issuer_id"].as<std::string>();
            route.handler_type = row["handler_type"].is_null() ? "" : row["handler_type"].as<std::string>();
            if (!row["description"].is_null()) {
                route.description = row["description"].as<std::string>();
            }
            new_bin_cache[route.bin_prefix] = route;
        } {
            std::unique_lock<std::shared_mutex> lock(bin_cache_mutex_);
            bin_route_cache_ = std::move(new_bin_cache);
        }
        LOG_INFO("{} rotas de BIN carregadas para o cache.", bin_route_cache_.size());

        pqxx::result r_issuers = txn.exec(
            "SELECT issuer_id, handler_type, endpoint_config FROM visa.visanet_issuers");

        std::unordered_map<std::string, IssuerConfig> new_issuer_config_cache;
        for (auto row: r_issuers) {
            IssuerConfig config;
            config.issuer_id = row["issuer_id"].is_null() ? "" : row["issuer_id"].as<std::string>();
            config.handler_type = row["handler_type"].is_null() ? "" : row["handler_type"].as<std::string>();
            if (!row["endpoint_config"].is_null()) {
                config.endpoint_config = nlohmann::json::parse(row["endpoint_config"].as<std::string>());
            }
            new_issuer_config_cache[config.issuer_id] = config;
        } {
            std::unique_lock<std::shared_mutex> lock(issuer_config_mutex_);
            issuer_config_cache_ = std::move(new_issuer_config_cache);
        }
        LOG_INFO("{} configurações de emissor carregadas para o cache.", issuer_config_cache_.size()); {
            std::unique_lock<std::shared_mutex> lock(card_cache_mutex_);
            card_cache_.clear();
        }
    } catch (const std::exception &e) {
        LOG_ERROR("Erro ao atualizar cache do PostgreSQL: {}", e.what());
    }
}

std::optional<SimulatedCard> DatabaseManager::loadSimulatedCardFromDb(const std::string &pan) {
    try {
        std::lock_guard<std::mutex> db_lock(db_mutex_);
        connect_if_needed();
        if (!conn_ || !conn_->is_open()) {
            LOG_ERROR("Conexão com o banco de dados não está ativa para loadSimulatedCardFromDb.");
            return std::nullopt;
        }

        pqxx::work txn(*conn_);
        pqxx::result r = txn.exec_params(
            "SELECT pan, issuer_id, is_active, balance, daily_limit, daily_spent, force_response_code, simulation_delay "
            "FROM visa.visanet_simulated_cards WHERE pan = $1", pan);

        if (!r.empty()) {
            SimulatedCard card;
            const auto &row = r[0];

            // Protegendo TODOS os campos contra NULL
            card.pan = row["pan"].is_null() ? "" : row["pan"].as<std::string>();
            card.issuer_id = row["issuer_id"].is_null() ? "UNKNOWN" : row["issuer_id"].as<std::string>();
            card.is_active = row["is_active"].is_null() ? true : row["is_active"].as<bool>();
            card.balance = row["balance"].is_null() ? 0.0 : row["balance"].as<double>();

            // Lendo do banco ou setando zero
            card.daily_limit = row["daily_limit"].is_null() ? 0.0 : row["daily_limit"].as<double>();
            card.daily_spent = row["daily_spent"].is_null() ? 0.0 : row["daily_spent"].as<double>();

            if (!row["force_response_code"].is_null()) {
                card.force_response_code = row["force_response_code"].as<std::string>();
            }
            if (!row["simulation_delay"].is_null()) {
                card.simulation_delay_ms = row["simulation_delay"].as<int>();
            }

            return card;
        }
    } catch (const std::exception &e) {
        LOG_ERROR("Erro ao carregar cartão simulado do PostgreSQL: {}", e.what());
    }
    return std::nullopt;
}