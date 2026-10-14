# Upstream economy comparison

Compared on 2026-10-13: CR integration `6bfd9d6` in `D:\S2x`, and upstream master `0048253` in this rebuild candidate. This is a source comparison, not a runtime certification. The comparison itself was read-only: no existing profile was written or migrated, and no candidate was deployed over an existing installation. Separate isolated hosting smoke checks are recorded in [candidate validation](UPSTREAM-REBUILD-VALIDATION.md).

Recommendation for this candidate: retain the upstream economy as its sole backend while preserving the separately scoped CR hosting work. Upstream has substantial marketplace, loot, Orders, contracts and Zombies progression functionality. Do not merge both economy implementations: their stores, hooks and reward policies overlap. Before any later replacement of the installed CR client, review the losses below and separately decide which CR acquisition policies to restore and whether to migrate existing profiles.

## Coverage and behavior

| Area | CR integration | Upstream candidate | Effect |
| --- | --- | --- | --- |
| Wallet and inventory | Shared HQ JSON economy with atomic mutation | Marketplace store with atomic achievement/inventory/currency settlement, request fingerprints and permanent transaction receipts | Upstream foundation retained; persisted format incompatible |
| MP daily Orders | 20 generic plus 41 weapon-reward Orders | 36 daily offers | Smaller/different catalog; 39 of CR's 41 weapon Order names absent |
| MP weekly Orders | 10 | 18 | Expanded upstream catalog, but CR weekly wins and scorestreak-call Orders absent |
| MP contracts | 56: 48 weapon and 8 generic | 18: 14 generic and 4 Moonraven | All 48 CR weapon contracts absent; no CR rotating 2,500 AC weapon-contract route |
| MP special Orders | Not included in CR rotation | 39 | Adds weapon and bribe acquisition paths |
| Zombies daily/weekly | 20 daily and 7 weekly, kill-event subset | 24 daily and 9 weekly | Adds purchases, revives, wave streaks and objectives; changes targets/rewards |
| Zombies contracts | 8 | 12 | Adds location/time-trial waves, Shellshock and all-doors contracts |
| Zombies specials | No corresponding CR rotation | 4 Stormraven and 4 always-available Moonraven contracts | Adds character acquisition paths; Moonraven counts within kind-4 total above |
| Above & Beyond | MP and Zombies daily/weekly counters | MP daily/weekly counters only | CR Zombies meta counters/bonuses absent |
| Payroll | 200 AC including master prestige | 200 normal / 300 master prestige, shared cooldown | Improved master-prestige payout |
| Social ranks | 20 hardcoded reward bindings | 20 catalog-backed rewards with native score thresholds | Preserved; upstream also has player commendation transport |
| MP rank / prestige | Rare rank-up drops, master milestone cosmetics, prestige helmets/cards, eight division reward sets | No corresponding MP reward processor found | CR reward grants absent; this does not mean native XP/prestige itself is removed |
| Zombies rank / progression | Rare drops on each rank-up; vendor prestige-card backfill | Rank-table normal/rare drop milestones, character/unlock progression | Different drop policy; CR blanket rare-drop and vendor prestige-card paths absent |
| Match-end drops | No separate host award system in CR economy | Host selects distinct human recipients; default 1 award, maximum 18 | New acquisition path; progression must be enabled |
| Loot opening | Common/rare/Zombies pools, duplicate AC conversion, consumable stock stacking | Slot/type/operation policies, bribes, duplicate protection, restrictions and Zombies policy | Broader policy; different local odds |
| Collection redemption / pawn / consumption | Implemented | Implemented with catalog and transaction validation | Preserved |
| Marketplace | Custom SKUs and collection-price policy | Captured 1,024 SKUs and 330 products, plus CWL and local contract policy | Broader captured coverage; CR custom special-bundle definitions not ported |
| CWL | 16 packs at 1,000 AC | 16 packs at 500 AC | Preserved, cheaper |
| Beta / Ambassador | Custom purchasable pack | Five once-per-profile vouchers | Changes acquisition path |
| Console tools | HQ-specific tracing and mutation commands | General item/currency/drop listings, dumps and grant commands | CR diagnostic command names absent |

Upstream offer counts, read directly from `achievement_offers.json`: kinds 1/2/3/4/8/9/10/11 have **36/18/39/18/24/9/4/12** entries, **160 total**. Both systems offer six daily and three weekly cards; upstream permits three activations per daily/weekly kind and one special activation. Upstream uses daily reset at 17:00 UTC and its weekly phase; CR uses UTC midnight and `day / 7` week buckets. Catalog counts describe definitions, not simultaneous visible cards.

