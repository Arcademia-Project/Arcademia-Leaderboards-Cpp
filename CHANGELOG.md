# Changelog

All notable changes to the Arcademia Leaderboards SDK for C++ are
documented here. The format follows
[Keep a Changelog](https://keepachangelog.com/en/1.1.0/).

## [1.0.0] - 2026-09-22

### Added
- Flat C ABI: `arcademia_leaderboards_init/shutdown/mode/configure`, `_ping`, `_submit_score`, `_get_test_scores`, `_request_claim`, `_free`.
- Windows x64 shared library, built on WinHTTP (sandbox HTTPS) and Win32 named pipes (launcher mode), same wire protocol as the Unity and .NET SDKs.
- `arcademia.json` config file support (read from next to the built executable), same schema as the other SDKs.
- CMake build producing `arcademia_leaderboards.dll` plus a `quickstart` console sample.
