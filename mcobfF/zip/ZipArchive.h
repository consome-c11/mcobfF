#pragma once

#include "miniz.h"
#include <string>
#include <vector>
#include <optional>

namespace mcobfF
{
    class ZipArchive
    {
    public:
        ZipArchive() = default;
        ~ZipArchive();

        ZipArchive(const ZipArchive&) = delete;
        ZipArchive& operator=(const ZipArchive&) = delete;

        ZipArchive(ZipArchive&& other) noexcept;
        ZipArchive& operator=(ZipArchive&& other) noexcept;

        [[nodiscard]] bool open(const std::string& path);
        void close();

        [[nodiscard]] bool isOpen() const { return isOpen_; }
        [[nodiscard]] mz_uint getFileCount() const;

        [[nodiscard]] int locateFile(const std::string& filename) const;
        [[nodiscard]] bool getFileStat(mz_uint index, mz_zip_archive_file_stat& stat) const;
        [[nodiscard]] std::optional<std::vector<char>> extractToMemory(mz_uint index) const;

        [[nodiscard]] std::vector<std::string> listEntries() const;
        [[nodiscard]] bool readFile(const std::string& filename, std::string& outContent) const;

        static bool filterJar(const std::string& inputPath,
                              const std::string& outputPath,
                              const std::vector<std::string>& keepPrefixes);

        static bool removeRootClassFiles(const std::string& inputPath,
                                         const std::string& outputPath);

    private:
        mz_zip_archive archive_{};
        bool isOpen_ = false;
    };
} // namespace mc