# Rank command regression (CR #7)

`setrank` already selected the correct minimum XP from column 2 of the zero-based rank table. Multiplayer displays **experience + max(0, inventoryTotalXP - inventoryXPAtLastReset)**, however. Leaving the inventory delta intact adds previous economy rewards on top of the requested level. The UNLOCKS chooser previously read only `experience`, so it could also disagree with the Soldier screen before applying a change.

The native `Engine.GetPlayerDataMPXP` binding at image offset `0x110210` calls `0xD71B0`, which computes that sum. Script-string initialization at `0x5D1AC8` / `0x5D2938` identifies the two inventory fields. Native prestige reset `0x18ED20` writes both to currency 1's current balance. The rank commands now apply the same baseline using the persisted economy wallet (the source of the native wallet projection), retaining the wallet and allowing future XP awards to count. Zombies continues writing `totalXP` without changing MP inventory fields. `setprestige` and the chooser's Apply action share this path.

Run with VS 2022 C++ tools and Lua 5.1 through `lupa`:

```powershell
python tests/stats/rank.py --lupa-path build/research/slice9-python --tables build/research/tables
```

Both optional paths can be omitted when `lupa` is installed normally and extracted game tables are unavailable. `--msbuild PATH` overrides the default VS 2022 Build Tools executable. Output stays under `build/tests/stats`.

The harness compiles the actual production command/application and level-conversion functions against synthetic stat writes and wallet reads. With the optional tables, it checks all 1,000 MP and 1,000 Zombies rank thresholds. It also checks both command entry points, caps, stale native wallet projection, subsequent reward XP, unchanged balances, unavailable wallet, invalid stat paths, overflow and invalid input. The Lua portion executes the actual chooser reader with native engine stubs. These checks do not execute native DDL writes or the Soldier screen.

Live checks: back up the game profile, set MP levels 1, 2 and 3 with a nonzero inventory XP balance, compare the console and reopened Soldier screen, and verify the chooser initializes to the displayed rank. Check `setprestige`, a chooser Apply and Zombies rank setting. Earn XP afterwards, load a map to allow the stats upload, then restart and check persistence. Restore the profile after rank testing if retaining the previous progression.
