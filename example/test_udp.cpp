
#include "co_user_datagram.hh"
#include "co_io_context_manager.hh"


using namespace boost::asio;
using namespace chrindex::andren_boost;




awaitable<void> create_udp_listener(io_context_info info)
{

    co_return ;
}


int main(int argc , char ** argv)
{
    co_io_context_manager ctx_manager;
    ctx_manager.create_and_then([&ctx_manager](boost::asio::io_context &ctx,
                                             uint64_t uid) {
        co_spawn(ctx, create_udp_listener(
            {&ctx_manager, uid}), 
            boost::asio::detached);
    });
    return 0;
}

