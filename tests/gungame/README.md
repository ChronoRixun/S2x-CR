# Gun Game bot costumes (CR #4)

Install `data/scripts/mp/s2x_gungame_bots.gsc` as `s2x/scripts/mp/s2x_gungame_bots.gsc` on the host/server. It uses the existing GSC loader and needs no client download. The same script supports a dedicated server or a locally hosted Gun Game match.

The extracted Gun Game weapon-change function updates the division, then builds a costume from player-profile `costumes`/`globalCostume` fields before applying customization and sending `applyLoadout`. This bypasses the bot-specific path in the normal class script, which obtains a generated division costume from builtin `0x333`. Bots can therefore lose their generated uniform when Gun Game applies profile-backed costume data. The division itself is assigned; the issue's original missing-division hypothesis was not the cause found in the scripts.

The repair listens for that completed loadout update on bots only. It chooses the same Allies costume source as the normal bot class path, caches the result by division, and reapplies the costume through method `0x84C7` and `loadcustomizationplayerview`. It preserves the current weapon and division. Division 5 uses the normal default-division fallback. A native customization override flag is respected, and Sandbox is excluded because its special appearance path is also excluded by stock Gun Game. Human players and other gametypes are untouched.

Local research references (not distributed): `maps_mp_gametypes_gun.gscbin.gsc` weapon-change body; class token `0x4C4` / `1220.gscbin.gsc`, costume helpers `_id_1F93`/`_id_1F97` and the bot branch in the class application; teams token `0x510` / `1296.gscbin.gsc`, `_id_73CA` customization application. `_id_0079` is the current division, `_id_267E` the costume array and `_id_5097` the customization override flag.

Verification on September 21, 2026:

- Compiled with the local S2 GSC compiler (250 bytecode bytes).
- A hidden local dedicated server ran London Docks Gun Game, four bots, and a full match restart: two match starts, 40 repair calls across division changes, no script runtime error in the captured run.
- Evidence is retained locally in `build/tests/gungame/dedicated-gun.log`. The validation process was stopped and its temporary config removed.

Headless execution cannot establish the rendered result. Check initial bodies, promotions, respawns, human appearance, Sandbox and a Gun Game → Domination → Gun Game rotation in the [morning runbook](../MORNING-RUNBOOK.md). Set `s2x_gungame_debug 1` for repair messages, then return it to `0`.
