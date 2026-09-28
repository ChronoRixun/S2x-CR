# Scripted next map (s2x_nextmap): test record

Branch `feat/scripted-next-map`, from `integration` at 0ef554f (v1.6.0). Release x64 build, no warnings. Bots only, private LAN runtime (`sv_lanOnly 1`, `master_server_enable 0`, `bot_fill 4`, `party_minplayers 1`, `party_matchStartDelay 5`), ports 27080-27083. No human client and no vote script were used. The design's test numbers (B1-B11) are used below.

Files here:
- `nextmap-test.cfg`: the B1-B3/B7 server (rotation war, dom, gun, dm on `mp_shipment_s2`, no time or score limits).
- `cfg-probe.cfg`: the B11 server (30-entry rotation plus the quoting probes). UTF-8 without BOM and LF line ends, the way Server Manager writes a cfg.
- `nextmap_probe.gsc`: fixture, idle unless `scr_nmtest` is set. It prints the next-map dvars at each map start, runs the B2 steps (`scr_nmtest 2`), prints the B11 values (`scr_nmtest_b11 1`), the end-of-game notifies, and a 1 s heartbeat (`scr_nmtest_hb 1`). Copy it into `<server>\s2x\scripts\mp\` for a run and remove it afterwards.

Launch, from the game folder: `s2x.exe -noupdate -dedicated -terminal +set net_port <port> +set sv_lanOnly 1 +set master_server_enable 0 +set g_consoleLog s2x\logs\nextmap-<port>.log +exec nextmap-test.cfg +map_rotate`. Console lines were typed into the `-terminal` console with a small WriteConsoleInput helper, and the log was followed with wall-clock offsets. Map cycles took about 11 s once cached.

Note: a bare dvar name typed into the console (or on its own line in a cfg) prints nothing to `g_consoleLog` on this build and gives no `Unknown command` either. Values were read with `dvarDump <file>` (exact values in `s2x\<file>.txt`) and with the fixture's `getdvar` lines.

## B1: plumbing: pass

| Check | Result |
| --- | --- |
| Rotation listing | `Dedicated map rotation:` 1-4 (war, dom, gun, dm), then `selected next map mp_shipment_s2 war`. |
| Capability | `s2x_nextmap_api "1"`. `set s2x_nextmap_api 0` leaves it at `"1"` (read-only). The script sees `api=1`. |
| Preview | `s2x_nextmap_preview "mp_shipment_s2 dom"`, `s2x_nextmap ""`. |
| `set s2x_nextmap "mp_shipment_s2 gun"`, `endMatch` | In order: `match ended` (+1.4 s), `returned to lobby`, `vote chose the next map mp_shipment_s2 gun (replaces the next rotation entry)`, `selected next map mp_shipment_s2 gun`. Next map start: `preview="mp_shipment_s2 dm" nextmap=""` (dom replaced, gun used up). |
| Then `endMatch` twice | `selected next map mp_shipment_s2 dm`, then `... war` (preview `dom`). |
| Natural end (time limit 1, `s2x_nextmap` set mid-match) | `game_ended` t=80050, `exitLevel_called` t=93600, `match ended` 1.1 s later, then `vote chose the next map mp_shipment_s2 gun` and `selected ... gun`. This is the path the vote uses. |
| Value in the startup cfg (`set s2x_nextmap "mp_shipment_s2 dm"`) | First match: `vote chose the next map mp_shipment_s2 dm`, then `selected ... dm`; preview `mp_shipment_s2 dom` (war replaced). |

## B2: rejection: pass

The fixture set one value per match with `setdvar` (so `;` never went through the command buffer); each match was ended with `endMatch`.

| Value | Log | Next map |
| --- | --- | --- |
| `mp_shipment_s2 war;quit` | `ignoring s2x_nextmap 'mp_shipment_s2 war;quit'; the rotation continues.` Server kept running, nothing was executed. | gun (the preview) |
| `mp_doesnotexist war` | `Multiplayer map 'mp_doesnotexist' is not present in the stock arena catalog.`, then the ignoring line | dm (the preview) |
| `mp_shipment_s2 war extra` | ignoring line | war (the preview) |
| `MP_SHIPMENT_S2 DM` | `vote chose the next map mp_shipment_s2 dm` (lower-cased) | dm; preview afterwards `dom` |
| `mp_shipment_s2` (one token) | ignoring line | gun (the preview) |
| `  mp_carentan_s2   dom  ` | `vote chose the next map mp_carentan_s2 dom` | Carentan loaded; preview afterwards `mp_shipment_s2 war` |

`s2x_nextmap` read back empty at every following map start, including after a rejection.

## B3: operator override wins: pass

`set s2x_nextmap "mp_shipment_s2 gun"`, then `map mp_shipment_s2 conf` (`Next dedicated match set to mp_shipment_s2 conf.`). `dvarDump`: `s2x_nextmap_preview "mp_shipment_s2 conf fixed"`. `endMatch`: `the server's map override replaces the vote result mp_shipment_s2 gun.`, then `selected next map mp_shipment_s2 conf`. At the conf start: `preview="mp_shipment_s2 dom" nextmap=""`; the next `endMatch` selected dom, so the rotation index did not move.

