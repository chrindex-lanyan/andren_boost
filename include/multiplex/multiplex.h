﻿#pragma once 



#include <array>
#include <deque>
#include <functional>
#include <memory>
#include <string>
#include <stdint.h>
#include <map>
#include <tuple>
#include <vector>
#include <utility>


/**
    队列数量：8
   队列号     包大小范围     优先级      包并发数
---------------------------------------------
     0       1 ~  64        1           32
     1       65 ~ 128       2           16
     2       129 ~ 256      3           8
     3       257 ~ 512      4           4
     4      513 ~ 1024      5           4
     5      1025 ~ 2048     6           2
     6      2049 ~ 4096     7           2
     7      4097 ~ 8192     8           1
---------------------------------------------

传送通知 magic_num = 0x23aa34bb598ff670
{
    uint64_t  magic_num
    uint64_t  id
    uint64_t  all_size
    uint8_t   que_no
}  25 bytes and only que_0

数据包  magic_num  = 0x12ff34aa567cc890
{
    uint64_t  magic_num
    uint64_t  id
    uint8_t   que_no
    uint16_t  size
    uint8_t   first_byte
}  19 bytes + data's bytes

 */


#define MULTIPLEX_META_MAGIC_NUM 0x23aa34bb598ff670
#define MULTIPLEX_PAKCAGE_MAGIC_NUM 0x12ff34aa567cc890

struct multiplex_meta_t // 32 bytes
{
    uint64_t  magic_num = MULTIPLEX_META_MAGIC_NUM;
    uint64_t  id;
    uint64_t  all_size;
    uint8_t   que_no;
    uint32_t  extention: 24;
};

struct multiplex_package_meta_t
{
    uint64_t  magic_num = MULTIPLEX_PAKCAGE_MAGIC_NUM;
    uint64_t  id;
    uint32_t  extention;
    uint16_t  size;
    uint8_t   que_no;
};

struct multiplex_package_t // 24 bytes
{
    multiplex_package_meta_t meta;
    uint8_t   first_byte;
};

class multiplex_manager
{
public :

    enum class Type
    {
        SEND,
        RECV,
    };

    enum class QueueNum :uint8_t
    {
        QUE_0 = 0,
        QUE_1,
        QUE_2,
        QUE_3,
        QUE_4,
        QUE_5,
        QUE_6,
        QUE_7,
    };

    void operator=(multiplex_manager const&) = delete ;
    multiplex_manager(multiplex_manager const &)=delete;

    multiplex_manager() ;
    multiplex_manager(multiplex_manager &&) noexcept;
    ~multiplex_manager() = default;
    void operator=(multiplex_manager &&)noexcept;

    /// 添加一个数据传送任务
    void add_data_task_to_send(uint64_t id, QueueNum queue_no , std::string const & data);

    /// 取出已接收的数据
    /// 该函数调用后，已接受的数据会被清理。
    std::tuple<bool,std::string> take_out_recv_in_advance(uint64_t id);

    void parse_raw_data(std::string const & raw_data);

    std::vector<std::string> fetch_some_data_to_send();

    using OnRecvPackage = std::function<void(multiplex_package_meta_t package)> ;
    using OnNewRequest = std::function<void(multiplex_meta_t meta)> ;

    void setOnRecvPackage(OnRecvPackage cb);

    void setOnNewRequest(OnNewRequest cb);

private :

    struct _Private
    {
        std::array<std::deque<std::string>, 8> ready_send;
        std::map<uint64_t, std::deque<std::string>> ready_recv;

        OnRecvPackage onRecvPackage;
        OnNewRequest onNewRequest;
    };
    std::unique_ptr<_Private> m_private;

};




