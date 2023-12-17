
#include "co_sockstream_awaiter.hh"
#include "co_sockstream.hh"
#include <boost/asio/ip/tcp.hpp>
#include <memory>


namespace chrindex::andren_boost 
{
    using namespace boost::asio;

    co_sockstream_awaiter::co_sockstream_awaiter(io_context::executor_type & executor, std::string const & ip, int port)
        : acceptor(executor , ip::tcp::endpoint(ip::address::from_string(ip), port))
    {
        acceptor.listen(boost::asio::socket_base::max_listen_connections);
    }

    co_sockstream_awaiter::~co_sockstream_awaiter(){}

    awaitable<co_sockstream> co_sockstream_awaiter::async_accept()
    {
        co_return co_await acceptor.async_accept(use_awaitable);
    }

}
