#include "virtual_session.hh"
#include <cstdint>
#include <memory>
#include <atomic>
#include <thread>

thread_local std::atomic<uint64_t> virtual_session_id { 1 };

static uint64_t create_virtual_session_id()
{
    uint64_t _tid = std::hash<std::thread::id>()(std::this_thread::get_id()) ;
    _tid = (_tid << 32) + virtual_session_id++;
    return _tid;
}

namespace chrindex::andren_boost
{
    virtual_session::virtual_session()
    {
        context = nullptr;
    }

    virtual_session::virtual_session(virtual_session_enum type)
    {
        context = std::make_unique<_context_t>();
        context->m_session_type = type;
        context->m_vid = create_virtual_session_id() & 0x00ffffff;
        context->m_remote_vid = 0;
        context->m_data_size = 0;
        context->m_keep_alive = false;
        context->m_trans_start = false;
        context->m_per_data_size = DEFAULT_DATA_FRAME_SIZE;
        context->m_status = virtual_session_enum::STATUS_OPEN;
    }

    virtual_session::virtual_session(virtual_session && _ano) noexcept
    {
        context = std::move(_ano.context);
    }

    void virtual_session::operator=(virtual_session && _ano) noexcept
    {
        this->~virtual_session();
        context = std::move(_ano.context);
    }

    virtual_session::~virtual_session()
    {
        if (context && context->m_status 
                == virtual_session_enum::STATUS_ONLINE)
        {
            send_response(virtual_session_enum::
                RESPONSE_STATUS_DISCONNECT,0);
            if (context->m_callback_on_disconnect)
            {
                context->m_callback_on_disconnect(this,true);
            }
        }
    }


    uint32_t virtual_session::vid() const noexcept
    {
        return context == nullptr ? 0 : context->m_vid;
    }

    virtual_session_enum virtual_session::type() const noexcept
    {
        return context == nullptr ? 
            virtual_session_enum::NONE : context->m_session_type;
    }

    /////////// request /////////////

    /// 建立虚拟会话
    bool virtual_session::request_create(std::string data)
    {
        if (!context || context->m_status 
            != virtual_session_enum::STATUS_OPEN)
        {
            return false;
        }

        std::string _package(sizeof(virtual_package_t),0x00);
        virtual_package_t & package = 
            *reinterpret_cast<virtual_package_t*>(&_package[0]);

        package.magic_num = MAGIC_NUM;
        package.package_size = sizeof(package);
        package.type = (uint16_t)virtual_session_enum::PACKAGE_TYPE_CREATE;
        package.vid = vid();
        package.target_vid = 0;
        package.content.request.content = data.size();

        context->m_buffer = std::move(data);
        context->m_data_size = 0;
        context->m_next_send.clear();

        context->m_next_send.push_back(_package);
        return true;
    }

    /// 发送请求
    bool virtual_session::send_request(virtual_session_enum type ,
        uint64_t or_use_extention)
    {
        if (!context || context->m_status 
            != virtual_session_enum::STATUS_ONLINE)
        {
            return false;
        }
        std::string _package(sizeof(virtual_package_t),0x00);
        virtual_package_t & package = 
            *reinterpret_cast<virtual_package_t*>(&_package[0]);

        package.magic_num = MAGIC_NUM;
        package.package_size = sizeof(package);
        package.type = (
            type == virtual_session_enum::NONE ?
                (uint16_t)virtual_session_enum::PACKAGE_TYPE_EXTREQ
            :   (uint16_t)virtual_session_enum::PACKAGE_TYPE_REQUEST);
        package.vid = vid();
        package.target_vid = context->m_remote_vid;
        package.content.request.content = (
            type == virtual_session_enum::NONE ?
                (uint64_t)or_use_extention
            :   (uint64_t)type );
        
        context->m_next_send.push_back(_package);
        return true;
    }

    /// 发送响应
    bool virtual_session::send_response(virtual_session_enum type , 
        uint64_t or_use_extention)
    {
        if (!context || context->m_status 
            != virtual_session_enum::STATUS_ONLINE)
        {
            return false;
        }
        std::string _package(sizeof(virtual_package_t),0x00);
        virtual_package_t & package = 
            *reinterpret_cast<virtual_package_t*>(&_package[0]);

        package.magic_num = MAGIC_NUM;
        package.package_size = sizeof(package);
        package.type = ( type == virtual_session_enum::NONE ?
            (uint16_t)virtual_session_enum::PACKAGE_TYPE_EXTRES
            :(uint16_t)virtual_session_enum::PACKAGE_TYPE_RESPONSE);
        package.vid = vid();
        package.target_vid = context->m_remote_vid;
        package.content.response.status = (type == virtual_session_enum::NONE ?
            or_use_extention
            : (uint64_t) type
            );
        
        context->m_next_send.push_back(_package);
        return true;
    }

