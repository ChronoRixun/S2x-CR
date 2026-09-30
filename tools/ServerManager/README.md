# S2x Server Manager

A Windows desktop app for hosting several S2x dedicated servers out of one Call of Duty: WWII
folder. It shows one card per server, tells you what each one is doing, starts and stops them,
and edits what each one runs. C# on WPF, .NET Framework 4.8, one exe with nothing beside it.

Version 1.1.1 adds local player administration for servers this Manager started: player list,
announcements, warnings and kicks, from the console drawer's ADMIN button. It needs the 1.1.1 core
s2x.exe; see ADMINISTRATION.md. Version 1.1.0 added saved appearance themes, persistent card
ordering, and a read-only master-server browser. The theme pack (see Themes, below) replaces
1.1.0's Classic and Light themes with eight new ones and keeps High contrast. The Bodega Cervantes and U.S.S. Mount Olympus
survival maps are included in the map picker, so their registered two/three-bot launch profiles
appear. The bot mods remain separate packages.

## Theme pack (unreleased)

- Settings → Appearance is a grid of nine themes, each tile drawn in its own colours, fonts and
  texture: Field Ops, **Undead** (the default for new users), Phosphor, Outrun, Pack-a-Punch,
  Night Vision, Arcade and Prestige from design/S2x Theme Pack, plus High contrast.
- **Textures & glow** (on by default) switches the themes' textures, scanline/vignette
  overlays, glows, text shadows and card shadows on or off. Off is the same theme in flat colours,
  applied at once. High contrast is always flat.
- Status badges follow the theme: border width and style (solid, dashed, double), radius,
  tilt or skew, a tint of the status colour, and a slowly breathing dot while a server runs.
- 1.1.0 settings carry over: Classic opens as Phosphor (the dark amber one), Light as Field Ops,
  HighContrast stays. See Themes for the details.

## New in 1.1.1

- ADMIN in the console drawer opens player administration for that card's server. It works only
  for a server this Manager started with the 1.1.1 core s2x.exe; old or adopted processes say why
  they are unavailable. Every action asks first, protects bots and local hosts, targets a
  connection token rather than a slot, and is logged. No bans, no team balancing.
- Every Manager launch passes `-server-manager-admin <nonce>` and records the process identity
  under %LOCALAPPDATA%/S2x/ServerManager/managed-servers.
- Zombies launches pass `+zombiesMode 1`. Without it s2x.exe relaunches itself, so the process
  the Manager started (and wrote to the pid file) was not the one left running.
- The launch line also sets `sv_lanOnly` and `master_server_enable` from the preset's advertise
  setting, ahead of the cfg that sets them the same way.
- Launch profile entries can opt in with an `admin` argument template (ADMINISTRATION.md).

## New in 1.1.0

- Settings in the title bar offered Classic (dark amber), Light, and High contrast (dark),
  applied immediately and saved per Windows user at
  %LOCALAPPDATA%/S2x/ServerManager/settings.json. The theme pack has since replaced Classic and
  Light (see Themes).
- Card order: drag a card by its header or blank area; drop on the left or right half
  of another card to place it before or after. Buttons and text inputs do not start drags.
  Keyboard: focus a card and use Alt+Left/Right. The order is saved for the current preset
  directory, including hidden cards. Display-name/port changes retain position; new, Save As,
  or externally renamed preset files append. Ordering never rewrites the server presets.
- Master list queries the public S2x master and each advertised server for its current map,
  mode, humans, bots, capacity and ping. Search/filter the results and copy a connect command
  for the in-game console. Refresh is manual; close or Cancel ends an active query.
  No reply means unknown availability, not confirmed offline. Failed refreshes retain the
  previous results and their timestamp. This view does not start, stop or advertise servers.

Card order is stored under <game>/s2x/server-manager/card-order-<preset-folder-hash>.json.
See tests/server-manager for isolated regression tests. No live presets are used by those tests.

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
    S2xServerManager.exe --show-hidden                open with the hidden servers already shown
    S2xServerManager.exe --screenshot out.png         render the window off-screen and exit
    S2xServerManager.exe --screenshot-editor out.png [preset]
    S2xServerManager.exe --demo-editor mp out.png
    S2xServerManager.exe --screenshot-console out.png
    S2xServerManager.exe --demo-roster out.png
    S2xServerManager.exe --demo-master out.png --theme Phosphor
    S2xServerManager.exe --screenshot-settings out.png --theme Outrun --effects off
    S2xServerManager.exe --demo empty --screenshot out.png --theme HighContrast

