#include "FernflowerDecompiler.h"
#include "mcobfF/Logger.h"
#include "mcobfF/config/ApiConfig.h"
#include "mcobfF/network/HttpsClient.h"
#include "mcobfF/file/FileSystem.h"
#include "mcobfF/zip/ZipArchive.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <functional>
#include <algorithm>

#ifdef _WIN32
#include <windows.h>
#endif

namespace fs = std::filesystem;

namespace mcobfF
{
    namespace
    {
        std::optional<std::string> findDecompiledFile(const std::string& outputDir,
                                                      const std::string& className)
        {
            std::string javaFile = className + ".java";
            std::error_code ec;

            std::function<std::optional<std::string>(const fs::path&)> searchDir;
            searchDir = [&](const fs::path& dir) -> std::optional<std::string>
            {
                for (const auto& entry : fs::recursive_directory_iterator(dir, ec))
                {
                    if (entry.is_regular_file())
                    {
                        std::string relPath = fs::relative(entry.path(), dir, ec).string();
                        std::ranges::replace(relPath, '\\', '/');

                        bool matchFull = (relPath == javaFile);
                        bool matchName = (entry.path().filename().string() == fs::path(javaFile).filename().string());

                        Logger::info("Fernflower") << "  compare: relPath='" << relPath << "' javaFile='" << javaFile
                            << "' matchFull=" << matchFull << " matchName=" << matchName;

                        if (matchFull || matchName)
                        {
                            auto content = FileSystem::readFile(entry.path().string());
                            if (content) return *content;
                            Logger::warn("Fernflower") << "  found file but read failed: " << entry.path().string();
                        }
                    }
                }
                return std::nullopt;
            };

            Logger::info("Fernflower") << "findDecompiledFile: looking for '" << javaFile << "' in '" << outputDir <<
                "'";
            Logger::info("Fernflower") << "findDecompiledFile: outputDir exists=" << fs::exists(outputDir);

            if (fs::exists(outputDir))
            {
                for (const auto& entry : fs::recursive_directory_iterator(outputDir, ec))
                {
                    Logger::info("Fernflower") << "  entry: " << entry.path().string();
                }
            }

            return searchDir(outputDir);
        }

#ifdef _WIN32
        std::string readRegistryValue(HKEY rootKey, const wchar_t* subKey, const wchar_t* valueName)
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
    }

    FernflowerDecompiler::FernflowerDecompiler() = default;

    FernflowerDecompiler::~FernflowerDecompiler()
    {
        shutdown();
    }

    void FernflowerDecompiler::shutdown()
    {
        shuttingDown_ = true;
        if (jvm_ && ownsJvm_)
        {
            jvm_->DestroyJavaVM();
            jvm_ = nullptr;
            env_ = nullptr;
        }
#ifdef _WIN32
        if (jvmDll_)
        {
            FreeLibrary(jvmDll_);
            jvmDll_ = nullptr;
        }
#else
        if (jvmDll_)
        {
            dlclose(jvmDll_);
            jvmDll_ = nullptr;
        }
#endif
    }

    std::string FernflowerDecompiler::findJavaHome()
    {
        std::string javaHome;

        if (const char* env = std::getenv("JAVA_HOME"))
        {
            javaHome = env;
            if (fs::exists(javaHome + "/bin/java.exe") || fs::exists(javaHome + "/bin/java"))
            {
                Logger::info("Fernflower") << "JAVA_HOME from env: " << javaHome;
                return javaHome;
            }
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
            if (!home.empty() && (fs::exists(home + "/bin/java.exe") || fs::exists(home + "/bin/java")))
                return home;
            return {};
        };

        for (const auto* key : jdkKeys)
        {
            javaHome = tryRegistry(HKEY_LOCAL_MACHINE, key);
            if (!javaHome.empty())
            {
                Logger::info("Fernflower") << "JAVA_HOME from registry (JDK): " << javaHome;
                return javaHome;
            }
        }
        for (const auto* key : jreKeys)
        {
            javaHome = tryRegistry(HKEY_LOCAL_MACHINE, key);
            if (!javaHome.empty())
            {
                Logger::info("Fernflower") << "JAVA_HOME from registry (JRE): " << javaHome;
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
                    if (fs::exists(candidate + "/bin/java.exe"))
                    {
                        if (fs::exists(candidate + "/bin/server/jvm.dll") ||
                            fs::exists(candidate + "/jre/bin/server/jvm.dll") ||
                            fs::exists(candidate + "/lib/jvm.cfg"))
                        {
                            Logger::info("Fernflower") << "JAVA_HOME auto-detected: " << candidate;
                            return candidate;
                        }
                    }
                }
            }
        }
#endif

