#pragma once
#include <asio.hpp>
#include <memory>
#include <vector>
#include "../common/thread_pool.h"
#include "../iso8583/iso_message.h"

using namespace std;

using asio::ip::tcp;

class Session : public enable_shared_from_this<Session> {
public:
    Session(tcp::socket socket, shared_ptr<ThreadPool> thread_pool);
    void start();

    // Public helper to send a response (built bytes)
    void send_response(const vector<uint8_t>& data);

private:
    void do_read_header();
    void do_read_body(size_t length);
    void process_message(const vector<uint8_t>& data);
    void do_write(const vector<uint8_t>& data);
    void handle_network_message(const IsoMessage& req);

    tcp::socket socket_;
    vector<uint8_t> header_buffer_;
    vector<uint8_t> body_buffer_;
    shared_ptr<ThreadPool> thread_pool_;
};