`--demo` takes `empty`, `one`, `three-notanswering` or `three-crashed`. `--demo-editor` takes
`mp`, `zombies` or `empty` and renders the editor on a made-up server, no game folder read.
`--screenshot-editor` renders the editor for a real preset: the one named, or the only sensible
one when no name is given. `--screenshot-console` renders the editor with the console drawer
open, on a log it writes into the temp folder itself: a build machine has no server running, and
the game folder's logs are not this switch's to write. `--demo-roster` renders the roster on the
demo fleet. All of them render 1160 x 740 and exit. `--screenshot-settings` renders the
Settings → Appearance dialog on its own, at its own size, and `--demo-master` the master list. `--presets` points the preset folder
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

Rendering may override the theme with `--theme <name>` (FieldOps, Undead, Phosphor, Outrun,
PackAPunch, NightVision, Arcade, Prestige or HighContrast; the design's ids such as `packapunch`
and the old Classic and Light work too) and Textures & glow with `--effects on|off`, without
saving either. --settings-path <file> selects a separate preference file for isolated tests. The master preview
uses clearly labeled synthetic rows and sends no network requests.

## Themes

Settings in the title bar opens Appearance: one tile per theme, drawn in that theme's own colours,
display font and texture, with a ring round the one showing. Picking a tile applies it at once;
the Textures & glow switch below it does the same for effects, and the badges under PREVIEW show
the result. DONE (or Esc) closes it. Both choices are saved per Windows user in
%LOCALAPPDATA%/S2x/ServerManager/settings.json:

    {"Theme":"Undead","Effects":true}

The file is written beside itself and swapped in, so an interrupted write never leaves half a
file, and keys this version does not know are kept. A save that fails (a read-only profile, a
locked file) says so in the dialog and leaves the visible theme and switch as they were.

| Theme | Look | Fonts (design; Windows fallback until the font files are added) |
| --- | --- | --- |
| Field Ops | olive drab, stencil, crosshatch, vignette, tilted 2px badges | Black Ops One / Barlow Condensed / Special Elite; Bahnschrift Bold, Bahnschrift Condensed, Courier New |
| Undead (default) | rust and bone, red gradient fill, red and green washes, vignette | Creepster / Oswald / JetBrains Mono; Impact, Bahnschrift SemiCondensed, Cascadia Mono |
| Phosphor | amber CRT, glowing text, scanlines, vignette | VT323 throughout; Consolas |
| Outrun | synthwave grid, pink-purple fill, pill badges | Orbitron / Rajdhani / Share Tech Mono; Bahnschrift SemiBold, Bahnschrift SemiCondensed, Consolas |
| Pack-a-Punch | violet gradient, neon glow, skewed badges | Russo One / Chakra Petch / JetBrains Mono; Bahnschrift Bold, Bahnschrift, Cascadia Mono |
| Night Vision | green tube, fine scanlines, dashed badges | Share Tech Mono throughout; Consolas |
| Arcade | 8-bit checker, hard black offset shadows | Press Start 2P / Pixelify Sans / Space Mono; Lucida Console, Bahnschrift, Cascadia Mono |
| Prestige | black and gold gradient, double-ruled badges | Cinzel / Manrope / IBM Plex Mono; Palatino Linotype, Segoe UI, Cascadia Mono |
| High contrast | 1.1.0's accessibility theme: black, white, yellow, no effects | Segoe UI, Consolas |

Settings from 1.1.0 are read without being rewritten: Classic reads as Phosphor, Light as Field
Ops, HighContrast stays. A name that is not recognised, or a file that cannot be read, falls back
to Undead and says so in the dialog; nothing crashes. The file changes only when the user picks
something, so an older Manager reading it later still finds its own name until then.

How the design's tokens become WPF (Services/ThemeCatalog.cs holds them as the design writes
them, Services/ThemeResources.cs converts them):

