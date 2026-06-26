#pragma once

#include <string>
#include <optional>
#include <vector>
#include <atomic>
#include <jni.h>

#ifdef _WIN32
#include <windows.h>
using CreateJavaVM_t = jint (JNICALL *)(JavaVM**, void**, JavaVMInitArgs*);
using GetCreatedJavaVMs_t = jint (JNICALL *)(JavaVM**, jsize, jsize*);
#else
#include <dlfcn.h>
using CreateJavaVM_t = jint (*)(JavaVM**, void**, JavaVMInitArgs*);
using GetCreatedJavaVMs_t = jint (*)(JavaVM**, jsize, jsize*);
#endif

namespace mcobfF
{
    class FernflowerDecompiler
    {
    public:
        FernflowerDecompiler();
        ~FernflowerDecompiler();

        FernflowerDecompiler(const FernflowerDecompiler&) = delete;
        FernflowerDecompiler& operator=(const FernflowerDecompiler&) = delete;

        [[nodiscard]] bool initialize(const std::string& fernflowerJarPath, const std::string& javaHome = "");
        [[nodiscard]] bool isInitialized() const { return jvm_ != nullptr; }

        [[nodiscard]] std::optional<std::string> decompileClass(const std::string& jarPath,
                                                                const std::string& className,
                                                                const std::string& cacheDir);

        [[nodiscard]] bool decompileAllClasses(const std::string& jarPath,
                                               const std::string& cacheDir,
                                               const std::vector<std::string>& classNames = {});

        [[nodiscard]] bool runMainMethod(const std::string& jarPath,
                                         const std::string& className,
                                         const std::vector<std::string>& args);

        void shutdown();

        [[nodiscard]] static std::string getDefaultFernflowerPath();
        [[nodiscard]] static bool downloadFernflower(const std::string& outputPath);

        [[nodiscard]] static std::string getCachePath(const std::string& cacheDir,
                                                       const std::string& className);

        [[nodiscard]] static bool hasCache(const std::string& cacheDir,
                                           const std::string& className);
        [[nodiscard]] static std::optional<std::string> readCached(const std::string& cacheDir,
                                                                    const std::string& className);

    private:
        JavaVM* jvm_ = nullptr;
        JNIEnv* env_ = nullptr;
        bool ownsJvm_ = false;
        std::atomic<bool> shuttingDown_{false};
        std::string fernflowerJarPath_;
        std::string resolvedJavaHome_;
        bool isFernflower_ = false;

#ifdef _WIN32
        HMODULE jvmDll_ = nullptr;
#else
        void* jvmDll_ = nullptr;
#endif

        CreateJavaVM_t fnCreateJavaVM_ = nullptr;
        GetCreatedJavaVMs_t fnGetCreatedJavaVMs_ = nullptr;

        [[nodiscard]] static std::string findJavaHome();
        [[nodiscard]] bool loadJvmLibrary(const std::string& javaHome);
        [[nodiscard]] bool createJvm(const std::string& javaHome);
        [[nodiscard]] std::optional<std::string> decompileSingleClass(const std::string& jarPath,
                                                                      const std::string& className,
                                                                      const std::string& outputDir);
        [[nodiscard]] std::optional<std::string> decompileViaConsole(const std::string& jarPath,
                                                                     const std::string& outputDir);
    };
}
