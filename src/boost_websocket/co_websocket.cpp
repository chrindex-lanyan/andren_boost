#include "co_websocket.hh"

#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

#include <boost/asio/io_context.hpp>
#include <boost/beast/core/error.hpp>
#include <boost/beast/http/field.hpp>
#include <boost/beast/http/fields.hpp>
#include <exception>
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

    bool co_websocket::is_empty() const  noexcept
    {
        return m_websocket == nullptr;
    }

    bool co_websocket::is_closed() const noexcept
    {
        return !is_empty() && m_websocket->is_open() == false;
    }

    void co_websocket::set_data_type(websocket_data_type_t type)  noexcept
    {
        if (is_empty() || type == websocket_data_type_t::NONE)[[unlikely]]
        {
            return ;
        }
        websocket().text(type == websocket_data_type_t::TEXT);
    }

    bool co_websocket::set_websocket_type(websocket_type_t type)  noexcept
    {
        try 
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
        catch (std::exception e)
        {
            return false;
        }
        return true;
    }

    bool co_websocket::set_handshake_message(websocket_type_t type, std::string_view _msg) noexcept
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

        try 
        {
            websocket().set_option(websocket::stream_base::decorator(
            [msg = std::move(msg), name]
            (websocket::response_type& res)
            {
                res.set(name,
                    msg);
            }));
        }
        catch (std::exception e)
        {
            return false;
        }
        return true;
    }

    bool co_websocket::set_other_option(std::function<void (websocket_type * ws_ptr)> cb) noexcept
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
        try 
        {
            cb(&w);
        }
        catch (std::exception e)
        {
            return false;
        }
        return true;
    }

    websocket_data_type_t co_websocket::get_data_type() const noexcept
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
        (std::string host, std::string target) noexcept
    {
        if (is_empty())
        {
            co_return false;
        }
        try 
        {
            co_await websocket().async_handshake(host, target,use_awaitable);
        }
        catch (std::exception e)
        {
            co_return false;
        }
        co_return true;
    }

    awaitable<std::tuple<int64_t, std::string>> 
        co_websocket::try_recv() noexcept
    {
        flat_buffer buffer_;
        int64_t nread = 0 ; 
        try 
        {
            nread = co_await websocket().async_read(buffer_, use_awaitable);
        }
        catch (std::exception e)
        {
            co_return std::tuple<int64_t , std::string>
            { nread , {}};
        }
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

    awaitable<int64_t> co_websocket::try_send(std::string && data) noexcept
    {
        int64_t nbytes = 0;
        try 
        {
            nbytes = co_await websocket()
            .async_write(buffer(std::move(data)), 
            use_awaitable);
        }
        catch(std::exception e)
        {
            nbytes = -1;
        }
        co_return nbytes;
    } 

    awaitable<int64_t> co_websocket::try_send(std::string const & data) noexcept
    {
        int64_t nwrite = 0;
        try {
            nwrite = co_await try_send(std::string(data));
        }
        catch (std::exception )
        {
            co_return -1;
        }
        co_return nwrite;
    }

    awaitable<bool> co_websocket::try_accept_client() noexcept
    {
        try 
        {
            co_await websocket().async_accept();
        }
        catch (std::exception )
        {
            co_return false;
        }
        co_return true;
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