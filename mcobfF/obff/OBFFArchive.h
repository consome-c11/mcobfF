#pragma once

#include <string>
#include <optional>

namespace mcobfF
{
    class OBFFArchive
    {
    public:
        static bool pack(const std::string& sourceDir, const std::string& obffPath);
        static std::optional<std::string> readEntry(const std::string& obffPath,
                                                     const std::string& entryName);
        static bool hasEntry(const std::string& obffPath, const std::string& entryName);
        static std::string getEntryName(const std::string& className);
        static std::string getObffPath(const std::string& decompileCacheDir);
    };
}
