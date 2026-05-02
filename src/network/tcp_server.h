#pragma once
#include <asio.hpp>
#include <memory>
#include "session.h"
#include "../common/thread_pool.h"

using asio::ip::tcp;

class TcpServer {
public:
    TcpServer(asio::io_context& io_context, short port, std::shared_ptr<ThreadPool> thread_pool);
private:
    void do_accept();
    tcp::acceptor acceptor_;
    std::shared_ptr<ThreadPool> thread_pool_;
};
