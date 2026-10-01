# S2x-CR release runbook: v1.6.0

**v1.6.2 addendum (2026-09-30):** for hosts. Two native additions and Server Manager 1.2.2:
the local administration bridge (`-server-manager-admin <nonce>`, from the Manager 1.1.1 work)
and the one-shot `s2x_nextmap` read in the dedicated party (`0815e33`); the Manager adds themes,
auto-balance, chat commands, a key-driven end-of-match map vote and ADMIN; the status card finds
launch-profile servers and names the `_srv` Zombies maps. Tested on OJAMD with two real players
on a LAN-only server built from `rc/v1.6.2` (2026-09-29 and 2026-09-30): ADMIN announce, warn
and kick (after Refresh players; a roster older than 30 s answers `stale_target` by design);
auto-balance replacing bots as people join and leave, the countdown and a live switch;
`!help`, `!rules`, `!discord`, `!nextmap`; map votes by chat and by keys over the final killcam,
early close three seconds after everyone voted, no-vote and tie keeping the rotation, and the
exe loading the winning map. Automated: client build clean, Manager suite 397 PASS, the native
admin harness and the status card test pass. Not run with people: a scorestreak kept across a
live auto-balance switch (verified with bots only), the 160-character admin message, the theme
walk-through. Box 4: section 10, then turn the new options on per preset in the Manager.
Players need nothing. Released 2026-09-30 from `integration` at `6c2295c`:
https://github.com/ChronoRixun/S2x-CR/releases/tag/v1.6.2 (`s2x-cr-v1.6.2.zip`, 18,027,793 bytes).

