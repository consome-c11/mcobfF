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
} // namespace mc
