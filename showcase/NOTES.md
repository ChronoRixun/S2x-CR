# The showcase bake-off — what happened

Branch `feat/showcase-site`, worktree `D:\S2x-showcase`. Nothing here touches `integration`.

**Start here:** `http://127.0.0.1:4181/showcase/lineup.html` — the lineup page, with all five
versions side by side and what each contributed.

```bash
cd D:\S2x-showcase && python -m http.server 4181 --bind 127.0.0.1
```

| | URL |
|---|---|
| **The lineup** | `/showcase/lineup.html` |
| **The build** | `/showcase/master/index.html` |
| Candidate — Kimi K3 | `/showcase/candidates/kimi.html` |
| Candidate — MiniMax M3 | `/showcase/candidates/minimax.html` |
| Candidate — Claude | `/showcase/candidates/claude.html` |
| Prior — GPT's landing page | `/showcase/reference/gpt/index.html` |
| Prior — the live dossier | `/docs/index.html` |

---

## How it was run

All three designers got the same brief (`BRIEF.md`) — audience, goal, the factual copy they
had to stay inside, the constraints, and one hard honesty rule. None of them saw each
other's work.

The 103 screenshots from the Steam zip were extracted to `showcase/shots/` and catalogued in
`SHOTS.md` — index, filename, exact pixel size and a description of each. I built labelled
contact sheets (`showcase/sheets/`) to do that; `mksheets.sh` regenerates them.

## The result nobody expected

**All three independently arrived at the same thesis:** that the server browser reading
**0/18 on every row** should be the centrepiece of the page rather than something to bury.

Three different models, no contact, same conclusion. That is about as strong a signal as
this kind of exercise produces, and it's why the final build leads with it.

Then they diverged in a way that turned out to be complementary:

- **MiniMax wrote the headline.** *"The servers are empty. That's the whole problem S2x
  solves."* Nine words that name the weakness and resolve it in the same breath.
- **MiniMax also attacked its own idea** — the only one of the three to do so. It pointed
  out that a skeptic reading "0/18" in the first second on a phone may bounce before the
  bots explanation lands, and that the bridge has to close "within roughly the same
  glance". That critique changed the final build: the objection is answered *in the
  headline*, not three screens down.
- **Kimi wrote the best body copy**, and found the stat row — nine servers, four hosts,
  19–154 ms ping, and *"0/18 players — the part that's on you."*
- **My contribution was structural:** transcribing the server browser into a real HTML
  table beside the screenshot. The most important evidence on the site was trapped in a
  JPEG that is illegible at 375px, which is where most readers are. As markup it is
  readable, selectable and indexable, and it keeps the colour-coded host names that prove
  the launcher's `^1`/`^3`/`^7` codes survive end to end.

MiniMax's own candidate demonstrates MiniMax's own risk: at 375px its server browser
screenshot is completely unreadable. The final build has both its framing and the fix.

## Fixing what the priors got wrong

Both existing pages had real faults, and the build closes all of them:

- The live dossier has **no `<!doctype html>`** — it renders in quirks mode — and **no
  viewport meta**, so phones lay it out at 980px and shrink it until the body text is
  unreadable. Most arrivals are on a phone, from Discord.
- Neither prior has **any Open Graph or Twitter tags**, so the link renders as a bare URL
  in Discord. The build has full cards with a proper preview image.
- The landing page used full-resolution JPEGs as grid thumbnails — 11 MB over a scroll.

Every image in the build is lazy-loaded below the fold with intrinsic dimensions, has real
alt text, and the page is keyboard operable with visible focus states.

## Honesty audit

The 0/18 rule was enforced against everything, including the models' own output. Three
things were caught and kept out of the final build:

- **MiniMax invented a download size** ("~40 MB") and **guessed the host name colours**.
  Neither is verified, so neither is on the page. The colours in the build are the ones
  actually visible in the capture.
- **Kimi wrote that the Mail tab "has things in it".** The screenshot says *"No Mail Right
  Now, Soldier."* The build says Mail is wired up and waiting, which is what the capture
  supports.
- Both briefly implied `bot_fill` covers gametypes beyond the ones it's documented for.
  The build lists only TDM, Domination, Free-for-All, Hardpoint and War.

The server browser screenshot is used uncropped, player counts and all, on every page.

## Working with Hermes — for next time

The gateway handles short turns fine and **wedges on long ones**, and the MCP call caps at
600 s. Two things worth knowing:

1. **A timeout does not mean failure.** The work usually completes minutes later; only the
   reply is lost. Ojamd is the same machine, so check the file rather than trusting the
   transport. Kimi's page arrived this way, long after the call that asked for it errored.
2. **Do not ask it to open images** — reading JPEGs reliably hung the tool loop. Handing it
   a written catalogue instead (`SHOTS.md`) worked immediately.

MiniMax sits on the remote Mac gateway (`hermes-mac`, session model lock confirmed) and
truncates above roughly 500 words, so it works best in small copy-sized chunks.

## Not done

- The build has not been promoted to `docs/`. The live site is untouched — that's your
  call, and it's a one-line copy when you want it.
- `showcase/shots/` is 45 MB of screenshots and `showcase/reference/gpt/` is another 11 MB.
  Neither is committed; decide what's worth keeping in the repo before this branch goes
  anywhere.
- `og:image` points at `chronorixun.github.io/S2x/shots/…`, which only resolves once the
  shots directory is published alongside the page.
