
#include <chrono>
#include <memory>
#include <thread>

#include <atomic>
#include <utility>
#include <cstdio>

#include "co_io_context_manager.hh"


using boost::asio::detached;
using boost::asio::awaitable;
using boost::asio::co_spawn;

using namespace chrindex;

awaitable<void> print_2(andren_boost::io_context_info info , int val){
    fprintf(stdout, "[%d] print_2 is OK!\n",val);
    co_return ;
}

awaitable<void> print_hello(andren_boost::io_context_info info)
{
    for (int i = 0; i < 4 ; i++)
    {
        fprintf(stdout, "[%d] print_hello is OK!\n",i);
        co_await print_2(info , i);
    }
    co_return ;
}

int main(int argc, char ** argv)
{
    andren_boost::co_io_context_manager ctx_manager;

    ctx_manager.create_and_then([&ctx_manager] (boost::asio::io_context & ctx, uint64_t uid) 
    {
        fprintf(stdout, "io_context (uid [%zu]) co_spawn a task.\n",uid);
        co_spawn(ctx,
            print_hello({ &ctx_manager, uid }),
            detached);
    });
    return 0;
}