## B7: endMatch: new code pass; pre-existing hazard found

`endMatch` does not go through the GSC endGame path: none of `game_ended`, `game_win`, `round_end_finished`, `final_killcam_done`, `spawning_intermission` or `exitLevel_called` fired, and the map unloaded directly once the end took effect. A vote hook on `game_win`/`_id_75E7` therefore never runs on `endMatch`; the `_id_3E16`/`_id_4DFE` guard is not what protects it.

Time from typing `endMatch` in live play to `match ended` (the map unload comes 1.0-1.5 s before it), this build, 22 matches:
- 12: 1.1-1.6 s.
- 4: 3.3, 3.4 (while a `dvarDump` was still writing), 6.2 and 7.6 s.
- 6: 25.9, 29.4, 31.1, 31.3, 31.5 and 31.5 s. Where the unload was timed, it came at 24.8, 28.4, 30.0 and 30.1 s. The 1 s heartbeat showed the match carrying on with 4 players until then, and `status` answered at once, so the game was running, not hung.
- Four of the six reached the unload at about 30 s, which is also the lifecycle's own `ending_match` limit. This build survived all four.

Baseline, same cfg with the runtime's previous `s2x.exe` (no s2x_nextmap code): 1.2-1.5 s five times, 5.8 s once, then on the seventh the match was still running at +29.9 s and the lifecycle ended the process: `Dedicated party: normal match end timed out.` (dedicated_party.cpp `ending_match`, `utils::nt::terminate(1)`). This is v1.6.0 behaviour, not part of this change: the new code runs only after `returned to lobby`.

`endMatch` during a natural postgame (the window a vote would hold), 2 runs: sent 0.3 s after `game_ended`, and 1 s after `round_end_finished`. Both unloaded at once; `match ended` after 1.2 and 1.3 s.

## B11: cfg quoting: every probe kept byte for byte, except a trailing backslash

`cfg-probe.cfg` executed at startup by `+exec`. No `Unknown command` line, no `nmsplit_*` marker executed. Values as scripts read them (`getdvar`, fixture) and as `dvarDump` stored them, which are the same:

| Line in the cfg | Stored value | Bytes (expected) |
| --- | --- | --- |
| `set nm_semi "Be nice; nmsplit_a"` | `Be nice; nmsplit_a` | 18 (18) |
| `set nm_semi_marker "x;nmsplit_b"` | `x;nmsplit_b` | 11 (11) |
| `set nm_url "https://discord.gg/abc123"` | `https://discord.gg/abc123` | 25 (25) |
| `set nm_comment "before // after"` | `before // after` | 15 (15) |
| `set nm_trailc "kept" // trailing comment nmsplit_c` | `kept` (the comment is dropped) | 4 (4) |
| `set nm_bslash_mid "C:\path\to\file"` | `C:\path\to\file` | 15 (15) |
| `set nm_bslash_end "ends with backslash\"` | `ends with backslash"`: `\"` is read as an escaped quote. The backslash is lost, a `"` is kept, and the value runs to the end of the line. | 20 |
| `set nm_after_bslash "next line ok"` | `next line ok` (the line after is not affected) | 12 (12) |
| `set nm_colour "^1Red ^2Green ^7White ^:rainbow"` | unchanged | 31 (31) |
| `set nm_caret_end "caret at end^"` | unchanged | 13 (13) |
| `set nm_pct "100% sure %d %x %% done"` | unchanged, not formatted | 23 (23) |
| `set nm_pct_s "fmt %s %s %s end"` | unchanged, no crash | 16 (16) |
| `set nm_utf8 "Café – Zürich ★ 日本"` | unchanged (UTF-8 bytes intact; client rendering not checked) | 28 (28) |
| `set nm_apos "don't camp"` | unchanged | 10 (10) |
| `set nm_long120 "<120 bytes>"` | unchanged | 120 (120) |
| `set nm_unquoted two words unquoted` | `two words unquoted` | 18 |
| `set nm_last "last line reached"` | unchanged | 17 (17) |

30-entry `sv_maprotation` (914 characters, 26 maps, 8 modes): `Dedicated map rotation:` listed 1-30, and the fixture, parsing the way `s2x_servercmds.gsc` does, printed `sv_maprotation len=914 words=120 pool=30`.

For the Manager's sanitiser this means `;`, `//`, `%`, `^` and UTF-8 need no replacement inside a quoted `set` value. A `\` needs handling only as the last character before the closing quote. `"` must still become `'`, as today.

## Not run here

- B0, B4-B6, B8-B10 need the vote script (`s2x_servercmds.gsc`) or an older exe; they are Manager-side or later work.
- Human tests H1-H5.
