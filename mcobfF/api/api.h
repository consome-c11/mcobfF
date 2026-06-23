#pragma once

#include <string>
#include <vector>
#include <optional>
#include <memory>
#include "mcobfF/class/ClassInfo.h"
#include "mcobfF/mapping/MappingData.h"

namespace mcobfF
{
    class MappingResolver;
    class ZipArchive;

    class api
    {
    public:
        api();
        explicit api(const std::string& cacheDir);
        ~api();

        api(const api&) = delete;
        api& operator=(const api&) = delete;
        api(api&&) = delete;
        api& operator=(api&&) = delete;

        static std::optional<std::string> getLatestReleaseVersion();
        static std::optional<std::string> getLatestSnapshotVersion();

        bool loadMappings(const std::string& version);
        bool loadMappingsWithInheritance(const std::string& version, const std::string& jarPath);

        std::optional<std::string> resolveClass(const std::string& className, bool deobfToObf = true) const;
        std::optional<std::string> resolveMethod(const std::string& className,
                                                 const std::string& methodName,
                                                 const std::vector<std::string>& paramTypes = {},
                                                 bool deobfToObf = true) const;
        std::optional<std::string> resolveField(const std::string& className,
                                                const std::string& fieldName,
                                                bool deobfToObf = true) const;

        bool loadJar(const std::string& jarPath);
        ClassHierarchy buildClassHierarchy();

        static bool dumpMappings(const std::string& version, const std::string& outputPath);
        static bool dumpJarMappings(const std::string& jarPath, const std::string& version,
                                    const std::string& outputPath);

        const MappingData& getMappingData() const;
        bool isMappingLoaded() const;
        const std::string& getCurrentVersion() const;

    private:
        std::unique_ptr<MappingResolver> resolver_;
        std::unique_ptr<ZipArchive> zip_;
        ClassHierarchy hierarchy_;
        std::string cacheDir_;
        std::string currentVersion_;
    };
} // namespace mc
