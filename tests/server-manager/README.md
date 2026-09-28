# Read-only master browser tests

Compile the harness with the production backend/model files (no package restore needed):

```powershell
New-Item -ItemType Directory -Force build/master-browser-tests | Out-Null
& 'C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/Roslyn/csc.exe' /nologo /out:build/master-browser-tests/tests.exe tools/ServerManager/Services/MasterBrowserClient.cs tools/ServerManager/Models/MasterServerRow.cs tests/server-manager/MasterBrowserTests.cs
& ./build/master-browser-tests/tests.exe
```

Tests run on ephemeral loopback UDP sockets: binary master endpoints, EOT/truncation/garbage, source-address and challenge validation, duplicate endpoint deduplication, multipart master responses, malformed info/checksum rejection, legacy `getinfo` fallback, unresponsive counts remaining unknown, cancellation.

Optional live read-only smoke: `./build/master-browser-tests/tests.exe --smoke`. This sends only master-list and info requests; it never advertises or controls a server. A network timeout is not proof a listed server is offline.

Wire behavior verified against local game sources and a bounded live probe on 2026-09-27: master.s2x.dev:20810 accepts standard four-FF OOB `getservers S2 1` without the game's checksum trailer. Game-server queries use the three-byte S2 checksum/socket trailer; responses require a valid checksum. Current Brent upstream supports both `s2x_getInfo` and `getinfo`; the latter is retained as fallback. Master replies have binary IPv4/network-order-port entries and EOT (including observed trailing NULs). Protocol is 1.

Refresh bounds: IPv4 only, max128 endpoints, eight concurrent server queries, 3.5s master/DNS wait, 1.2s per query flavor, 100ms cancellation polling. The master protocol has no challenge; its resolved source IP+port is required. Server replies require exact source IP+port and unpredictable challenge. A partial master list is labeled incomplete. Counts are nullable when absent/invalid. `clients` counts human party members only when the custom response explicitly reports `party_session=1`; custom non-party responses count bots in clients, so humans are clients minus real bots. Legacy replies without `s2x_bots` use clients minus bots. Legacy replies with `s2x_bots` omit party_session and are ambiguous: humans stay unknown unless bots is zero. Inconsistent counts also stay unknown. Legacy bots can be underreported by upstream's clamping; the browser cannot recover a count absent from the wire.

Live smoke found ten listed endpoints, nine valid info responses and one unknown/unresponsive endpoint. This is point-in-time evidence, not an uptime claim.

Run the complete isolated suite from the repository root:
    powershell -NoProfile -ExecutionPolicy Bypass -File tests/server-manager/run.ps1

This builds the application and runs the UDP backend, browser UI state, themes,
card persistence, and disconnected-WPF drag/drop behavior tests, and autobalance.ps1:
the `autoBalance` preset key, its cfg lines, the embedded script installer (fresh,
current, upgrade, custom copy refused, held-open and unwritable targets, several
targets) and the editor switch's gating. server-scripts.ps1 covers chat commands and the
map vote: the `chatCommands` and `mapVote` groups (defaults, clamps, deep copy, unknown
nested keys kept), their cfg lines (order, the explicit off lines, nothing for Zombies and
only off lines for a launch profile), the cfg text sanitiser, s2x_servercmds.gsc's installer
states and start path (a failure turns off only these two, for that launch), the script's
map and mode names against GameData, a lint of the script (no getarraykeys(, no ternary, no
variable named size, settext only in mv_text and mv_hud_result), the editor sections and
the card. Theme/card/auto-balance/server-script tests use temporary
directories; the installer never sees a real game folder or the real
installed-scripts.json. layout-fit.ps1 lays out the fleet (running, crashed and empty),
the roster, the editor (multiplayer, Zombies, and chat commands and the map vote with long
rules, a long Discord line and every note) and Settings off-screen in all nine themes,
at 1160 x 740 and at the 1000 x 620 minimum, with the embedded fonts, and fails on any text
a clip cuts without an ellipsis. It hosts a plain WPF Application with Theme.xaml rather than
the Manager's App, whose startup would otherwise run on the real game folder and settings.
WindowLeakTests.cs hosts the same plain Application and opens and closes the fleet, the
roster, the editor, Settings, the master browser (demo rows, no refresh) and the admin window
(an empty ownership store, so nothing is sent) three times each across the themes, keeping
their view models alive the way the Manager keeps its fleet. It fails when a closed window is
still reachable after garbage collection, when a window binds to an object that neither raises
PropertyChanged nor is a DependencyObject (WPF holds those through PropertyDescriptor), or
when a running server's badge breathes while it is not on screen (a collapsed pane, the window
closed to the tray, a closed window).
Add -LiveMaster for one bounded read-only live query.
No test launches or controls game servers. Native-pointer drag gestures are not
automated; the actual WPF drop handlers and visual-tree exclusions are exercised.
