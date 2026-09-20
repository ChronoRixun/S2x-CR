<img width="1200" height="350" alt="s2x-readme-banner" src="https://github.com/user-attachments/assets/733e21a4-2703-4ba7-ab07-151b0f17e94a" />

# S2x ⭐ — ChronoRixun's fork

This is a personal fork of [Brentdevent/S2x](https://github.com/Brentdevent/S2x), the custom client for **Call of Duty®: WWII**. It carries fixes and features on top of upstream `master`, merged together on the `integration` branch. If you want everything at once, build `integration`.

> [!WARNING]
> S2x is actively being developed and is not feature-complete. Expect bugs, crashes, missing features, and general instability. Dedicated servers, modding support, online functionality, and further gameplay stability are still in development.

S2x is a custom client project for Call of Duty®: WWII, focused on preserving and extending functionality for campaign, multiplayer, and zombies. The project is inspired by the work of the former XLabs community, but S2x is an independent project and is not affiliated with XLabs, Activision, Sledgehammer Games, Microsoft, or any related publisher, developer, or trademark holder.

## What this fork adds

| Branch | What it does | Upstream |
|---|---|---|
| `fix/44-rotation-defaults` | Admin gameplay settings survive dedicated map rotations | [#44](https://github.com/Brentdevent/S2x/issues/44) |
| `fix/lobby-party-slot-bound` | Fixes a dedicated-server crash in the lobby party walk | crash found while testing #44 |
| `fix/console-long-line` | A console line over 4 KB is truncated instead of ending the process | crash found while tracing #44 |
| `fix/scheduler-drop-throwing-tasks` | A scheduled task that throws is contained and dropped instead of taking the game down | found during the #39 review |
| `feat/22-bot-fill` | `bot_fill`: bots added automatically on every map start | [#22](https://github.com/Brentdevent/S2x/issues/22) |
| `feat/52-stringtable-override` | Loose `.csv` string-table overrides, `dumpstringtable`, `reloadstringtables` | [#52](https://github.com/Brentdevent/S2x/issues/52) |
| `feat/48-rank-commands` | `setrank` / `setprestige` console commands | [#48](https://github.com/Brentdevent/S2x/issues/48) |
| `feat/48-rank-menu` | Prestige and Rank chooser in the UNLOCKS tab (stacked on the commands) | [#48](https://github.com/Brentdevent/S2x/issues/48) |
| `feat/53-zombies-unlock-menu` | Groesten Haus toggle and `unlockzmeastereggs` in the UNLOCKS tab | [#53](https://github.com/Brentdevent/S2x/issues/53) |
| `fix/53-zombies-progression-only` | Tortured Path chapter tracking and main-quest progression recording | [#53](https://github.com/Brentdevent/S2x/issues/53) |
| `feat/39-hq-economy` | Headquarters economy: Orders, contracts, payroll, supply drops, Quartermaster, Mail | [#39](https://github.com/Brentdevent/S2x/issues/39) |
| `feat/39a` | Expanded daily and weekly order pools (20 daily, 10 weekly) | extends #39 |
| `feat/bot-names` | Custom bot name pools: default, modern (2016-2026), nostalgia (2005-2015) | — |
| `feat/server-launcher` | Dedicated server launcher GUI | — |

### Dedicated server settings that stick

On upstream, `scr_dom_scorelimit` and friends set from the command line or `server.cfg` lasted exactly one map. The cause is that the engine's lobby code runs `default_xboxlive.cfg`, and through it the 497 gameplay defaults in `default_mp_allmodes.cfg`, every time the party is created, a match ends and the lobby returns: seven times per rotation, wiping whatever the admin set. That file exists to be overridden by the playlist rules afterwards, and a dedicated server has no playlist. So this fork executes it once at startup, ahead of the server config, and skips it from then on. Nothing resets the values, so nothing has to restore them.

### Rank and prestige

`setrank <level> [prestige]` and `setprestige <prestige>` write the prestige and the rank's minimum XP from the game's own rank tables, in Multiplayer and in Zombies. The UNLOCKS tab of the Soldier menu gains a Prestige and Rank group with steppers and an Apply action behind a confirmation. The stats are written the same way `unlockstatsmp` writes them; the engine uploads them a few seconds later or at the next map load, so load any map before quitting if you want the rank to stick. The prestige is a stat write rather than a call into the game's own prestige routine, which advances by exactly one prestige in Multiplayer only and whose reward call is an empty function in this build. The command says so after a successful write.

### Zombies progression

A saved `cg_unlock_zm_progression` toggle (also an UNLOCKS row) makes the tutorial map Groesten Haus available. Tortured Path chapters, the DLC3 survival unlock, the Easter eggs and the red skull are recorded into the persisted achievements from the game's own reward events, including for remote players on a listen or dedicated server. `unlockzmeastereggs confirm` marks the main-quest achievements complete outright.

### Headquarters economy

Orders, contracts, payroll, supply drops, the Quartermaster and Mail all run over the Achievement Engine protocol that upstream stubs. This fork answers those requests from a local economy store (`players2/user/hq_economy.json`), with retail-shaped Orders, nine contracts priced in Armory Credits, payroll, supply drops that open, and a Quartermaster whose purchases are usable in Create-a-Class. It is the largest branch and is offered upstream as a draft. The Zombies Supplies screens are out of scope for now.

Headquarters balances, inventory, Orders, contracts, Mail and reward receipts are saved in `players2/user/hq_economy.json`; `hqeconomy reload` in the console reloads it. Deleting `players2/user/hq_economy.json` with every instance closed resets the Headquarters economy and nothing else.

### Bot names

Bots can use one of three name pools instead of the engine's stock names. The saved dvar `bot_names` selects the pool:

- `default` — the engine's built-in names (unchanged)
- `modern` — 54 names in 2016-2026 gamertag style (e.g. ColdPulse, Havoc04, softlock, TacoTuesday44)
- `nostalgia` — 54 names in 2005-2015 Xbox 360 era style (e.g. xXDarkAngelXx, N00bSl4y3r, CrimsonEagle47, IEatBullets)

The pool is shuffled on each map load, so with 54 names and at most 18 players per match, each game sees a different mix. Set it in the server console or `server.cfg`:

```text
bot_names nostalgia
```

### Dedicated server launcher

`tools/server-launcher.ps1` is a GUI for launching a dedicated server without writing configs by hand. It lets you pick a server name, build a map rotation from dropdowns, set per-gametype score limits, single-round domination, bot fill, bot names, and port. It writes `server.cfg` and launches the server with one click.

```text
powershell -ExecutionPolicy Bypass -File tools\server-launcher.ps1
```

## Requirements

You must own a legitimate Steam copy of **Call of Duty®: WWII** to use S2x. S2x does **not** provide game files, cracked executables, or any method to obtain the game without purchasing it.

## Compile from source code

- Clone the repository with [Git](https://git-scm.com/install/windows) or [GitHub Desktop](https://desktop.github.com/download/). **Do not download it as a ZIP**, as that will not include the required submodules.
- Check out the branch you want: `integration` for everything, or one of the branches above for a single change.
- Run `generate.bat` to generate the project solution, then build `build\s2x.sln` (Release, x64).
- Copy `build\bin\x64\Release\s2x.exe` (and the `.pdb` if you want readable crash reports) into the game folder, and the Lua patches from `data\ui_scripts\` into `<game folder>\s2x\ui_scripts\`.

## How it is tested

Every branch is built into `integration`, installed and exercised in the game: dedicated servers through full rotations with bots and a connected client, the console commands with their error paths, the menus in Multiplayer and Zombies, and the persisted files afterwards.

## Combat Training

Start Multiplayer, open the console with the tilde/backtick key and load a map with a gametype:

```text
map mp_shipment_s2 dom
```

Once the map has loaded, add bots:

```text
spawnBot 6
```

`bot_fill` does that on every map start, so changing map does not mean retyping `spawnBot`. It is saved with your profile:

```text
bot_fill 6
map mp_shipment_s2 war
```

Set `bot_fill 0` to disable it again. The value is a number of bots from 0 to the multiplayer player limit of 18; a value outside that range is rejected and the previous one is kept. Bots count toward the match's player limit, and the console reports how many the engine actually added. Progression works in these matches.

One trap worth knowing: a dedicated server started from this game folder reads the same profile, so `+set bot_fill N` there also changes the value for your own matches. Set it back to `0` afterwards if you do not want bots in them.

## Modding: loose file overrides

S2x loads loose files from `%LOCALAPPDATA%\s2x\data\` and `<game folder>\s2x\` before the packaged game assets.

- **GSC scripts**: `scripts\mp\*.gsc` (multiplayer and zombies) or `scripts\sp\*.gsc` (campaign), plus `scripts\mp\<mapname>\` and `scripts\mp\<gametype>\` subfolders.
- **UI scripts**: `ui_scripts\mp\<folder>\__init__.lua` or `ui_scripts\sp\<folder>\__init__.lua`.
- **String tables (`.csv`)**: place the file at the asset path under either search root — `%LOCALAPPDATA%\s2x\data\mp\botDivisionTable.csv` or `<game folder>\s2x\mp\botDivisionTable.csv` replaces `mp/botDivisionTable.csv`. Tables that do not exist in the game can be added the same way and read from GSC with `tablelookup`.

Console commands for string tables:

- `dumpstringtable mp/botDivisionTable.csv` exports the loaded table to `<game folder>\s2x\dump\mp\botDivisionTable.csv`. Edit the copy and move it into one of the loose file folders above.
- `listassetpool 59 <filter>` lists the names of loaded string tables.
- `reloadstringtables` drops the cached loose tables; edited files are also picked up automatically when the next map loads.

Loose tables are plain CSV: a newline ends a row, a comma ends a cell, and a cell that starts with a double quote is read RFC 4180 style (`""` for a literal quote), which is how `dumpstringtable` writes them. A loose table is used only when it is at most 8 MiB, has at most 65,535 rows and 1,024 columns, and pads to at most 1,048,576 cells; an empty file, a file with an unclosed quote, or one outside those limits is reported in the console and the packaged table is used instead.

The search paths in use are printed once at startup as an `[FS]` line.

## Credits

- [Brentdevent](https://github.com/Brentdevent) and the S2x contributors - the upstream project this fork builds on.
- [momo5502](https://github.com/momo5502) - Former lead developer of [XLabsProject](https://github.com/XLabsProject), research, codebase, and Sogen.
- [Auroramod](https://github.com/auroramod) - Multiple components, int 2d patching, and Demonware emulation.
- [Mallgrab](https://github.com/mallgrab/CWHook) - Cold War Arxan research.
- [Wanted](https://github.com/WantedDV) - `find_lea_target` helper and feedback on VirtualAlloc hook research.

## Disclaimer

S2x is an independent community project created for educational, research, preservation, and interoperability purposes.

This project is not affiliated with, endorsed by, sponsored by, or approved by Activision Publishing, Inc., Sledgehammer Games, Microsoft, Steam, Valve, XLabs, or any related companies.

Call of Duty, Call of Duty: WWII, and all related names, logos, assets, and trademarks are property of their respective owners.

S2x does not include or distribute copyrighted game files. Users are required to own a legitimate copy of the game.

The maintainers are not responsible for misuse of this software. Use responsibly.
