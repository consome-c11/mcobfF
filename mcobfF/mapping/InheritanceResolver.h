#pragma once

#include "mcobfF/class/ClassInfo.h"
#include "MappingData.h"
#include <unordered_set>

namespace mcobfF
{
    class InheritanceResolver
    {
    public:
        InheritanceResolver(MappingData& mappings, const ClassHierarchy& hierarchy);

        void resolveAll();

    private:
        void resolveClass(const std::string& obfClassName);

        void inheritFrom(const std::string& childObf, const std::string& parentObf) const;

        MappingData& mappings_;
        const ClassHierarchy& hierarchy_;
        std::unordered_set<std::string> resolved_;
        std::unordered_set<std::string> visiting_;
    };
} // namespace mc
