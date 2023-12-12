
#include "co_io_context_manager.hh"


#include "uid_creator.hh"

namespace  chrindex::andren_boost
{
    struct io_ctx_pair
    {
        io_context io_ctx_;
        std::atomic<int> flag_exit_;
        uint64_t uid_;
        signal_set signal_set_;
        std::jthread thread_;

        io_ctx_pair(uint64_t uid) : 
            
            io_ctx_ (1),
            flag_exit_(0), uid_(uid),
            signal_set_(io_ctx_,SIGINT | SIGTERM){}

        ~io_ctx_pair() {  }

        bool start()
        {
            flag_exit_ = 0;
            thread_ = std::jthread([this]()
            { 
                io_ctx_.run(); 
            }); 
            return true;
        }

        void stop()
        {
            flag_exit_ = 1;
            if (thread_.joinable())
            {
                thread_.join();
            }
        }
    };

    uint64_t get_uid_by_io_ctx_pair(io_ctx_pair * p_ctx_pair) 
    {
        return p_ctx_pair->uid_;
    }

}

namespace chrindex::andren_boost
{
    co_io_context_manager::co_io_context_manager (){}

    co_io_context_manager::~co_io_context_manager (){}

    bool co_io_context_manager::create_and_then(std::function<void( io_context & ctx  , uint64_t uid)> cb)
    {
        uint64_t uid = andren::base::create_uid_u64();
        auto sp_ioctx_pair = std::make_shared<io_ctx_pair>(uid);

        m_io_ctx_pairs[uid] = sp_ioctx_pair;
        if (sp_ioctx_pair->start()) 
        {
            if (cb)
            {
                cb(sp_ioctx_pair->io_ctx_, uid);
            }
            return true;
        }
        return false;
    }

    bool co_io_context_manager::stop_and_before(uint64_t uid , std::function<void( io_context & ctx  , uint64_t uid)> cb)
    {
        auto iter = m_io_ctx_pairs.find(uid);
        if (iter == m_io_ctx_pairs.end())
        {
            return false;
        }
        if(cb)
        {
            cb(iter->second->io_ctx_,uid);
        }
        iter->second->stop();
        return true;
    }

    io_context * co_io_context_manager::find_io_context_by_uid(int64_t uid)
    {
        auto iter = m_io_ctx_pairs.find(uid);

        if (iter != m_io_ctx_pairs.end())
        {
            return &(iter->second->io_ctx_);
        }
        return nullptr;
    }

}