# Server Manager administration (1.1.1)

Console -> ADMIN offers the current player list, announcements, warnings and kicks.
No player joins the game to perform these actions. This release does not add bans or team balancing.

Administration is local to the host and only available for a process launched by this Manager.
Each launch has a new random correlation ID. The Manager saves its PID, creation time, executable
path and port, then checks those values and the named-pipe server PID before every request.
Discovered servers and the public master list are not administration targets. Old running processes
are not adopted. Close/reopen of the Manager retains ownership of its own still-running processes.

A compatible server executable is required. Its notice helper is embedded and loaded only for Manager-enabled dedicated servers.
The Manager executable alone cannot add the bridge to an old server. The local pipe is restricted to
the Windows account running the server and rejects remote clients. It has a fixed operation list,
not an arbitrary console-command endpoint.

Warnings and kicks use expiring connection tokens, not a name or slot alone. Bots and local hosts
are protected. Each mutation asks for confirmation; outcomes are recorded in the user's
ServerManager/managed-servers/actions.jsonl. A timeout after sending means unconfirmed, not
proof that nothing happened; there is no automatic retry. A notice acknowledgement means the
server queued it to registered script listeners, not proof a person saw it.

Messages are short, single-line text (160 UTF-8 bytes). HUD notices replace the previous notice
and expire after ten seconds. Announcements and warnings are unavailable until the map's
notice helper is ready; player lists require a running match.

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

Native policy and Windows pipe tests cover ACLs, framing, pending cancellation, identity matching,
protected targets and bounds. Manager fake-pipe tests cover ownership, wrong IDs/PIDs, timeouts,
audit failures and disabled actions. A real private MP server launched through ServerController
returned two bots and zero humans, loaded the actual notice script, and refused bot/stale-target
mutations and an empty-human announcement. A private Zombies match also loaded the real helper
and returned a ready empty roster without any admin player slot.

No human was warned/kicked or received a test announcement. Positive human HUD visibility and
human kick behavior still need a private human acceptance check. Core builds pass; release is
not a claim that every client/rendering combination has been tested.
