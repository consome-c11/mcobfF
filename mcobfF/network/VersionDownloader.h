#pragma once

#include <string>
#include <optional>
#include <nlohmann/json.hpp>

namespace mcobfF
{
    class VersionDownloader
    {
    public:
        [[nodiscard]] static std::optional<std::string> getLatestRelease();
        [[nodiscard]] static std::optional<std::string> getLatestSnapshot();
        [[nodiscard]] static std::optional<std::string> getClientJarUrl(const std::string& version);
        [[nodiscard]] static bool downloadClientJar(const std::string& version, const std::string& outputPath);

        [[nodiscard]] static std::optional<nlohmann::json> fetchManifest();
        [[nodiscard]] static std::optional<std::string> getVersionUrl(const std::string& version);
        [[nodiscard]] static std::optional<nlohmann::json> fetchVersionJson(const std::string& version);
    };
} // namespace mc