#include "api.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#ifdef _WIN32
#include <windows.h>
#endif
#include "mcobfF/config/ApiConfig.h"
#include "mcobfF/Logger.h"
#include "mcobfF/class/ClassHierarchyBuilder.h"
#include "mcobfF/decompiler/FernflowerDecompiler.h"
#include "mcobfF/dumper/JarDumper.h"
#include "mcobfF/mapping/InheritanceResolver.h"
#include "mcobfF/mapping/MappingResolver.h"
#include "mcobfF/mapping/MappingWriter.h"
#include "mcobfF/mapping/SRGResolver.h"
#include "mcobfF/network/HttpsClient.h"
#include "mcobfF/network/VersionDownloader.h"
#include "mcobfF/zip/ZipArchive.h"

namespace fs = std::filesystem;

namespace mcobfF
{
    api::api()
        : resolver_(std::make_unique<MappingResolver>(""))
          , cacheDir_("")
    {
        JarDumper::setCacheDir(cacheDir_);
    }

    api::api(const std::string& cacheDir)
        : resolver_(std::make_unique<MappingResolver>(cacheDir))
          , cacheDir_(cacheDir)
    {
        JarDumper::setCacheDir(cacheDir_);
    }

    api::~api()
    {
        cancelled_ = true;
        if (srgFuture_.valid())
        {
            srgFuture_.wait();
        }
        if (batchDecompileFuture_.valid())
        {
            batchDecompileFuture_.wait();
        }
    }

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

        if (srgFuture_.valid())
        {
            srgFuture_.wait();
        }

        if (!resolver_->isPreMojmap())
        {
            std::string srgCacheDir = resolver_->getCachePath(version);
            srgFuture_ = std::async(std::launch::async, [this, version, srgCacheDir]()
            {
                return SRGResolver::downloadAndLoad(version, srgCacheDir, resolver_->getData());
            });
        }

        currentVersion_ = version;
        generateMappingFile();

