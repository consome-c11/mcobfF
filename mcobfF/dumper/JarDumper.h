#pragma once

#include <string>

namespace mcobfF {
    class JarDumper {
    public:
        static void setCacheDir(const std::string& dir);

        [[nodiscard]] static bool dump(const std::string& jarPath, const std::string& version, const std::string& outputPath);

        [[nodiscard]] static bool dumpFromVersion(const std::string& version, const std::string& outputPath);

    private:
        [[nodiscard]] static std::string getDefaultCacheDir();

        static std::string cacheDir_;
    };
}
