#include "co_io_context_manager.hh"
#include "co_sockstream.hh"
#include "co_sockstream_awaiter.hh"
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/this_coro.hpp>

using namespace chrindex;
using namespace boost::asio;

awaitable<void> echo_server(andren_boost::co_sockstream sock) {
  auto [nread, buffer] = co_await sock.async_read(1024);

  fprintf(stdout, "nread = %ld, content=[%s]\n", nread, buffer.c_str());
  if (nread > 0) {
    std::string data(&buffer[0], nread);
    int64_t nwrite = co_await sock.async_write(data);
    auto executor = co_await this_coro::executor;

    fprintf(stdout, "nwrite = %ld bytes.\n", nwrite);
    co_spawn(executor, echo_server(std::move(sock)), boost::asio::detached);
  } else {
    fprintf(stdout, "nread = [%ld] , client disconnected.\n", nread);
  }
  co_return;
}

awaitable<void> tcp_accept_link() {
  auto executor = co_await this_coro::executor;
  andren_boost::co_sockstream_awaiter accept(executor, "0.0.0.0", 8080);

  while (1) {
    andren_boost::co_sockstream sock = co_await accept.async_accept();
    fprintf(stdout, "new connection [%s:%d].\n",
            sock.peer_endpoint().address().to_string().c_str(),
            sock.peer_endpoint().port());

    executor = co_await this_coro::executor;
    co_spawn(executor, echo_server(std::move(sock)),
             boost::asio::detached);
  }
  co_return;
}

int main(int argc, char **argv) {
  andren_boost::co_io_context_manager ctx_manager;
  ctx_manager.create_and_then([](boost::asio::io_context &ctx,
                                             uint64_t uid) {
    co_spawn(ctx, tcp_accept_link(), boost::asio::detached);
  });
  return 0;
}
