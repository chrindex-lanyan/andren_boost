
#include "azure_protocol.hh"

#include <cstdint>
#include <cstdlib>
#include <cstring>

#include <functional>
#include <set>
#include <string_view>

namespace chrindex::andren_boost
{
    // 数据碎片

    size_t fragment_t::fragment_head_size()
    {
        return sizeof(_head_t);
    }

    bool fragment_t::operator>(fragment_t const & _) const noexcept 
    {
        return head.fid > _.head.fid;
    }

    bool fragment_t::operator==(fragment_t const &_) const noexcept
    {
        return head.fid == _.head.fid;
    }

    bool fragment_t::operator<(fragment_t const &_) const noexcept
    {
        return head.fid < _.head.fid;
    }

    bool fragment_t::operator!=(fragment_t const &_) const noexcept
    {
        return head.fid != _.head.fid;
    }

    fragment_t const * 
        fragment_t::create_fragment_from_buffer(
            std::string & mutable_buffer,
            uint64_t gid,
            uint64_t fid,
            std::string const & data 
        )
    {
        mutable_buffer.resize(fragment_head_size() + data.size());
        auto p_fragment = reinterpret_cast<fragment_t*>(&mutable_buffer[0]);

        p_fragment->head.magic_number = DEFAULT_FRAGMENT_MAGIC_NUM_64;
        p_fragment->head.gid = gid;
        p_fragment->head.fid = fid;
        p_fragment->head.data_size = data.size();
        ::memcpy(&p_fragment->data.first_byte,data.c_str(),data.size());

        return p_fragment;
    }

    /// 获取整个fragment包的大小（head + data部分）。
    size_t fragment_t::fragment_package_size() const
    {
        return fragment_head_size() + head.data_size;
    }

    // 碎片组及碎片组生产者

    fragment_group_t::fragment_group_t(fragment_group_t && _) noexcept
    {
        gid = _.gid ; 
        priority = _.priority; 
        fragment_data_maxium_size = _.fragment_data_maxium_size; 
        fragment_count = _.fragment_count; 
        all_data_size = _.all_data_size; 
        all_fragment_buffer = std::move(_.all_fragment_buffer); 
    }

    void fragment_group_t::operator=(fragment_group_t && _) noexcept
    {
        gid = _.gid ; 
        priority = _.priority; 
        fragment_data_maxium_size = _.fragment_data_maxium_size; 
        fragment_count = _.fragment_count; 
        all_data_size = _.all_data_size; 
        all_fragment_buffer = std::move(_.all_fragment_buffer); 
    }

    bool fragment_group_t::operator>(fragment_group_t const & _ano)const noexcept
    {
        return priority > _ano.priority;
    }

    bool fragment_group_t::operator<(fragment_group_t const & _ano)const noexcept
    {
        return priority<_ano.priority;
    }

    bool fragment_group_t::operator==(fragment_group_t const & _ano)const noexcept
    {
        return priority == _ano.priority;
    }

    bool fragment_group_t::operator!=(fragment_group_t const & _ano)const noexcept
    {
        return priority != _ano.priority;
    }

    /// 初始化一个新的fragment_group
    bool fragment_group_t::init_a_new_group(
        uint64_t group_id,
        uint32_t group_priority,
        uint32_t single_fragment_data_maxium_size,
        std::string const & data
    )
    {
        gid = group_id;
        priority = group_priority;
        fragment_data_maxium_size = single_fragment_data_maxium_size;

        all_data_size = data.size();
        size_t count = all_data_size / single_fragment_data_maxium_size; // 整除个数
        size_t remainder = all_data_size % single_fragment_data_maxium_size; // 余数
        if (remainder !=0)
        {
            count++;
        }
        fragment_count = count;
        size_t index = 0;

        for (size_t i = 0; i < count ; i++)
        {
            std::string _this_data ;
            std::string _package;
            if ( i==count-1 )
            {
                _this_data = std::string(&data[index],
                    remainder);
            }
            else 
            {
                _this_data = std::string(&data[index],
                    fragment_data_maxium_size);
                index += fragment_data_maxium_size;    
            }
            fragment_t::create_fragment_from_buffer(
                _package, 
                gid, 
                i+1, 
                _this_data
            );
            all_fragment_buffer.push_back(std::move(_package));
        }
        return true;
    }

