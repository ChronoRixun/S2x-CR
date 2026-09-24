<img width="1200" height="350" alt="s2x-readme-banner" src="https://github.com/user-attachments/assets/733e21a4-2703-4ba7-ab07-151b0f17e94a" />

# S2x-CR

A fork of [Brentdevent/S2x](https://github.com/Brentdevent/S2x), the custom client for **Call of Duty®: WWII**, kept by [ChronoRixun](https://github.com/ChronoRixun). It exists so a small community can run dedicated servers that stay up, fill with bots until people arrive, and keep progression working. Everything here is merged on the `integration` branch and shipped as releases.

> [!WARNING]
> S2x is actively being developed and is not feature-complete. Expect bugs, crashes, missing features, and general instability. Dedicated servers, modding support, online functionality, and further gameplay stability are still in development.

S2x is a custom client project for Call of Duty®: WWII, focused on preserving and extending functionality for campaign, multiplayer, and zombies. The project is inspired by the work of the former XLabs community, but S2x is an independent project and is not affiliated with XLabs, Activision, Sledgehammer Games, Microsoft, or any related publisher, developer, or trademark holder.

## Get it

1. Own **Call of Duty®: WWII** on Steam. S2x does not include game files.
2. Download the latest zip from [Releases](https://github.com/ChronoRixun/S2x-CR/releases).
3. Extract it into your game folder, next to `s2_mp64_ship.exe`.
4. Start Steam, then run `s2x.exe`.

| In the zip | What it is |
|---|---|
| `s2x.exe`, `s2x.pdb` | The client, and symbols for readable crash reports |
| `s2x\ui_scripts\` | Menu patches: server browser, dedicated lobby, rank and Zombies unlocks |
| `s2x\scripts\mp\` | Server-side scripts: Gun Game bot costumes, chat events for server scripting |
| `s2x\tools\` | The dedicated server launcher and the Discord status card script |

## Community

- **CoD WWII Community** on Discord: <https://discord.gg/yMPWMTyPPZ>. Our servers, looking-for-game, and the place to report anything that's wrong with them.
- **S2x** on Discord: <https://discord.gg/wdC8Jpc2cC>. The upstream project's server, run by Brentdevent.
- Bugs and ideas for this fork go in [Issues](https://github.com/ChronoRixun/S2x-CR/issues).
- The [showcase](https://chronorixun.github.io/S2x-CR/) is a screenshot tour of what the fork adds.

## What this fork adds

Where an item carries a status, it says where the item stands upstream: **Fork only**; **Ready to offer**, where a branch shaped for upstream is waiting on [ChronoRixun/S2x-upstream](https://github.com/ChronoRixun/S2x-upstream) for its pull request; or **Offered upstream**, with the pull request linked. Once an item is merged into [Brentdevent/S2x](https://github.com/Brentdevent/S2x), it comes off this list.

### Dedicated servers

- **Ready to offer:** startup crash fixed. Dedicated servers died on roughly one launch in ten with `0xC0000409`. An Arxan repair guard that the client did not patch was restoring three regions the client relies on; it is now filtered like the others. ([#2](https://github.com/ChronoRixun/S2x-CR/issues/2))
- **Ready to offer:** final killcams play smoothly. The slow-motion stretch never slowed a dedicated server's clock, so the replay skipped frames; the server now slows with it. ([#14](https://github.com/ChronoRixun/S2x-CR/issues/14))
- **Ready to offer:** podium emotes play. The dedicated lobby re-requested every loadout on each update, which rebuilt the podium soldiers and cut the emote short. ([#15](https://github.com/ChronoRixun/S2x-CR/issues/15))
- **Ready to offer:** Gun Game bots keep their bodies. Gun Game rebuilt bot outfits from profile data bots don't have, leaving them legless. A server-side script restores the generated uniform after each weapon change. ([#4](https://github.com/ChronoRixun/S2x-CR/issues/4))
- **Ready to offer:** a console log per server. `+set g_consoleLog <path>` on the command line now takes effect, so several servers from one folder no longer share `s2x\logs\console.log`.
- **Ready to offer:** chat for server scripts, `level waittill("say", player, message, team_chat)`. **Fork only:** the rest of [server scripting](tools/server-scripts/README.md): small text files, a player's address and a roster snapshot. ([#9](https://github.com/ChronoRixun/S2x-CR/issues/9))
- **Fork only:** bots that fill the server. `bot_fill 17` adds bots on every map start; they make room as people join. `bot_names` picks a name pool: `default`, `modern` (2016-2026 gamertags) or `nostalgia` (2005-2015 Xbox 360 era). Both are saved dvars.

### Progression

- **Ready to offer:** rank and prestige. `setrank <level> [prestige]` and `setprestige <prestige>` in Multiplayer and Zombies, plus a Prestige and Rank chooser in the UNLOCKS tab. Past reward XP is rebaselined so the requested level is the level you get. ([#7](https://github.com/ChronoRixun/S2x-CR/issues/7))
- **Ready to offer:** Zombies progression. A saved toggle unlocks Groesten Haus; Tortured Path chapters, the DLC3 survival unlock, the Easter eggs and the red skull are recorded from the game's own reward events, including for remote players. `unlockzmeastereggs confirm` completes the main quest outright.
- **Fork only:** Local Play bots. `bot_fill` works in Local Play too, and `spawnBot 2` adds more mid-match. ([#1](https://github.com/ChronoRixun/S2x-CR/issues/1))

### Headquarters economy

Upstream stubs the Achievement Engine, so Orders, contracts, payroll, supply drops, the Quartermaster and Mail did nothing. This fork answers those requests from a local store (`players2/user/hq_economy.json`):

- Retail-shaped Orders and contracts priced in Armory Credits, payroll, and supply drops that open with the full card flip. Every soldier level-up awards a Rare Supply Drop, as the end-of-match screen promises.
- Duplicate cards convert to Armory Credits at the game's own pawn values, in Multiplayer and Zombies. ([#6](https://github.com/ChronoRixun/S2x-CR/issues/6))
- Zombies has its own rotation: 20 daily and 7 weekly orders (six and three offered at a time) and eight timed contracts, tracked from the game's kill events. Rare Zombie Supply Drops reward consumables like Self-Revives and Elektromagnet alongside cosmetics.
- `hqeconomy reload` reloads the store; deleting the file with every instance closed resets the economy and nothing else.

### Modding

Loose GSC and UI scripts are upstream's: the client loads `scripts\` (with its per-map and per-gametype folders) and `ui_scripts\` from `%LOCALAPPDATA%\s2x\data\` and `<game folder>\s2x\`, and loose UI scripts add to the packaged ones rather than replacing them. Put your own files in `<game folder>\s2x\`, which no updater writes to: since [3163800](https://github.com/Brentdevent/S2x/commit/31638003e9ee2648cc415f55e18a6898233c6636), upstream's updater deletes every file under `%LOCALAPPDATA%\s2x\data\` that is not in its update manifest each time it runs. This fork's updater runs only with `-update` and deletes nothing, but launching upstream's client on the same PC still clears that folder.

- **Ready to offer:** string tables from the same folders, at their asset path (`mp\botDivisionTable.csv` replaces `mp/botDivisionTable.csv`). `dumpstringtable <name>` exports a loaded table to `s2x\dump\`, `reloadstringtables` drops the cache, and `listassetpool 59 <filter>` lists what is loaded. Loose tables follow RFC 4180 quoting and are capped at 8 MiB, 65,535 rows and 1,024 columns; anything outside that is reported and the packaged table is used.

## Hosting a server

`tools\S2xServerManager.exe` runs a fleet from one window: a card per preset and port, start and stop, an editor for presets, a console drawer with each server's own log, a roster, a tray icon and connect lines to copy. It shares the launcher's presets and finds the game folder the same way; its own [README](tools/ServerManager/README.md) covers the switches and the build.

`tools\server-launcher.ps1` is a GUI for running a dedicated server without writing configs; `server-launcher.cmd` opens it with a double-click and no console window left behind. A mode toggle switches between Multiplayer and Zombies, each with its own maps, settings and saved presets. It handles the server name (with `^0`–`^7` colour codes and a live preview), the rotation, per-gametype score limits, bot fill and names, and the port. Maps that need a DLC pack are labelled so a rotation can stick to what everyone owns.

```text
powershell -ExecutionPolicy Bypass -File s2x\tools\server-launcher.ps1
```

It finds the game folder on its own (Steam registry, its own location, or a remembered choice) and works on a box without Steam if it is run from the game folder or given `-GameDir`. Forward the server's UDP port (the launcher defaults to 27016) and the server appears in everyone's browser through the master list; nothing else needs registering.

`tools\server-status.ps1` keeps a Discord embed current with what the box's servers are doing: one entry per server with map and mode, players, its rotation and whether the master list has it. It finds the servers the launcher started from the same folder on its own, queries them the way the client does and rewrites only its own fields on an existing bot message, so the rest of the card stays as you wrote it. `-NameFilter` limits the card to servers whose name matches, for example `\"^\^1CR's\"` for a red CR's; `-Ports` adds servers started some other way. `-Install` registers a scheduled task that runs every two minutes with no window; `-DryRun` shows what it would write; `-Uninstall` removes the task.

```text
powershell -ExecutionPolicy Bypass -File s2x\tools\server-status.ps1 -ChannelId <channel> -MessageId <message> -PublicAddress <public ip> -TokenFile C:\s2x-status\token.txt -Install
```

For server-side scripting, drop `.gsc` files into `s2x\scripts\mp\` on the server. Players need nothing. The [server scripting guide](tools/server-scripts/README.md) covers the chat event, persistence, addresses and the optional Discord arrivals feed.

## Compile from source

- Clone with [Git](https://git-scm.com/install/windows) or [GitHub Desktop](https://desktop.github.com/download/). **Do not download it as a ZIP**; the submodules would be missing.
- Check out `integration`.
- Run `generate.bat`, then build `build\s2x.sln` (Release, x64).
- Copy `build\bin\x64\Release\s2x.exe` into the game folder, and the loose files from `data\` into `<game folder>\s2x\` (`ui_scripts`, `scripts`).

## How it is tested

Everything on `integration` is built and exercised in play before a release: dedicated servers through full rotations with bots and a connected client, the console commands with their error paths, the menus in Multiplayer and Zombies, and the persisted files afterwards. The economy, rank, scripting and storage code also have offline harnesses under `tests\`, and [tests/RUNBOOK.md](tests/RUNBOOK.md) lists the live checks for the current build.

## Contributing

Read [CONTRIBUTING.md](CONTRIBUTING.md) first: it is short, and it is the standard every change here is held to. Security reports go through [SECURITY.md](SECURITY.md), not the issue tracker. Longer guides live in the [wiki](https://github.com/ChronoRixun/S2x-CR/wiki).

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
