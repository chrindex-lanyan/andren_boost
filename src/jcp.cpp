
#include "jcp.h"

inline std::string Package::create_head(std::string name, size_t size, std::unordered_map<std::string, std::string> extention)
{
    nlohmann::json json;

    json["type"] = "head";
    json["name"] = name;
    json["size"] = size;

    std::string str = json.dump();

    str.push_back('\0');
    return str;
}

inline std::string Package::create_frame(std::string name, std::string data, size_t index)
{
    nlohmann::json json;

    json["type"] = "frame";
    json["name"] = name;
    json["index"] = index;
    json["size"] = data.size();

    std::string str = json.dump();

    str.push_back('\0');
    str += data;

    return str;
}

inline std::string Package::create_message(std::string message, std::unordered_map<std::string, std::string> extention)
{
    nlohmann::json json;

    json["type"] = "message";
    json["size"] = message.size();

    std::string str = json.dump();

    str.push_back('\0');
    str += message;

    return str;
}

inline bool Package::fromString(std::string str, size_t& usedBytes)
{
    usedBytes = 0;

    char const* cstr = str.c_str();
    std::string headStr = cstr;
    nlohmann::json j;

    try {
        j = nlohmann::json::parse(headStr);

        if (!j["type"].is_string())
        {
            return false;
        }
        type = j["type"];

        if (type == "head")
        {
            head["name"] = j["name"].get<std::string>();
            head["size"] = j["size"].get<size_t>();
            if (str[headStr.size()] != 0)
            {
                return false;
            }
            usedBytes = headStr.size() + 1;
        }
        else if (type == "frame")
        {
            head["name"] = j["name"].get<std::string>();
            head["index"] = j["index"].get<size_t>();
            size_t frameSize = j["size"].get<size_t>();
            head["size"] = frameSize;

            if (str.size() < (headStr.size() + 1 + frameSize))
            {
                return false;
            }
            data.clear();
            size_t basePos = headStr.size() + 1;
            if (str[headStr.size()] != 0)
            {
                return false;
            }
            data.insert(data.begin(), str.begin() + basePos, str.begin() + basePos + frameSize);
            usedBytes = basePos + frameSize;
        }
        else if (type == "message")
        {
            size_t msgSize = j["size"].get<size_t>();
            if (str.size() < (headStr.size() + 1 + msgSize))
            {
                return false;
            }
            data.clear();
            size_t basePos = headStr.size() + 1;
            if (str[headStr.size()] != 0)
            {
                return false;
            }
            data.insert(data.begin(), str.begin() + basePos, str.begin() + basePos + msgSize);
            usedBytes = basePos + msgSize;
        }
        else
        {
            return false;
        }
    }
    catch (...)
    {
        return false;
    }
    return true;
}
