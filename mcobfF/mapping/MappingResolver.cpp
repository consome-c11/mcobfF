#include "MappingResolver.h"
#include "MappingParser.h"
#include "TinyMappingParser.h"
#include <algorithm>

#include "mcobfF/Logger.h"
#include "mcobfF/config/ApiConfig.h"
#include "mcobfF/file/FileSystem.h"
#include "mcobfF/network/HttpsClient.h"
#include "mcobfF/network/VersionDownloader.h"
#include "mcobfF/zip/ZipArchive.h"

namespace fs = std::filesystem;

namespace mcobfF
{
    MappingResolver::MappingResolver(const std::string& cacheBasePath)
        : cacheBasePath_(cacheBasePath)
    {
    }

    bool MappingResolver::initialize()
    {
        fs::path absPath = fs::absolute(cacheBasePath_);
        cacheBasePath_ = absPath.string();

        Logger::info("MappingResolver") << "Initializing with cache path: " << cacheBasePath_;

        if (!FileSystem::createDirectories(cacheBasePath_))
        {
            Logger::error("MappingResolver") << "Failed to create cache directory: " << cacheBasePath_;
            return false;
        }

        Logger::info("MappingResolver") << "Cache directory ready";
        return true;
    }

    std::string MappingResolver::normalizeClassName(const std::string& name)
    {
        std::string result = name;
        if (result.starts_with("L") && result.ends_with(";"))
        {
            result = result.substr(1, result.size() - 2);
        }
        std::ranges::replace(result, '.', '/');
        return result;
    }

    bool MappingResolver::loadMappings(const std::string& version)
    {
        bool result = loadFromCache(version) || downloadAndParseMappings(version);
        if (result)
        {
            currentVersion_ = version;
            loadIntermediaryMappings(version);
        }
        return result;
    }

    bool MappingResolver::loadFromCache(const std::string& version)
    {
        fs::path cacheFile = fs::path(getCachePath(version)) / "client.txt";
        std::string cachePath = cacheFile.string();
        Logger::info("MappingResolver") << "Checking cache: " << cachePath;

        if (!FileSystem::fileExists(cachePath))
        {
            Logger::info("MappingResolver") << "Cache miss, file not found";
            return false;
        }

        const auto content = FileSystem::readFile(cachePath);
        if (!content)
        {
            Logger::error("MappingResolver") << "Failed to read cache file";
            return false;
        }

        if (!MappingParser::parse(*content, mappings_))
        {
            Logger::error("MappingResolver") << "Failed to parse cache file";
            return false;
        }

        return true;
    }

    bool MappingResolver::downloadAndParseMappings(const std::string& version)
    {
        Logger::info("MappingResolver") << "Downloading mappings for version: " << version;

        const auto versionJson = VersionDownloader::fetchVersionJson(version);
        if (!versionJson)
        {
            return false;
        }

        if (!versionJson->contains("downloads") ||
            !(*versionJson)["downloads"].contains("client_mappings"))
        {
            Logger::error("MappingResolver") << "'client_mappings' not found in downloads";
            return false;
        }

        const auto mappingsUrl = (*versionJson)["downloads"]["client_mappings"]["url"].get<std::string>();

        const auto mappingsContent = HttpsClient::get(mappingsUrl);
        if (!mappingsContent)
        {
            Logger::error("MappingResolver") << "Failed to fetch mappings content";
            return false;
        }

        if (!MappingParser::parse(*mappingsContent, mappings_))
        {
            Logger::warn("MappingResolver") << "Failed to parse mappings content";
            return false;
        }

        const std::string cachePath = getCachePath(version);
        fs::create_directories(cachePath);
        if (const fs::path cacheFilePath = fs::path(cachePath) / "client.txt"; !FileSystem::writeFile(
            cacheFilePath.string(), *mappingsContent))
        {
            Logger::warn("MappingResolver") << "Failed to write cache file";
        }

        currentVersion_ = version;
        Logger::info("MappingResolver") << "Initialization complete for version: " << version;
        return true;
    }

