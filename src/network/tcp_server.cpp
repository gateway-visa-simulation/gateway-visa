#include "tcp_server.h"
#include "../common/logger.h"

TcpServer::TcpServer(asio::io_context &io_context, short port, std::shared_ptr<ThreadPool> thread_pool)
    : acceptor_(io_context) {
    // Apenas constrói o acceptor

    tcp::endpoint endpoint(tcp::v4(), port);

    acceptor_.open(endpoint.protocol());
    acceptor_.set_option(asio::socket_base::reuse_address(true));
    acceptor_.bind(endpoint);
    acceptor_.listen();

    thread_pool_ = thread_pool;

    LOG_INFO("Server started on port {}", port);
    do_accept();
}

void TcpServer::do_accept() {
    acceptor_.async_accept(
        [this](std::error_code ec, tcp::socket socket) {
            if (!ec) {
                LOG_INFO("Accepted connection from {}", socket.remote_endpoint().address().to_string());
                std::make_shared<Session>(std::move(socket), thread_pool_)->start();
            }
            do_accept();
        });
}
