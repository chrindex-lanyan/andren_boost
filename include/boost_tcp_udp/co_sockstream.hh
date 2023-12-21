#pragma once



#include <sys/socket.h>
#include <future>
#include <tuple>
#include <exception>

#ifndef BOOST_ASIO_HAS_CO_AWAIT
#define BOOST_ASIO_HAS_CO_AWAIT
#endif

#include <boost/asio/ip/address.hpp>
#include <boost/system/error_code.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>

#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/any_io_executor.hpp>

namespace chrindex::andren_boost
{
    using namespace boost::asio;

    class co_sockstream 
    {
    public :

        using base_sock_type = ip::tcp::socket ;
        using executor_type = any_io_executor;

        co_sockstream();

        co_sockstream(base_sock_type && s) ;

        co_sockstream(executor_type & executor, std::string const & ip, uint16_t port ) ;

        co_sockstream(co_sockstream const &) = delete;

        co_sockstream(co_sockstream && ss) noexcept;

        ~co_sockstream();

        void operator = (co_sockstream && ss) ;

        awaitable<int> async_connect(std::string const & ip, int port);

        awaitable<int> async_connect(
            ip::basic_resolver_results<ip::tcp>const & resolver_result);

        awaitable<std::tuple<int64_t,std::string>> async_read (uint64_t bufsize_max = 1024);

        awaitable<int64_t> async_write(std::string & buffer);

        ip::tcp::endpoint peer_endpoint() const;

        ip::tcp::endpoint self_endpoint() const;


        base_sock_type & reference_base_socket();

    private :
        base_sock_type sock;
    };


}


