#ifndef ARCADEMIA_LEADERBOARDS_H
#define ARCADEMIA_LEADERBOARDS_H

#if defined(_WIN32)
  #if defined(ARCADEMIA_LEADERBOARDS_BUILD_DLL)
    #define ARCADEMIA_API __declspec(dllexport)
  #elif defined(ARCADEMIA_LEADERBOARDS_STATIC)
    #define ARCADEMIA_API
  #else
    #define ARCADEMIA_API __declspec(dllimport)
  #endif
#else
  #define ARCADEMIA_API
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define ARCADEMIA_MODE_SANDBOX 0
#define ARCADEMIA_MODE_LAUNCHER 1

#define ARCADEMIA_SCOPE_LOCAL 0
#define ARCADEMIA_SCOPE_INSTITUTIONAL 1
#define ARCADEMIA_SCOPE_COUNTRY 2
#define ARCADEMIA_SCOPE_GLOBAL 3

ARCADEMIA_API void arcademia_leaderboards_init(void);
ARCADEMIA_API void arcademia_leaderboards_shutdown(void);
ARCADEMIA_API int arcademia_leaderboards_mode(void);

ARCADEMIA_API void arcademia_leaderboards_configure(const char* api_base, const char* api_key);

ARCADEMIA_API const char* arcademia_leaderboards_ping(void);

ARCADEMIA_API const char* arcademia_leaderboards_submit_score(
    const char* board_slug,
    long long value,
    const char* player_name,
    const char* metadata_json,
    const char* score_id);

ARCADEMIA_API const char* arcademia_leaderboards_get_test_scores(
    const char* board_slug,
    int limit,
    int offset);

ARCADEMIA_API const char* arcademia_leaderboards_get_scores(
    const char* board_slug,
    int scope,
    const char* ranks,
    const char* player_score_id,
    int before,
    int after,
    int best_per_player);

ARCADEMIA_API const char* arcademia_leaderboards_request_claim(const char* score_id);

ARCADEMIA_API void arcademia_leaderboards_free(const char* ptr);

#ifdef __cplusplus
}
#endif

#endif
