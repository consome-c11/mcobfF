#include "SRGResolver.h"
#include "SRGParser.h"
#include "mcobfF/file/FileSystem.h"
#include "mcobfF/network/HttpsClient.h"
#include "mcobfF/config/ApiConfig.h"
#include <fstream>
#include <sstream>
#include "mcobfF/Logger.h"
#include <filesystem>

#include "mcobfF/zip/ZipArchive.h"

namespace fs = std::filesystem;

namespace mcobfF
{
    bool SRGResolver::loadFromZip(const std::string& zipPath, MappingData& mappings)
    {
        ZipArchive zip;
        if (!zip.open(zipPath))
        {
            Logger::error("SRGResolver") << "Failed to open zip archive: " << zipPath;
            return false;
        }

        int index = zip.locateFile("config/joined.tsrg");
        if (index < 0)
        {
            index = zip.locateFile("joined.tsrg");
        }

        if (index < 0)
        {
            Logger::error("SRGResolver") << "joined.tsrg not found in archive.";
            return false;
        }

        auto data = zip.extractToMemory(index);
        if (!data)
        {
            Logger::error("SRGResolver") << "Failed to extract joined.tsrg to memory.";
            return false;
        }

        const std::string content(data->begin(), data->end());
        return SRGParser::parse(content, mappings);
    }

    bool SRGResolver::loadFromFile(const std::string& filePath, MappingData& mappings)
    {
        const std::ifstream file(filePath, std::ios::binary);
        if (!file.is_open())
        {
            Logger::error("SRGResolver") << "Failed to open file: " << filePath;
            return false;
        }

        std::stringstream buffer;
        buffer << file.rdbuf();

        return SRGParser::parse(buffer.str(), mappings);
    }

    bool SRGResolver::isVersionAtLeast1122(const std::string& version)
    {
        if (version.find('w') != std::string::npos) return true;
        if (!version.starts_with("1.")) return version.starts_with("2.") || version.starts_with("3.");
        std::string afterPrefix = version.substr(2);
        const size_t dot = afterPrefix.find('.');
        const std::string minorStr = (dot == std::string::npos) ? afterPrefix : afterPrefix.substr(0, dot);
        try
        {
            const int minor = std::stoi(minorStr);
            return minor >= 12;
        }
        catch (...)
        {
            return false;
        }
    }

    std::string SRGResolver::convertJoinedSrgToTsrg(const std::string& content)
    {
        std::istringstream stream(content);
        std::string line;
        std::ostringstream result;

        auto trimLeft = [](const std::string& s) -> std::string
        {
            const size_t start = s.find_first_not_of(" \t");
            return (start == std::string::npos) ? "" : s.substr(start);
        };

        auto replaceArrow = [](const std::string& s) -> std::string
        {
            if (const size_t pos = s.find(" -> "); pos != std::string::npos)
            {
                std::string out = s;
                out.replace(pos, 4, "\t");
                return out;
            }
            const size_t space = s.find(' ');
            const size_t tab = s.find('\t');
            if (const size_t firstSep = std::min(space, tab); firstSep != std::string::npos)
            {
                std::string out = s;
                out[firstSep] = '\t';
                return out;
            }
            return s;
        };

        while (std::getline(stream, line))
        {
            if (!line.empty() && line.back() == '\r') line.pop_back();

            if (line.empty() || line[0] == '#')
            {
                result << line << '\n';
                continue;
            }

            int tabs = 0;
            while (tabs < (int)line.size() && line[tabs] == '\t') tabs++;

            if (std::string trimmed = line.substr(tabs); trimmed.rfind("<class>", 0) == 0)
            {
                std::string rest = trimLeft(trimmed.substr(7));
                result << std::string(tabs, '\t') << replaceArrow(rest) << '\n';
            }
            else if (trimmed.rfind("<method>", 0) == 0)
            {
                std::string rest = trimLeft(trimmed.substr(8));
                size_t paren = rest.find('(');
                if (size_t arrow = rest.find(" -> "); paren != std::string::npos && arrow != std::string::npos)
                {
                    std::string obfName = rest.substr(0, paren);
                    if (!obfName.empty() && obfName.back() == ' ') obfName.pop_back();
                    size_t close = rest.find(')', paren);
                    std::string desc = (close != std::string::npos)
                                           ? rest.substr(paren, close - paren + 1)
                                           : rest.substr(paren);
                    std::string srgName = trimLeft(rest.substr(arrow + 4));
                    result << std::string(tabs, '\t') << obfName << '\t' << desc << '\t' << srgName << '\n';
                }
                else
                {
                    result << line << '\n';
                }
            }
            else if (trimmed.rfind("<field>", 0) == 0)
            {
                std::string rest = trimLeft(trimmed.substr(7));
                result << std::string(tabs, '\t') << replaceArrow(rest) << '\n';
            }
            else
            {
                result << line << '\n';
            }
        }

        return result.str();
    }

