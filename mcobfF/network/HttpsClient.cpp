#include "HttpsClient.h"
#include <windows.h>
#include <winhttp.h>
#include <fstream>
#include <vector>

#include "mcobfF/Logger.h"
#include "mcobfF/config/ApiConfig.h"

#pragma comment(lib, "winhttp.lib")

#ifndef WINHTTP_REDIRECT_POLICY_AUTOMATIC
#define WINHTTP_REDIRECT_POLICY_AUTOMATIC 0
#endif

namespace mcobfF
{
    std::string HttpsClient::getErrorMessage(unsigned long error)
    {
        LPWSTR msgBuf = nullptr;
        FormatMessageW(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr, error, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPWSTR)&msgBuf, 0, nullptr);

        std::string result;
        if (msgBuf)
        {
            int size = WideCharToMultiByte(CP_UTF8, 0, msgBuf, -1, nullptr, 0, nullptr, nullptr);
            if (size > 0)
            {
                result.resize(size - 1);
                WideCharToMultiByte(CP_UTF8, 0, msgBuf, -1, result.data(), size, nullptr, nullptr);
            }
            LocalFree(msgBuf);
        }
        return result.empty() ? "Unknown error " + std::to_string(error) : result;
    }

    std::optional<HttpsClient::ParsedUrl> HttpsClient::parseUrl(const std::string& url)
    {
        const int wideSize = MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, nullptr, 0);
        if (wideSize == 0) return std::nullopt;

        std::wstring urlWide(wideSize, 0);
        MultiByteToWideChar(CP_UTF8, 0, url.c_str(), -1, urlWide.data(), wideSize);

        wchar_t scheme[32] = {0};
        wchar_t host[256] = {0};
        wchar_t path[2048] = {0};

        URL_COMPONENTSW components = {sizeof(components)};
        components.lpszScheme = scheme;
        components.dwSchemeLength = _countof(scheme);
        components.lpszHostName = host;
        components.dwHostNameLength = _countof(host);
        components.lpszUrlPath = path;
        components.dwUrlPathLength = _countof(path);

        if (!WinHttpCrackUrl(urlWide.c_str(), 0, ICU_DECODE, &components))
        {
            Logger::error("HttpsClient") << "WinHttpCrackUrl failed: " << getErrorMessage(GetLastError());
            return std::nullopt;
        }

        ParsedUrl result;
        result.scheme = scheme;
        result.host = host;
        result.port = components.nPort;
        result.path = (path[0] != L'\0') ? path : L"/";

        if (result.port == 0)
        {
            if (components.nScheme == INTERNET_SCHEME_HTTPS)
            {
                result.port = INTERNET_DEFAULT_HTTPS_PORT;
            }
            else if (components.nScheme == INTERNET_SCHEME_HTTP)
            {
                result.port = INTERNET_DEFAULT_HTTP_PORT;
            }
            else
            {
                return std::nullopt;
            }
        }

        return result;
    }

    std::optional<std::string> HttpsClient::performRequest(
        const ParsedUrl& parsed,
        bool saveToFile,
        const std::string* savePath)
    {
        int wlen = MultiByteToWideChar(CP_UTF8, 0, ApiConfig::USER_AGENT, -1, nullptr, 0);
        std::wstring userAgentWide(static_cast<size_t>(wlen) - 1, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, ApiConfig::USER_AGENT, -1, userAgentWide.data(), wlen);
        HINTERNET session = WinHttpOpen(
            userAgentWide.c_str(),
            WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
            WINHTTP_NO_PROXY_NAME,
            WINHTTP_NO_PROXY_BYPASS,
            0);
        if (!session)
        {
            Logger::error("HttpsClient") << "WinHttpOpen failed: " << getErrorMessage(GetLastError());
            return std::nullopt;
        }

        if (parsed.scheme == L"https")
        {
            DWORD flags = SECURITY_FLAG_IGNORE_UNKNOWN_CA | SECURITY_FLAG_IGNORE_CERT_WRONG_USAGE |
                SECURITY_FLAG_IGNORE_CERT_CN_INVALID | SECURITY_FLAG_IGNORE_CERT_DATE_INVALID;
            WinHttpSetOption(session, WINHTTP_OPTION_SECURITY_FLAGS, &flags, sizeof(flags));
        }

        const HINTERNET connect = WinHttpConnect(session, parsed.host.c_str(), parsed.port, 0);
        if (!connect)
        {
            Logger::error("HttpsClient") << "WinHttpConnect failed: " << getErrorMessage(GetLastError());
            WinHttpCloseHandle(session);
            return std::nullopt;
        }

        HINTERNET request = WinHttpOpenRequest(
            connect, L"GET", parsed.path.c_str(), nullptr,
            WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
            (parsed.scheme == L"https") ? WINHTTP_FLAG_SECURE : 0);
        if (!request)
        {
            Logger::error("HttpsClient") << "WinHttpOpenRequest failed: " << getErrorMessage(GetLastError());
            WinHttpCloseHandle(connect);
            WinHttpCloseHandle(session);
            return std::nullopt;
        }

#ifdef WINHTTP_OPTION_REDIRECT_POLICY
        DWORD redirectPolicy = WINHTTP_REDIRECT_POLICY_AUTOMATIC;
        WinHttpSetOption(request, WINHTTP_OPTION_REDIRECT_POLICY, &redirectPolicy, sizeof(redirectPolicy));
#endif

        DWORD receiveTimeout = 30000;
        DWORD connectTimeout = 10000;
        WinHttpSetOption(request, WINHTTP_OPTION_RECEIVE_TIMEOUT, &receiveTimeout, sizeof(receiveTimeout));
        WinHttpSetOption(request, WINHTTP_OPTION_CONNECT_TIMEOUT, &connectTimeout, sizeof(connectTimeout));

        LPCWSTR userAgent = L"MC-OBF-Find/1.0 (Windows NT 10.0; Win64; x64)";
        WinHttpAddRequestHeaders(request, userAgent, -1, WINHTTP_ADDREQ_FLAG_ADD);

        if (!WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0))
        {
            Logger::error("HttpsClient") << "WinHttpSendRequest failed: " << getErrorMessage(GetLastError());
            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connect);
            WinHttpCloseHandle(session);
            return std::nullopt;
        }

        if (!WinHttpReceiveResponse(request, nullptr))
        {
            Logger::error("HttpsClient") << "WinHttpReceiveResponse failed: " << getErrorMessage(GetLastError());
            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connect);
            WinHttpCloseHandle(session);
            return std::nullopt;
        }

        DWORD statusCode = 0;
        DWORD size = sizeof(statusCode);
        if (!WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                                 WINHTTP_HEADER_NAME_BY_INDEX, &statusCode, &size, WINHTTP_NO_HEADER_INDEX))
        {
            Logger::error("HttpsClient") << "WinHttpQueryHeaders failed: " << getErrorMessage(GetLastError());
            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connect);
            WinHttpCloseHandle(session);
            return std::nullopt;
        }

        if (statusCode != 200)
        {
            Logger::error("HttpsClient") << "HTTP error: " << statusCode;
            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connect);
            WinHttpCloseHandle(session);
            return std::nullopt;
        }

        std::string response;
        std::ofstream fileStream;

        if (saveToFile && savePath)
        {
            fileStream.open(*savePath, std::ios::binary);
            if (!fileStream.is_open())
            {
                Logger::error("HttpsClient") << "Failed to open file for writing: " << *savePath;
                WinHttpCloseHandle(request);
                WinHttpCloseHandle(connect);
                WinHttpCloseHandle(session);
                return std::nullopt;
            }
        }

        DWORD bytesRead = 0;
        char buffer[8192];

        while (true)
        {
            if (!WinHttpReadData(request, buffer, sizeof(buffer), &bytesRead) || bytesRead == 0)
            {
                break;
            }

            if (saveToFile)
            {
                fileStream.write(buffer, bytesRead);
                if (!fileStream.good())
                {
                    Logger::error("HttpsClient") << "Failed to write file: " << *savePath;
                    WinHttpCloseHandle(request);
                    WinHttpCloseHandle(connect);
                    WinHttpCloseHandle(session);
                    return std::nullopt;
                }
            }
            else
            {
                response.append(buffer, bytesRead);
            }
        }

        WinHttpCloseHandle(request);
        WinHttpCloseHandle(connect);
        WinHttpCloseHandle(session);

        if (saveToFile)
        {
            fileStream.close();
            if (!fileStream.good())
            {
                Logger::error("HttpsClient") << "Failed to finalize file write: " << *savePath;
                return std::nullopt;
            }
            return "";
        }

        return response;
    }

    std::optional<std::string> HttpsClient::get(const std::string& url)
    {
        const auto parsed = parseUrl(url);
        if (!parsed)
        {
            Logger::error("HttpsClient") << "Failed to parse URL: " << url;
            return std::nullopt;
        }
        return performRequest(*parsed, false, nullptr);
    }

    bool HttpsClient::downloadToFile(const std::string& url, const std::string& path)
    {
        const auto parsed = parseUrl(url);
        if (!parsed)
        {
            Logger::error("HttpsClient") << "Failed to parse URL: " << url;
            return false;
        }
        const auto result = performRequest(*parsed, true, &path);
        return result.has_value();
    }
} // namespace mc
