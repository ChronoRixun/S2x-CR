# S2x Server Manager

A Windows desktop app for hosting several S2x dedicated servers out of one Call of Duty: WWII
folder. It shows one card per server, tells you what each one is doing, starts and stops them,
and edits what each one runs. C# on WPF, .NET Framework 4.8, one exe with nothing beside it.

Two slices are in: the fleet home and the editor. The console and packaging come later.

## Build

    dotnet build tools/ServerManager/S2xServerManager.csproj -c Release

or with Visual Studio 2022's MSBuild:

    "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" ^
        tools\ServerManager\S2xServerManager.csproj /p:Configuration=Release

The exe lands in `tools\ServerManager\bin\Release\net48\S2xServerManager.exe`. Nothing else has
to ship with it: no NuGet packages, no DLLs, only framework assemblies.

## Running it

    S2xServerManager.exe                              the fleet
    S2xServerManager.exe --view roster                open on the roster instead of the cards
    S2xServerManager.exe --presets <dir>              read and write presets somewhere else
    S2xServerManager.exe --demo three-notanswering    the mockup's demo fleet, no game folder read
    S2xServerManager.exe --screenshot out.png         render the window off-screen and exit
    S2xServerManager.exe --screenshot-editor out.png [preset]
    S2xServerManager.exe --demo-editor mp out.png

`--demo` takes `empty`, `one`, `three-notanswering` or `three-crashed`. `--demo-editor` takes
`mp`, `zombies` or `empty` and renders the editor on a made-up server, no game folder read.
`--screenshot-editor` renders the editor for a real preset, the named one or the first found.
All of them render 1160 x 740 and exit. `--presets` points the preset folder somewhere other
than the game folder, which is how the editor gets exercised without touching a real one.

The game folder is found the way the PowerShell launcher finds it: the Steam registry entry for
app 476600, this exe's own folder walking up, the folder a launcher remembered in
`%LOCALAPPDATA%\s2x\launcher-gamedir.txt`, then a folder picker. A folder counts when it holds
`s2x.exe`.

## The editor

EDIT on a card, the roster's right pane and + New server all open the same editor for that
preset; whichever way you got there it is one edit, and unsaved work survives going back to the
fleet. Ctrl+S saves. Everything applies at the next launch: the model is stop, change, start.
Nothing is sent to a server that is already running, so LAUNCH SERVER and RESTART save the
preset first and then go through the same start the card's button does.

A port belongs to one server: the second one to ask for the socket never gets it. A card whose
port another preset also claims says so, the editor's port box says so as you type, Save says so,
and Start refuses until it is fixed. + New server proposes the next free port.

## How it relates to the PowerShell launcher

`tools\server-launcher.ps1` still works and is untouched. Both read and write the same files, so
you can use either:

- `<game>\s2x\presets\*.json` — a preset is a server. Presets whose name starts with `_` are the
  launcher's own state (`_lastused`, `_lastused_mp`, `_lastused_zombies`) and stay hidden here.
- `<game>\s2x\server-<port>.cfg` — written from the preset at launch.
- `<game>\s2x\server-<port>.pid` — the pid of the process that owns the port. Stop deletes it, so
  a pid file with no process behind it means nobody stopped that server.

A server is a preset plus its port. Two presets can name the same port; they then show the same
state, because the state belongs to the port.

### What the editor adds to a preset file

The launcher's keys keep their spellings and their values. The editor's settings are new keys in
the same file, and any key neither app knows is read into `ServerPreset.Raw` and written back
untouched:

| key | values | default |
| --- | --- | --- |
| `botDifficulty` | recruit, regular, hardened, veteran | regular |
| `maxPlayers` | 1-18 in multiplayer, 1-4 in Zombies | 18 / 4 |
| `minPlayers` | 1 to `maxPlayers` | 1 |
| `startDelay` | 0-120 seconds | 60 |
| `advertise` | true, false | true |
| `extraLines` | the advanced block, one dvar per entry | empty |
| `shuffleOnLaunch` | true, false | false |

The file is written the way `Save-Preset` writes it under Windows PowerShell: `ConvertTo-Json`
escaping, four-space indents measured from the column the block opened at, CRLF, a closing
newline and a byte order mark. A preset written here and then saved in the launcher does not
churn beyond the keys that changed.

### What the cfg carries

`Build-ServerCfg`'s lines come first, in its order, so a preset the launcher wrote still produces
the text the launcher wrote: `sv_hostname`, `scr_<gt>_scorelimit` for each mode in the rotation,
`scr_dom_halftime 0` and `scr_dom_roundlimit 1` for single-round Domination, `bot_fill`,
`bot_names`, `sv_maprotation`. Then the editor's: `bot_DifficultyDefault`, `party_maxplayers`,
`party_minplayers`, `party_matchStartDelay`, and `master_server_enable 1` with `sv_lanOnly 0`
when the server advertises, 0 and 1 when it does not. The advanced block is last, verbatim, so it
wins. In Zombies the score limits, `bot_fill` and `bot_names` are left out: bots do not run there
and the modes do not match. A quote or a line break in the server name is dropped before the name
is quoted; colour codes stay. Shuffle on every launch shuffles the rotation the cfg gets, not the
one in the preset.

States come from two places every three seconds: the process list (WMI, `s2x.exe` with
`-dedicated` and `net_port <port>` on its command line, which is also the test before anything is
killed) and the server's own query reply on `127.0.0.1:<port>`, the same packet
`tools\server-status.ps1` sends. Running means it answered. Starting means the process is alive,
has not answered yet and is less than 90 s old. Not answering means alive but three misses in a
row. Crashed means the pid file is there and the process is not.

Start writes the cfg, then runs
`s2x.exe -noupdate -dedicated[ -zombies] +set net_port <port> +exec server-<port>.cfg +map_rotate`
from the game folder, then writes the pid file. Never `+map`: it runs before the dedicated party
exists and is dropped.

## What is not here yet

- Slice 3, the console: the per-server log tail, as a drawer on the cards and a panel under the
  editor. CONSOLE is disabled.
- Slice 4, packaging: shipping the exe in the release zip beside the launcher, and the tray icon.
