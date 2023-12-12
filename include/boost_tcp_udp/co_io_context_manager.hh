#pragma once

#include <functional>
#include <memory>
#include <thread>
#include <atomic>
#include <utility>

#ifndef BOOST_ASIO_HAS_CO_AWAIT
#define BOOST_ASIO_HAS_CO_AWAIT
#endif

#include <boost/asio/this_coro.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/signal_set.hpp>


namespace chrindex::andren_boost
{
    using namespace boost::asio;

    struct io_ctx_pair;

    class co_io_context_manager 
    {
    public :

        co_io_context_manager ();
        co_io_context_manager(co_io_context_manager const &) = delete;
        co_io_context_manager(co_io_context_manager &&) = delete;
        ~co_io_context_manager ();

        bool create_and_then(std::function<void( io_context & ctx  , uint64_t uid)> cb);

        bool stop_and_before(uint64_t uid , std::function<void( io_context & ctx  , uint64_t uid)> cb);

        io_context * find_io_context_by_uid(int64_t uid);

    private :

        std::map<uint64_t ,std::shared_ptr<io_ctx_pair>> m_io_ctx_pairs;
    };

    struct io_context_info
    {
        co_io_context_manager * p_manager;
        uint64_t uid;

        io_context_info(
            co_io_context_manager * _p_manager,
            uint64_t _uid):
            p_manager(_p_manager),uid(_uid){}
    };

}
