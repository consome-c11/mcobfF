#include "JreDownloader.h"
#include "mcobfF/config/ApiConfig.h"
#include "mcobfF/network/HttpsClient.h"
#include "mcobfF/Logger.h"

#include <filesystem>
#include <fstream>
#include <vector>
#include <cstdlib>

#include "miniz.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace mcobfF
{
    std::string JreDownloader::getJreCachePath()
    {
#ifdef _WIN32
        char buf[MAX_PATH] = {};
        DWORD ret = GetEnvironmentVariableA("LOCALAPPDATA", buf, MAX_PATH);
        if (ret > 0 && ret < MAX_PATH)
        {
            return (fs::path(buf) / "mcobfF" / "cache" / "tools" / "jre").string();
        }
#else
        if (const char* localAppData = std::getenv("LOCALAPPDATA"))
        {
            return (fs::path(localAppData) / "mcobfF" / "cache" / "tools" / "jre").string();
        }
#endif
        return "cache/tools/jre";
    }

    bool JreDownloader::isJavaAvailable()
    {
        return !findExistingJava().empty();
    }

    static bool hasJavaExe(const std::string& dir)
    {
        return fs::exists(dir + "/bin/java.exe") || fs::exists(dir + "/bin/java");
    }

    static bool hasJvmDll(const std::string& dir)
    {
        return fs::exists(dir + "/bin/server/jvm.dll") ||
               fs::exists(dir + "/jre/bin/server/jvm.dll");
    }

#ifdef _WIN32
    static std::string readRegistryValue(HKEY rootKey, const wchar_t* subKey, const wchar_t* valueName)
    {
        HKEY hKey = nullptr;
        if (RegOpenKeyExW(rootKey, subKey, 0, KEY_READ, &hKey) != ERROR_SUCCESS)
            return {};

        wchar_t buffer[512] = {};
        DWORD bufferSize = sizeof(buffer);
        DWORD type = 0;
        LONG result = RegQueryValueExW(hKey, valueName, nullptr, &type,
                                       reinterpret_cast<LPBYTE>(buffer), &bufferSize);
        RegCloseKey(hKey);

        if (result != ERROR_SUCCESS || type != REG_SZ)
            return {};

        std::wstring ws(buffer);
        return std::string(ws.begin(), ws.end());
    }
#endif

    std::string JreDownloader::findExistingJava()
    {
        std::string javaHome;

        if (const char* env = std::getenv("JAVA_HOME"))
        {
            javaHome = env;
            if (hasJavaExe(javaHome) && hasJvmDll(javaHome))
            {
                Logger::info("JreDownloader") << "Found JAVA_HOME: " << javaHome;
                return javaHome;
            }
        }

        std::string cachePath = getJreCachePath();
        if (hasJavaExe(cachePath) && hasJvmDll(cachePath))
        {
            Logger::info("JreDownloader") << "Found cached JRE: " << cachePath;
            return cachePath;
        }

#ifdef _WIN32
        const wchar_t* jdkKeys[] = {
            L"SOFTWARE\\JavaSoft\\JDK",
            L"SOFTWARE\\JavaSoft\\Java Development Kit",
        };
        const wchar_t* jreKeys[] = {
            L"SOFTWARE\\JavaSoft\\JRE",
            L"SOFTWARE\\JavaSoft\\Java Runtime Environment",
        };

        auto tryRegistry = [](HKEY root, const wchar_t* basePath) -> std::string
        {
            HKEY hKey = nullptr;
            if (RegOpenKeyExW(root, basePath, 0, KEY_READ | KEY_WOW64_64KEY, &hKey) != ERROR_SUCCESS)
                return {};

            wchar_t currentVersion[64] = {};
            DWORD cvSize = sizeof(currentVersion);
            DWORD type = 0;
            if (RegQueryValueExW(hKey, L"CurrentVersion", nullptr, &type,
                                 reinterpret_cast<LPBYTE>(currentVersion), &cvSize) != ERROR_SUCCESS)
            {
                RegCloseKey(hKey);
                return {};
            }
            RegCloseKey(hKey);

            std::wstring versionPath = std::wstring(basePath) + L"\\" + currentVersion;
            std::string home = readRegistryValue(root, versionPath.c_str(), L"JavaHome");
            if (!home.empty() && hasJavaExe(home) && hasJvmDll(home))
                return home;
            return {};
        };

        for (const auto* key : jdkKeys)
        {
            javaHome = tryRegistry(HKEY_LOCAL_MACHINE, key);
            if (!javaHome.empty())
            {
                Logger::info("JreDownloader") << "Found JDK in registry: " << javaHome;
                return javaHome;
            }
        }
        for (const auto* key : jreKeys)
        {
            javaHome = tryRegistry(HKEY_LOCAL_MACHINE, key);
            if (!javaHome.empty())
            {
                Logger::info("JreDownloader") << "Found JRE in registry: " << javaHome;
                return javaHome;
            }
        }

        const char* paths[] = {
            "C:\\Program Files\\Java",
            "C:\\Program Files (x86)\\Java",
        };
        for (const auto* basePath : paths)
        {
            std::error_code ec;
            if (!fs::exists(basePath, ec)) continue;
            for (const auto& entry : fs::directory_iterator(basePath, ec))
            {
                if (entry.is_directory())
                {
                    std::string candidate = entry.path().string();
                    if (hasJavaExe(candidate) && hasJvmDll(candidate))
                    {
                        Logger::info("JreDownloader") << "Found Java in Program Files: " << candidate;
                        return candidate;
                    }
                }
            }
        }
#endif

        Logger::info("JreDownloader") << "No Java installation found.";
        return {};
    }

    bool JreDownloader::downloadJre(const std::string& outputZip)
    {
        Logger::info("JreDownloader") << "Downloading JRE from Adoptium...";

        fs::create_directories(fs::path(outputZip).parent_path());

        if (!HttpsClient::downloadToFile(ApiConfig::JRE_ZIP_URL, outputZip))
        {
            Logger::error("JreDownloader") << "Failed to download JRE.";
            return false;
        }

        Logger::info("JreDownloader") << "JRE downloaded to: " << outputZip;
        return true;
    }

    bool JreDownloader::extractJre(const std::string& zipPath, const std::string& outputDir)
    {
        Logger::info("JreDownloader") << "Extracting JRE...";

        std::error_code ec;
        fs::remove_all(outputDir, ec);
        fs::create_directories(outputDir, ec);

        mz_zip_archive archive = {};
        if (!mz_zip_reader_init_file(&archive, zipPath.c_str(), 0))
        {
            Logger::error("JreDownloader") << "Failed to open JRE zip for extraction.";
            return false;
        }

        mz_uint numFiles = mz_zip_reader_get_num_files(&archive);
        std::string stripPrefix;

        for (mz_uint i = 0; i < numFiles; ++i)
        {
            mz_zip_archive_file_stat stat = {};
            if (!mz_zip_reader_file_stat(&archive, i, &stat))
                continue;

            std::string name(stat.m_filename);

            if (name.empty())
                continue;

            if (stripPrefix.empty())
            {
                size_t slashPos = name.find('/');
                if (slashPos != std::string::npos)
                {
                    stripPrefix = name.substr(0, slashPos + 1);
                }
            }

            std::string relativePath;
            if (!stripPrefix.empty() && name.find(stripPrefix) == 0)
            {
                relativePath = name.substr(stripPrefix.size());
            }
            else
            {
                relativePath = name;
            }

            if (relativePath.empty())
                continue;

            std::string fullPath = (fs::path(outputDir) / relativePath).string();

            if (stat.m_is_directory)
            {
                fs::create_directories(fullPath, ec);
                continue;
            }

            fs::create_directories(fs::path(fullPath).parent_path(), ec);

            if (!mz_zip_reader_extract_to_file(&archive, i, fullPath.c_str(), 0))
            {
                Logger::warn("JreDownloader") << "Failed to extract: " << name;
                continue;
            }
        }

        mz_zip_reader_end(&archive);

        if (!hasJavaExe(outputDir) || !hasJvmDll(outputDir))
        {
            Logger::error("JreDownloader") << "Extraction succeeded but JRE structure is invalid.";
            return false;
        }

        Logger::info("JreDownloader") << "JRE extracted to: " << outputDir;
        return true;
    }

    bool JreDownloader::ensureJre()
    {

        std::string existing = findExistingJava();
        if (!existing.empty())
        {
            Logger::info("JreDownloader") << "Java already available at: " << existing;

#ifdef _WIN32
            SetEnvironmentVariableA("JAVA_HOME", existing.c_str());
#endif
            return true;
        }


        Logger::info("JreDownloader") << "No Java found. Downloading JRE...";

        std::string cachePath = getJreCachePath();
        std::string zipPath = cachePath + ".zip";

        std::error_code ec;

        if (fs::exists(zipPath))
        {
            Logger::info("JreDownloader") << "Using existing zip: " << zipPath;
        }
        else
        {
            if (!downloadJre(zipPath))
            {
                Logger::error("JreDownloader") << "Failed to download JRE.";
                return false;
            }
        }

        bool alreadyExtracted = (fs::exists(cachePath) && hasJavaExe(cachePath) && hasJvmDll(cachePath));
        if (!alreadyExtracted)
        {
            if (!extractJre(zipPath, cachePath))
            {
                Logger::error("JreDownloader") << "Failed to extract JRE.";
                return false;
            }
            fs::remove(zipPath, ec);
        }
        else
        {
            Logger::info("JreDownloader") << "JRE already extracted, using cached: " << cachePath;
        }

#ifdef _WIN32
        SetEnvironmentVariableA("JAVA_HOME", cachePath.c_str());
#endif
        Logger::info("JreDownloader") << "JAVA_HOME set to: " << cachePath;
        return true;
    }
}
