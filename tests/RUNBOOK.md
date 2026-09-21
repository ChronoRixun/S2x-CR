# S2x-CR release runbook — v1.3.0 candidate

**Written:** 2026-09-21 (from Astra's morning runbook, reordered by risk and extended with the install, box 4 and release steps)
**Build under test:** `integration` at `7cdc566` (everything since `v1.2.0`; rc3 adds the updater change and the consumable stock fix on top of `d8268d3`)
**Repo:** `D:\S2x` — **Game:** `D:\Program Files\Steam\steamapps\common\Call of Duty WWII` — **Box 4:** runs v1.1.2 today, plus a hand-copied `server-status.ps1`
**Budget:** about 90 minutes for sections 1–6 on this PC, then 20 minutes for box 4 and the release

## How to use this

Work top to bottom. Section 0 builds the candidate *the way it ships* and installs it; sections 1–6 are ordered by how likely they are to block the release, so the thing most likely to fail is the first thing you look at. Each section says what changed, exactly what to do, what the console should print, and what I already verified versus what has never been run on a real game. A section is done when every box is ticked. If something fails, don't fix it on the spot: capture the evidence listed in that section and note it in the results table, so it can be diagnosed from the log rather than from memory.

Where I know the exact console wording it is quoted verbatim from real runs. Where it was never observed, the runbook says "look for a line mentioning ..." instead of inventing text.

## What changed since v1.2.0

| Commit | Change | Tested by |
|---|---|---|
| `550e958` | `setrank` / `setprestige` rebaseline past reward XP (#7) | section 4 |
| `f3a0c56` | Gun Game bot costume repair script (#4) | section 3 |
| `7570c5d` | Chat event bridge, `fileread` / `filewrite` / `getip` / roster, status-card arrivals (#9) | section 2 |
| `9839e66` | 20 daily / 7 weekly Zombies orders, 8 contracts, duplicate drops pay Armory Credits (#6) | sections 1 and 5 |
| `4335186` | Drops still open when an item has no pawn value (my fix on top of #6) | section 1 |
| `a46e3c8` | Launcher labels DLC maps, Zombies zone names corrected | section 6 |
| `57424a2` | Discord status card script | already live on box 4; section 7 regression |
| `8a50b18` | Upstream update download is opt-in (`-update`); a plain launch no longer pulls upstream's UI scripts into AppData | section 0.4 |
| `f27228e`, `7cdc566` | Zombies consumable cards stack their charge count on the family stock row, so a Self-Revive card raises "In Stock" | section 1 step 5 |
| `86b712d`–`d8268d3` | README, contributing files, CI, the showcase site | nothing to play |

Automated coverage before you start: the economy, rank, scripting and storage harnesses pass offline; the fork's CI run is green; the chat bridge's argument order and the two engine addresses it calls were verified against the decompiled engine. What none of that proves is anything rendered on screen or anything a human types in chat. That is what this runbook is for.

---

# 0. Before you start

## 0.1 Build the candidate the way it ships

Close the game and any local server first. Build `integration`, then let the packager stage exactly what a release zip contains, and install from that stage. Testing the zip, not a hand-copied set of files, is the point.

```powershell
cd D:\S2x
git checkout integration; git pull --ff-only
.\tools\premake5.exe vs2022
& 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe' build\s2x.sln /t:client /p:Configuration=Release /p:Platform=x64 /m /v:minimal
powershell -File build\diagnostics\package-release.ps1 -Version v1.3.0-rc3
```

The packager prints every file it staged. Expect exactly this shape, and stop if anything is missing:

| In the zip | Count |
|---|---|
| `s2x.exe`, `s2x.pdb` | 2 |
| `s2x\tools\`: `server-launcher.ps1`, `ServerLauncher.xaml`, `server-status.ps1` | 3 |
| `s2x\ui_scripts\mp\find_match\*.lua` | 6 |
| `s2x\ui_scripts\mp\patches\*.lua` (`__init__`, `cwl_currency`, `dedicated_gametype`, `dedicated_lobby`, `dedicated_members`, `dedicated_party`, `unlocks`) | 7 |
| `s2x\scripts\mp\`: `s2x_gungame_bots.gsc`, `s2x_server_events.gsc` | 2 |

Install by copying the stage over the game folder:

```powershell
$game = 'D:\Program Files\Steam\steamapps\common\Call of Duty WWII'
Copy-Item D:\S2x\build\release\v1.3.0-rc3\stage\* $game -Recurse -Force
```

Two things that have bitten before: `%LOCALAPPDATA%\s2x\data\ui_scripts_off` must stay renamed off (it outranks the game folder), and the `patches` folder must hold all seven files, not just the changed one.

- [ ] Release build succeeded (`client.vcxproj -> ...\s2x.exe`, no errors)
- [ ] Packager listed 20 files in the shape above, including the two `.gsc` files and `server-status.ps1`
- [ ] Stage copied over the game folder; `s2x.exe` timestamp is today's

## 0.2 Back up the profile and the economy store

`setrank` writes real progression and section 1 spends real drops. Take the backup before anything else, and remember that restoring `players2` later also restores the old Armory Credit balance.

```powershell
$game = 'D:\Program Files\Steam\steamapps\common\Call of Duty WWII'
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
Copy-Item "$game\players2" "D:\S2x\build\backups\players2-$stamp" -Recurse
```

`players2\user\hq_economy.json` rides along in that copy. Do **not** delete it for this pass; the point of section 1 is what happens to an existing inventory.

- [ ] `players2` backed up to `build\backups\players2-<stamp>`

## 0.3 Launching

Launch the client from a shortcut, with Steam running and signed in: `C:\Users\Owen\Desktop\S2x Development.lnk` (`-noupdate -multiplayer`) and `D:\S2x\build\backups\S2x Zombies Dev.lnk` (`-noupdate -zombies`). Astra's runbook asks for `-demonware_debug`; on this PC that launch died three seconds in on 2026-09-13 and was never fixed. Try it once if you want the extra event evidence; if it dies, use the plain shortcut, nothing below needs it.

A local dedicated server for sections 2 and 3, on a port that does not collide with box 4, from a cmd window in the game folder:

```bat
cd /d "D:\Program Files\Steam\steamapps\common\Call of Duty WWII"
start "" s2x.exe -noupdate -dedicated +set net_port 27017 +set party_maxplayers 18 +set party_matchStartDelay 15 +set bot_fill 8 +set bot_names nostalgia +set sv_hostname "rc test" +set sv_maprotation "gametype gun map mp_shipment_s2 gametype dom map mp_shipment_s2" +exec server.cfg +map_rotate
```

`server.cfg` runs after the command line and wins; the one in the game folder today sets the box 4 hostname and rotation, so expect its name in the browser and edit it if you want the rotation above. Things that look wrong and aren't: `Dedicated party: failed to select match-rules gametype 'dom'` prints on a working server; `status` answers `Server is not running.` until the first match starts; a map load takes 40–50 seconds; the first `bot_fill` line often reads `the engine added 16 of 17` and a second line tops it up.

Reading the console: `<game>\s2x\logs\console.log` is shared by every instance and only ever appended to, so note the line count before a test and read the tail.

```powershell
$log = 'D:\Program Files\Steam\steamapps\common\Call of Duty WWII\s2x\logs\console.log'
$before = (Get-Content $log).Count
# ... test ...
Get-Content $log | Select-Object -Skip $before | Select-String -Pattern 'error|FAILED|exception|assert|HQ economy|bot_fill|Gun Game'
```

Typing into a server console from a script: `D:\S2x\build\backups\syscon2.ps1 -TargetPid <pid> -Commands 'status'`; reading its window: `syscon-read.ps1 -TargetPid <pid> -Tail 40`.

## 0.4 The updater stays quiet

Until rc2, every launch without `-noupdate` ran upstream's updater, which downloaded upstream's UI scripts into `%LOCALAPPDATA%\s2x\data\ui_scripts`; the loader runs every `ui_scripts` folder it finds, so those loaded after ours and replaced the fork's `unlocks.lua`. It happened at 09:11 on 2026-09-21. rc2 makes the download opt-in. Launch `s2x.exe` directly for this check, without any flags, because the desktop shortcuts still pass `-noupdate` and that flag is now ignored.

- [ ] Console prints `[Updater] Automatic updates are off in this fork; new builds are at https://github.com/ChronoRixun/S2x/releases`
- [ ] No `ui_scripts` folder under `%LOCALAPPDATA%\s2x\data` after the launch (only the renamed `ui_scripts_off-*` folders)

## 0.5 Baseline pass

One launch of each mode before testing anything specific, so a broken menu is found before it is blamed on a feature.

- [ ] MP frontend reaches the main menu; server browser and UNLOCKS tab present
- [ ] Zombies frontend reaches the main menu; UNLOCKS tab present with the Zombies rows
- [ ] New console lines from both launches contain no `error` you have not seen before and no `FAILED`
- [ ] No new file in `<game>\minidumps` newer than the start of the pass
- [ ] Console shows the pawn-value line from section 1 at least once (it prints when the loot pool loads)

---

# 1. Supply drops and duplicate credits — #6

**Why first:** this is the one change with a known way to fail hard. Duplicates now convert to Armory Credits at the game's pawn values, and the first version of that refused to open *any* drop if a single loot item had no pawn value. I removed that guard; a duplicate without a value now changes nothing and the drop still opens. The offline harness proves the logic, not the game's tables. If the Quartermaster won't open a drop, the release stops here.

**Time:** 10 minutes, if you have drops. A drop with no duplicate in it proves the reveal works but not the credit; open until you see one.

## What to test

1. In MP, before anything else, find the loot-pool line in the console and write down the numbers:

   ```text
   [HQ economy] N of M loot items have no Armory Credit pawn value; their duplicates grant nothing extra
   ```

   No line at all means every item has a value, which is the best case. `N` equal to `M` means the pawn table did not load, and duplicates would never pay; stop and report that.
2. `hqwallet` in the console and note the AC balance. `hqeconomy` prints the inventory; note which cosmetics you already own.
3. Quartermaster > open an MP common drop. The three-card reveal completes. For each card you already owned, the balance rises by that card's displayed value; add them up and compare with `hqwallet` afterwards.
4. Repeat with an MP rare drop if you have one.
5. Switch to Zombies. Open a Rare Zombie Supply Drop: two regular cards, then three consumable cards; the reveal finishes. Owned regular cards pay AC; consumables stack as quantities even when repeated.
6. Reopen the Quartermaster, then quit and relaunch. Balance and inventory are unchanged by the reopen and survive the restart; no second payout for looking again.

**Evidence if it fails:** the console tail from the moment you clicked open, the drop type, AC before and after, and a screenshot of the reveal. If the drop refuses to open at all, `hqeconomy` output too.

**Verified already:** every roll combination (unowned, owned, expired, zero quantity, missing value, overflow, replay of a receipt, failed save) in the offline harness; the live drop opening has not been tried on this build.

## Checklist

- [ ] Pawn-value line found and recorded (or absent); `N` is not equal to `M`
- [ ] MP common drop opens; duplicate credit equals the card values
- [ ] MP rare drop opens (or none available, say so)
- [ ] Zombies drop: two regular then three consumables, reveal completes
- [ ] Owned cards paid AC; consumables stacked
- [ ] A Self-Revive card raises "In Stock" on the Consumables screen; a rare card adds 2 units and an epic 4
- [ ] Balance and inventory survive reopen and restart, no double payout

---

# 2. Chat bridge and server scripting — #9

**Why second, and why local:** `s2x_server_events.gsc` turns every human `say` into a script event through a new native hook. The hook's argument order and the two engine calls it makes were checked against the decompiled engine, and the GSC side ran on a bot server, but no human has typed in chat through it. A crash there would take box 4 down the first time a player says hello, so it gets proven on the local server before the public one.

**Time:** 15 minutes.

## What to test

1. Start the local server (0.3). Copy the example script next to the shipped one so there is something visible to trigger:

   ```powershell
   Copy-Item D:\S2x\tools\server-scripts\examples\scripting_api.gsc "D:\Program Files\Steam\steamapps\common\Call of Duty WWII\s2x\scripts\mp\"
   ```

2. Launch the client, `connect 127.0.0.1:27017`, spawn. Expect the HUD line `Welcome! Type !visits to see your saved map visits.`
3. Type `!visits` in global chat. Your own chat line still appears as normal, and the HUD answers `Server: Saved map visits: 1`.
4. Say something in team chat. Nothing crashes; the server console keeps printing normally.
5. On the server console: `map_restart`. Spawn again and `!visits` says `2`.
6. Optional, address lookup: nothing to type; if you want to see it, drop a one-line script that prints `getip(self)` on spawn and confirm it prints `127.0.0.1` for you and nothing for bots. Skip if short on time; the bot side of this was exercised in Astra's run.
7. Optional, Discord arrivals: this only matters if you want the "Recent arrivals / departures" field on the card. It needs `set s2x_roster_file "rc-roster.json"` in `server.cfg`, a restart, then a dry run of the status script pointed at the local server:

   ```powershell
   powershell -File D:\S2x\tools\server-status.ps1 -ServerHost 127.0.0.1 -Port 27017 -RosterFile "$env:LOCALAPPDATA\s2x\scriptdata\rc-roster.json" -DryRun
   ```

   Expect the normal four fields plus a presence field, with no bots in it. Nothing is sent to Discord.
8. Remove `scripting_api.gsc` from the game folder afterwards unless you want to keep the example. Leave the two shipped scripts.

**Evidence if it fails:** the server console tail around your chat line, and if the server died, the `0x` code from the Windows event log and the minidump.

**Verified already:** argument order, string interning and reference release in the native hook (decompiled and compared); file storage, chat routing and the roster in the offline harnesses; the GSC bridge, storage and bot-IP checks on a local bot server. Never run: a human's chat through the hook, a remote player's address, a real card update with arrivals.

## Checklist

- [ ] Welcome HUD line on spawn
- [ ] `!visits` in global chat answered on the HUD; chat itself unaffected
- [ ] Team chat does nothing odd
- [ ] Count survives `map_restart` and reads `2`
- [ ] Optional: `getip` prints `127.0.0.1` for you, empty for bots
- [ ] Optional: status dry run shows the presence field without bots
- [ ] Example script removed (or kept on purpose)

---

# 3. Gun Game bots keep their bodies — #4

**What changed:** Gun Game rebuilt each bot's outfit from profile costume data bots don't have, so after the first weapon change they rendered as head, torso and arms. `s2x_gungame_bots.gsc` waits for each bot's loadout update and reapplies the generated division uniform. Nothing about this can be checked without eyes on the bots.

**Time:** 15 minutes.

## What to test

1. With the local server on the Gun Game rotation from 0.3 (or set it in the launcher: Gun Game on Shipment, 8 bots), on the server console `set s2x_gungame_debug 1` before the match starts.
2. Join, spawn, and look at bots on first spawn: full bodies, legs included.
3. Play through several promotions and bot respawns. Bodies stay complete. The server console prints `S2x Gun Game: repaired bot costume for division ...` as bots change weapons.
4. Your own outfit is the one you'd expect for the division you picked.
5. Let the rotation move to Domination. Bots look and play normally. Rotate back to Gun Game (or `map_restart` on it) and check once more.
6. `set s2x_gungame_debug 0` when done.

The debug line proves the repair ran; only your eyes prove it rendered.

**Evidence if it fails:** a screenshot of a legless bot with the server console tail showing whether the repair line printed for that bot's division.

**Verified already:** the script compiles and ran a full London Docks Gun Game match with four bots, 40 repair calls, no script errors. Never seen: the rendered result.

## Checklist

- [ ] Bots have legs on first spawn in Gun Game
- [ ] Bodies stay complete through promotions and respawns
- [ ] Repair lines print on the server console
- [ ] Your outfit is normal
- [ ] Domination bots normal; Gun Game still fine after rotating back
- [ ] Debug dvar set back to 0

---

# 4. Rank and prestige — #7

**What changed:** MP displays `experience + (inventoryTotalXP - inventoryXPAtLastReset)`, so `setrank` used to land above the requested level whenever you had earned reward XP. The command now rebaselines both inventory fields the way the game's own prestige reset does. The chooser in UNLOCKS reads the same total the Soldier screen does.

**Time:** 10 minutes plus one map load. Backup from 0.2 first; write down your current rank and prestige.

## What to test

1. MP console: `setrank 1`. Reopen Soldier: level **1**.
2. `setrank 2`: level **2** (4,000 XP). `setrank 3`: level **3** (8,600 XP).
3. UNLOCKS > Prestige and Rank shows the same level as Soldier. Pick level 4, Apply, confirm; Soldier reads **4**.
4. `setprestige 1`: prestige **1**, level **1**. No prestige cosmetics are awarded; that is expected.
5. Zombies: `setrank 3` reads **3** there, and MP's rank has not moved.
6. In each mode, load any map before quitting so the stats upload. Relaunch and confirm the ranks stuck.
7. Play a few minutes and earn XP; progress moves forward from the set level. If a claimable MP reward with XP is handy, claim it and confirm that XP counts too.

**Evidence if it fails:** the console output of the command and a screenshot of the Soldier screen.

**Verified already:** the production command against all 2,000 rank thresholds and the inventory-XP baseline in the offline harness; the chooser reader under Lua stubs. Never run: the commands on this profile since the change.

## Checklist

- [ ] `setrank 1`, `2`, `3` each land on the requested level
- [ ] Chooser agrees with Soldier and Apply sets level 4
- [ ] `setprestige 1` gives prestige 1, level 1
- [ ] Zombies `setrank 3` does not touch MP
- [ ] Ranks persist across a map load and relaunch
- [ ] XP earned afterwards counts

---

# 5. Zombies orders and contracts

**What changed:** the pool is now 20 daily and 7 weekly orders (six and three offered at a time) and eight timed contracts, all tracked from kill events. Claiming already passed your earlier check, so this section is match progression, timers and persistence.

**Time:** 20 minutes for one ordinary match; the specialist checks are optional.

## What to test

1. Zombies lobby: Orders offers six dailies and three weeklies; Contracts offers eight. `hqcontracts` lists the active native records.
2. Accept a general kill order and buy one kill contract. Note its AC price, the balance after (debited once), starting progress and time remaining.
3. Play a wave or two, kill a handful of zombies, return to the lobby. Progress moved and stays after reopening Orders. Two kills in the same second both count.
4. The contract timer counts down in the match and holds in the lobby. A third contract can be active; a fourth is refused.
5. Finish and claim one objective. A daily pays 250 AC; a weekly or contract pays one Zombies drop. Relaunch: the claim and the reward are still there.
6. Back in MP, the MP orders have not moved because of anything you did in Zombies.

Optional specialist checks, if a matching objective is offered: an airborne throwing-knife contract ignores ordinary gun kills and counts the real thing; a Darkest Shore or Ripsaw objective ignores wrong-map and wrong-weapon kills; if a second player is around, a short objective as the remote co-op player, with this build on both sides.

The catalog with every target, price and time limit is `tests\economy\zombies-catalog.md`.

**Evidence if it fails:** objective name, map, host or client, weapon, what you did, displayed progress, and the session's console tail.

## Checklist

- [ ] 6 / 3 / 8 offered
- [ ] Order accepted and contract bought; price debited once
- [ ] Kill progress advances and persists through a lobby round-trip
- [ ] Timer runs in match, pauses in lobby; fourth contract refused
- [ ] Claim pays the right reward and survives a relaunch
- [ ] MP orders untouched
- [ ] Optional: a specialist or co-op objective behaves

---

# 6. Launcher: DLC labels and Zombies names

**What changed:** both map pickers label maps that need a DLC pack (Season Pass, DLC 1–4), and the Zombies list now uses the real zone names (`mp_zombie_house` and friends) with the three Tortured Path chapters spelled out.

**Time:** 5 minutes.

## What to test

1. Run the launcher from the game folder: `powershell -ExecutionPolicy Bypass -File s2x\tools\server-launcher.ps1`.
2. Multiplayer picker: DLC maps carry a label (for example Dunkirk as DLC 2); base maps carry none.
3. Zombies picker: Groesten Haus, The Final Reich, The Darkest Shore, The Shadowed Throne and the three Tortured Path chapters are listed by name.
4. Load a saved preset; it still loads. Start a Zombies server from the launcher on The Final Reich and confirm the console shows `mp_zombie_descent` loading, then stop it.

## Checklist

- [ ] DLC labels on the MP picker
- [ ] Zombies names correct, Tortured Path split in three
- [ ] Presets still load
- [ ] A Zombies server started from the launcher loads the right zone

---

# 7. Box 4 upgrade

Only after sections 1–6 are ticked. Box 4 has no Steam and runs the launcher from the game folder; the status task keeps running through the upgrade.

1. Copy `D:\S2x\build\release\v1.3.0-rc3\s2x-cr-v1.3.0-rc3.zip` to box 4.
2. Stop the server (close its console window). Extract the zip over the game folder, replacing what is there. `s2x\scripts\mp` now exists with the two scripts.
3. Start the server from the launcher with the "My Server" preset.
4. From this PC: the server appears in the browser within a minute, `connect` works, chat works on the public server (say something; nothing dies), and the Discord card updates within two minutes with the current map.
5. If the preset ever rotates through Gun Game, watch one bot; otherwise this is covered by section 3.

- [ ] Zip on box 4, server stopped, files replaced, scripts folder present
- [ ] Server back up from the launcher
- [ ] Listed in the browser; joined; chat exchanged; card updated

---

# 8. Cut the release

1. Tag the tested commit and push the tag: `git tag v1.3.0 d8268d3; git push origin v1.3.0` (or the newer tip if anything was fixed during the pass, in which case rebuild and repackage first).
2. Re-run the packager as `v1.3.0` so the zip name is right, and upload that zip to a GitHub release on the tag. Notes lead with what changed for players: bots keep their bodies in Gun Game, duplicates pay Armory Credits, `setrank` lands where you asked, 27 Zombies orders and 8 contracts, server scripting for hosts, DLC labels in the launcher.
3. The Discord download card already points at the latest release, so nothing to edit there. Post the notes in #announcements yourself.
4. Note what was **not** tested in the release notes; that sentence is worth more than an implied full pass.

- [ ] Tag pushed
- [ ] Release published with `s2x-cr-v1.3.0.zip`
- [ ] Announcement posted

---

# Results

| Check | Pass / fail / skipped | Notes |
|---|---|---|
| 0 Build, package, install, baseline | | |
| 1 Drops and duplicate credits | | |
| 2 Chat bridge and scripting | | |
| 3 Gun Game bot bodies | | |
| 4 Rank and prestige | | |
| 5 Zombies orders and contracts | | |
| 6 Launcher labels and names | | |
| 7 Box 4 upgrade | | |
| 8 Release | | |

Not tested in this pass: (fill in before tagging)
