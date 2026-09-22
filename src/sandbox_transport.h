#pragma once

#include <string>

namespace arcademia
{
    struct SandboxResponse
    {
        int status_code = 0;
        std::string body;
        bool ok() const { return status_code >= 200 && status_code < 300; }
    };

    SandboxResponse SandboxGet(const std::string& api_base, const std::string& path, const std::string& api_key);
    SandboxResponse SandboxPost(const std::string& api_base, const std::string& path, const std::string& api_key, const std::string& json_body);
}
