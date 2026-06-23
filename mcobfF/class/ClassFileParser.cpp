#include "ClassFileParser.h"
#include <string>
#include <vector>

namespace mcobfF
{
    bool ClassFileParser::parse(std::span<const uint8_t> data, ClassInfo& outInfo)
    {
        if (data.size() < 10) return false;
        size_t offset = 0;

        auto read_u2 = [&]() -> uint16_t
        {
            if (offset + 2 > data.size()) return 0;
            uint16_t val = (static_cast<uint16_t>(data[offset]) << 8) | data[offset + 1];
            offset += 2;
            return val;
        };
        auto read_u4 = [&]() -> uint32_t
        {
            if (offset + 4 > data.size()) return 0;
            uint32_t val = (static_cast<uint32_t>(data[offset]) << 24) |
                (static_cast<uint32_t>(data[offset + 1]) << 16) |
                (static_cast<uint32_t>(data[offset + 2]) << 8) | data[offset + 3];
            offset += 4;
            return val;
        };

        //Magic Number
        if (read_u4() != 0xCAFEBABE) return false;
        read_u2();
        read_u2(); //version

        //Constant Pool
        uint16_t cp_count = read_u2();
        std::vector<std::string> utf8_pool(cp_count);
        std::vector<uint16_t> class_idx(cp_count, 0);

        for (uint16_t i = 1; i < cp_count; ++i)
        {
            if (offset >= data.size()) return false;
            uint8_t tag = data[offset++];
            switch (tag)
            {
            case 1:
                {
                    // Utf8
                    uint16_t len = read_u2();
                    if (offset + len > data.size()) return false;
                    utf8_pool[i] = std::string(reinterpret_cast<const char*>(&data[offset]), len);
                    offset += len;
                    break;
                }
            case 5:
            case 6: //Long, Double: 2 slots
                offset += 8;
                ++i;
                break;
            case 3:
            case 4:
            case 12: //Integer, Float, NameAndType
                offset += 4;
                break;
            case 7:
            case 8:
            case 16:
            case 19:
            case 20: //Class, String, etc.
                class_idx[i] = read_u2();
                break;
            case 9:
            case 10:
            case 11:
            case 17:
            case 18: //Refs, Dynamic
                offset += 4;
                break;
            case 15: //MethodHandle
                offset += 3;
                break;
            default:
                return false; //Unknown tag
            }
        }

        read_u2(); //access_flags
        uint16_t this_class = read_u2();
        uint16_t super_class = read_u2();

        auto getClass = [&](uint16_t idx) -> std::string
        {
            if (idx == 0 || idx >= cp_count) return "";
            uint16_t nameIdx = class_idx[idx];
            if (nameIdx == 0 || nameIdx >= cp_count) return "";
            return utf8_pool[nameIdx];
        };

        outInfo.name = getClass(this_class);
        outInfo.superName = getClass(super_class);

        uint16_t ifaces_count = read_u2();
        for (uint16_t i = 0; i < ifaces_count; ++i)
        {
            uint16_t iface_idx = read_u2();
            std::string iface = getClass(iface_idx);
            if (!iface.empty()) outInfo.interfaces.push_back(iface);
        }
        return true;
    }
} // namespace mc
