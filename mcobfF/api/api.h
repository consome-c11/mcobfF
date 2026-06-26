#pragma once

#include <string>
#include <vector>
#include <optional>
#include <memory>
#include <future>
#include <atomic>
#include <mutex>
#include "mcobfF/class/ClassInfo.h"
#include "mcobfF/mapping/MappingData.h"

namespace mcobfF
{
    class MappingResolver;
    class ZipArchive;
    class FernflowerDecompiler;

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

        bool initializeDecompiler();
        std::optional<std::string> decompileClass(const std::string& className);
        std::optional<std::string> decompileAndRemapClass(const std::string& className);
        bool isDecompilerAvailable() const;

        void startDecompileAllAsync();
        void cancelDecompileAll();
        bool isDecompilingAll() const;
        float getDecompileProgress() const;

        const std::string& getCurrentJarPath() const { return currentJarPath_; }
        std::string getDecompileCacheDir() const;
        bool hasDecompiledCache(const std::string& className) const;
        const std::string& getMappingFilePath() const { return mappingFilePath_; }

    private:
        void generateMappingFile();
        bool downloadJarRemapper();
        bool remapJar();
        std::string getToolsDir() const;
        std::unique_ptr<MappingResolver> resolver_;
        std::unique_ptr<ZipArchive> zip_;
        std::unique_ptr<FernflowerDecompiler> decompiler_;
        ClassHierarchy hierarchy_;
        std::string cacheDir_;
        std::string currentVersion_;
        std::string currentJarPath_;
        std::future<bool> srgFuture_;
        std::future<bool> batchDecompileFuture_;
        std::string mappingFilePath_;
        std::string jarRemapperPath_;
        std::atomic<bool> decompilingAll_{false};
        std::atomic<bool> cancelled_{false};
        std::atomic<float> decompileProgress_{0.0f};
        std::mutex decompileMutex_;
    };
} // namespace mc
