#include "uid_creator.hh"

#include <atomic>

namespace chrindex::andren_boost
{
    uint64_t create_uid_u64()
    {
        static std::atomic<uint64_t> uid {0};
        return uid.fetch_add(1);
    }
}
