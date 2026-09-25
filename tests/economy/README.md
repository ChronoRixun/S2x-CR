# Zombies economy checks

Build with Visual Studio 2022 C++ tools and initialized submodules. Build `build\s2x.sln` (Release, x64) first: the test links its `common.lib` and `libtomcrypt.lib` from `build\bin\x64\Release`.

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
2. Open Quartermaster through Supplies. Check that it opens and displays owned drops/prices. If a drop is available, open it and verify consumables arrive. The native two-stage Zombies reveal receives two regular items (collection items or weapon variants) followed by three consumables. The rarity tier weights are local policy, not retail odds.
3. Open Mail and claim any available welcome voucher/payroll. A voucher already redeemed in MP must not pay a second time.
4. Play and meet the accepted order condition, return, claim, then relaunch and verify persistence. Test remote-client progress separately in co-op; both sides need this build for the Zombies relay tag.

Logs should include `[HQ economy] native economy enabled for Zombies`, `[ZM economy] catalog ready: 20 daily / 7 weekly pool, offering 6/3, native kinds 8/9`, and bounded `[ZM AE]` dispatch diagnostics. A `waiting for native challenge identities` message indicates catalog readiness failed. `hqeconomy` prints persisted wallet/items/orders without granting anything. Preserve logs when a menu fails.

The Zombies pool has **20 daily orders, seven weekly orders and eight contracts**. The rotation offers six dailies and three weeklies; all eight contracts fit the existing nine-offer contract cap. Three contracts may be active at once, independently of MP. Previously accepted orders keep their progress. Daily orders award 250 AC; weeklies and contracts each award one Zombies drop. Existing completion bonuses remain separate from MP.

See [the full catalog](zombies-catalog.md) for objectives, targets, prices and match-time limits. Contract identities, targets, timers and cost tokens come from the extracted game tables; prices and rewards are local policy. Map-specific offers name The Darkest Shore in their descriptions. Purchase, accept, progress and claim through Quartermaster > Contracts. The `hqcontracts` command prints SKU/token data and active native kind-4/11 records.

Owner testing confirmed Orders, Quartermaster Deals and Mail, and claiming with the expanded build. Match progress and specialist objectives still need live testing. Offline checks cover all 35 objectives with matching, nonmatching, missing-field and partial-flag events; purchase/activation/claim replay; timer expiry; mode isolation; pool coverage; and preservation of carried progress. The Lua checks validate every identity, predicate, contract target, timer and token against extracted tables, then exercise visibility, inventory grouping and shared price/SKU helpers. The ZM-specific menu body was unavailable for offline execution.

For a live smoke test, verify the lobby shows six daily, three weekly and eight contract offers, and accepting a fourth contract is refused. Try one general kill contract, one compound predicate (airborne throwing knives), and one map-specific Ripsaw objective. Confirm ordinary kills do not advance the specialist objective, timers pause in the lobby, and a claimed reward survives restart. Repeat with a remote co-op client.

Revive, purchase and wave objectives are tracked in [CR issue #8](https://github.com/ChronoRixun/S2x-CR/issues/8). Their unfiltered table rows need action and counter interpretation. Two other retail rows are deliberately excluded: 1078's wave-10 shovel text conflicts with its electrical-kill flag, and 1104's bit 31 lacks a producer in the inspected reporting scripts.


## Reveal and active Orders regressions

The first live supply-drop test exposed an incomplete reward shape: three consumables alone reach an invalid card in the initial reveal stage. The production response now retains five grant records (two regular items and three consumables), even when IDs repeat. Repeated non-consumable cards now convert to Armory Credits; Zombies consumable quantities aggregate normally. Receipt retries do not consume/grant again. Either missing pool rejects the transaction without consuming the drop.

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

## Duplicate supply-drop credits (CR issue #6)

Duplicate collection items now award Armory Credits for common MP drops (1), rare MP drops (2), and the two regular cards in Zombies drops (6). A repeat roll within the same opening also converts after the first copy is granted. Owned, unexpired items retain their existing quantity; expired or zero-quantity items receive a permanent first copy. Zombies consumables continue stacking: the shipped StatsTable marks them non-pawnable.

Prices are **not hard-coded rarity guesses or collection purchase prices**. On the game thread, `load_loot_catalog` calls the native `Inventory_GetItemPawnValue` helper at image offset `0x274CF0` after checking `mp/pawnValues.csv` is loaded. The helper checks StatsTable column 30 (pawnability), selects category/subtype, and reads column `rarity + 2` in pawnValues. The packed result contains currency ID in the low 32 bits and amount in the high 32 bits. This is the helper called by the Lua binding at `0x11E9E0`; rarity comes from StatsTable column 29. The transport receives a copied map of resolved values, never game asset pointers. Missing values prevent an opening without consuming the drop.

All original item IDs remain in `GrantedItems` so the native three-card MP and two-plus-three Zombies reveals retain their shape. `GrantedCurrencies` aggregates conversion credits using `currency_id`, `balance_before`, and `balance_delta`, the fields read by native handler `0x27C480` from `0x2AF7A0`. The receipt stores the original items and currency report together with the debit/grants. Replays retain that original report; the existing `hq_native::sync_wallet` poll projects today's persisted balance to the native wallet, and `DetailedInventory` always reports current quantities. Old receipts with no credits are replayed unchanged, without retroactive compensation.

C++ coverage exercises all three drop IDs, several synthetic per-item valuations (including zero), already-owned and same-opening duplicates, expired/zero ownership, consumable stacking, exact response fields, receipt replay after spending credits and clearing the catalog, conflicting transactions, missing metadata, currency overflow, and an actual exclusive lock on the temporary save file. The save-failure test confirms rollback and successful retry. Synthetic fixture values deliberately do not claim to be retail prices.

Live check with the combined build:

1. Note your AC balance (`hqwallet` also prints it), then open an MP drop containing a duplicate. Confirm the balance increases by the displayed duplicate value(s).
2. Repeat with a Zombies drop. The two regular cards can convert; the three consumables must still arrive and the reveal must finish normally.
3. Restart and check that the credited balance and inventory persist. A new item should be granted once; an owned collection item should retain its existing quantity.
4. Play a match with the expanded Zombies orders/contracts and check progress, timer pause in the lobby, and claiming separately.

The combined build and offline tests are verified by the task; native conversion values, duplicate animations and the live wallet display still need the owner's in-game confirmation.
