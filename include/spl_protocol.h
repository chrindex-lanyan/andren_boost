#pragma once

#include <stdint.h>
#include <mutex>
#include <string>
#include <memory>
#include <deque>



/**
* 这是一个简单的应用层分包协议。
* 有时不想使用麻烦的HTTP，因此简单地实现了
* 这个协议，主要是用于应用层分包，此外提供了
* service 和 request两个字段，可用可不用，
* 但这两个字段不能留空，至少各自提供一个字符。
** format:
***********
        protocol_head   + next_line     + service_head + service 
    +   next_line       + request_head  + request + next_line 
    +   data_size_head  + data_size 
    +   next_line       + empty_line    + next_line + data;
************
*/

namespace chrindex::andren_boost{

extern const std::string protocol_head;
extern const std::string service_head;
extern const std::string request_head;
extern const std::string data_size_head;
extern const std::string empty_line;
extern const std::string next_line;

class inputor_t : public std::enable_shared_from_this<inputor_t>
{
public:

    // 往里丢数据
    void push_data(std::string data);

    // 取出一个数据
    std::string fetch_one();

    // 待取数据个数
    size_t bufCount();

private:
    std::mutex mut;
    std::deque<std::string> data;
};


struct spl_package_t
{
    std::string head;
    std::string service;
    std::string request;
    std::string data;
    int64_t data_size = data.size();

    /// 打包器
    static std::string package(std::string service, std::string request, std::string data);

    /// 打包器
    std::string package();
};

class spl_protocol_t : public std::enable_shared_from_this<spl_protocol_t>
{
public:
    enum class check_type_enum : int32_t
    {
        OK = 1, // 整条数据成功解析
        OK_HEAD = 2, // 头部数据成功解析
        OK_PROTOCOL = 4, // 协议头成功解析
        FAILED_HEAD = 8, // 头部数据解析失败
        FAILED_SERVICE = 16, // Service字段解析失败
        FAILED_REQUEST = 32, // Request字段解析失败
        FAILED_DATA_SIZE = 64, // 数据大小字段解析失败
        FAILED_DATA = 128,  // 数据解析失败
        FAILED_UNKNOW = 256,    // 未知错误
        FAILED_NONE = 512,  // 无意义
        FAILED_EMPTY_INPUTOR = 1024, // 错误，指示数据输入器是空的
        OK_BUT_HAS_MORE_DATA = 2048, // 本条成功解析，但后续有未解析的数据
        FAILED_EMPTY_LINE = 4096,    // 空行部分解析失败
    };

    spl_protocol_t(spl_protocol_t &&) = default;
    spl_protocol_t &operator=(spl_protocol_t &&) = default;
    spl_protocol_t();
    ~spl_protocol_t();

    // 设置输入器
    void setInputor(std::shared_ptr<inputor_t> input);

    // 解析尝试一条数据
    check_type_enum check();

    // 解析完成的数据包的数量
    int32_t readyPackageCount();

    // 取出一个已经解析出来的数据包
    struct spl_package_t fetchOnePackage();

    // 历史待解析数据量
    int64_t historyDataSize();

    // 历史待解析数据
    std::string historyData();

    // 取出所有历史待解析数据
    std::string fetchHistoryData();

private:

    // 解释器
    check_type_enum parser(std::string &data, struct spl_package_t &pack, int64_t &ret);

private:
    struct _data_t
    {
        std::mutex historyDataMut;
        std::string historyData;
        std::mutex waitReadQueMut;
        std::deque<spl_package_t> waitReadQue;
        std::shared_ptr<inputor_t> inputor;
    };

    std::unique_ptr<struct _data_t> m_data;
};

}