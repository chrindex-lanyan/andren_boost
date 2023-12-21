#include "co_io_context_manager.hh"
#include "co_sockstream.hh"
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/this_coro.hpp>
#include <cstdio>

using namespace chrindex;
using namespace boost::asio;

awaitable<void> tcp_client() {
  andren_boost::co_sockstream sock(
    andren_boost::co_sockstream::base_sock_type{
        co_await this_coro::executor});
  int ret = co_await sock.async_connect("127.0.0.1", 8080);

  if (ret == 0) {
    std::string buffer = "hello world!!";
    int64_t nwrite = co_await sock.async_write(buffer);

    if (nwrite > 0) {
      auto [nread, data] = co_await sock.async_read();

      if (nread > 0) {
        fprintf(stdout, "received data [%s].\n", data.c_str());
      } else {
        fprintf(stdout, "read data from server failed.\n");
      }
    } else {
      fprintf(stdout, "write data to server failed.\n");
    }
  } else {
    fprintf(stdout, "connect server failed.\n");
  }
  co_return;
}

int main(int argc, char **argv) {
  andren_boost::co_io_context_manager io_context_manager;
  io_context_manager.create_and_then([](io_context &ctx, uint64_t uid) {
    co_spawn(ctx, tcp_client(), boost::asio::detached);
  });
  return 0;
}