- Colours become the existing resource keys (Panel is the window, Bar the chrome and cards, Ink
  the text, Knob the text on a filled button), live brushes recoloured in place, plus Surface,
  Fill, FillHot, FillInk and the badge tints. The in-between greys are mixed from each theme's
  own colours. `tests/server-manager/themes.ps1` checks their contrast in every theme.
- Gradient fills become LinearGradientBrush with the CSS angle mapped into the element's box
  (exact for 0/90/180/270 and square boxes; a diagonal runs corner to corner).
- Glows and shadows become DropShadowEffect: a glow has no depth, a hard shadow such as Arcade's
  `4px 4px 0 #000` keeps its direction and distance with no blur. WPF draws one shadow per
  element, so the first outer layer is used; inset and spread-only layers have no equivalent.
- Textures (stripes, grids, checker, rings, radial washes) are drawn as frozen tiled or radial
  brushes behind the cards; overlays (scanlines, vignette) are a layer over each window that never
  takes input. Vignettes are drawn at half the design's strength so the brand, first stat and
  close button in the corners stay readable.
- The only animation is the running badge's dot, at 24 frames a second.
- Letter spacing has no WPF equivalent and is not reproduced per theme; small caps labels keep
  the Manager's thin-space tracking, whose spaces are set in a proportional font so monospace
  themes do not double their width.

Fonts: the exe stays one file. The 23 font files in Assets/Fonts are built in as WPF Resources
and used automatically, at the design's scales; a family whose file is missing falls back to the
installed Windows font in the table, scaled to match. Assets/Fonts/README.md lists the files,
where they come from and where their licence texts are.

Layout follows the fonts, whose widths differ a lot (Press Start 2P, Orbitron and Black Ops One
are wide, VT323 is drawn at 1.4x). Nothing is cut without an ellipsis in any theme, at the
default 1160 x 740 or the 1000 x 620 minimum (`tests/server-manager/layout-fit.ps1` checks):
a card's button row gives every button its label's width and shares the rest by the design's
proportions, only the copy address trimming; the card grid keeps the design's 310 px minimum
card width, so a narrow window shows two columns; the stat strip, like the design's, wraps its
buttons under the stats when both do not fit; and the editor puts a label's buttons (Name
pool, Difficulty), mode and port, or the rotation's controls under their label when a wide font
leaves too little room beside it. Long names, summaries and score labels trim, with the whole
text in a tooltip.

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

## Auto-balance

Auto-balance, the switch at the foot of the editor's BOTS section, keeps a multiplayer team match
at the bot fill size: 12 is 6v6, 18 is 9v9. Bots fill the places people have not taken, join the
short side and leave as people join, and people are kept evenly split, with a countdown. On
free-for-all modes (Free-for-All, Gun Game) it only keeps the player count at the bot fill size.
The work is done by a server script, `s2x_autobalance.gsc`, which this exe carries; s2x.exe needs
no change for it. It is off unless switched on, and it is not offered for Zombies or for a launch
profile's server: Save writes it off for both. The note under the switch says what it does, and
the one thing most worth knowing about this server with it on: a player cap under 18,
free-for-all modes in the rotation, or an odd size.

With it on, the script owns every bot, so the cfg carries, in place of `set bot_fill <n>`:

    set bot_fill 0
    set s2x_autobalance 1
    set s2x_autobalance_target <n>
    set scr_teambalance 0

`bot_fill 0` sits where `bot_fill` always goes; the other three follow the editor's lines, ahead
of the advanced block, which still wins. With it off the cfg says `set bot_fill <n>` as before
and adds `set s2x_autobalance 0`. A Zombies cfg gets none of these lines.

Before a server with it on starts, the Manager makes sure
`<game>\s2x\scripts\mp\s2x_autobalance.gsc` is the copy it carries. It writes the file only when
there is none, or when the one there is a copy a Manager wrote before, known by its SHA-256 in
%LOCALAPPDATA%\S2x\ServerManager\installed-scripts.json; that is how a newer Manager replaces an
older one's copy. A copy changed by hand, or brought from somewhere else, is never written over:
the server starts with it and the toast says a custom copy of s2x_autobalance.gsc is in use. If
the file cannot be written, the server still starts, on the normal bot fill, and the toast says
auto-balance is not active. Nothing deletes the script: other servers in the same folder may use
it, and it does nothing unless `s2x_autobalance` is 1. Where it goes is one list,
`AutoBalanceTargets` in Services\ServerScriptInstaller.cs.

