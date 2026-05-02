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

        //Config app_config = Config::load("C:\\Users\\chico\\CLionProjects\\gateway-visa\\config.json");
        // Altere no seu main.cpp
        Config app_config = Config::load("/mnt/c/Users/chico/CLionProjects/gateway-visa/config.json");

        // Inicializa conexão com o Banco e carrega o cache inicial
        try {
            DatabaseManager::getInstance().init(app_config.db_connection_string);
        } catch (const std::exception &e) {
            LOG_ERROR("Falha crítica ao conectar no banco de dados. O gateway não pode iniciar. Erro: {}", e.what());
            return 1;
        }

        // Init HSM Mock
        HSM::getInstance().init();

        // --- Configuração Kafka ---
        std::string brokers = app_config.kafka_brokers;
        std::string request_topic = app_config.kafka_request_topic;
        std::string response_topic = app_config.kafka_response_topic;

        std::shared_ptr<KafkaProducer> producer;
        if (!brokers.empty()) {
            try {
                producer = std::make_shared<KafkaProducer>(brokers);
            } catch (const std::exception &e) {
                LOG_ERROR("Nao foi possivel conectar ao Kafka. Erro: {}", e.what());
            }
        }

        // --- Registro da Lógica de Roteamento Dinâmico via Banco de Dados ---
        Router::get_instance().setRoutingLogic([](const IsoMessage &msg) -> std::optional<BinRoute> {
            if (!msg.has_field(2)) {
                LOG_WARN("Mensagem sem PAN (DE2) para roteamento.");
                return std::nullopt;
            }
            std::string pan = msg.get_field(2);
            if (pan.length() < 6) {
                LOG_WARN("PAN inválido para extrair BIN: {}", pan);
                return std::nullopt;
            }
            std::string bin = pan.substr(0, 6);

            // Busca a rota do BIN no banco de dados (que usa cache)
            auto route_info_opt = DatabaseManager::getInstance().lookupRoute(bin);
            if (route_info_opt) {
                // Mapeia RouteInfo (do DatabaseManager) para BinRoute (esperado pelo Router)
                BinRoute br;
                br.bin_prefix = bin;
                br.issuer_id = route_info_opt->issuer_id;
                // A descrição pode ser o handler_type ou uma descrição real do BIN
                br.description = route_info_opt->handler_type; // Usando handler_type como descrição para passar adiante
                return br;
            }
            return std::nullopt;
        });

        // --- Registro dos Handlers (A Execução Específica de Cada Emissor) ---

        Router::get_instance().registerIssuerHandler(
            "KAFKA", [producer, request_topic](const IsoMessage &msg, const std::string &endpoint_config) -> std::string {
                if (!producer) return std::string("91");

                std::string pan = msg.get_field(2);
                double amount = std::stod(msg.get_field(4)) / 100.0; // Assume campo 4 como centavos

                // --- LÓGICA DE VALIDAÇÃO DO CARTÃO (INCLUINDO FORCE_RESPONSE_CODE) ---
                auto card_opt = DatabaseManager::getInstance().lookupCard(pan);
                if (!card_opt) {
                    LOG_WARN("Cartão não encontrado no banco para o handler KAFKA: {}", msg.mask_pan());
                    return std::string("14"); // Invalid card number
                }
                const auto& card = *card_opt;

                // Se force_response_code não for nulo, ignorar o processamento e usar esse código
                if (card.force_response_code.has_value() && !card.force_response_code.value().empty()) {
                    LOG_INFO("[KAFKA HANDLER] Aplicando RC forçado '{}' para o cartão {}", card.force_response_code.value(), msg.mask_pan());
                    return card.force_response_code.value();
                }
                // --- FIM DA LÓGICA DE VALIDAÇÃO ---

                // Tenta descontar o saldo antes de enviar para o Kafka
                if (!DatabaseManager::getInstance().updateCardBalance(pan, amount)) {
                    LOG_WARN("Transação negada: Saldo insuficiente ou cartão inativo para o PAN: {}", pan.substr(0,6));
                    return std::string("51"); // Código de saldo insuficiente
                }

                try {
                    nlohmann::json original_json = IsoToJsonMapper::mapIsoToJson(msg);
                    std::string plaintext_payload = original_json.dump();
                    std::string encrypted_payload_hex = HSM::getInstance().encryptData(plaintext_payload);

                    nlohmann::json kafka_envelope;
                    kafka_envelope["stan"] = original_json["stan"];
                    kafka_envelope["encrypted_payload"] = encrypted_payload_hex;

                    // Exemplo de uso da configuração do endpoint (vinda do banco de dados jsonb)
                    if (!endpoint_config.empty()) {
                        try {
                            auto config_json = nlohmann::json::parse(endpoint_config);
                            // Exemplo: Substituir o tópico de destino se estiver configurado
                            // std::string custom_topic = config_json.value("topic", request_topic);
                        } catch (...) {
                            LOG_WARN("Falha ao fazer parse do endpoint_config: {}", endpoint_config);
                        }
                    }

                    std::string final_payload_for_kafka = kafka_envelope.dump();
                    std::string key = original_json["stan"].get<std::string>();

                    LOG_INFO("Publicando payload ENCRIPTADO no Kafka. Topico: {} | Key: {}", request_topic, key);

                    if (producer->send(request_topic, key, final_payload_for_kafka)) {
                        return std::string("00");
                    }
                } catch (const std::exception &e) {
                    LOG_ERROR("Erro ao mapear e criptografar mensagem para o Kafka: {}", e.what());
                }

                return std::string("91");
            });

        Router::get_instance().registerIssuerHandler("IssuerLocal",
                                                     [](const IsoMessage &msg, const std::string & /*config*/) {
                                                         if (msg.mti == "0400") return IssuerService::get_instance().
                                                                 process_reversal(msg);
                                                         return IssuerService::get_instance().process_transaction(msg);
                                                     });

        // --- Configuração Consumer ---
        if (!brokers.empty()) {
            std::thread consumer_th([brokers, response_topic]() {
                try {
                    KafkaConsumer consumer(brokers, "visa-gateway-group");
                    consumer.subscribe({response_topic});

                    consumer.start_consuming([](const std::string &key, const std::string &payload) {
                        try {
                            auto kafka_envelope = nlohmann::json::parse(payload);
                            std::string encrypted_data = kafka_envelope["encrypted_payload"].get<std::string>();
                            std::string decrypted_payload_str = HSM::getInstance().decryptData(encrypted_data);

                            LOG_DEBUG("<< [KAFKA CONSUMER] Payload decriptado para STAN {}: {}", key,
                                      decrypted_payload_str);

                            auto original_json = nlohmann::json::parse(decrypted_payload_str);
                            IsoMessage responseIso = JsonToIsoMapper::mapJsonToIso(original_json);

                            LOG_INFO("<< [KAFKA CONSUMER] Resposta Mapeada! STAN: {} | Código: {}",
                                     responseIso.get_field(11), responseIso.get_field(39));
                        } catch (const std::exception &e) {
                            LOG_ERROR("Erro ao processar/decriptografar payload do consumidor (STAN: {}): {}", key,
                                      e.what());
                        }
                    });
                } catch (const std::exception &e) {
                    LOG_ERROR("Falha no Consumer: {}", e.what());
                }
            });
            consumer_th.detach();
        }

        // Iniciar thread de refresh do cache
        std::thread cache_th(cache_refresh_thread, app_config.cache_refresh_interval_sec);
        cache_th.detach();

        // --- Inicialização do Servidor TCP ---
        auto worker_pool = std::make_shared<ThreadPool>(4);
        asio::io_context io_context;
        short port = app_config.server_port;
        TcpServer server(io_context, port, worker_pool);

        std::vector<std::thread> io_threads;
        for (std::size_t i = 0; i < std::thread::hardware_concurrency(); ++i) {
            io_threads.emplace_back([&io_context]() {
                io_context.run();
            });
        }

        std::thread metrics_th(metrics_printer_thread);
        metrics_th.detach();

        LOG_INFO("Gateway started com sucesso na porta {}.", port);

        for (auto &t: io_threads) {
            t.join();
        }
    } catch (const std::exception &e) {
        LOG_ERROR("Fatal error: {}", e.what());
    }

    return 0;
}
