# CR catalog and Orders restoration

Reference: CR integration `6bfd9d6`, its checked-in contract/Orders/bundle policy headers, and archived retail item table (`issue-24-pool.csv`). Candidate starts from upstream base `2541f12`. No player profile was read to derive these definitions.

Restored named definitions: 56 MP contracts (48 Rare weapon rewards at 2500 AC, eight generic contracts), 41 weapon variant daily Orders, 20 generic MP daily Orders, ten MP weekly Orders, 20 Zombies daily Orders, seven Zombies weekly Orders, eight Zombies contracts. All 162 are scheduled. Candidate has 267 offers total, retaining 105 upstream-only offers. Four Zombies dailies were missing entirely: Jack-in-the-Box, Ripsaw heavy, propeller trap, and Meuchler spines.

Targets, timers and success rewards for matching CR names use CR policy. Generic XP uses native currency 1; SocialScore wins use currency 7 at 250 points. Native XP display remains untested. Weapon rewards are the captured Rare variant GUID, never the hidden base GUID.

Catalog additions use the upstream catalog/product and atomic purchase APIs: 64 contract token products, nine promotional cosmetic bundles, and two rare drop products. CWL's 16 packs use CR's 1000 AC price. Rare MP/Zombies products 2/6 use 1000 AC and precede captured MP/ZM tag matches. Captured CP rare/bulk and other drop products remain present and retain their purchase route. All nine special-bundle publisher tag/start/end triples match their catalog products, with windows 0..2147483647. C.O.D.E. feature GUIDs remain part of their bundle contents.

Merged scheduler differences: upstream resets at 17:00 UTC and keeps its weekly phase; CR used midnight. The MP contract pool has 69 rotating names (56 CR plus 13 upstream-only), showing nine consecutive cards and moving one row daily. Original CR had a 56-name cycle. Daily/weekly rotations retain upstream pool ordering and append restored names, rather than CR's original generic/variant interleave. Unique pool sizes are 39 special MP, 82 daily MP, 20 weekly MP, 69 contract MP, 28 daily Zombies, nine weekly Zombies, 12 contract Zombies, four special Zombies; four Moonraven contracts remain always available.

Zombies Above & Beyond is settled atomically after successful claims: six daily or three weekly claims award product 6. Four canonical meta descriptors are queryable before a first claim. Native receipt replay and rollback protect both reward and counter. Its counter follows upstream's reset-on-award cumulative semantics, not CR's legacy day/week-reset bookkeeping. MP meta counters stay separate; special Orders do not increment these bonuses.

Unverified live behavior: actual vendor tile/layout/cache behavior, native XP presentation, event hooks and remote/co-op progress. Offline tests confirm definitions and backend decisions; they do not run the game. Production profile migration is handled separately.

## Checks

Run from the candidate repository root:

```powershell
msbuild tests/cr-catalog/claims.vcxproj /m /v:minimal /p:Configuration=Release /p:Platform=x64
./build/tests/cr-catalog/bin/claims.exe
python tests/cr-catalog/catalog.py
```

The C++ harness compiles production catalog, SKU/product serialization, byte buffer, purchase, resource scheduler, predicate/settlement, achievement store and marketplace persistence. Resource IO, unrelated HQ initial records, the unused native scheduled-response serializer, and crypto dependency linkage use local fixtures (crypto runs real Windows SHA256/Base64). It reads the committed embedded catalog files and writes only a unique synthetic temporary profile. It tests actual manifest hashes, all nine bundle debit/grant/replay, rare AC debit 6 with no CP debit, captured CP rare debit 2, current timed contract purchase/token consumption/settlement/claim, 600-day scheduling coverage, paid-token retention, predicate negatives, ZM bonus rollback/replay and MP meta isolation. Claim callbacks are test reward callbacks; native reward pushes and hooks remain live smoke checks.

The Python check validates token-to-offer uniqueness, 48 Rare weapon rewards, bundle contents/limiter/tile windows, SocialScore win rewards, quotas and captured identity collision avoidance. Both checks passed. Full Debug/Release client builds are coordinated at repository level.

## Exact CR definition coverage