**v1.6.1 addendum (2026-09-29):** one change on top of v1.6.0, for hosts only. The dedicated
server's player limit never worked: the cfg's `party_maxplayers` and `party_minplayers` are
names the stock engine keeps for itself, and a `set` on them is swallowed before any dvar sees
it, so every server ran 18 wide (a 6v6 on box 4 took 17 humans). The limits are now
`sv_maxplayers` and `sv_minplayers` (#39, `fd5c54a`). Tested on private LAN servers built from
the fix: `sv_maxplayers 12` reports `sv_maxclients 12` and holds `bot_fill 14` to 12, a
Zombies server takes `sv_maxplayers 3`, and a cfg with the old names behaves as it always did.
Not run: a real client joining a capped server as its 13th player, and box 4 itself. The Server
Manager in this zip is 1.0.1: the same Manager with its player cap field writing the new names,
so a preset's cap applies as set. Box 4: section 10 as written; after START ALL each server's
lobby and the master list show x/<cap>. Players need nothing. Released 2026-09-29 from
`integration` at `3c8ad10`: https://github.com/ChronoRixun/S2x-CR/releases/tag/v1.6.1
(`s2x-cr-v1.6.1.zip`, 16,118,845 bytes). The rest of this runbook is v1.6.0's.

**Written:** 2026-09-26 (the v1.5.0 runbook, rewritten for what changed since v1.5.0)
**Build under test:** `integration` at `c45126a`, packaged as `v1.6.0-rc1`: twelve economy fixes (#19, #24 to #32, #36, #37) and upstream #71's singleplayer healer entry. Until the tag exists the exe reports itself as `v1.5.0-28-gc45126a`.
**Repo:** `D:\S2x`. **Game:** `D:\Program Files\Steam\steamapps\common\Call of Duty WWII`. **Box 4:** runs the v1.5.0 code with the Server Manager's launch profiles: four Multiplayer presets, two Zombies servers from a separately installed server package, and the status card with `-Ports`.
**Budget:** about 2 h 30 min: 85 minutes on this PC (sections 0 to 7 and 9), 30 minutes on a second PC with a fresh profile (section 8), 35 minutes for box 4 and the release (sections 10 and 11). The optional steps add 40 minutes (1.3: 20, 1.4: 5, 3.5: 15).

## How to use this

Work top to bottom. Section 0 installs the candidate *the way it ships* and backs up your profile. Sections 1 to 5 then run in one Multiplayer session and one Zombies session on your own profile, starting with the first HQ entry, which is where this build first writes your store. Each section says what changed, how long it takes, exactly what to do, what the console should print, and what was already verified versus never run. A section is done when every box is ticked. If something fails, don't fix it on the spot: capture the evidence listed in that section and note it in the results table.

Where the console wording is known it is quoted verbatim from real runs or from the source. Where it was never observed, the runbook says "look for a line mentioning ..." instead of inventing text. Values that belong to one machine (the second PC, box 4's ports) are in your notes for this release, not here.

Every fix below was live-tested on its own branch build this week (the three prestige fixes also together), and the merged tree passes every harness. **The merged build itself has never been launched: this pass is its first run in the game.** So the pass spends your time on what those tests could not show: things drawn on screen, your long-lived profile, a profile with nothing in it, singleplayer, and the Server Manager on box 4. Steps marked **Optional** or **Can be skipped** say what skipping costs.

## What changed since v1.5.0

| Commit | Change | Tested by |
|---|---|---|
| `b9933f1` | At a master rank, staying in HQ paid about 8 Rare Supply Drops a second; now one drop per new master rank (#37; v1.5.0 has the bug) | 1.2, 1.3 |
| `782a0a4` | The 18 master prestige items are granted, with catch-up at HQ entry (#27) | 1.2, 1.3 |
| `b39b82b` | A division prestige grants its calling card, uniform and weapon variant (#28) | 1.2, 1.4 |
| `8ebb480` | Each Zombies prestige grants its calling card (#29) | 1.2, 5.1 |
| `43b1e2a` | Social ranks pay their rewards; the two win dailies pay 250 Social Score (#30) | 1.2, 3.4 |
| `35b263c` | The drop pool holds every tradeable item, 1,110 to 3,410 (#24) | 2, 8 |
| `71a3d1b` | 47 weapon contracts at 2,500 AC, the LAD contract lowered to 2,500, the contract board rotates (#26) | 3.1, 3.2 |
| `40c7dbe` | 41 variant daily orders pay their named weapon (#36) | 3.3, 3.5, 8 |
| `8b9cb1c` | Nine Quartermaster packs sell the pre-order and promotional cosmetics for Armory Credits (#31) | 4.1 to 4.3, 5.2 |
| `6465e4b` | Mail delivers the 40th anniversary cards and the community event rewards (#32) | 4.4, 4.5, 8 |
| `5271890` | Only the newest 256 card-use (`consume:`) receipts are kept (#25) | 5.3 |
| `1fbd41c` | The hosted preload's map and gametype restore, which never ran, is removed (#19) | 7 |
| `a9b4384` | Arxan healer filter: upstream #71's singleplayer entry (singleplayer startup only) | 6 |

Each commit except `a9b4384` came in through its own merge; `git log --first-parent v1.5.0..c45126a` lists them.

Automated coverage before you start: the merged tree builds with 0 warnings, and the economy harness (build `build\s2x.sln` Release x64 first, then `msbuild tests/economy/zombies.vcxproj /p:Configuration=Release /p:Platform=x64` and `.\build\tests\zombies\bin\zombies.exe`) prints 14 PASS lines and exits 0 on `c45126a`. Its new sections cover card-use receipts, Zombies prestige cards, the 56-row contract board, a variant daily, social ranks, the promo packs, the Mail deliveries and reward-only items never dropping. `tests\economy\overlay.py` (the board overlay's Lua; needs the `lupa` Python package) passes. Four more harnesses kept outside the repo (Multiplayer prestige, master rewards, division rewards, the master replay) pass on the merged tree; the last three fail on v1.5.0. What none of that proves: the merged build running at all, anything it draws, your profile's first HQ entry on it, an empty profile, the hold-to-confirm Purchase button, a real match moving a variant order, and singleplayer.

---

# 0. Before you start

**Time:** 15 minutes.

## 0.1 The candidate the way it ships

rc1 is built and packaged at `D:\S2x\build\release\v1.6.0-rc1\`: `s2x-cr-v1.6.0-rc1.zip` (16,162,442 bytes) and its `stage` folder, 28 files, the Server Manager included. Close the game, any local server and the Server Manager (tray > Exit), then install by copying the stage over the game folder.

```powershell
$game = 'D:\Program Files\Steam\steamapps\common\Call of Duty WWII'
Copy-Item D:\S2x\build\release\v1.6.0-rc1\stage\* $game -Recurse -Force
(Get-FileHash "$game\s2x.exe").Hash -eq (Get-FileHash D:\S2x\build\release\v1.6.0-rc1\stage\s2x.exe).Hash
```

The shape, unchanged since v1.5.0:

| In the zip | Count |
|---|---|
| `s2x.exe`, `s2x.pdb` | 2 |
| `s2x\tools\`: `server-launcher.ps1`, `ServerLauncher.xaml`, `server-status.ps1`, `server-launcher.cmd` | 4 |
| `s2x\ui_scripts\mp\find_match\*.lua` | 6 |
| `s2x\ui_scripts\mp\patches\*.lua` (`__init__`, `cwl_currency`, `dedicated_gametype`, `dedicated_lobby`, `dedicated_members`, `dedicated_party`, `unlocks`) | 7 |
| `s2x\scripts\mp\`: `s2x_gungame_bots.gsc`, `s2x_server_events.gsc` | 2 |
| `s2x\tools\presets\*.json` (starter presets) | 5 |
| `s2x\tools\`: `S2xServerManager.exe`, `S2xServerManager.exe.config` | 2 |

Only if something is fixed during the pass, rebuild and repackage as the next rc. Run this from PowerShell, not Git Bash, which mangles MSBuild's `/t:` and `/p:` switches:

```powershell
cd D:\S2x
git checkout integration; git pull --ff-only
.\tools\premake5.exe vs2022
& 'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe' build\s2x.sln /t:client /p:Configuration=Release /p:Platform=x64 /m /v:minimal
powershell -File build\diagnostics\package-release.ps1 -Version v1.6.0-rc2
```

Two things that have bitten before: `%LOCALAPPDATA%\s2x\data\ui_scripts_off` must stay renamed off (it outranks the game folder), and the `patches` folder must hold all seven files. The `s2x-*.exe` test builds beside `s2x.exe` are not part of the pass; launch `s2x.exe` only.

- [ ] `D:\S2x\build\release\v1.6.0-rc1\stage` holds 28 files in the shape above (`(Get-ChildItem D:\S2x\build\release\v1.6.0-rc1\stage -Recurse -File).Count`)
- [ ] Stage copied; the hash line prints `True`; `s2x\tools\S2xServerManager.exe` carries the rc1 timestamp (2026-09-26 15:47)

## 0.2 Back up the profile and the economy store

The first launch of this build rewrites the store: the new contract and daily pools, the Mail deliveries, and at a master rank the replay's progress record and every catch-up grant. Sections 2 to 5 also spend Armory Credits and drops. Take the backup with the game closed, **before the first launch**:

```powershell
$game = 'D:\Program Files\Steam\steamapps\common\Call of Duty WWII'
$backup = "D:\S2x\build\backups\players2-before-v1.6.0-rc1-$(Get-Date -Format 'yyyyMMdd-HHmmss')"
Copy-Item "$game\players2" $backup -Recurse
Copy-Item "$game\players2\user\hq_economy.json" "$backup-hq_economy.json"
(Get-Content "$game\players2\user\hq_economy.json" -Raw | ConvertFrom-Json).revision
```

Write down the folder name and the revision. To restore, with the game and every `s2x` process closed (`$backup` is the folder above):

```powershell
robocopy $backup "$game\players2" /MIR
Compare-Object (Get-ChildItem $backup -Recurse -File | Get-FileHash).Hash (Get-ChildItem "$game\players2" -Recurse -File | Get-FileHash).Hash
```

No output from `Compare-Object` means the profile is back byte for byte. A restore takes everything back: Armory Credits, drops, purchases, and a real division prestige from 1.4. Unclaimed Mail and unbought packs come back on the next launch, so at the end of the pass you can restore without losing anything the game would not give you again. Decide then whether to keep what the pass left.

- [ ] `players2` and a copy of `hq_economy.json` backed up; folder name and revision written down

## 0.3 Launching

Launch the client from a shortcut, with Steam running and signed in: `C:\Users\Owen\Desktop\S2x Development.lnk` (`-noupdate -multiplayer`) and `D:\S2x\build\backups\S2x Zombies Dev.lnk` (`-noupdate -zombies`). Singleplayer is launched in section 6.

The local dedicated server for section 7 takes its settings from `<game folder>\rc-test.cfg`, left there by the v1.5.0 pass: `+set master_server_enable 0` on the command line is ignored for that dvar, and the leftover `s2x\server.cfg` would override a command-line rotation. Check that the file reads:

```
set sv_hostname "rc test"
set master_server_enable 0
set sv_maxplayers 18
set party_matchStartDelay 15
set bot_fill 8
set bot_names nostalgia
set scr_gun_cycleCount 2
set sv_maprotation "gametype gun map mp_shipment_s2 gametype dom map mp_shipment_s2"
```

Start it from PowerShell, before the client (a running client holds UDP 27016, and a server asked for a taken port moves to the next free one without saying so):

```powershell
Set-Location 'D:\Program Files\Steam\steamapps\common\Call of Duty WWII'; Start-Process -FilePath '.\s2x.exe' -WorkingDirectory (Get-Location) -ArgumentList '-noupdate -dedicated +set net_port 27017 +exec rc-test.cfg +map_rotate'
```

Things that look wrong and aren't: `Dedicated party: failed to select match-rules gametype 'dom'` prints on a working server; `status` answers `Server is not running.` until the first match starts; a map load takes 40 to 50 seconds; the first `bot_fill` line often reads `the engine added 16 of 17` and a second line tops it up.

Reading the console: `<game>\s2x\logs\console.log` is shared by every instance and only ever appended to, so note the line count before a test and read the tail.

```powershell
$log = 'D:\Program Files\Steam\steamapps\common\Call of Duty WWII\s2x\logs\console.log'
$before = (Get-Content $log).Count
# ... test ...
Get-Content $log | Select-Object -Skip $before | Select-String -Pattern 'error|FAILED|FAILURE|exception|assert|HQ economy|HQ AE|HQ mail|HQ purchase|HQ vendor|missing task|Hosted dedicated'
```

Reading the store without the game's menus (read only, safe while the game runs; if it fails mid-write, run it again):

```powershell
$game = 'D:\Program Files\Steam\steamapps\common\Call of Duty WWII'
function Show-Store {
  $s = Get-Content "$game\players2\user\hq_economy.json" -Raw | ConvertFrom-Json
  $ac = ($s.currencies | Where-Object currencyID -eq 6).amount
  $ss = ($s.currencies | Where-Object currencyID -eq 7).amount
  "revision $($s.revision)  AC $ac  Social Score $ss"
  foreach ($g in 1, 2, 6) { 'drops {0}: {1}' -f $g, ($s.inventory | Where-Object { $_.guid -eq $g -and $_.collision -eq 0 }).quantity }
  $s.transactions | Group-Object { $_.id.Split(':')[0] } | Sort-Object Name | ForEach-Object { '{0} {1}' -f $_.Name, $_.Count }
  $s.achievements | Where-Object name -eq 'player_master_prestige_ranks' | ForEach-Object { "master progress $($_.progress)/$($_.progressTarget) $($_.status)" }
}
Show-Store
```

It prints the revision, Armory Credits and Social Score; `drops 1`, `drops 2` and `drops 6` (common, Rare and Rare Zombie supply drops); the receipt count per kind (`prestige`, `mail`, `social`, `consume`, `item-data`, `purchase`, ...); and, once this build has stored it, the master replay's progress. Console commands used below: `hqownership <guid>` (run it in HQ or the Zombies lobby, not at the main menu, where the same item can read locked), `hqwallet`, `hqeconomy`, `hqgrant item <guid> <n>`. Typing into a server console from a script: `D:\S2x\build\backups\syscon2.ps1 -TargetPid <pid> -Commands 'status'`.

- [ ] `Show-Store` run before 0.4's launch (the first launch of this build) and its output saved: this is v1.5.0's store

## 0.4 The updater stays quiet

Launch `s2x.exe` directly for this check, without any flags, because the desktop shortcuts pass `-noupdate`. Quit at the main menu; do not enter HQ yet.

- [ ] Console prints `[Updater] Automatic updates are off in this fork; new builds are at https://github.com/ChronoRixun/S2x-CR/releases`
- [ ] No `ui_scripts` folder under `%LOCALAPPDATA%\s2x\data` after the launch (only the renamed `ui_scripts_off-*` folders)

## 0.5 Baseline pass

Multiplayer only, and only to the main menu: do not enter HQ yet, section 1 is the first HQ entry. The Zombies baseline is 5.1.

- [ ] MP frontend reaches the main menu; the menu shows `v1.5.0-28-gc45126a`; server browser and UNLOCKS tab present
- [ ] New console lines contain no `error` you have not seen before and no `FAILED`
- [ ] No new file in `<game>\minidumps` newer than the start of the pass

---

# 1. First HQ entry and the master rank replay: #37, #27, #28, #29, #30

**What changed:**
- **#37, the one new bug:** at a master rank (prestige 10, level 56 or above) the HQ hub replays your master rank-ups from a progress record the fork never stored. So the replay restarted at the first master rank about every 1.2 seconds, and each rank-up paid a Rare Supply Drop: about 8 a second, for as long as you stayed. The fork now keeps that record (`player_master_prestige_ranks`), the replay resumes where it stopped, and each master rank pays one drop, once. v1.5.0 has the bug.
- **#27:** the 18 master prestige items: a camo and a helmet at level 56, a uniform at level 100 and every 100 levels to 1000, camos at 200, 400, 600, 666, 800 and 1000. A profile already at a master rank gets everything up to its level on its next HQ visit, because the replay re-sends the highest rank reached.
- **#28:** a division prestige grants level 2's calling card, level 3's uniform and level 4's weapon variant. A level passed before this build is paid with that division's next prestige.
- **#29:** a Zombies prestige grants its calling card, levels 1 to 10, in the same way.
- **#30:** each social rank pays its reward (Armory Credits, drops, cards, emotes, weapon variants, the rank 20 nameplate). HQ entry pays every rank your Social Score has reached.

The game re-sends these events at every HQ entry. Each grant has a permanent receipt, so a re-send pays nothing.

**Why first:** a mistake here costs players drops or items without anyone pressing anything, and it is the first thing this build writes to your store. If drops climb while you stand in HQ, the release stops here.

**Time:** 10 minutes. Optional: 1.3 20 minutes, 1.4 5 minutes.

## 1.1 Where your profile stands

At the MP main menu from 0.5: run `Show-Store` and note AC, Social Score, `drops 2` and the `prestige`, `social` and `mail` counts. Note your prestige and level. A v1.5.0 store has no `master progress` line. Note the console line count.

## 1.2 Enter HQ and stay

1. PLAY > Headquarters. Once HQ has loaded, touch nothing for 3 minutes. Run `Show-Store`, wait one more minute, run it again.
2. **Below a master rank:** no `[HQ AE]` line saying `granted` during the stay, except `[HQ AE] social rank <n>: granted its reward` once per rank if you hold 100 Social Score or more. `drops 2` is the same in all three reads.
3. **At a master rank:** within about 10 seconds, one `[HQ AE] rank up: granted a Rare Supply Drop` per master rank you hold (a one-time catch-up: this is the first build that records them) and one `[HQ AE] master rank <n>: granted its rewards` per threshold reached; then nothing more for the rest of the stay. The last two reads show the same `drops 2` and `master progress <level>/1000 inProgress`. On v1.5.0 the same stay paid about 500 drops a minute.
4. No `[HQ AE] prestige <n>: granted its helmet and calling card` line (v1.5.0 paid those and their receipts block a second grant), no `division prestige` line and no `Zombies prestige` line.
5. `hqwallet`: AC as in 1.1, plus 200 if a payroll came due.
6. Quit, relaunch, enter HQ again for a minute: no grant line, `drops 2` unchanged.

If drops climb: leave HQ at once, quit, note the count and the time, and restore from 0.2.

## 1.3 Optional: the master path on a throwaway copy

**Time:** 20 minutes. Skip it if you are at a master rank: 1.2 was the real test. Otherwise this is the only run of #37 and #27 on the merged build; skipping leaves them proven by the harnesses on the merged tree and in the game only on the branch build.

1. Quit the game. Back up again with the 0.2 commands: this is the copy you restore in step 6.
2. Launch MP and press PLAY: at the main menu `setrank` answers `player stats are not available`, the stats load in the Multiplayer menu. Do not pick Headquarters. Console: `setrank 101 10` (it prints `prestige 10, level 101 applied`), then `uploadStats`. Quit.
3. Relaunch, note the console line count, PLAY > Headquarters, stay 3 minutes.
4. Expect `[HQ AE] prestige 3: granted its helmet and calling card` through `prestige 10` (v1.5.0's prestige catch-up: the copy jumped to prestige 10), exactly 46 `[HQ AE] rank up: granted a Rare Supply Drop` lines within about 10 seconds, `[HQ AE] master rank 56: granted its rewards` and `[HQ AE] master rank 100: granted its rewards` once each, then no new `[HQ AE]` line. `Show-Store`: `drops 2` up by 46, `master progress 101/1000 inProgress`.
5. In HQ, `hqownership 0x6632116`, `0x704001E` and `0x6000039` (the level 56 helmet and camo, the level 100 uniform): `usable=1 lock=0` and `CAC=Unlocked`; `0x6021039` (level 200) is not owned. If you can spare a minute, find the helmet in Barracks > Special Helmets or the camo in Create-a-Class: nobody has looked at a master item on screen. Quit, relaunch, one minute in HQ: no grant line, `drops 2` unchanged.
6. Quit and restore the copy from step 1 (0.2). `Show-Store` reads as it did before step 2.

## 1.4 Optional: a real division prestige

**Time:** 5 minutes. A division prestige resets that division's level: it is real play, and a later restore from 0.2 undoes it. Only if a division is at its top level: Divisions, pick it, and prestige it with the menu's own button. Console: one `[HQ AE] <division> division prestige <n>: granted its reward` line per level paid, so a division that reached prestige 2 before this build and now goes to 3 prints two lines (2 and 3). The division's items are its row in `division_prestige_rewards` (`src\client\game\demonware\achievement_engine.cpp`): card, uniform, variant. `hqownership` on each one paid reads `usable=1 lock=0`; the uniform is listed under Divisions > the division > Personalize > Uniforms. Skipping costs: the menu button was never pressed; the test called the game's prestige function directly, and called without a controller that function drops the event (the next prestige would then pay it).

**Evidence if it fails:** the console tail from the HQ load, every `Show-Store` read with its time, `hqeconomy` output.

**Verified already:** #37 and #27 in the harnesses on the merged tree (a replay from a new store at level 101: 7 runs and 46 drops; at 1000: 945 drops and the 18 items once; nothing on a revisit), and live on the branch build: at prestige 2 no drop; at 101, 46 drops within 8 seconds and nothing in the next 3 minutes; a re-entry paid nothing; at 1000, the 18 items once and nothing after. #28 live through the game's prestige function (a level-2 card paid late, with level 3's uniform; 8 items unlocked). #29 live through four real Zombies prestiges (cards 1 to 4, unlocked in both modes). #30 live (ranks 1 to 20 paid once, the rank-up splashes on screen). Never run: the merged build, a master item on screen, the Divisions menu's button, a Zombies prestige card in the Dossier.

## Checklist

- [ ] Four minutes in HQ: `drops 2` equal across the reads, no drop line (or at a master rank: one per rank, once, then none)
- [ ] No second prestige grant, no stray division or Zombies grant, AC unchanged
- [ ] Relaunch and re-entry: nothing granted
- [ ] Optional 1.3: prestige 3 to 10 once, 46 drops then none, `master rank 56` and `100` once, items unlocked, copy restored and compared
- [ ] Optional 1.4: one line per level paid, items owned (or skipped, say so)

---

# 2. Supply drops hold every tradeable item: #24

**What changed:** the drop pool was the 774 collection items plus the Epic and Heroic weapon variants: 1,110 items. It now holds every item with a trade-in value, 3,410, except the items another channel grants (prestige, master, division, social rank, Mail and the Quartermaster packs), which stay out; weapon variants that were already in the pool stay in. Tier odds and the Rare-or-better first card are unchanged. Because the pool is three times bigger, a weapon variant turns up in about one drop in ten, down from one in five.

**Time:** 10 minutes.

## What to test

1. In HQ. If `drops 2` is 0: `hqgrant item 2 3` (drops you did not earn; the 0.2 restore takes them back).
2. HQ menu > E Supply Drops > Rare Supply Drop > ENTER, then "Open Next" for three drops. Every reveal finishes with three cards; card 1 of every drop is Rare or better.
3. About 7 cards in 10 are items outside the Quartermaster collections, new to the pool. A collection item shows its collection count on the reveal (the test run showed `5/6` on an emblem); a new item shows none.
4. A duplicate card pays its displayed value: AC before plus the duplicates equals `hqwallet` after.
5. Take one new item and find it in its menu: a calling card in Barracks > Calling Cards, an emblem, a camo in Create-a-Class, a uniform under Divisions > Personalize. It is listed and selectable. This has only been read from `hqownership`, never picked on screen.
6. Optional: `hqgrant item 1 3` and three common drops (`sd_mp`).

**Evidence if it fails:** the drop type, the three cards (screenshot), AC before and after, the console tail from the click.

**Verified already:** harness: 500 openings keep the Rare floor and the tier weights, reward-only items never drop; a pool check against the game's tables counts 3,410 with no reward or sold item in it. Live on the branch build: 31 drops (93 cards), 72 cards outside the collections, none outside the pool, no Rare-floor miss, duplicates paid 896 AC exactly, four new items unlocked; two Zombies drops gave 2 + 3 cards. Never run: the merged build, a new item picked from its menu.

## Checklist

- [ ] Three Rare drops: reveals finish, card 1 Rare or better, duplicate credit adds up
- [ ] Most cards from outside the collections; one of them selectable in its menu

---

# 3. Contracts and daily orders: #26, #36, #30

**What changed:**
- **#26:** retail's 47 weapon contracts join the fork's nine (eight generic ones and the LAD). Each day the board shows nine contracts in a row of that pool of 56 and moves one row a day, so it holds one or two generic contracts and seven or eight weapon contracts, and each weapon comes back for nine days every eight weeks. A weapon contract costs 2,500 AC and pays that weapon's Rare variant. The LAD contract is lowered to 2,500 too (retail charged 5,000).
- **#36:** the daily pool grows from 20 orders to 61: 41 of retail's variant dailies, each paying its named Epic or Heroic variant, sit between the generic ones, six a day. The 22 variant dailies whose kill rule is not captured yet are not offered.
- **#30:** the Domination Victor and Team Player dailies pay 250 Social Score. With the bigger pool they are on the board only on some days.

**Time:** 10 minutes. Optional: 3.5 15 minutes, after the daily reset.

## What to test

1. HQ > Contracts: nine contracts; seven or eight titled "<weapon> Contract" at 2,500 AC with the weapon shown as the reward, and one or two generic ones at their old prices (for example SMG Kill Contract, 350 AC, 3000 XP).
2. Buy a weapon contract you would like to finish (2,500 AC; it stays yours): hold Purchase Contract until its bar fills, about a second (a tap does nothing). Console `[HQ purchase] sku=<n> quantity=1 error=0`; AC down by 2,500 once; the tile reads Contract Activated with its time to complete and the weapon. Skipping costs: the hold-to-confirm button has never been pressed by hand on this build; the test called the button's own purchase function.
3. Orders: a daily board that v1.5.0 made before the reset (00:00 UTC, 19:00 CDT) keeps its six orders until they end. After the reset the board comes from the new pool: a variant order is titled `"<variant>" Order` and shows that weapon as its reward, with the weapon's picture.
4. If Domination Victor or Team Player is on the board, its reward reads 250 SOCIAL SCORE. On most days neither is; your notes list the next dates. Skip if absent.
5. Optional, after the reset: accept a variant order and play one match (the section 7 server will do). It moves only with kills of its weapon class: an LMG order ignores SMG kills. If it completes, claim it: the weapon is owned and selectable in Create-a-Class. Skipping costs: a variant order has never moved in real play (the test fed it events; generic orders with the same kind of rule move in real play).

**Evidence if it fails:** a screenshot of the board or tile, AC before and after, the console tail, `hqeconomy` output.

**Verified already:** #26 harness (56 rows with one SKU each, purchase, activation, claim, an off-board purchase refused); the board on screen exactly as predicted for the day (nine tiles, 2,500 AC, weapon rewards, the generic row unchanged); a purchase through the button's call debited 2,500 once and activated the contract; a completion set in the store paid the Sten once, unlocked. #36 harness (class rule, one payout); on screen `"Commander" Order` and `"Torpedo" Order` with their Kar98k variants; a claim driven by test events paid once. #30: both win dailies read 250 SOCIAL SCORE; one claimed through test events paid it once. Never run: the button held by hand, a contract or variant order finished in real play, the merged build.

## Checklist

- [ ] Contract board: nine tiles, weapon contracts at 2,500 with their weapon shown
- [ ] One weapon contract bought by holding the button: one debit, Contract Activated
- [ ] After the reset: variant orders on the board with their weapon (or not reached yet, say so)
- [ ] Win daily reads 250 SOCIAL SCORE (or not on the board, say so)
- [ ] Optional 3.5: a variant order moved in a real match, claim paid the weapon

---

# 4. Quartermaster packs and Mail: #31, #32

**What changed:**
- **#31:** nine packs on the Quartermaster's Deals page sell the pre-order, beta, partner and paid-pack cosmetics that had no way to be earned: C.O.D.E., Beta and Ambassador, Zombies, Divisions Pre-order, C.O.D.E. Viper, C.O.D.E. Endowment, Partner, Twitch Uniform and Army Men. A pack with a uniform costs what the Armory charges for its uniforms; the others cost 1,000 AC, like the CWL pack. Each can be bought once, and a bought pack's tile moves on to the next pack. The C.O.D.E. packs also carry the unlock item Zombies checks, so their items read unlocked in Zombies too.
- **#32:** HQ Post delivers two new messages to every profile: "Activision 40th Anniversary Calling Cards" (ten cards) and "Community Event Rewards" (11 items: the Community Care helmet, a grip, three calling cards, two charms, three universal camos and the Cruiser II Sawed-Off variant).

**Time:** 10 minutes.

## What to test

1. HQ > Quartermaster, Deals: the pack tiles show their name, picture and AC price (in the test: C.O.D.E. 1,000, C.O.D.E. Viper 1,000, Partner 1,000, Army Men 6,500, beside the two drops).
2. Open the C.O.D.E. Pack: its name, the promo line `3 cosmetics`, the price and limit `0/1`. The "Guaranteed Contents" list is empty: that menu only lists supply-drop contents (a known issue).
3. Buy it (1,000 AC; it stays yours): `[HQ purchase] sku=33554436 quantity=1 error=0`, AC down by 1,000 once, limit `1/1`, PURCHASE gone, and the tile shows the next pack. `hqownership 0x2000004`, `0x2400225` and `0x6600100` (its emblem, calling card and helmet): `usable=1 lock=0`. 5.2 checks them in Zombies.
4. HQ Post: two new messages, each with its own icon. Collect both: `[HQ mail] redeem id=2 slot=9 success=1` and `[HQ mail] redeem id=3 slot=10 success=1` (the slot numbers can differ); the inbox is empty afterwards and `Show-Store` shows `mail` up by 2.
5. On screen: Barracks > Calling Cards lists the ten anniversary cards; equip one and see it on your player card. In Create-a-Class, a weapon's camo list holds Redacted and a camo shown as Beach Side (its name was inferred, never read in the game: note what it shows). Neither camo has been granted in the game before.

**Evidence if it fails:** screenshots of the tile, the details page and HQ Post; AC before and after; the console tail.

**Verified already:** #31 harness (every item for one debit, one replay, a rebuy and short funds refused); live on the build before its last data change: the Deals tiles on screen, the C.O.D.E. Pack bought once for 1,000, four packs and their 24 items owned, a pre-order uniform equipped; in Zombies the C.O.D.E. items read locked until the unlock item was granted by hand, which is why the packs now carry it. #32 harness; live: both messages in HQ Post with their icons, collected, every item owned and unlocked, Cruiser II included. Never run: the packs as shipped, Redacted and Beach Side in the game, a Mail item picked from its menu.

## Checklist

- [ ] Pack tiles render with names, pictures and prices
- [ ] C.O.D.E. Pack bought once: one debit, limit 1/1, its three items unlocked
- [ ] Both new Mail messages collected: `success=1` twice
- [ ] Anniversary card equipped; Redacted and Beach Side in a camo list

---

# 5. Zombies: card use, the packs, prestige cards: #25, #31, #29

**What changed:** #25 keeps only the newest 256 card-use receipts, as v1.5.0 did for item-data receipts; the old unbounded ones still answer their replays and are the first to go. #31 and #29 are described in sections 4 and 1; this section checks their Zombies side.

**Time:** 10 minutes.

## What to test

1. Zombies baseline: the Zombies frontend reaches the main menu, the UNLOCKS tab shows the Zombies rows, no `FAILED` line, and no `[HQ AE] Zombies prestige` line (your Zombies prestige cards are paid at your next Zombies prestige, not now).
2. If you bought the C.O.D.E. Pack in 4.3: in the Zombies lobby, `hqownership 0x2000004`, `0x2400225` and `0x6600100` read `lock=0`. Before the fix they read `lock=6` here.
3. Consumables (v1.5.0's #10 check, short). `Show-Store`, note `consume`. List the new-style receipts (none on a store from before this build):

   ```powershell
   (Get-Content "$game\players2\user\hq_economy.json" -Raw | ConvertFrom-Json).transactions | Where-Object { $_.id -like 'consume:*' -and $_.request -like 'sequence:*' }
   ```

   Equip a card you hold two or more of, start a solo match from PLAY and use it once: its effect happens. Back in the lobby: the card's count in the store is one lower (the HUD can show fewer charges than the store holds: 2 shown with 3 held in the test), `consume` is one higher, no `missing task '96'` line, and the list above holds one receipt reading `sequence:<n>:<guid>:1;`.
4. Optional: `hqgrant item 6 1`, then E Supply Drops > Rare Zombie Supply Drop: two regular cards, then three consumables; card 1 Rare or better.

**Evidence if it fails:** the card, its count before and after, the console tail of the match.

**Verified already:** #25 harness (256 kept, the oldest and the legacy ones removed first, replays still answered or refused); live: one Flamethrower use wrote a `sequence:` receipt and the card went from 3 to 2. The C.O.D.E. items' Zombies lock with and without the unlock item. Never run: 256 card uses live, the pack as shipped in Zombies.

## Checklist

- [ ] Zombies frontend and UNLOCKS tab; no stray prestige grant
- [ ] C.O.D.E. items `lock=0` in Zombies (or pack not bought, say so)
- [ ] A consumable loses one charge, `consume` +1, one `sequence:` receipt, no `missing task '96'`

---

# 6. Singleplayer startup: `a9b4384`

**What changed:** the fork carried upstream #71 (the Arxan healer that undid three S2x patches at startup) as first opened, with the singleplayer slot empty. It now takes the merged upstream version, which filters the singleplayer healer too. Singleplayer startup only; Multiplayer, Zombies and servers are unchanged.

**Time:** 5 minutes.

## What to test

1. With Steam running, from PowerShell:

   ```powershell
   Set-Location 'D:\Program Files\Steam\steamapps\common\Call of Duty WWII'; Start-Process -FilePath '.\s2x.exe' -WorkingDirectory (Get-Location) -ArgumentList '-noupdate -singleplayer'
   ```

2. If the game asks "Set Optimal Settings?", answer No. The campaign main menu appears; quit from it.
3. No crash box and no new file in `<game>\minidumps`.

A crash with `0xC0000409` is what this change is about: record it and stop. An access violation (`0xC0000005`, an S2x ERROR box before the menu) is a separate, known startup crash in the anti-debug code, about one singleplayer start in 20, before and after this change: note it and relaunch once.

**Evidence if it fails:** the crash box text, the newest minidump, the console window's last lines.

**Verified already:** the same two files, on the upstream branch: 40 singleplayer starts with no `0xC0000409` and no patch undone; the four failures were the known access violation (six in 40 without the change). The files are byte-identical to upstream's merge. Never run: a fork build in singleplayer.

## Checklist

- [ ] Singleplayer reaches the campaign main menu, no `0xC0000409`, no new minidump

---

# 7. Dedicated lobby: #19, with v1.5.0's HUD limits and browser checks

**What changed:** at a hosted preload the client tried to restore the go's map and gametype, but only while a flag was set that covers the handling of the go. The engine runs the preload after that, so the block never ran (the flag read 0 at all 8 preloads measured). It is removed, and the comments now say where the map comes from. No behaviour change is expected: this is the regression check, and it repeats v1.5.0's section 2 in short.

**Time:** 10 minutes. **Can be skipped:** box 4's step 10.6 joins a dedicated server on the zip. Skipping costs a gametype-changing rotation on the zip, and a local LAN-only server's effect on the browser.

## What to test

1. Start the local server (0.3) and wait for its first match (`status` answers).
2. Launch the client and press PLAY (at the main menu `connect` answers `Cannot connect: virtual lobby is not loaded.`), then `connect 127.0.0.1:27017`. The client console prints `Joining hosted dedicated lobby on map 'mp_shipment_s2' gametype 'gun'.` and `Hosted dedicated lobby: gun limits 36/1/1.` The HUD reads `GUN RANK: 0 / 36`.
3. Once you are in the match (not on the loading screen), send `endMatch` to the server: `Hosted dedicated lobby: match updated to mp_shipment_s2 dom.` and `Hosted dedicated lobby: dom limits 200/1/2.` Domination loads with its A, B and C flags. Its HUD shows team scores only, so the console line is the check.
4. `endMatch` again: `Hosted dedicated lobby: gun limits 36/1/1.` and `0 / 36` again.
5. Quit the server and the client. Launch the client the normal way and open the server browser: the console logs `[server_list] requesting S2 servers from ...` and `queued <n> server(s) from master`, and the list fills. Do this after any LAN-only server has run from this folder.

**Evidence if it fails:** a screenshot of the HUD, the client console tail from the connect, the server's gametype.

**Verified already:** live on the branch build with the same cfg: the join, gun to dom and dom to gun, with the lines and HUD values above. Never run: the zip.

## Checklist

- [ ] Join: `gun limits 36/1/1.`, HUD `0 / 36`
- [ ] Rotation to Domination and back: `match updated`, `dom limits 200/1/2.`, then 36 again
- [ ] After the server quit: `[server_list] requesting` and the browser fills

---

# 8. A fresh profile on a second PC

**What it covers:** everything a new player meets first, on a store with nothing in it: the three Mail messages, the day's boards, the Quartermaster packs, and a real level-up paying a Rare drop from the new pool. Nothing else in this pass starts from an empty store. Your notes for this release have the second PC's network setup and one extra Zombies check.

**Time:** 30 minutes.

## What to test

1. On the second PC, close the game. Copy the rc1 zip there and extract it over that PC's game folder, as a player would (`Expand-Archive <zip> -DestinationPath <game folder> -Force`). Move that PC's profile aside; it goes back in step 8:

   ```powershell
   $game = '<game folder on that PC>'
   Rename-Item "$game\players2" 'players2-before-v1.6.0-rc1'
   ```

2. Launch MP with Steam running. Note every prompt on the way (Windows, firewall, "Set Optimal Settings?") with its wording. The menu shows `v1.5.0-28-gc45126a`; the server browser fills.
3. PLAY > Headquarters. HQ Post lists three messages: Welcome to Headquarters, Activision 40th Anniversary Calling Cards and Community Event Rewards, each with its own icon. Collect all three: `[HQ mail] redeem id=1 ...`, `id=2` and `id=3`, each `success=1`; AC up by 500.
4. Barracks > Calling Cards: the ten anniversary cards; equip one.
5. Orders: six dailies from the new pool, variant orders showing their weapon. Contracts: nine, as in 3.1. Quartermaster: the pack tiles. Look only.
6. Play one Multiplayer match on a server from the browser. A new profile levels up in its first match: back in the menus the console has one `[HQ AE] rank up: granted a Rare Supply Drop` per level gained. Open one of those drops: card 1 Rare or better, most cards from outside the collections.
7. Zombies: launch, main menu, UNLOCKS tab. Then the Zombies check in your notes.
8. Quit. Put the PC's own profile back and keep the fresh one for evidence:

   ```powershell
   Rename-Item "$game\players2" 'players2-fresh-v1.6.0-rc1'
   Rename-Item "$game\players2-before-v1.6.0-rc1" 'players2'
   ```

**Evidence if it fails:** that PC's `s2x\logs\console.log` tail, a screenshot, the kept fresh profile folder.

**Verified already:** nothing on an empty store, in this release or v1.5.0. Each piece above was checked on this PC's long-lived profile.

## Checklist

- [ ] Zip extracted; prompts noted; version and browser right
- [ ] Three Mail messages collected, AC +500; an anniversary card equipped
- [ ] Boards and packs render on a new profile
- [ ] A real level-up paid a Rare drop; it opens with a Rare-or-better first card
- [ ] Zombies frontend; the check in your notes; the PC's own profile put back

---

# 9. Regression: what v1.5.0 proved

**Time:** 5 minutes: most of it is ticked by the sections above. Nothing in v1.6.0 touches these.

- [ ] Server browser fills after a LAN-only server ran from this folder (7.5)
- [ ] Dedicated HUD and scoreboard limits (7.2 to 7.4)
- [ ] Launch profiles: box 4's package still registered, its servers start and stop from the Manager (10.4, 10.5)
- [ ] Zombies consumables used up (5.3)
- [ ] After 00:00 UTC: a new MP daily set, accepted orders carried
- [ ] On this PC: a native Manager preset starts and stops, the tray works, `s2x\tools\server-launcher.cmd` opens the old launcher
- [ ] Status card dry run on the section 7 server: `powershell -File "$game\s2x\tools\server-status.ps1" -ServerHost 127.0.0.1 -Port 27017 -DryRun`; the Multiplayer line still ends `bots fill the rest of N`
- [ ] Unchanged since v1.4.0, if you see them: Gun Game bots keep full bodies through promotions (the section 7 server), killcam and podium emotes on a dedicated server

---

# 10. Box 4 upgrade

**Time:** 20 minutes. Only after sections 0 to 7 are ticked. Box 4 has no Steam and runs its Multiplayer servers from the game folder; the package's Zombies servers are started through the Manager's launch profiles. Its ports, the package's folder and how files reach box 4 are in your notes. Nothing in v1.6.0 changes a server; the upgrade keeps box 4 on the release that players get.

1. Copy `D:\S2x\build\release\v1.6.0-rc1\s2x-cr-v1.6.0-rc1.zip` to box 4.
2. Manager: STOP ALL for the Multiplayer presets, then STOP each profile server (whether they run the game folder's exe was never checked; stopping them is the safe side). Exit the Manager from the tray: its exe is replaced.
3. Extract the zip over the game folder, replacing what is there.
4. Start `s2x\tools\S2xServerManager.exe`. LAUNCH PROFILES... still lists the package with every mode and no orange `start script missing: ...` line; the presets are unchanged.
5. START ALL, then LAUNCH SERVER on each profile preset.
6. From this PC: every box 4 server appears in the browser within a minute; join a Multiplayer one: its HUD shows the server's own score limit; the Discord card updates within two minutes with every server and the right map names.

- [ ] Zip on box 4, servers stopped, files replaced
- [ ] Package still registered; presets unchanged
- [ ] Servers back up, listed, joined, HUD limit right, card updated

---

# 11. Cut the release

**Time:** 15 minutes.

1. Tag the tested commit and push the tag: `git tag v1.6.0 c45126a; git push origin v1.6.0`. If anything was fixed during the pass, tag the newer tip. If the runbook's results commit lands on integration first, you may tag that instead: only `tests/RUNBOOK.md` differs.
2. Rebuild at the tag from PowerShell (premake and MSBuild as in 0.1) so the exe reports `v1.6.0` instead of `v1.5.0-28-gc45126a`, then `powershell -File build\diagnostics\package-release.ps1 -Version v1.6.0`. The zip is `D:\S2x\build\release\v1.6.0\s2x-cr-v1.6.0.zip`.
3. Before publishing, re-check the final build: install its stage (0.1: 28 files, the hash line `True`), then 0.5 on it (the menu and the console show `v1.6.0`, MP and Zombies reach their main menus, no `FAILED`, no new minidump), and a fresh client logs `[server_list] requesting S2 servers from ...` and fills the browser.
4. Upload `s2x-cr-v1.6.0.zip` to a GitHub release on the tag. Notes: `D:\S2x\build\research\release-notes-v1.6.0-draft.md`; fill its "Tested" and "Not tested" lines from the results table below.
5. The Discord download card already points at the latest release. Post the notes in #announcements yourself.
6. Box 4 runs the rc; install the final zip there when convenient (only the version string differs).

- [ ] Tag pushed
- [ ] Rebuilt at the tag; 0.1 and 0.5 re-checked on the final build; browser fills
- [ ] Release published with `s2x-cr-v1.6.0.zip`; "Tested" and "Not tested" lines filled; announcement posted

---

# Results

| Check | Pass / fail / skipped | Notes |
|---|---|---|
| 0 Install, backup, updater, baseline | pass | rc1 installed 16:20 with players2 backed up; 0.4 by a launch without `-noupdate` (updater line in the console window, no data folder); 0.5 version `v1.5.0-28-gc45126a`, browser, UNLOCKS |
| 1 First HQ entry, master replay, prestige rewards | pass | 1.2 four minutes in HQ at prestige 2: nothing granted, store untouched. 1.3 on a throwaway copy at 101: exactly 46 drops in one burst, prestige 3-10 and master 56/100 once, then silence; items unlocked and on screen; restored by hash. 1.4 skipped |
| 2 Supply drops | pass | three Rare drops: card 1 Legendary/Rare/Epic, 7 of 9 cards new to the pool, one duplicate paid 12 AC; a new uniform and calling card selectable on screen |
| 3 Contracts and daily orders | pass | Orso contract bought by holding the button: one 2,500 debit, activated. After the reset the board rotated to the Lewis variant orders with their pictures; the English Oak order finished in real play and paid the weapon. 3.4 skipped (no win daily until 2026-09-29) |
| 4 Quartermaster packs and Mail | pass | C.O.D.E. Pack: one 1,000 debit, items unlocked, tile moved on (MP and Zombies); both new Mail messages collected; anniversary cards and Redacted on screen |
| 5 Zombies | pass | UNLOCKS rows; C.O.D.E. items unlocked in Zombies; three card uses wrote three `sequence:` receipts and each count went down by one; a level-up paid a Rare Zombie drop |
| 6 Singleplayer startup | pass | launched without `-noupdate`: loaded, alive 100 s, no crash, no minidump (campaign menu not seen by eye) |
| 7 Dedicated lobby | pass | join `gun limits 36/1/1`, endMatch to `dom limits 200/1/2` and back to `gun limits 36/1/1`; a fresh client afterwards had `master_server_enable 1`. `connect` needs PLAY first |
| 8 Fresh profile, second PC | pass (on this PC) | empty profile on Owen's PC: no prompts, six dailies from the new pool, three Mail messages collected (+500 AC, 21 items), packs and nine contracts render, Zombies menu; profile restored by hash. Not done: first-match level-up (proven on Owen's profile), the second PC and its Zombies check |
| 9 Regression list | pass | browser after a LAN-only server (7), HUD limits (7), launch profiles and the status card on Box 4 (10), Zombies consumables (5) |
| 10 Box 4 upgrade | pass | rc1 on Box 4; all six servers on the master and answering; card 6 servers with the right Zombies lines. Joining a Box 4 server for its HUD not done (limits passed in 7) |
| 11 Release | pass | tag v1.6.0 at 619dde0 pushed; rebuilt at the tag (console `S2x: v1.6.0`, browser fills); release published with s2x-cr-v1.6.0.zip 2026-09-26 |

Not tested in this pass: the second PC (the empty-profile checks ran on this PC instead) and its Zombies loadout check; a first-match level-up on an empty profile; 1.4 (a real division prestige); 3.4 (no win daily on the board); Known before it starts: a first-run SmartScreen prompt on a machine that never ran the fork; a contract finished in real play; the 22 variant dailies that are not offered; 256 card uses live; weekly order rotation; a remote client on a Gun Game server.