    /// 如果收到了会话建立
    void virtual_session::setOnCreateReq(OnCreate callback)
    {
        if (context)
        {
            context->m_callback_on_create = std::move(callback);
        }   
    }
    
    /// 如果收到了请求或者响应
    void virtual_session::setOnReqOrRes(OnReqOrRes callback)
    {
        if (context)
        {
            context->m_callback_on_req_or_res 
                = std::move(callback);
        }    
    }

    /// 如果连接断开(主动或被动)
    void virtual_session::setOnDisconnected(OnDisconnectd callback)
    {
        if (context)
        {
            context->m_callback_on_disconnect = std::move(callback);
        }
    }

    /// 往里吐数据,解析,并尝试取出一个准备发送的数据
    std::string virtual_session::working(
        std::string const & data,uint64_t & used_bytes)
    {
        used_bytes = 0;

        if (context==nullptr)
        {
            return {};
        }

        // 处理数据帧
        send_next_data_frame();
        
        auto _func = [this]()->std::string
        {
            if (context->m_next_send.empty() == false)
            {
                std::string _next_send = 
                    std::move(context->m_next_send.front());
                context->m_next_send.pop_front();
                return _next_send;
            }
            return {};
        };

        if(data.empty())
        {
            return _func();
        }

        if (data.size() != sizeof(virtual_package_t))
        {
            return _func();   
        }

        virtual_package_t const & package =
            *reinterpret_cast<virtual_package_t const*>(data.c_str());
        
        if (package.magic_num != MAGIC_NUM 
            || package.package_size > data.size())
        {
            return _func(); 
        }

        virtual_session_enum package_type =
            (virtual_session_enum)package.type;

        switch (package_type) 
        {
            case virtual_session_enum::PACKAGE_TYPE_CREATE: 
            {
                if(context->m_callback_on_create)
                {
                    bool bret = context->m_callback_on_create(this,
                        package.content.request.content);
                    if (bret) // 是否同意
                    {
                        context->m_remote_vid = package.vid;
                        context->m_buffer.resize(
                            package.content.request.content);
                        context->m_status = 
                            virtual_session_enum::STATUS_ONLINE;
                        
                        context->m_trans_start = true;
                        // 发送response
                        send_response(virtual_session_enum::RESPONSE_STATUS_ACCEPT,
                        0);
                    }
                }
                break;
            };
            case virtual_session_enum::PACKAGE_TYPE_EXTREQ: 
            {
                if(context->m_callback_on_create)
                {
                    context->m_callback_on_req_or_res(
                        this, package_type, 
                        package.content.request.content);
                }
                break;
            };
            case virtual_session_enum::PACKAGE_TYPE_EXTRES: 
            {
                if(context->m_callback_on_create)
                {
                    context->m_callback_on_req_or_res(
                        this, package_type, 
                        package.content.response.status);
                }
                break;
            };
            case virtual_session_enum::PACKAGE_TYPE_REQUEST: 
            {
                if (package.content.request.content 
                    == (uint64_t) 
                    virtual_session_enum::REQUEST_CONTENT_DISCONNECT)
                {
                    if(context->m_callback_on_disconnect)
                    {
                        context->m_callback_on_disconnect(
                            this, false);
                    } 
                }
                else if (package.content.request.content 
                    == (uint64_t) 
                        virtual_session_enum::REQUEST_CONTENT_KEEP_ALIVE)
                {
                    if (context->m_callback_on_req_or_res)
                    {
                        context->m_callback_on_req_or_res(
                            this,
                            (virtual_session_enum)
                                package.content.request.content ,
                            0
                        );
                    }
                }
                
                break;
            };
            case virtual_session_enum::PACKAGE_TYPE_RESPONSE: 
            {
                /**
                 *  RESPONSE_STATUS_KEEP_ALIVE, // 响应：同意保持连接
                    RESPONSE_STATUS_NOT_KEEP,   // 响应：不同意保持连接
                    RESPONSE_STATUS_DISCONNECT, // 响应：虚拟连接（对端）已经断开
                 */
                if (package.content.response.status 
                    == (uint64_t) virtual_session_enum::RESPONSE_STATUS_DISCONNECT)
                {
                    if(context->m_callback_on_disconnect)
                    {
                        context->m_callback_on_disconnect(
                            this, false);
                    }
                    break;
                }
                else if (package.content.response.status 
                    == (uint64_t) virtual_session_enum::RESPONSE_STATUS_NOT_KEEP)
                {   
                    context->m_keep_alive = false;
                }
                else if (package.content.response.status 
                    == (uint64_t) virtual_session_enum::RESPONSE_STATUS_KEEP_ALIVE)
                {
                    context->m_keep_alive = true;
                }
                else if(package.content.response.status 
                    == (uint64_t)virtual_session_enum::RESPONSE_STATUS_ACCEPT)
                {
                    context->m_remote_vid = package.vid;
                    context->m_trans_start = true;
                }
                if(context->m_callback_on_req_or_res)
                {
                    context->m_callback_on_req_or_res(
                        this, package_type, 
                        0);
                }
                break;
            };
            case virtual_session_enum::PACKAGE_TYPE_TRANSFER: 
            {
                char const * _pdata = &package.content.data.first;

                if (context->m_buffer.size() < context->m_data_size)
                {
                    context->m_buffer.resize(context->m_data_size);
                }

                for (uint32_t i = 0 ; i < package.content.data.size 
                    && context->m_data_size > package.content.data.size ;
                    i++)
                {
                    context->m_buffer[context->m_data_size + i]
                        = _pdata[i];
                }
                context->m_data_size += package.content.data.size;
                
                used_bytes = package.package_size;
                break;
            };
            default:
            {
                used_bytes = 0;
                return _func(); 
            }
        }

        used_bytes = 0;
        return {};
    }

