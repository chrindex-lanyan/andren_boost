

#include "boost_tcp_udp/co_io_context_manager.hh"
#include "boost_websocket/co_websocket.hh"
#include "co_sockstream.hh"

#include <boost/asio.hpp>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/this_coro.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <cstdio>

//using namespace chrindex::andren;
using namespace chrindex::andren_boost;


std::string const host = "127.0.0.1" ; 
int const port = 8080;

awaitable<void> websocket_client(co_sockstream & sockstream)
{
    co_websocket websocket{std::move(sockstream)};
            
    websocket.set_websocket_type(websocket_type_t::CLIENT);
    websocket.set_handshake_message(websocket_type_t::CLIENT,
        std::string(
            BOOST_BEAST_VERSION_STRING) 
                + " websocket-server-coro");
    bool bret = co_await websocket.handshake_with_server(host, "/");
    if (!bret)
    {
        fprintf(stdout, "cannot handshake with server.\n");
        co_return ;
    }

    for (int i =0 ; i < 3 ; i++)
    {
        int64_t nwrite = co_await 
            websocket.try_send("hello world - " + std::to_string(i));
        fprintf(stdout,"send [%ld] bytes.\n",nwrite);

        auto [nread, data] = co_await websocket.try_recv();
        fprintf(stdout,"recv [%ld] bytes, data=[%s]\n"
            ,nread, data.c_str());
    }
    fprintf(stdout,"end.\n");
    co_return ;
}

awaitable<void> tcp_client() 
{
    auto resolver = boost::asio::use_awaitable.as_default_on(
            boost::asio::ip::tcp::resolver(
                co_await boost::asio::this_coro::executor));

    auto result = co_await resolver
        .async_resolve(host, std::to_string(port));
    
    fprintf(stdout,"resolved : [%s:%d].\n", 
            result->endpoint().address().to_string().c_str(),
            result->endpoint().port());

    co_sockstream sock(
        co_sockstream::base_sock_type{
            co_await boost::asio::this_coro::executor});
    
    int ret = co_await sock.async_connect(result);
    if (ret !=0)
    {
        fprintf(stdout, "cannot connect server.\n");
        co_return ;
    }
    else 
    {
        fprintf(stdout,"connected [%s:%d].\n", 
            result->endpoint().address().to_string().c_str(),
            result->endpoint().port());
    }
    co_await websocket_client(sock);
    co_return ;
}


int main(int argc , char ** argv)
{
    co_io_context_manager io_context_manager;

    io_context_manager.create_and_then([](io_context & ctx, uint64_t id){
        boost::asio::co_spawn(ctx,tcp_client(),boost::asio::detached);
    });
    return 0;
}



