#include "MappingData.h"
#include <cstdint>
#include <istream>

namespace
{
    constexpr uint32_t CLASS_FILE_MAGIC = 0xCAFEBABE;

    bool readU2(std::istream& is, uint16_t& out)
    {
        const int b1 = is.get();
        const int b2 = is.get();
        if (b1 == std::char_traits<char>::eof() || b2 == std::char_traits<char>::eof()) return false;
        out = static_cast<uint16_t>((b1 << 8) | b2);
        return true;
    }

    bool readU4(std::istream& is, uint32_t& out)
    {
        const int b1 = is.get();
        const int b2 = is.get();
        const int b3 = is.get();
        const int b4 = is.get();
        if (b1 == std::char_traits<char>::eof() || b2 == std::char_traits<char>::eof() ||
            b3 == std::char_traits<char>::eof() || b4 == std::char_traits<char>::eof())
            return false;
        out = (static_cast<uint32_t>(b1) << 24) | (static_cast<uint32_t>(b2) << 16) |
            (static_cast<uint32_t>(b3) << 8) | static_cast<uint32_t>(b4);
        return true;
    }

    bool readAndSkipBytes(std::istream& is, const size_t count)
    {
        for (size_t i = 0; i < count; ++i)
        {
            if (is.get() == std::char_traits<char>::eof()) return false;
        }
        return true;
    }

    struct ConstantPoolEntry
    {
        uint8_t tag = 0;
        std::string str;
        uint16_t index1 = 0;
        uint16_t index2 = 0;
    };

    bool readConstantPoolEntry(std::istream& is, ConstantPoolEntry& entry)
    {
        const int tag = is.get();
        if (tag == std::char_traits<char>::eof()) return false;
        entry.tag = static_cast<uint8_t>(tag);

        switch (entry.tag)
        {
        case 1:
            {
                uint16_t len;
                if (!readU2(is, len)) return false;
                entry.str.resize(len);
                is.read(&entry.str[0], len);
                return is.good() || is.eof();
            }
        case 3:
        case 4:
            return readAndSkipBytes(is, 4);
        case 5:
        case 6:
            return readAndSkipBytes(is, 8);
        case 7:
        case 8:
        case 16:
        case 19:
        case 20:
            return readU2(is, entry.index1);
        case 9:
        case 10:
        case 11:
        case 12:
        case 17:
        case 18:
            return readU2(is, entry.index1) && readU2(is, entry.index2);
        case 15:
            return readAndSkipBytes(is, 1) && readU2(is, entry.index1);
        default:
            return false;
        }
    }
}

namespace mcobfF
{
    std::optional<std::string> MappingData::remapClass(const std::string& className, bool deobfToObf) const
    {
        const auto& index = deobfToObf ? deobfClassIndex : obfClassIndex;
        auto it = index.find(className);
        if (it != index.end())
        {
            const auto& entry = entries[it->second];
            return deobfToObf ? entry.classInfo.obfClass : entry.classInfo.deobfClass;
        }
        return std::nullopt;
    }

    std::optional<std::string> MappingData::remapMethod(const std::string& className, const std::string& methodName,
                                                        const std::vector<std::string>& params, bool deobfToObf) const
    {
        const auto& index = deobfToObf ? deobfClassIndex : obfClassIndex;
        auto it = index.find(className);
        if (it == index.end()) return std::nullopt;

        const auto& entry = entries[it->second];
        for (const auto& m : entry.methods)
        {
            if (deobfToObf)
            {
                if (m.deobfName == methodName && m.paramTypes == params) return m.obfName;
            }
            else
            {
                if (m.obfName == methodName) return m.deobfName;
            }
        }
        return std::nullopt;
    }

    std::optional<std::string> MappingData::remapField(const std::string& className, const std::string& fieldName,
                                                       bool deobfToObf) const
    {
        const auto& index = deobfToObf ? deobfClassIndex : obfClassIndex;
        auto it = index.find(className);
        if (it == index.end()) return std::nullopt;

        const auto& entry = entries[it->second];
        for (const auto& f : entry.fields)
        {
            if (deobfToObf)
            {
                if (f.deobfName == fieldName) return f.obfName;
            }
            else
            {
                if (f.obfName == fieldName) return f.deobfName;
            }
        }
        return std::nullopt;
    }

    std::optional<std::string> MappingData::readClassName(std::istream& classFileStream)
    {
        uint32_t magic;
        if (!readU4(classFileStream, magic) || magic != CLASS_FILE_MAGIC) return std::nullopt;

        uint16_t minor, major, cpCount;
        if (!readU2(classFileStream, minor) || !readU2(classFileStream, major) || !readU2(classFileStream, cpCount))
            return std::nullopt;

        std::vector<ConstantPoolEntry> cp(cpCount);
        for (uint16_t i = 1; i < cpCount; ++i)
        {
            if (!readConstantPoolEntry(classFileStream, cp[i])) return std::nullopt;
            if (cp[i].tag == 5 || cp[i].tag == 6) ++i;
        }

        uint16_t accessFlags, thisClass, superClass;
        if (!readU2(classFileStream, accessFlags) || !readU2(classFileStream, thisClass) || !readU2(
            classFileStream, superClass)) return std::nullopt;

        if (thisClass < 1 || thisClass >= cpCount || cp[thisClass].tag != 7) return std::nullopt;

        uint16_t nameIndex = cp[thisClass].index1;
        if (nameIndex < 1 || nameIndex >= cpCount || cp[nameIndex].tag != 1) return std::nullopt;

        return cp[nameIndex].str;
    }
} // namespace mc