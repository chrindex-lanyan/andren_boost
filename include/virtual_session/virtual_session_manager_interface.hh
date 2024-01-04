#pragma  once 

#include "co_websocket.hh"
#include <functional>

namespace chrindex::andren_boost
{
    using namespace boost::asio;
    using namespace boost::beast;

    class virtual_session_manager_interface
    {
    public:
        virtual_session_manager_interface() = default;
        virtual ~virtual_session_manager_interface() = default;

    public:

        virtual bool is_empty() const noexcept = 0;

        virtual bool is_closed() const noexcept = 0;

        virtual void set_on_recv(std::function<
            void(int64_t,std::string data)> cb) noexcept = 0;
        
        virtual void set_on_write(std::function<
            void(int64_t)> cb
        ) noexcept = 0;

        virtual int64_t request_to_write() noexcept = 0;

        virtual void close_virtual_session() noexcept;

        virtual void open_virtual_session() noexcept;

    };



}