#pragma once

#include <cstdint>
#include <utility>
#include <string>
#include <queue>
#include <deque>
#include <tuple>
#include <functional>
#include <map>



/*

协议设计
    数据包分组、拆分和组装：

    将大于N字节的数据包拆分成一组fragment。
    每个组拥有魔术字作为协议头、
        唯一组ID（GID）、
        总数据大小（size_t size）、
        优先级(gPrio)、
        最大的fragment大小N、
        fragment数量count、
        一组fragment。

    每个fragment的数据部分大小不超过N字节。

    每个fragment包含魔术字作为协议头、
        一个唯一ID（FID，从1开始递增）、
        组ID、
        数据部分、
        数据大小字段。

    N>=16 bytes。X >=1 。GID>=1 。FID>=1 。三者都是uint64_t。

    fragment字段：
        magic_number (uint64_t)
        gid (uint64_t)    
        fid (uint64_t)  
        data_size (uint32_t) 
        data (bytes[data_size])
    
    fragment_group字段：
        magic_number (uint64_t)
        gid (uint64_t)
        fragment_count (uint64_t) // size = { x | 0 < x < $(int32_max)  }
        all_data_size (uint64_t)
        fragment_data_maxium_size (uint32_t) // 推荐设为256~4096字节
        priority (uint32_t)
        all_fragments (std::deque<fragment_t>);

请求-响应机制：
    request和response都有拥有魔术字作为协议头。
    发送方发送一个request，接收方收到request后，
    返回response表示是否接受这次传输（accept或者reject）。

    request字段：
        magic_number (uint64_t)
        gid (uint64_t) 
        fragment_count (uint64_t) 
        all_data_size (uint64_t) 
        fragment_data_maxium_size (uint32_t) 
        extention_code (uint32_t)

    response字段：
        magic_number (uint64_t)
        target_gid (uint64_t)； 
        target_fid (uint64_t)；

        /// OK = 100, 接收完成。
        ///   在全部接收完fragment后回复一次。
        ///   也可以在中途适当地回复，
        ///   这样发送端知道要不要继续发送。
        /// 
        /// ACCEPT, 同意接收该Group。
        ///
        /// REJECT, 拒绝接收该Group。
        ///
        /// RETRY, 重传目标帧。
        /// ERROR, 响应方发生错误。
        /// NONE , 双方什么也不需要做。
        ///   该字段也可以测试双方服务的可用性。
        state (int32_t)；

        extention_code (int32_t)
    
优先级和队列管理：

    使用优先队列（std::priority_queue）来管理待发送fragment队列。
    队列按照组的优先级（priority）对fragment进行排序。
    队列一次最大容纳X个fragment ，高优先级的先占用，但至少给低优先级的预留一个位置。
    priority = { p |   pNum > p > 1   } ， p越大优先级越高，pNum推荐为5。
    优先队列容量建议设置为128，最好不要小于4，不应小于2。
*/


namespace chrindex::andren_boost
{

    #define DEFAULT_MAGIC_NUM_64 0x0a1b2c3e4f5e6c7b
    #define DEFAULT_PRIORITY_CAPACITY 256

    // 数据碎片
    struct fragment_t 
    {
        struct _head_t
        {
            uint64_t magic_number = DEFAULT_MAGIC_NUM_64 ; 
            uint64_t fid = 0;           // FID
            uint64_t gid = 0;           // GID
            uint32_t data_size = 0;     // data size
            char _reverse[4];
        } head;

        struct _data_t
        {
            char first_byte;
            char _reverse[7];
        } data;

        fragment_t() = default;
        ~fragment_t() = default;

        /// 除数据字段以外，fragment头部字段所需大小
        static size_t fragment_head_size();

        /// 提供必要数据，并将mutable_buffer的数据
        /// 改为fragment结构数据。
        /// 注意，返回的fragment指针，其生命周期不应短于
        /// mutable_buffer的生命周期。
        static fragment_t const * 
            create_fragment_from_buffer(
                std::string & mutable_buffer,
                uint64_t gid,
                uint64_t fid,
                std::string const & data 
            );

