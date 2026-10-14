# S2x-CR upstream rebuild candidate

This branch rebuilds CR on Brentdevent/S2x master `0048253` (2026-10-12). It is a candidate for validation, not an installed upgrade. The existing CR `integration` branch and deployed servers are unchanged.

The candidate retains CR hosting: bot fill and name pools, capped match slots, local Server Manager administration/notices, script file storage/chat/roster APIs, one-shot map selection and map voting, hosted score-limit mirroring, loose CSV string tables, Server Manager, launch profiles, auto-balance and status scripts. It retains upstream's raw UDP networking, custom sessions, RmlUi launcher, improved browser, rcon and IW4MAdmin support.

## Economy decision

The candidate uses one upstream economy backend. Existing CR stores have not been converted or replaced. Read [the comparison](docs/UPSTREAM-ECONOMY-COMPARISON.md) before considering a client upgrade: CR weapon contracts/Orders, several MP rank/prestige grants and Zombies bonus Orders are absent upstream, and `hq_economy.json` is not imported. Never rename that file to `marketplace.json`.

## Build

Initialize the pinned submodules, run `tools/premake5.exe vs2022`, then build `build/s2x.sln` in Debug and Release x64 with Visual Studio 2022. Release output is `build/bin/x64/Release/s2x.exe` and its matching PDB. Build the Manager with `msbuild tools/ServerManager/S2xServerManager.csproj /restore /p:Configuration=Release`.

## Candidate testing and installation boundaries

Use a separate game/test folder and fresh loose files from this branch. The client normally loads scripts from both `<executable folder>/s2x` and `%LOCALAPPDATA%/s2x/data`; LUI initialization can execute folders from both roots. Copying a new executable over an old CR installation does not remove obsolete Lua wrappers.

For isolated testing, `-appdata-dir <absolute directory>` replaces the entire S2x AppData root, including loose data, caches and script storage. It does not select a different game profile: use a separate working/game folder for dedicated testing, or a new named `-profile <name>` for client testing. Both roots must be considered. Existing game-local mods need an inventory and backup before a later installation; this candidate performs no cleanup or migration.

Updates remain opt-in (`-update`; `-noupdate` takes precedence). The update service still serves upstream builds, so do not use it to refresh a CR candidate. CR loose files are never pruned by an upstream update manifest. Obtain matching CR binaries/data/tools as one package.

## Hosting

Use `sv_maxplayers` and `sv_minplayers`, `bot_fill` and `bot_names` (`default`, `modern`, `nostalgia`). Server scripts set `s2x_nextmap "<map> <gametype>"`; an operator's pending `map` or `sv_mapRotationCurrent` wins over a vote. `s2x_nextmap_preview` describes the pending choice and `s2x_nextmap_api` reports availability.

The [Server Manager guide](tools/ServerManager/README.md) and [script API guide](tools/server-scripts/README.md) describe fleet operation. Manager/status probes support the current raw packet format and validated legacy fallbacks. Current `clients` includes bots; human counts subtract `bots`.

See [candidate validation](docs/UPSTREAM-REBUILD-VALIDATION.md) for checks and runtime limits. No existing economy, profile or deployed server is replaced by this branch.
