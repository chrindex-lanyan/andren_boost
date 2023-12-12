
#include "co_user_datagram.hh"
#include <boost/asio/ip/address.hpp>
#include <boost/asio/ip/multicast.hpp>
#include <boost/asio/ip/udp.hpp>
#include <boost/asio/use_awaitable.hpp>

namespace chrindex::andren_boost
{
    static io_context _default_io_context{1};

    co_user_datagram::co_user_datagram():sock(_default_io_context){}
    co_user_datagram::co_user_datagram(io_context & ctx):sock(ctx){ sock.set_option(ip::udp::socket::reuse_address()); }
    co_user_datagram::co_user_datagram(io_context & ctx, std::string const & bind_ip, uint16_t bind_port) 
        :sock(ctx, ip::udp::endpoint(ip::address::from_string(bind_ip), bind_port))
        { sock.set_option(ip::udp::socket::reuse_address()); }
    co_user_datagram::co_user_datagram(co_user_datagram &&u)noexcept 
        :sock(std::move(u.sock)){ sock.set_option(ip::udp::socket::reuse_address()); }
    co_user_datagram::~co_user_datagram(){}
    
    void co_user_datagram::operator=(co_user_datagram &&u)noexcept { sock = std::move(u.sock); }

    bool co_user_datagram::set_option(std::function<void(ip::udp::socket & sock)> cb)
    {
        if (cb)
        {
            cb (sock);
            return true;
        }
        return false;
    }

    bool co_user_datagram::add_remote_for_multicast(std::vector<std::string> const & group)
    {
        for (auto & ep : group)
        {
            ip::address addr = ip::address::from_string(ep);
            sock.set_option(ip::multicast::join_group(addr));
            multicast_address.push_back(addr) ;
        }
        return true;
    }

    awaitable<int64_t> co_user_datagram::async_broadcast_local(std::string const & data , uint16_t remote_port)
    {
        co_return co_await async_broadcast_local(data,ip::udp::endpoint(ip::address::from_string("255.255.255.255"),remote_port));
    }

    awaitable<int64_t> co_user_datagram::async_broadcast_local(std::string const & data, ip::udp::endpoint const & remote)
    {
        co_return co_await async_send_to(data,remote);
    }

    awaitable<int64_t> co_user_datagram::async_broadcast_directly(std::string const & data, std::string const & netid ,uint16_t remote_port)
    {
        co_return co_await async_broadcast_directly(data,ip::udp::endpoint(ip::address::from_string(netid),remote_port));
    }

    awaitable<int64_t> co_user_datagram::async_broadcast_directly(std::string const & data,  ip::udp::endpoint const & remote)
    {
        co_return co_await async_send_to(data,remote);
    }

    awaitable<int64_t> co_user_datagram::async_multicast_send(std::string const & data, int target_port)
    {
        int count = 0;
        for (auto & addr : multicast_address)
        {
            co_await async_send_to(data, ip::udp::endpoint(addr, target_port));
            count ++;
        }
        co_return count;
    }
    
    awaitable<int64_t> co_user_datagram::async_send_to(std::string const & data, std::string const & remote_ip, int remote_port)
    {
        co_return co_await async_send_to(data,ip::udp::endpoint(ip::address::from_string(remote_ip), remote_port ));
    }

    awaitable<int64_t> co_user_datagram::async_send_to(std::string const & data, ip::udp::endpoint const & remote)
    {
        int64_t nwrite = 0;
        try
        {
            nwrite = co_await sock.async_send_to(
                boost::asio::buffer(&data[0], data.size()), 
                remote,
                use_awaitable);
        }
        catch(std::exception & e)
        {
            co_return -1;
        }
        co_return nwrite;
    }

    awaitable<std::tuple<int64_t,std::string,ip::udp::endpoint>> co_user_datagram::async_multicast_recv(int target_port)
    {
        for (auto & addr : multicast_address)
        {
            sock.set_option(ip::multicast::join_group(addr));
        }
        ip::udp::endpoint sender;
        co_return co_await async_receive_from();
    }

    awaitable<std::tuple<int64_t,std::string, ip::udp::endpoint>> co_user_datagram::async_receive_from()
    {
        auto [nread , data , sender] = 
            std::tuple<int64_t,std::string ,ip::udp::endpoint>();

        data.resize(1600);
        nread = co_await sock.async_receive_from(
            boost::asio::buffer(data, data.size()), 
            sender,
            use_awaitable
        );
        data.resize(nread > 0 ? nread : 0);

        co_return std::tuple<int64_t,std::string, ip::udp::endpoint>
            {
                std::move(nread),
                std::move(data) ,
                std::move(sender)
            };
    }


}

