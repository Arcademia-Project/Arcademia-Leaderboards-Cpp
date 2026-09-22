#pragma once

#include <string>
#include <memory>

namespace arcademia
{
    class LauncherTransport
    {
    public:
        static std::unique_ptr<LauncherTransport> TryCreateFromEnvironment();
        ~LauncherTransport();

        LauncherTransport(const LauncherTransport&) = delete;
        LauncherTransport& operator=(const LauncherTransport&) = delete;

        const std::string& SessionId() const { return session_id_; }

        bool Send(const std::string& op, const std::string& extra_fields_json, int timeout_ms, std::string& out_response);

    private:
        LauncherTransport(std::string pipe_name, std::string nonce, std::string session_id);

        bool EnsureConnected();
        void Reset();

        std::string pipe_name_;
        std::string nonce_;
        std::string session_id_;
        void* pipe_handle_ = nullptr;
    };
}
