#pragma once

#include <string>
#include <unordered_map>
#include <vector>

namespace mcobfF
{
    struct ClassInfo
    {
        std::string name;
        std::string superName;
        std::vector<std::string> interfaces;
    };

    struct ClassNode
    {
        std::string superClass; //obfed
        std::vector<std::string> interfaces; //obfed
    };

    using ClassHierarchy = std::unordered_map<std::string, ClassNode>;
} // namespace mc
