#pragma once


#include <string>
#include <unordered_map>
#include <variant>
#include <stdint.h>

#include "nlohmann/json.hpp"

/**
 * JCP , Json TCP.
 * 有两种传输模式，消息模式、分块传输模式。
 * 消息模式适用于，单次传送数据量小于1024字节。
 * 分块传输模式适用于，单次传送数据超过1024字节。
 * 
 * 传输的数据支持文本格式和二进制格式。
 * 协议包格式：json头 + '\0' + 数据段.
 * 其中数据段的数据大小在 json头中指明。
 * 分块传输时，一般建议使用64字节、128字节、256字节、
 * 512字节、1024字节、2048字节的块大小进行传输。
 * 如果网络环境较好，也可使用4096字节和8192字节的块大小进行传输。
 * 块大小指数据端部分的大小，头部字段的大小不含在内。
 * 头部字段大小通常在32字节左右。
 * 
 * 注意，解析失败后可以留到下一次解析，但下一次
 * 解析会重头开始，因此不建议把块大小设置的太大。
 * 
 * 以下是设置建议：
 *    局域网：64字节（低延迟），2048字节（快速）
 *    公网：64字节（抗干扰+加速解析），512字节（用于稳定网络）
 *    本机：1024字节（通常），4096字节（快速）
 *    Server端：64字节（节约内存），512字节（下载等服务）
 *    Client端：32字节（低延时），128字节（稳定），512字节（快速）
 *
 * 一、对于Head包，有：
 *      {
 *          "type":"head",
 *          "name":[str], 
 *          "size":[int64_t]
 *      }
 * 
 * Head包允许拓展字段，前提是字段别和已有的重复。
 * Head包不允许"size"小于等于0。
 * 
 * 二、对于Frame包，有：
 *      { 
 *          "type":"frame", 
 *          "name":[str], 
 *          "size":[int64_t], 
 *          "index":[int64_t]
 *      }
 *      '\0'
 *      [data-content]
 * 
 * Frame包不允许"size"小于等于0。
 * 
 * 三、对于Message包，有：
 *     {
 *          "type":"message",
 *          "size":[int64_t]
 *     }
 *     '\0'
 *     [data-content]
 * 
 * Message包允许拓展字段，前提是别和已有字段重复了。
 * Message包允许"size"为0时data-content为空。 
*/

struct Package
{
    std::string type; /// { "frame", "head", "message"  }
    std::unordered_map<std::string, std::variant<
        std::string, size_t>> head; /// for head,frame,message, json formation.
    std::string data; /// for { "frame" , "message" }
public:

    static std::string create_head(std::string name, size_t size, std::unordered_map<std::string, std::string> extention);

    static std::string create_frame(std::string name, std::string data, size_t index);

    static std::string create_message(std::string message, std::unordered_map<std::string, std::string> extention);

    bool fromString(std::string str, size_t& usedBytes);
};

