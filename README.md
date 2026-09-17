<img width="1200" height="350" alt="s2x-readme-banner" src="https://github.com/user-attachments/assets/733e21a4-2703-4ba7-ab07-151b0f17e94a" />

# S2x ⭐

> [!WARNING]
> S2x is actively being developed, and several features are already implemented, but the project is not yet feature-complete.
>
> Expect bugs, crashes, missing features, and general instability.
>
> Dedicated servers, modding support, online functionality, and further gameplay stability are still in development.

S2x is a custom client project for **Call of Duty®: WWII**, focused on preserving and extending functionality for campaign, multiplayer, and zombies.

Join the S2x community on [Discord](https://discord.gg/wdC8Jpc2cC).

The project is inspired by the work of the former XLabs community, but S2x is an independent project and is not affiliated with XLabs, Activision, Sledgehammer Games, Microsoft, or any related publisher, developer, or trademark holder.

### Headquarters economy

Headquarters balances, inventory, Orders, contracts, Mail and reward receipts are saved in `players2/user/hq_economy.json`; `hqeconomy reload` in the console reloads it. If two game instances share one profile, each sees the other's saved changes only after its own next successful economy change or an `hqeconomy reload`; the file lock prevents lost writes. The receipt ledger holds at most 10,000 entries and is never pruned, so after enough play new claims, purchases and payroll stop saving and the console says so once. Deleting `players2/user/hq_economy.json` with every instance closed resets the Headquarters economy and nothing else.

## Requirements

You must own a legitimate Steam copy of **Call of Duty®: WWII** to use S2x.

S2x does **not** provide game files, cracked executables, or any method to obtain the game without purchasing it. Please support the original developers and publishers by owning a legal copy of the game.

## Compile from source code

- Clone the Git repository using [Git](https://git-scm.com/install/windows) or [GitHub Desktop](https://desktop.github.com/download/). **Do not download it as a ZIP**, as that will not include the required submodules.
- Run the `generate.bat` script to generate the project solution.
- Build the project using the generated solution file at `build\s2x.sln`.

## Combat Training

Start Multiplayer, open the console with the tilde/backtick key and load a map with a
gametype:

```text
map mp_shipment_s2 dom
```

Once the map has loaded, add bots:

```text
spawnBot 6
```

`bot_fill` does that on every map start, so changing map does not mean retyping
`spawnBot`. It is saved with your profile:

```text
bot_fill 6
map mp_shipment_s2 war
```

Set `bot_fill 0` to disable it again. The value is a number of bots from 0 to the
multiplayer player limit of 18; a value outside that range is rejected and the previous
one is kept. Bots count toward the match's player limit, and the console reports how many
the engine actually added. Progression works in these matches.

One trap worth knowing: a dedicated server started from this game folder reads the same
profile, so `+set bot_fill N` there also changes the value for your own matches. Set it
back to `0` afterwards if you do not want bots in them.

## Modding: loose file overrides

S2x loads loose files from `%LOCALAPPDATA%\s2x\data\` and `<game folder>\s2x\` before the packaged game assets.

- **GSC scripts**: `scripts\mp\*.gsc` (multiplayer and zombies) or `scripts\sp\*.gsc` (campaign), plus `scripts\mp\<mapname>\` and `scripts\mp\<gametype>\` subfolders.
- **UI scripts**: `ui_scripts\mp\<folder>\__init__.lua` or `ui_scripts\sp\<folder>\__init__.lua`.
- **String tables (`.csv`)**: place the file at the asset path, for example `%LOCALAPPDATA%\s2x\data\mp\botDivisionTable.csv` replaces `mp/botDivisionTable.csv`. Tables that do not exist in the game can be added the same way and read from GSC with `tablelookup`.

Console commands for string tables:

- `dumpstringtable mp/botDivisionTable.csv` exports the loaded table to `<game folder>\s2x\dump\mp\botDivisionTable.csv`. Edit the copy and move it into one of the loose file folders above.
- `listassetpool 59 <filter>` lists the names of loaded string tables.
- `reloadstringtables` drops the cached loose tables; edited files are also picked up automatically when the next map loads.

Loose tables are plain CSV: a newline ends a row, a comma ends a cell, and a cell that starts with a double quote is read RFC 4180 style (`""` for a literal quote), which is how `dumpstringtable` writes them. A loose table is used only when it is at most 8 MiB, has at most 65,535 rows and 1,024 columns, and pads to at most 1,048,576 cells; an empty file, a file with an unclosed quote, or one outside those limits is reported in the console and the packaged table is used instead.

## Credits

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
