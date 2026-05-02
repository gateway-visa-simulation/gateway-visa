#include <boost/asio.hpp>
#include <memory>

using boost::asio::ip::tcp;

class Session;

class TcpServer {
public:
    TcpServer(boost::asio::io_context& io_context, short port);

private:
    void do_accept();

    tcp::acceptor acceptor_;
};