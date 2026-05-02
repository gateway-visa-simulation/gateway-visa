#include "session.h"
#include "../common/logger.h"
#include "../iso8583/iso_parser.h"
#include "../iso8583/iso_builder.h"
#include "../switch/router.h"
#include "../metrics/metrics_registry.h"
#include "../database/database_manager.h"
#include "../security/soft_hsm.h"
#include <iostream>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <random>

Session::Session(tcp::socket socket, std::shared_ptr<ThreadPool> thread_pool)
    : socket_(std::move(socket)), header_buffer_(2), thread_pool_(thread_pool) {}

void Session::start() {
    do_read_header();
}

void Session::do_read_header() {
    auto self(shared_from_this());
    asio::async_read(socket_, asio::buffer(header_buffer_),
        [this, self](std::error_code ec, std::size_t length) {
            if (!ec) {
                size_t body_length = (header_buffer_[0] << 8) | header_buffer_[1];
                do_read_body(body_length);
            } else {
                LOG_ERROR("Error reading header: {}", ec.message());
            }
        });
}

void Session::do_read_body(size_t length) {
    auto self(shared_from_this());
    body_buffer_.resize(length);
    asio::async_read(socket_, asio::buffer(body_buffer_),
        [this, self](std::error_code ec, std::size_t length) {
            if (!ec) {
                std::stringstream hex_stream;
                hex_stream << std::hex << std::setfill('0');
                for (uint8_t byte : body_buffer_) {
                    hex_stream << std::setw(2) << static_cast<int>(byte);
                }

                LOG_INFO("[RAW IN] Recebidos {} bytes de {}. Payload Hex: {}",
                         body_buffer_.size(),
                         socket_.remote_endpoint().address().to_string(),
                         hex_stream.str());

                auto data_copy = body_buffer_;
                thread_pool_->enqueue([this, self, data_copy]() {
                    process_message(data_copy);
                });
                do_read_header();
            } else {
                LOG_ERROR("Error reading body: {}", ec.message());
            }
        });
}

void Session::process_message(const std::vector<uint8_t>& data) {
    auto start_time = std::chrono::high_resolution_clock::now();
    try {
        auto req = IsoParser::parse(data);
        if (req.mti.empty()) {
            LOG_ERROR("Failed to parse message");
            return;
        }

        // --- LÓGICA DE NETWORK MANAGEMENT ---
        if (req.mti == "0800") {
            handle_network_message(req);
            return; // Encerra o processamento aqui
        }

        // --- VALIDAÇÃO DE SEGURANÇA (MAC) ---
        if (req.mti == "0100" || req.mti == "0200" || req.mti == "0400") {
            std::string test_mak_hex = "0123456789ABCDEFFEDCBA9876543210"; // Chave de teste
            if (!SoftHsm::get_instance().validate_retail_mac(req, test_mak_hex)) {
                LOG_ERROR("MAC validation failed for STAN {}", req.get_field(11));
                auto res = IsoBuilder::build_response(req, "A0"); // A0: MAC Error
                auto res_data = IsoParser::build(res);
                do_write(res_data);
                DatabaseManager::getInstance().logTransaction(req, "A0", "MAC_ERROR");
                return;
            }
        }

        MetricsRegistry::get().record_transaction();

        // --- Verificação de Idempotência ---
        std::string unique_key = req.get_unique_key();
        auto previous_result = DatabaseManager::getInstance().getPreviousTransactionResult(unique_key);
        if (previous_result.has_value()) {
            LOG_WARN("Duplicate transaction detected: {}. Resending previous response.", unique_key);
            auto res = IsoBuilder::build_response(req, previous_result->response_code);
            auto res_data = IsoParser::build(res);
            do_write(res_data);
            return;
        }

        // --- Injeção de Caos Global ---
        auto global_config = DatabaseManager::getInstance().getGlobalConfig();
        std::string issuer_id = "UNKNOWN";
        if(req.has_field(2) && req.get_field(2).length() >= 6) {
             std::string bin = req.get_field(2).substr(0, 6);
             auto route_info_opt = DatabaseManager::getInstance().lookupRoute(bin);
             if(route_info_opt.has_value()) {
                 issuer_id = route_info_opt->issuer_id;
             }
        }

        if (global_config.global_latency_ms > 0) {
            std::this_thread::sleep_for(std::chrono::milliseconds(global_config.global_latency_ms));
        }

        if (global_config.simulate_network_instability) {
            static thread_local std::mt19937 generator(std::random_device{}());
            std::uniform_real_distribution<double> distribution(0.0, 1.0);
            if (distribution(generator) < global_config.error_probability) {
                LOG_WARN("[CHAOS] Simulação de instabilidade: Timeout simulado para STAN: {}", req.get_field(11));
                std::string response_code = "91";
                std::string status = "FAILED_CHAOS";
                auto res = IsoBuilder::build_response(req, response_code);
                DatabaseManager::getInstance().logTransaction(req, response_code, status, issuer_id);
                MetricsRegistry::get().record_response_code(response_code);
                return;
            }
        }

        // --- Execução do Roteamento e Resposta ---
        std::string response_code = Router::get_instance().route(req);
        std::string status = (response_code == "00") ? "APPROVED" : "DENIED";
        auto res = IsoBuilder::build_response(req, response_code);
        DatabaseManager::getInstance().logTransaction(req, response_code, status, issuer_id);
        MetricsRegistry::get().record_response_code(response_code);
        auto res_data = IsoParser::build(res);
        do_write(res_data);

    } catch (const std::exception& e) {
        LOG_ERROR("Exception processing message: {}", e.what());
    }
    auto end_time = std::chrono::high_resolution_clock::now();
    double latency_ms = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    MetricsRegistry::get().record_latency(latency_ms);
}

void Session::handle_network_message(const IsoMessage& req) {
    LOG_INFO("Processing Network Management message (0800)");
    std::string nmm_code = req.get_field(70);
    std::string response_code = "96"; // Default to System Malfunction

    if (nmm_code == "301" || nmm_code == "001") { // 301: Echo Test, 001: Logon
        response_code = "00";
    } else {
        response_code = "30"; // Format Error for unknown codes
    }

    IsoMessage res;
    res.mti = "0810";
    res.set_field(7, req.get_field(7));
    res.set_field(11, req.get_field(11));
    res.set_field(39, response_code);
    res.set_field(70, nmm_code);

    auto res_data = IsoParser::build(res);
    do_write(res_data);
    LOG_INFO("Sent 0810 response for NMM code {} with RC {}", nmm_code, response_code);
}

void Session::do_write(const std::vector<uint8_t>& data) {
    auto self(shared_from_this());
    auto out_buffer = std::make_shared<std::vector<uint8_t>>();
    out_buffer->push_back((data.size() >> 8) & 0xFF);
    out_buffer->push_back(data.size() & 0xFF);
    out_buffer->insert(out_buffer->end(), data.begin(), data.end());

    std::stringstream hex_stream;
    hex_stream << std::hex << std::setfill('0');
    for (uint8_t byte : *out_buffer) {
        hex_stream << std::setw(2) << static_cast<int>(byte);
    }
    LOG_INFO("[RAW OUT] Enviando {} bytes para {}. Payload Hex: {}",
             out_buffer->size(),
             socket_.remote_endpoint().address().to_string(),
             hex_stream.str());

    asio::async_write(socket_, asio::buffer(*out_buffer),
        [this, self, out_buffer](std::error_code ec, std::size_t /*length*/) {
            if (ec) {
                LOG_ERROR("Error writing response: {}", ec.message());
            }
        });
}