        /// 获取整个fragment包的大小（head + data部分）。
        size_t fragment_package_size() const;
    };


    // 碎片组及碎片组生产者
    struct fragment_group_t
    {
        using fragment_buffer_t = std::string;

        uint64_t magic_number = 0x0a1b2c3e4f5e6c7b ; 
        uint64_t gid; // Group ID 
        uint32_t priority; // 优先级

        // 单个fragment数据大小（不含head）
        uint32_t fragment_data_maxium_size; 
        uint64_t fragment_count; // fragment总数

        // 总数据大小（全部fragment的data的大小总和）
        uint64_t all_data_size;  

        // 已打包好的fragment结构（以buffer呈现）
        std::deque<fragment_buffer_t> all_fragment_buffer;

        fragment_group_t()= default;

        ~fragment_group_t()=default;

        bool operator>(fragment_group_t const & _ano)const 
        {
            return priority > _ano.priority;
        }

        bool operator<(fragment_group_t const & _ano)const 
        {
            return priority<_ano.priority;
        }

        bool operator==(fragment_group_t const & _ano)const 
        {
            return priority == _ano.priority;
        }

        /// 初始化一个新的fragment_group
        bool init_a_new_group(
            uint64_t group_id,
            uint32_t group_priority,
            uint32_t single_fragment_data_maxium_size
        );

        /// 获取全部fragment_t的引用的数组。
        /// 被引用的fragment_t在fragment_group_t
        /// 的生命周期内有效。
        std::vector<fragment_t const *> 
            all_fragment_reference() const;

    };

    // 碎片组传输请求
    struct fragment_group_request_t
    {
        // 固定的协议头。
        uint64_t magic_number = 0x0a1b2c3e4f5e6c7b ; 
        uint64_t gid = 0;   // 组ID
        uint64_t fragment_count = 0; // 碎片数量
        uint64_t all_data_size = 0; // 数据段数据总大小

        // 单个碎片的数据段最大字节数。
        uint32_t fragment_data_maxium_size = 0; 

        // 扩展用的预留字段，供用户自由使用。
        uint32_t extention_code = 0; 

        fragment_group_request_t () = default;
        ~ fragment_group_request_t () = default;

        std::string create_request() const;
    };

    enum class fragment_group_response_state_code : int32_t
    {
        NONE,
        OK = 100, 
        ACCEPT,
        REJECT,
        RETRY,
        ERROR,
        
    };

    // 碎片组传输请求的响应
    struct fragment_group_response_t
    {
        // 固定的协议头。
        // 默认情况是： 0x0A1B2C3E4F5E6C7B , 共8字节。
        uint64_t magic_number = 0x0a1b2c3e4f5e6c7b ; 

        // 控制字段相关的组ID、回应状态或错误的组ID。
        // 必须对该字段赋值，
        // 否则请求方不知道这是哪个组的response。
        uint64_t target_gid = 0;

        // 控制字段相关的碎片ID
        // 根据情况选择是否需要对此字段赋值。
        uint64_t target_fid = 0;

        // 状态字段
        fragment_group_response_state_code
            state_code = 
                fragment_group_response_state_code::NONE;

        // 拓展码。供用户自由使用。
        int32_t extention_code = 0;

        fragment_group_response_t (){}

        ~fragment_group_response_t(){}

        std::string create_request(uint64_t your_gid) const;
    };


    // 碎片组发送器
    struct fragment_group_sender_t
    {
        fragment_group_sender_t() = default;
        ~fragment_group_sender_t () = default;

        

    private :
        std::map<uint64_t,fragment_group_t> groups;
        std::priority_queue<fragment_t *> m_readyque;
    };

    // 碎片组接收器
    struct fragment_group_receiver_t
    {
        //
    };

    
}