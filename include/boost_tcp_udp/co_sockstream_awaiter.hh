#pragma once

#include "co_io_context_manager.hh"
#include "co_sockstream.hh"


#ifndef BOOST_ASIO_HAS_CO_AWAIT
#define BOOST_ASIO_HAS_CO_AWAIT
#endif

#include <boost/asio/executor.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/basic_endpoint.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>


namespace chrindex::andren_boost
{
    using namespace boost::asio;

    class co_sockstream_awaiter
    {
    public :
        co_sockstream_awaiter(io_context::executor_type & executor, std::string const & ip, int port);

        ~co_sockstream_awaiter();

        awaitable<co_sockstream> async_accept();

    private :
        ip::tcp::acceptor acceptor;
    };
    

}



