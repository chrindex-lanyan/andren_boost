#pragma once



#include <memory>
#include <optional>
#include <future>
#include <stdexcept>
#include <tuple>
#include <exception>

#ifndef BOOST_ASIO_HAS_CO_AWAIT
#define BOOST_ASIO_HAS_CO_AWAIT
#endif

#include <boost/asio/ip/address.hpp>
#include <boost/system/error_code.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/beast/core/buffers_to_string.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>

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

        using websocket_type = websocket::stream<ip::tcp::socket>;

        co_websocket();
        co_websocket(co_websocket && _ano) noexcept;
        explicit co_websocket(ip::tcp::socket && s) noexcept;
        ~co_websocket();
        void operator=(co_websocket && _ano) noexcept;

        co_websocket(co_websocket const &) = delete;
        void operator=(co_websocket const &) = delete;
        
        bool is_empty() const ;

        void set_data_type(websocket_data_type_t type);

        websocket_data_type_t get_data_type() const;

        void set_websocket_type(websocket_type_t type);

        bool set_other_option(std::function<void (websocket_type * ws_ptr)> cb);

        awaitable<std::tuple<int64_t, std::string>> 
            try_recv() ;

        awaitable<int64_t> try_send(std::string const & data);

        awaitable<int64_t> try_send(std::string &&data);

    private :

        websocket_type & websocket() ;

        websocket_type const & websocket() const;

    private :

        std::unique_ptr<websocket_type> m_websocket;
    }; 

}