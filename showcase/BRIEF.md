# Brief: a showcase site for the S2x-CR fork

You are one of three designers competing on the same brief. Another model and I are
building our own candidates from this identical brief. The client will line all three up
side by side, then a synthesis will be built from whatever each of us got right. Design
like you intend to win it.

---

## 1. What the thing is

**S2x** is a community custom client for **Call of Duty: WWII** (2017, Sledgehammer
Games). The retail multiplayer is effectively dead — matchmaking is empty, the
progression servers are gone. S2x brings the game back: dedicated servers you can run
yourself, bots that fill empty lobbies, and a local re-implementation of the
Headquarters progression economy so ranking up, supply drops and orders all work again.

**S2x-CR** is ChronoRixun's fork of it. Current version **v1.3.0**.

The person who owns this — handle **ChronoRixun**, "Chrono" in game — does not want to
run a company or a launcher business. He wants **good matches against real people**. The
entire purpose of this site is to get more people running the client and populating the
servers. Every design decision should be measured against that one goal: does this make a
lapsed CoD: WWII player download the build and join a server tonight?

## 2. Who lands on this page

Three audiences, in order of how much they matter:

1. **A lapsed CoD: WWII player.** Probably found the link in a Discord. Owns the game on
   Steam, hasn't launched it in years, assumes it's dead. Skeptical — they've seen
   "revival" projects that were a Discord server and a promise. They need *proof it
   works* faster than they need a feature list.
2. **Someone who'd host a server.** More technical, wants to know what running one
   involves and whether it's a pain.
3. **A curious developer.** Wants to know what was actually changed and whether the
   project is real.

Audience 1 is the one to design for. The other two need a clear path, not the front page.

## 3. What you have to work with

### Screenshots — `D:\S2x-showcase\showcase\shots\`

**103 real in-game captures.** Every one is genuine; nothing is a mockup. Filenames are
Steam timestamps (`YYYYMMDDHHMMSS_1.jpg`). **Open them and look.** Curating these is a
large part of the job — which twenty you pick and what you say about them matters more
than any amount of styling.

Contact sheets of all 103, twelve per sheet in filename order, are at
`D:\S2x-showcase\showcase\sheets\sheet01.jpg` … `sheet09.jpg` if you want a fast overview
first. `D:\S2x-showcase\showcase\shotlist.txt` maps sheet position to filename (line N =
image N).

Broadly what's in there:
- Headquarters economy UI — Quartermaster, Contracts, Orders, Above and Beyond,
  Headquarters Post / payroll, CWL team packs
- Supply drop openings — card flips, rarity glows, Legendary pulls, duplicates
  converting, crates called in on foot
- Multiplayer scoreboards showing all three bot-name pools
- Live multiplayer gameplay — Pointe du Hoc, Shipment, jeeps, gunfights
- **The in-game server browser** listing real community servers (see below)
- Nazi Zombies — Groesten Haus, The Final Reich, The Shadowed Throne, USS Texas,
  Thulian Archives — wave completions, rank promotions, after-action reports,
  Zombies orders/contracts, Zombies consumables, Zombies supply drops
- The Headquarters social space with other players in it

### The two existing attempts

Both are real pages built for this same project. Read both. Take what works, and do not
repeat what doesn't.

**Attempt A — `D:\S2x\showcase-site\dist\index.html`** (with `styles.css`, `app.js`).
A marketing landing page. Hero with "KEEP THE FIGHT ALIVE", three numbered sections,
CTAs to the GitHub release / Discord / issues, a six-image lightbox gallery, footer.

- *Works:* someone landing cold knows what it is in five seconds and where to click.
  Mobile layout holds up. Real alt text, skip link, visible focus rings. Genuinely
  handsome — restrained gold-on-charcoal, good typographic rhythm.
- *Fails:* the evidence is thin. Six screenshots where the other page has twenty-five.
  The specifics that make this project believable got flattened into taglines. It asserts
  rather than shows. No Open Graph tags at all, which matters because the link gets
  pasted into Discord. Full-resolution JPEGs used as grid thumbnails — 11 MB over a full
  scroll.

