
#include "azure_protocol.hh"
#include "co_websocket.hh"
#include "co_sockstream.hh"
#include "co_io_context_manager.hh"
#include "co_sockstream_awaiter.hh"

#include <cstdio>
#include <list>

/*
 设计思路：
   1. 一个websocket server 和一个 websocket client；
   2. 仅一个线程；
   3. 每个websocket连接，提供一个azure_protocol收发器；
   4. 每个连接一共并发8个fragment group；
   6. server和client各有一个连接；

*/


using namespace chrindex::andren_boost;

constexpr size_t size = 1024 * 1024 * 32 + 26; // 32M + 26B
static char const dict[62] = 
{
    'a','b','c','d',
    'e','f','g','h',
    'i','j','k','l',
    'm','n','o','p',
    'q','r','s','t',
    'u','v','w','x',
    'y','z',
    'A','B','C','D',
    'E','F','G','H',
    'I','J','K','L',
    'M','N','O','P',
    'Q','R','S','T',
    'U','V','W','X',
    'Y','Z',
    '0','1','2','3','4',
    '5','6','7','8','9',
};

char rand_char(size_t x) 
{ 
    return dict[ (x+(x/2)*x) % sizeof(dict)]; 
};

struct messages_t 
{
    std::string messages ;

    messages_t () 
    {
        for (size_t i =0; i < size; i ++)
        {
            messages.push_back(rand_char(i));
        }
    }

    messages_t (messages_t && _) noexcept
    {
        messages = std::move(_.messages);
    }

    void operator =(messages_t && _)noexcept
    {
        messages = std::move(_.messages);
    }

    std::string const & get_messages() const
    {
        return messages;
    }

    bool compare_with(std::string const & data) const
    {
        return data == messages;
    }
};

static messages_t messages;
std::string const host = "127.0.0.1" ; 
int const port = 8080;


//// ############### client ##############

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
        fprintf(stdout, "[client] cannot handshake with server.\n");
        co_return ;
    }
    fprintf(stdout, "[client] handshake done.\n");

    fragment_group_sender_t azure_sender;
    fragment_group_receiver_t azure_receiver;
    std::map<uint64_t,fragment_group_request_t> request_list;
    std::vector<std::tuple<uint64_t, std::string>> data_list;

    for (int i = 0 ; i < 5 ; i++)
    {
        /// 创建fragment group
        /// 准备request
        /// 保存request
        /// 添加到group到sender
        fragment_group_t group;
        fragment_group_request_t request;

        group.init_a_new_group(
            i + 1001,  
            i + 1, 
            512 , 
            messages.get_messages());
        request.init_request(group);
        
        request_list[request.gid]=(std::move(request));
        azure_sender.append_group(std::move(group));
    }
    websocket.set_data_type(websocket_data_type_t::TEXT);

    for (auto & req_iter : request_list)
    {
        /// 使用sender循环发送request
        co_await websocket.try_send(req_iter.second.create_request());
    }

    int count = 0;
    while (count < 5)
    {
        /// 发送数据（如果有）
        /// 循环接收数据
        /// move数据到receiver解析
        /// 判断是否有数据已就绪：
        ///     1. 收到了对端的response，准备一个组的fragments到就绪队列。
        if(auto ready_data = azure_sender.fecth_some_data();
            ready_data.empty() == false)
        {
            int64_t nwrite = co_await websocket.try_send(ready_data);
            if(nwrite <= 0)
            {
                fprintf(stdout, "[Client] Send Failed.\n");
            }
        }

        auto [nread, data] = co_await websocket.try_recv();
        if (nread <= 0)
        {
            break;
        }
        azure_receiver.append_some_data(std::move(data));

        if(azure_receiver.response_count() > 0)
        {
            auto _response = azure_receiver.fecth_one_group_response();
            if (auto iter = request_list
                    .find(_response->target_gid);
                _response.has_value() && iter != request_list.end())
            {
                auto & response= _response.value();
                /// 让发送器准备发送
                azure_sender.notify_a_group(response);
            }
        }
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
    fragment_group_receiver_t azure_receiver;

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
    messages = messages_t{};

    io_context_manager.create_and_then([](io_context & ctx, uint64_t id){
        co_spawn(ctx,tcp_client(),boost::asio::detached);
    });

    io_context_manager.create_and_then([](io_context & ctx, uint64_t id){
        co_spawn(ctx, tcp_acceptor(), boost::asio::detached);
    });

    return 0;
}

