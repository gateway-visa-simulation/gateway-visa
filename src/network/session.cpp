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
#include <algorithm> // Para remove_if
#include <cctype>    // Para isspace

Session::Session(tcp::socket socket, shared_ptr<ThreadPool> thread_pool)
    : socket_(move(socket)), header_buffer_(2), thread_pool_(thread_pool) {
}

void Session::start() {
    do_read_header();
}

void Session::do_read_header() {
    auto self(shared_from_this());
    asio::async_read(socket_, asio::buffer(header_buffer_),
                     [this, self](error_code ec, size_t length) {
                         if (!ec) {
                             size_t body_length = (header_buffer_[0] << 8) | header_buffer_[1];
                             do_read_body(body_length);
                         } else {
                             if (ec != asio::error::eof) {
                                 LOG_ERROR("Error reading header: {}", ec.message());
                             }
                         }
                     });
}

void Session::do_read_body(size_t length) {
    auto self(shared_from_this());
    body_buffer_.resize(length);
    asio::async_read(socket_, asio::buffer(body_buffer_),
                     [this, self](error_code ec, size_t length) {
                         if (!ec) {
                             stringstream hex_stream;
                             hex_stream << hex << setfill('0');
                             for (uint8_t byte: body_buffer_) {
                                 hex_stream << setw(2) << static_cast<int>(byte);
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
                             if (ec != asio::error::eof) {
                                 LOG_ERROR("Error reading body: {}", ec.message());
                             }
                         }
                     });
}

void Session::handle_network_message(const IsoMessage &req) {
    LOG_INFO("Processing Network Management message (0800)");

    string nmm_code = req.get_field(70);
    nmm_code.erase(remove_if(nmm_code.begin(), nmm_code.end(),
                             [](unsigned char c) { return isspace(c) || c == '\0'; }),
                   nmm_code.end());

    string response_code = "30"; // Default to Format Error

    if (nmm_code == "301" || nmm_code == "001") {
        response_code = "00";
    }

    DatabaseManager::getInstance().logTransaction(req, response_code, "NETWORK_MSG");

    IsoMessage res;
    res.mti = "0810";
    res.set_field(7, req.get_field(7));
    res.set_field(11, req.get_field(11));
    res.set_field(39, response_code);
    res.set_field(70, nmm_code);

    auto res_data = IsoParser::build(res);
    do_write(res_data);
    LOG_INFO("Sent 0810 response for NMM code [{}] with RC [{}]", nmm_code, response_code);
}

void Session::process_message(const vector<uint8_t> &data) {
    auto start_time = chrono::high_resolution_clock::now();
    try {
        auto req = IsoParser::parse(data);
        if (req.mti.empty()) {
            LOG_ERROR("Failed to parse message");
            return;
        }

        if (req.mti == "0800") {
            handle_network_message(req);
            return;
        }

        if (req.mti == "0100" || req.mti == "0200" || req.mti == "0400" || req.mti == "0220") {

            if (req.mti != "0400") {
                bool has_mac = req.has_field(64) || req.has_field(128);

                if (has_mac) {
                    string mak_label = "RETAIL_MAK_01";

                    if (!SoftHsm::get_instance().validate_retail_mac(req, mak_label)) {
                        LOG_ERROR("MAC validation failed for STAN {}. Invalid MAC code A0 returned.",
                                  req.get_field(11));
                        auto res = IsoBuilder::build_response(req, "A0");
                        DatabaseManager::getInstance().logTransaction(req, "A0", "MAC_ERROR");
                        auto res_data = IsoParser::build(res);
                        do_write(res_data);
                        return;
                    }
                    LOG_INFO("MAC validated successfully for STAN {}", req.get_field(11));
                } else {
                    LOG_DEBUG("Skipping MAC validation for STAN {}: No MAC field present in message.",
                              req.get_field(11));
                }
            }

            // REVERTIDO: Validação de PIN é executada sempre que o campo 52 está presente.
            if (req.has_field(52) && req.has_field(2)) {
                string pin_block_str = req.get_field(52);
                vector<uint8_t> pin_block_bytes(pin_block_str.begin(), pin_block_str.end());
                string pan = req.get_field(2);
                string zpk_hex = "0123456789ABCDEFFEDCBA9876543210";

                LOG_INFO("Iniciando validação de PIN com SoftHSM local para PAN {}", req.mask_pan());
                LOG_DEBUG("Using ZPK for decryption: {}", zpk_hex);

                auto decrypted_pin_opt = SoftHsm::get_instance().decrypt_pin_block_iso0(pin_block_bytes, pan, zpk_hex);

                if (!decrypted_pin_opt.has_value()) {
                    LOG_WARN("PIN validation failed for STAN {}. Could not decrypt PIN block.", req.get_field(11));
                    auto res = IsoBuilder::build_response(req, "55");
                    DatabaseManager::getInstance().logTransaction(req, "55", "INVALID_PIN");
                    auto res_data = IsoParser::build(res);
                    do_write(res_data);
                    return;
                }
                LOG_INFO("PIN block decrypted successfully for STAN {}", req.get_field(11));
            }

            MetricsRegistry::get().record_transaction();

            string unique_key = req.get_unique_key();
            auto previous_result = DatabaseManager::getInstance().getPreviousTransactionResult(unique_key);
            if (previous_result.has_value()) {
                LOG_WARN("Duplicate transaction detected: {}. Resending previous response.", unique_key);
                auto res = IsoBuilder::build_response(req, previous_result->response_code);
                auto res_data = IsoParser::build(res);
                do_write(res_data);
                return;
            }

            auto global_config = DatabaseManager::getInstance().getGlobalConfig();
            string issuer_id = "UNKNOWN";
            string handler_type = "IssuerLocal";
            if (req.has_field(2) && req.get_field(2).length() >= 6) {
                string bin = req.get_field(2).substr(0, 6);
                auto route_info_opt = DatabaseManager::getInstance().lookupRoute(bin);
                if (route_info_opt.has_value()) {
                    issuer_id = route_info_opt->issuer_id;
                    auto issuer_config_opt = DatabaseManager::getInstance().lookupIssuerConfig(issuer_id);
                    if (issuer_config_opt.has_value()) {
                        handler_type = issuer_config_opt->handler_type;
                    }
                }
            }

            // If the handler is asynchronous (KAFKA/LOCK-FREE), register this session to receive the response later
            if (handler_type == "KAFKA" ) {
                string stan = req.get_field(11);
                LOG_DEBUG("Registering session for async response STAN {}", stan);
                SessionRegistry::get().register_session(stan, shared_from_this());
            }

            if (global_config.global_latency_ms > 0) {
                this_thread::sleep_for(chrono::milliseconds(global_config.global_latency_ms));
            }

            if (global_config.simulate_network_instability) {
                static thread_local mt19937 generator(random_device{}());
                uniform_real_distribution<double> distribution(0.0, 1.0);
                if (distribution(generator) < global_config.error_probability) {
                    LOG_WARN("[CHAOS] Simulação de instabilidade: Timeout simulado para STAN: {}", req.get_field(11));
                    DatabaseManager::getInstance().logTransaction(req, "91", "FAILED_CHAOS", issuer_id);
                    MetricsRegistry::get().record_response_code("91");
                    return;
                }
            }

            string response_code = Router::get_instance().route(req);
            string status = (response_code == "00") ? "APPROVED" : "DENIED";
            auto res = IsoBuilder::build_response(req, response_code);
            DatabaseManager::getInstance().logTransaction(req, response_code, status, issuer_id);
            MetricsRegistry::get().record_response_code(response_code);
            auto res_data = IsoParser::build(res);
            do_write(res_data);
        } else {
            LOG_WARN("MTI não suportado: {}. Rejeitando.", req.mti);
            auto res = IsoBuilder::build_response(req, "06");
            DatabaseManager::getInstance().logTransaction(req, "06", "UNSUPPORTED_MTI");
            auto res_data = IsoParser::build(res);
            do_write(res_data);
        }
    } catch (const exception &e) {
        LOG_ERROR("Exception processing message: {}", e.what());
    }
    auto end_time = chrono::high_resolution_clock::now();
    double latency_ms = chrono::duration<double, milli>(end_time - start_time).count();
    MetricsRegistry::get().record_latency(latency_ms);
}


void Session::do_write(const vector<uint8_t> &data) {
    auto self(shared_from_this());
    auto out_buffer = make_shared<vector<uint8_t> >();
    out_buffer->push_back((data.size() >> 8) & 0xFF);
    out_buffer->push_back(data.size() & 0xFF);
    out_buffer->insert(out_buffer->end(), data.begin(), data.end());

    stringstream hex_stream;
    hex_stream << hex << setfill('0');
    for (uint8_t byte: *out_buffer) {
        hex_stream << setw(2) << static_cast<int>(byte);
    }
    LOG_INFO("[RAW OUT] Enviando {} bytes para {}. Payload Hex: {}",
             out_buffer->size(),
             socket_.remote_endpoint().address().to_string(),
             hex_stream.str());

    asio::async_write(socket_, asio::buffer(*out_buffer),
                      [this, self, out_buffer](error_code ec, size_t /*length*/) {
                          if (ec && ec != asio::error::eof) {
                              LOG_ERROR("Error writing response: {}", ec.message());
                          }
                      });
}

void Session::send_response(const vector<uint8_t> &data) {
    // For thread-safety, post the write to the io_context's thread by reusing do_write
    // do_write already uses asio::async_write which is thread-safe if called from any thread.
    do_write(data);
}