Kind 1 = MP daily, 2 = MP weekly, 4 = MP contract, 8 = Zombies daily, 9 = Zombies weekly, 11 = Zombies contract. Timer is seconds. Currency 1 = XP, 6 = AC, 7 = SocialScore. Product rows show exact item GUID and quantity.

| Name | Kind | Target | Timer | Reward |
| --- | ---: | ---: | ---: | --- |
| `daily_ch_assault_headshots` | 1 | 5 | — | item 0x1 x 1 |
| `daily_ch_assault_kills` | 1 | 35 | — | item 0x1 x 1 |
| `daily_ch_assists` | 1 | 10 | — | currency 1 x 250 |
| `daily_ch_barloot0_1` | 1 | 35 | — | item 0x1055100 x 1 |
| `daily_ch_barloot1_2` | 1 | 3 | — | item 0x1055202 x 1 |
| `daily_ch_barloot2_1` | 1 | 5 | — | item 0x1055301 x 1 |
| `daily_ch_barloot3_1` | 1 | 5 | — | item 0x1055400 x 1 |
| `daily_ch_brenloot0_1` | 1 | 5 | — | item 0x1026100 x 1 |
| `daily_ch_brenloot0_2` | 1 | 3 | — | item 0x1026102 x 1 |
| `daily_ch_brenloot2_1` | 1 | 25 | — | item 0x1026301 x 1 |
| `daily_ch_brenloot2_2` | 1 | 5 | — | item 0x1026302 x 1 |
| `daily_ch_brenloot3_2` | 1 | 25 | — | item 0x1026402 x 1 |
| `daily_ch_destroy_scorestreaks` | 1 | 2 | — | currency 1 x 300 |
| `daily_ch_dom_caps` | 1 | 5 | — | currency 1 x 250 |
| `daily_ch_dom_wins` | 1 | 1 | — | currency 7 x 250 |
| `daily_ch_equipment_kills` | 1 | 5 | — | item 0x1 x 1 |
| `daily_ch_ffa_killer` | 1 | 15 | — | item 0x1 x 1 |
| `daily_ch_headshots` | 1 | 3 | — | item 0x1 x 1 |
| `daily_ch_karloot1_2` | 1 | 3 | — | item 0x1016202 x 1 |
| `daily_ch_karloot2_1` | 1 | 3 | — | item 0x1016301 x 1 |
| `daily_ch_karloot3_1` | 1 | 15 | — | item 0x1016400 x 1 |
| `daily_ch_karloot3_2` | 1 | 3 | — | item 0x1016402 x 1 |
| `daily_ch_kills` | 1 | 25 | — | item 0x1 x 1 |
| `daily_ch_killstreak` | 1 | 3 | — | currency 1 x 300 |
| `daily_ch_lewisloot0_1` | 1 | 25 | — | item 0x1015100 x 1 |
| `daily_ch_lewisloot0_2` | 1 | 3 | — | item 0x1015102 x 1 |
| `daily_ch_lewisloot2_1` | 1 | 5 | — | item 0x1015301 x 1 |
| `daily_ch_lewisloot2_2` | 1 | 5 | — | item 0x1015302 x 1 |
| `daily_ch_lewisloot3_2` | 1 | 25 | — | item 0x1015402 x 1 |
| `daily_ch_lmg_headshots` | 1 | 5 | — | item 0x1 x 1 |
| `daily_ch_lmg_kills` | 1 | 25 | — | item 0x1 x 1 |
| `daily_ch_lugerloot0_1` | 1 | 3 | — | item 0x102C100 x 1 |
| `daily_ch_lugerloot0_2` | 1 | 10 | — | item 0x102C102 x 1 |
| `daily_ch_lugerloot2_1` | 1 | 10 | — | item 0x102C301 x 1 |
| `daily_ch_lugerloot3_1` | 1 | 5 | — | item 0x102C400 x 1 |
| `daily_ch_lugerloot3_2` | 1 | 5 | — | item 0x102C402 x 1 |
| `daily_ch_m1911loot0_1` | 1 | 10 | — | item 0x1014100 x 1 |
| `daily_ch_m1911loot0_2` | 1 | 3 | — | item 0x1014102 x 1 |
| `daily_ch_m1911loot1_2` | 1 | 5 | — | item 0x1014202 x 1 |
| `daily_ch_m1911loot2_1` | 1 | 5 | — | item 0x1014301 x 1 |
| `daily_ch_m30loot0_1` | 1 | 100 | — | item 0x1023100 x 1 |
| `daily_ch_m30loot0_2` | 1 | 5 | — | item 0x1023102 x 1 |
| `daily_ch_m30loot1_1` | 1 | 3 | — | item 0x1023200 x 1 |
| `daily_ch_m30loot3_1` | 1 | 100 | — | item 0x1023400 x 1 |
| `daily_ch_m30loot3_2` | 1 | 5 | — | item 0x1023402 x 1 |
| `daily_ch_pistol_kills` | 1 | 10 | — | item 0x1 x 1 |
| `daily_ch_shotgun_kills` | 1 | 25 | — | item 0x1 x 1 |
| `daily_ch_shovel_kills` | 1 | 5 | — | currency 1 x 500 |
| `daily_ch_smg_kills` | 1 | 30 | — | item 0x1 x 1 |
| `daily_ch_sniper_headshots` | 1 | 3 | — | item 0x1 x 1 |
| `daily_ch_sniper_kills` | 1 | 15 | — | item 0x1 x 1 |
| `daily_ch_tdm_wins` | 1 | 1 | — | currency 7 x 250 |
| `daily_ch_thompsonloot0_1` | 1 | 30 | — | item 0x1012100 x 1 |
| `daily_ch_thompsonloot0_2` | 1 | 3 | — | item 0x1012102 x 1 |
| `daily_ch_thompsonloot2_1` | 1 | 5 | — | item 0x1012301 x 1 |
| `daily_ch_thompsonloot3_1` | 1 | 30 | — | item 0x1012400 x 1 |
| `daily_ch_winchesterloot0_1` | 1 | 5 | — | item 0x1017100 x 1 |
| `daily_ch_winchesterloot1_2` | 1 | 3 | — | item 0x1017202 x 1 |
| `daily_ch_winchesterloot2_1` | 1 | 3 | — | item 0x1017301 x 1 |
| `daily_ch_winchesterloot3_1` | 1 | 100 | — | item 0x1017400 x 1 |
| `daily_ch_winchesterloot3_2` | 1 | 100 | — | item 0x1017402 x 1 |
| `weekly_ch_airborne_kills` | 2 | 100 | — | item 0x2 x 1 |
| `weekly_ch_armored_kills` | 2 | 100 | — | item 0x2 x 1 |
| `weekly_ch_expeditionary_kills` | 2 | 100 | — | item 0x2 x 1 |
| `weekly_ch_explosive_kills` | 2 | 50 | — | item 0x2 x 1 |
| `weekly_ch_infantry_kills` | 2 | 100 | — | item 0x2 x 1 |
| `weekly_ch_kills` | 2 | 500 | — | item 0x2 x 1 |
| `weekly_ch_long_range_kills` | 2 | 50 | — | item 0x2 x 1 |
| `weekly_ch_mountain_kills` | 2 | 100 | — | item 0x2 x 1 |
| `weekly_ch_scorestreak_calls` | 2 | 25 | — | item 0x2 x 1 |
| `weekly_ch_wins` | 2 | 10 | — | item 0x2 x 1 |
| `contract_25_kills_dom` | 4 | 25 | 1200 | currency 1 x 3000 |
| `contract_25_kills_lmg` | 4 | 25 | 1200 | currency 1 x 3000 |
| `contract_45_kills` | 4 | 45 | 2400 | currency 1 x 3000 |
| `contract_4_headshots_tdm` | 4 | 4 | 1200 | currency 1 x 3000 |
| `contract_50_kills_lmg` | 4 | 50 | 3000 | item 0x1 x 1 |
| `contract_50_kills_smg` | 4 | 50 | 2400 | currency 1 x 3000 |
| `contract_55_kills_tdm` | 4 | 55 | 3000 | item 0x1 x 1 |
| `contract_9_headshots` | 4 | 9 | 2400 | currency 1 x 3000 |
| `contract_ch_3line` | 4 | 10 | 4800 | item 0x1088100 x 1 |
| `contract_ch_arisaka` | 4 | 15 | 3600 | item 0x1024100 x 1 |
| `contract_ch_austen` | 4 | 30 | 3600 | item 0x1083100 x 1 |
| `contract_ch_autom2` | 4 | 45 | 3600 | item 0x108D100 x 1 |
| `contract_ch_avs` | 4 | 75 | 4500 | item 0x1094100 x 1 |
| `contract_ch_baseballbat` | 4 | 25 | 3600 | item 0x1070100 x 1 |
| `contract_ch_bechowiec` | 4 | 200 | 10800 | item 0x1084100 x 1 |
| `contract_ch_blunderbuss` | 4 | 20 | 2100 | item 0x104C100 x 1 |
| `contract_ch_blyskawica` | 4 | 10 | 3600 | item 0x1085100 x 1 |
| `contract_ch_burstrifle` | 4 | 60 | 3600 | item 0x1071100 x 1 |
| `contract_ch_charlton` | 4 | 45 | 3600 | item 0x1097100 x 1 |
| `contract_ch_chatellerault` | 4 | 50 | 6000 | item 0x10A5100 x 1 |
| `contract_ch_claymore` | 4 | 25 | 2700 | item 0x108C100 x 1 |
| `contract_ch_combatknife` | 4 | 10 | 3600 | item 0x104F100 x 1 |
| `contract_ch_crossbow` | 4 | 5 | 3600 | item 0x10A4100 x 1 |
| `contract_ch_dagger` | 4 | 40 | 7200 | item 0x1078100 x 1 |
| `contract_ch_delisle` | 4 | 50 | 3600 | item 0x108A100 x 1 |
| `contract_ch_emp` | 4 | 30 | 3600 | item 0x1086100 x 1 |
| `contract_ch_enfield` | 4 | 50 | 7200 | item 0x102E100 x 1 |
| `contract_ch_erma` | 4 | 15 | 5100 | item 0x10A1100 x 1 |
| `contract_ch_fire_axe` | 4 | 15 | 1800 | item 0x1079100 x 1 |
| `contract_ch_gdb` | 4 | 15 | 5100 | item 0x109C100 x 1 |
| `contract_ch_gewehr` | 4 | 200 | 7200 | item 0x1020100 x 1 |
| `contract_ch_gpmg` | 4 | 75 | 7200 | item 0x1056100 x 1 |
| `contract_ch_icepick` | 4 | 50 | 7200 | item 0x1053100 x 1 |
| `contract_ch_kg` | 4 | 75 | 4800 | item 0x1081100 x 1 |
| `contract_ch_lad` | 4 | 10 | 4800 | item 0x109D100 x 1 |
| `contract_ch_lever_action` | 4 | 10 | 1800 | item 0x107D100 x 1 |
| `contract_ch_m1919` | 4 | 10 | 4800 | item 0x1027100 x 1 |
| `contract_ch_m2` | 4 | 30 | 3600 | item 0x1099100 x 1 |
| `contract_ch_m36` | 4 | 10 | 4800 | item 0x1087100 x 1 |
| `contract_ch_mas38` | 4 | 60 | 4500 | item 0x1075100 x 1 |
| `contract_ch_mg81` | 4 | 25 | 4800 | item 0x1040100 x 1 |
| `contract_ch_nambu` | 4 | 30 | 3600 | item 0x1073100 x 1 |
| `contract_ch_orso` | 4 | 65 | 7200 | item 0x101D100 x 1 |
| `contract_ch_ptrs` | 4 | 50 | 3600 | item 0x1089100 x 1 |
| `contract_ch_revolver` | 4 | 20 | 7200 | item 0x102F100 x 1 |
| `contract_ch_ribeyrolls` | 4 | 15 | 5100 | item 0x1098100 x 1 |
| `contract_ch_sledge` | 4 | 10 | 1800 | item 0x107C100 x 1 |
| `contract_ch_sten` | 4 | 30 | 7200 | item 0x101A100 x 1 |
| `contract_ch_sterling` | 4 | 30 | 3600 | item 0x1074100 x 1 |
| `contract_ch_tokyo` | 4 | 200 | 10800 | item 0x10A0100 x 1 |
| `contract_ch_trenchknife` | 4 | 1 | 7200 | item 0x106F100 x 1 |
| `contract_ch_type5` | 4 | 10 | 3600 | item 0x1030100 x 1 |
| `contract_ch_vmg` | 4 | 10 | 4800 | item 0x1072100 x 1 |
| `contract_ch_volk` | 4 | 200 | 7200 | item 0x101E100 x 1 |
| `contract_ch_wimmer` | 4 | 150 | 7200 | item 0x10A6100 x 1 |
| `contract_ch_wz` | 4 | 50 | 3600 | item 0x10A2100 x 1 |
| `daily_zm_ch_bomber_bomb_kills` | 8 | 15 | — | currency 6 x 250 |
| `daily_zm_ch_bomber_with_bomb` | 8 | 1 | — | currency 6 x 250 |
| `daily_zm_ch_empty_clip` | 8 | 3 | — | currency 6 x 250 |
| `daily_zm_ch_explosives_1` | 8 | 40 | — | currency 6 x 250 |
| `daily_zm_ch_headshots_1` | 8 | 25 | — | currency 6 x 250 |
| `daily_zm_ch_kills_bombers_clean` | 8 | 5 | — | currency 6 x 250 |
| `daily_zm_ch_kills_jack_in_box_1` | 8 | 40 | — | currency 6 x 250 |
| `daily_zm_ch_kills_pistols_upgraded_1` | 8 | 60 | — | currency 6 x 250 |
| `daily_zm_ch_kills_ripsaw_1` | 8 | 25 | — | currency 6 x 250 |
| `daily_zm_ch_kills_traps_1` | 8 | 50 | — | currency 6 x 250 |
| `daily_zm_ch_kills_upgraded_1` | 8 | 100 | — | currency 6 x 250 |
| `daily_zm_ch_lmgs_1` | 8 | 75 | — | currency 6 x 250 |
| `daily_zm_ch_pest_melee` | 8 | 15 | — | currency 6 x 250 |
| `daily_zm_ch_pistols_1` | 8 | 50 | — | currency 6 x 250 |
| `daily_zm_ch_shotguns_1` | 8 | 50 | — | currency 6 x 250 |
| `daily_zm_ch_sniper_headshots_1` | 8 | 20 | — | currency 6 x 250 |
| `daily_zm_ch_throwing_knifes_1` | 8 | 15 | — | currency 6 x 250 |
| `daily_zm_ch_traps_propeller_1` | 8 | 30 | — | currency 6 x 250 |
| `daily_zm_ch_wave_asn_spines_1` | 8 | 2 | — | currency 6 x 250 |
| `daily_zm_ch_wustling_melee_only` | 8 | 2 | — | currency 6 x 250 |
| `weekly_zm_ch_headshots_1` | 9 | 200 | — | item 0x6 x 1 |
| `weekly_zm_ch_kills_1` | 9 | 750 | — | item 0x6 x 1 |
| `weekly_zm_ch_kills_benners_1` | 9 | 10 | — | item 0x6 x 1 |
| `weekly_zm_ch_kills_electrical_1` | 9 | 200 | — | item 0x6 x 1 |
| `weekly_zm_ch_kills_equipment` | 9 | 150 | — | item 0x6 x 1 |
| `weekly_zm_ch_ripsaw_kills_no_heavy` | 9 | 100 | — | item 0x6 x 1 |
| `weekly_zm_ch_traps_1` | 9 | 150 | — | item 0x6 x 1 |
| `contract_zm_ch_beheadings_1` | 11 | 50 | 3600 | item 0x6 x 1 |
| `contract_zm_ch_jumping_knifes_1` | 11 | 15 | 900 | item 0x6 x 1 |
| `contract_zm_ch_kills_1` | 11 | 250 | 3000 | item 0x6 x 1 |
| `contract_zm_ch_kills_2` | 11 | 400 | 3600 | item 0x6 x 1 |
| `contract_zm_ch_kills_3` | 11 | 750 | 7200 | item 0x6 x 1 |
| `contract_zm_ch_unique_kills_fr_1` | 11 | 1 | 1800 | item 0x6 x 1 |
| `contract_zm_kills_freefire_1` | 11 | 175 | 1800 | item 0x6 x 1 |
| `contract_zm_treasure_1` | 11 | 1 | 10800 | item 0x6 x 1 |
