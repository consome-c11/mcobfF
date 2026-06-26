#pragma once

#include <string>

namespace mcobfF
{
    class JreDownloader
    {
    public:
        [[nodiscard]] static bool ensureJre();
        [[nodiscard]] static std::string getJreCachePath();
        [[nodiscard]] static bool isJavaAvailable();
        [[nodiscard]] static std::string findExistingJava();

    private:
        [[nodiscard]] static bool downloadJre(const std::string& outputZip);
        [[nodiscard]] static bool extractJre(const std::string& zipPath, const std::string& outputDir);
    };
}
