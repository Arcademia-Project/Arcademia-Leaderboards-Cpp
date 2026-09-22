#include "launcher_transport.h"

#include <windows.h>
#include <random>
#include <sstream>
#include <future>
#include <cstdlib>

#include "../third_party/json.hpp"

namespace arcademia
{
    namespace
    {
        std::string GetEnv(const char* name)
        {
            const char* value = std::getenv(name);
            return value != nullptr ? std::string(value) : std::string();
        }

        std::string NewRequestId()
        {
            std::random_device rd;
            std::mt19937_64 gen(rd());
            std::uniform_int_distribution<uint64_t> dist;
            std::ostringstream oss;
            oss << std::hex << dist(gen) << dist(gen);
            auto s = oss.str();
            if (s.size() > 32)
                s = s.substr(0, 32);
            while (s.size() < 32)
                s += "0";
            return s;
        }

        bool ReadLine(HANDLE pipe, std::string& out_line)
        {
            std::string buffer;
            char chunk[512];
            for (;;)
            {
                DWORD read = 0;
                if (!ReadFile(pipe, chunk, sizeof(chunk), &read, nullptr) || read == 0)
                    return false;

                buffer.append(chunk, read);
                auto pos = buffer.find('\n');
                if (pos != std::string::npos)
                {
                    out_line = buffer.substr(0, pos);
                    return true;
                }
            }
        }
    }

    std::unique_ptr<LauncherTransport> LauncherTransport::TryCreateFromEnvironment()
    {
        auto pipe = GetEnv("ARCADEMIA_PIPE");
        auto nonce = GetEnv("ARCADEMIA_NONCE");
        auto session = GetEnv("ARCADEMIA_SESSION_ID");

        if (pipe.empty() || nonce.empty())
            return nullptr;

        return std::unique_ptr<LauncherTransport>(new LauncherTransport(pipe, nonce, session));
    }

    LauncherTransport::LauncherTransport(std::string pipe_name, std::string nonce, std::string session_id)
        : pipe_name_(std::move(pipe_name)), nonce_(std::move(nonce)), session_id_(std::move(session_id))
    {
    }

    LauncherTransport::~LauncherTransport()
    {
        Reset();
    }

    void LauncherTransport::Reset()
    {
        if (pipe_handle_ != nullptr)
        {
            CloseHandle((HANDLE)pipe_handle_);
            pipe_handle_ = nullptr;
        }
    }

    bool LauncherTransport::EnsureConnected()
    {
        if (pipe_handle_ != nullptr)
            return true;

        std::string full_name = "\\\\.\\pipe\\" + pipe_name_;

        HANDLE handle = CreateFileA(full_name.c_str(), GENERIC_READ | GENERIC_WRITE,
            0, nullptr, OPEN_EXISTING, 0, nullptr);

        if (handle == INVALID_HANDLE_VALUE && GetLastError() == ERROR_PIPE_BUSY)
        {
            WaitNamedPipeA(full_name.c_str(), 3000);
            handle = CreateFileA(full_name.c_str(), GENERIC_READ | GENERIC_WRITE,
                0, nullptr, OPEN_EXISTING, 0, nullptr);
        }

        if (handle == INVALID_HANDLE_VALUE)
            return false;

        pipe_handle_ = (void*)handle;
        return true;
    }

    bool LauncherTransport::Send(const std::string& op, const std::string& extra_fields_json, int timeout_ms, std::string& out_response)
    {
        if (!EnsureConnected())
            return false;

        nlohmann::json request;
        if (!extra_fields_json.empty())
        {
            request = nlohmann::json::parse(extra_fields_json, nullptr, false);
            if (request.is_discarded())
                request = nlohmann::json::object();
        }
        else
        {
            request = nlohmann::json::object();
        }
        request["id"] = NewRequestId();
        request["nonce"] = nonce_;
        request["op"] = op;

        std::string line = request.dump() + "\n";

        HANDLE pipe = (HANDLE)pipe_handle_;
        DWORD written = 0;
        if (!WriteFile(pipe, line.data(), (DWORD)line.size(), &written, nullptr))
        {
            Reset();
            return false;
        }

        auto future = std::async(std::launch::async, [pipe]() {
            std::string line;
            bool ok = ReadLine(pipe, line);
            return std::make_pair(ok, line);
        });

        if (future.wait_for(std::chrono::milliseconds(timeout_ms)) != std::future_status::ready)
        {
            Reset();
            return false;
        }

        auto [ok, response] = future.get();
        if (!ok)
        {
            Reset();
            return false;
        }

        out_response = response;
        return true;
    }
}
