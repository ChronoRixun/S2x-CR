# Offline CR economy migration

`tools/migrate-cr-economy.py` converts an explicitly supplied **copy** of the CR `hq_economy.json` into a new upstream economy JSON file. It never discovers profiles, launches the game, installs the result, or changes its input. Existing output paths are refused, including during dry-run; actual writes use exclusive creation and read-back verification.

Both files claim schema version 1 but have incompatible layouts. Do not rename or copy a CR economy over an upstream economy. An existing upstream economy is rejected as input; merging an existing target wallet is unsupported.

Run from this checkout with Python 3:

```powershell
python tools/migrate-cr-economy.py --input C:\migration\hq-copy.json --output C:\migration\converted-new.json --user-id 12345
```

This validates and prints a report without writing. Add `--write` only after reviewing the report. The output directory must already exist. Supply the actual target Steam ID explicitly; the tool cannot verify account ownership. Optionally supply `--legacy-achievements C:\migration\achievements-copy.json` to preserve standalone legacy achievement state. Duplicate names between those stores fail for manual reconciliation, rather than picking a winner.

## Value and history

Currency IDs retain their meaning and balances: 6 Armory Credits, 7 Social Score, 2 COD Points. Other byte-sized currency IDs remain unchanged. A store without `migration:payroll-currency6-v1` and with nonzero 2/7 balances is ambiguous: conversion refuses it by default. `--currency-policy preserve` explicitly preserves those slots and does **not** repair old misassigned payroll balances. Reconcile a copy separately if that is required. The converter never reruns CR migration markers or attempts to reconstruct already removed stock.

Positive permanent inventory quantities preserve GUID/item ID, collision field, modification date, and all metadata bytes (at most 64). Positive rows require CR expires = 0 and become the target legacy-permanent 0/0 expiry form. The target requires one record per item ID, so multiple positive collision rows for one GUID fail. A zero quantity cannot enter the native inventory; it is retained as an archival receipt. Any positive row with nonzero CR expires is refused, including already expired dates, finite future dates and UINT32_MAX. CR omitted expiryDuration, while the native predicate rejects nonzero-date/zero-duration stock immediately. The converter cannot invent a verified duration or interpret the MAX date as permanent. Preserve such stock in the untouched input for explicit reconciliation; no unusable target row is silently accepted. Player ID and account type are set to the supplied Steam identity; CR never persisted those fields.

Every original CR transaction and achievement row is preserved in a permanent bounded target transaction response. Receipt IDs longer than the target's 64 bytes receive deterministic SHA-256-derived IDs with collision checks. Original IDs and requests remain in the response. `prestige:*` receipt IDs are retained exactly and use the `migration:cr-settled` fingerprint recognized by the restored milestone gate. No accepted receipt is evicted. A migration receipt records the canonical source SHA-256, revision, and policy actions. Identical inputs and policies produce identical output; converted target inputs and existing outputs are refused, preventing double conversion/refunds.

CR `social:rank:N` receipts generate finished `hit_social_rank_N` records with completion count 1. Finished/claimed achievement state remains settled. The latest persisted payroll completion is propagated to both normal and master-prestige payroll IDs so their common cooldown survives. A payroll receipt without recoverable completion time fails. Source reward descriptors are archived, not executed; imported records carry no grantable reward actions. Unclaimed `claimable` rows fail because their reward catalog may have changed.

Ordinary CR rank events used opaque, pruned event receipts, so per-rank paid counts cannot be reconstructed. Conversion adds a permanent `cr_progression_rebaseline` marker. The restored progression code must baseline Multiplayer and Zombies independently at frontend readiness, with initialized native profile statistics and the virtual lobby loaded, before issuing rank rewards. Pending rank events remain retryable and do not consume the baseline. If loaded statistics are first observed outside that frontend callback, restart in the frontend before playing; the guard blocks migration rewards for that session. This avoids replaying rank awards already earned; the source economy and native profile statistics must describe the same profile. A restored/rolled-back or unrelated native stats profile is unsupported.

## Changed active objectives

Default policy refuses active orders/contracts. No target/timer compatibility is assumed.

* `--contract-policy refund-token` cancels an `inProgress` CR MP/Zombies contract and restores exactly one permanent retail cost token from the converter's verified CR mapping. It retains the original progress, timing and rewards in the archival receipt, resets the target record to inactive, and reports the refund. Unknown contracts and an existing positive cost-token record fail. It does not guess a cash price, grant rewards, or refund a completed/abandoned contract. Restart the refunded token through the restored catalog after a separate, deliberate installation.
* `--order-policy archive-inactive` explicitly archives unpaid active free-order progress and resets that target record to inactive. It does not affect inventory or wallets. Claimable rewards remain unsupported under this policy. The four recognized Above & Beyond names (MP/Zombies daily/weekly) are never imported with empty grant descriptors: by default their presence fails. This reset policy archives only unpaid meta rows without a live target meta record, allowing production code to create its canonical descriptor and counter. Any finished/claimable row, claim receipt, completion timestamp, or positive legacy completion count requires explicit external reconciliation; resetting it could repay a settled bonus. All original rows remain in archival receipts and every reset is listed in the report.

## Validation and limits

Input is bounded to 16 MiB with at most 10,000 rows per CR collection. All object fields are checked against the supported schema. Duplicate JSON keys, duplicate IDs, unknown fields, booleans/floats where integers are required, negative/overflowing values, invalid statuses, non-printable identifiers, excess metadata/rewards, and native-bound violations fail before output creation. Target inventory/receipt limits are 65,536, embedded achievement state 8 MiB, file 64 MiB, native achievement progress 16-bit, names 64 bytes, and archived response 6,143 bytes. Large archival rows or reports fail instead of losing data. Read/parse/validation failures return exit code 2 and print the cause.

Unsupported: automatic installation; merging an existing target profile; unknown CR schema extensions; positive inventory ID collisions; nonzero inventory expiry without verified native duration; settled or claimable Above & Beyond state; external reward settlement; reconstructing missing/previously pruned receipts or stock; choosing legacy currency corrections; unverified contract refunds; automatic target/timer restart; and account-ownership verification. Keep the original backups and review all policy changes before any manual installation.

## Synthetic verification

```powershell
python tests/economy-migration/test_migration.py
python tests/economy-migration/test_migration.py --emit-fixture C:\migration\synthetic-new.json
```

Tests cover strict types and schema, duplicate keys/receipts, metadata bounds, currency ambiguity, inventory collisions and nonzero-expiry refusal, Above & Beyond explicit unpaid reset and settled-state refusal, payroll/Social Score settlement, milestone receipt fingerprints, deterministic output, explicit contract refund and refund collisions, free-order policy, optional legacy records, dry-run, exclusive output creation and unchanged inputs. The fixture emitter is only for synthetic native-loader testing and is not a profile conversion command.


The synthetic suite passes 17 tests. A separate native harness compiled the production marketplace persistence and achievement store parsers and accepted the emitted fixture (ready, wallet and refunded-token inventory intact, achievement store available). Catalog seed data was stubbed for this loader check. This confirms schema acceptance; it does not validate a real profile, installation, or in-game behavior.
