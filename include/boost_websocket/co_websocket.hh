#pragma once

#include "co_sockstream.hh"
#include <thread>
#include <chrono>
#include <memory>
#include <optional>
#include <future>
#include <stdexcept>
#include <tuple>
#include <exception>
#include <string_view>
#include <coroutine>


#ifndef BOOST_ASIO_HAS_CO_AWAIT
#define BOOST_ASIO_HAS_CO_AWAIT
#endif

#include <boost/system/error_code.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>

#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket/stream.hpp>
#include <boost/beast/websocket.hpp>


namespace chrindex::andren_boost
{
    using namespace boost::asio;
    using namespace boost::beast;

    enum class websocket_data_type_t
    {
        NONE,
        TEXT,
        BINARY,
    };

    enum class websocket_type_t
    {
        CLIENT,
        SERVER,
    };

    class co_websocket
    {
    public :

        using base_stream = typename tcp_stream::rebind_executor<
            typename use_awaitable_t<>::executor_with_default<any_io_executor>>::other;
        //using base_stream = ip::tcp::socket;
        using websocket_type = websocket::stream<base_stream>;

        co_websocket();
        co_websocket(co_websocket && _ano) noexcept;
        explicit co_websocket(base_stream && s) noexcept;
        explicit co_websocket(co_sockstream && s) noexcept;
        ~co_websocket();
        void operator=(co_websocket && _ano) noexcept;

        co_websocket(co_websocket const &) = delete;
        void operator=(co_websocket const &) = delete;
        
        bool is_empty() const noexcept;

        void set_data_type(websocket_data_type_t type) noexcept;

        websocket_data_type_t get_data_type() const noexcept;

        bool set_websocket_type(websocket_type_t type) noexcept;

        bool set_handshake_message(websocket_type_t type, std::string_view msg) noexcept;

        bool set_other_option(std::function<void (websocket_type * ws_ptr)> cb)  noexcept;

        awaitable<bool> handshake_with_server(std::string host, std::string target)  noexcept;

        awaitable<std::tuple<int64_t, std::string>> 
            try_recv()  noexcept;

        awaitable<int64_t> try_send(std::string const & data) noexcept;

        awaitable<int64_t> try_send(std::string &&data) noexcept;

    private :

        websocket_type & websocket() ;

        websocket_type const & websocket() const;

    private :

        std::unique_ptr<websocket_type> m_websocket;
    }; 

}