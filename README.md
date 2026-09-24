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
| Claiming | Shows a QR code on the cabinet | Gives you a link to open in your browser instead of a QR code |

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
  default to `"Player"`). Server-side profanity filtering applies. Pass
  `nullptr` if the player might claim the score instead (see *Typed name
  or account* below).
- `metadata_json`: an optional JSON object string (max 2 KB, pass
  `nullptr` to omit), e.g. `"{\"level\":\"3-2\"}"`. It's stored with the
  score and comes back as `Metadata` on every score your game reads, so
  you can show things like the level or character next to each entry.
- `score_id`: pass `nullptr` normally, a GUID gets generated for you.
  Supplying your own lets you safely retry a submission (say, after a
  network blip) without creating a duplicate, since the server
  deduplicates by this id.

The returned JSON's `Status` field is one of `"submitted"`, `"queued"`,
`"rejected"`, or `"error"`. If the player's offline on the machine, the
launcher queues the score and flushes it once connectivity returns, so
treat `"queued"` the same as `"submitted"`.

Keep the returned `ScoreId` if you plan to offer a claim next.

### Typed name or account

Once a score is submitted, the player can put a name on it in one of two
ways. Offer them the choice and use whichever they pick:

1. **Type a name in your game.** Pass it to `submit_score`, or call
   `set_player_name` afterwards if you submitted first.
2. **Save it to their Arcademia account.** Call `request_claim`. The
   player scans a QR code on the cabinet and signs in on their phone, and
   the result's `PlayerName` is their account's username so your game can
   show it.

They don't need to do both. A claimed score always shows the account's
username. If the player cancels the QR code or it times out, the score
is still there under `"Player"`, so you can fall back to your own name
entry and call `set_player_name`.

When you read scores back, each row's `Claimed` field is `true` when
`PlayerName` is a verified Arcademia username, and `false` when it's a
name someone typed in a game.

### `const char* arcademia_leaderboards_set_player_name(const char* score_id, const char* player_name)`
Sets or changes the name on a score submitted earlier in the same play
session. The same name rules apply as for `submit_score`.

```json
{ "Success": true, "Status": "saved", "ScoreId": "...", "PlayerName": "MAL", "Message": null, "Mode": "Launcher" }
```

`Status` is `"saved"`, `"queued"` (the cabinet is offline and the score
hasn't uploaded yet, the new name will go with it), `"rejected"` or
`"error"`. Claimed scores can't be renamed.

### `const char* arcademia_leaderboards_request_claim(const char* score_id)`
Lets the player save a score to their Arcademia account instead of typing
a name. On a cabinet, the launcher shows a QR code. The call blocks until
the player scans it, cancels, or about five minutes pass, so call it from
a worker thread or a prompt handler rather than your main loop.

```json
{ "Success": true, "Status": "saved", "ScoreId": "...", "PlayerName": "Malphatt", "Message": null, "Mode": "Launcher" }
```

`Status` is `"saved"`, `"cancelled"`, `"expired"`, `"rejected"`,
`"offline"` or `"error"`. When it's `"saved"`, `PlayerName` is the
player's Arcademia username.

In sandbox mode there's no cabinet to show a QR code on, so you get a
link instead. It's written to stderr and the debugger output, and passed
to your callback if you've set one. Open it in your browser, sign in and
save the score, and the call returns just like it would on a cabinet.

### `void arcademia_leaderboards_set_claim_link_callback(arcademia_leaderboards_claim_link_callback callback, void* user_data)`
Sets a function to receive the sandbox claim link, e.g. to show it on
screen. It's called on the thread that called `request_claim`. Pass
`nullptr` to clear it.

```cpp
static void OnClaimLink(const char* claim_url, void* user_data)
{
    std::cout << "Claim it here: " << claim_url << std::endl;
}

arcademia_leaderboards_set_claim_link_callback(OnClaimLink, nullptr);
```

### `const char* arcademia_leaderboards_get_scores(const char* board_slug, int scope, const char* ranks, const char* player_score_id, int before, int after, int best_per_player)`
Loads a leaderboard to show in your game. You decide what comes back:
which group of players to rank against, which positions to include, and
whether to include the current player's own position with the players
either side of them.

- `scope`: one of `ARCADEMIA_SCOPE_LOCAL` (this cabinet),
  `ARCADEMIA_SCOPE_INSTITUTIONAL` (every cabinet at the same
  institution), `ARCADEMIA_SCOPE_COUNTRY` (every cabinet in the same
  country) or `ARCADEMIA_SCOPE_GLOBAL`. The cabinet, institution and
  country are worked out on the server from the play session, so there's
  nothing to pass in for them.
- `ranks`: which positions to return, e.g. `"1-10"` or `"1-3,10,50-55"`.
  `"none"` skips the ranked list. `NULL` gives you the top of the board
  up to its display cap. A request can cover up to 200 positions.
- `player_score_id`: the `ScoreId` from `submit_score`, to find the
  player's own position. `NULL` to skip.
- `before` / `after`: how many scores either side of the player to
  include (0 to 50).
- `best_per_player`: `1` ranks each player's best score, `0` ranks every
  submission.

```cpp
const char* json = arcademia_leaderboards_get_scores(
    "highscore", ARCADEMIA_SCOPE_COUNTRY, "1-10", last_score_id, 2, 2, 1);
arcademia_leaderboards_free(json);
```

```json
{
  "Success": true, "Mode": "Launcher", "Scope": "Country", "BestPerPlayer": true,
  "BoardSlug": "highscore", "BoardName": "High Score", "Total": 118,
  "Scores": [ { "Rank": 1, "PlayerName": "REX", "Value": 99999, "AchievedAt": "...", "Claimed": false, "IsPlayer": false, "MachineName": "Bartik", "SiteName": "University of Lincoln", "Country": "United Kingdom", "Metadata": "{\"level\": \"3-2\"}" } ],
  "Player": { "Rank": 42, "PlayerName": "MAL", "IsPlayer": true, "...": "..." },
  "Around": [ "ranks 40 to 44, in order" ]
}
```

`Player` is `null` if you didn't pass a score id or the score isn't in
that scope. `Metadata` is the JSON object you submitted with the score,
as a string (the server may tidy up the spacing), or `null` if there was
none. Parse it with the same JSON library you read the result with. In
best-per-player mode it comes from each player's best run. It's only
returned to your game, never shown on the public leaderboard pages. For a screen with a tab per scope, call it once per scope.
In sandbox mode it reads your test scores, and every scope returns the
same list because test scores don't come from a cabinet.

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
