#pragma once
#include <array>
#include <string>
namespace box_ota {
inline bool ParseVersion(const std::string& text, std::array<unsigned, 3>& values) {
    if (text.empty() || text.size() > 20)
        return false;
    unsigned index = 0;
    bool digit = false;
    values = {};
    for (char c : text) {
        if (c == '.') {
            if (!digit || ++index >= values.size())
                return false;
            digit = false;
        } else {
            if (c < '0' || c > '9')
                return false;
            values[index] = values[index] * 10 + c - '0';
            if (values[index] > 65535)
                return false;
            digit = true;
        }
    }
    return digit && index == 2;
}
inline bool IsNewer(const std::string& current, const std::string& next) {
    std::array<unsigned, 3> a{}, b{};
    return ParseVersion(current, a) && ParseVersion(next, b) && b > a;
}
inline bool ValidHash(const std::string& hash) {
    if (hash.size() != 64)
        return false;
    for (char c : hash)
        if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')))
            return false;
    return true;
}
inline bool ValidUrl(const std::string& url, const std::string& version) {
    return url == "https://ota.ainotex.com/box/firmware/" + version + ".bin";
}
}  // namespace box_ota
