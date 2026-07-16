#include <iostream>
#include <vector>
#include <thread>
#include <asio.hpp>
#include "common/logger.h"
#include "common/config.h"
#include "common/thread_pool.h"
#include "network/tcp_server.h"
#include "switch/router.h"
#include "issuer/issuer_service.h"
#include "metrics/metrics_registry.h"
#include "mapper/iso_to_json.h"
#include "mapper/json_to_iso.h"
#include "security/hsm.h"
#include "database/database_manager.h"
#include <chrono>
#include <nlohmann/json.hpp>
#include <sstream>
#include <locale>
#include "lockfree/lockfree_queue.h"

void metrics_printer_thread() {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(10));
        MetricsRegistry::get().print_metrics();
    }
}

// Thread para atualizar o cache periodicamente
void cache_refresh_thread(int interval_sec) {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(interval_sec));
        try {
            DatabaseManager::getInstance().refreshCache();
            LOG_INFO("Cache atualizado periodicamente.");
        } catch (const std::exception &e) {
            LOG_ERROR("Erro ao atualizar o cache: {}", e.what());
        }
    }
}

int main() {
    try {
        Logger::init();
        LOG_INFO("Starting Visa Gateway Simulator (Refactored PostgreSQL Edition)...");

        Config app_config = Config::load("/mnt/c/Users/chico/CLionProjects/gateway-visa/config.json");

        DatabaseManager::getInstance().init(app_config.db_connection_string);
        HSM::getInstance().init();

        // Lock-free queue initialization
        global_lockfree_queue = create_boost_lockfree_queue(32768);

        // --- Registro da Lógica de Roteamento Dinâmico via Banco de Dados ---
        Router::get_instance().setRoutingLogic([](const IsoMessage &msg) -> std::optional<BinRoute> {
            // Se for uma mensagem de rede, não há rota a ser determinada.
            if (msg.mti == "0800") {
                return std::nullopt;
            }

            if (!msg.has_field(2)) {
                LOG_WARN("Mensagem transacional sem PAN (DE2) para roteamento.");
                return std::nullopt;
            }

            std::string pan = msg.get_field(2);
            if (pan.length() < 6) {
                LOG_WARN("PAN inválido para extrair BIN: {}", pan);
                return std::nullopt;
            }
            std::string bin = pan.substr(0, 6);

            auto route_info_opt = DatabaseManager::getInstance().lookupRoute(bin);
            if (route_info_opt) {
                return route_info_opt;
            }
            return std::nullopt;
        });

        // --- Registro dos Handlers ---
        Router::get_instance().registerIssuerHandler("KAFKA", [](const IsoMessage &msg, const std::string &) -> std::string {
            std::string pan = msg.get_field(2);
            auto card_opt = DatabaseManager::getInstance().lookupCard(pan);

            if (!card_opt) {
                LOG_WARN("Cartão não encontrado no banco para o handler KAFKA: {}", msg.mask_pan());
                return "14";
            }

            const auto& card = *card_opt;

            if (card.force_response_code.has_value() && !card.force_response_code.value().empty()) {
                LOG_INFO("[KAFKA HANDLER -> LOCK-FREE] Aplicando RC forçado '{}' para o cartão {}", card.force_response_code.value(), msg.mask_pan());
                return card.force_response_code.value();
            }

            double amount = 0.0;
            if (msg.has_field(4)) {
                std::stringstream ss(msg.get_field(4));
                ss.imbue(std::locale::classic());
                ss >> amount;
                amount = amount / 100.0;
            }

            if (!DatabaseManager::getInstance().updateCardBalance(pan, amount)) {
                LOG_WARN("Transação negada: Saldo insuficiente ou cartão inativo para o PAN: {}", pan.substr(0,6));
                return "51";
            }

            // Build JSON payload and enqueue to lock-free queue
            try {
                nlohmann::json original_json = IsoToJsonMapper::mapIsoToJson(msg);
                std::string plaintext_payload = original_json.dump();
                std::string encrypted_payload_hex = HSM::getInstance().encryptData(plaintext_payload);

                nlohmann::json envelope;
                envelope["stan"] = original_json["stan"];
                envelope["encrypted_payload"] = encrypted_payload_hex;

                std::string final_payload = envelope.dump();
                std::string key = original_json["stan"].get<std::string>();

                LFMessage m{key, final_payload};
                if (global_lockfree_queue && global_lockfree_queue->enqueue(m)) {
                    return "00";
                }
            } catch (const std::exception &e) {
                LOG_ERROR("Erro ao mapear e enfileirar mensagem no lock-free (STAN: {}): {}", msg.get_field(11), e.what());
            }

            return "91";
        });

        Router::get_instance().registerIssuerHandler("INTERNAL",
            [](const IsoMessage &msg, const std::string &) -> std::string {
                if (msg.mti == "0400") {
                    return std::string(IssuerService::get_instance().process_reversal(msg));
                }
                return std::string(IssuerService::get_instance().process_transaction(msg));
            });

        // Kafka consumer disabled — Lock-Free will handle incoming responses

        // Lock-free consumer thread
        std::thread lockfree_consumer([]() {
            while (true) {
                void* item = nullptr;
                if (global_lockfree_queue && global_lockfree_queue->dequeue(item)) {
                    // TODO: Replace with actual response type
                    // auto* resp = static_cast<ResponseType*>(item);
                    // Decrypt, map, log, etc. as in previous Kafka consumer
                    // LOG_INFO("Processed response");
                } else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                }
            }
        });
        lockfree_consumer.detach();

        // Iniciar threads
// (Lock-free consumer thread started above)
        std::thread cache_th(cache_refresh_thread, app_config.cache_refresh_interval_sec);
        cache_th.detach();

        std::thread metrics_th(metrics_printer_thread);
        metrics_th.detach();

        // --- Inicialização do Servidor TCP ---
        auto worker_pool = std::make_shared<ThreadPool>(4);
        asio::io_context io_context;
        TcpServer server(io_context, app_config.server_port, worker_pool);

        std::vector<std::thread> io_threads;
        for (std::size_t i = 0; i < std::thread::hardware_concurrency(); ++i) {
            io_threads.emplace_back([&io_context]() { io_context.run(); });
        }

        LOG_INFO("Gateway started com sucesso na porta {}.", app_config.server_port);

        for (auto &t: io_threads) {
            t.join();
        }
    } catch (const std::exception &e) {
        LOG_ERROR("Fatal error: {}", e.what());
    }

    return 0;
}