The PowerShell launcher ignores the `autoBalance` key and drops it when it saves a preset, which
turns auto-balance off for that server.

## Chat commands

05 · CHAT COMMANDS in the editor gives a multiplayer server chat commands. Players type them in
chat and the reply goes only to the player who asked; chat itself cannot be hidden, so everyone
sees the typed command:

    !help      the commands this server has
    !rules     the server's rules, one line each
    !discord   the server's Discord line
    !nextmap   the next map and mode (and whether the map vote may change it)
    !vote      the ballot while a map vote is open (only when the vote is on)

A player can ask once every 2 seconds. A `!` word the script does not know gets no reply, so
another script can have its own. The work is done by a second server script this exe carries,
`s2x_servercmds.gsc`, which also runs the map vote (below); chat commands need no s2x.exe change.
They are off unless switched on, and not offered for Zombies or for a launch profile's server:
Save writes them off for both, as it does auto-balance.

The Rules box takes one rule per line, up to five; the count beside it says how many there are,
and turns red with a note when there are more than five, which are not saved. The Discord box
takes the invite as you would say it (`discord.gg/yourcode`). With chat commands on, the cfg
carries, after the auto-balance lines:

    set s2x_chatcmds 1
    set s2x_rules1 "<first rule>"          one line per rule, numbered 1 to 5 with no gaps
    set s2x_discord "<Discord line>"       only when there is one

and with them off only `set s2x_chatcmds 0`. The text is written as typed apart from quotes,
control characters and a trailing backslash: it goes between the quotes of a `set` line, and a
probe of the game's command buffer found that only a double quote (which ends the value), a line
break (which ends the command) and a backslash just before the closing quote (which escapes it)
do harm there, while `;`, `//` (so `https://` links), `%`, `^` colour codes and UTF-8 arrive byte
for byte. So, when the cfg is written (Services\CfgText.cs; the preset keeps the text as typed):

- control characters are dropped, and a double quote becomes an apostrophe;
- the text is trimmed, cut to 120 bytes a rule and 64 for Discord on a character boundary (an
  accented letter takes 2 bytes, an emoji 4), and a backslash left at the end goes;
- a rule with nothing left is skipped and the rules after it move up.

Colour codes (`^1` to `^7`) work in rules and in the Discord line.

## Map vote

06 · MAP VOTE in the editor lets the players of a multiplayer server pick the next map when a
match ends. After the Victory/Defeat screen, a ballot of 2 to 5 choices (Choices, 3 by default)
opens for 10, 15, 20 or 30 seconds (Vote time, 15 by default; a preset file may hold any value
from 10 to 30). Players type `!1` to `!N` (or `!vote N`) and can change their vote; the vote
closes when the time is up, or three seconds after everyone has voted. Bots do not vote, and a
match with no people in it has no vote.

- Choice 1 is always the rotation's own next map, marked `(next)`. Choices 2 to N are other
  entries of the rotation, different maps first; the match just played is never on the ballot.
- The most votes wins. A tie, or no votes, keeps the rotation, as does a vote for choice 1.
- A winning choice 2 to N **replaces the next rotation entry**; it is not added to the rotation.
  If the entry after that is the same map and mode, that one is used up too, so a match never
  plays twice in a row and the rotation still moves one entry per match. An operator's `map`
  command outranks the vote.
- It needs at least three different map-and-mode entries in the rotation to offer a choice; the
  editor says so under the switch when there are fewer.
- It holds the end of the match for the vote: at 15 s, up to about 12 s more than the game's own
  wait before the next map.

Making the winning map load next is done by s2x.exe, not the script: it **needs an S2x build with
map-vote support**, which reads the script's pick (`s2x_nextmap`), tells it the next rotation
entry (`s2x_nextmap_preview`) and says it can (`s2x_nextmap_api 1`). On an older build the script
never opens the vote, and chat commands still work; the note under the switch says this too. The
card's rotation line ends in `vote` for a server with it on; its NEXT line is still the
rotation's order, which a vote may change.

