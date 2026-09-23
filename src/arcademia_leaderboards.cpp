#define ARCADEMIA_LEADERBOARDS_BUILD_DLL
#include "../include/arcademia_leaderboards.h"

#include "sandbox_transport.h"
#include "launcher_transport.h"
#include "../third_party/json.hpp"

#include <windows.h>
#include <string>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <memory>
#include <random>

using json = nlohmann::json;
using namespace arcademia;

namespace
{
    struct Settings
    {
        std::string api_base = "https://manager.arcademia.ac";
        std::string api_key;
    };

    Settings g_settings;
    std::unique_ptr<LauncherTransport> g_launcher;
    bool g_initialised = false;

    const int kClaimResponseTimeoutMs = 6 * 60 * 1000;
    const int kDefaultResponseTimeoutMs = 35000;

    const char* AllocResult(const std::string& s)
    {
        char* out = (char*)std::malloc(s.size() + 1);
        if (out == nullptr)
            return nullptr;
        std::memcpy(out, s.c_str(), s.size() + 1);
        return out;
    }

    std::string NewGuid()
    {
        std::random_device rd;
        std::mt19937_64 gen(rd());
        std::uniform_int_distribution<uint64_t> dist;
        uint64_t hi = dist(gen);
        uint64_t lo = dist(gen);
        char buf[64];
        std::snprintf(buf, sizeof(buf), "%08x-%04x-%04x-%04x-%012llx",
            (unsigned)(hi >> 32), (unsigned)((hi >> 16) & 0xFFFF), (unsigned)(hi & 0xFFFF),
            (unsigned)((lo >> 48) & 0xFFFF), (unsigned long long)(lo & 0xFFFFFFFFFFFFULL));
        return std::string(buf);
    }

    std::string UrlEncode(const std::string& value)
    {
        std::ostringstream out;
        for (unsigned char c : value)
        {
            if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~')
                out << c;
            else
                out << '%' << std::uppercase << std::hex << (int)c << std::nouppercase;
        }
        return out.str();
    }

    std::string ExecutableDir()
    {
        char path[MAX_PATH];
        DWORD len = GetModuleFileNameA(nullptr, path, MAX_PATH);
        if (len == 0)
            return "";
        std::string full(path, len);
        auto pos = full.find_last_of("\\/");
        return pos == std::string::npos ? "" : full.substr(0, pos + 1);
    }

    bool LoadSettingsFile(Settings& out)
    {
        std::string path = ExecutableDir() + "arcademia.json";
        std::ifstream file(path);
        if (!file.is_open())
            return false;

        std::ostringstream buffer;
        buffer << file.rdbuf();

        json parsed = json::parse(buffer.str(), nullptr, false);
        if (parsed.is_discarded() || !parsed.is_object())
            return false;

        if (parsed.contains("apiBase") && parsed["apiBase"].is_string())
            out.api_base = parsed["apiBase"].get<std::string>();
        if (parsed.contains("apiKey") && parsed["apiKey"].is_string())
            out.api_key = parsed["apiKey"].get<std::string>();
        return true;
    }

    void EnsureInitialised()
    {
        if (g_initialised)
            return;
        g_initialised = true;

        g_launcher = LauncherTransport::TryCreateFromEnvironment();

        Settings loaded;
        if (LoadSettingsFile(loaded))
            g_settings = loaded;
    }

    bool InLauncherMode()
    {
        EnsureInitialised();
        return g_launcher != nullptr;
    }

    std::string ModeString()
    {
        return InLauncherMode() ? "Launcher" : "Sandbox";
    }

    std::string Describe(const SandboxResponse& response)
    {
        std::string body = response.body;
        while (!body.empty() && (body.front() == '"' || body.back() == '"'))
        {
            if (body.front() == '"') body.erase(0, 1);
            if (!body.empty() && body.back() == '"') body.pop_back();
        }
        return "HTTP " + std::to_string(response.status_code) + (body.empty() ? "" : ": " + body);
    }

    json ParseOrEmpty(const std::string& raw)
    {
        json parsed = json::parse(raw, nullptr, false);
        return parsed.is_discarded() ? json::object() : parsed;
    }
}

void arcademia_leaderboards_init(void)
{
    EnsureInitialised();
}

void arcademia_leaderboards_shutdown(void)
{
    g_launcher.reset();
    g_settings = Settings();
    g_initialised = false;
}

int arcademia_leaderboards_mode(void)
{
    return InLauncherMode() ? ARCADEMIA_MODE_LAUNCHER : ARCADEMIA_MODE_SANDBOX;
}

