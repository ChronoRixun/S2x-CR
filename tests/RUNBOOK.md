# S2x-CR release runbook — v1.5.0

**Written:** 2026-09-25 (the v1.3.0 runbook and the v1.4.0 Server Manager section, rewritten for what changed since v1.4.0)
**Build under test:** `integration` at `ebcb45b`, packaged as `v1.5.0-rc1` (everything since `v1.4.0`: the HQ economy fixes, the dedicated HUD limits, Server Manager launch profiles and the Final Reich / Frozen Dawn name swap). Until the tag exists the exe reports itself as `v1.4.0-46-gebcb45b`.
**Repo:** `D:\S2x` — **Game:** `D:\Program Files\Steam\steamapps\common\Call of Duty WWII` — **Box 4:** runs the v1.4.0 zip: two Multiplayer servers, Zombies servers from a separately installed server package, and the status card with `-Ports`
**Budget:** about 105 minutes for sections 0–7 on this PC, then 30 minutes for box 4 and the release

## How to use this

Work top to bottom. Section 0 installs the candidate *the way it ships*; the following sections are ordered by how likely they are to block the release, so the thing most likely to fail is the first thing you look at. Each section says what changed, exactly what to do, what the console should print, and what I already verified versus what has never been run on the shipped build. A section is done when every box is ticked. If something fails, don't fix it on the spot: capture the evidence listed in that section and note it in the results table, so it can be diagnosed from the log rather than from memory.

Where I know the exact console wording it is quoted verbatim from real runs. Where it was never observed, the runbook says "look for a line mentioning ..." instead of inventing text.

Every economy fix below was already live-tested on this profile this week, one branch at a time or in the combined test build. This pass re-checks them on the zip, together, which is what players get.

## What changed since v1.4.0

