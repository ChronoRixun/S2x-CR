# S2x Server Manager

A Windows desktop app for hosting several S2x dedicated servers out of one Call of Duty: WWII
folder. It shows one card per server, tells you what each one is doing, and starts and stops
them. C# on WPF, .NET Framework 4.8, one exe with nothing beside it.

This slice is the fleet home only. The editor, the console and packaging come later.

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
    S2xServerManager.exe --demo three-notanswering    the mockup's demo fleet, no game folder read
    S2xServerManager.exe --screenshot out.png         render the window off-screen and exit

`--demo` takes `empty`, `one`, `three-notanswering` or `three-crashed`. `--screenshot` renders
1160 x 740 with whatever data the other switches select, and can be combined with them.

The game folder is found the way the PowerShell launcher finds it: the Steam registry entry for
app 476600, this exe's own folder walking up, the folder a launcher remembered in
`%LOCALAPPDATA%\s2x\launcher-gamedir.txt`, then a folder picker. A folder counts when it holds
`s2x.exe`.

## How it relates to the PowerShell launcher

`tools\server-launcher.ps1` still works and is untouched. Both read and write the same files, so
you can use either:

- `<game>\s2x\presets\*.json` — a preset is a server. Presets whose name starts with `_` are the
  launcher's own state (`_lastused`, `_lastused_mp`, `_lastused_zombies`) and stay hidden here.
- `<game>\s2x\server-<port>.cfg` — written from the preset at launch, the same lines in the same
  order as the launcher's `Build-ServerCfg`.
- `<game>\s2x\server-<port>.pid` — the pid of the process that owns the port. Stop deletes it, so
  a pid file with no process behind it means nobody stopped that server.

A server is a preset plus its port. Two presets can name the same port; they then show the same
state, because the state belongs to the port.

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

- Slice 2, the editor: name with colour swatches, the rotation builder, match rules, bots, lobby,
  visibility, the advanced block. Until then EDIT and + New server are disabled and a preset is
  changed in the PowerShell launcher. This app only reads preset files; it never writes one.
- Slice 3, the console: the per-server log tail, as a drawer on the cards and a panel in the
  roster. CONSOLE is disabled.
- Slice 4, packaging: shipping the exe in the release zip beside the launcher, and the tray icon.

The preset format has no player cap key, so the cap falls back to 18 for multiplayer and 4 for
Zombies; a running server reports its real `sv_maxclients`. The editor slice should add the key.
Preset files are read into `ServerPreset.Raw` key by key, so that slice can write one back without
dropping anything a newer launcher put in it.