void arcademia_leaderboards_configure(const char* api_base, const char* api_key)
{
    EnsureInitialised();
    if (api_base != nullptr)
        g_settings.api_base = api_base;
    if (api_key != nullptr)
        g_settings.api_key = api_key;
}

const char* arcademia_leaderboards_ping(void)
{
    EnsureInitialised();
    json result;

    if (g_launcher != nullptr)
    {
        std::string raw;
        if (!g_launcher->Send("hello", "", kDefaultResponseTimeoutMs, raw))
        {
            result = { {"Success", false}, {"Mode", "Launcher"}, {"Message", "The launcher did not respond in time."} };
            return AllocResult(result.dump());
        }

        json dto = ParseOrEmpty(raw);
        bool ok = dto.value("ok", false);
        result["Success"] = ok;
        result["Mode"] = "Launcher";
        result["Environment"] = "Live (via launcher)";
        result["Message"] = ok ? nullptr : json(dto.value("message", dto.value("error", std::string())));
        return AllocResult(result.dump());
    }

    auto response = SandboxGet(g_settings.api_base, "/api/Sdk/Ping", g_settings.api_key);
    if (!response.ok())
    {
        result = { {"Success", false}, {"Mode", "Sandbox"}, {"Message", Describe(response)} };
        return AllocResult(result.dump());
    }

    json dto = ParseOrEmpty(response.body);
    result["Success"] = true;
    result["Mode"] = "Sandbox";
    result["GameId"] = dto.value("gameId", 0);
    result["GameName"] = dto.value("gameName", std::string());
    result["Environment"] = dto.value("environment", std::string()) + " (sandbox)";
    return AllocResult(result.dump());
}

const char* arcademia_leaderboards_submit_score(
    const char* board_slug,
    long long value,
    const char* player_name,
    const char* metadata_json,
    const char* score_id)
{
    EnsureInitialised();
    std::string slug = board_slug != nullptr ? board_slug : "";
    std::string id = (score_id != nullptr && score_id[0] != '\0') ? score_id : NewGuid();

    json result;

    if (g_launcher != nullptr)
    {
        json fields;
        fields["boardSlug"] = slug;
        fields["apiKey"] = g_settings.api_key;
        fields["value"] = value;
        if (player_name != nullptr && player_name[0] != '\0')
            fields["playerName"] = player_name;
        fields["scoreId"] = id;
        if (metadata_json != nullptr && metadata_json[0] != '\0')
        {
            json meta = ParseOrEmpty(metadata_json);
            if (meta.is_object())
                fields["metadata"] = meta;
        }

        std::string raw;
        if (!g_launcher->Send("submitScore", fields.dump(), kDefaultResponseTimeoutMs, raw))
        {
            result = { {"Success", false}, {"Status", "error"}, {"ScoreId", id}, {"Message", "The launcher did not respond in time."}, {"Mode", "Launcher"} };
            return AllocResult(result.dump());
        }

        json dto = ParseOrEmpty(raw);
        bool ok = dto.value("ok", false);
        if (!ok)
        {
            result = { {"Success", false}, {"Status", "error"}, {"ScoreId", id},
                {"Message", dto.value("message", dto.value("error", std::string()))}, {"Mode", "Launcher"} };
            return AllocResult(result.dump());
        }

        std::string status = dto.value("status", std::string());
        result["Success"] = status == "submitted" || status == "queued";
        result["Status"] = status;
        std::string returned_id = dto.value("scoreId", std::string());
        result["ScoreId"] = returned_id.empty() ? id : returned_id;
        long long rank = dto.value("rank", (long long)-1);
        result["Rank"] = rank >= 0 ? json(rank) : json(nullptr);
        result["Duplicate"] = dto.value("duplicate", false);
        result["Message"] = dto.value("message", json(nullptr));
        result["Mode"] = "Launcher";
        return AllocResult(result.dump());
    }

    json body;
    body["value"] = value;
    if (player_name != nullptr && player_name[0] != '\0')
        body["playerName"] = player_name;
    body["scoreId"] = id;
    if (metadata_json != nullptr && metadata_json[0] != '\0')
    {
        json meta = ParseOrEmpty(metadata_json);
        if (meta.is_object())
            body["metadata"] = meta;
    }

    auto response = SandboxPost(g_settings.api_base, "/api/Sdk/Leaderboards/" + UrlEncode(slug) + "/scores", g_settings.api_key, body.dump());
    if (!response.ok())
    {
        result = { {"Success", false}, {"Status", "rejected"}, {"ScoreId", id}, {"Message", Describe(response)}, {"Mode", "Sandbox"} };
        return AllocResult(result.dump());
    }

    json dto = ParseOrEmpty(response.body);
    result["Success"] = true;
    result["Status"] = "submitted";
    std::string returned_id = dto.value("scoreId", std::string());
    result["ScoreId"] = returned_id.empty() ? id : returned_id;
    long long rank = dto.value("rank", (long long)-1);
    result["Rank"] = rank >= 0 ? json(rank) : json(nullptr);
    result["Duplicate"] = dto.value("duplicate", false);
    result["Mode"] = "Sandbox";
    return AllocResult(result.dump());
}

