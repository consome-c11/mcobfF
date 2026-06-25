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

        [[nodiscard]] static bool downloadAndLoadStandalone(const std::string& version, const std::string& cacheDir,
                                                            MappingData& mappings);

        [[nodiscard]] static bool isVersionPreMojmap(const std::string& version);

    private:
        [[nodiscard]] static std::string convertJoinedSrgToTsrg(const std::string& content);
    };
} // namespace mc