With the vote on, the cfg carries, after the chat command lines:

    set s2x_mapvote 1
    set s2x_mapvote_choices <2-5>
    set s2x_mapvote_time <10-30>

and with it off only `set s2x_mapvote 0`. A Zombies cfg gets none of the chat command or map vote
lines. The advanced block still comes after them, so a line there wins.

### s2x_servercmds.gsc in the game folder

Before a server with chat commands or the vote on starts, the Manager makes sure
`<game>\s2x\scripts\mp\s2x_servercmds.gsc` is the copy it carries, the way it does for
auto-balance's script: written only where there is none or where the copy there is one a Manager
wrote (its SHA-256 is in the same installed-scripts.json, one entry per target), and never over
a copy changed by hand, which the server then starts with while the toast says a custom copy of
s2x_servercmds.gsc is in use. If it cannot be written, the server still starts, with chat
commands and the vote off for that launch only, and the toast says they are not active and why;
auto-balance is not affected, and the other way round. Nothing deletes it: every multiplayer
server in the folder loads it, which is why the cfg always says 0 for what is off. Where it goes
is `ServerCmdsTargets` in Services\ServerScriptInstaller.cs.

The PowerShell launcher ignores the `chatCommands` and `mapVote` keys and drops them when it saves
a preset, which turns both off for that server; a server it starts never sets their dvars, so the
script stays idle there.

## Hiding and deleting a server

HIDE on a card takes a server off the fleet: out of the counts, out of the attention line, out of
the roster and the tray menu, and skipped by Start All and Stop All. It changes one key in the
preset file, `hidden`, and nothing else — the preset keeps its name, its port and its files, and
the PowerShell launcher ignores the key. It is only offered with no process behind the card,
because hiding a server is not stopping it. `SHOW HIDDEN (n)` appears in the top bar while
something is hidden; with it on they come back dimmed, each with UNHIDE, and Start All still
skips them.

DELETE PRESET, in the editor's footer, is only there with nothing running on the port. It asks
first and names the preset. It deletes that preset file, and `server-<port>.cfg` and
`server-<port>.pid` with it when no other preset claims that port: those two belong to the port,
not to this preset. The process list is read again on the way through, because the last poll
round is up to three seconds old; a scan that cannot be read, or a server found on the port,
deletes nothing and says so.

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

## Launch profiles

A launch profile is a separately installed server package that starts its own servers: it
stages its own runtime, writes its own cfg and has its own start script. LAUNCH PROFILES... in
the title bar registers the folder it lives in. ADD... takes a folder only when its
`server-manager.json` parses; Remove only unregisters it. The list is
`<game>\s2x\launch-profiles.txt`, one folder per line, blank lines and `#` comments skipped, so
it can be edited by hand. It is read when the app starts and again when the dialog closes.

`server-manager.json` has a `profile` name, a `title` and `entries`. Each entry has a `key`, the
`map` it runs, the `mode` label the editor shows, a `short` tag for the rotation row and `start`,
the whole command line, run from the package folder; `public` is what `{public}` becomes when the
server advertises, and `log` is its log, relative to the folder. `start` can use `{gameDir}`,
`{port}`, `{name}` and `{public}`. An entry whose start script is not in the folder is not
offered, and the dialog names the missing script.

In the editor, a Zombies map that profile entries cover lists them after Zombies in the mode
picker. Adding one needs an empty rotation, and the server then runs that one map: nothing can be
added beside it, shuffle and randomize are off, the lobby shows the package's fixed party, the
advanced block is greyed, and the name cannot hold a quote or a line break. The preset keeps a
`launch` key naming the profile and the entry, which the PowerShell launcher ignores.

