#pragma once

#include <string>
#include <vector>
#include <optional>

namespace mcobfF
{
    struct MethodMapping
    {
        std::string deobfName;
        std::string obfName;
        std::string returnType;
        std::vector<std::string> paramTypes;
        std::string jvmDescriptor;
        std::optional<int> startLine;
        std::optional<int> endLine;
        std::optional<std::string> intermediaryName;
        std::optional<std::string> srgName;
    };

    struct FieldMapping
    {
        std::string deobfName;
        std::string obfName;
        std::string type;
        std::optional<int> lineNumber;
        std::optional<std::string> intermediaryName;
        std::optional<std::string> srgName;
    };

    struct ClassMapping
    {
        std::string deobfClass;
        std::string obfClass;
        std::optional<std::string> intermediaryClass;
        std::optional<std::string> srgClass;
    };

    struct MappingEntry
    {
        ClassMapping classInfo;
        std::vector<MethodMapping> methods;
        std::vector<FieldMapping> fields;
    };
} // namespace mc
