# Native-to-script admin notices

Production files: `src/client/component/gsc/script_extension.{hpp,cpp}` and `src/client/resources/server_admin_notice.gsc`.

`gsc::notify_admin_notice(slot,text,warning)` must be called on the server thread. It rejects inactive maps, lobby, bots, unregistered listeners and reused connections. Successful return means the synchronous script notify was queued to the registered listener, not that a remote client rendered it. Native command validation remains responsible for plain-text/UTF-8 validation; delivery additionally rejects empty, NUL-containing and over-160-byte messages, and turns a double quote into an apostrophe because the text travels inside a quoted server command.

The custom builtin `serveradminnoticeready(slot,ready)` accepts -1 for the map's connection watcher and a concrete slot for a waiting per-human listener. Native registration is bound to GUID, address, qport and connection time; map init/shutdown clear all entries and availability. Disconnect cleanup clears its entry. A late old cleanup can make a new listener unavailable, never authorize another connection. The script waits one frame before registration to avoid racing map-init reset and retries if a connected human is not yet active.

The script gates on native `s2x_server_admin_ready`, which requires dedicated owner opt-in. It shows each message to that one player with `iprintlnbold`, prefixed `SERVER NOTICE:` (green) or `SERVER WARNING:` (amber). The client's bold-print area shows it for its usual few seconds; a later message follows it rather than replacing it. It changes no health, team, weapon, economy or bot state.

## Why not a HUD element

The first version drew a ten-second `newclienthudelem` banner with `settext`. Every distinct string given to `settext` takes a slot in the engine's string table for the rest of the map, and slots are never given back. A private probe (empty `war` on `mp_shipment_s2`, no bots) set a hidden level HUD element to a new string every 50 ms: the server crashed with an access violation after about 180 distinct strings. A control run of 160 distinct strings followed by 1340 alternations between two fixed strings survived, as did 1500 distinct `iprintlnbold` messages. A real match already uses more of that table than the empty probe map, so each announcement or warning with new text would have brought the server closer to a crash. `iprintlnbold` sends the text inside the server command and uses no slot.

## Offline verification

The installed `D:/tools/gsc-tool/s2gsc-check.exe` predates `serveradminnoticeready` and cannot classify that new function name. The exact production script plus a local stub declaration was compiled in ignored `build/admin-notice-check/s2x_server_admin.gsc`. This verifies script structure and stock calls only; it does not verify the new native builtin or engine runtime. Never stage that stubbed copy. Production runtime compiles the original script with the function registered by the C++ extension.

The independent development observer `tests/server-manager/admin-notice-probe.gsc` compiles directly without a shim. It neither registers recipients nor generates messages. It logs each real incoming event with its warning flag, sequence and text. This is server-side delivery evidence, not proof of client visibility.

## Isolated engine check

1. Use the compatible native build in a private runtime with explicit owner-admin flags. The production helper is embedded as SERVER_ADMIN_NOTICE_SCRIPT and loaded only for this mode; no loose GSC is installed. No public listing. Optionally stage the passive probe separately under scripts/mp.
2. Check that a default/no-admin launch creates no administration endpoint. An active Manager-owned match should report listener capability after the embedded script initializes.
3. Connect an actual human test client. Issue a targeted notice, then a warning. Confirm the bridge returns only queued counts, the probe receives the matching warning flag, and the client shows both bold lines with the right prefix.
4. Test a 160-byte plain-text body, including multibyte UTF-8 and a double quote, then an invalid/over-limit request (rejected by owner bridge). Check MP and Zombies independently.
5. Disconnect/reconnect and switch/restart map. Old roster tokens must fail; readiness must reset and re-register. A bot-only/no-human map must report zero eligible recipients rather than a successful display.

Do not send test messages to live public players.
