#pragma once

#include <string>
#include <optional>

namespace mcobfF
{
    class FileSystem
    {
    public:
        [[nodiscard]] static bool createDirectories(const std::string& path);
        [[nodiscard]] static bool fileExists(const std::string& path);
        [[nodiscard]] static std::optional<std::string> readFile(const std::string& path);
        [[nodiscard]] static bool writeFile(const std::string& path, const std::string& content);
    };
} // namespace mc