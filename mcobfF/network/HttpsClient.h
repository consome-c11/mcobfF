#pragma once

#include <string>
#include <optional>

namespace mcobfF
{
    class HttpsClient
    {
    public:
        [[nodiscard]] static std::optional<std::string> get(const std::string& url);
        [[nodiscard]] static bool downloadToFile(const std::string& url, const std::string& path);

    private:
        struct ParsedUrl
        {
            std::wstring scheme;
            std::wstring host;
            unsigned short port;
            std::wstring path;
        };

        [[nodiscard]] static std::optional<ParsedUrl> parseUrl(const std::string& url);
        [[nodiscard]] static std::string getErrorMessage(unsigned long error);
        [[nodiscard]] static std::optional<std::string> performRequest(
            const ParsedUrl& parsed,
            bool saveToFile,
            const std::string* savePath
        );
    };
} // namespace mc