        Logger::info("Fernflower") << "Could not auto-detect JAVA_HOME.";
        return {};
    }

    bool FernflowerDecompiler::loadJvmLibrary(const std::string& javaHome)
    {
        if (jvmDll_) return true;

        std::vector<std::string> candidates;
#ifdef _WIN32
        candidates.push_back(javaHome + "/bin/server/jvm.dll");
        candidates.push_back(javaHome + "/jre/bin/server/jvm.dll");
        candidates.push_back(javaHome + "/bin/hotspot/jvm.dll");
#else
        candidates.push_back(javaHome + "/lib/server/libjvm.so");
        candidates.push_back(javaHome + "/jre/lib/amd64/server/libjvm.so");
        candidates.push_back(javaHome + "/lib/libjvm.so");
#endif

        for (const auto& path : candidates)
        {
            if (!fs::exists(path))
                continue;

            Logger::info("Fernflower") << "Loading JVM library: " << path;

#ifdef _WIN32
            std::string binDir = javaHome + "/bin";
            std::string jreBinDir = javaHome + "/jre/bin";
            std::string serverDir = fs::path(path).parent_path().string();

            std::string prependPaths = serverDir + ";" + binDir;
            if (fs::exists(jreBinDir))
            {
                prependPaths += ";" + jreBinDir;
            }

            std::string origPath;
            if (const char* envPath = std::getenv("PATH"))
            {
                origPath = envPath;
            }
            std::string newPath = prependPaths + ";" + origPath;
            SetEnvironmentVariableA("PATH", newPath.c_str());

            jvmDll_ = LoadLibraryA(path.c_str());
            DWORD lastErr = GetLastError();

            if (!origPath.empty())
                SetEnvironmentVariableA("PATH", origPath.c_str());
            else
                SetEnvironmentVariableA("PATH", nullptr);

            if (!jvmDll_)
            {
                Logger::error("Fernflower") << "LoadLibraryA failed for " << path
                    << " (error=" << lastErr << ")";
                continue;
            }

            fnCreateJavaVM_ = reinterpret_cast<CreateJavaVM_t>(
                GetProcAddress(jvmDll_, "JNI_CreateJavaVM"));
            fnGetCreatedJavaVMs_ = reinterpret_cast<GetCreatedJavaVMs_t>(
                GetProcAddress(jvmDll_, "JNI_GetCreatedJavaVMs"));
#else
            jvmDll_ = dlopen(path.c_str(), RTLD_NOW | RTLD_GLOBAL);
            if (!jvmDll_)
            {
                Logger::error("Fernflower") << "dlopen failed: " << dlerror();
                continue;
            }
            fnCreateJavaVM_ = reinterpret_cast<CreateJavaVM_t>(dlsym(jvmDll_, "JNI_CreateJavaVM"));
            fnGetCreatedJavaVMs_ = reinterpret_cast<GetCreatedJavaVMs_t>(dlsym(jvmDll_, "JNI_GetCreatedJavaVMs"));
#endif

            if (fnCreateJavaVM_ && fnGetCreatedJavaVMs_)
            {
                Logger::info("Fernflower") << "JVM library loaded successfully.";
                resolvedJavaHome_ = javaHome;
                return true;
            }

            Logger::error("Fernflower") << "JNI_CreateJavaVM symbol not found in " << path;
#ifdef _WIN32
            FreeLibrary(jvmDll_);
#else
            dlclose(jvmDll_);
#endif
            jvmDll_ = nullptr;
        }

        Logger::error("Fernflower") << "Failed to find/load JVM library.";
        Logger::error("Fernflower") << "JAVA_HOME: " << javaHome;
        Logger::error("Fernflower") << "Ensure a valid JDK/JRE is installed.";
        return false;
    }

    std::string FernflowerDecompiler::getDefaultFernflowerPath()
    {
        std::string cacheDir;
        if (const char* localAppData = std::getenv("LOCALAPPDATA"))
        {
            cacheDir = std::string(localAppData) + "\\mcobfF\\cache\\tools";
        }
        else
        {
            cacheDir = "cache/tools";
        }
        fs::create_directories(cacheDir);
        return (fs::path(cacheDir) / "vineflower.jar").string();
    }

    bool FernflowerDecompiler::downloadFernflower(const std::string& outputPath)
    {
        Logger::info("Fernflower") << "Downloading CFR decompiler...";

        const std::string url = ApiConfig::DECOMPILER_JAR_URL;

        fs::create_directories(fs::path(outputPath).parent_path().string());

        if (!HttpsClient::downloadToFile(url, outputPath))
        {
            Logger::error("Fernflower") << "Failed to download decompiler JAR.";
            return false;
        }

        Logger::info("Fernflower") << "Decompiler JAR downloaded to: " << outputPath;
        return true;
    }

    bool FernflowerDecompiler::createJvm(const std::string& javaHome)
    {
        if (!loadJvmLibrary(javaHome))
        {
            return false;
        }

        JavaVM* existingVm = nullptr;
        jsize numVms = 0;
        if (fnGetCreatedJavaVMs_(&existingVm, 1, &numVms) == JNI_OK && numVms > 0)
        {
            jvm_ = existingVm;
            ownsJvm_ = false;
            jint rc = jvm_->GetEnv(reinterpret_cast<void**>(&env_), JNI_VERSION_1_8);
            if (rc == JNI_EDETACHED)
            {
                rc = jvm_->AttachCurrentThread(reinterpret_cast<void**>(&env_), nullptr);
            }
            if (rc != JNI_OK)
            {
                Logger::error("Fernflower") << "Failed to attach to existing JVM.";
                jvm_ = nullptr;
                return false;
            }
            Logger::info("Fernflower") << "Attached to existing JVM.";
            return true;
        }

        std::string classpath;
        if (!fernflowerJarPath_.empty())
        {
#ifdef _WIN32
            classpath = "-Djava.class.path=" + fernflowerJarPath_;
#else
            classpath = "-Djava.class.path=" + fernflowerJarPath_;
#endif
        }

        std::string javaHomeOption = "-Djava.home=" + resolvedJavaHome_;

        std::vector<std::string> optionStrings;
        if (!classpath.empty())
            optionStrings.push_back(classpath);
        optionStrings.push_back(javaHomeOption);
        optionStrings.push_back("-Xms128m");
        optionStrings.push_back("-Xmx2048m");

        Logger::info("Fernflower") << "JVM options:";
        for (const auto& opt : optionStrings)
            Logger::info("Fernflower") << "  " << opt;

        std::vector<JavaVMOption> jvmOptions(optionStrings.size());
        for (size_t i = 0; i < optionStrings.size(); ++i)
        {
            jvmOptions[i].optionString = const_cast<char*>(optionStrings[i].c_str());
            jvmOptions[i].extraInfo = nullptr;
        }

        JavaVMInitArgs vmArgs = {};
        vmArgs.version = JNI_VERSION_1_8;
        vmArgs.nOptions = static_cast<jint>(jvmOptions.size());
        vmArgs.options = jvmOptions.data();
        vmArgs.ignoreUnrecognized = JNI_TRUE;

        Logger::info("Fernflower") << "Calling JNI_CreateJavaVM...";

        jint rc = fnCreateJavaVM_(&jvm_, reinterpret_cast<void**>(&env_), &vmArgs);
        if (rc != JNI_OK)
        {
            Logger::error("Fernflower") << "JNI_CreateJavaVM failed. Error code: " << rc;
            Logger::error("Fernflower") << "  -4 = JNI_ENOMEM (not enough memory)";
            Logger::error("Fernflower") << "  -5 = JNI_EEXIST (VM already created)";
            Logger::error("Fernflower") << "  -6 = JNI_EINVAL (invalid arguments)";
            Logger::error("Fernflower") << "JAVA_HOME: " << resolvedJavaHome_;
            Logger::error("Fernflower") << "classpath: " << classpath;
            jvm_ = nullptr;
            env_ = nullptr;
            return false;
        }

        ownsJvm_ = true;
        Logger::info("Fernflower") << "JVM created successfully (rc=" << rc << ").";
        return true;
    }

    bool FernflowerDecompiler::initialize(const std::string& fernflowerJarPath, const std::string& javaHome)
    {
        fernflowerJarPath_ = fernflowerJarPath;

        if (!fs::exists(fernflowerJarPath))
        {
            Logger::info("Fernflower") << "Decompiler JAR not found at: " << fernflowerJarPath;
            if (!downloadFernflower(fernflowerJarPath))
            {
                return false;
            }
        }

        std::string resolvedHome = javaHome.empty() ? findJavaHome() : javaHome;
        if (resolvedHome.empty())
        {
            Logger::error("Fernflower") << "JAVA_HOME is not set and could not be auto-detected.";
            Logger::error("Fernflower") << "Please set JAVA_HOME environment variable to a valid JDK/JRE path.";
            return false;
        }

        return createJvm(resolvedHome);
    }

    std::optional<std::string> FernflowerDecompiler::decompileViaConsole(const std::string& jarPath,
                                                                         const std::string& outputDir)
    {
        if (!env_) return std::nullopt;

        jclass consoleDecompilerClass = env_->FindClass(
            "org/jetbrains/java/decompiler/main/decompiler/ConsoleDecompiler");

        bool isFernflower = true;
        if (!consoleDecompilerClass)
        {
            env_->ExceptionClear();
            consoleDecompilerClass = env_->FindClass("org/benf/cfr/reader/Main");
            if (!consoleDecompilerClass)
            {
                env_->ExceptionClear();
                Logger::error("Fernflower") << "Failed to find ConsoleDecompiler or CFR Main class. "
                    << "Check the decompiler JAR classpath.";
                return std::nullopt;
            }
            isFernflower = false;
        }

        jmethodID mainMethod = env_->GetStaticMethodID(consoleDecompilerClass, "main",
                                                       "([Ljava/lang/String;)V");
        if (!mainMethod)
        {
            Logger::error("Fernflower") << "Failed to find main method.";
            return std::nullopt;
        }

        jclass stringClass = env_->FindClass("java/lang/String");
        jobjectArray args;

        if (isFernflower)
        {
            args = env_->NewObjectArray(2, stringClass, nullptr);
            env_->SetObjectArrayElement(args, 0, env_->NewStringUTF(jarPath.c_str()));
            env_->SetObjectArrayElement(args, 1, env_->NewStringUTF(outputDir.c_str()));
        }
        else
        {
            args = env_->NewObjectArray(3, stringClass, nullptr);
            env_->SetObjectArrayElement(args, 0, env_->NewStringUTF(jarPath.c_str()));
            env_->SetObjectArrayElement(args, 1, env_->NewStringUTF("--outputdir"));
            env_->SetObjectArrayElement(args, 2, env_->NewStringUTF(outputDir.c_str()));
        }

        env_->CallStaticVoidMethod(consoleDecompilerClass, mainMethod, args);

        if (env_->ExceptionCheck())
        {
            env_->ExceptionDescribe();
            env_->ExceptionClear();
            Logger::error("Fernflower") << "Decompilation threw an exception.";
            return std::nullopt;
        }

        return std::optional<std::string>(outputDir);
    }

    std::optional<std::string> FernflowerDecompiler::decompileSingleClass(const std::string& jarPath,
                                                                          const std::string& className,
                                                                          const std::string& outputDir)
    {
        if (!env_) return std::nullopt;

        std::string classFile = className + ".class";
        std::string tempClassDir;
        if (const char* localAppData = std::getenv("LOCALAPPDATA"))
        {
            tempClassDir = std::string(localAppData) + "\\mcobfF\\cache\\temp_classes";
        }
        else
        {
            tempClassDir = "cache/temp_classes";
        }

        std::error_code ec;
        fs::remove_all(tempClassDir, ec);
        fs::create_directories(tempClassDir);

        ZipArchive zip;
        if (!zip.open(jarPath))
        {
            Logger::error("Fernflower") << "Failed to open JAR: " << jarPath;
            return std::nullopt;
        }

        std::string classContent;
        if (!zip.readFile(classFile, classContent))
        {
            Logger::info("Fernflower") << "Class file not found in JAR: " << classFile;
            return std::nullopt;
        }

        std::string tempClassPath = (fs::path(tempClassDir) / fs::path(classFile).filename()).string();
        std::ofstream ofs(tempClassPath, std::ios::binary);
        if (!ofs)
        {
            Logger::error("Fernflower") << "Failed to write temp class file: " << tempClassPath;
            return std::nullopt;
        }
        ofs.write(classContent.data(), static_cast<std::streamsize>(classContent.size()));
        ofs.close();

        jclass consoleDecompilerClass = env_->FindClass(
            "org/jetbrains/java/decompiler/main/decompiler/ConsoleDecompiler");

        bool isFernflower = true;
        if (!consoleDecompilerClass)
        {
            env_->ExceptionClear();
            consoleDecompilerClass = env_->FindClass("org/benf/cfr/reader/Main");
            if (!consoleDecompilerClass)
            {
                env_->ExceptionClear();
                Logger::error("Fernflower") << "Failed to find decompiler class.";
                return std::nullopt;
            }
            isFernflower = false;
        }

        jmethodID mainMethod = env_->GetStaticMethodID(consoleDecompilerClass, "main",
                                                       "([Ljava/lang/String;)V");
        if (!mainMethod)
        {
            Logger::error("Fernflower") << "Failed to find main method.";
            return std::nullopt;
        }

        Logger::info("Fernflower") << "Temp class file: " << tempClassPath;
        Logger::info("Fernflower") << "Temp class file exists: " << fs::exists(tempClassPath);

        jclass stringClass = env_->FindClass("java/lang/String");
        jobjectArray args;

        fs::create_directories(outputDir);

        if (isFernflower)
        {
            args = env_->NewObjectArray(2, stringClass, nullptr);
            env_->SetObjectArrayElement(args, 0, env_->NewStringUTF(tempClassPath.c_str()));
            env_->SetObjectArrayElement(args, 1, env_->NewStringUTF(outputDir.c_str()));
            Logger::info("Fernflower") << "Vineflower args: [" << tempClassPath << ", " << outputDir << "]";
        }
        else
        {
            args = env_->NewObjectArray(3, stringClass, nullptr);
            env_->SetObjectArrayElement(args, 0, env_->NewStringUTF(tempClassPath.c_str()));
            env_->SetObjectArrayElement(args, 1, env_->NewStringUTF("--outputdir"));
            env_->SetObjectArrayElement(args, 2, env_->NewStringUTF(outputDir.c_str()));
            Logger::info("Fernflower") << "CFR args: [" << tempClassPath << ", --outputdir, " << outputDir << "]";
        }

        env_->CallStaticVoidMethod(consoleDecompilerClass, mainMethod, args);

        if (env_->ExceptionCheck())
        {
            Logger::error("Fernflower") << "Decompilation threw an exception:";
            env_->ExceptionDescribe();
            env_->ExceptionClear();
            Logger::error("Fernflower") << "Exception caught and cleared.";
            fs::remove_all(tempClassDir, ec);
            return std::nullopt;
        }

        fs::remove_all(tempClassDir, ec);

        return findDecompiledFile(outputDir, className);
    }

    std::string FernflowerDecompiler::getCachePath(const std::string& cacheDir,
                                                   const std::string& className)
    {
        std::string safeClassName = className;
        std::ranges::replace(safeClassName, '/', '_');
        std::ranges::replace(safeClassName, '.', '_');
        return (fs::path(cacheDir) / (safeClassName + ".java")).string();
    }

    std::optional<std::string> FernflowerDecompiler::decompileClass(const std::string& jarPath,
                                                                    const std::string& className,
                                                                    const std::string& cacheDir)
    {
        if (shuttingDown_) return std::nullopt;

        if (!jvm_)
        {
            Logger::error("Fernflower") << "JVM not initialized.";
            return std::nullopt;
        }

        std::string cachePath = getCachePath(cacheDir, className);
        if (fs::exists(cachePath))
        {
            auto cached = FileSystem::readFile(cachePath);
            if (cached && !cached->empty())
            {
                Logger::info("Fernflower") << "Cache hit: " << cachePath;
                return cached;
            }
        }

        JNIEnv* threadEnv = nullptr;
        jint rc = jvm_->GetEnv(reinterpret_cast<void**>(&threadEnv), JNI_VERSION_1_8);
        bool attached = false;
        if (rc == JNI_EDETACHED)
        {
            rc = jvm_->AttachCurrentThread(reinterpret_cast<void**>(&threadEnv), nullptr);
            if (rc != JNI_OK)
            {
                Logger::error("Fernflower") << "Failed to attach thread to JVM.";
                return std::nullopt;
            }
            attached = true;
        }
        else if (rc != JNI_OK)
        {
            Logger::error("Fernflower") << "Failed to get JNIEnv for current thread.";
            return std::nullopt;
        }

        JNIEnv* savedEnv = env_;
        env_ = threadEnv;

        std::string tempDir;
        if (const char* localAppData = std::getenv("LOCALAPPDATA"))
        {
            tempDir = std::string(localAppData) + "\\mcobfF\\cache\\temp_decompile";
        }
        else
        {
            tempDir = "cache/temp_decompile";
        }

        std::error_code ec;
        fs::remove_all(tempDir, ec);
        fs::create_directories(tempDir);

        auto result = decompileSingleClass(jarPath, className, tempDir);

        if (result)
        {
            fs::create_directories(fs::path(cachePath).parent_path());
            std::ofstream ofs(cachePath, std::ios::binary);
            if (ofs)
            {
                ofs.write(result->data(), static_cast<std::streamsize>(result->size()));
                ofs.close();
                Logger::info("Fernflower") << "Cached decompiled source: " << cachePath;
            }
        }

        fs::remove_all(tempDir, ec);

        env_ = savedEnv;

        if (attached)
        {
            jvm_->DetachCurrentThread();
        }

        return result;
    }

    bool FernflowerDecompiler::decompileAllClasses(const std::string& jarPath,
                                                   const std::string& cacheDir,
                                                   const std::vector<std::string>& classNames)
    {
        if (shuttingDown_) return false;

        if (!jvm_)
        {
            Logger::error("Fernflower") << "JVM not initialized.";
            return false;
        }

        std::vector<std::string> classesToDecompile = classNames;
        if (classesToDecompile.empty())
        {
            ZipArchive zip;
            if (!zip.open(jarPath))
            {
                Logger::error("Fernflower") << "Failed to open JAR: " << jarPath;
                return false;
            }

            auto entries = zip.listEntries();
            for (const auto& entry : entries)
            {
                if (entry.size() > 6 && entry.substr(entry.size() - 6) == ".class")
                {
                    std::string className = entry.substr(0, entry.size() - 6);
                    std::string cachePath = getCachePath(cacheDir, className);
                    if (!fs::exists(cachePath))
                    {
                        classesToDecompile.push_back(className);
                    }
                }
            }
        }

        if (classesToDecompile.empty())
        {
            Logger::info("Fernflower") << "All classes already cached.";
            return true;
        }

        Logger::info("Fernflower") << "Decompiling " << classesToDecompile.size() << " classes...";

        JNIEnv* threadEnv = nullptr;
        jint rc = jvm_->GetEnv(reinterpret_cast<void**>(&threadEnv), JNI_VERSION_1_8);
        bool attached = false;
        if (rc == JNI_EDETACHED)
        {
            rc = jvm_->AttachCurrentThread(reinterpret_cast<void**>(&threadEnv), nullptr);
            if (rc != JNI_OK)
            {
                Logger::error("Fernflower") << "Failed to attach thread to JVM.";
                return false;
            }
            attached = true;
        }
        else if (rc != JNI_OK)
        {
            Logger::error("Fernflower") << "Failed to get JNIEnv for current thread.";
            return false;
        }

        JNIEnv* savedEnv = env_;
        env_ = threadEnv;

        std::string tempDir;
        if (const char* localAppData = std::getenv("LOCALAPPDATA"))
        {
            tempDir = std::string(localAppData) + "\\mcobfF\\cache\\temp_decompile";
        }
        else
        {
            tempDir = "cache/temp_decompile";
        }

        int successCount = 0;
        int failCount = 0;

        for (const auto& className : classesToDecompile)
        {
            std::error_code ec;
            fs::remove_all(tempDir, ec);
            fs::create_directories(tempDir);

            auto result = decompileSingleClass(jarPath, className, tempDir);
            if (result)
            {
                std::string cachePath = getCachePath(cacheDir, className);
                fs::create_directories(fs::path(cachePath).parent_path());
                std::ofstream ofs(cachePath, std::ios::binary);
                if (ofs)
                {
                    ofs.write(result->data(), static_cast<std::streamsize>(result->size()));
                    ofs.close();
                    successCount++;
                }
            }
            else
            {
                failCount++;
            }

            if ((successCount + failCount) % 50 == 0)
            {
                Logger::info("Fernflower") << "Progress: " << (successCount + failCount)
                    << "/" << classesToDecompile.size()
                    << " (success: " << successCount << ", fail: " << failCount << ")";
            }
        }

        std::error_code ec;
        fs::remove_all(tempDir, ec);

        env_ = savedEnv;
        if (attached)
        {
            jvm_->DetachCurrentThread();
        }

        Logger::info("Fernflower") << "Batch decompilation complete. Success: " << successCount
            << ", Failed: " << failCount;
        return failCount == 0;
    }

    bool FernflowerDecompiler::runMainMethod(const std::string& jarPath,
                                             const std::string& className,
                                             const std::vector<std::string>& args)
    {
        if (!jvm_)
        {
            Logger::error("Fernflower") << "JVM not initialized.";
            return false;
        }

        JNIEnv* threadEnv = nullptr;
        jint rc = jvm_->GetEnv(reinterpret_cast<void**>(&threadEnv), JNI_VERSION_1_8);
        bool attached = false;
        if (rc == JNI_EDETACHED)
        {
            rc = jvm_->AttachCurrentThread(reinterpret_cast<void**>(&threadEnv), nullptr);
            if (rc != JNI_OK)
            {
                Logger::error("Fernflower") << "Failed to attach thread to JVM.";
                return false;
            }
            attached = true;
        }
        else if (rc != JNI_OK)
        {
            Logger::error("Fernflower") << "Failed to get JNIEnv for current thread.";
            return false;
        }

        JNIEnv* savedEnv = env_;
        env_ = threadEnv;

        bool success = false;

        jclass urlClass = env_->FindClass("java/net/URL");
        jclass urlClassLoaderClass = env_->FindClass("java/net/URLClassLoader");
        jclass stringClass = env_->FindClass("java/lang/String");

        if (urlClass && urlClassLoaderClass && stringClass)
        {
            jmethodID urlCtor = env_->GetMethodID(urlClass, "<init>", "(Ljava/lang/String;)V");
            jmethodID urlClassLoaderCtor = env_->GetMethodID(urlClassLoaderClass, "<init>", "([Ljava/net/URL;)V");
            jmethodID loadClassMethod = env_->GetMethodID(urlClassLoaderClass, "loadClass",
                                                          "(Ljava/lang/String;)Ljava/lang/Class;");

            if (urlCtor && urlClassLoaderCtor && loadClassMethod)
            {
                std::string fileUrl = "file:///" + jarPath;
                std::ranges::replace(fileUrl, '\\', '/');
                jstring jarUrlStr = env_->NewStringUTF(fileUrl.c_str());
                jobject url = env_->NewObject(urlClass, urlCtor, jarUrlStr);
                env_->DeleteLocalRef(jarUrlStr);

                jobjectArray urls = env_->NewObjectArray(1, urlClass, nullptr);
                env_->SetObjectArrayElement(urls, 0, url);

                jobject classLoader = env_->NewObject(urlClassLoaderClass, urlClassLoaderCtor, urls);

                jstring classNameStr = env_->NewStringUTF(className.c_str());
                jclass targetClass = static_cast<jclass>(
                    env_->CallObjectMethod(classLoader, loadClassMethod, classNameStr));
                env_->DeleteLocalRef(classNameStr);

                if (targetClass && !env_->ExceptionCheck())
                {
                    jmethodID mainMethod = env_->GetStaticMethodID(targetClass, "main",
                                                                   "([Ljava/lang/String;)V");
                    if (mainMethod)
                    {
                        jobjectArray jArgs = env_->NewObjectArray(
                            static_cast<jsize>(args.size()), stringClass, nullptr);
                        for (size_t i = 0; i < args.size(); ++i)
                        {
                            env_->SetObjectArrayElement(jArgs, static_cast<jsize>(i),
                                                        env_->NewStringUTF(args[i].c_str()));
                        }

                        env_->CallStaticVoidMethod(targetClass, mainMethod, jArgs);

                        if (!env_->ExceptionCheck())
                        {
                            success = true;
                        }
                        else
                        {
                            env_->ExceptionDescribe();
                            env_->ExceptionClear();
                            Logger::error("Fernflower") << "Exception in " << className << ".main()";
                        }
                    }
                }
                else
                {
                    env_->ExceptionClear();
                    Logger::error("Fernflower") << "Failed to load class: " << className;
                }
            }
        }

        env_ = savedEnv;
        if (attached)
        {
            jvm_->DetachCurrentThread();
        }

        return success;
    }
}
