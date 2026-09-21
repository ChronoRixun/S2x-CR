# Morning test runbook

The combined build includes the expanded Zombies catalog, duplicate-drop credits (#6), rank commands/chooser (#7), Gun Game bot outfits (#4), and server scripting support (#9). Unchecked live items below are not claimed as verified. Revive/purchase/wave objectives (#8) remain deferred until their live event semantics are confirmed.

## Install once

Close the game and local dedicated server first. Keep a copy of the current `s2x.exe`, matching PDB and `players2` directory before testing. Rank commands deliberately change progression; restoring a profile backup also restores its old economy balance, so do rank testing before testing rewards, or keep the new rank instead of restoring.

Copy these from this checkout into the normal game directory:

| Source in `D:\S2x` | Destination relative to the game directory |
| --- | --- |
| `build\bin\x64\Release\s2x.exe` | `s2x.exe` |
| `build\bin\x64\Release\s2x.pdb` | `s2x.pdb` |
| `data\ui_scripts\mp\patches\unlocks.lua` | `s2x\ui_scripts\mp\patches\unlocks.lua` |
| `data\scripts\mp\s2x_gungame_bots.gsc` | `s2x\scripts\mp\s2x_gungame_bots.gsc` |
| `data\scripts\mp\s2x_server_events.gsc` | `s2x\scripts\mp\s2x_server_events.gsc` |
| `tools\server-status.ps1` (optional server tool) | `tools\server-status.ps1` |

The Lua and GSC files are loose files: copying the EXE alone does not install those changes. Keep the rest of the existing `patches` folder, including `__init__.lua`. The Gun Game GSC belongs on the server hosting the match as well. Do not reset or delete `hq_economy.json`; the earlier claiming results and inventory should carry forward.

Launch with `-noupdate -demonware_debug`; add `-zombies` for Zombies. The debug option records event evidence for progression failures. Default console log: `s2x\logs\console.log` under the game directory.

## 1. Rank commands and chooser — CR #7

Allow about five minutes plus a map load. Record your original rank/prestige first.

- [ ] In MP, run `setrank 1`, reopen Soldier: level **1**.
- [ ] Run `setrank 2`, reopen: level **2** (base XP 4,000).
- [ ] Run `setrank 3`, reopen: level **3** (base XP 8,600).
- [ ] Open UNLOCKS > Prestige and Rank. Its selected level agrees with Soldier. Choose level 4 and Apply; reopen Soldier and confirm **4**.
- [ ] Run `setprestige 1`: prestige **1**, level **1**. This sets progression; it does not award prestige cosmetics.
- [ ] Switch to Zombies and run `setrank 3`. Confirm level **3** there; MP progression should not change.
- [ ] In each tested mode, load a map before quitting so the stats can upload. Restart and confirm the last rank persists.
- [ ] After setting a rank, earning XP still increases progress. If convenient, also claim an XP-bearing MP reward and check that new reward XP counts.

The fix removes *past* inventory reward XP from the requested rank by updating its baseline. It leaves the economy wallet unchanged. A screenshot of the console output and Soldier screen is useful if they disagree.

## 2. Duplicate drop credits — CR #6

Allow about five minutes if you have drops. An opening without a duplicate is inconclusive for the credit fix.

- [ ] Record the AC balance; `hqwallet` and `hqeconomy` provide diagnostic readings.
- [ ] Open an MP common drop containing a duplicate. Add the duplicate card values: the final AC increase should equal that total.
- [ ] Repeat with an MP rare drop if available.
- [ ] Open a Zombies drop. Expect two regular collection cards followed by three consumable cards; the reveal finishes normally.
- [ ] Owned regular cards convert to their displayed AC values. Consumables still arrive as quantities, including repeats.
- [ ] Reopen the vendor and then restart. The final balance and inventory persist, without a second payout just for reopening the menu.

Record mode/drop type, AC before and after, duplicate card names/values, and any reveal error. Normal UI reopening checks persistence; receipt-level replay and failed-save rollback are covered by automated tests, so you do not need to simulate network retries manually.

## 3. Zombies orders and contracts

Claiming already passed your earlier check. Prioritize match progression now. Allow one ordinary match, then use the optional specialist checks when convenient.

- [ ] Lobby offers six daily orders, three weeklies and eight contracts. The complete pool has 20 dailies and seven weeklies; they are rotated, not all shown at once.
- [ ] Accept an available general kill order and one kill contract. Record its AC price, starting progress and remaining time. Purchase consumes the displayed price once.
- [ ] Kill a small count of zombies, then return to the lobby. Progress increases and remains after reopening Orders. Two fast kills should not collapse into one merely because they share a timestamp.
- [ ] The contract timer decreases during the match and pauses in the lobby. A third contract can be active; a fourth should be refused.
- [ ] Finish and claim an objective. Dailies give 250 AC; weeklies and contracts give one Zombies drop. Relaunch and verify the reward and completion persist.
- [ ] Compare MP and Zombies active lists; one mode's actions should not advance the other's orders.

Optional specialist coverage:

- [ ] With an airborne throwing-knife contract active, ordinary gun kills do not advance it. Satisfying the actual combined condition does.
- [ ] Test a Darkest Shore/Ripsaw objective on the named map. A wrong-map or wrong-weapon kill must not count.
- [ ] If a second player is available, repeat a short objective as the remote co-op player with the same build on both sides. Local/host success alone does not establish remote progress.

See [the catalog](economy/zombies-catalog.md) for each target, price and time limit. `hqcontracts` and `hqeconomy` are useful diagnostics. If progress is wrong, record the objective, map, host/client role, weapon, actions performed, displayed progress and session time. Keep that session's logs and Demonware debug captures.

Revive, purchase and wave objectives are tracked separately in CR #8; do not expect those new options in the current catalog.

## 4. Gun Game bot outfits — CR #4

The new server-side repair is scoped to Gun Game bots. It does not change human outfits, Domination or Sandbox's special appearance rules.

- [ ] On a server with the GSC installed, start Gun Game with bots (Cliffside reproduces the reported issue).
- [ ] Bots have complete bodies, including legs, on initial spawn.
- [ ] Watch several weapon promotions and bot respawns; bodies remain complete.
- [ ] Rotate to Domination and confirm normal bot appearance and gameplay. Rotate back to Gun Game and check again.
- [ ] A human player keeps the expected outfit. If testing Sandbox, verify its special models remain unchanged.

For script diagnostics, set `s2x_gungame_debug 1`; the console prints `S2x Gun Game: repaired bot costume for division ...` when the repair runs. Return it to `0` after testing. A debug line confirms execution, not correct rendering: the visible body check still matters.

## 5. Optional server scripting — CR #9

These are server-owner checks, separate from the player/economy tests. See [the API guide](../tools/server-scripts/README.md) for installation, configuration and the new `getip(player)`, `fileread`, `filewrite` and roster functions. HUD messages remain the supported server voice. The existing Discord MCP is unaffected.

- [ ] With the new EXE and `s2x_server_events.gsc` on the server, optionally install `tools/server-scripts/examples/scripting_api.gsc` as `s2x/scripts/mp/scripting_api.gsc`.
- [ ] Join as a human, spawn, and type `!visits` in global chat. Chat still reaches normal recipients, and the server shows the saved count as a HUD reply.
- [ ] Repeat with team chat. Rotate/restart the map, spawn again, and check the saved map-visit count increased once. Remove the optional example afterwards if unwanted.
- [ ] If testing address lookup, call `getip(player)` in your own server script and privately verify that it returns the remote player's address without a port. No geolocation service is contacted.
- [ ] For the Discord card, copy the updated `tools/server-status.ps1` to the server tools folder, configure a unique `s2x_roster_file`, then run the guide's **DryRun** command first. Expect the correct status and a presence field; bots should not appear as arrivals.
- [ ] If the preview looks right, run using the existing card/channel/token settings. First success establishes the baseline; have a human join/leave across subsequent polls. Expect one event per observed change, and unchanged card content elsewhere. Names cannot ping users/roles.

No Discord message or scheduled task was created during development. Arrivals use periodic snapshots, so very short visits between polls can be missed. Human chat delivery, remote IP output and Discord publication remain your live checks.

## Record results

| Check | Pass / fail / skipped | Notes |
| --- | --- | --- |
| Rank commands and UNLOCKS | | |
| Rank persistence and later XP | | |
| MP common/rare duplicates | | |
| Zombies duplicates and consumables | | |
| Zombies match progression | | |
| Timers, claims and persistence | | |
| Optional specialist/co-op checks | | |
| Gun Game bodies / promotions / respawns | | |
| Map rotation and Domination | | |
| Optional scripting, saved visits and chat | | |
| Optional Discord arrivals | | |

Automated verification so far: expanded economy C++/Lua tests and reveal checks passed in the preceding work; the new production rank-command harness passes all 2,000 extracted MP/Zombies rank thresholds, inventory-XP baseline/future rewards and failure guards. The production chooser reader passes Lua checks. Release x64 builds successfully. Gun Game GSC compiled and ran on a local London Docks server with four bots: two match starts and 40 repair calls through division changes, with no script runtime errors. That test server is stopped. Rendered appearance still needs your check.

Server scripting: production storage and chat-routing tests and mocked status-card tests pass. A local dedicated Domination match verified persistence across three map initializations, empty/missing files, twelve bot-IP and twelve GSC chat-bridge checks, and a local-only roster/status preview. That test server is also stopped. Its presence data contained no humans; no remote-IP or real Discord result is claimed.
