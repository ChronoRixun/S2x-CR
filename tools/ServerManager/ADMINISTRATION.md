# Server Manager administration (1.1.1)

Console -> ADMIN offers the current player list, announcements, warnings and kicks.
No player joins the game to perform these actions. This release does not add bans or team balancing.

## What you need

- Server Manager 1.1.1.
- The 1.1.1 core `s2x.exe` in the game folder. The Manager alone cannot add administration to an
  older server: an older `s2x.exe` ignores the launch flag, and ADMIN says no bridge answered.
- The server started (or restarted) from this Manager after both are in place. A server that was
  already running, or was started by the PowerShell launcher or anything else, is never adopted.

The notice helper script is built into the 1.1.1 `s2x.exe` and loaded only for Manager-started
dedicated servers. There is no GSC file to copy into the game folder.

## Which servers can be administered

Administration is local to the host and only available for a process launched by this Manager.
Each launch has a new random correlation ID. The Manager saves its PID, creation time, executable
path and port, then checks those values and the named-pipe server PID before every request.
Discovered servers and the public master list are not administration targets. Closing and
reopening the Manager keeps ownership of its own still-running processes. The records live in
%LOCALAPPDATA%\S2x\ServerManager\managed-servers, one file per port, for the current Windows user.

The local pipe is restricted to the Windows account running the server and rejects remote clients.
It has a fixed operation list, not an arbitrary console-command endpoint.

## Actions and outcomes

Warnings and kicks use expiring connection tokens, not a name or slot alone. A token is valid for
30 seconds after REFRESH PLAYERS and only for the same connection on the same map; if an action
says the connection changed or the roster expired, refresh and select the player again. Bots and
local hosts are protected. Each action asks for confirmation. A kick disconnects the player with
the standard "kicked" reason and does not ban them.

Outcomes are appended to actions.jsonl in the same folder as the ownership records. A timeout
after sending means unconfirmed, not proof that nothing happened; there is no automatic retry.
A notice acknowledgement means the server queued it to the player's notice listener, not proof
a person saw it.

## Messages

Messages are single-line text of up to 160 UTF-8 bytes. Each player sees it as the game's bold
on-screen message, prefixed SERVER NOTICE (green) or SERVER WARNING (amber), for the game's usual
few seconds. A second message shows after the first instead of replacing it.
A double quote in the text is shown as an apostrophe. Announcements and warnings are unavailable
until the map's notice helper is ready; player lists need a running match.

Notices are not drawn as a HUD element on purpose. The game keeps every distinct HUD text for the
rest of the map in a small table, and a server that runs out of room crashes. Bold messages carry
their text directly and use no room in that table, so any number of notices is safe.

## Launch profiles

Profiles are opt-in. A compatible entry may include:
    "admin": "-ManagerAdmin {admin}"

Its launcher must accept that nonce and pass "-server-manager-admin <nonce>" to its dedicated
server, before +commands. The Manager records the resulting process only if its command line
contains the matching launch nonce. Legacy profiles launch normally with administration unavailable.

The existing Zombies alpha packages need separately rebuilt compatible server binaries and updated
wrappers/manifests. Those updates are deferred. Do not put the standard core bridge executable into
a bot-alpha runtime: it does not contain the experimental bot native component.

## Validation

Native policy and Windows pipe tests (tests/server-admin) cover ACLs, framing, pending
cancellation, reuse of the one pipe instance, identity matching, protected targets and bounds.
Manager fake-pipe tests (tests/server-manager/admin.ps1) cover ownership, wrong IDs/PIDs, a server
without a bridge, timeouts, audit failures and disabled actions. Private LAN servers launched
through the real ServerController cover the MP and Zombies rosters, the embedded helper reaching
ready, and refusal of bot/stale-target actions and of announcements with no human listeners.

Delivery to a real human client, and a real human kick, need a private human acceptance check;
see tests/server-manager/ADMIN-NOTICES.md. Never test on live public players.