    void virtual_session::send_next_data_frame()
    {
        if ( !context->m_trans_start ||
            context->m_session_type 
                != virtual_session_enum::SESSION_TYPE_SEND)
        {
            return ;
        }
        std::string data(sizeof(virtual_package_t),0x00) ;

        virtual_package_t & package =
            *reinterpret_cast<virtual_package_t*>(&data[0]);

        package.magic_num = MAGIC_NUM;
        package.target_vid = context->m_remote_vid;
        package.vid = context->m_vid;
        package.type = (uint16_t)virtual_session_enum::PACKAGE_TYPE_TRANSFER;

        // 没发完
        if (context->m_data_size + 
            context->m_per_data_size < context->m_buffer.size())
        {
            data.insert(data.begin() + sizeof(virtual_package_t) , 
                context->m_buffer.begin() + context->m_data_size,
                context->m_buffer.begin() + context->m_data_size 
                    + context->m_per_data_size);
            context->m_data_size += context->m_per_data_size;
            package.content.data.size = context->m_per_data_size;
            package.package_size = data.size();
            context->m_next_send.push_back(std::move(data));
        }
        else  // 能够发完
        {
            size_t size = std::min(
                context->m_buffer.size() - context->m_data_size,
                (uint64_t)context->m_per_data_size);
            data.insert(data.begin() + sizeof(virtual_package_t) ,
                context->m_buffer.begin() + context->m_data_size,
                context->m_buffer.begin() + context->m_data_size + size);
            context->m_data_size += size;
            package.content.data.size = size;
            package.package_size = data.size();
            context->m_next_send.push_back(std::move(data));
            if (context->m_keep_alive == false)
            {
                send_response(virtual_session_enum::RESPONSE_STATUS_DISCONNECT, 
                    0);
                if (context->m_callback_on_disconnect)
                {
                    context->m_callback_on_disconnect(this,true);
                }
            }
        }
    }

    /// 取出数据
    std::string virtual_session::fetch_data(size_t size)
    {
        size_t _size = 0;
        std::string _data;

        if (context->m_session_type == 
            virtual_session_enum::SESSION_TYPE_SEND)
        {
            _size = context->m_buffer.size() - context->m_data_size;
            _size = std::min(size,_size);
            size_t need = context->m_buffer.size() - _size;
            _data.insert(_data.begin(),
                context->m_buffer.begin() + need, 
                context->m_buffer.begin() + _size);
        }
        else if (context->m_session_type == 
            virtual_session_enum::SESSION_TYPE_RECV)
        {
            _size = context->m_data_size;
            _data.insert(_data.begin(),
                context->m_buffer.begin(),
                context->m_buffer.begin() + _size);
        }
        return _data;
    }

    /// 缓冲区内剩余的数据大小
    size_t virtual_session::remanent_size()
    {
        if (context==nullptr)
        {
            return 0;
        }
        if (context->m_session_type == 
            virtual_session_enum::SESSION_TYPE_SEND)
        {
            return context->m_buffer.size() - context->m_data_size;
        }
        else if (context->m_session_type == 
            virtual_session_enum::SESSION_TYPE_RECV)
        {
            return context->m_data_size;
        }
        return 0;
    }

    void virtual_session::clear_buffer()
    {
        if (context)
        {
            context->m_buffer.clear();
            context->m_data_size = 0;
        }
    }

    void virtual_session::set_data_frame_max_size(uint16_t nbytes )
    {
        if(context)
        {
            context->m_per_data_size = 
                std::min((uint16_t)8192 , 
                    std::max( (uint16_t)128, nbytes));
        }
    }

}