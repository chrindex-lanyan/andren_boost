

#include "co_websocket.hh"
#include "co_sockstream.hh"
#include "co_io_context_manager.hh"
#include "co_sockstream_awaiter.hh"

#include <boost/asio/awaitable.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/this_coro.hpp>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <list>
#include <string>
#include <thread>

#include "multiplex.h"


using namespace chrindex::andren_boost;


std::string const host = "127.0.0.1" ; 
int const port = 8080;

int count = 20;


////////////// session /////////////

/// server's
awaitable<void> do_session(co_websocket websocket)
{
    multiplex_manager multiplex ; 
    std::map<uint64_t, std::tuple<size_t,size_t>> already_recv;
    int server_count = 0;

    multiplex.setOnNewRequest([&](multiplex_meta_t meta)
    {
        fprintf(stdout,
        "[server] new group, que_no=%d, id=[%lu], all_size=[%lu]\n",
            (int)meta.que_no,
            meta.id,
            meta.all_size);
        already_recv[meta.id] = {meta.all_size , 0};
    });

    multiplex.setOnRecvPackage([&]
            (multiplex_package_meta_t package_meta)
    {
        auto & [all_size, recv_size]
             = already_recv[package_meta.id] ;
        recv_size += package_meta.size;
        if (recv_size == all_size)
        {
            auto [bret, data] = multiplex.take_out_recv_in_advance(package_meta.id);
            if (bret)
            {
                fprintf(stdout,
                    "[server] recv id=[%lu] done, content = [%s]\n",
                    package_meta.id, data.c_str());
                server_count++;
                fprintf(stdout,
                    "[server] current done [%d]\n",
                    server_count);
            }
        }
    });

    while (1)
    {
        auto [nread, data] = co_await websocket.try_recv();
        multiplex.parse_raw_data(data);
        printf("nread =%ld, ll = %d , current = %d.\n",
        nread,count,server_count);
        if (nread <= 0 || server_count >= count)
        {
            break;
        }
    }    
    fprintf(stdout, "[server] end...\n");
    co_return ;
}

/// client's
awaitable<void> do_client(co_websocket websocket)
{
    multiplex_manager multiplex ; 

    for (uint64_t index = 0; index < count ; index ++ )
    {
        multiplex.add_data_task_to_send(index, 
            (multiplex_manager::QueueNum)(index % 8), 
            "hello world  - "+std::to_string(index));
    }
    std::vector<std::string> dataList;
    do
    {
        dataList = multiplex.fetch_some_data_to_send();
        fprintf(stdout,"[client] need send %lu package.\n", dataList.size());
        for (auto & data : dataList)
        {
            co_await websocket.try_send(data);
        }
    }while(!dataList.empty());
    fprintf(stdout,"[client] end....\n");
    co_return ;
}



//// ############### client ##############

awaitable<void> websocket_client(co_websocket websocket)
{
    websocket.set_websocket_type(websocket_type_t::CLIENT);
    websocket.set_data_type(websocket_data_type_t::BINARY);
    websocket.set_handshake_message(websocket_type_t::CLIENT,
        std::string(
            BOOST_BEAST_VERSION_STRING) 
                + " websocket-server-coro");
    bool bret = co_await websocket.handshake_with_server(host, "/");
    if (!bret)
    {
        fprintf(stdout, "[client] cannot handshake with server.\n");
        co_return ;
    }
    fprintf(stdout, "[client] handshake done.\n");

    auto executor = co_await this_coro::executor;
    co_spawn(executor,do_client(std::move(websocket)),detached);
    
    co_return ;
}

awaitable<void> tcp_client() 
{
    auto resolver = boost::asio::use_awaitable.as_default_on(
            boost::asio::ip::tcp::resolver(
                co_await boost::asio::this_coro::executor));

    auto result = co_await resolver
        .async_resolve(host, std::to_string(port));
    
    fprintf(stdout,"[client] resolved [%s:%d].\n", 
            result->endpoint().address().to_string().c_str(),
            result->endpoint().port());

    co_sockstream sock(
        co_sockstream::base_sock_type{
            co_await boost::asio::this_coro::executor});
    
    int ret = co_await sock.async_connect(result);
    if (ret !=0)
    {
        fprintf(stdout, "[client] cannot connect server.\n");
        co_return ;
    }
    else 
    {
        fprintf(stdout,"[client] connected [%s:%d].\n", 
            result->endpoint().address().to_string().c_str(),
            result->endpoint().port());
    }
    co_await websocket_client(co_websocket{std::move(sock)});
    co_return ;
}

//// ############### server ##############

awaitable<void> websocket_server(co_sockstream sockstream)
{
    co_websocket websocket{std::move(sockstream)};

    websocket.set_data_type(websocket_data_type_t::BINARY);
    websocket.set_websocket_type(websocket_type_t::SERVER);
    websocket.set_handshake_message(websocket_type_t::SERVER,
        std::string(
            BOOST_BEAST_VERSION_STRING) 
                + " websocket-server-coro");
    bool bret = co_await websocket.try_accept_client();
    if (!bret)
    {
        fprintf(stdout, "[server] cannot handshake with client.\n");
        co_return ;
    }
    fprintf(stdout, "[server] handshake done.\n");

    auto executor = co_await this_coro::executor;
    co_spawn(executor,
        do_session(std::move(websocket)) ,
        boost::asio::detached);

    co_return ;
}

awaitable<void> tcp_acceptor()
{
    auto executor = co_await this_coro::executor;
    co_sockstream_awaiter accept(executor, "0.0.0.0", 8080);

    while (1) 
    {
        co_sockstream sock = co_await accept.async_accept();
        fprintf(stdout, "[server] new connection [%s:%d].\n",
            sock.peer_endpoint().value().address().to_string().c_str(),
            sock.peer_endpoint().value().port());

        executor = co_await this_coro::executor;
        co_spawn(executor, websocket_server(std::move(sock)),
             boost::asio::detached);
    }
    co_return ;
}

/////////////##############

int main(int argc , char ** argv)
{
    co_io_context_manager iocm1;
    iocm1.create_and_then([](io_context & ctx, uint64_t id){
        co_spawn(ctx, tcp_acceptor(), boost::asio::detached);
    });

    std::thread([]()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        co_io_context_manager  iocm2;
        iocm2.create_and_then([](io_context & ctx, uint64_t id){
            co_spawn(ctx,tcp_client(),boost::asio::detached);
        });
    }).join();

    return 0;
}

