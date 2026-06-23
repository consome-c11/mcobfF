#include "SRGParser.h"
#include <sstream>
#include <algorithm>

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
                    if (auto it = mappings.obfClassIndex.find(obfCls); it != mappings.obfClassIndex.end())
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
    }

    std::string SRGParser::trimCR(const std::string& str)
    {
        if (!str.empty() && str.back() == '\r')
        {
            return str.substr(0, str.size() - 1);
        }
        return str;
    }

    void SRGParser::parseDescriptor(const std::string& desc, std::string& returnType,
                                    std::vector<std::string>& paramTypes)
    {
        if (desc.empty() || desc[0] != '(') return;

        size_t i = 1;
        while (i < desc.size() && desc[i] != ')')
        {
            const size_t start = i;
            if (desc[i] == 'L')
            {
                const size_t end = desc.find(';', i);
                if (end == std::string::npos) break;
                i = end + 1;
            }
            else if (desc[i] == '[')
            {
                i++;
                continue;
            }
            else
            {
                i++;
            }
            paramTypes.push_back(desc.substr(start, i - start));
        }

        if (i < desc.size() && desc[i] == ')')
        {
            returnType = desc.substr(i + 1);
        }
    }

    void SRGParser::processTokens(const std::vector<std::string>& tokens, MappingEntry*& currentEntry,
                                  MethodMapping*& currentMethod, const MappingData& mappings)
    {
        if (tokens.size() >= 3 && tokens[1].find('(') != std::string::npos)
        {
            currentMethod = nullptr;
            const std::string remappedDesc = remapDescriptor(tokens[1], mappings);
            for (auto& m : currentEntry->methods)
            {
                if (m.obfName == tokens[0] && m.jvmDescriptor == remappedDesc)
                {
                    m.srgName = tokens[2];
                    currentMethod = &m;
                    break;
                }
            }
        }
        else if (tokens.size() >= 2)
        {
            currentMethod = nullptr;
            for (auto& f : currentEntry->fields)
            {
                if (f.obfName == tokens[0])
                {
                    f.srgName = tokens[1];
                    break;
                }
            }
        }
    }

    bool SRGParser::parse(const std::string& content, MappingData& mappings)
    {
        std::istringstream stream(content);
        std::string line;

        MappingEntry* currentEntry = nullptr;
        MethodMapping* currentMethod = nullptr;

        while (std::getline(stream, line))
        {
            line = trimCR(line);
            if (line.empty() || line[0] == '#') continue;

            if (line.rfind("tsrg2", 0) == 0) continue;

            int tabs = 0;
            while (tabs < (int)line.size() && line[tabs] == '\t') tabs++;
            std::string trimmed = line.substr(tabs);

            std::vector<std::string> tokens;
            std::istringstream iss(trimmed);
            std::string token;
            while (iss >> token) tokens.push_back(token);

            if (tokens.empty()) continue;

            if (tabs == 0)
            {
                currentEntry = nullptr;
                currentMethod = nullptr;
                if (tokens.size() >= 2)
                {
                    if (auto it = mappings.obfClassIndex.find(tokens[0]); it != mappings.obfClassIndex.end())
                    {
                        currentEntry = &mappings.entries[it->second];
                        currentEntry->classInfo.srgClass = tokens[1];
                    }
                }
            }
            else if (tabs == 1 && currentEntry)
            {
                processTokens(tokens, currentEntry, currentMethod, mappings);
            }
            else if (tabs == 2 && currentMethod)
            {
            }
        }

        return true;
    }
} // namespace mc
