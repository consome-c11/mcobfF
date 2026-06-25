#pragma once

#include "mcobfF/mapping/MappingData.h"
#include <string>

namespace mcobfF
{
    class MappingWriter
    {
    public:
        static std::string generate(const MappingData& mappings);
        static bool writeToFile(const MappingData& mappings, const std::string& filePath);
    };
}