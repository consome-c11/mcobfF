#include "api.h"
#include <filesystem>
#include "mcobfF/Logger.h"
#include "mcobfF/class/ClassHierarchyBuilder.h"
#include "mcobfF/dumper/JarDumper.h"
#include "mcobfF/mapping/InheritanceResolver.h"
#include "mcobfF/mapping/MappingResolver.h"
#include "mcobfF/mapping/SRGResolver.h"
#include "mcobfF/network/VersionDownloader.h"
#include "mcobfF/zip/ZipArchive.h"

namespace fs = std::filesystem;

namespace mcobfF
{
    api::api()
        : cacheDir_("")
          , resolver_(std::make_unique<MappingResolver>(""))
    {
        JarDumper::setCacheDir(cacheDir_);
    }

    api::api(const std::string& cacheDir)
        : resolver_(std::make_unique<MappingResolver>(cacheDir))
          , cacheDir_(cacheDir)
    {
        JarDumper::setCacheDir(cacheDir_);
    }

    api::~api() = default;

    std::optional<std::string> api::getLatestReleaseVersion()
    {
        return VersionDownloader::getLatestRelease();
    }

    std::optional<std::string> api::getLatestSnapshotVersion()
    {
        return VersionDownloader::getLatestSnapshot();
    }

    bool api::loadMappings(const std::string& version)
    {
        if (!resolver_->initialize())
        {
            Logger::error("CoreApi") << "Failed to initialize MappingResolver";
            return false;
        }

        if (!resolver_->loadMappings(version))
        {
            Logger::error("CoreApi") << "Failed to load mappings for version: " << version;
            return false;
        }

        std::string srgCacheDir = resolver_->getCachePath(version);
        if (!SRGResolver::downloadAndLoad(version, srgCacheDir, resolver_->getData()))
        {
            Logger::info("CoreApi") << "No SRG mappings for version: " << version;
        }

        currentVersion_ = version;
        return true;
    }

    bool api::loadMappingsWithInheritance(const std::string& version, const std::string& jarPath)
    {
        if (!loadMappings(version)) return false;

        ZipArchive zip;
        if (!zip.open(jarPath))
        {
            Logger::error("CoreApi") << "Failed to open JAR: " << jarPath;
            return false;
        }

        const ClassHierarchy hierarchy = ClassHierarchyBuilder::buildFromJar(zip);
        InheritanceResolver ihResolver(resolver_->getData(), hierarchy);
        ihResolver.resolveAll();

        return true;
    }

    std::optional<std::string> api::resolveClass(const std::string& className, bool deobfToObf) const
    {
        if (deobfToObf)
        {
            return resolver_->resolveToObfuscated(className);
        }
        return resolver_->resolveToDeobfuscated(className);
    }

    std::optional<std::string> api::resolveMethod(const std::string& className,
                                                      const std::string& methodName,
                                                      const std::vector<std::string>& paramTypes,
                                                      bool deobfToObf) const
    {
        if (deobfToObf)
        {
            return resolver_->resolveMethodToObfuscated(className, methodName, paramTypes);
        }
        return resolver_->resolveMethodToDeobfuscated(className, methodName, paramTypes);
    }

    std::optional<std::string> api::resolveField(const std::string& className,
                                                     const std::string& fieldName,
                                                     bool deobfToObf) const
    {
        if (deobfToObf)
        {
            return resolver_->resolveFieldToObfuscated(className, fieldName);
        }
        return resolver_->resolveFieldToDeobfuscated(className, fieldName);
    }

    bool api::loadJar(const std::string& jarPath)
    {
        zip_ = std::make_unique<ZipArchive>();
        if (!zip_->open(jarPath))
        {
            Logger::error("CoreApi") << "Failed to open JAR: " << jarPath;
            zip_.reset();
            return false;
        }
        return true;
    }

    ClassHierarchy api::buildClassHierarchy()
    {
        if (!zip_)
        {
            Logger::error("CoreApi") << "No JAR loaded. Call loadJar() first.";
            return {};
        }
        hierarchy_ = ClassHierarchyBuilder::buildFromJar(*zip_);
        return hierarchy_;
    }

    bool api::dumpMappings(const std::string& version, const std::string& outputPath)
    {
        return JarDumper::dumpFromVersion(version, outputPath);
    }

    bool api::dumpJarMappings(const std::string& jarPath, const std::string& version,
                                  const std::string& outputPath)
    {
        return JarDumper::dump(jarPath, version, outputPath);
    }

    const MappingData& api::getMappingData() const
    {
        return resolver_->getData();
    }

    bool api::isMappingLoaded() const
    {
        return resolver_->isLoaded();
    }

    const std::string& api::getCurrentVersion() const
    {
        return currentVersion_;
    }
} // namespace mc
