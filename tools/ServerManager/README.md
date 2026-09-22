# S2x Server Manager

A Windows desktop app for hosting several S2x dedicated servers out of one Call of Duty: WWII
folder. It shows one card per server, tells you what each one is doing, starts and stops them,
and edits what each one runs. C# on WPF, .NET Framework 4.8, one exe with nothing beside it.

Three slices are in: the fleet home, the editor and the console. Packaging comes last.

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
    S2xServerManager.exe --screenshot-console out.png
    S2xServerManager.exe --demo-roster out.png

`--demo` takes `empty`, `one`, `three-notanswering` or `three-crashed`. `--demo-editor` takes
`mp`, `zombies` or `empty` and renders the editor on a made-up server, no game folder read.
`--screenshot-editor` renders the editor for a real preset: the one named, or the only sensible
one when no name is given. `--screenshot-console` renders the editor with the console drawer
open, on a log it writes into the temp folder itself: a build machine has no server running, and
the game folder's logs are not this switch's to write. `--demo-roster` renders the roster on the
demo fleet. All of them render 1160 x 740 and exit. `--presets` points the preset folder
somewhere other than the game folder, which is how the editor gets exercised without touching a
real one.

Keys: Ctrl+S saves the editor on screen, Esc closes the console, Ctrl+L goes to the console's
filter box and opens it first if it is shut, F5 forces a poll round instead of waiting for the
next three seconds.

A render switch that cannot be honoured writes the reason to standard error and exits without
showing a window: 2 for a missing or unreadable switch argument, 3 when the editor it was asked
for is not there. It never writes a different screen under the name it was given.

The game folder is found the way the PowerShell launcher finds it: the Steam registry entry for
app 476600, this exe's own folder walking up, the folder a launcher remembered in
`%LOCALAPPDATA%\s2x\launcher-gamedir.txt`, then a folder picker. A folder counts when it holds
`s2x.exe`.

## The editor

EDIT on a card, the roster's right pane and + New server all open the same editor for that
preset file; whichever way you got there it is one edit, and unsaved work survives going back to
the fleet. Ctrl+S saves whichever editor is on screen. Everything applies at the next launch: the
model is stop, change, start. Nothing is sent to a server that is already running, so LAUNCH
SERVER and RESTART save the preset first and then go through the same start the card's button
does. A new server has no card to go back to, so leaving one asks whether to save or drop it.

Saving writes a candidate copy and only then hands it to the fleet, so a write that fails leaves
the card on the configuration its file still holds. The file is written beside itself and swapped
in, so an interrupted write cannot leave half a preset. SAVE AS... refuses a name another preset
already has: writing over it would lose it without asking.

A port belongs to one server: the second one to ask for the socket never gets it. A card whose
port another preset also claims says so, the editor's port box says so as you type, Save says so,
and every launch path refuses until it is fixed — Start, Start All, Restart and the editor's
LAUNCH SERVER. Start also refuses when the process list cannot be read at all, because a scan
that failed looks exactly like a free port. While a server is running, its port is the one its
process is on: STOP and RESTART act on that port, and the box refuses to move until the server is
stopped. + New server proposes the next free port.

If another launcher writes a preset while it is open here, an editor with nothing unsaved in it
takes the new reading and one with a draft in it keeps the draft and says FILE CHANGED ON DISK.

## The console

CONSOLE on a card, on the roster's strip or in the editor's footer opens one drawer on that
server's log: over the cards on the fleet home, and under the editor above its footer on the
editor screen and in the roster. It is the same drawer and the same server wherever you opened
it from, and in the roster it follows the row you pick. Esc closes it. Drag its top edge to
resize it; it keeps its height and whether it was open until the app is closed.

It holds the last 500 lines, follows the end of the file unless FOLLOW is turned off, PAUSE
holds what is on screen and counts what arrived behind it, the filter box keeps the lines that
contain what you type, and COPY puts the lines on screen on the clipboard. The file is read
once a second, only the part that is new, shared with the server that is writing it and never
opened for writing: nothing here deletes or shortens a log. A line caught halfway through a
write waits for the round that finishes it. When the file gets shorter than it was, the server
started again over the top of it, and the drawer says so and carries on.

### Which log

Every server this app starts is launched with `+set g_consoleLog s2x\logs\server-<port>.log`,
so it writes a log of its own. The fork's default is `s2x\logs\console.log`, which every server
on the box appends to at once, so a line in it does not say which server wrote it.

`g_consoleLog` has not been confirmed on a live server yet, so the drawer does not depend on it.
If the per-port file has not appeared 30 seconds after the server started, the drawer reads the
shared `console.log` instead and says `shared log; per-server log did not appear` beside the
path in its header. A server started by the PowerShell launcher, or one that was already running
when this app opened, has no per-port log either, and falls back the same way.

## The tray

The app keeps an icon in the notification area: the tooltip is how many servers are up out of
how many, and the menu lists them with what each is doing. Clicking one opens the window on the
roster with that server picked. Start All and Stop All are on the menu too, and Show brings the
window back.

The window's close button hides to the tray instead of quitting; `Close to tray` on the same
menu turns that off. Exit really quits, and it says on itself what is true either way: the
servers keep running. Nothing here stops a server except Stop All.

Windows puts a tray icon it has not seen before behind the chevron in the notification area. If
the window seems to have gone, look there first, and drag the icon out onto the taskbar.

## Connect lines

The copy button on a card and on the roster's strip copies `connect 127.0.0.1:<port>`, which is
what a player sitting at this machine pastes. Right-click it for `connect <this machine's
IPv4>:<port>`, which is what the rest of the house pastes. Neither is the public address:
nothing here knows it, because the master heartbeat does not hand it back. A host behind a
router swaps in their own public IP, and forwards the UDP port to this machine.

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
untouched, on the preset and on each line of its rotation:

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
`s2x.exe -noupdate -dedicated[ -zombies] +set net_port <port> +set g_consoleLog
s2x\logs\server-<port>.log +exec server-<port>.cfg +map_rotate`
from the game folder, then writes the pid file. Never `+map`: it runs before the dedicated party
exists and is dropped.

## Packaging

`build\diagnostics\package-release.ps1` builds this project in Release and stages
`S2xServerManager.exe` and its `.exe.config` into the release zip's `s2x\tools`, beside
`server-launcher.ps1`, `ServerLauncher.xaml` and `server-status.ps1`; `tools\server-launcher.cmd`
goes into the same folder. Nothing else ships with the exe: no DLLs, no NuGet packages.

`tools\server-launcher.cmd` is the double-click way into the old launcher: `@echo off` and a
hidden, no-profile PowerShell host running `server-launcher.ps1`, so opening it never leaves a
console window behind. `server-launcher.ps1` itself is untouched.

The version in the title bar, next to SERVER MANAGER, is this project's `<Version>` (and
`<FileVersion>` alongside it for the exe's own file properties) — bump both here for a release.
`<ApplicationIcon>` points at `Assets\S2xServerManager.ico`, drawn by `Assets\make-icon.ps1`; the
tray icon is the same one, read back off the running exe rather than composed again at runtime.

## What is not here yet

- The console shows the lines as the fork writes them, which carry no timestamps, so there is no
  time column. The mockup has one; the file has nothing to put in it.
