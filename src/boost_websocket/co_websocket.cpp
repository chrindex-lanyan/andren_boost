#include "co_websocket.hh"

#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/fields.hpp>
#include <future>


namespace  chrindex::andren_boost
{
    using namespace boost::asio;

    co_websocket::co_websocket(){}
    co_websocket::co_websocket(co_websocket && _ano) noexcept
    {
        m_websocket = std::move(_ano.m_websocket);
    }
    co_websocket::co_websocket(base_stream && s) noexcept
    {
        m_websocket = std::make_unique<websocket_type>(std::move(s));
    }
    co_websocket::co_websocket(co_sockstream && s) noexcept
    {
        m_websocket = std::make_unique<websocket_type>
            (std::move(s.reference_base_socket()));
    }
    co_websocket::~co_websocket(){}

    void co_websocket::operator=(co_websocket && _ano) noexcept
    {
        m_websocket = std::move(_ano.m_websocket);
    }

    bool co_websocket::is_empty() const 
    {
        return m_websocket == nullptr;
    }

    void co_websocket::set_data_type(websocket_data_type_t type)
    {
        if (is_empty() || type == websocket_data_type_t::NONE)[[unlikely]]
        {
            return ;
        }
        websocket().text(type == websocket_data_type_t::TEXT);
    }

    void co_websocket::set_websocket_type(websocket_type_t type)
    {
        if (type == websocket_type_t::CLIENT)
        {
            websocket().set_option(
            websocket::stream_base::timeout::suggested(
            role_type::client));
        }
        else 
        {
            websocket().set_option(
            websocket::stream_base::timeout::suggested(
            role_type::server));
        }
    }

    void co_websocket::set_handshake_message(websocket_type_t type, std::string_view _msg)
    {
        std::string msg ( _msg.data() , _msg.size() );
        http::field name = http::field::server;

        if (type == websocket_type_t::SERVER)
        {
            name = http::field::server;
        }
        else if( type == websocket_type_t::CLIENT)
        {
            name = http::field::user_agent;
        }
        websocket().set_option(websocket::stream_base::decorator(
            [msg = std::move(msg), name]
            (websocket::response_type& res)
        {
            res.set(name,
                msg);
        }));
    }

    bool co_websocket::set_other_option(std::function<void (websocket_type * ws_ptr)> cb)
    {
        if (!cb)
        {
            return false;
        }
        if (is_empty())
        {
            cb(nullptr);
        }
        websocket_type & w = websocket();
        cb(&w);
        return true;
    }

    websocket_data_type_t co_websocket::get_data_type()const
    {
        if (is_empty())[[unlikely]]
        {
            return websocket_data_type_t::NONE;
        }
        if (websocket().got_text())
        {
            return websocket_data_type_t::TEXT;
        }
        return websocket_data_type_t::BINARY;
    }

    awaitable<bool> co_websocket::handshake_with_server
        (std::string host, std::string target)
    {
        if (is_empty())
        {
            co_return false;
        }
        co_await websocket().async_handshake(host, target,use_awaitable);
        co_return true;
    }

    awaitable<std::tuple<int64_t, std::string>> 
        co_websocket::try_recv() 
    {
        flat_buffer buffer_;
        int64_t nread = co_await websocket().async_read(buffer_, use_awaitable);

        if (nread > 0)[[likely]]
        {
            co_return std::tuple<int64_t , std::string>{ 
                    nread , 
                    buffers_to_string(buffer_.data())
                };
        }
        co_return std::tuple<int64_t , std::string>
            { nread , {}};
    }

    awaitable<int64_t> co_websocket::try_send(std::string && data)
    {
        int64_t nbytes = co_await websocket()
            .async_write(buffer(std::move(data)), 
            use_awaitable);
        co_return nbytes;
    } 

    awaitable<int64_t> co_websocket::try_send(std::string const & data)
    {
        co_return co_await try_send(std::string(data));
    }

    co_websocket::websocket_type & co_websocket::websocket() 
    {
        return *m_websocket;
    }

    co_websocket::websocket_type const & co_websocket::websocket() const
    {
        return *m_websocket;
    }


}