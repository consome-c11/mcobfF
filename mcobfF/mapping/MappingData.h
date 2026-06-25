#pragma once

#include "mcobfF/Types.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <optional>
#include <iosfwd>

namespace mcobfF
{
    struct MappingData
    {
        std::vector<MappingEntry> entries;
        std::unordered_map<std::string, size_t> deobfClassIndex;
        std::unordered_map<std::string, size_t> obfClassIndex;

        [[nodiscard]] std::optional<std::string> remapClass(
            const std::string& className, bool deobfToObf = true) const;
        [[nodiscard]] std::optional<std::string> remapMethod(
            const std::string& className,
            const std::string& methodName,
            const std::vector<std::string>& params,
            bool deobfToObf = true) const;
        [[nodiscard]] std::optional<std::string> remapField(
            const std::string& className,
            const std::string& fieldName,
            bool deobfToObf = true) const;
        [[nodiscard]] static std::optional<std::string> readClassName(std::istream& classFileStream);
    };
} // namespace mc