## Specific CR losses to review

- **Weapon contracts:** all 48 CR `contract_ch_<weapon>` definitions, including LAD and the later weapons, are absent from upstream's scheduled MP contract catalog. The CR catalog charges 2,500 AC for the weapon rewards and rotates a nine-card sliding window. Upstream offers generic contracts and special Orders instead; it is not an equivalent guarantee of every CR weapon unlock.
- **Weapon daily Orders:** only `daily_ch_karloot2_1` and `daily_ch_winchesterloot3_2` of the 41 CR weapon Orders survive by name. Even these have upstream descriptors/targets, so name overlap is not exact behavioral parity.
- **Generic MP Orders:** upstream lacks CR `daily_ch_assists`, `daily_ch_destroy_scorestreaks`, `daily_ch_dom_caps`, `daily_ch_dom_wins`, `daily_ch_ffa_killer`, `daily_ch_killstreak`, `daily_ch_tdm_wins`, `weekly_ch_scorestreak_calls`, and `weekly_ch_wins`. The two win dailies are CR's 250 Social Score payout route.
- **MP inventory progression rewards:** CR event 14 awards rare drops and master-rank milestones; event 11 awards prestige helmets/cards and division progression items. Upstream's rank processor is explicitly Zombies-only. CR's MP reward grant policies therefore need a separate port if desired. CR's Zombies prestige calling-card backfill through vendor selector 5 is also absent from that upstream path.
- **Zombies Above & Beyond:** CR defines `zm_above_beyond_daily` and `zm_above_beyond_weekly`. Upstream advances its meta reward only for kinds 1/2, not 8/9.
- **Custom special bundles:** CR defines C.O.D.E., Beta/Ambassador, Zombies cosmetics, Divisions preorder, Viper, Endowment, Partner, Twitch and Army Men bundles. These exact local tags/prices/reward sets are absent from upstream's local catalog additions. Captured upstream products and individual cosmetics can still overlap; absence of the CR bundle does not establish that every constituent item is unobtainable. Beta/Ambassador receives an explicit upstream voucher path; CWL remains supported.
- **Diagnostic tools:** CR `hqeconomy [reload]`, `hqgrant`, `aeevent`, `hqnative`, `hqwallet`, `hqvendor`, `hqmail`, `hqopendrop`, `hqtask99`, `hqskutest`, `hqpayrollstate`, `hqcontracts` and `hqownership` are not the upstream command interface. Upstream exposes `giveItem`, `giveCurrency`, `listItems`, `dumpItems`, `listCurrencies`, `dumpCurrencies`, `listDrops` and `dumpDrops`.

## Persistence and migration risks

CR's authoritative HQ store is fixed at `players2/user/hq_economy.json`; legacy native achievement completion lives separately in `players2/user/achievements.json`. Upstream stores currency, inventory, achievements and transaction history together in the selected profile's `marketplace.json`. Its default profile is `players2/user`; named profiles use `players2/profiles/<name>/user`.

Upstream only imports legacy `achievements.json`, and only when `marketplace.json` is absent. Its seven-field achievement-record importer can accept CR's separate legacy achievement file if it satisfies strict validation. It does **not** import `hq_economy.json`. Without a separately approved converter, CR wallet balances, inventory/drop stock, HQ Order/contract progress, payroll cooldowns, social/milestone receipts and claim history do not transfer. Keeping the original file preserves data for later recovery, but does not make upstream read it.

Both formats call themselves schema version 1, but renaming the HQ file to `marketplace.json` is invalid. Upstream expects `achievementState`, `processedTransactions`, different currency fields, richer ownership/expiry inventory records and request fingerprints. Concrete converter concerns:

- CR inventory is keyed by `(GUID, collision)`; upstream keys by item ID. Multiple collision rows require an explicit policy, not silent aggregation.
- CR item metadata, receipt identifiers and progress fields have different bounds. Upstream limits item data to 64 bytes, ClientTx to 64 characters, inventory and permanent transaction records to 65,536 each; its achievement progress is 16-bit.
- CR transaction values are not uniformly the upstream `(ClientTx, fingerprint, exact response, sequence)` format. Reward-history conversion must preserve deduplication, especially prestige/social grants and consumed/opened drops, without pretending incompatible receipts are native upstream responses.
- CR has existing payroll-currency and contract-token migrations. A converter must interpret their markers and legacy records before translating balances/tokens.
- Catalog identities, targets, timers, activation limits and reset periods differ. Active/claimable contracts should not be copied blindly. A future migration needs an explicit refund/restart/settlement policy.
- Upstream skips legacy import when marketplace storage already exists; copying achievements afterward will not merge them automatically.

