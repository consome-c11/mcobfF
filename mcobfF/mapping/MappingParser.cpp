#include "MappingParser.h"
#include <regex>
#include <sstream>
#include <algorithm>

namespace mcobfF
{
    namespace
    {
        const std::regex& getClassRegex()
        {
            static const std::regex regex(R"(^([a-zA-Z0-9_.$/]+)\s*->\s*([a-zA-Z0-9_.$]+):\r?$)");
            return regex;
        }

        const std::regex& getMethodRegex()
        {
            static const std::regex regex(
                R"(^\s*(?:(\d+)(?::(\d+))?:)?\s*([a-zA-Z0-9_.\[\]$]+)\s+([a-zA-Z0-9_$<>]+)\s*\(([^)]*)\)\s*->\s*([a-zA-Z0-9_$<>]+)\r?$)"
            );
            return regex;
        }

        const std::regex& getFieldRegex()
        {
            static const std::regex regex(
                R"(^\s*(?:(\d+)(?::(\d+))?:)?\s*([a-zA-Z0-9_.\[\]$]+)\s+([a-zA-Z0-9_$]+)\s*->\s*([a-zA-Z0-9_$]+)\r?$)"
            );
            return regex;
        }

        std::vector<std::string> parseParams(const std::string& params)
        {
            std::vector<std::string> result;
            if (params.empty()) return result;

            std::istringstream paramStream(params);
            std::string param;
            while (std::getline(paramStream, param, ','))
            {
                result.push_back(MappingParser::normalizeType(param));
            }
            return result;
        }

        std::string buildDescriptor(const std::vector<std::string>& paramTypes, const std::string& returnType)
        {
            std::string descriptor = "(";
            for (const auto& p : paramTypes)
            {
                descriptor += MappingParser::typeToDescriptor(p);
            }
            descriptor += ")" + MappingParser::typeToDescriptor(returnType);
            return descriptor;
        }

        std::optional<ClassMapping> parseClassLine(const std::smatch& match)
        {
            ClassMapping c;
            c.deobfClass = MappingParser::normalizeType(match[1].str());
            c.obfClass = MappingParser::normalizeType(match[2].str());
            return c;
        }

        std::optional<MethodMapping> parseMethodLine(const std::smatch& match)
        {
            MethodMapping m;
            if (match[1].matched) m.startLine = std::stoi(match[1].str());
            if (match[2].matched) m.endLine = std::stoi(match[2].str());

            m.returnType = MappingParser::normalizeType(match[3].str());
            m.deobfName = match[4].str();
            m.obfName = match[6].str();
            m.paramTypes = parseParams(match[5].str());
            m.jvmDescriptor = buildDescriptor(m.paramTypes, m.returnType);

            return m;
        }

        std::optional<FieldMapping> parseFieldLine(const std::smatch& match)
        {
            FieldMapping f;
            if (match[1].matched) f.lineNumber = std::stoi(match[1].str());
            f.type = MappingParser::normalizeType(match[3].str());
            f.deobfName = match[4].str();
            f.obfName = match[5].str();
            return f;
        }
    }

    std::string MappingParser::typeToDescriptor(const std::string& type)
    {
        if (type.empty()) return "V";

        switch (type[0])
        {
        case 'b':
            if (type == "boolean") return "Z";
            if (type == "byte") return "B";
            break;
        case 'c': if (type == "char") return "C";
            break;
        case 's': if (type == "short") return "S";
            break;
        case 'i': if (type == "int") return "I";
            break;
        case 'j': if (type == "long") return "J";
            break;
        case 'f': if (type == "float") return "F";
            break;
        case 'd': if (type == "double") return "D";
            break;
        case 'v': if (type == "void") return "V";
            break;
        default: ;
        }

        if (!type.empty() && type.back() == ']')
        {
            if (const size_t bracketPos = type.find('['); bracketPos != std::string::npos)
            {
                const std::string base = type.substr(0, bracketPos);
                const size_t dims = std::count(type.begin() + bracketPos, type.end(), '[');
                return std::string(dims, '[') + typeToDescriptor(base);
            }
        }

        return "L" + normalizeType(type) + ";";
    }

    std::string MappingParser::normalizeType(const std::string& type)
    {
        std::string result = type;
        const size_t first = result.find_first_not_of(" \t");
        if (first == std::string::npos) return "";
        const size_t last = result.find_last_not_of(" \t");
        result = result.substr(first, (last - first + 1));
        std::ranges::replace(result, '.', '/');
        return result;
    }

    std::optional<MappingParser::ParsedLine> MappingParser::parseLine(const std::string& line)
    {
        std::smatch match;

        if (std::regex_match(line, match, getClassRegex()))
        {
            return parseClassLine(match);
        }

        if (std::regex_match(line, match, getMethodRegex()))
        {
            return parseMethodLine(match);
        }

        if (std::regex_match(line, match, getFieldRegex()))
        {
            return parseFieldLine(match);
        }

        return std::nullopt;
    }

    std::string MappingParser::descriptorToNormalizedType(const std::string& descriptor)
    {
        if (descriptor.empty()) return "";

        size_t arrayDepth = 0;
        size_t pos = 0;
        while (pos < descriptor.size() && descriptor[pos] == '[')
        {
            ++arrayDepth;
            ++pos;
        }

        if (pos >= descriptor.size())
        {
            return "";
        }

        std::string baseType;
        char typeChar = descriptor[pos];

        switch (typeChar)
        {
        case 'Z': baseType = "boolean";
            break;
        case 'B': baseType = "byte";
            break;
        case 'C': baseType = "char";
            break;
        case 'S': baseType = "short";
            break;
        case 'I': baseType = "int";
            break;
        case 'J': baseType = "long";
            break;
        case 'F': baseType = "float";
            break;
        case 'D': baseType = "double";
            break;
        case 'V': baseType = "void";
            break;
        case 'L':
            {
                const size_t endPos = descriptor.find(';', pos + 1);
                if (endPos == std::string::npos) return "";
                baseType = descriptor.substr(pos + 1, endPos - pos - 1);
                break;
            }
        default:
            return "";
        }

        for (size_t i = 0; i < arrayDepth; ++i)
        {
            baseType += "[]";
        }

        return normalizeType(baseType);
    }

    bool MappingParser::parse(const std::string& content, MappingData& mappings)
    {
        std::istringstream stream(content);
        std::string line;

        MappingEntry* currentEntry = nullptr;

        while (std::getline(stream, line))
        {
            if (!line.empty() && line.back() == '\r') line.pop_back();
            if (line.empty() || line.starts_with("#")) continue;

            auto parsed = parseLine(line);
            if (!parsed) continue;

            if (std::holds_alternative<ClassMapping>(*parsed))
            {
                auto& c = std::get<ClassMapping>(*parsed);
                mappings.entries.push_back({c, {}, {}});
                auto& entry = mappings.entries.back();

                mappings.deobfClassIndex[c.deobfClass] = mappings.entries.size() - 1;
                mappings.obfClassIndex[c.obfClass] = mappings.entries.size() - 1;
                currentEntry = &entry;
            }
            else if (std::holds_alternative<MethodMapping>(*parsed))
            {
                if (!currentEntry) continue;
                currentEntry->methods.push_back(std::get<MethodMapping>(*parsed));
            }
            else if (std::holds_alternative<FieldMapping>(*parsed))
            {
                if (!currentEntry) continue;
                currentEntry->fields.push_back(std::get<FieldMapping>(*parsed));
            }
        }
        return !mappings.entries.empty();
    }
} // namespace mc