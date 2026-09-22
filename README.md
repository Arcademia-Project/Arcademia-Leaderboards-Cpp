# Arcademia Leaderboards SDK (C++)

Submit high scores from your game and let players claim them to their
Arcademia account by scanning a QR code. Your code never has to care
whether it's running on an arcade cabinet or on your own PC, it just
works either way.

This is the native build of the SDK: a flat C ABI shared library, for
anything that isn't Unity or .NET, a custom C++ engine, or any other
language with basic C FFI support (Python's ctypes, Go's cgo, and so on).
If you're building in Unity, use the
[Unity package](https://github.com/Arcademia-Project/ac.arcademia.leaderboards-unity)
instead. If you're on .NET (including Godot's C# scripting), use the
[NuGet package](https://github.com/Arcademia-Project/Arcademia-Leaderboards-DotNet).
Both have the identical API surface described here.

Full platform docs (including the raw HTTP API for other languages):
**https://manager.arcademia.ac/docs**

Windows x64 only for now, since live mode is inherently tied to the
arcade cabinets' named-pipe transport. Sandbox-only builds for other
platforms are a reasonable future addition if there's demand.

## Install

**NuGet (Visual Studio / MSBuild C++ projects)**

```
nuget install Arcademia.Leaderboards.Cpp
```

or add it through Visual Studio's NuGet Package Manager. This wires the
include path, library path, and a post-build DLL copy into your project
automatically, nothing else to configure. Windows x64 only.

**Build from source**

Requires CMake 3.20+ and a C++17 compiler targeting Windows (MSVC or
MinGW-w64 both work, this SDK is built and tested against both, the
published NuGet package specifically is built with MSVC).

```
cmake -S . -B build
cmake --build build --config Release
```

Produces `arcademia_leaderboards.dll` (plus its import library) and a
`quickstart` sample executable. Link against `include/arcademia_leaderboards.h`
and the import library from your own project the way you'd link any
other native DLL.

## Two modes, one API

| | Launcher (Live) | Sandbox |
|---|---|---|
| When | Game was started by the Arcademia launcher on an arcade machine | Anywhere else: your dev machine, a build you're testing, CI |
| Auth | Nothing you set. The launcher and the machine's credentials handle it | Your game's API key |
| Scores land on | The live, public leaderboard | The **Test area** only. Visible to you in the dashboard, never public |
| Claiming | Works, shows a QR popup on the cabinet | Not available (`request_claim` returns `rejected` immediately) |

You don't choose the mode yourself. The library figures it out
automatically by checking for environment variables the launcher sets on
the game process before starting it. Write your gameplay code once and it
behaves correctly in both places.

## How results come back

Every call that returns data gives you a heap-allocated, null-terminated
UTF-8 JSON string, shaped like the type it's named after (`ping` returns
a `PingResult`-shaped object, `submit_score` a `ScoreResult`-shaped one,
and so on, using the exact same field names as the Unity and .NET SDKs'
result objects). Parse it with whatever JSON library your project already
uses, then **always** release it with `arcademia_leaderboards_free`, not
your own language's `free`/`delete`, since the string was allocated
inside this DLL.

## Quick start

```cpp
#include <arcademia_leaderboards.h>
#include <iostream>

void OnGameOver(long long finalScore, const char* playerName)
{
    const char* result = arcademia_leaderboards_submit_score("highscore", finalScore, playerName, nullptr, nullptr);
    std::cout << result << std::endl;
    arcademia_leaderboards_free(result);
}
```

That covers a minimal integration. Everything below is optional.

## Configuration

The SDK needs an **API base URL** and an **API key**. Grab both from your
game's *Leaderboards* tab in the Arcademia dashboard (request access,
then generate a key, there's more detail in the dashboard itself). The
key only matters in sandbox mode. On the machine it's ignored in favour
of the launcher/machine credentials, so **it's safe to ship inside your
build** (see *Shipping the key* below, and the full docs for the full
security explanation).

**Option A: `arcademia.json`** (recommended, since you can change it
without a rebuild)

Place an `arcademia.json` file next to your built executable:

```json
{
  "apiBase": "https://manager.arcademia.ac",
  "apiKey": "arc_xxxxxxxx_xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx"
}
```

This gets read automatically the first time you call anything in the
library. You can leave `apiBase` out entirely to use the default, which
is the live API. You'd only override it if you're pointing at a private
or staging deployment.

**Option B: `arcademia_leaderboards_configure` in code**

```cpp
arcademia_leaderboards_configure("https://manager.arcademia.ac", "arc_xxxxxxxx_...");
```

Call this before anything else if you'd rather set the key at runtime
instead of shipping `arcademia.json`. Calling it on the machine is
harmless too; the key just gets ignored there.

## API reference

### `void arcademia_leaderboards_init(void)`
Explicitly runs the one-time setup (mode detection, config loading).
Optional, every other call does this automatically on first use, this
just lets you control exactly when that happens.

### `void arcademia_leaderboards_shutdown(void)`
Releases the launcher connection, if any, and resets configuration. Call
this once when your game closes.

### `int arcademia_leaderboards_mode(void)`
Returns `ARCADEMIA_MODE_SANDBOX` (0) or `ARCADEMIA_MODE_LAUNCHER` (1).

### `void arcademia_leaderboards_configure(const char* api_base, const char* api_key)`
Sets the API base URL and key at runtime. See *Configuration* above.

### `const char* arcademia_leaderboards_ping(void)`
A sanity check: confirms connectivity and, in sandbox mode, that the API
key is valid. Worth calling once on startup.

```cpp
const char* result = arcademia_leaderboards_ping();
std::cout << result << std::endl;
arcademia_leaderboards_free(result);
```

### `const char* arcademia_leaderboards_submit_score(const char* board_slug, long long value, const char* player_name, const char* metadata_json, const char* score_id)`
Submits a score to the named board.

- `board_slug`: from the dashboard, e.g. `"highscore"` or `"time-trial"`.
- `value`: a whole number. For time-based boards, submit milliseconds,
  the dashboard formats it back for display.
- `player_name`: free text, any characters, up to 32 (pass `nullptr` to
  default to `"Player"`). Server-side profanity filtering applies.
- `metadata_json`: an optional raw JSON object string (max 2 KB, pass
  `nullptr` to omit), e.g. `"{\"level\":\"3-2\"}"`. It gets stored
  alongside the score and isn't shown to players, useful for support or
  anti-cheat review later.
- `score_id`: pass `nullptr` normally, a GUID gets generated for you.
  Supplying your own lets you safely retry a submission (say, after a
  network blip) without creating a duplicate, since the server
  deduplicates by this id.

The returned JSON's `Status` field is one of `"submitted"`, `"queued"`,
`"rejected"`, or `"error"`. If the player's offline on the machine, the
launcher queues the score and flushes it once connectivity returns, so
treat `"queued"` the same as `"submitted"`.

Keep the returned `ScoreId` if you plan to offer a claim next.

### `const char* arcademia_leaderboards_request_claim(const char* score_id)`
Offers a just-submitted live score for the player to save to their
Arcademia account. This shows a QR code popup on the cabinet and won't
return until the player scans it, cancels, or about five minutes pass, so
call it from a "Save my score?" prompt handler rather than your main
loop.

This only really means anything in launcher mode. In sandbox mode it just
returns immediately with `Status = "rejected"`, since there's no cabinet
to show a QR code on and test scores can't be claimed anyway. Feel free
to call it unconditionally.

### `const char* arcademia_leaderboards_get_test_scores(const char* board_slug, int limit, int offset)`
Reads back scores from the sandbox test area, i.e. whatever you or
another dev submitted while not on a cabinet. Only works in sandbox mode
(returns `Success: false` on the machine, since the concept doesn't
apply there). Handy for a debug overlay while you're developing.

### `void arcademia_leaderboards_free(const char* ptr)`
Frees a string returned by any of the calls above. Always call this once
you're done reading the result, never a language-level `free`/`delete`.

## Shipping the key

Your compiled build, including an `arcademia.json` next to it if you're
using one, is something a player (or a curious developer) can open up.
**That's expected, and it's safe.** The API key only grants writes to the
sandbox test area, it can never write to a live leaderboard. A live write
has to come from an arcade machine with an open play session for your
game, verified server-side, which is something a key alone can never
fake, stolen or not. The full explanation, including exactly what a
malicious actor can and can't do with a leaked key, is on the online
docs' Security model page.

## Sample

`samples/quickstart/main.cpp` is a small interactive console demo
exercising every call above, built automatically alongside the library.
Run `build/quickstart.exe` after building.

## Dependencies

Vendors [nlohmann/json](https://github.com/nlohmann/json) (single header,
`third_party/json.hpp`, MIT licensed) for JSON parsing. Everything else
is Win32/WinHTTP, already on every Windows machine, no other third-party
libraries required.