const char* arcademia_leaderboards_get_test_scores(const char* board_slug, int limit, int offset)
{
    EnsureInitialised();
    json result;

    if (g_launcher != nullptr)
    {
        result = { {"Success", false}, {"Message", "Test scores are only available in sandbox mode."} };
        return AllocResult(result.dump());
    }

    std::string slug = board_slug != nullptr ? board_slug : "";
    std::string path = "/api/Sdk/Leaderboards/" + UrlEncode(slug) + "/scores?mode=all&limit=" + std::to_string(limit) + "&offset=" + std::to_string(offset);

    auto response = SandboxGet(g_settings.api_base, path, g_settings.api_key);
    if (!response.ok())
    {
        result = { {"Success", false}, {"Message", Describe(response)} };
        return AllocResult(result.dump());
    }

    json dto = ParseOrEmpty(response.body);
    result["Success"] = true;
    result["BoardSlug"] = dto.contains("board") ? dto["board"].value("slug", json(nullptr)) : json(nullptr);
    result["BoardName"] = dto.contains("board") ? dto["board"].value("name", json(nullptr)) : json(nullptr);
    result["Total"] = dto.value("total", 0);

    json scores = json::array();
    if (dto.contains("scores") && dto["scores"].is_array())
    {
        for (auto& row : dto["scores"])
        {
            scores.push_back({
                {"Rank", row.value("rank", 0)},
                {"PlayerName", row.value("playerName", std::string())},
                {"Value", row.value("value", (long long)0)},
                {"AchievedAt", row.value("achievedAt", std::string())},
            });
        }
    }
    result["Scores"] = scores;
    return AllocResult(result.dump());
}

namespace
{
    const char* ScopeName(int scope)
    {
        switch (scope)
        {
            case ARCADEMIA_SCOPE_LOCAL: return "local";
            case ARCADEMIA_SCOPE_INSTITUTIONAL: return "institutional";
            case ARCADEMIA_SCOPE_COUNTRY: return "country";
            default: return "global";
        }
    }

    const char* ScopeLabel(int scope)
    {
        switch (scope)
        {
            case ARCADEMIA_SCOPE_LOCAL: return "Local";
            case ARCADEMIA_SCOPE_INSTITUTIONAL: return "Institutional";
            case ARCADEMIA_SCOPE_COUNTRY: return "Country";
            default: return "Global";
        }
    }

    json ToScoreRow(const json& row)
    {
        auto text = [&](const char* key) { return row.contains(key) && row[key].is_string() ? row[key] : json(nullptr); };
        return {
            {"Rank", row.value("rank", 0)},
            {"PlayerName", row.value("playerName", std::string())},
            {"Value", row.value("value", (long long)0)},
            {"AchievedAt", row.value("achievedAt", std::string())},
            {"Claimed", row.value("claimed", false)},
            {"IsPlayer", row.value("isPlayer", false)},
            {"MachineName", text("machineName")},
            {"SiteName", text("siteName")},
            {"Country", text("country")},
        };
    }

    json ToScoreRows(const json& dto, const char* key)
    {
        json rows = json::array();
        if (dto.contains(key) && dto[key].is_array())
            for (auto& row : dto[key])
                rows.push_back(ToScoreRow(row));
        return rows;
    }
}

