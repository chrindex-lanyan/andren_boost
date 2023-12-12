#pragma once

#include <functional>
#include <utility>
#include <exception>
#include <sys/socket.h>
#include <future>
#include <tuple>
#include <vector>
#include <string>

#ifndef BOOST_ASIO_HAS_CO_AWAIT
#define BOOST_ASIO_HAS_CO_AWAIT
#endif

#include <boost/asio/awaitable.hpp>
#include <boost/asio/ip/address.hpp>
#include <boost/system/error_code.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/ip/multicast.hpp>

#include <boost/asio/use_awaitable.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>

namespace chrindex::andren_boost
{
    using namespace boost::asio;

    class co_user_datagram
    {
    public :
        co_user_datagram();
        co_user_datagram(io_context & ctx);
        co_user_datagram(io_context &ioctx, std::string const & bind_ip, uint16_t bind_port) ;
        co_user_datagram(co_user_datagram const &) = delete;
        co_user_datagram(co_user_datagram &&u) noexcept ;
        ~co_user_datagram();

        void operator=(co_user_datagram &&u) noexcept;

        bool set_option(std::function<void(ip::udp::socket & sock)> cb);

        bool add_remote_for_multicast(std::vector<std::string> const & group);

        awaitable<int64_t> async_broadcast_local(std::string const & data, uint16_t port);

        awaitable<int64_t> async_broadcast_local(std::string const & data, ip::udp::endpoint const & remote);

        awaitable<int64_t> async_broadcast_directly(std::string const & data, std::string const & netid, uint16_t port);

        awaitable<int64_t> async_broadcast_directly(std::string const & data,  ip::udp::endpoint const & remote);

        awaitable<int64_t> async_multicast_send(std::string const & data, int target_port);

        awaitable<std::tuple<int64_t,std::string, ip::udp::endpoint>> async_multicast_recv(int target_port);
        
        awaitable<int64_t> async_send_to(std::string const & data, std::string const & remote_ip, int remote_port);

        awaitable<int64_t> async_send_to(std::string const & data, ip::udp::endpoint const & remote);

        awaitable<std::tuple<int64_t,std::string, ip::udp::endpoint>> async_receive_from();


    private :

        ip::udp::socket sock;
        std::vector<ip::address> multicast_address;
        

    };

}