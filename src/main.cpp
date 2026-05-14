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
#include "kafka/kafka_producer.h"
#include "kafka/kafka_consumer.h"
#include "mapper/iso_to_json.h"
#include "mapper/json_to_iso.h"
#include "security/hsm.h"
#include "database/database_manager.h"
#include <chrono>
#include <nlohmann/json.hpp>

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

        std::shared_ptr<KafkaProducer> producer;
        if (!app_config.kafka_brokers.empty()) {
            try {
                producer = std::make_shared<KafkaProducer>(app_config.kafka_brokers);
            } catch (const std::exception &e) {
                LOG_ERROR("Nao foi possivel conectar ao Kafka. Erro: {}", e.what());
            }
        }

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
        Router::get_instance().registerIssuerHandler(
            "KAFKA", [producer, request_topic = app_config.kafka_request_topic](const IsoMessage &msg, const std::string &endpoint_config) -> std::string {
                if (!producer) return "91";

                std::string pan = msg.get_field(2);
                auto card_opt = DatabaseManager::getInstance().lookupCard(pan);

                if (!card_opt) {
                    LOG_WARN("Cartão não encontrado no banco para o handler KAFKA: {}", msg.mask_pan());
                    return "14";
                }

                const auto& card = *card_opt;

                if (card.force_response_code.has_value() && !card.force_response_code.value().empty()) {
                    LOG_INFO("[KAFKA HANDLER] Aplicando RC forçado '{}' para o cartão {}", card.force_response_code.value(), msg.mask_pan());
                    return card.force_response_code.value();
                }

                double amount = std::stod(msg.get_field(4)) / 100.0;
                if (!DatabaseManager::getInstance().updateCardBalance(pan, amount)) {
                    LOG_WARN("Transação negada: Saldo insuficiente ou cartão inativo para o PAN: {}", pan.substr(0,6));
                    return "51";
                }

                try {
                    nlohmann::json original_json = IsoToJsonMapper::mapIsoToJson(msg);
                    std::string plaintext_payload = original_json.dump();
                    std::string encrypted_payload_hex = HSM::getInstance().encryptData(plaintext_payload);

                    nlohmann::json kafka_envelope;
                    kafka_envelope["stan"] = original_json["stan"];
                    kafka_envelope["encrypted_payload"] = encrypted_payload_hex;

                    std::string final_payload_for_kafka = kafka_envelope.dump();
                    std::string key = original_json["stan"].get<std::string>();

                    LOG_INFO("Publicando payload ENCRIPTADO no Kafka. Topico: {} | Key: {}", request_topic, key);

                    if (producer->send(request_topic, key, final_payload_for_kafka)) {
                        return "00";
                    }
                } catch (const std::exception &e) {
                    LOG_ERROR("Erro ao mapear e criptografar mensagem para o Kafka: {}", e.what());
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

        // --- Configuração Consumer ---
        if (!app_config.kafka_brokers.empty()) {
            std::thread consumer_th([brokers = app_config.kafka_brokers, response_topic = app_config.kafka_response_topic]() {
                try {
                    KafkaConsumer consumer(brokers, "visa-gateway-group");
                    consumer.subscribe({response_topic});
                    consumer.start_consuming([](const std::string &key, const std::string &payload) {
                        try {
                            auto kafka_envelope = nlohmann::json::parse(payload);
                            std::string encrypted_data = kafka_envelope["encrypted_payload"].get<std::string>();
                            std::string decrypted_payload_str = HSM::getInstance().decryptData(encrypted_data);
                            auto original_json = nlohmann::json::parse(decrypted_payload_str);
                            IsoMessage responseIso = JsonToIsoMapper::mapJsonToIso(original_json);
                            LOG_INFO("<< [KAFKA CONSUMER] Resposta Mapeada! STAN: {} | Código: {}",
                                     responseIso.get_field(11), responseIso.get_field(39));
                        } catch (const std::exception &e) {
                            LOG_ERROR("Erro ao processar/decriptografar payload do consumidor (STAN: {}): {}", key, e.what());
                        }
                    });
                } catch (const std::exception &e) {
                    LOG_ERROR("Falha no Consumer: {}", e.what());
                }
            });
            consumer_th.detach();
        }

        // Iniciar threads
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