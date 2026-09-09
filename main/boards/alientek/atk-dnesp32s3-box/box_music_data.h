#pragma once
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace box_music {
inline uint32_t Read32(const uint8_t* data) {
    return uint32_t(data[0]) | uint32_t(data[1]) << 8 | uint32_t(data[2]) << 16 |
           uint32_t(data[3]) << 24;
}
class Track {
public:
    bool Load(const void* data, size_t size) {
        data_ = nullptr;
        if (!data || size < 28)
            return false;
        auto p = static_cast<const uint8_t*>(data);
        if (memcmp(p, "BXM1", 4) || Read32(p + 4) != 24000 || Read32(p + 8) != 60)
            return false;
        auto count = Read32(p + 12);
        if (!count || count > 60000 || Read32(p + 16) != count * 60 || 20 + (count + 1) * 4 > size)
            return false;
        const size_t start = 20 + (count + 1) * 4;
        if (Read32(p + 20) != 0 || Read32(p + 20 + count * 4) != size - start)
            return false;
        for (uint32_t i = 0; i < count; ++i) {
            auto a = Read32(p + 20 + i * 4), b = Read32(p + 24 + i * 4);
            if (b <= a || b - a > 2048 || b > size - start)
                return false;
        }
        data_ = p;
        start_ = start;
        count_ = count;
        return true;
    }
    uint32_t Count() const { return data_ ? count_ : 0; }
    const uint8_t* Packet(uint32_t index, size_t& size) const {
        size = 0;
        if (!data_ || index >= count_)
            return nullptr;
        auto offset = Read32(data_ + 20 + index * 4);
        size = Read32(data_ + 24 + index * 4) - offset;
        return data_ + start_ + offset;
    }

private:
    const uint8_t* data_ = nullptr;
    size_t start_ = 0;
    uint32_t count_ = 0;
};
}  // namespace box_music
