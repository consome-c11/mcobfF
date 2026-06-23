#pragma once

#include "MappingData.h"
#include <string>

namespace mcobfF
{
    class SRGResolver
    {
    public:
        [[nodiscard]] static bool loadFromZip(const std::string& zipPath, MappingData& mappings);
        [[nodiscard]] static bool loadFromFile(const std::string& filePath, MappingData& mappings);
        [[nodiscard]] static bool downloadAndLoad(const std::string& version, const std::string& cacheDir,
                                                  MappingData& mappings);

    private:
        [[nodiscard]] static bool isVersionAtLeast1122(const std::string& version);
        [[nodiscard]] static std::string convertJoinedSrgToTsrg(const std::string& content);
    };
} // namespace mc
