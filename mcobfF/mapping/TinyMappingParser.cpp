#include "TinyMappingParser.h"
#include <sstream>
#include <algorithm>

#include "MappingParser.h"

namespace mcobfF
{
    namespace
    {
        std::string remapDescriptor(const std::string& desc, const MappingData& mappings)
        {
            std::string result;
            result.reserve(desc.size());
            size_t i = 0;

            while (i < desc.size())
            {
                if (desc[i] == 'L')
                {
                    const size_t end = desc.find(';', i + 1);
                    if (end == std::string::npos)
                    {
                        result += desc.substr(i);
                        break;
                    }
                    std::string obfCls = desc.substr(i + 1, end - i - 1);
                    auto it = mappings.obfClassIndex.find(obfCls);
                    if (it != mappings.obfClassIndex.end())
                    {
                        result += "L" + mappings.entries[it->second].classInfo.deobfClass + ";";
                    }
                    else
                    {
                        result += "L" + obfCls + ";";
                    }
                    i = end + 1;
                }
                else
                {
                    result += desc[i];
                    i++;
                }
            }
            return result;
        }

        void processClassMapping(
            std::istringstream& lineStream,
            MappingData& mappings,
            std::string& currentObfClass)
        {
            std::string obf, inter;
            lineStream >> obf >> inter;
            if (obf.empty() || inter.empty()) return;

            if (const auto it = mappings.obfClassIndex.find(obf); it != mappings.obfClassIndex.end())
            {
                mappings.entries[it->second].classInfo.intermediaryClass = inter;
            }
            currentObfClass = obf;
        }

        void processFieldMapping(std::istringstream& lineStream, MappingData& mappings)
        {
            std::string className, desc, obf, inter;
            lineStream >> className >> desc >> obf >> inter;

            if (!inter.empty() && inter.back() == '\r') inter.pop_back();
            if (!desc.empty() && desc.back() == '\r') desc.pop_back();

            if (className.empty() || obf.empty() || inter.empty()) return;

            const auto classIt = mappings.obfClassIndex.find(className);
            if (classIt == mappings.obfClassIndex.end()) return;

            const std::string remappedDesc = remapDescriptor(desc, mappings);
            std::string normalizedDesc = MappingParser::descriptorToNormalizedType(remappedDesc);

            for (auto& entry = mappings.entries[classIt->second]; auto& field : entry.fields)
            {
                if (field.obfName == obf)
                {
                    field.intermediaryName = inter;
                    break;
                }
            }
        }

        void processMethodMapping(std::istringstream& lineStream, MappingData& mappings)
        {
            std::string className, desc, obf, inter;
            lineStream >> className >> desc >> obf >> inter;

            if (!inter.empty() && inter.back() == '\r') inter.pop_back();
            if (!desc.empty() && desc.back() == '\r') desc.pop_back();

            if (className.empty() || obf.empty() || inter.empty()) return;

            const auto classIt = mappings.obfClassIndex.find(className);
            if (classIt == mappings.obfClassIndex.end()) return;

            std::string remappedDesc = remapDescriptor(desc, mappings);

            for (auto& entry = mappings.entries[classIt->second]; auto& method : entry.methods)
            {
                if (method.obfName == obf && method.jvmDescriptor == remappedDesc)
                {
                    method.intermediaryName = inter;
                    break;
                }
            }
        }
    }

    bool TinyMappingParser::parse(const std::string& content, MappingData& mappings)
    {
        std::istringstream stream(content);
        std::string line;

        std::getline(stream, line);

        std::string currentObfClass;

        while (std::getline(stream, line))
        {
            if (line.empty() || line.starts_with("#")) continue;
            if (!line.empty() && line.back() == '\r') line.pop_back();

            std::istringstream lineStream(line);
            std::string type;
            lineStream >> type;

            if (type == "CLASS")
            {
                processClassMapping(lineStream, mappings, currentObfClass);
            }
            else if (type == "FIELD")
            {
                processFieldMapping(lineStream, mappings);
            }
            else if (type == "METHOD")
            {
                processMethodMapping(lineStream, mappings);
            }
        }
        return true;
    }
} // namespace mc
