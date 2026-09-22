"""Copy corrections for the live build (2026-09-22).

- Host count: "four different people" was an inference. Who runs the two 24/7
  boxes is unknown, so the page now claims only what the captures show:
  at least three named hosts.
- A false line: "Two of the nine servers aren't mine". CR's runs three.
- Bot fill: fills on map start, on servers that run it - not "every lobby"
  and not "the moment you connect".
- Staleness: the hero no longer states a live count in the present tense,
  and the version number is gone from the kicker.
- Links: the repo is private, so every github.com/ChronoRixun/S2x link is a
  404 to the public. Download and issue reporting now go to the Discord.
Every replacement must match exactly once or the script stops.
"""
import os
import sys

PAGE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "master", "index.html")
DISCORD = "https://discord.gg/yMPWMTyPPZ"

FIXES = [
    # --- meta ---
    ('Nine dedicated servers across four hosts, bots that fill every lobby, and the Headquarters progression economy running again.',
     'Community-run dedicated servers, bots that fill lobbies, and the Headquarters progression economy running again.'),
    ('<meta property="og:description" content="Call of Duty: WWII, brought back — dedicated servers, bots that fill every lobby,',
     '<meta property="og:description" content="Call of Duty: WWII, brought back — dedicated servers, bots that fill lobbies,'),
    ('<meta name="twitter:description" content="Call of Duty: WWII, brought back — dedicated servers, bots that fill every lobby,',
     '<meta name="twitter:description" content="Call of Duty: WWII, brought back — dedicated servers, bots that fill lobbies,'),

    # --- hero ---
    ('S2x-CR · Call of Duty: WWII · community client · v1.3.0</p>',
     'S2x-CR · Call of Duty: WWII · community client</p>'),
    ('<p class="stand">Nine dedicated servers, run by four different people, sitting there waiting. S2x-CR is the client that gets you onto them — and bots that fill the lobby the moment you connect, so an empty server still gives you a full match.</p>',
     '<p class="stand">When these captures were taken, nine dedicated servers from at least three different hosts were up and waiting. S2x-CR is the client that gets you onto them — and on a server running bot_fill, or in a custom match of your own, bots fill the lobby so an empty server still gives you a full game.</p>'),
    ('<a class="btn solid" href="https://github.com/ChronoRixun/S2x/releases/latest">Download the build →</a>',
     f'<a class="btn solid" href="{DISCORD}">Get the build on Discord →</a>'),

    # --- proof ---
    ('the actual state of it. Nine dedicated servers. Four independent hosts.',
     'the state of it on 21 September. Nine dedicated servers. At least three different hosts.'),
    ('<div><b>4</b><span>independent hosts</span></div>',
     '<div><b>3+</b><span>different hosts</span></div>'),
    ('nine real servers, run by four different people, answering pings from 19 to 154 ms, with bots ready to fill any lobby the second someone walks into it.',
     'nine real servers from at least three different hosts, answering pings from 19 to 154 ms, and — wherever the host runs bot_fill — bots ready to fill a lobby as soon as a map starts.'),

    # --- time series ---
    ('<b>Two hosts</b>CR’s and NamelessNoobs, plus the two unattended 24/7 boxes.',
     '<b>Two named hosts</b>CR’s and NamelessNoobs, plus the two 24/7 boxes.'),
    ('showing eight dedicated servers across four hosts,',
     'showing eight dedicated servers from at least three hosts,'),
    ('— nine servers across four hosts.">',
     '— nine servers from at least three hosts.">'),

    # --- bots ---
    ('lets a handful of people keep nine servers worth playing on',
     'lets a handful of people keep servers worth playing on'),

    # --- steps ---
    ('<h3>Extract the release into the game folder</h3><p>The one with <code>s2_mp64_ship.exe</code> in it. Drop the contents of the ZIP in beside it.</p>',
     f'<h3>Get the build and extract it</h3><p>The zip is on the <a href="{DISCORD}" style="color:var(--gold)">Discord</a>. Drop its contents into the game folder, beside <code>s2_mp64_ship.exe</code>.</p>'),
    ('<h3>Open the browser and pick a server</h3><p>Or start a Custom Match and set <code>bot_fill 17</code>. Either way you’re in a full game inside a minute.</p>',
     '<h3>Open the browser and pick a server</h3><p>Or start a Custom Match and set <code>bot_fill 17</code> — that gives you a full lobby straight away, whether or not anyone else is online.</p>'),

    # --- hosting ---
    ('<strong>Two of the nine servers up there aren’t mine</strong>',
     '<strong>Only three of the nine servers up there are mine</strong>'),

    # --- close ---
    ('<h2>Nine servers. Nobody on them yet.</h2>',
     '<h2>The servers are up. The seats are yours.</h2>'),
    ('<a class="btn solid" href="https://github.com/ChronoRixun/S2x/releases/latest">Download latest build →</a>\n'
     '      <a class="btn ghost" href="https://discord.gg/yMPWMTyPPZ">Join the Discord</a>\n'
     '      <a class="btn ghost" href="https://github.com/ChronoRixun/S2x/issues">Report an issue</a>',
     f'<a class="btn solid" href="{DISCORD}">Get the build on Discord →</a>'),
    ('If you hit one, the issue tracker is the fastest way to get it fixed.',
     'If you hit one, say so in #server-feedback on the Discord — that’s the fastest way to get it fixed.'),

    # --- footer: the repo is private, these are 404s for everyone but the owner ---
    ('        <a href="https://github.com/ChronoRixun/S2x">Repository</a>\n'
     '        <a href="https://github.com/ChronoRixun/S2x/releases/latest">Releases</a>\n'
     '        <a href="https://github.com/ChronoRixun/S2x/issues">Issues</a>\n',
     ''),
]

with open(PAGE, encoding="utf-8", newline="") as fh:
    html = fh.read()

# The page may be checked out with CRLF; match against LF and restore after.
crlf = "\r\n" in html
if crlf:
    html = html.replace("\r\n", "\n")

for old, new in FIXES:
    count = html.count(old)
    if count != 1:
        sys.exit(f"expected exactly 1 match, found {count}:\n  {old[:110]}")
    html = html.replace(old, new)

if crlf:
    html = html.replace("\n", "\r\n")

with open(PAGE, "w", encoding="utf-8", newline="") as fh:
    fh.write(html)

print(f"applied {len(FIXES)} fixes")
