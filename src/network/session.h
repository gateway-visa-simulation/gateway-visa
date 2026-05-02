#pragma once
#include <asio.hpp>
#include <memory>
#include <vector>
#include "../common/thread_pool.h"
#include "../iso8583/iso_message.h"

using asio::ip::tcp;

class Session : public std::enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket, std::shared_ptr<ThreadPool> thread_pool);
    void start();

private:
    void do_read_header();
    void do_read_body(size_t length);
    void process_message(const std::vector<uint8_t>& data);
    void do_write(const std::vector<uint8_t>& data);
    void handle_network_message(const IsoMessage& req);

    tcp::socket socket_;
    std::vector<uint8_t> header_buffer_;
    std::vector<uint8_t> body_buffer_;
    std::shared_ptr<ThreadPool> thread_pool_;
};
