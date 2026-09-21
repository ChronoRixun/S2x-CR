# Server scripting APIs (CR #9)

Use the combined S2x build and copy `data/scripts/mp/s2x_server_events.gsc` into the server's `s2x/scripts/mp/` directory. It bridges native per-player chat to a level event. Players need no files. The GSC also works on a locally hosted MP/Zombies match. The APIs are not registered for Singleplayer.

## Chat

```c
level waittill("say", player, message, team_chat);
```

`message` is the parsed `say`/`say_team` payload, limited to 256 bytes; `team_chat` is 0 or 1. The native chat command runs first, then the observation is delivered on the originating player as `s2x_chat`; the supplied GSC forwards it with the player entity. Scripts may also listen directly with `player waittill("s2x_chat", message, team_chat)`. A notification does not cancel native chat or grant command permissions. It describes a submitted chat command, not proof that every recipient saw it (native mute/routing rules still apply).

For server-voiced responses, use the supported HUD route: `player iPrintLn("Server: ...")` for the requester, or stock `iPrintLn(...)`/`iPrintLnBold(...)` for broadcasts. CR #9 explicitly allowed HUD prints; this implementation keeps that route. No guessed chat packet opcode is introduced.

`examples/scripting_api.gsc` demonstrates a persistent visit count and `!visits`. Copy it into `s2x/scripts/mp/` only if wanted. It counts a human's first spawn on each map, including map changes, rather than attempting to infer separate network sessions. Claude's existing MOTD/trivia script remains separate; it can consume the new level event for answers.

## Addresses and persistence

| API | Result |
| --- | --- |
| `getip(player)` | IPv4 address without a port; loopback is `127.0.0.1`; bots/unavailable address types return an empty string. |
| `fileread(filename)` | Text, or `undefined` when missing; an empty file returns an empty string. |
| `filewrite(filename, text)` | Atomically replaces the file and returns 1. Failure raises a script error and leaves the prior file intact. |
| `getplayerroster()` | JSON snapshot with `generated_utc`, `mapname`, and human `players` (local stable IDs and display names). No IP addresses. |

The address lookup is a global function (`getip(player)`), rather than a new method-dispatch hook. It reads the connected client's address; it does not perform geolocation or contact a service. If country display is added later, use an offline database on the server. No player addresses are sent to a third party by these features.

Files live under `%LOCALAPPDATA%\s2x\scriptdata` for the Windows account running the server. Use a plain filename of at most 80 characters: letters, digits, `.`, `_`, `-`. Directories, drive paths, alternate streams, device names and links/junctions are rejected. Text files are limited to 64 KiB and cannot contain NUL bytes. Writes use a unique temporary file and replace only after flushing. This namespace is shared by server processes under the same account; prefix names by server when counters should be separate. Concurrent read/modify/write counters across processes are not transactions.

## Optional Discord arrivals on the existing card

In the server config, enable a separate roster filename for that server:

```text
set s2x_roster_file "server-27016-roster.json"
```

The GSC writes a local snapshot every five seconds during a match. Pass it to the existing status script:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools/server-status.ps1 `
  -ServerHost 127.0.0.1 -Port 27016 `
  -RosterFile "$env:LOCALAPPDATA\s2x\scriptdata\server-27016-roster.json" -DryRun
```

For an actual card update, use the existing `-ChannelId`, `-MessageId` and token options and omit `-DryRun`. `-Install` carries `-RosterFile` and optional `-PresenceStateFile` into the scheduled task. No task is installed and no Discord publication is performed automatically by the code change.

The optional **Recent arrivals / departures** field keeps the last eight observed changes. The first successful update establishes a baseline. The state defaults to `<RosterFile>.presence.json`; use a distinct `-PresenceStateFile` for each card tracking the same roster. State advances only after the card update succeeds. Dry runs leave it untouched. Stale snapshots (over 90 seconds), another map's snapshot, offline servers and lobby periods do not generate departures. Existing unrelated embed content stays intact, and mentions are disabled.

This is periodic observation, so a short visit entirely between card polls may be missed. Clients briefly absent across a same-map restart can still appear as a leave/rejoin; it is not a lossless connection log. The roster's local IDs are used only to compare snapshots; Discord receives display names, event type and time. Run the status task as the same account as the server, or explicitly point it at that account's roster file.

## Checks

See `tests/scripting/`: native storage helper tests, production chat-routing tests, mocked PowerShell status tests, and an optional temporary GSC smoke test. The smoke test is deliberately outside `data/` so normal installation cannot enable it. Native chat from a real human, remote IP output and actual Discord delivery still require an operator check; automated tests do not claim those results.
