# Zombies economy checks

Build with Visual Studio 2022 C++ tools and initialized submodules:

```powershell
msbuild tests/economy/zombies.vcxproj /p:Configuration=Release /p:Platform=x64
./build/tests/zombies/bin/zombies.exe
python tests/economy/overlay.py
```

The Python test needs `lupa` with Lua 5.1. `--lupa-path PATH` adds a local installation. Optional `--research-root PATH` checks the catalog against locally extracted `tables/dwgamechallenges.csv` and runs the extracted visibility function from `luafiles/dec/ui_utility_mp_achievementengineutils.dec.lua`. Game assets are not distributed here.

The C++ test compiles production economy, marketplace, event parsing and response code with minimal game/IO shims. It uses a unique temporary profile. Coverage includes native kinds 8/9, quotas, predicates, deduplication, claims and bonuses, MP isolation, legacy achievement merging, rollover, supply drops, purchases, mail, persistence and co-op envelopes. The Lua test checks the embedded policy in both modes.

These checks do not run the game's native hooks, vendor menus or a live co-op session.

## Live test

Use the freshly built `build/bin/x64/Release/s2x.exe` with the normal game directory as its working directory (or copy it into that directory). Launch with `-zombies -noupdate -demonware_debug`. Keep the current economy profile: currency/inventory and mail receipts are intentionally shared with MP; offer kinds and bonuses are separate.

1. In the Zombies lobby, wait about five seconds. Open Orders through the usual route. Expect six daily and three weekly offers. Accept an order and check it appears in active Orders.
2. Open Quartermaster through Supplies. Check that it opens and displays owned drops/prices. If a drop is available, open it and verify consumables arrive. The native two-stage Zombies reveal receives two regular collection items followed by three consumables. Rolls are uniform local policy, not retail odds.
3. Open Mail and claim any available welcome voucher/payroll. A voucher already redeemed in MP must not pay a second time.
4. Play and meet the accepted order condition, return, claim, then relaunch and verify persistence. Test remote-client progress separately in co-op; both sides need this build for the Zombies relay tag.

Logs should include `[HQ economy] native economy enabled for Zombies`, `[ZM economy] catalog ready: 6 daily / 3 weekly orders, native kinds 8/9`, and bounded `[ZM AE]` dispatch diagnostics. A `waiting for native challenge identities` message indicates catalog readiness failed. `hqeconomy` prints persisted wallet/items/orders without granting anything. Preserve logs when a menu fails.

Three native Zombies contracts are offered: 250 kills in 50 minutes (100 AC), 400 kills in 60 minutes (250 AC), and 750 kills in 120 minutes (450 AC). Each awards one Zombies drop. Timers run only in a match of the same mode. Targets, time limits and cost tokens come from the game tables; purchase prices and payouts are local policy. Test purchase, acceptance, progress and claim through Quartermaster > Contracts. The `hqcontracts` command prints SKU/token data and active native kind-4/11 records.

Owner testing of the initial build confirmed Orders, Quartermaster Deals and Mail; the newly added Contracts flow still needs live testing. The expanded tests cover native kind-11 offers, purchase/activation/claim replay, paid-token enforcement, expiry, abandonment, persistence and isolation from three active MP contracts. Lua checks also validate contract identities, timers and tokens against extracted tables, plus the shared contract price/SKU lookup helpers. The ZM-specific menu body was unavailable for offline execution.

## Reveal and active Orders regressions

The first live supply-drop test exposed an incomplete reward shape: three consumables alone reach an invalid card in the initial reveal stage. The production response now retains five grant records (two regular items and three consumables), even when IDs repeat. Inventory quantities aggregate the grants; receipt retries do not consume/grant again. Either missing pool rejects the transaction without consuming the drop.

The C++ test emits a `zombies-drop-response.json` path. Pass it to this optional extracted-script test:

```powershell
python tests/economy/reveal.py --research-root PATH --response PATH_TO_RESPONSE_JSON
```

Like `overlay.py`, it accepts `--lupa-path PATH`. It runs the actual Zombies reveal coroutine and card-flip validation using mocked animation/engine calls: the old three-consumable response raises `HUB_SUPPLYDROP_TX_ERROR`, and the production five-item response completes both stages. Native filtering and live animation remain outside this test.

The active Orders tab groups by periodic types `AEC_DAILY` / `AEC_WEEKLY`, as the retail Zombies rows do. Native achievement kinds remain 8/9 and provide Zombies styling. `overlay.py` now executes that tab's extracted grouping function in addition to visibility checks. Accepted progress remains in the existing profile.

## Native same-second kill regression

Live testing found two genuine LMG kills with the same native timestamp and fields in one task-11 report. The parser now preserves their occurrence within that user batch, and economy receipts and co-op forwarding retain it. Ordinal zero keeps legacy receipts. Retries may regenerate their native transaction ID, so the identity deliberately excludes that ID. Tests cover changed-ID retries, persistence, malformed neighboring events, normalized relay fields and fragmentation.

For the owner's three-report 27-kill capture, optionally run `zombies.exe --captured-zm-match REPORT155 REPORT163 REPORT171` with absolute paths. It reads captures but writes only its unique temporary profile, and verifies two full submissions produce 27 contract kills, 12 native headshot flags, 13 LMG kills, 11 pistol kills and one upgraded flag. Other order predicates remain unchanged. Captures are not distributed.

The owner's AAR shows 26 headshots while native reports carry only 12 headshot flags. That discrepancy remains under investigation. Identical events split across separate batches or retry subsets also remain ambiguous because the native wire supplies only coarse timestamps, not a stable occurrence ID. The fix covers the proven same-batch collision; it does not claim to resolve that broader ambiguity.