**Attempt B — `D:\S2x-showcase\docs\index.html`** (live at
https://chronorixun.github.io/S2x/). A single self-contained file, styles inline.
A documentation dossier: headline, one paragraph, hero image, then section after section
of feature + screenshot grid + caption.

- *Works:* it is *convincing*. Every claim is followed immediately by a capture proving
  it. The captions are specific and unhedged — "4v3 with bots, full scoreboard and
  progression", "9v9 War mode with bot teammates", "Five level-ups in a single match,
  five drops credited". The copy (reproduced in §4 below) is the best asset this project
  has and you should treat it as source material, not as something to rewrite into
  marketing voice.
- *Fails:* **no `<!doctype html>`** — the file starts at `<title>`, so browsers render it
  in quirks mode. **No viewport meta** — phones lay it out at 980px and scale down until
  the body text is unreadable, and most people arrive from a Discord link on a phone.
  No description meta, no OG tags. No download link, no Discord link, no call to action
  anywhere until the very bottom. It documents; it never asks for anything.

### The honest read

A is a front door with nothing behind it. B is a warehouse of evidence with no front
door. Neither is the answer on its own, and "A's hero bolted onto B's body" is the
obvious move — so if that's all you do, you won't win this. Find the thing neither of
them saw.

## 4. The facts — source of truth

This copy is from Attempt B and is accurate. Do not invent features, do not inflate
numbers, do not describe anything not listed here. If you want to say something you can't
source from this section or see in a screenshot, don't.

**Bot fill.** Automatic bot spawning on every map start. Set `bot_fill 17` and every match
fills with bots across all gametypes — TDM, Domination, Free-for-All, Hardpoint, and even
War mode with full 9v9 teams.

**Bot names.** Three name pools replace the engine's stock bot names. `Default` keeps the
originals (military codenames — Domino, Causeway, Overlord, Juno). `Modern` uses
2016–2026 gamertag styles (BlindSprint, Reaver22, bleakwinter, SleepyDave). `Nostalgia`
brings back the Xbox 360 era (xXDarkAngelXx, NoSc0pe360, CamperKing2010). 54 names per
pool, shuffled every map — a different lobby every game.

**Bots in Custom Matches.** Offline Local Play used to accept bots and never show them.
The fork routes `bot_fill` and `spawnBot` through the game's own spawn script on listen
servers, so bots pick teams and classes and actually walk onto the map. Requests are
capped at the free slots (17 bots plus the host).

**Headquarters economy.** Orders, contracts, payroll, supply drops, the Quartermaster and
Mail — all running locally over the Achievement Engine protocol. Earn rewards, complete
challenges, and open supply drops just like the retail game.

**Supply drops.** Open with the full card-flip animation. Common, Rare, Epic and Legendary
items — calling cards, weapon charms, emotes, pistol grips, uniforms — with duplicates
converting to Armory Credits. Or call one in on foot: walk to the marker in Headquarters,
the crate comes down, and the cards come out of it.

**Level-up supply drops.** Every soldier level-up now pays out the Rare Supply Drop the
end-of-match screen promises. The game always reported the level-up; the fork's economy
finally credits it, on dedicated servers and in Local Play alike.

**Server launcher.** A WPF launcher with a mode toggle for Multiplayer and Zombies. Pick
maps, gametypes, score limits and bot settings, name the server in colour with the swatch
row (`^1` red, `^3` yellow, `^7` white) and watch the preview, then hit Launch. It finds
the game folder on its own, so a spare box with the game files copied over and no Steam
installed becomes a dedicated server in one click.

**A dedicated server that starts every time.** Roughly one launch in ten used to die a few
seconds in. The cause was a repair guard in the game's anti-tamper layer that quietly
restored three of the client's own patches, after which an integrity check failed and the
process was shut down. The guard is now filtered like the rest: forty cold starts in a row
reached a full match with bots, against nine of ten before.

**Zombies economy.** The HQ economy extends into Nazi Zombies with its own orders,
contracts and supply drops. Kill zombies, survive waves, earn headshots — progress tracked
in real time with Armory Credit and Rare Zombie Supply Drop rewards.

**Nazi Zombies.** Groesten Haus and The Final Reich playable with full progression. Wall
buys, mystery box, consumables, wave tracking. Screenshots also show The Shadowed Throne,
USS Texas, Thulian Archives, Artillery Bunker and Overlook, rank promotions mid-wave, and
after-action reports.

**Zombies supply drops.** Open with the skull-backed card flip. Consumables like
Self-Revives, weapon guarantees (M1903 Guarantee forces the Mystery Box's next weapon)
and Double Jolts drop alongside uniforms and calling cards.

### The server browser — new, and not on either existing site

Two captures (`20260921171022_1.jpg`, `20260921175918_1.jpg`) show the in-game server
browser listing **nine dedicated servers across four different hosts**:

| Host | Server | Map | Type |
|---|---|---|---|
| CR's | Dom/HP/TDM/KC 9v9 on Shipment | Shipment 1944 | Hardpoint |
| CR's | Small Map Moshpit Mayhem | Shipment 1944 | Kill Confirmed |
| CR's | Small Map Moshpit (DLC) | London Docks | Hardpoint |
| Glooples | Prop Hunt — DLC Only | Anthropoid | Prop Hunt |
| Glooples | Prop Hunt — Standard Maps Only | Aachen | Prop Hunt |
| Glooples | OBJ Server — CTF & DOM | Chancellery | Capture the Flag |
| NamelessNoobs | Stock Maps | USS Texas | Team Deathmatch |
| — | SHIPMENT 24/7 | Shipment 1944 | Free-for-all |
| — | WAR 24/7 ALL MAPS | Operation Husky | War |

Pings 19–154. Host names render in colour, which is the launcher's `^1`/`^3` codes
working end to end. **Prop Hunt is running on this engine**, which is a custom gametype
nobody expects to see in WWII.

This is the strongest single asset on the page: it is independent evidence that other
people already host on this fork. Use it well.

**One hard constraint on honesty:** every server in those captures reads **0/18 players**.
They are empty at the moment of capture. You may say the servers exist, that multiple
people host them, that they are joinable. You may **not** imply they are busy, populated,
or that there is a thriving playerbase right now. Do not crop the player count out to
dodge this. The client would rather lose a signup than overstate — he has been burned
before by claiming more than he could show, and a skeptical audience will check.

### Links

- Latest build: `https://github.com/ChronoRixun/S2x/releases/latest`
- Repository: `https://github.com/ChronoRixun/S2x`
- Issues: `https://github.com/ChronoRixun/S2x/issues`
- Discord: `https://discord.gg/yMPWMTyPPZ`
- Upstream project this forks: `https://github.com/Brentdevent/S2x` — credit it in the footer.
- Requires a legitimate Steam copy of Call of Duty: WWII. Say so near the download.
- Not affiliated with Activision, Sledgehammer Games, Microsoft or xLabs. Call of Duty is
  a trademark of its owner. Footer disclaimer.

## 5. Constraints

- **One self-contained HTML file.** CSS in a `<style>` block, JS in a `<script>` block.
  It gets served from GitHub Pages with no build step.
- Reference images as `../shots/<filename>.jpg` — the originals, by their Steam filename.
- Google Fonts via `<link>` is allowed. No other external resources, no frameworks, no CDN
  scripts.
- Must start with `<!doctype html>` and carry `<meta name="viewport">`. Non-negotiable —
  both prior attempts got this wrong in one way or another.
- Include `<meta name="description">` and full Open Graph + Twitter card tags. The link
  gets pasted into Discord constantly and currently renders as a bare URL.
- Dark by default. This is a WWII shooter; the screenshots are dark and gold and the page
  should sit with them rather than fight them.
- Must work at 375px wide with no horizontal scroll. Most arrivals are on a phone.
- `loading="lazy"` and explicit `width`/`height` on every image below the fold.
- Real `alt` text on every image. Keyboard-operable. Visible focus states.
- Assume the reader has *no idea* what S2x is and has never heard of a custom client.

## 6. What to deliver

Write your page to **`D:\S2x-showcase\showcase\candidates\<yourname>.html`** — you will be
told your filename. Then reply in chat with:

1. **Your concept in three sentences.** What's the organising idea, and what did you see
   in the brief that you think the other two will miss?
2. **Your structure** — the sections in order, one line each.
3. **What you cut and why.** Anything you deliberately left out.
4. **The one risk in your design** — where it could fall down.

Do not reply with the HTML in chat; write it to the file. Take the time to look at the
screenshots properly before you start — the curation is the job.
