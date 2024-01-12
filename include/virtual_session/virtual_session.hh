#pragma  once

/**
 * @file virtual_session.hh
 * @author chrindex 
 * @brief 
 * @version 0.1
 * @date 2024-01-12
 * 
 * @copyright Copyright (c) 2024
 * 
 * 对于一个通信协议，需要考虑的事情很多。
 *    1. 分包。通常可以选256，512，1024，2048，4096的大小.
 *    2. 分流。
 *          1. 一个连接可以维护多个虚拟连接。
 *          2. 每个虚拟连接有着状态：{ 连接中，已连接，已断开 }。
 *          3. 物理连接断开后，虚拟连接也要跟着断开。
 *          4. 每个虚拟连接有着唯一标识，比如ID。
 *          5. 每个虚拟连接不必分帧，只需要每次记录已发送/接收的数据大小即可。
 *          6. 虚拟连接传送数据完成后直接断开（除非keep-alive），无需回应。
 *          7. 虚拟连接发起数据传送需要被接收端确认才能开始发送。
 *          8. 预留拓展字段。
 */


#include <cstdint>
#include <functional>
#include <string>
#include <deque>
#include <stdint.h>
#include <memory>

namespace chrindex::andren_boost
{
    #define MAGIC_NUM 0xffeeaadd11559900
    #define DEFAULT_DATA_FRAME_SIZE 512

    struct virtual_package_t
    {
        uint64_t magic_num = MAGIC_NUM;   // 魔术字
        int16_t  package_size;            // 包大小
        uint16_t type      ; // 包类型 { 建立会话, 请求, 响应, 传送, 拓展请求, 拓展响应  }
        uint32_t vid : 24 ;  // 虚拟连接ID
        uint32_t target_vid : 24; // 目标虚拟连接ID
        union 
        {
            struct request_t
            {
                /*  type         content
                 * ----------------------
                 *  建立会话      数据传送大小  
                 *  请求         {保持会话，断开连接}     
                 *  拓展请求      用户自定义
                 */
                uint64_t content; 
            } request;

            struct response_t
            {
                /**
                 * type         status
                 * ---------------------
                 * 响应      {已保持，不同意保持，已断开} 
                 * 拓展响应   用户自定义
                 */
                uint64_t status;
            } response;

            struct transfer_data_request
            {
                /**
                 * type     size    first
                 * ---------------------------
                 * 传送     数据大小  数据首字节
                 */
                uint16_t size;
                char first;
            } data;
        } content;
    };

    enum class virtual_session_enum : int 
    {
        NONE = 0,               // (对接口)指示使用拓展字段{拓展请求、拓展响应}

        SESSION_TYPE_SEND,      // 虚拟会话是数据接收模式
        SESSION_TYPE_RECV,      // 虚拟会话是数据发送模式

        PACKAGE_TYPE_CREATE,    // 包类型：创建虚拟会话
        PACKAGE_TYPE_REQUEST ,  // 包类型：发送请求
        PACKAGE_TYPE_RESPONSE , // 包类型：发送回应
        PACKAGE_TYPE_TRANSFER,  // 包类型：数据传送
        PACKAGE_TYPE_EXTREQ,    // 包类型：拓展请求
        PACKAGE_TYPE_EXTRES,    // 包类型：拓展响应

        REQUEST_CONTENT_KEEP_ALIVE, // 请求内容：保持连接不中断
        REQUEST_CONTENT_DISCONNECT, // 请求内容：断开连接

        RESPONSE_STATUS_KEEP_ALIVE, // 响应：同意保持连接
        RESPONSE_STATUS_NOT_KEEP,   // 响应：不同意保持连接
        RESPONSE_STATUS_DISCONNECT, // 响应：虚拟连接（对端）已经断开
        RESPONSE_STATUS_ACCEPT,     // 响应：接收虚拟会话创建请求

        STATUS_CLOSED,  // 会话状态：已关闭
        STATUS_ONLINE,  // 会话状态：在线
        STATUS_OPEN,    // 会话状态：已打开但不在线
    };

    class virtual_session : public std::enable_shared_from_this<virtual_session>
    {
    public :

        virtual_session();
        virtual_session(virtual_session_enum type);
        virtual_session(virtual_session && _ano) noexcept;
        void operator=(virtual_session && _ano) noexcept;
        virtual ~virtual_session();

        void operator=(virtual_session const & )= delete;
        virtual_session(virtual_session const &) = delete;

        uint32_t vid() const noexcept;

        virtual_session_enum type() const noexcept;

        /////////// request /////////////

        /// 建立虚拟会话
        bool request_create(std::string data);

        /// 发送请求
        bool send_request(virtual_session_enum type , uint64_t or_use_extention);

        /// 发送响应
        bool send_response(virtual_session_enum type , uint64_t or_use_extention);


        using OnCreate = std::function<bool(virtual_session * session ,
             uint64_t size)>;

        using OnReqOrRes = std::function<void(virtual_session * session ,
             virtual_session_enum type, uint64_t or_extention)>;

        using OnDisconnectd = std::function<void(virtual_session * session, 
            bool forwardly)>;

        /// 如果收到了会话建立
        void setOnCreateReq(OnCreate callback);
        
        /// 如果收到了请求或者响应
        void setOnReqOrRes(OnReqOrRes callback);

        /// 如果连接断开(主动或被动)
        void setOnDisconnected(OnDisconnectd callback);

        /// 往里吐数据,解析,并尝试取出一个准备发送的数据
        std::string working(std::string const & data , uint64_t & used_bytes);

        /// 取出数据
        /// 该操作不会删除缓冲区的数据
        std::string fetch_data(size_t size);

        /// 缓冲区内剩余的数据大小
        size_t remanent_size();

        /// 清空数据缓冲区,将已发送数据字段清零。
        /// 该操作不会清空就绪队列。
        /// 在你传送数据完成后仍然keep-alive且
        /// 希望释放内存时使用。
        void clear_buffer();

        /// 设置数据帧最大大小
        /// 函数只对数据发送类型的会话有效
        void set_data_frame_max_size(uint16_t nbytes = DEFAULT_DATA_FRAME_SIZE);

    private :

        void send_next_data_frame();

    private :

        struct _context_t
        {
            uint32_t m_vid ;                     // 会话ID
            uint32_t m_remote_vid;               // 对端会话ID
            virtual_session_enum m_session_type; // 会话类型
            virtual_session_enum m_status;       // 会话状态
            size_t m_data_size;                  // 已传送的数据大小
            uint16_t m_per_data_size;            // 每个数据帧的数据段最大大小 
            bool m_keep_alive;                   // 是否保持连接(当数据传送完成后)
            bool m_trans_start ;                 // 是否开始了数据传送
            std::string m_buffer;                // 数据缓冲区
            std::deque<std::string> m_next_send; // 下一个准备发送的数据
            OnCreate m_callback_on_create; // 当收到建立连接的请求
            OnReqOrRes m_callback_on_req_or_res; // 当收到其他请求或响应
            OnDisconnectd m_callback_on_disconnect; // 当虚拟连接已断开
        } ;
        std::unique_ptr<_context_t> context;
    };

}