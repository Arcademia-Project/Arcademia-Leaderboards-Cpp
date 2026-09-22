#include "sandbox_transport.h"

#include <windows.h>
#include <winhttp.h>
#include <vector>

#if defined(_MSC_VER)
#pragma comment(lib, "winhttp.lib")
#endif

namespace arcademia
{
    namespace
    {
        std::wstring Utf8ToWide(const std::string& s)
        {
            if (s.empty())
                return std::wstring();
            int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
            std::wstring out(len, L'\0');
            MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], len);
            return out;
        }

        SandboxResponse Send(const std::string& method, const std::string& api_base, const std::string& path, const std::string& api_key, const std::string* json_body)
        {
            SandboxResponse result;

            std::wstring wide_url = Utf8ToWide(api_base + path);

            URL_COMPONENTS parts{};
            parts.dwStructSize = sizeof(parts);
            wchar_t host[256]{};
            parts.lpszHostName = host;
            parts.dwHostNameLength = 256;
            wchar_t url_path[2048]{};
            parts.lpszUrlPath = url_path;
            parts.dwUrlPathLength = 2048;
            parts.dwSchemeLength = (DWORD)-1;

            if (!WinHttpCrackUrl(wide_url.c_str(), 0, 0, &parts))
            {
                result.status_code = 0;
                result.body = "Invalid apiBase/path URL.";
                return result;
            }

            bool secure = parts.nScheme == INTERNET_SCHEME_HTTPS;

            HINTERNET session = WinHttpOpen(L"Arcademia-Leaderboards-Cpp/1.0",
                WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
            if (!session)
            {
                result.body = "Could not initialise WinHTTP session.";
                return result;
            }

            HINTERNET connect = WinHttpConnect(session, parts.lpszHostName, parts.nPort, 0);
            if (!connect)
            {
                WinHttpCloseHandle(session);
                result.body = "Could not connect to host.";
                return result;
            }

            std::wstring wmethod = Utf8ToWide(method);
            HINTERNET request = WinHttpOpenRequest(connect, wmethod.c_str(), parts.lpszUrlPath,
                nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                secure ? WINHTTP_FLAG_SECURE : 0);
            if (!request)
            {
                WinHttpCloseHandle(connect);
                WinHttpCloseHandle(session);
                result.body = "Could not open request.";
                return result;
            }

            std::wstring header = L"X-Arcademia-Key: " + Utf8ToWide(api_key);
            WinHttpAddRequestHeaders(request, header.c_str(), (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);

            bool sent;
            if (json_body != nullptr)
            {
                std::wstring content_type = L"Content-Type: application/json";
                WinHttpAddRequestHeaders(request, content_type.c_str(), (DWORD)-1, WINHTTP_ADDREQ_FLAG_ADD);
                sent = WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                    (LPVOID)json_body->data(), (DWORD)json_body->size(), (DWORD)json_body->size(), 0);
            }
            else
            {
                sent = WinHttpSendRequest(request, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                    WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
            }

            if (sent)
                sent = WinHttpReceiveResponse(request, nullptr) != FALSE;

            if (!sent)
            {
                result.body = "Request failed (network error or timeout).";
                WinHttpCloseHandle(request);
                WinHttpCloseHandle(connect);
                WinHttpCloseHandle(session);
                return result;
            }

            DWORD status = 0;
            DWORD status_size = sizeof(status);
            WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size, WINHTTP_NO_HEADER_INDEX);
            result.status_code = (int)status;

            std::string body;
            for (;;)
            {
                DWORD available = 0;
                if (!WinHttpQueryDataAvailable(request, &available) || available == 0)
                    break;

                std::vector<char> buffer(available);
                DWORD read = 0;
                if (!WinHttpReadData(request, buffer.data(), available, &read))
                    break;
                body.append(buffer.data(), read);
            }
            result.body = body;

            WinHttpCloseHandle(request);
            WinHttpCloseHandle(connect);
            WinHttpCloseHandle(session);
            return result;
        }
    }

    SandboxResponse SandboxGet(const std::string& api_base, const std::string& path, const std::string& api_key)
    {
        return Send("GET", api_base, path, api_key, nullptr);
    }

    SandboxResponse SandboxPost(const std::string& api_base, const std::string& path, const std::string& api_key, const std::string& json_body)
    {
        return Send("POST", api_base, path, api_key, &json_body);
    }
}
