#pragma once

#include "MappingData.h"
#include <string>
#include <vector>
#include <filesystem>
#include <optional>

namespace mcobfF
{
    class MappingResolver
    {
    public:
        explicit MappingResolver(const std::string& cacheBasePath);

        [[nodiscard]] bool initialize();
        [[nodiscard]] bool loadMappings(const std::string& version);

        [[nodiscard]] std::optional<std::string> resolveToObfuscated(const std::string& deobfuscatedName) const;
        [[nodiscard]] std::optional<std::string> resolveToDeobfuscated(const std::string& obfuscatedName) const;

        [[nodiscard]] std::optional<std::string> resolveMethodToObfuscated(
            const std::string& deobfClass,
            const std::string& methodName,
            const std::vector<std::string>& paramTypes) const;
        [[nodiscard]] std::optional<std::string> resolveMethodToDeobfuscated(
            const std::string& obfClass,
            const std::string& methodName,
            const std::vector<std::string>& paramTypes) const;

        [[nodiscard]] std::optional<std::string> resolveFieldToObfuscated(
            const std::string& deobfClass, const std::string& fieldName) const;
        [[nodiscard]] std::optional<std::string> resolveFieldToDeobfuscated(
            const std::string& obfClass, const std::string& fieldName) const;

        [[nodiscard]] const MappingData& getData() const { return mappings_; }
        [[nodiscard]] MappingData& getData() { return mappings_; }
        [[nodiscard]] bool isLoaded() const { return !mappings_.entries.empty(); }
        [[nodiscard]] std::string getCachePath(const std::string& version) const;
        [[nodiscard]] bool isPreMojmap() const { return isPreMojmap_; }

    private:
        [[nodiscard]] bool loadFromCache(const std::string& version);
        [[nodiscard]] bool downloadAndParseMappings(const std::string& version);
        [[nodiscard]] bool loadIntermediaryMappings(const std::string& version);
        [[nodiscard]] bool loadMcpMappings(const std::string& version);
        [[nodiscard]] static std::string normalizeClassName(const std::string& name);

        std::string cacheBasePath_;
        MappingData mappings_;
        std::string currentVersion_;
        bool isPreMojmap_ = false;
    };
} // namespace mc