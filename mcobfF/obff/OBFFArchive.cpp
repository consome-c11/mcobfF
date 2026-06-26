#include "OBFFArchive.h"
#include "mcobfF/Logger.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include "miniz.h"

namespace fs = std::filesystem;

namespace mcobfF
{
    std::string OBFFArchive::getEntryName(const std::string& className)
    {
        std::string safe = className;
        for (auto& c : safe)
        {
            if (c == '/' || c == '.') c = '_';
        }
        return safe + ".java";
    }

    std::string OBFFArchive::getObffPath(const std::string& decompileCacheDir)
    {
        return decompileCacheDir + ".obff";
    }

    bool OBFFArchive::pack(const std::string& sourceDir, const std::string& obffPath)
    {
        std::error_code ec;
        if (!fs::exists(sourceDir, ec))
        {
            Logger::warn("OBFF") << "Source dir does not exist: " << sourceDir;
            return false;
        }

        mz_zip_archive writer{};
        if (!mz_zip_writer_init_file(&writer, obffPath.c_str(), 0))
        {
            Logger::error("OBFF") << "Failed to create obff: " << obffPath;
            return false;
        }

        int addedCount = 0;
        for (const auto& entry : fs::recursive_directory_iterator(sourceDir, ec))
        {
            if (!entry.is_regular_file()) continue;
            const auto& path = entry.path();
            if (path.extension() != ".java") continue;

            std::ifstream file(path, std::ios::binary);
            if (!file) continue;
            std::ostringstream buf;
            buf << file.rdbuf();
            std::string content = buf.str();

            std::string entryName = path.filename().string();

            if (!mz_zip_writer_add_mem(&writer, entryName.c_str(),
                                        content.data(), content.size(),
                                        MZ_BEST_COMPRESSION))
            {
                Logger::warn("OBFF") << "Failed to add entry: " << entryName;
                continue;
            }
            addedCount++;
        }

        mz_zip_writer_finalize_archive(&writer);
        mz_zip_writer_end(&writer);

        Logger::info("OBFF") << "Packed " << addedCount << " entries into " << obffPath;

        if (addedCount == 0)
        {
            fs::remove(obffPath, ec);
            return false;
        }

        for (const auto& entry : fs::recursive_directory_iterator(sourceDir, ec))
        {
            if (entry.is_regular_file() && entry.path().extension() == ".java")
            {
                fs::remove(entry.path(), ec);
            }
        }

        Logger::info("OBFF") << "Removed individual .java cache files from " << sourceDir;
        return true;
    }

    std::optional<std::string> OBFFArchive::readEntry(const std::string& obffPath,
                                                       const std::string& entryName)
    {
        mz_zip_archive archive{};
        if (!mz_zip_reader_init_file(&archive, obffPath.c_str(), 0))
        {
            return std::nullopt;
        }

        int idx = mz_zip_reader_locate_file(&archive, entryName.c_str(), nullptr, 0);
        if (idx < 0)
        {
            mz_zip_reader_end(&archive);
            return std::nullopt;
        }

        mz_zip_archive_file_stat stat{};
        if (!mz_zip_reader_file_stat(&archive, static_cast<mz_uint>(idx), &stat))
        {
            mz_zip_reader_end(&archive);
            return std::nullopt;
        }

        std::string content(stat.m_uncomp_size, '\0');
        if (!mz_zip_reader_extract_to_mem(&archive, static_cast<mz_uint>(idx),
                                           content.data(), content.size(), 0))
        {
            mz_zip_reader_end(&archive);
            return std::nullopt;
        }

        mz_zip_reader_end(&archive);
        return content;
    }

    bool OBFFArchive::hasEntry(const std::string& obffPath, const std::string& entryName)
    {
        mz_zip_archive archive{};
        if (!mz_zip_reader_init_file(&archive, obffPath.c_str(), 0))
        {
            return false;
        }
        int idx = mz_zip_reader_locate_file(&archive, entryName.c_str(), nullptr, 0);
        mz_zip_reader_end(&archive);
        return idx >= 0;
    }
}
