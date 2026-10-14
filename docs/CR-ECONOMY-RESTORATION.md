# CR economy restoration candidate

The upstream-based candidate now restores CR acquisition/reward policies on one authoritative upstream backend. Original CR source: integration6bfd9d6. Hosting rebuild base:2541f12. No installed client/server or existing profile has been replaced or migrated.

## Restored

- All56 CR MP contracts, including48 weapon contracts at2500AC and a nine-card sliding MP contract window.
- CR weapon/generic MP daily and weekly definitions, including win/social-score rewards. Same-name CR targets/rewards replace the upstream descriptor; upstream-added names remain.
- CR20 Zombies daily,7 weekly and8 contract definitions, with upstream-added names retained.
- MP and Zombies Above & Beyond: six daily and three weekly completions, with canonical bonus descriptors, atomic claim settlement and replay/rollback protection.
- Nine custom CR shop bundles with their publisher-variable tile references/prices/reward sets. All16 CWL packs retain CR1000AC prices.
- Rare MP/Zombies drops at1000AC alongside the preserved captured CP route and upstream seasonal/bribe types.
- MP/Zombies rare rank-up drops; MP prestige helmets/cards, twelve master milestones, all eight division prestige reward sets, and Zombies prestige calling-card backfill.
- Native Zombies milestone completion/quest/character behavior remains; its overlapping table-drop callback is removed so CR alone grants rank-up drops.
- CR normal and guaranteed-first rarity weights, while retaining upstream slot filtering, duplicate handling, guaranteed floors and rarity-scale controls.
- Operational hqgrant currency/item syntax and hqwallet. hqeconomy reports authoritative-store status; hot-reloading edited stores requires restarting instead of resetting an active transaction cache. The old engine-specific hqnative/aeevent diagnostic machinery is not transplanted; upstream item/currency/drop grant/list/dump commands remain.

Upstream's17:00UTC reset schedule remains. The merged MP contract cycle contains69names, so appearance frequency differs from the old56-name CR cycle; daily/weekly ordering follows the merged pool. Restoring acquisition rules does not claim to recover retail loot probabilities or certify every human-client UI/native XP effect.

## Save conversion

[Offline migration](CR-ECONOMY-MIGRATION.md) accepts explicit copied input, prints a dry-run report by default, and writes only a NEW output when requested. It preserves representable balances, permanent stock/metadata, settled receipt history, Social Score and payroll state. It never discovers or writes installed profiles.

Unsupported positive rental/expiry rows, ambiguous collisions, oversized metadata, unresolved valuable claims, and settled/claimable bonus counters stop conversion. Known unpaid bonus counters can be explicitly archived/reset; active objectives require explicit refund/restart policy. These refusals protect data and must be resolved on a copy before rollout; this is not a universal automatic upgrade tool.

Migrated progression is baselined from matching native stats in verified frontend readiness before gameplay. Events cannot finalize migration from advanced XP. Loading waits; gameplay before baseline latches a restart-in-frontend requirement. The first legitimate post-baseline rank-up pays normally; old migrated prestige receipts cannot grant twice.

## Verification

- Native Release/Debug builds are recorded under build/logs/economy-restoration-*.log.
- tests/cr-catalog/claims.vcxproj compiles the actual catalog, products/SKUs, purchase, byte buffer, scheduling, predicates and atomic stores. It checks embedded manifest hashes,600-day offer coverage, all nine bundles, AC/CP rare debit isolation, token activation/timing, MP/ZM bonuses, rollback and replay.
- tests/progression/policy.cpp uses production readiness/reward policy helpers with an atomic store stub: all reward sets, rank/replay/re-earn behavior, failed grants, rental/collision guards, frontend/loading/gameplay transitions and first actual rank-up.
- tests/cr-loot/policy.py compiles extracted production selection/weights:100000 deterministic draws, guarantee floors, empty eligible tiers and invalid-scale handling. It failed before restoration and passes after it.
- tests/economy-migration/test_migration.py covers17 synthetic conversion/refusal/idempotence cases.
- tests/economy-loader/loader.vcxproj compiles production marketplace persistence/store and achievement store code. It accepts a synthetic converted file, validates all four canonical bonus descriptors/targets, rejects conflicting receipt fingerprints and replays settled receipts without invoking a grant. Input hash is unchanged. Engine-dependent seed access is stubbed; production fixture record decoding is used.
- Independent review identified and verified fixes for expired-stock conversion, imported bonus-counter descriptors and migration baselining timing (including delayed stats/table readiness).

No real economy saves were inspected or converted. Isolated native MP/Zombies client checks now cover purchases, supply-drop opening, contract activation/redemption, Headquarters, CWL UI purchases and synthetic rank-event/replay boundaries. See tests/client-economy/README.md for precise evidence and fixture limitations. The prior isolated hosting smoke evidence still applies to the hosting port. A native Zombies melee kill also earned XP, promoted rank 3 and delivered one additional rare drop during gameplay; that check used a near-threshold disposable stat fixture and local QA invulnerability. Full unmodified human MP/ZM matches and representative real-save migration remain rollout checks.
