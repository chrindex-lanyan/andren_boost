#include "co_sockstream.hh"

#include <algorithm>
#include <exception>
#include <utility>

#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/address.hpp>


namespace chrindex::andren_boost
{
    using namespace boost::asio;

    static io_context _default_io_ctx{1};

    co_sockstream::co_sockstream():sock(_default_io_ctx,{}){}

    co_sockstream::co_sockstream(base_sock_type && s) 
        : sock(std::move(s)){ sock.set_option(base_sock_type::reuse_address()); }

    co_sockstream::co_sockstream(
            executor_type & executor, 
        std::string const & ip, uint16_t port) 
        : sock(executor,{ip::address::from_string(ip),port})
        { sock.set_option(base_sock_type::reuse_address()); }

    co_sockstream::co_sockstream(co_sockstream && ss) noexcept :sock(std::move(ss.sock)) {}

    co_sockstream::~co_sockstream(){}

    void co_sockstream::operator = (co_sockstream && ss) { sock = std::move(ss.sock); }

    awaitable<int> co_sockstream::async_connect(std::string const & ip, int port)
    {
        ip::tcp::endpoint ep (ip::address::from_string(ip),port);
        try 
        {
            co_await sock.async_connect(ep,use_awaitable);
        }
        catch(std::exception &e)
        {
            co_return -1;
        }
        co_return 0;
    }

    awaitable<int> 
    co_sockstream::async_connect (
        ip::basic_resolver_results<ip::tcp> const &resolver_result)
    {
        try 
        {
            co_await sock.async_connect(resolver_result->endpoint(),use_awaitable);
        }
        catch(std::exception &e)
        {
            co_return -1;
        }
        co_return 0;
    }

    awaitable<std::tuple<int64_t,std::string>> co_sockstream::async_read (uint64_t bufsize_max)
    {
        bufsize_max = std::max(16ul, bufsize_max);

        int64_t nread = 0; 
        std::string buffer(bufsize_max, 0x00);

        try 
        {
            nread = co_await sock.async_read_some(
                boost::asio::buffer(&buffer[0],buffer.size()),
                use_awaitable);
        }
        catch (std::exception & e)
        {
            nread = 0;
        }

        if (nread > 0)
        {
            co_return std::tuple<int64_t, std::string>{nread , std::move(buffer)};
        }
        else if (nread ==0)
        {
            co_return std::tuple<int64_t, std::string>{0,{}};
        }
        co_return std::tuple<int64_t, std::string>{-1,{}};
    }

    awaitable<int64_t> co_sockstream::async_write(std::string & buffer)
    {
        int nwrite = 0;
        try
        {
            nwrite = co_await sock.async_write_some(
                boost::asio::buffer(&buffer[0], buffer.size()), 
                    use_awaitable);
        }
        catch(std::exception & e)
        {
            co_return -1;
        }
        co_return nwrite;
    }

    ip::tcp::endpoint co_sockstream::peer_endpoint() const
    {
        return sock.remote_endpoint();
    }

    ip::tcp::endpoint co_sockstream::self_endpoint() const
    {
        return sock.local_endpoint();
    }

    co_sockstream::base_sock_type & co_sockstream::reference_base_socket()
    {
        return sock;
    }

}
