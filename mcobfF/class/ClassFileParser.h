#pragma once

#include "ClassInfo.h"
#include <span>
#include <cstdint>

namespace mcobfF
{
    class ClassFileParser
    {
    public:
        static bool parse(std::span<const uint8_t> data, ClassInfo& outInfo);
    };
} // namespace mc