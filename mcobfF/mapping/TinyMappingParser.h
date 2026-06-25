#pragma once

#include "MappingData.h"
#include <string>

namespace mcobfF
{
    class TinyMappingParser
    {
    public:
        [[nodiscard]] static bool parse(const std::string& content, MappingData& mappings);
    };
} // namespace mc