    std::vector<fragment_t const *> 
        fragment_group_t::all_fragment_reference() const
    {
        std::vector<fragment_t const *> result;

        for (auto & _buf : all_fragment_buffer)
        {
            result.push_back( reinterpret_cast<fragment_t const *>(&_buf[0]));
        }
        return result;
    }

    std::optional<std::tuple<bool , std::string>>
        fragment_group_t::combine_fragments_as_data() const
    {
        if(fragment_count != all_fragment_buffer.size())
        {
            return {{false , {}}};
        }
        std::string data ;
        data.reserve(all_data_size);
        for (auto & _data : all_fragment_buffer)
        {
            data += std::move(_data);
        }
        return {{true, std::move(data)}};
    }
    
    // 碎片组传输请求
    std::string fragment_group_request_t::create_request() const
    {
        return  {
            reinterpret_cast<char const *>(this), 
            sizeof(fragment_group_request_t) 
        };
    }

    void fragment_group_request_t::init_request(fragment_group_t const & group)
    {
        // magic_number = DEFAULT_REQUEST_MAGIC_NUM_64;
        gid = group.gid;
        fragment_count = group.fragment_count;
        all_data_size = group.all_data_size; 

        // 单个碎片的数据段最大字节数。
        fragment_data_maxium_size = group.fragment_data_maxium_size; 

        // 扩展用的预留字段，供用户自由使用。
        //extention_code = 0; 
    }
    
    // 碎片组传输请求的响应

    std::string fragment_group_response_t::create_request() const
    {
        return {
            reinterpret_cast<char const *>(this),
            sizeof(fragment_group_response_t)
        };
    }
    
    // 碎片组发送器

    std::optional<fragment_group_request_t> 
        fragment_group_sender_t::append_group(fragment_group_t && group)
    {
        if (pending_groups.find(group.gid)
            != pending_groups.end())[[unlikely]]
        {
            return {};
        }

        std::optional<fragment_group_request_t> result = fragment_group_request_t{};
        fragment_group_request_t & req = result.value();

        req.magic_number = DEFAULT_REQUEST_MAGIC_NUM_64;
        req.gid = group.gid;
        req.fragment_count = group.fragment_count;
        req.all_data_size = group.all_data_size;
        req.fragment_data_maxium_size = group.fragment_data_maxium_size;
        req.extention_code = 0;

        pending_groups[group.gid] = std::move(group);
        return result;
    }

    bool fragment_group_sender_t::notify_a_group(fragment_group_response_t const& response)
    {
        auto iter = pending_groups.find(response.target_gid);
        if (iter == pending_groups.end())
        {
            return false;
        }
        if (response.state_code != 
            fragment_group_response_state_code::ACCEPT)
        {
            return false;
        }
        wait_groups[response.target_gid] = std::move(iter->second); 
        pending_groups.erase(iter);

        return true;
    }

    fragment_group_t * 
        fragment_group_sender_t::find_group_reference(uint64_t gid, 
            from_map_enum from_map)
    {
        std::map<uint64_t , fragment_group_t> *p_map = nullptr;

        if (from_map == from_map_enum::PENDING_MAP)
        {
            p_map = &pending_groups;
        }
        else if (from_map == from_map_enum::WAITING_MAP)
        {
            p_map = &wait_groups;
        }
        else 
        {
            return nullptr;
        }
        auto iter = p_map->find(gid);
        if (iter == p_map->end())
        {
            return nullptr;
        }
        return &iter->second;
    }

    bool fragment_group_sender_t::compare_fragment_larger(
        _ready_data_t const & left , _ready_data_t const & right)
    {
        if (std::get<0>(left) == std::get<0>(right))
        {
            return reinterpret_cast<fragment_t const*>(std::get<1>(left).c_str())->head.fid
                > 
                reinterpret_cast<fragment_t const*>(std::get<1>(right).c_str())->head.fid;
        }
        return std::get<0>(left) > std::get<0>(right);
    }

