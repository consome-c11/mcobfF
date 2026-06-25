#include "VersionDownloader.h"
#include "HttpsClient.h"
#include "mcobfF/Logger.h"
#include "mcobfF/config/ApiConfig.h"

#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace mcobfF
{
    std::optional<json> VersionDownloader::fetchManifest()
    {
        const auto data = HttpsClient::get(ApiConfig::MANIFEST_URL);
        if (!data)
        {
            Logger::error("VersionDownloader") << "Failed to fetch version manifest";
            return std::nullopt;
        }
        try
        {
            return json::parse(*data);
        }
        catch (const std::exception& e)
        {
            Logger::error("VersionDownloader") << "Failed to parse manifest JSON: " << e.what();
            return std::nullopt;
        }
    }

    std::optional<std::string> VersionDownloader::getVersionUrl(const std::string& version)
    {
        const std::optional<nlohmann::json> manifest = fetchManifest();
        if (!manifest) return std::nullopt;

        if (!manifest->contains("versions"))
        {
            Logger::error("VersionDownloader") << "'versions' not found in manifest";
            return std::nullopt;
        }

        for (const auto& v : (*manifest)["versions"])
        {
            if (v.contains("id") && v["id"] == version && v.contains("url"))
            {
                return v["url"].get<std::string>();
            }
        }

        Logger::error("VersionDownloader") << "Version '" << version << "' not found in manifest";
        return std::nullopt;
    }

    std::optional<json> VersionDownloader::fetchVersionJson(const std::string& version)
    {
        const auto versionUrl = getVersionUrl(version);
        if (!versionUrl) return std::nullopt;

        const auto versionData = HttpsClient::get(*versionUrl);
        if (!versionData)
        {
            Logger::error("VersionDownloader") << "Failed to fetch version JSON for: " << version;
            return std::nullopt;
        }

        try
        {
            return json::parse(*versionData);
        }
        catch (const std::exception& e)
        {
            Logger::error("VersionDownloader") << "Failed to parse version JSON: " << e.what();
            return std::nullopt;
        }
    }

    std::optional<std::string> VersionDownloader::getLatestRelease()
    {
        const auto manifest = fetchManifest();
        if (!manifest) return std::nullopt;

        if (!manifest->contains("latest") || !(*manifest)["latest"].contains("release"))
        {
            Logger::error("VersionDownloader") << "'latest.release' not found in manifest";
            return std::nullopt;
        }
        return (*manifest)["latest"]["release"].get<std::string>();
    }

    std::optional<std::string> VersionDownloader::getLatestSnapshot()
    {
        const auto manifest = fetchManifest();
        if (!manifest) return std::nullopt;

        if (!manifest->contains("latest") || !(*manifest)["latest"].contains("snapshot"))
        {
            Logger::error("VersionDownloader") << "'latest.snapshot' not found in manifest";
            return std::nullopt;
        }
        return (*manifest)["latest"]["snapshot"].get<std::string>();
    }

    std::optional<std::string> VersionDownloader::getClientJarUrl(const std::string& version)
    {
        const auto versionJson = fetchVersionJson(version);
        if (!versionJson) return std::nullopt;

        if (!versionJson->contains("downloads") || !(*versionJson)["downloads"].contains("client"))
        {
            Logger::error("VersionDownloader") << "'downloads.client' not found in version JSON for: " << version;
            return std::nullopt;
        }
        return (*versionJson)["downloads"]["client"]["url"].get<std::string>();
    }

    bool VersionDownloader::downloadClientJar(const std::string& version, const std::string& outputPath)
    {
        const auto jarUrl = getClientJarUrl(version);
        if (!jarUrl)
        {
            Logger::error("VersionDownloader") << "Could not determine download URL for version: " << version;
            return false;
        }

        Logger::info("VersionDownloader") << "Downloading client.jar for " << version << "...";
        Logger::info("VersionDownloader") << "URL: " << *jarUrl;

        if (!HttpsClient::downloadToFile(*jarUrl, outputPath))
        {
            Logger::error("VersionDownloader") << "Failed to download client.jar";
            return false;
        }

        Logger::info("VersionDownloader") << "Downloaded to: " << outputPath;
        return true;
    }
} // namespace mc