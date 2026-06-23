#include "FileSystem.h"
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;

namespace mcobfF
{
    bool FileSystem::createDirectories(const std::string& path)
    {
        std::error_code ec;
        fs::create_directories(path, ec);
        return !ec;
    }

    bool FileSystem::fileExists(const std::string& path)
    {
        return fs::exists(path);
    }

    std::optional<std::string> FileSystem::readFile(const std::string& path)
    {
        std::ifstream file(path, std::ios::binary);
        if (!file.is_open()) return std::nullopt;

        std::stringstream buffer;
        buffer << file.rdbuf();
        return buffer.str();
    }

    bool FileSystem::writeFile(const std::string& path, const std::string& content)
    {
        std::ofstream file(path, std::ios::binary);
        if (!file.is_open()) return false;
        file << content;
        return file.good();
    }
} // namespace mc