LAUNCH SERVER runs the usual port checks, then the entry's command, hidden, for up to 90 s, and
waits up to 10 s for a server with `-dedicated` and `net_port <port>` on that port. Its pid goes
in `server-<port>.pid`, so STOP, RESTART, Start All and the cards treat it like any other. A
profile that is not registered, or an entry whose script is missing, is said in the toast before
anything runs; a script that fails puts the error it stopped on, or its last three lines, in the
toast. The console tails the entry's `log` instead of `s2x\logs\server-<port>.log`.

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
| `hidden` | true, false | false |
| `autoBalance` | true, false (multiplayer, not a launch profile) | false |
| `chatCommands` | `enabled` true, false; `rules` up to 5 lines of text; `discord` text (multiplayer, not a launch profile) | off, no rules, "" |
| `mapVote` | `enabled` true, false; `choices` 2-5; `seconds` 10-30 (multiplayer, not a launch profile) | off, 3, 15 |
| `launch` | `profile` and `entry` of a launch profile | absent |

The file is written the way `Save-Preset` writes it under Windows PowerShell: `ConvertTo-Json`
escaping, four-space indents measured from the column the block opened at, CRLF, a closing
newline and a byte order mark. A preset written here and then saved in the launcher does not
churn beyond the keys that changed.

### What the cfg carries

`Build-ServerCfg`'s lines come first, in its order, so a preset the launcher wrote still produces
the text the launcher wrote: `sv_hostname`, `scr_<gt>_scorelimit` for each mode in the rotation,
`scr_dom_halftime 0` and `scr_dom_roundlimit 1` for single-round Domination, `bot_fill`,
`bot_names`, `sv_maprotation`. Then the editor's: `bot_DifficultyDefault`, `sv_maxplayers`,
`sv_minplayers` (S2x v1.6.1 and later; the `party_` names the cfg carried before were swallowed
by the game, so no exe ever applied them), `party_matchStartDelay`, and `master_server_enable 1` with `sv_lanOnly 0`
when the server advertises, 0 and 1 when it does not, then `s2x_autobalance` (with it on,
`bot_fill` is 0 and two more lines follow; see Auto-balance), then `s2x_chatcmds` and
`s2x_mapvote` (with them on, the rules, Discord, choices and time follow; see Chat commands and
Map vote). The advanced block is last, verbatim, so it wins. In Zombies the score limits,
`bot_fill`, `bot_names`, the auto-balance lines and the chat command and map vote lines are left
out: bots do not run there, the modes do not match, and the script is multiplayer only. A quote or a line break in
the server name is dropped before the name is quoted; colour codes stay. Shuffle on every launch
shuffles the rotation the cfg gets, not the one in the preset.

States come from two places every three seconds: the process list (WMI, `s2x.exe` with
`-dedicated` and `net_port <port>` on its command line, which is also the test before anything is
killed) and the server's own query reply on `127.0.0.1:<port>`, the same packet
`tools\server-status.ps1` sends. Running means it answered. Starting means the process is alive,
has not answered yet and is less than 90 s old. Not answering means alive but three misses in a
row. Crashed means the pid file is there and the process is not.

Start puts the auto-balance script and s2x_servercmds.gsc in place when the server uses them (see
Auto-balance, Chat commands and Map vote), writes the cfg, then runs
`s2x.exe -server-manager-admin <nonce> -noupdate -dedicated[ -zombies +zombiesMode 1]
+set net_port <port> +set sv_lanOnly <0|1> +set master_server_enable <1|0> +set g_consoleLog
s2x\logs\server-<port>.log +exec server-<port>.cfg +map_rotate`
from the game folder, then writes the pid file and the administration ownership record. Never `+map`: it runs before the dedicated party
exists and is dropped. A launch profile's server is started by its own script instead (see
Launch profiles).

## Packaging

`build\diagnostics\package-release.ps1` builds this project in Release and stages
`S2xServerManager.exe` and its `.exe.config` into the release zip's `s2x\tools`, beside
`server-launcher.ps1`, `ServerLauncher.xaml` and `server-status.ps1`; `tools\server-launcher.cmd`
goes into the same folder. Nothing else ships with the exe: no DLLs, no NuGet packages. The
server scripts in `ServerScripts` are embedded in the exe as manifest resources (the csproj names
each one's `LogicalName`) and written into the game folder only when a server needs them.

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
- Administration for the zombie-bot alpha runtimes needs their own rebuilt binaries. Team
  balancing is the Auto-balance switch, for multiplayer only; ADMIN does not move players.
