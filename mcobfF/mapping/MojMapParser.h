#pragma once

#include "MappingData.h"
#include <string>
#include <optional>
#include <variant>

namespace mcobfF
{
    class MojMapParser
    {
    public:
        [[nodiscard]] static bool parse(const std::string& content, MappingData& mappings);
        [[nodiscard]] static std::string normalizeType(const std::string& type);
        [[nodiscard]] static std::string typeToDescriptor(const std::string& type);
        [[nodiscard]] static std::string descriptorToNormalizedType(const std::string& descriptor);

    private:
        using ParsedLine = std::variant<ClassMapping, MethodMapping, FieldMapping>;
        [[nodiscard]] static std::optional<ParsedLine> parseLine(const std::string& line);
    };
} // namespace mc