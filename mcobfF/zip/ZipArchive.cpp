#include "ZipArchive.h"
#include "mcobfF/Logger.h"

namespace mcobfF
{
    ZipArchive::~ZipArchive()
    {
        close();
    }

    ZipArchive::ZipArchive(ZipArchive&& other) noexcept
        : archive_(other.archive_), isOpen_(other.isOpen_)
    {
        other.isOpen_ = false;
    }

    ZipArchive& ZipArchive::operator=(ZipArchive&& other) noexcept
    {
        if (this != &other)
        {
            close();
            archive_ = other.archive_;
            isOpen_ = other.isOpen_;
            other.isOpen_ = false;
        }
        return *this;
    }

    bool ZipArchive::open(const std::string& path)
    {
        close();
        if (mz_zip_reader_init_file(&archive_, path.c_str(), 0))
        {
            isOpen_ = true;
            return true;
        }
        Logger::error("ZipArchive") << "Failed to open: " << path;
        return false;
    }

    void ZipArchive::close()
    {
        if (isOpen_)
        {
            mz_zip_reader_end(&archive_);
            isOpen_ = false;
        }
    }

    mz_uint ZipArchive::getFileCount() const
    {
        if (!isOpen_) return 0;
        return mz_zip_reader_get_num_files(const_cast<mz_zip_archive*>(&archive_));
    }

    int ZipArchive::locateFile(const std::string& filename) const
    {
        if (!isOpen_) return -1;
        return mz_zip_reader_locate_file(const_cast<mz_zip_archive*>(&archive_), filename.c_str(), nullptr, 0);
    }

    bool ZipArchive::getFileStat(mz_uint index, mz_zip_archive_file_stat& stat) const
    {
        if (!isOpen_) return false;
        return mz_zip_reader_file_stat(const_cast<mz_zip_archive*>(&archive_), index, &stat) != 0;
    }

    std::optional<std::vector<char>> ZipArchive::extractToMemory(mz_uint index) const
    {
        if (!isOpen_) return std::nullopt;

        mz_zip_archive_file_stat stat{};
        if (!getFileStat(index, stat)) return std::nullopt;

        std::vector<char> buffer(stat.m_uncomp_size);
        if (!mz_zip_reader_extract_to_mem(const_cast<mz_zip_archive*>(&archive_), index, buffer.data(), buffer.size(),
                                          0))
        {
            return std::nullopt;
        }
        return buffer;
    }

    std::vector<std::string> ZipArchive::listEntries() const
    {
        std::vector<std::string> entries;
        if (!isOpen_) return entries;

        mz_uint count = getFileCount();
        entries.reserve(count);
        for (mz_uint i = 0; i < count; i++)
        {
            mz_zip_archive_file_stat stat{};
            if (getFileStat(i, stat))
            {
                if (!stat.m_is_directory)
                {
                    entries.emplace_back(stat.m_filename);
                }
            }
        }
        return entries;
    }

    bool ZipArchive::readFile(const std::string& filename, std::string& outContent) const
    {
        if (!isOpen_) return false;
        int idx = locateFile(filename);
        if (idx < 0) return false;
        auto data = extractToMemory(static_cast<mz_uint>(idx));
        if (!data) return false;
        outContent.assign(data->data(), data->size());
        return true;
    }

    bool ZipArchive::filterJar(const std::string& inputPath,
                               const std::string& outputPath,
                               const std::vector<std::string>& keepPrefixes)
    {
        mz_zip_archive reader{};
        if (!mz_zip_reader_init_file(&reader, inputPath.c_str(), 0))
        {
            Logger::error("ZipArchive") << "filterJar: Failed to open input: " << inputPath;
            return false;
        }

        mz_zip_archive writer{};
        if (!mz_zip_writer_init_file(&writer, outputPath.c_str(), 0))
        {
            Logger::error("ZipArchive") << "filterJar: Failed to create output: " << outputPath;
            mz_zip_reader_end(&reader);
            return false;
        }

        const mz_uint numFiles = mz_zip_reader_get_num_files(&reader);
        mz_uint kept = 0;

        for (mz_uint i = 0; i < numFiles; ++i)
        {
            mz_zip_archive_file_stat stat{};
            if (!mz_zip_reader_file_stat(&reader, i, &stat))
                continue;

            bool keep = true;
            if (stat.m_is_directory)
            {
                keep = false;
            }
            else
            {
                bool matchesPrefix = false;
                for (const auto& prefix : keepPrefixes)
                {
                    if (strncmp(stat.m_filename, prefix.c_str(), prefix.size()) == 0)
                    {
                        matchesPrefix = true;
                        break;
                    }
                }
                keep = matchesPrefix;
            }

            if (keep)
            {
                if (!mz_zip_writer_add_from_zip_reader(&writer, &reader, i))
                {
                    Logger::error("ZipArchive") << "filterJar: Failed to copy entry: " << stat.m_filename;
                    mz_zip_writer_end(&writer);
                    mz_zip_reader_end(&reader);
                    return false;
                }
                ++kept;
            }
        }

        mz_zip_writer_finalize_archive(&writer);
        mz_zip_writer_end(&writer);
        mz_zip_reader_end(&reader);

        Logger::info("ZipArchive") << "filterJar: Kept " << kept << " of " << numFiles << " entries in " << outputPath;
        return true;
    }

    bool ZipArchive::removeRootClassFiles(const std::string& inputPath,
                                          const std::string& outputPath)
    {
        mz_zip_archive reader{};
        if (!mz_zip_reader_init_file(&reader, inputPath.c_str(), 0))
        {
            Logger::error("ZipArchive") << "removeRootClassFiles: Failed to open input: " << inputPath;
            return false;
        }

        mz_zip_archive writer{};
        if (!mz_zip_writer_init_file(&writer, outputPath.c_str(), 0))
        {
            Logger::error("ZipArchive") << "removeRootClassFiles: Failed to create output: " << outputPath;
            mz_zip_reader_end(&reader);
            return false;
        }

        const mz_uint numFiles = mz_zip_reader_get_num_files(&reader);
        mz_uint kept = 0;
        mz_uint removed = 0;

        for (mz_uint i = 0; i < numFiles; ++i)
        {
            mz_zip_archive_file_stat stat{};
            if (!mz_zip_reader_file_stat(&reader, i, &stat))
                continue;

            bool skip = false;
            if (!stat.m_is_directory)
            {
                const std::string name(stat.m_filename);
                const bool isClass = name.size() > 6 && name.substr(name.size() - 6) == ".class";
                const bool hasSlash = name.find('/') != std::string::npos;
                skip = isClass && !hasSlash;
            }

            if (skip)
            {
                ++removed;
                continue;
            }

            if (!mz_zip_writer_add_from_zip_reader(&writer, &reader, i))
            {
                Logger::error("ZipArchive") << "removeRootClassFiles: Failed to copy entry: " << stat.m_filename;
                mz_zip_writer_end(&writer);
                mz_zip_reader_end(&reader);
                return false;
            }
            ++kept;
        }

        mz_zip_writer_finalize_archive(&writer);
        mz_zip_writer_end(&writer);
        mz_zip_reader_end(&reader);

        Logger::info("ZipArchive") << "removeRootClassFiles: Removed " << removed
            << " root class files, kept " << kept << " of " << numFiles << " entries";
        return true;
    }
} // namespace mc