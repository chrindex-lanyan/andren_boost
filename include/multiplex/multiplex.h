#pragma once 



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
     0       1 ~  64        1           16
     1       1 ~ 128        2           8
     2       1 ~ 256        3           4
     3       1 ~ 512        4           2
     4       1 ~ 1024       5           2
     5       1 ~ 2048       6           1
     6       1 ~ 4096       7           1
     7       1 ~ 8192       8           1
---------------------------------------------

传送通知 magic_num = 0x23aa34bb598ff670
{
    uint64_t  magic_num
    uint64_t  id
    uint64_t  all_size
    uint8_t   que_no
    uint32_t  extention: 24
}  32 bytes and only que_0

数据包  magic_num  = 0x12ff34aa567cc890
{
    uint64_t  magic_num
    uint64_t  id
    uint32_t  extention
    uint16_t  size
    uint8_t   que_no
    uint8_t   first_byte
}  23 bytes + data's bytes

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
    /// 该函数调用后，已接收的数据会被清理。
    std::tuple<bool,std::string> take_out_recv_in_advance(uint64_t id);

    /// 解析一个原始数据。
    /// 此解析器只能解析一个原始数据，请确保下层协议
    /// 能够正确分包。
    void parse_raw_data(std::string const & raw_data);

    /// 取几个打包好的数据以发送。
    /// 不要对数据进行拼接，应该每个单独发送。
    std::vector<std::string> fetch_some_data_to_send();

    using OnRecvPackage = std::function<void(multiplex_package_meta_t package)> ;
    using OnNewRequest = std::function<void(multiplex_meta_t meta)> ;

    /// 设置数据包接收回调。
    /// 每个数据包的接收都会触发此回调，
    /// 需要自行判断数据是否接收完成。
    void setOnRecvPackage(OnRecvPackage cb);

    /// 设置传送请求回调。
    /// 发送端传送数据任务前，会发送一个请求，
    /// 请求内包含了许多信息，用户需要注意总大小信息。
    void setOnNewRequest(OnNewRequest cb);

private :

    struct _Private
    {
        std::array<std::map<uint64_t,std::deque<std::string>>, 8> ready_send;
        std::map<uint64_t, std::deque<std::string>> ready_recv;

        OnRecvPackage onRecvPackage;
        OnNewRequest onNewRequest;
    };
    std::unique_ptr<_Private> m_private;

};





