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

    bool SRGParser::isOldSrgFormat(const std::string& content)
    {
        std::istringstream stream(content);
        std::string line;
        while (std::getline(stream, line))
        {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty() || line[0] == '#') continue;
            if (line.rfind("CL:", 0) == 0 || line.rfind("FD:", 0) == 0 ||
                line.rfind("MD:", 0) == 0 || line.rfind("PK:", 0) == 0)
            {
                return true;
            }
            return false;
        }
        return false;
    }

    bool SRGParser::parseOldSrg(const std::string& content, MappingData& mappings)
    {
        std::istringstream stream(content);
        std::string line;

        while (std::getline(stream, line))
        {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty() || line[0] == '#') continue;

            if (line.rfind("CL: ", 0) == 0)
            {
                std::istringstream iss(line.substr(4));
                std::string obfClass, srgClass;
                iss >> obfClass >> srgClass;
                if (obfClass.empty() || srgClass.empty()) continue;

                ClassMapping cm;
                cm.obfClass = obfClass;
                cm.deobfClass = srgClass;
                cm.srgClass = srgClass;

                mappings.entries.push_back({cm, {}, {}});
                size_t idx = mappings.entries.size() - 1;
                mappings.obfClassIndex[obfClass] = idx;
                mappings.deobfClassIndex[srgClass] = idx;
            }
            else if (line.rfind("FD: ", 0) == 0)
            {
                std::istringstream iss(line.substr(4));
                std::string obfFull, srgFull;
                iss >> obfFull >> srgFull;
                if (obfFull.empty() || srgFull.empty()) continue;

                size_t obfLastSlash = obfFull.rfind('/');
                std::string obfClass = (obfLastSlash != std::string::npos) ? obfFull.substr(0, obfLastSlash) : obfFull;
                std::string obfField = (obfLastSlash != std::string::npos) ? obfFull.substr(obfLastSlash + 1) : obfFull;

                size_t srgLastSlash = srgFull.rfind('/');
                std::string srgField = (srgLastSlash != std::string::npos) ? srgFull.substr(srgLastSlash + 1) : srgFull;

                auto it = mappings.obfClassIndex.find(obfClass);
                if (it == mappings.obfClassIndex.end()) continue;

                auto& entry = mappings.entries[it->second];
                bool found = false;
                for (auto& f : entry.fields)
                {
                    if (f.obfName == obfField)
                    {
                        f.srgName = srgField;
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    FieldMapping fm;
                    fm.obfName = obfField;
                    fm.deobfName = srgField;
                    fm.srgName = srgField;
                    entry.fields.push_back(std::move(fm));
                }
            }
            else if (line.rfind("MD: ", 0) == 0)
            {
                std::istringstream iss(line.substr(4));
                std::string obfFull, obfDesc, srgFull, srgDesc;
                iss >> obfFull >> obfDesc >> srgFull >> srgDesc;
                if (obfFull.empty() || obfDesc.empty() || srgFull.empty()) continue;

                size_t obfLastSlash = obfFull.rfind('/');
                std::string obfClass = (obfLastSlash != std::string::npos) ? obfFull.substr(0, obfLastSlash) : obfFull;
                std::string obfMethod = (obfLastSlash != std::string::npos)
                                            ? obfFull.substr(obfLastSlash + 1)
                                            : obfFull;

                size_t srgLastSlash = srgFull.rfind('/');
                std::string srgMethod = (srgLastSlash != std::string::npos)
                                            ? srgFull.substr(srgLastSlash + 1)
                                            : srgFull;

                auto it = mappings.obfClassIndex.find(obfClass);
                if (it == mappings.obfClassIndex.end()) continue;

                auto& entry = mappings.entries[it->second];
                std::string remappedDesc = remapDescriptor(obfDesc, mappings);

                bool found = false;
                for (auto& m : entry.methods)
                {
                    if (m.obfName == obfMethod && m.jvmDescriptor == remappedDesc)
                    {
                        m.srgName = srgMethod;
                        found = true;
                        break;
                    }
                }
                if (!found)
                {
                    MethodMapping mm;
                    mm.obfName = obfMethod;
                    mm.deobfName = srgMethod;
                    mm.jvmDescriptor = remappedDesc;
                    mm.srgName = srgMethod;
                    parseDescriptor(remappedDesc, mm.returnType, mm.paramTypes);
                    entry.methods.push_back(std::move(mm));
                }
            }
        }

        return !mappings.entries.empty();
    }

    bool SRGParser::parseTsrg(const std::string& content, MappingData& mappings)
    {
        std::istringstream stream(content);
        std::string line;
        MappingEntry* currentEntry = nullptr;

        while (std::getline(stream, line))
        {
            if (!line.empty() && line.back() == '\r') line.pop_back();
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

            if (tabs == 0 && tokens.size() >= 2)
            {
                ClassMapping cm;
                cm.obfClass = tokens[0];
                cm.deobfClass = tokens[1];
                cm.srgClass = tokens[1];

                mappings.entries.push_back({cm, {}, {}});
                size_t idx = mappings.entries.size() - 1;
                mappings.obfClassIndex[tokens[0]] = idx;
                mappings.deobfClassIndex[tokens[1]] = idx;
                currentEntry = &mappings.entries[idx];
            }
            else if (tabs == 1 && currentEntry)
            {
                if (tokens.size() >= 3 && tokens[1].find('(') != std::string::npos)
                {
                    MethodMapping mm;
                    mm.obfName = tokens[0];
                    mm.deobfName = tokens[2];
                    mm.jvmDescriptor = tokens[1];
                    mm.srgName = tokens[2];
                    parseDescriptor(mm.jvmDescriptor, mm.returnType, mm.paramTypes);
                    currentEntry->methods.push_back(std::move(mm));
                }
                else if (tokens.size() >= 2)
                {
                    FieldMapping fm;
                    fm.obfName = tokens[0];
                    fm.deobfName = tokens[1];
                    fm.srgName = tokens[1];
                    currentEntry->fields.push_back(std::move(fm));
                }
            }
        }

        return !mappings.entries.empty();
    }

    bool SRGParser::parseStandalone(const std::string& content, MappingData& mappings)
    {
        if (content.empty()) return false;

        if (isOldSrgFormat(content))
        {
            return parseOldSrg(content, mappings);
        }
        return parseTsrg(content, mappings);
    }
} // namespace mc