    /// 取出一次队列数据，
    /// 数据可能会比较大。 
    /// 数据大小 约等于 就绪队列.size() * 单个fragment大小。
    /// 返回拼接好的数据
    std::string 
        fragment_group_sender_t::fecth_some_data()
    {
        ready_queue_t ready_que = std::move(m_readyque);
        std::string data;

        while (!ready_que.empty())
        {
            auto [priority, fragment_str] = ready_que.top();
            data += std::move(fragment_str);
            ready_que.pop();
        }
        return data;
    }

    /// 从等待表分配一些fragment到就绪队列。
    /// 返回的是本次分配的数量。
    size_t fragment_group_sender_t::flush()
    {
        size_t count = 0;

        while (count < DEFAULT_PRIORITY_CAPACITY 
                && !wait_groups.empty())
        {
            std::vector<uint64_t> droplist; // 丢弃列表
            for (auto & iter : wait_groups)
            {
                fragment_group_t & group = iter.second;

                if (group.all_fragment_buffer.empty())
                {
                    droplist.push_back(group.gid);
                    continue;
                }
                m_readyque.push(
                    _ready_data_t{
                        group.priority ,
                        std::move(group.all_fragment_buffer.front())
                    });
                group.all_fragment_buffer.pop_front();
                count++;
            }
            // 去除已经发完的。
            for (auto & drop_id : droplist)
            {
                wait_groups.erase(drop_id);
            }
        }
        return count;
    }

    // 碎片组接收器

    /// 添加一些数据，
    /// 数组会在此被解开成一个或多个fragment、request、response。
    size_t fragment_group_receiver_t::append_some_data(std::string && data)
    {
        std::string str = std::move(buffer) + std::move(data);
        uint64_t count = 0;

        if (str.empty())
        {
            return 0;
        }
        std::string_view slice = str;
        std::function<std::tuple<int,
                std::string_view>(std::string_view)> real_func;
            
        while (!slice.empty())
        {
            uint64_t const* magic_number = 
                reinterpret_cast<uint64_t const*>(&slice[0]);
            
            switch (*magic_number) 
            {
                case DEFAULT_REQUEST_MAGIC_NUM_64:
                {
                    real_func = [this](std::string_view _slice) 
                        { return parse_as_request(_slice); };
                    break;
                }
                case DEFAULT_RESPONSE_MAGIC_NUM_64:
                {
                    real_func = [this](std::string_view _slice) 
                        { return parse_as_response(_slice); };
                    break;
                }
                case DEFAULT_FRAGMENT_MAGIC_NUM_64:
                {
                    real_func = [this](std::string_view _slice) 
                        { return parse_as_fragment(_slice); };
                    break;
                }
                default :
                {
                    real_func = {};
                    break;
                }
            }

            if (!real_func)
            {
                buffer = slice;
                break;
            }
            auto [ret , next_slice] = real_func(slice);
            if (ret != 0)
            {
                buffer = slice;
                break;
            }
            slice = next_slice;
            count++;
        }
        return count ;
    }

    std::tuple<int, std::string_view> 
        fragment_group_receiver_t::parse_as_request(std::string_view slice)
    {
        fragment_group_request_t const *req = 
            reinterpret_cast<fragment_group_request_t const*>(&slice[0]); 
        
        if(pending_request.end() 
            != pending_request.find(req->gid))
        {
            return {-1,{}};
        }
        if (slice.size() < sizeof(fragment_group_request_t))
        {
            return {-2,{}};
        }
        
        pending_request.insert({ req->gid, *req});
        
        if (slice.size() == sizeof(fragment_group_request_t))
        {
            return {0, {}};
        }
        return {0, {slice.begin(), slice.begin()
             + sizeof(fragment_group_request_t)}};
    }

    std::tuple<int, std::string_view> 
        fragment_group_receiver_t::parse_as_response(std::string_view slice)
    {
        fragment_group_response_t const* response = 
            reinterpret_cast<fragment_group_response_t const *>
                (&slice[0]);
        
        if (pending_response.end() !=
            pending_response.find(response->target_gid))
        {
            return {-1, {}};
        }
        if (slice.size() < sizeof (fragment_group_response_t))
        {
            return {-2, {}};
        }

        pending_response.insert({response->target_gid, 
            *response});
        
        if (slice.size() == sizeof(fragment_group_response_t))
        {
            return {0, {}};
        }
        return {0, {slice.begin(),slice.begin()
            + sizeof(fragment_group_response_t)}};
    }

