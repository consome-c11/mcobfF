#pragma once

#include "ClassInfo.h"

#include "mcobfF/zip/ZipArchive.h"

namespace mcobfF
{
    class ClassHierarchyBuilder
    {
    public:
        static ClassHierarchy buildFromJar(ZipArchive& zip);
    };
} // namespace mc
