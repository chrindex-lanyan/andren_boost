
#include "azure_protocol.hh"
#include "co_websocket.hh"
#include "co_sockstream.hh"
#include "co_io_context_manager.hh"
#include "co_sockstream_awaiter.hh"


/*
 设计思路：
   1. 一个websocket server 和一个 websocket client；
   2. 仅一个线程；
   3. 每个websocket连接，提供一个azure_protocol收发器；
   4. 每个连接一共并发8个fragment group；
   6. server和client各有一个连接；

*/


using namespace chrindex::andren_boost;

constexpr size_t size = 1024 * 1024 * 32;

struct messages_t 
{
    std::string messages ;

    messages_t () : messages(size , 'A') {}
    std::string const & get_messages() const
    {
        return messages;
    }

    bool compare_with(std::string const & data)const
    {
        return data == messages;
    }
};

static messages_t messages={};
std::string const host = "127.0.0.1" ; 
int const port = 8080;


//// ############### client ##############

awaitable<void> websocket_client(co_sockstream & sockstream)
{
    co_websocket websocket{std::move(sockstream)};
    fragment_group_sender_t azure_sender;
    fragment_group_receiver_t zazure_receiver;
            
    websocket.set_websocket_type(websocket_type_t::CLIENT);
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

    for (int i =0 ; i < 5 ; i++)
    {
        /// 创建fragment group
        /// 准备receiver
        /// 添加到group到sender
    }

    while (1)
    {
        /// 使用sender循环发送，
        /// 使用receiver循环接收。
        /// 完成后保存
    }

    while(1)
    {
        /// 循环对比数据正确性
    }

    fprintf(stdout,"[client] end.\n");
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


//// ############### server ##############


awaitable<void> websocket_server(co_sockstream sockstream)
{
    co_websocket websocket{std::move(sockstream)};
    fragment_group_sender_t azure_sender;
    fragment_group_receiver_t zazure_receiver;

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

    for (int i =0 ; i < 5 ; i++)
    {
        /// 准备receiver
        /// 准备sender
    }

    while (1)
    {   
        /// 接收request
        /// 发送response
        /// 使用receiver循环接收。
        /// 完成后保存
    }

    while(1)
    {
        /// 循环对比数据正确性
    }


    co_return ;
}


awaitable<void> tcp_acceptor()
{
    auto executor = co_await this_coro::executor;
    co_sockstream_awaiter accept(executor, "0.0.0.0", 8080);

    while (1) {
        co_sockstream sock = co_await accept.async_accept();
        fprintf(stdout, "new connection [%s:%d].\n",
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
    co_io_context_manager io_context_manager;

    io_context_manager.create_and_then([](io_context & ctx, uint64_t id){
        co_spawn(ctx,tcp_client(),boost::asio::detached);
    });

    io_context_manager.create_and_then([](io_context & ctx, uint64_t id){
        co_spawn(ctx, tcp_acceptor(), boost::asio::detached);
    });

    return 0;
}