    std::tuple<int, std::string_view> 
        fragment_group_receiver_t::parse_as_fragment(std::string_view slice)
    {
        fragment_t const* fragment = 
            reinterpret_cast<fragment_t const *>(&slice[0]);
        auto iter = wait_groups.find(fragment->head.gid);

        if (wait_groups.end() != iter)
        {
            return {-1, {}};
        }

        if (slice.size() < sizeof (fragment_t))
        {
            return {-2, {}};
        }
        size_t need_size = fragment_t::fragment_head_size() +
                fragment->head.data_size;

        if (slice.size() < need_size)
        {
            return {-2, {}};
        }
        fragment_group_t & group = iter->second;
        std::string data{ 
            reinterpret_cast<char const *>
                (&fragment),
                need_size
            };
        
        group.all_fragment_buffer.push_back(std::move(data));
        if (slice.size() == need_size)
        {               
            return {0, {}};
        }
        return {0, {slice.begin(),slice.begin()
            + need_size}};
    }

    bool fragment_group_receiver_t::try_collect_fragments(
        fragment_group_request_t const & request)
    {
        if (wait_groups.end() !=
            wait_groups.find(request.gid))
        {
            return false;
        }
        fragment_group_t & group = wait_groups[request.gid];

        group.gid = request.gid;
        group.priority = UINT32_MAX;
        group.all_data_size = request.all_data_size;
        group.fragment_count = request.fragment_count;
        group.fragment_data_maxium_size = 
            request.fragment_data_maxium_size;
        group.all_fragment_buffer ={};

        return true;
    }

    /// 取出一个已经完成接收的数据组。
    std::optional<fragment_group_t> 
        fragment_group_receiver_t::fecth_one_completed_group()
    {
        if (completed_groups.empty())
        {
            return {};
        }
        auto iter = completed_groups.begin();
        std::optional<fragment_group_t> result 
            = std::move(iter->second);
        completed_groups.erase(iter);
        return result;
    }

    /// 取出一个接收到的数据组发送请求。
    std::optional<fragment_group_request_t> 
        fragment_group_receiver_t::fecth_one_group_request()
    {
        if (pending_request.empty())
        {
            return {};
        }
        auto iter = pending_request.begin();
        std::optional<fragment_group_request_t> result 
            = std::move(iter->second);
        pending_request.erase(iter);
        return result;
    }

    /// 取出一个已经接收到的数据组接收响应。
    std::optional<fragment_group_response_t>
        fragment_group_receiver_t::fecth_one_group_response()
    {
        if (pending_response.empty())
        {
            return {};
        }
        auto iter = pending_response.begin();
        std::optional<fragment_group_response_t> 
            result = std::move(iter->second);
        pending_response.erase(iter);
        return result;
    }

    /// 从等待表或完成表查找一个数据组，返回组的引用。
    fragment_group_t * fragment_group_receiver_t::
        find_group_reference(
            uint64_t gid,  from_map_enum from_map)
    {
        std::map<uint64_t, fragment_group_t> * p_map = nullptr;

        if (from_map == from_map_enum::COMPLETED_MAP)
        {
            p_map = &completed_groups;
        }
        else if (from_map == from_map_enum::WAITING_MAP)
        {
            p_map = &wait_groups;
        }
        else 
        {
            return nullptr;
        }
        auto iter = p_map->find(gid);
        if (iter == p_map->end())
        {
            return nullptr;
        }
        return &iter->second;
    }
           

    /// 等待表的当前大小
    size_t fragment_group_receiver_t::wait_count() const
    {
        return wait_groups.size();
    }

    /// 完成表的当前大小
    size_t fragment_group_receiver_t::completed_count() const
    {
        return completed_groups.size();
    }

    /// 请求表的当前大小
    size_t fragment_group_receiver_t::request_count() const 
    {
        return pending_request.size();
    }

    /// 响应表的当前大小
    size_t fragment_group_receiver_t::response_count() const 
    {
        return pending_response.size();
    }

    /// 临时缓冲区的数据大小
    size_t fragment_group_receiver_t::tmp_buffer_size() const
    {
        return buffer.size();
    }

    /// 清除临时缓冲区的数据
    void fragment_group_receiver_t::clear_tmp_buffer()
    {
        buffer.clear();
        return ;
    }
    


}