This candidate does not include a converter, start the game, copy economy stores, modify existing profiles, or deploy over CR. Any future migration is a distinct operation requiring a backed-up input and a reviewable result.

## Evidence

Upstream links are repository-relative from this document. CR evidence names exact original checkout paths and line numbers so the source distinction remains clear.

- [Upstream offers and rotations](../src/client/resources/dw/achievement_offers.json), [rotation/reset implementation](../src/client/game/demonware/achievement/orders.cpp) lines 22-25 and 355-384.
- [Upstream meta Orders and claim settlement](../src/client/game/demonware/achievement/store.cpp) lines 422-475 and 876; [payroll](../src/client/game/demonware/reward/hq_rewards.cpp) lines 69-73.
- [Upstream server predicates](../src/client/game/demonware/achievement/order_progress.cpp) lines 92-130; [Zombies progression](../src/client/component/demonware/zombies_progression.cpp) lines 30-44, 145-194 and 491-501.
- [Upstream match-drop pool](../src/client/game/demonware/reward/match_drop.cpp) lines 21-32; [host match-drop configuration](../src/client/component/demonware/match_drops.cpp) lines 127-130 and 234-235.
- [Upstream loot policy](../src/client/game/demonware/loot/policy.cpp) lines 44-46 and 68-86; [captured marketplace manifest](../src/client/resources/dw/marketplace_catalog_manifest.json); [local catalog additions](../src/client/game/demonware/marketplace/catalog.cpp) lines 480-541.
- [Upstream CWL packs](../src/client/game/demonware/marketplace/cwl.hpp) line 8; [promotional vouchers](../src/client/game/demonware/marketplace/promotional_vouchers.cpp) lines 24-38; [commands](../src/client/component/demonware/economy.cpp) lines 601-638.
- [Upstream store constraints](../src/client/game/demonware/marketplace/store.hpp) lines 11-26; [legacy import and store serialization](../src/client/game/demonware/marketplace/store_persistence.cpp) lines 186-264, 436-499 and 589-599; [profile directories](../src/client/game/player_profile.cpp) lines 45-48.
- CR `D:\S2x\src\client\game\demonware\hq_contract_catalog.hpp` line 21 onward: 56 MP contracts and 41 weapon Orders; `D:\S2x\src\client\component\hq_economy.cpp` lines 291-322: 20 generic daily / 10 weekly Orders.
- CR `D:\S2x\src\client\game\demonware\hq_zombies_catalog.hpp` line 21 onward: 20 daily / 7 weekly; `D:\S2x\src\client\game\demonware\hq_zombies_contract_catalog.hpp` line 25 onward: eight contracts.
- CR `D:\S2x\src\client\game\demonware\achievement_engine.cpp` lines 29-57, 564-572, 670-776: master, social, division and rank/prestige rewards; lines 44-47: Zombies meta-counter names; lines 289-300: offer rotation.
- CR `D:\S2x\src\client\game\demonware\achievement_engine.cpp` line 1173: local rarity weights `45/30/15/8/2` and guaranteed-first rare weights `0/60/25/12/3`. Upstream uses `600/260/80/45/15`, improved `350/380/160/80/30`, and guaranteed rare `0/650/220/100/30`. Neither claims recovered retail odds.
- CR `D:\S2x\src\client\game\demonware\hq_marketplace.hpp` lines 147-177: CWL and custom special bundles; `D:\S2x\src\client\component\hq_native.cpp` lines 875-887 and `D:\S2x\src\client\component\hq_economy.cpp` lines 364-366: diagnostics.
- CR `D:\S2x\src\client\game\demonware\hq_economy.cpp` line 11: HQ store path; lines 180-201 and 338-387: legacy migration markers; `D:\S2x\src\client\game\demonware\achievement_store.cpp` line 16: legacy achievement path.

Validation: read and counted catalog definitions; inspected the producer/claim/store implementations and compared offer names. Runtime acquisition, game asset readiness and existing profile contents were not tested. Documentation-only work does not require Debug/Release builds.