| Commit | Change | Tested by |
|---|---|---|
| `3df76b5`, `ba4a48d`, `45e657d` | A dedicated server's score, win and round limits reach the client HUD and scoreboard (#12) | section 2 |
| `ae2a46c` | Spare "Any" uniforms trade in for Armory Credits at HQ entry instead of failing every minute (#11) | section 1.1 |
| `abf9019` | Collection rewards redeem (#13) | section 1.3 |
| `c7f55f9` | The LAD contract grants the Rare LAD, which Create-a-Class unlocks (#17) | section 1.3 |
| `a22cea8` | Trade-in values for every pawnable item, not only the drop pool | sections 0.5, 1.1 |
| `234bab9` | Duplicates trade in through task 199 at HQ entry (#18) | section 1.1 |
| `022e523` | Class camos priced one tier up, as retail | section 1.3 |
| `8140dbf` | Orders show a new board each day and week | section 1.5 |
| `8601bcd` | Zombies consumables are used up through task 96 (#10) | section 1.4 |
| `590beca` | Each Multiplayer prestige grants its helmet and calling card, with catch-up at HQ entry (#22) | section 1.1 |
| `42883da` | Drops add the Epic and Heroic weapon variants; a Rare drop's first card is Rare or better (#23) | sections 1.2, 1.4 |
| `1139a53` | Only the newest 256 item-data receipts are kept (#25, card-use receipts still open) | section 1.1 |
| `bd672ba` | The economy harness builds again | offline line below; section 6 |
| `f1c1f74` | README: keep mods in the game folder's `s2x` directory | section 6 |
| `627962c`, `152d0e9` | Status card: The Final Reich and The Frozen Dawn named correctly; Zombies lines stop saying bots fill the server | section 5 |
| `32ec918`, `6e32463`, `def7ac2`, `e4cc562`, `585ab7e`, `6b0966f` | Server Manager launch profiles: register a server package, pick its modes, start and stop its servers | section 3 |
| `69285bd` | The Final Reich and The Frozen Dawn swapped back in the Manager's and the launcher's map tables | section 4 |
| `b21b025` | The two master-server settings are no longer saved, so a server run from the game folder with the master disabled no longer leaves the client's browser empty | section 2, step 7 |
| `d18def5`–`ffc3a5a` | Rename to S2x-CR, README, showcase, SECURITY | nothing to play |

Automated coverage before you start: the economy harness builds and passes again (build `build\s2x.sln` Release x64 first, then `msbuild tests/economy/zombies.vcxproj /p:Configuration=Release /p:Platform=x64` and `.\build\tests\zombies\bin\zombies.exe`; every section prints PASS and it exits 0). It covers the Rare floor and tier weights over 500 openings, the empty-tier re-roll and the item-data receipt bound; each fix branch also passed its own harness (consumables 26/26, uniform pawn 18/18, collection redeem 20/20, prestige 20/20). The Manager builds with 0 warnings, and its launch-profile loader and editor were driven from PowerShell. What none of that proves is anything rendered on screen, the game's own requests at HQ entry, or a server package started from the Manager's window. That is what this runbook is for.

---

# 0. Before you start

**Time:** 15 minutes.

## 0.1 The candidate the way it ships

rc1 is already built and packaged at `D:\S2x\build\release\v1.5.0-rc1\` (`s2x-cr-v1.5.0-rc1.zip` plus its `stage` folder, 28 files, the Server Manager included). Close the game and any local server, then install by copying the stage over the game folder. Testing the zip, not a hand-copied set of files, is the point.

```powershell
$game = 'D:\Program Files\Steam\steamapps\common\Call of Duty WWII'
Copy-Item D:\S2x\build\release\v1.5.0-rc1\stage\* $game -Recurse -Force
```

The shape, unchanged since v1.4.0:

| In the zip | Count |
|---|---|
| `s2x.exe`, `s2x.pdb` | 2 |
| `s2x\tools\`: `server-launcher.ps1`, `ServerLauncher.xaml`, `server-status.ps1`, `server-launcher.cmd` | 4 |
| `s2x\ui_scripts\mp\find_match\*.lua` | 6 |
| `s2x\ui_scripts\mp\patches\*.lua` (`__init__`, `cwl_currency`, `dedicated_gametype`, `dedicated_lobby`, `dedicated_members`, `dedicated_party`, `unlocks`) | 7 |
| `s2x\scripts\mp\`: `s2x_gungame_bots.gsc`, `s2x_server_events.gsc` | 2 |
| `s2x\tools\presets\*.json` (starter presets) | 5 |
| `s2x\tools\`: `S2xServerManager.exe`, `S2xServerManager.exe.config` | 2 |

Only if something is fixed during the pass, rebuild and repackage as the next rc (the packager builds the Server Manager itself):

```powershell
cd D:\S2x
git checkout integration; git pull --ff-only
.\tools\premake5.exe vs2022
& 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe' build\s2x.sln /t:client /p:Configuration=Release /p:Platform=x64 /m /v:minimal
powershell -File build\diagnostics\package-release.ps1 -Version v1.5.0-rc2
```

Two things that have bitten before: `%LOCALAPPDATA%\s2x\data\ui_scripts_off` must stay renamed off (it outranks the game folder), and the `patches` folder must hold all seven files, not just the changed one.

- [ ] `D:\S2x\build\release\v1.5.0-rc1\stage` holds 28 files in the shape above (`(Get-ChildItem D:\S2x\build\release\v1.5.0-rc1\stage -Recurse -File).Count`)
- [ ] Stage copied over the game folder; `s2x.exe` and `s2x\tools\S2xServerManager.exe` carry the rc1 timestamps (2026-09-25 18:18 and 18:19)

## 0.2 Back up the profile and the economy store

This matters more than in any earlier pass: v1.5.0 changes what the store does on every HQ entry (trade-ins, prestige catch-up, receipt pruning), and section 1 spends real drops and consumables. Take the backup before the first launch, and remember that restoring `players2` later also restores the old Armory Credit balance and drop counts.

```powershell
$game = 'D:\Program Files\Steam\steamapps\common\Call of Duty WWII'
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
Copy-Item "$game\players2" "D:\S2x\build\backups\players2-$stamp" -Recurse
```

`players2\user\hq_economy.json` rides along in that copy. Do **not** delete it for this pass; the point of section 1 is what happens to an existing inventory.

- [ ] `players2` backed up to `build\backups\players2-<stamp>`

## 0.3 Launching

Launch the client from a shortcut, with Steam running and signed in: `C:\Users\Owen\Desktop\S2x Development.lnk` (`-noupdate -multiplayer`) and `D:\S2x\build\backups\S2x Zombies Dev.lnk` (`-noupdate -zombies`). `-demonware_debug` is not needed for anything below.

A local dedicated server for section 2, on a port that does not collide with box 4. Its settings go in a cfg, not on the command line: `+set master_server_enable 0` is ignored for that dvar (tested on rc2 and rc3), while `set` in a cfg works, and the leftover `s2x\server.cfg` from earlier sessions would override a command-line rotation. Put this in `<game folder>c-test.cfg`:

```
set sv_hostname "rc test"
set master_server_enable 0
set party_maxplayers 18
set party_matchStartDelay 15
set bot_fill 8
set bot_names nostalgia
set scr_gun_cycleCount 2
set sv_maprotation "gametype gun map mp_shipment_s2 gametype dom map mp_shipment_s2"
```

Then, from a cmd window in the game folder:

```bat
cd /d "D:\Program Files\Steam\steamapps\common\Call of Duty WWII"
start "" s2x.exe -noupdate -dedicated +set net_port 27017 +exec rc-test.cfg +map_rotate
```

The same launch from PowerShell, where `cd /d` and `start ""` do not work:

```powershell
Set-Location 'D:\Program Files\Steam\steamapps\common\Call of Duty WWII'; Start-Process -FilePath '.\s2x.exe' -WorkingDirectory (Get-Location) -ArgumentList '-noupdate -dedicated +set net_port 27017 +exec rc-test.cfg +map_rotate'
```

`master_server_enable 0` keeps the test server off the public list (it is the setting the Manager's "Advertise off" writes into its cfg); section 2 step 7 checks that it no longer sticks to your client.

Start the server before the client, or give it a port other than 27016: a client already running on this PC holds UDP 27016, and a server asked for a taken port moves to the next free one without saying so. To see where a server really is: `Get-NetUDPEndpoint | Where-Object OwningProcess -eq <server pid>`.

Things that look wrong and aren't: `Dedicated party: failed to select match-rules gametype 'dom'` prints on a working server; `status` answers `Server is not running.` until the first match starts; a map load takes 40–50 seconds; the first `bot_fill` line often reads `the engine added 16 of 17` and a second line tops it up.

Reading the console: `<game>\s2x\logs\console.log` is shared by every instance and only ever appended to, so note the line count before a test and read the tail.

```powershell
$log = 'D:\Program Files\Steam\steamapps\common\Call of Duty WWII\s2x\logs\console.log'
$before = (Get-Content $log).Count
# ... test ...
Get-Content $log | Select-Object -Skip $before | Select-String -Pattern 'error|FAILED|FAILURE|exception|assert|HQ economy|HQ AE|HQ vendor|missing task|Hosted dedicated'
```

Reading the store without the game's menus, for section 1 (read only, safe while the game runs):

```powershell
$game = 'D:\Program Files\Steam\steamapps\common\Call of Duty WWII'
function Show-Store {
  $s = Get-Content "$game\players2\user\hq_economy.json" -Raw | ConvertFrom-Json
  'AC ' + ($s.currencies | Where-Object currencyID -eq 6).amount
  $s.transactions | Group-Object { $_.id.Split(':')[0] } | Sort-Object Name | ForEach-Object { '{0} {1}' -f $_.Name, $_.Count }
  foreach ($g in 0x6002027, 0x109D100) { '{0:X} = {1}' -f $g, (($s.inventory | Where-Object { $_.guid -eq $g -and $_.collision -eq 0 }).quantity) }
}
Show-Store
```

It prints the Armory Credit balance, the receipt count per kind (`item-data`, `consume`, `prestige`, `pawn`, ...) and the two items section 1.1 uses. Typing into a server console from a script: `D:\S2x\build\backups\syscon2.ps1 -TargetPid <pid> -Commands 'status'`; reading its window: `syscon-read.ps1 -TargetPid <pid> -Tail 40`.

## 0.4 The updater stays quiet

Launch `s2x.exe` directly for this check, without any flags, because the desktop shortcuts pass `-noupdate`.

- [ ] Console prints `[Updater] Automatic updates are off in this fork; new builds are at https://github.com/ChronoRixun/S2x-CR/releases`
- [ ] No `ui_scripts` folder under `%LOCALAPPDATA%\s2x\data` after the launch (only the renamed `ui_scripts_off-*` folders)

## 0.5 Baseline pass

One launch of each mode before testing anything specific, so a broken menu is found before it is blamed on a feature.

- [ ] MP frontend reaches the main menu; server browser and UNLOCKS tab present
- [ ] Zombies frontend reaches the main menu; UNLOCKS tab present with the Zombies rows
- [ ] New console lines from both launches contain no `error` you have not seen before and no `FAILED`
- [ ] No new file in `<game>\minidumps` newer than the start of the pass
- [ ] No `[HQ economy] N of M loot items have no Armory Credit pawn value` line once HQ has loaded: since `a22cea8` every pool item has a value (the #23 test saw none over 1,110 pool items). A line with `N` equal to `M` means the pawn table did not load; stop and report that

---

# 1. HQ economy

**Why first:** most of v1.5.0 is here, and much of it runs without anyone pressing anything. Every HQ entry now trades in spare uniforms (#11) and duplicates (#18), replays the prestige events (#22) and prunes item-data receipts (#25), and every drop, consumable and order goes through changed code. A mistake here costs players items or credits silently, on the first launch. If HQ does not load, AC goes down on entry, or a drop refuses to open, the release stops here.

**Time:** 40 minutes, with the backup from 0.2 taken. 1.5 has to wait for the daily reset at 00:00 UTC (19:00 CDT); do it last if the pass starts earlier.

## 1.1 HQ entry: trade-ins, prestige, receipts

1. MP, main menu. `Show-Store` (0.3) and write down AC and the receipt counts.
2. Make one spare of each kind in the console: `hqgrant item 0x6002027 1` (an "Any" uniform, the #11 path) and `hqgrant item 0x109D100 1` (the Rare LAD, a weapon variant: the #18 path, and an item outside the drop pool, so it needs the `a22cea8` values). `Show-Store`: both read `2`.
3. Quit and relaunch (the client only queues trade-ins when HQ loads, and leaving HQ from the console did not work before), then enter HQ and wait a minute.
4. Console: a `[HQ vendor] conversion-rule native success callback ...` line for the uniform and no `conversion-rule native FAILURE callback` line; no `missing task` line. `hqvendor` two minutes later shows `rejected=0` and the 242 counts not climbing.
5. `Show-Store`: both items back to `1`; AC up by 378 for the uniform (measured on #11) plus the LAD's value (not measured before: write it down); one more `pawn` receipt.
6. Prestige catch-up: this profile already received its Prestige 1 and 2 helmets and calling cards (`prestige` count 2 in `Show-Store`). On this HQ entry there must be no `[HQ AE] prestige N: granted its helmet and calling card` line: the game replays the events at every HQ load, and the receipts make the replay pay nothing. Barracks > Calling Cards shows the Prestige 1 and 2 cards; Special Helmets shows the two prestige helmets.
7. Receipts: `item-data` stays at 256 or fewer after the session (the store already holds 256, so the bound is being exercised live). A new one reads `sequence:<n>:...`:

   ```powershell
   (Get-Content "$game\players2\user\hq_economy.json" -Raw | ConvertFrom-Json).transactions | Where-Object id -like 'item-data:*' | Select-Object -Last 2
   ```

Do not use `setprestige` on this profile to test #22: the catch-up pays every level up to the prestige the profile holds at the next HQ entry, so a debug prestige becomes real helmets and cards.

**Evidence if it fails:** the console tail from HQ load, `Show-Store` before and after, `hqvendor` output.

**Verified already:** the uniform pawn, the duplicate trade-in, the values map, the prestige catch-up and the receipt bound each in their harness and live on this profile (uniform 2 → 1 and +378 AC; eight duplicates to one copy each, +214 AC; Prestige 1 and 2 on HQ entry; new `sequence:` receipts). Never run: all of them together from the release zip, and a weapon-variant duplicate traded in.

## 1.2 Multiplayer drops

1. HQ menu > E Supply Drops > Rare Supply Drop > ENTER, then "Open Next" for two or three drops (you hold Rare drops; if not, `hqgrant item 2 3`). Every reveal finishes with three cards.
2. Card 1 of every Rare drop is Rare or better. Weapon variants can appear on any card (9 in 26 drops on the test run); Heroic is rare (2–3%).
3. A duplicate card pays its displayed value: AC before plus the duplicates equals `hqwallet` after.
4. If a weapon variant dropped, find it in Create-a-Class: it is selectable. That screen itself was never checked by eye; `hqownership <guid>` in HQ read `usable=1 lock=0` and `CAC=Unlocked` for dropped variants.
5. Optional: one common drop (`sd_mp`) opens as before.

**Evidence if it fails:** the drop type, the three cards (screenshot), AC before and after, the console tail from the click.

**Verified already:** 500 Rare openings in the harness; live, 26 Rare drops with card 1 always Rare or better (16 Rare, 6 Legendary, 3 Epic, 1 Heroic), duplicates paid 1,038 AC, dropped variants read unlocked. Never run: the zip.

## 1.3 Collections, the LAD contract, camo prices

1. In HQ, `hqownership 0x109D100`: `usable=1 lock=0` and `CAC=Unlocked`. Run it in HQ, not at the main menu, where the same item reads lock 19. The LAD Machine Gun is unlocked in Create-a-Class.
2. Collections: the MG15 collection reward redeemed this week is still owned and equippable. If another collection is complete, press its reward button: input returns after about 6 seconds, the reward shows unlocked, the button is disabled when you come back. The redeem itself has only been pressed once by hand, on the MG15 collection.
3. A class camo in Collections is priced one tier above its rarity: Rattlesnake 250, Metalflage and Victory 550. Turquoise: Rifle read 550 when the fix was tested. Do not buy.

**Verified already:** redeem in the harness and live (reward in the inventory before the success callback); the LAD reads Locked with only the base and Unlocked with the Rare variant; 20 of the 23 captured retail prices match. Never run: the zip.

## 1.4 Zombies: consumables, drops, orders

1. Zombies lobby, Consumables: note the count of a card you hold two or more of, and equip it. `Show-Store`: note the `consume` count.
2. Start a solo match from PLAY (the #10 test used Solo from PLAY; Local Play is refused by the fork's `map` guard). Use the card once: its HUD charge goes down by one and its effect happens.
3. Back in the lobby: the card's count is one lower, `consume` is one higher, and there is no `missing task '96'` line in the session. `cg_unlimited_zm_consumables` stays 0 for this.
4. Optional, costs a card: use up a card you hold one of. It leaves the picker, and the slot reads "NO CONSUMABLE EQUIPPED".
5. E Supply Drops > Rare Zombie Supply Drop (`hqgrant item 6 1` if you have none): two regular cards, then three consumables, the reveal finishes; card 1 is Rare or better.
6. Orders offers six dailies and three weeklies; Contracts offers eight. A kill order you have accepted moves during the match and keeps its progress after a lobby round-trip.
7. If a claim is ready, claim it: a daily pays 250 AC, a weekly or contract one Zombies drop. A Zombies level-up prints `[HQ AE] rank up: granted a Rare Zombie Supply Drop`.

**Evidence if it fails:** the card, its count before and after, the console tail of the match.

**Verified already:** consumables in the harness (26/26) and live twice (Max Ammo 1 → 0 and out of the picker; Blitz Machine Coupon 1 → 0 and a self-revive 4 → 3 on the combined test build); three Zombies Rare drops with a Rare-or-better first card. Never run: the zip, and co-op or specialist objectives.

## 1.5 Order rotation (after 00:00 UTC)

1. Before the reset, list the current boards (kind 1 = MP daily, 8 = Zombies daily):

   ```powershell
   (Get-Content "$game\players2\user\hq_economy.json" -Raw | ConvertFrom-Json).achievements | Where-Object { $_.kind -in 1,8 -and $_.status -in 'available','inProgress' } | Sort-Object kind, name | Format-Table name, kind, offerDay, status
   ```

2. After 00:00 UTC, enter HQ once and the Zombies lobby once, and run it again. The new `offerDay` boards share no name with the old ones except orders you had accepted (`inProgress`), which carry over. Weeklies change at the weekly boundary, not tonight.

**Verified already:** 200 simulated periods with no repeat; live, the MP board rotated 4 of 4 new dailies with accepted orders carried. Never run: the Zombies board (the one on the profile today was made by the old client).

## Checklist

- [ ] HQ entry: uniform and LAD spares traded in, AC up by 378 plus the LAD's value, no FAILURE line, `rejected=0`
- [ ] No second prestige grant; Prestige 1 and 2 cards and helmets present
- [ ] `item-data` receipts at 256 or fewer; new ones `sequence:`
- [ ] MP Rare drops: card 1 Rare or better, reveals finish, duplicate credit adds up
- [ ] A dropped weapon variant is selectable in Create-a-Class (or none dropped, say so)
- [ ] LAD unlocked; MG15 reward owned; camo prices 250 / 550
- [ ] A Zombies consumable loses one charge, `consume` +1, no `missing task '96'`
- [ ] Zombies Rare drop: 2 + 3, card 1 Rare or better
- [ ] Zombies orders 6 / 3 / 8, progress persists
- [ ] After the reset: MP and Zombies dailies are a new set, accepted orders carried

---

# 2. Dedicated HUD limits — #12

**What changed:** a client in a dedicated server's public lobby took the HUD's score maximum from a stock playlist recipe (TDM's 75) instead of the server's limit. The host now reports its score, win and round limits in the `s2x_getInfo` reply; the client asks at every hosted preload, sets its own `scr_<gametype>_*` dvars and switches the recipe off. The server's own rules are unchanged.

**Time:** 15 minutes.

## What to test

1. Start the local server (0.3; `scr_gun_cycleCount 2` makes Gun Game's limit 36). Wait for its first match to start (`status` answers).
2. Launch the client, `connect 127.0.0.1:27017`, join mid-match. The client console prints `Hosted dedicated lobby: gun limits 36/1/1.` The HUD reads `GUN RANK: 0 / 36`, not `0 / 75`, and the scoreboard shows the same limit.
3. Let the rotation move to Domination (or end the match from the server console). After the load, look for a `Hosted dedicated lobby: dom limits ...` line; its first number is the score limit (200 by default). Domination's HUD shows the team scores only, so the console line is the check here.
4. Rotate back to Gun Game: `0 / 36` again.
5. Stop the server, start it again and join during its first match: the limit shows there too (the fresh-host case, which reads 0 without the fix).
6. Let one Gun Game match run to its end, bots will do it: the match ends at 36. The release notes' known issue says Gun Game runs to 75 on a dedicated server; the server-side trace says it ends at the ladder length. Record what you see; the known-issues line depends on it.
7. Stop the server (it ran with the master disabled). Launch the client the normal way and open the server browser: the list fills and the console logs `[server_list] requesting S2 servers from ...`. Before this build a server whose cfg set `master_server_enable 0` wrote that setting into `players2\system_config_mp.cfg` while it started, and the client then showed "No servers found" at every launch with nothing logged.

**Evidence if it fails:** a screenshot of the HUD, the client console tail from the connect, the server's gametype and `scr_gun_cycleCount`.

**Verified already:** live on a local Gun Game server with a limit of 36: after a mid-match join, after a rotation and on a fresh host's first match (screenshots in `build\research\issue-12-live`). Never run: Domination's line, the zip, and a remote client.

## Checklist

- [ ] `Hosted dedicated lobby: gun limits 36/1/1.` on join
- [ ] HUD and scoreboard read 36, not 75
- [ ] After a rotation the HUD follows the new gametype's limit
- [ ] Fresh host's first match shows the limit
- [ ] Gun Game match ends at 36 (or note where it ended)
- [ ] After the server stopped, the browser lists servers and the console logs `[server_list] requesting`

---

# 3. Server Manager launch profiles

**What changed:** the Manager can start servers it cannot build a cfg for. A separately installed server package with a `server-manager.json` in its folder is registered once under LAUNCH PROFILES...; its modes then appear in the Zombies editor for the maps it covers, and LAUNCH SERVER runs the package's own start script, records the pid, and Stop, Console and the card work as for any server. The Manager ships no package.

**Time:** 15 minutes. You need a server package with a `server-manager.json` and a free UDP port. See your package's notes for its folder, its entry names and the messages it prints; the steps below say `<package>`, `<mode>` and `<port>` where yours go.

## What to test

1. Run `<game>\s2x\tools\S2xServerManager.exe` (the one from the zip, not a build folder).
2. LAUNCH PROFILES... (title bar, left of the clock). If `<package>` is listed, check its row: title, number of modes, folder, and no orange `start script missing: ...` line. If not, ADD... its folder: a folder whose `server-manager.json` does not parse is refused. CLOSE. `<game>\s2x\launch-profiles.txt` holds the folder.
3. + New server > Blank server. A name, UDP port `<port>`, ZOMBIES.
4. Pick a map the package covers: the mode list is Zombies followed by the package's modes. Pick a map it does not cover: Zombies only. Back to the covered map, pick `<mode>`, + ADD.
5. Expect: one row, the map with the entry's tag; 03 LOBBY `Set by the profile: ...` with the package's slots and bots; 05 ADVANCED greyed with `Not available for profile servers: the profile writes its own cfg.`; Shuffle and RANDOMIZE greyed; WRITES TO `Starts <package folder>\<script>; the profile writes its own cfg.`
6. + ADD again: toast `A profile server runs one map: clear the rotation first.` Type a `"` in the name: toast `A profile server's name cannot hold a quote or a line break.` and the quote is gone.
7. 04 VISIBILITY: turn "Advertise on the master list" OFF. A new server starts with it on, and the package would then list itself publicly.
8. SAVE PRESET, then LAUNCH SERVER. The script takes a few seconds: toast `Starting <name> on :<port>`, footer STARTING, then RUNNING once it answers. The card's summary shows the entry's bots and cap 4, not the preset's bot fill.
9. `<game>\s2x\server-<port>.pid` exists and holds the server's pid (`Get-NetUDPEndpoint -LocalPort <port>`).
10. CONSOLE: the drawer header names the entry's log inside the package folder, and lines arrive.
11. SAVE AS... a second preset (it gets the next free port), LAUNCH SERVER on it: the toast carries the package's own refusal to start a second copy of the same entry, and that server stays stopped with no pid file.
12. Back on the first (FLEET > EDIT): STOP. Card STOPPED, the pid file is gone, `Get-NetUDPEndpoint -LocalPort <port>` is empty.
13. Clean up: DELETE PRESET on both test presets. Leave the registration if box 4 will use the same package.

Regression from v1.4.0, because Start All and the card changed:

14. A native preset: START, it goes Starting then Running; the card reads the preset's own numbers (`12 bots · cap 18` style). STOP.
15. START ALL with two native presets on different ports: the window stays responsive while they start (each start now runs off the window's thread); both come up. STOP ALL.
16. Close the window: it goes to the tray, a running server keeps running; Exit from the tray. `s2x\tools\server-launcher.cmd` still opens the old launcher with no console window.

**Evidence if it fails:** a screenshot of the toast or editor, the preset's `.json`, `launch-profiles.txt`, the package's log.

**Verified already:** the loader (good, broken, missing and repeated folders, comments kept on Remove), the editor's mode list and refusals through the view model, renders of the dialog and editor, a by-hand launch with the controller's own command (server found on its port, answered with the name passed in, console path right, second launch refused with the package's message), and a GUI run of steps 2–12 on this PC with the branch build. Never run: the zip's exe, and box 4.

## Checklist

- [ ] Package registered; its row shows its modes and no missing script
- [ ] Modes listed only for the maps the package covers
- [ ] One-map rule and the name refusal, both toasts
- [ ] Lobby text, greyed advanced block, WRITES TO names the script
- [ ] Launch: Starting, then Running; pid file written
- [ ] Console tails the package's log
- [ ] Second launch of the same entry refused with the package's message
- [ ] Stop: card stopped, pid file gone, port free
- [ ] Native preset, Start All / Stop All, tray and `server-launcher.cmd` as in v1.4.0

---

# 4. Map names: The Final Reich and The Frozen Dawn

**What changed:** the Manager's and the launcher's Zombies tables had the two swapped: The Final Reich is `mp_zombie_nest_01` and `mp_zombie_descent` is The Frozen Dawn, DLC 4. Picking "The Final Reich" saved a rotation that ran The Frozen Dawn. Presets keep the key they hold, so one saved as "The Final Reich" before now shows "The Frozen Dawn" and needs re-picking.

**Time:** 5 minutes.

## What to test

1. Manager, + New server > Blank server > ZOMBIES, open the map list: Groesten Haus, The Final Reich, The Darkest Shore, The Shadowed Throne, the three Tortured Path chapters, The Frozen Dawn; the DLC tags sit on The Darkest Shore (DLC 1), The Shadowed Throne (DLC 2), the Tortured Path (DLC 3) and The Frozen Dawn (DLC 4), none on The Final Reich. Cancel.
2. Open a Zombies preset saved before this build with "The Final Reich" in it (any preset whose `.json` holds `mp_zombie_descent`): the row reads The Frozen Dawn · DLC 4. Pick The Final Reich again and SAVE; the `.json` now holds `mp_zombie_nest_01`.
3. The launcher (`s2x\tools\server-launcher.cmd`), Zombies picker: the same names and tags, and the re-picked preset loads as The Final Reich.
4. Start a native Zombies server on The Final Reich from the Manager (Advertise off): look for `mp_zombie_nest_01` in its console as it loads. Leave it running for section 5, then stop it.
5. Optional, pre-v1.3.0 placeholder names: copy a Zombies preset and set its map to `nazi_zombie_asylum_f` (the old Final Reich placeholder). The launcher rewrites it to The Final Reich. From the code, the Manager's own placeholder table was not swapped in `69285bd` (it still maps `nazi_zombie_asylum_f` to `mp_zombie_descent` and `nazi_zombie_mountaineer` to `mp_zombie_nest_01`), so expect the Manager to show The Frozen Dawn for it. Record which you see; it decides whether the Manager's table needs the same swap.

## Checklist

- [ ] Manager and launcher list The Final Reich without a tag and The Frozen Dawn with DLC 4
- [ ] An old "Final Reich" preset shows The Frozen Dawn; re-picked and saved as `mp_zombie_nest_01`
- [ ] A server started on The Final Reich loads `mp_zombie_nest_01`
- [ ] Optional: placeholder preset result recorded for the launcher and the Manager

---

# 5. Status card

**What changed:** `server-status.ps1` names The Final Reich and The Frozen Dawn correctly, and a Zombies line stops after the player count instead of saying bots fill the rest (Zombies bots wait for a player). Box 4 already runs this script; this is a regression check on the shipped copy.

**Time:** 5 minutes.

## What to test

1. While the section 4 server runs on The Final Reich:

   ```powershell
   powershell -File "$game\s2x\tools\server-status.ps1" -ServerHost 127.0.0.1 -Port <its port> -DryRun
   ```

   Its line reads `Zombies on The Final Reich · 0 players online` (the wording box 4's servers print), with no `bots fill the rest of`. Nothing is sent to Discord.
2. With the section 2 server up, the same dry run on 27017: the Multiplayer line still ends `bots fill the rest of N`.
3. After the box 4 upgrade (section 8): the Discord card lists every server, Zombies lines without "bots fill", map names right.

**Verified already:** dry runs against box 4's live servers when the fix was made. Never run: the zip's copy.

## Checklist

- [ ] Zombies line: correct map name, no "bots fill"
- [ ] Multiplayer line unchanged
- [ ] Box 4 card correct after the upgrade

---

# 6. README, docs and the harness

Nothing to play. The README's Modding section now says to keep your own scripts in `<game folder>\s2x\`, because upstream's updater deletes unmanaged files under `%LOCALAPPDATA%\s2x\data`; the rename, showcase and SECURITY changes are text. The harness line is under "Automated coverage" above.

- [ ] Economy harness built and passed on the tested commit (or skipped, say so)

---

# 7. Regression: unchanged since v1.4.0

Nothing in v1.5.0 touches these; tick what you happen to see, none is required.

- [ ] Gun Game bots keep full bodies through promotions (#4); the section 2 server shows it
- [ ] `!visits` from the T prompt answered on a local server with the example script (#9)
- [ ] `setrank 3` lands on level 3 (#7); not `setprestige`, see 1.1
- [ ] Launcher DLC labels on the MP picker
- [ ] Killcam and podium emotes on a dedicated server as in v1.4.0

---

# 8. Box 4 upgrade

Only after sections 0–5 are ticked. Box 4 has no Steam and runs its servers from the game folder; the status task keeps running through the upgrade. The package-specific values (its folder, entries and ports on box 4) are in your package's notes.

1. Copy `D:\S2x\build\release\v1.5.0-rc1\s2x-cr-v1.5.0-rc1.zip` to box 4.
2. Stop the Multiplayer servers (Manager: STOP ALL). The package's Zombies servers run from the package's own folder: stop them or leave them as its notes say.
3. Extract the zip over the game folder, replacing what is there. `s2x\tools\S2xServerManager.exe` is the new one (LAUNCH PROFILES... in its title bar).
4. Put the package's `server-manager.json` inside the package folder that will be registered (the Manager only reads it from there), then LAUNCH PROFILES... > ADD... that folder. Its row shows every mode with no missing script.
5. Open every Zombies preset: any that shows The Frozen Dawn but was meant to be The Final Reich, re-pick The Final Reich and SAVE.
6. If the Manager should own the package's servers from now on: stop them with the package's own stop script, then make one profile preset per server on the same ports, Advertise on, and LAUNCH SERVER each. Otherwise leave them; the card still reads them through `-Ports`.
7. START ALL for the Multiplayer servers.
8. From this PC: the servers appear in the browser within a minute, `connect` works, the HUD on a Multiplayer server shows the server's score limit rather than a stock 75 unless that is its limit, and the Discord card updates within two minutes with correct map names.

- [ ] Zip on box 4, servers stopped, files replaced
- [ ] Package registered with its `server-manager.json` inside it
- [ ] Zombies presets re-picked where needed
- [ ] Servers back up; listed in the browser; joined; HUD limit right; card updated

---

# 9. Cut the release

1. Tag the tested commit and push the tag: `git tag v1.5.0 ebcb45b; git push origin v1.5.0` (or the newer tip if anything was fixed during the pass).
2. Rebuild after tagging (premake and MSBuild as in 0.1) so the exe reports `v1.5.0` instead of `v1.4.0-46-gebcb45b`, then run the packager as `-Version v1.5.0` so the zip name is right. Upload `s2x-cr-v1.5.0.zip` to a GitHub release on the tag.
3. Notes: `D:\S2x\build\research\release-notes-v1.5.0-draft.md`. Fill its "Tested" line from the results table below, and check its Gun Game known issue against section 2 step 6 before publishing.
4. The Discord download card already points at the latest release, so nothing to edit there. Post the notes in #announcements yourself.
5. Say what was **not** tested in the release notes; that sentence is worth more than an implied full pass.

- [ ] Tag pushed
- [ ] Rebuilt at the tag; release published with `s2x-cr-v1.5.0.zip`
- [ ] "Tested" line filled; announcement posted

---

# Results

| Check | Pass / fail / skipped | Notes |
|---|---|---|
| 0 Install, backup, updater, baseline | pass | rc1 18:27, rc2, rc3 21:48 (game closed, profile backed up each time); updater quiet; baseline HQ entry smooth |
| 1 HQ economy | pass | 1.1 prestige helmet/card, trade-ins not exercised; 1.2 three Rare drops, first card Rare+; 1.3 collection reward incl. MAS-38; 1.4 consumables used up; 1.5 ZM 5 new dailies + accepted LMG carried, MP offerDay advanced with 2 accepted carried; #25 trimmed live at 256 |
| 2 Dedicated HUD limits | pass | rc3, server from rc-test.cfg: `gun limits 36/1/1` on join, HUD 0/36, `dom limits 200/1/2` after rotation (Dom HUD shows no maximum), 0/36 again, match ended at 36; step 7: fresh client after the LAN-only server requested the master and listed 13 |
| 3 Server Manager launch profiles | pass | GUI: register, four modes listed, launch, join, stop; Box 4 launched two profile servers |
| 4 Map names | pass | Manager, launcher tables and status card name The Final Reich (nest_01) and The Frozen Dawn (descent); Box 4 profile server on The Final Reich shows the right name in the Manager and the card |
| 5 Status card | pass | Box 4 card after the upgrade: 6 servers, Zombies lines with the right map and no "bots fill", MP lines unchanged |
| 6 README, docs, harness | partial | harness built and passed at build time (tier weights, Rare floor, receipt bound); README not re-read in the pass |
| 7 Regression list | partial | covered by play tonight: browser, connect, HQ, drops, Zombies match, Manager start/stop; the rest not walked |
| 8 Box 4 upgrade | pass | rc3 zip via the share, new Manager, kit profile registered, 4 MP + 2 ZM profile servers up and on the master, status task re-registered for the new ports |
| 9 Release | | tag v1.5.0 at 1637261 (local until pushed); rebuilt and packaged as v1.5.0 |

Not tested in this pass: a first-run Defender/SmartScreen prompt; a remote client on a Gun Game server; the launcher script's Zombies rotation; weekly order rotation; sections 6 and 7 in full.