const char* arcademia_leaderboards_get_scores(
    const char* board_slug,
    int scope,
    const char* ranks,
    const char* player_score_id,
    int before,
    int after,
    int best_per_player)
{
    EnsureInitialised();
    json result;

    std::string slug = board_slug != nullptr ? board_slug : "";
    std::string scope_name = ScopeName(scope);
    std::string mode = best_per_player ? "best" : "all";
    std::string mode_label = g_launcher != nullptr ? "Launcher" : "Sandbox";

    if (slug.empty())
    {
        result = { {"Success", false}, {"Message", "boardSlug is required."}, {"Mode", mode_label}, {"Scope", ScopeLabel(scope)} };
        return AllocResult(result.dump());
    }

    json dto;

    if (g_launcher != nullptr)
    {
        json fields;
        fields["boardSlug"] = slug;
        fields["apiKey"] = g_settings.api_key;
        fields["scope"] = scope_name;
        fields["mode"] = mode;
        fields["before"] = before;
        fields["after"] = after;
        if (ranks != nullptr)
            fields["ranks"] = ranks;
        if (player_score_id != nullptr && player_score_id[0] != '\0')
            fields["scoreId"] = player_score_id;

        std::string raw;
        if (!g_launcher->Send("getScores", fields.dump(), kDefaultResponseTimeoutMs, raw))
        {
            result = { {"Success", false}, {"Message", "The launcher did not respond in time."}, {"Mode", mode_label}, {"Scope", ScopeLabel(scope)} };
            return AllocResult(result.dump());
        }

        dto = ParseOrEmpty(raw);
        if (!dto.value("ok", false))
        {
            result = { {"Success", false}, {"Message", dto.value("message", dto.value("error", std::string()))}, {"Mode", mode_label}, {"Scope", ScopeLabel(scope)} };
            return AllocResult(result.dump());
        }
    }
    else
    {
        std::string path = "/api/Sdk/Leaderboards/" + UrlEncode(slug) + "/scores?scope=" + scope_name
            + "&mode=" + mode + "&before=" + std::to_string(before) + "&after=" + std::to_string(after);
        if (ranks != nullptr)
            path += "&ranks=" + UrlEncode(ranks);
        if (player_score_id != nullptr && player_score_id[0] != '\0')
            path += "&scoreId=" + UrlEncode(player_score_id);

        auto response = SandboxGet(g_settings.api_base, path, g_settings.api_key);
        if (!response.ok())
        {
            result = { {"Success", false}, {"Message", Describe(response)}, {"Mode", mode_label}, {"Scope", ScopeLabel(scope)} };
            return AllocResult(result.dump());
        }
        dto = ParseOrEmpty(response.body);
    }

    result["Success"] = true;
    result["Mode"] = mode_label;
    result["BoardSlug"] = dto.contains("board") ? dto["board"].value("slug", json(nullptr)) : json(nullptr);
    result["BoardName"] = dto.contains("board") ? dto["board"].value("name", json(nullptr)) : json(nullptr);
    result["Scope"] = ScopeLabel(scope);
    result["BestPerPlayer"] = dto.value("mode", std::string("best")) != "all";
    result["Total"] = dto.value("total", 0);
    result["Scores"] = ToScoreRows(dto, "scores");
    result["Player"] = dto.contains("player") && dto["player"].is_object() ? ToScoreRow(dto["player"]) : json(nullptr);
    result["Around"] = ToScoreRows(dto, "around");
    return AllocResult(result.dump());
}

const char* arcademia_leaderboards_request_claim(const char* score_id)
{
    EnsureInitialised();
    json result;

    std::string id = score_id != nullptr ? score_id : "";
    if (id.empty())
    {
        result = { {"Success", false}, {"Status", "error"}, {"Message", "scoreId is required."}, {"Mode", ModeString()} };
        return AllocResult(result.dump());
    }

    if (g_launcher == nullptr)
    {
        result = { {"Success", false}, {"Status", "rejected"},
            {"Message", "Claiming a score requires running through the Arcademia launcher."}, {"Mode", "Sandbox"} };
        return AllocResult(result.dump());
    }

    json fields;
    fields["scoreId"] = id;
    fields["apiKey"] = g_settings.api_key;

    std::string raw;
    if (!g_launcher->Send("requestClaim", fields.dump(), kClaimResponseTimeoutMs, raw))
    {
        result = { {"Success", false}, {"Status", "error"}, {"Message", "The launcher did not respond in time."}, {"Mode", "Launcher"} };
        return AllocResult(result.dump());
    }

    json dto = ParseOrEmpty(raw);
    bool ok = dto.value("ok", false);
    if (!ok)
    {
        result = { {"Success", false}, {"Status", "error"},
            {"Message", dto.value("message", dto.value("error", std::string()))}, {"Mode", "Launcher"} };
        return AllocResult(result.dump());
    }

    std::string status = dto.value("status", std::string());
    result["Success"] = status == "saved";
    result["Status"] = status;
    result["Message"] = dto.value("message", json(nullptr));
    result["Mode"] = "Launcher";
    return AllocResult(result.dump());
}

void arcademia_leaderboards_free(const char* ptr)
{
    std::free((void*)ptr);
}
