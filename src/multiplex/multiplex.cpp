#include "multiplex.h"
#include <memory>


static int QueSizeMap[8] = 
{
    64,
    128,
    256,
    512,
    1024,
    2048,
    4096,
    8192,
};

static int QueConcurrentMap[8] =
{
    16,
    8,
    4,
    2,
    2,
    1,
    1,
    1,
};


multiplex_manager::multiplex_manager()
{
    m_private = std::make_unique<_Private>();
}

multiplex_manager::multiplex_manager(multiplex_manager && _ano) noexcept
{
    m_private = std::move(_ano.m_private);
}

void multiplex_manager::operator=(multiplex_manager && _ano)noexcept
{
    m_private = std::move(_ano.m_private);
}

/// 添加一个数据传送任务
void multiplex_manager::add_data_task_to_send(uint64_t id, 
    QueueNum queue_no , std::string const & data)
{
    /// 创建请求，保存到队列0
    std::deque<std::string> dataList;
    std::string meta_data (sizeof(multiplex_meta_t), 0x00);
    multiplex_meta_t & meta = *reinterpret_cast<multiplex_meta_t*>(&meta_data[0]);

    meta.id = id;
    meta.extention = 0;
    meta.que_no = (uint8_t)queue_no;
    meta.all_size = data.size();
    meta.magic_num = MULTIPLEX_META_MAGIC_NUM;
    dataList.push_back(std::move(meta_data));

    /// 将数据切割，保存到目标队列
    uint16_t que_size = QueSizeMap[(uint8_t)queue_no];
    uint64_t times = data.size() / que_size;
    
    if (times == 0) // 够用
    {
        std::string package_data(
            sizeof(multiplex_package_meta_t) + data.size(), 0x00);
        multiplex_package_meta_t & package_meta =
            *reinterpret_cast<multiplex_package_meta_t*>(&package_data[0]);
        
        package_meta.id = id;
        package_meta.magic_num = MULTIPLEX_PAKCAGE_MAGIC_NUM;
        package_meta.que_no = (uint8_t)queue_no;
        package_meta.size = (uint16_t)data.size();
        package_meta.extention = 0;
        for (uint64_t j= 0, i = sizeof(multiplex_package_meta_t);
            i < package_data.size() ;i++,j++)
        {
            package_data[i] = data[j];
        }
        dataList.push_back(std::move(package_data));
    }
    else {
        ///////// 接下来要多次完成
        uint64_t data_position = 0;
        for (uint64_t i = 0; i < times; i++)
        {
            std::string package_data(
                sizeof(multiplex_package_meta_t) + que_size, 0x00);
            multiplex_package_meta_t& package_meta =
                *reinterpret_cast<multiplex_package_meta_t*>(&package_data[0]);

            package_meta.id = id;
            package_meta.magic_num = MULTIPLEX_PAKCAGE_MAGIC_NUM;
            package_meta.que_no = (uint8_t)queue_no;
            package_meta.size = que_size;
            package_meta.extention = 0;

            for (uint64_t j = data_position,
                k = sizeof(multiplex_package_meta_t);
                k < package_data.size(); j++, k++)
            {
                package_data[k] = data[j];
            }
            data_position += que_size;
            dataList.push_back(std::move(package_data));
        }

        if (uint64_t free_size = data.size() % que_size;
            free_size > 0) // 有余数
        {
            std::string package_data(
                sizeof(multiplex_package_meta_t) + free_size, 0x00);
            multiplex_package_meta_t& package_meta =
                *reinterpret_cast<multiplex_package_meta_t*>(&package_data[0]);

            package_meta.id = id;
            package_meta.magic_num = MULTIPLEX_PAKCAGE_MAGIC_NUM;
            package_meta.que_no = (uint8_t)queue_no;
            package_meta.size = (uint16_t)free_size;
            package_meta.extention = 0;

            for (uint64_t j = data_position,
                k = sizeof(multiplex_package_meta_t);
                k < package_data.size(); j++, k++)
            {
                package_data[k] = data[j];
            }
            data_position += free_size;
            dataList.push_back(std::move(package_data));
        }
    }
    m_private->ready_send[(uint8_t)queue_no][id] = std::move(dataList);
}

/// 提前取出已接收的数据
/// 该函数调用后，已接受的数据会被清理。
std::tuple<bool,std::string> multiplex_manager::take_out_recv_in_advance(uint64_t id)
{
    auto iter = m_private->ready_recv.find(id);
    if (iter == m_private->ready_recv.end())
    {
        return {false, {}};
    }
    std::string data;
    auto que = std::move(iter->second);

    for (auto & per_data : que)
    {
        data += per_data;
    }
    return {true, std::move(data)};
}

void multiplex_manager::parse_raw_data(std::string const & raw_data)
{
    if (raw_data.size() == sizeof(multiplex_meta_t))
    {
        auto p_meta =  
            reinterpret_cast<multiplex_meta_t const *>(raw_data.c_str());
        if (p_meta->magic_num == MULTIPLEX_META_MAGIC_NUM)
        {
            if (m_private->onNewRequest)
            {
                m_private->onNewRequest(*p_meta);
            }
        }
    }
    else if (raw_data.size() >= sizeof(multiplex_package_meta_t))
    {
        auto p_package =
            reinterpret_cast<multiplex_package_t const*>(raw_data.c_str());
        if (p_package->meta.magic_num == MULTIPLEX_PAKCAGE_MAGIC_NUM)
        {
            m_private->ready_recv[p_package->meta.id].push_back(
                std::string(
                    (char const*)&p_package->first_byte,
                    p_package->meta.size)
            );
            if (m_private->onRecvPackage)
            {
                m_private->onRecvPackage(p_package->meta);
            }
        }
    }
}

std::vector<std::string> multiplex_manager::fetch_some_data_to_send()
{
    std::vector<std::string> data_list;

    for (int que_index= 0; 
        que_index < m_private->ready_send.size() ; 
        que_index++) // array
    {
        std::vector<uint64_t> drop_list;
        int count = 0;
        int need = QueConcurrentMap[que_index];
        auto & _map = m_private->ready_send[que_index];

        for (auto & iter : _map) // map
        {
            if (!iter.second.empty())
            {
                data_list.push_back(std::move(iter.second.front()));
                iter.second.pop_front();
                count ++;
            }
            else 
            {
                drop_list.push_back(iter.first);
            }
            if(count >= need)
            {
                break;
            }
        }
        for (auto drop_id : drop_list)
        {
            _map.erase(drop_id);
        }
    }
    return data_list;
}


void multiplex_manager::setOnRecvPackage(OnRecvPackage cb)
{
    m_private->onRecvPackage = std::move(cb);
}

void multiplex_manager::setOnNewRequest(OnNewRequest cb)
{
    m_private->onNewRequest = std::move(cb);
}