    std::string MappingResolver::getCachePath(const std::string& version) const
    {
        return (fs::path(cacheBasePath_) / version).string();
    }

    std::optional<std::string> MappingResolver::resolveToObfuscated(const std::string& deobfuscatedName) const
    {
        return mappings_.remapClass(normalizeClassName(deobfuscatedName), true);
    }

    std::optional<std::string> MappingResolver::resolveToDeobfuscated(const std::string& obfuscatedName) const
    {
        return mappings_.remapClass(normalizeClassName(obfuscatedName), false);
    }

    std::optional<std::string> MappingResolver::resolveMethodToObfuscated(
        const std::string& deobfClass,
        const std::string& methodName,
        const std::vector<std::string>& paramTypes) const
    {
        std::string normalizedClass = normalizeClassName(deobfClass);
        std::vector<std::string> normalizedParams;
        normalizedParams.reserve(paramTypes.size());
        for (const auto& p : paramTypes) normalizedParams.push_back(normalizeClassName(p));
        return mappings_.remapMethod(normalizedClass, methodName, normalizedParams, true);
    }

    std::optional<std::string> MappingResolver::resolveMethodToDeobfuscated(
        const std::string& obfClass,
        const std::string& methodName,
        const std::vector<std::string>& paramTypes) const
    {
        std::string normalizedClass = normalizeClassName(obfClass);
        std::vector<std::string> normalizedParams;
        normalizedParams.reserve(paramTypes.size());
        for (const auto& p : paramTypes) normalizedParams.push_back(normalizeClassName(p));
        return mappings_.remapMethod(normalizedClass, methodName, normalizedParams, false);
    }

    std::optional<std::string> MappingResolver::resolveFieldToObfuscated(
        const std::string& deobfClass,
        const std::string& fieldName) const
    {
        return mappings_.remapField(normalizeClassName(deobfClass), fieldName, true);
    }

    std::optional<std::string> MappingResolver::resolveFieldToDeobfuscated(
        const std::string& obfClass,
        const std::string& fieldName) const
    {
        return mappings_.remapField(normalizeClassName(obfClass), fieldName, false);
    }

    bool MappingResolver::loadIntermediaryMappings(const std::string& version)
    {
        const std::string interJarUrl = std::string(ApiConfig::FABRIC_INTERMEDIARY_MAVEN) + "/" +
            version + "/intermediary-" + version + ".jar";
        const fs::path interJarPath = fs::path(getCachePath(version)) / "intermediary.jar";

        Logger::info("MappingResolver") << "Downloading Fabric Intermediary from: " << interJarUrl;

        if (!fs::exists(interJarPath))
        {
            if (!HttpsClient::downloadToFile(interJarUrl, interJarPath.string()))
            {
                Logger::warn("MappingResolver") << "Failed to download Fabric Intermediary JAR";
                return false;
            }
        }

        ZipArchive zip;
        if (!zip.open(interJarPath.string()))
        {
            Logger::warn("MappingResolver") << "Failed to open Intermediary JAR";
            return false;
        }

        const int tinyIndex = zip.locateFile("mappings/mappings.tiny");
        if (tinyIndex == -1)
        {
            Logger::warn("MappingResolver") << "mappings/mappings.tiny not found in Intermediary JAR";
            return false;
        }

        const auto tinyBuffer = zip.extractToMemory(tinyIndex);
        if (!tinyBuffer)
        {
            Logger::warn("MappingResolver") << "Failed to extract mappings.tiny";
            return false;
        }

        std::string tinyContent(tinyBuffer->data(), tinyBuffer->size());
        TinyMappingParser::parse(tinyContent, mappings_);

        Logger::info("MappingResolver") << "Successfully loaded Fabric Intermediary mappings";
        return true;
    }
} // namespace mc
