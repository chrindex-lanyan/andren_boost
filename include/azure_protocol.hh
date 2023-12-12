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

请求-响应机制：
    request和response都有拥有魔术字作为协议头。
    发送方发送一个request，指定
        fragment的数量、
        总数据大小、
        组ID、
        请求的优先级、
        最大fragment大小N、
        其他（拓展字段）。

    接收方收到request后，返回response表示是否接受这次传输（accept或者reject）。
    response字段：
        state：OK/NO （string）；
        transfer：accept / reject / retry （string）；
        target-gid（uint64_t）； 
        target-fid （uint64_t）；
        errors （string）; 
        errorcode(int)； 
        其他（拓展字段）；

优先级和队列管理：

    使用优先队列（std::priority_queue）来管理待发送fragment队列。
    队列按照组的优先级（gPrio）对fragment进行排序。
    队列一次最大容纳X个fragment ，高优先级的先占用，但至少给低优先级的预留一个位置。
    gPrio = { p |   pNum > p > 1   } ， p越大优先级越高，pNum推荐为5。
*/


namespace chrindex::andren_boost
{
    // 数据碎片
    struct fragment_t 
    {
        // 固定的协议头。
        // 默认情况是： 0x0A1B2C3E4F5E6C7B , 共8字节。
        uint64_t magic_number = 0x0a1b2c3e4f5e6c7b ; 

        uint64_t fid;    // FID
        uint64_t gid;    // GID

        // { uint32_t size; char const *  }
        std::string data; // Data and Data's size

        fragment_t():fid(0), gid(0){}

        fragment_t(uint64_t uid, uint64_t gid, size_t size, const std::string& d)
            : fid(uid), gid(gid), data(d) {}

        ~fragment_t(){}

        size_t data_size() const;

        std::string create_fragment() const;
    };

    // 数据碎片的优先队列比较器
    struct fragment_comparable_t {
        bool operator()(const fragment_t& f1, const fragment_t& f2) const {
            return f1.gid == f2.gid ? f1.fid > f2.fid : f1.gid > f2.gid;
        }
    };

    // 碎片组及碎片组生产者
    struct fragment_group_t
    {
        // 固定的协议头。
        // 默认情况是： 0x0A1B2C3E4F5E6C7B , 共8字节。
        uint64_t magic_number = 0x0a1b2c3e4f5e6c7b ; 

        uint64_t gid; // Group ID . ID >= 1
        uint32_t priority; // 优先级
        uint32_t fragment_data_maxium_size; // size = { x | 0 < x < $(int32_max)  }
        uint64_t fragment_count;
        uint64_t all_data_size;
        std::deque<fragment_t> all_fragments;

        fragment_group_t()
            : gid(0) ,fragment_data_maxium_size(0), 
                fragment_count(0), all_data_size(0) {}
                
        
        fragment_group_t(uint64_t _gid , uint64_t _fragment_maxium_size)
            : gid(_gid) ,fragment_data_maxium_size(_fragment_maxium_size),
                fragment_count(0),all_data_size(0) {}

        ~fragment_group_t()=default;

        fragment_group_t & set_gid(uint64_t group_id);

        fragment_group_t & set_fragment_maxium_size(uint64_t size);

        bool push_data_and_finish(std::string const & data);

        std::tuple<bool , uint64_t, fragment_t const *> 
        fetch_one_fragment();

    };

    // 碎片组传输请求
    struct fragment_group_request_t
    {
        // 固定的协议头。
        // 默认情况是： 0x0A1B2C3E4F5E6C7B , 共8字节。
        uint64_t magic_number = 0x0a1b2c3e4f5e6c7b ; 
        uint64_t gid = 0;   // 组ID
        uint64_t fragment_count = 0; // 碎片数量
        uint64_t all_data_size = 0; // 数据段数据总大小

        // 单个碎片的数据段最大字节数。
        // 请注意, size = { x | 0 < x < $(uint32_max) }，
        // 但是请尽量控制在 16 ~ 4096 之间。
        uint32_t fragment_data_maxium_size = 0; 
        uint32_t extention_code = 0; // 扩展用的预留字段，供用户自由使用。

        // { uint16_t size; char const *  }
        // 扩展用的预留字段，供用户自由使用。 
        // 字段长度s = { x | 0 <= x < $(uint16_max)  }
        // 请尽量不要超过256字节。
        std::string extention_data;

        fragment_group_request_t () = default;
        ~ fragment_group_request_t () = default;

        std::string create_request() const;
    };

    // 碎片组传输请求的响应
    struct fragment_group_response_t
    {
        // 固定的协议头。
        // 默认情况是： 0x0A1B2C3E4F5E6C7B , 共8字节。
        uint64_t magic_number = 0x0a1b2c3e4f5e6c7b ; 

        // 控制字段相关的组ID、回应状态或错误的组ID。
        // target-gid（uint64_t）； 
        uint64_t target_gid;

        // 控制字段相关的碎片ID
        // target-fid （uint64_t）；
        uint64_t target_fid;

        // 错误码。
        // errorcode(int)； 0->(OK) | otherwise->(error)
        int32_t errorcode = 0;

        // 拓展码。供用户自由使用。
        int32_t extention_code = 0;

        // 状态字段
        // state：OK/NO （string）或自定义；
        // { uint16_t size; char const *  }
        // 尽量不要超过256字节。
        std::string state;

        // 传输控制字段
        // transfer：accept / reject / retry （string）或自定义；
        // { uint16_t size; char const *  }
        // 尽量不要超过256字节。
        std::string transfer;

        // 错误提示信息。
        // errorstr （string）; 
        // { uint16_t size; char const *  }
        // 尽量不要超过256字节。
        std::string errorstr;

        fragment_group_response_t (){}

        ~fragment_group_response_t(){}

        std::string create_request(uint64_t your_gid) const;
    };


    // 碎片组发送器
    struct fragment_group_sender_t
    {
        using OnResponse = 
            std::function<void(fragment_group_response_t const &)>;

        using OnSendDone =
            std::function<void(uint64_t gid)>;

        using OnErrors = 
            std::function<void(uint64_t gid ,int32_t sender_errcode)>;

        
        // 按优先级对碎片组进行排序，并通过组ID区分不同组。
        // < priority , < gid, fragment_group > >
        std::map<uint32_t , std::map<uint64_t,fragment_group_t>> groups;

        // 下一批要发送的数据。
        // <priority , std::string (fragment) >
        std::map<uint32_t , std::string> ready_data;

    };

    // 碎片组接收器
    struct fragment_group_receiver_t
    {
        using OnRequest = 
            std::function<void(fragment_group_request_t const &)>;


    };

    
}