        return true;
    }

    bool api::loadMappingsWithInheritance(const std::string& version, const std::string& jarPath)
    {
        if (!loadMappings(version)) return false;

        currentJarPath_ = jarPath;

        ZipArchive zip;
        if (!zip.open(jarPath))
        {
            Logger::error("CoreApi") << "Failed to open JAR: " << jarPath;
            return false;
        }

        const ClassHierarchy hierarchy = ClassHierarchyBuilder::buildFromJar(zip);

        if (srgFuture_.valid())
        {
            if (!srgFuture_.get())
            {
                Logger::info("CoreApi") << "No SRG mappings for version: " << version;
            }
        }

        InheritanceResolver ihResolver(resolver_->getData(), hierarchy);
        ihResolver.resolveAll();

        if (initializeDecompiler())
        {
            if (!remapJar())
            {
                Logger::warn("CoreApi") << "Failed to remap JAR for version: " << version
                    << ". Decompilation will use obfuscated JAR.";
            }
        }
        else
        {
            Logger::warn("CoreApi") << "Failed to initialize decompiler. JAR remap skipped.";
        }

        startDecompileAllAsync();

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
        currentJarPath_ = jarPath;
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

    std::string api::getDecompileCacheDir() const
    {
        if (cacheDir_.empty() || currentVersion_.empty())
        {
            return {};
        }
        std::string dir = (fs::path(cacheDir_) / currentVersion_ / "decompiled").string();
        std::error_code ec;
        fs::create_directories(dir, ec);
        return dir;
    }

    bool api::hasDecompiledCache(const std::string& className) const
    {
        if (cacheDir_.empty() || currentVersion_.empty()) return false;
        std::string cachePath = FernflowerDecompiler::getCachePath(
            (fs::path(cacheDir_) / currentVersion_ / "decompiled").string(), className);
        return fs::exists(cachePath);
    }

    std::string api::getToolsDir() const
    {
#ifdef _WIN32
        char buf[MAX_PATH] = {};
        DWORD ret = GetEnvironmentVariableA("LOCALAPPDATA", buf, MAX_PATH);
        if (ret > 0 && ret < MAX_PATH)
        {
            return (fs::path(buf) / "mcobfF" / "cache" / "tools").string();
        }
        return "cache/tools";
#else
        if (const char* localAppData = std::getenv("LOCALAPPDATA"))
        {
            return (fs::path(localAppData) / "mcobfF" / "cache" / "tools").string();
        }
        return "cache/tools";
#endif
    }

    bool api::downloadMinecraftRemapper()
    {
        minecraftRemapperPath_ = (fs::path(getToolsDir()) / "MinecraftRemapper-1.1.jar").string();

        if (fs::exists(minecraftRemapperPath_))
        {
            Logger::info("CoreApi") << "MinecraftRemapper already exists: " << minecraftRemapperPath_;
            return true;
        }

        Logger::info("CoreApi") << "Downloading MinecraftRemapper...";
        fs::create_directories(fs::path(minecraftRemapperPath_).parent_path());

        if (!HttpsClient::downloadToFile(ApiConfig::MINECRAFT_REMAPPER_JAR_URL, minecraftRemapperPath_))
        {
            Logger::error("CoreApi") << "Failed to download MinecraftRemapper.";
            minecraftRemapperPath_.clear();
            return false;
        }

        Logger::info("CoreApi") << "MinecraftRemapper downloaded: " << minecraftRemapperPath_;
        return true;
    }

    bool api::remapJar()
    {
        if (currentVersion_.empty())
        {
            Logger::warn("CoreApi") << "Cannot remap: no version set.";
            return false;
        }

        std::string versionDir = resolver_->getCachePath(currentVersion_);

        // MinecraftRemapper output: root = <outputDir>/<version>client/
        // remapped jar = root/remapped-<version>.jar
        std::string rootDir = (fs::path(versionDir) / (currentVersion_ + "client")).string();
        std::string remappedJarPath = (fs::path(rootDir) / ("remapped-" + currentVersion_ + ".jar")).string();

        if (fs::exists(remappedJarPath))
        {
            Logger::info("CoreApi") << "Named JAR already exists: " << remappedJarPath;
            currentJarPath_ = remappedJarPath;
            return true;
        }

        if (!decompiler_ || !decompiler_->isInitialized())
        {
            Logger::error("CoreApi") << "Decompiler JVM not available for MinecraftRemapper.";
            return false;
        }

        if (!downloadMinecraftRemapper())
        {
            return false;
        }

        Logger::info("CoreApi") << "Running MinecraftRemapper via JNI...";

        std::vector<std::string> jvmArgs = {
            "-v", currentVersion_,
            "-t", "client",
            "-o", versionDir
        };

        if (!decompiler_->runMainMethod(minecraftRemapperPath_, "be.yvanmazy.minecraftremapper.Main", jvmArgs))
        {
            Logger::error("CoreApi") << "MinecraftRemapper failed.";
            return false;
        }

        if (!fs::exists(remappedJarPath))
        {
            Logger::error("CoreApi") << "MinecraftRemapper did not produce output JAR at: " << remappedJarPath;
            return false;
        }

        Logger::info("CoreApi") << "JAR remapped: " << remappedJarPath;

        currentJarPath_ = remappedJarPath;
        return true;
    }

    void api::generateMappingFile()
    {
        if (!resolver_ || !resolver_->isLoaded()) return;

        std::string cacheDir = resolver_->getCachePath(currentVersion_);
        mappingFilePath_ = (fs::path(cacheDir) / "mappings.tiny").string();

        if (!MappingWriter::writeToFile(resolver_->getData(), mappingFilePath_))
        {
            Logger::warn("CoreApi") << "Failed to write mapping file: " << mappingFilePath_;
            mappingFilePath_.clear();
        }
        else
        {
            Logger::info("CoreApi") << "Mapping file written: " << mappingFilePath_;
        }
    }

    bool api::initializeDecompiler()
    {
        if (decompiler_ && decompiler_->isInitialized())
        {
            return true;
        }

        decompiler_ = std::make_unique<FernflowerDecompiler>();
        std::string fernflowerPath = FernflowerDecompiler::getDefaultFernflowerPath();

        if (!decompiler_->initialize(fernflowerPath))
        {
            Logger::error("CoreApi") << "Failed to initialize decompiler.";
            decompiler_.reset();
            return false;
        }

        Logger::info("CoreApi") << "Decompiler initialized.";
        return true;
    }

    std::optional<std::string> api::decompileClass(const std::string& className)
    {
        if (currentJarPath_.empty())
        {
            Logger::error("CoreApi") << "No JAR path available for decompilation.";
            return std::nullopt;
        }

        std::string resolvedClassName = className;
        auto deobf = resolver_->resolveToDeobfuscated(className);
        if (deobf) resolvedClassName = *deobf;

        std::string cacheDir = getDecompileCacheDir();
        if (!cacheDir.empty())
        {
            std::string cachePath = FernflowerDecompiler::getCachePath(cacheDir, resolvedClassName);
            if (fs::exists(cachePath))
            {
                std::ifstream ifs(cachePath, std::ios::binary);
                if (ifs)
                {
                    std::ostringstream oss;
                    oss << ifs.rdbuf();
                    std::string content = oss.str();
                    if (!content.empty())
                    {
                        return content;
                    }
                }
            }
        }

        std::lock_guard<std::mutex> lock(decompileMutex_);

        if (!decompiler_ || !decompiler_->isInitialized())
        {
            if (!initializeDecompiler())
            {
                return std::nullopt;
            }
        }

        return decompiler_->decompileClass(currentJarPath_, resolvedClassName, cacheDir);
    }

    std::optional<std::string> api::decompileAndRemapClass(const std::string& className)
    {
        return decompileClass(className);
    }

    bool api::isDecompilerAvailable() const
    {
        return decompiler_ && decompiler_->isInitialized();
    }

    void api::startDecompileAllAsync()
    {
        if (decompilingAll_)
        {
            return;
        }

        if (currentJarPath_.empty() || currentVersion_.empty())
        {
            return;
        }

        std::string cacheDir = getDecompileCacheDir();
        if (cacheDir.empty())
        {
            return;
        }

        std::string jarPath = currentJarPath_;

        decompilingAll_ = true;
        decompileProgress_ = 0.0f;

        batchDecompileFuture_ = std::async(std::launch::async,
                                           [this, jarPath, cacheDir]() -> bool
                                           {
                                               if (!initializeDecompiler())
                                               {
                                                   decompilingAll_ = false;
                                                   return false;
                                               }

                                               std::vector<std::string> classNames;
                                               ZipArchive zip;
                                               if (!zip.open(jarPath))
                                               {
                                                   decompilingAll_ = false;
                                                   return false;
                                               }

                                               auto entries = zip.listEntries();
                                               for (const auto& entry : entries)
                                               {
                                                   if (entry.size() > 6 && entry.substr(entry.size() - 6) == ".class")
                                                   {
                                                       std::string cn = entry.substr(0, entry.size() - 6);
                                                       std::string cp =
                                                           FernflowerDecompiler::getCachePath(cacheDir, cn);
                                                       if (!fs::exists(cp))
                                                       {
                                                           classNames.push_back(cn);
                                                       }
                                                   }
                                               }

                                               if (classNames.empty())
                                               {
                                                   Logger::info("CoreApi") << "All classes already decompiled.";
                                                   decompilingAll_ = false;
                                                   decompileProgress_ = 1.0f;
                                                   return true;
                                               }

                                               Logger::info("CoreApi") << "Starting batch decompile of "
                                                   << classNames.size() << " classes...";

                                               size_t total = classNames.size();
                                               size_t done = 0;

                                               for (const auto& className : classNames)
                                               {
                                                   if (cancelled_)
                                                   {
                                                       Logger::info("CoreApi") << "Batch decompile cancelled.";
                                                       decompilingAll_ = false;
                                                       return false;
                                                   }

                                                   {
                                                       std::lock_guard<std::mutex> lock(decompileMutex_);
                                                       auto _ = decompiler_->decompileClass(
                                                           jarPath, className, cacheDir);
                                                   }
                                                   done++;
                                                   decompileProgress_ = static_cast<float>(done) / static_cast<float>(
                                                       total);
                                               }

                                               decompilingAll_ = false;
                                               decompileProgress_ = 1.0f;
                                               Logger::info("CoreApi") << "Batch decompile complete.";
                                               return true;
                                           });
    }

    void api::cancelDecompileAll()
    {
        cancelled_ = true;
    }

    bool api::isDecompilingAll() const
    {
        return decompilingAll_;
    }

    float api::getDecompileProgress() const
    {
        return decompileProgress_;
    }
} // namespace mc
