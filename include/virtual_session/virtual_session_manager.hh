#pragma once



#include "co_websocket.hh"
#include "virtual_session.hh"
#include "virtual_session_manager_interface.hh"

#include <set>

namespace chrindex::andren_boost
{
    
    class virtual_session_manager 
        : public virtual_session_manager_interface
    {
    public :

        virtual_session_manager(){}
        virtual_session_manager(co_websocket && websocket)noexcept 
            : m_websocket(std::move(websocket)) {}
        ~virtual_session_manager(){}
        void operator=(virtual_session_manager const &) = delete;
        void operator=(virtual_session_manager && _) noexcept {}
    
    public :

        


    public : // override list

        bool is_empty() const noexcept override { return m_websocket.is_empty(); }

        bool is_closed() const noexcept override { return m_websocket.is_closed(); };

        void set_on_recv(std::function<
            void(int64_t,std::string data)> cb) noexcept override
        {}
        
        void set_on_write(std::function<
            void(int64_t)> cb
        ) noexcept override
        {}

        int64_t request_to_write() noexcept override
        {
            return 0;
        }

        void close_virtual_session() noexcept override
        {}

        void open_virtual_session() noexcept override
        {}


    private :
        co_websocket m_websocket;
        std::set<uint64_t> virtual_session_list;
    };
    
}