    bool SRGResolver::downloadAndLoad(const std::string& version, const std::string& cacheDir, MappingData& mappings)
    {
        auto cachePath = fs::path(cacheDir);
        std::string cacheFile = (cachePath / "joined.tsrg").string();

        if (FileSystem::fileExists(cacheFile))
        {
            if (auto cached = FileSystem::readFile(cacheFile); cached && !cached->empty())
            {
                Logger::info("SRGResolver") << "Loading SRG from cache: " << cacheFile;
                std::string tsrgContent = std::move(*cached);
                return SRGParser::parse(tsrgContent, mappings);
            }
        }

        std::string tsrgContent;
        bool is1122Plus = isVersionAtLeast1122(version);

        if (is1122Plus)
        {
            std::string url = std::string(ApiConfig::FORGE_MCPCONFIG_RAW) + "/" + version + "/joined.tsrg";
            Logger::info("SRGResolver") << "Downloading SRG from: " << url;
            auto result = HttpsClient::get(url);
            if (!result)
            {
                url = std::string(ApiConfig::NEOFORGED_MCPCONFIG_RAW) + "/" + version + "/joined.tsrg";
                Logger::info("SRGResolver") << "Trying NeoForged MCPConfig: " << url;
                result = HttpsClient::get(url);
            }
            if (result)
            {
                tsrgContent = std::move(*result);
            }
        }

        if (tsrgContent.empty())
        {
            std::string zipUrl = std::string(ApiConfig::FORGE_MAVEN) + "/" + version + "/mcp-" + version + "-srg.zip";
            std::string zipPath = (cachePath / "mcp-srg.zip").string();
            Logger::info("SRGResolver") << "Downloading MCP SRG zip from: " << zipUrl;

            if (!HttpsClient::downloadToFile(zipUrl, zipPath))
            {
                Logger::error("SRGResolver") << "Failed to download MCP SRG zip.";
                return false;
            }

            ZipArchive zip;
            if (!zip.open(zipPath))
            {
                Logger::error("SRGResolver") << "Failed to open MCP SRG zip.";
                return false;
            }

            if (int idx = zip.locateFile("joined.tsrg"); idx >= 0)
            {
                auto data = zip.extractToMemory(idx);
                if (data) tsrgContent.assign(data->data(), data->size());
            }
            else
            {
                idx = zip.locateFile("joined.srg");
                if (idx >= 0)
                {
                    auto data = zip.extractToMemory(idx);
                    if (data)
                    {
                        std::string srgContent(data->data(), data->size());
                        tsrgContent = convertJoinedSrgToTsrg(srgContent);
                        Logger::info("SRGResolver") << "Converted old SRG format to TSRG.";
                    }
                }
            }
        }

        if (tsrgContent.empty())
        {
            Logger::error("SRGResolver") << "No SRG data found for version: " << version;
            return false;
        }

        if (!FileSystem::fileExists(cachePath.string()))
        {
            FileSystem::createDirectories(cachePath.string());
        }
        FileSystem::writeFile(cacheFile, tsrgContent);

        return SRGParser::parse(tsrgContent, mappings);
    }
} // namespace mc
