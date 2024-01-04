#pragma  once

#include <cstdint>
#include <string>
#include <deque>
#include <stdint.h>
#include <memory>

#include "virtual_session_manager_interface.hh"

namespace chrindex::andren_boost
{
    class virtual_session 
    {
    public :

        using manager_type = 
            std::weak_ptr<virtual_session_manager_interface>;

        virtual_session()
        {
            m_session_id = 0;
            m_is_closed = true;
        }

        virtual_session(uint64_t session_id , manager_type manager) noexcept 
        { 
            m_session_id = session_id;
            m_is_closed = true;
            m_manager_ptr = manager; 
        }

        ~virtual_session()
        {
            if (m_is_closed == false)
            {
                
            }
        }

        /// 添加一个数据包到等待队列
        bool append_write_request(std::string package) noexcept;

        /// 接收一个数据包，如果有接收完成的数据包
        std::string accept_package_if_no_empty() noexcept;

        /// 数据包接收队列大小
        size_t recv_queue_size() const noexcept;

        /// 数据包等待队列大小
        size_t write_queue_size() const noexcept;

        /// 会话管理器的有效性
        bool is_empty() const noexcept 
        { 
            auto manager = get_manager(); 
            return !manager || manager->is_empty()
                || manager->is_closed(); 
        }

        /// 会话的可用性
        bool is_closed() const noexcept 
        {
            return is_empty() || m_is_closed == false;
        }

        uint64_t session_id() const noexcept;

        /// 提交到就绪队列以准备发送
        bool try_commit() const noexcept;

    private :

        std::shared_ptr<virtual_session_manager_interface> 
            get_manager() const noexcept 
        {
            return m_manager_ptr.lock();   
        }

    private :

        uint64_t m_session_id;
        bool m_is_closed;
        manager_type m_manager_ptr;
        
        std::deque<std::string> m_recv_package_list;
        std::deque<std::string> m_write_package_list;
    };

}