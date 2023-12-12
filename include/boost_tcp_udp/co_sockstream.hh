#pragma once

#include <exception>
#include <sys/socket.h>
#include <future>
#include <tuple>

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

namespace chrindex::andren_boost
{
    using namespace boost::asio;

    class co_sockstream 
    {
    public :
        co_sockstream();

        co_sockstream(ip::tcp::socket && s) ;

        co_sockstream(io_context &ioctx) ;

        co_sockstream(io_context &ioctx, std::string const & ip, uint16_t port ) ;

        co_sockstream(co_sockstream const &) = delete;

        co_sockstream(co_sockstream && ss) noexcept;

        ~co_sockstream();

        void operator = (co_sockstream && ss) ;

        awaitable<int> async_connect(std::string const & ip, int port);

        awaitable<std::tuple<int64_t,std::string>> async_read (uint64_t bufsize_max = 1024);

        awaitable<int64_t> async_write(std::string & buffer);

        ip::tcp::endpoint peer_endpoint() const;

        ip::tcp::endpoint self_endpoint() const;

    private :
        ip::tcp::socket sock;
    };


}


