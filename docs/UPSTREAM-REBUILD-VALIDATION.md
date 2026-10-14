# Upstream rebuild validation â€” 2026-10-13

Candidate branch: `codex/s2x-cr-upstream-rebuild`, based on upstream master `0048253`.
Original CR reference: `integration` at `6bfd9d6`. The original checkout, installed executable, deployed servers and existing economy/profile stores were not replaced.

## Build and offline checks

- Fresh upstream Release baseline passed after populating missing tracked files in the new dependency clones. Every submodule uses the upstream pin, including FreeType/RmlUi.
- Candidate native Release and Debug x64 passed. The initial Debug dependency build reported upstream `deps/gsc-tool/src/gsc/parser.cpp:190` C4702 (unreachable code); candidate source introduced no compiler warnings. Final incremental builds passed.
- Server Manager Debug and Release passed without warnings/errors.
- Manager regression components passed: backend/presets, browser/UDP, browser UI, nine themes/layouts, card order/drag, administration, auto-balance, server scripts/mapvote settings, window collection and Zombies survival profiles. An initial combined run stopped at an existing 500 ms WMI process-query timeout; the measured 2 s bound fixed it and administration was rerun successfully.
- Status harness passed with PowerShell 7 and Windows PowerShell 5.1, including real loopback raw/legacy UDP fixtures, invalid source/challenge rejection and intact final fields.
- Production chat routing harness passed, including callback payload ownership, team chat, size/entity/lobby guards and balanced script references.
- Script storage harness passed filename confinement, text round trips, bounded payloads, atomic replacement and save-failure behavior.
- Native local admin policy/pipe harness passed framing, owner-only DACL/local-only pipe, identity equality, protected targets, cancellation/expiry/deduplication, fragmented IO, instance reuse and shutdown cancellation.
- Extracted production next-map/HUD-limit/capacity harness passed malformed/consumed votes, operator priority, duplicate skip, pending rotation, upstream rotation-current behavior, query validation and callback cancellation/supersession.
- Production AppData selector harness failed before the override implementation, then passed four cases: unchanged default resolution/cache, absolute override with no known-folder access, relative-path rejection and missing-value rejection.
- Independent review found no confirmed introduced hosting/admin defect. It identified shared AppData script loading as a rollout limitation; isolated tests now use the explicit override and a separate working folder. A historical bot-name-menu parity concern was retracted after checking current CR.

Logs and generated harnesses are under `build/logs`, `build/tests` and `build/server-admin-tests` in this checkout. They are not release files.

## Isolated live evidence

The smoke folder is `build/runtime-smoke`. Game asset files are hard-linked and asset directories junction-linked for reading; executable, matching loose scripts, config, profile directory, log and AppData root are separate. The launch uses `-dedicated -multiplayer -nosteam -noupdate -appdata-dir <absolute scratch root>`, LAN-only port 29317 and disabled master announcements. No original game/profile files were installed over or migrated.

The candidate reached an MP London Docks Domination match. Its CR event/bot scripts and embedded server-admin init script executed. `bot_fill 3` initially added two bots and filled the third on its bounded retry. Raw upstream-style `s2x_getInfo` returned the requested challenge, `sv_running=1`, `clients=3`, `bots=3`, `sv_maxclients=4`, correct map/gametype/session fields and `s2x_scorelimit=50`. Thus a bot-only server remains visible under the upstream count validation and all three slots are correctly classified as bots.

The local administration pipe answered correlated `hello` and `players` requests from the expected candidate process. The roster contained three modern-pool bot names. `noticeReady=true`; an announcement correctly returned `no_recipients` because no humans were present. Evidence: `build/runtime-smoke/raw-query.json`, `admin-query.json`, and `cr-smoke-match.log`.

A live one-shot handoff was also verified: a local test RCON request set `s2x_nextmap "mp_shipment_s2 dm"`, then ended the private match. The same hosted session reached Shipment Free-for-All with `party_match_sequence=2`, three refilled bots, the four-slot cap and the appropriate new score-limit fields. Evidence: `rotation-before.json`, `rotation-after.json`, `nextmap-after.txt` and `cr-smoke-rotation.log`. This exercises native handoff, not the human map-vote menu.

A live one-shot handoff was also verified: a local test RCON request set `s2x_nextmap "mp_shipment_s2 dm"`, then ended the private match. The same hosted session reached Shipment Free-for-All with `party_match_sequence=2`, three refilled bots, the four-slot cap and the appropriate new score-limit fields. A subsequent query confirmed the one-shot value was empty. Evidence: `rotation-before.json`, `rotation-after.json`, `nextmap-after.txt` and `cr-smoke-rotation.log`. This exercises native handoff, not the human map-vote menu.

The headless launch logged `failed to select match-rules gametype 'dom'` before continuing into the correct live Domination match. This is a runtime warning, not a claim of clean human-client behavior; upstream's native recipe helper depends on LUI and was retained. The live reply exposed the configured CR limit correctly, but a human HUD was not exercised.

## Runtime limits and economy boundary

No human joined the candidate. Native join/reconnect, client HUD, private announcements to humans, kicks, actual key/chat map-vote UI, remote/master discovery and full Zombies play have not been certified. Offline harnesses establish policy and API behavior, not those native/UI outcomes.

The candidate uses upstream's sole economy backend. No existing CR store was read into it, converted or overwritten. Read `UPSTREAM-ECONOMY-COMPARISON.md` before replacement: weapon acquisition, MP prestige/rank rewards, ZM bonus Orders and CR persistence need deliberate decisions. Existing CR economy harnesses are not copied as purported tests of the different upstream backend.

The package is for isolated testing. A future in-place installation must inventory/back up both game-local and AppData managed Lua files and user mods, then install matching binary/data/tools together. Existing old and new initialization folders may both execute if simply overlaid. `-appdata-dir` isolates AppData; it does not remove old game-local scripts or migrate a profile.
