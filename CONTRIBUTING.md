# Contributing

This fork lives by a lesson from its first pull request upstream. The maintainer of S2x closed it, and his reasons are the best contribution guide this project could have, so they come first.

> The original issue is fairly small, but this PR adds a lot of machinery around it. [...] I think we're solving the problem at the wrong level. [...] Don't only focus on making the reported issue work. Especially when a fix grows this large, first try to understand the root cause and whether it can be fixed closer to the source.
>
> Using AI to help is completely fine, I use it myself as well, but you still need to understand and take responsibility for the design you're submitting. You should be able to explain why something was implemented this way, what simpler options were considered and why the added complexity is necessary.
>
> — [Brentdevent, closing PR #61](https://github.com/Brentdevent/S2x/pull/61)

That pull request was about 1,200 lines. Once the cause was traced, the fix was 18 lines added and 15 removed. Everything below follows from that.

## The rules

1. **Find the root cause first.** Trace it: a file and line, a log line, a capture from a real run. A plausible story is not a cause. The first two explanations for #61 were both plausible and both wrong; instrumenting one real rotation settled it.
2. **Fix it at the source, or establish why you can't.** If a workaround is unavoidable, the comment beside it says what the cause is and why it cannot be removed, in plain words a reader can follow without repeating the investigation.
3. **Make the smallest change at that level.** No new configuration or state system as a first answer. Treat added logging and diagnostics as suspect: polish stacked on a working fix has broken the fix here before.
4. **Be able to explain it.** Why this design, what simpler options you considered, why any complexity is necessary. If that does not fit in a paragraph, the change is not ready.
5. **AI is fine. The design is yours.** Research and draft with whatever you like; what you submit is something you understand and can defend. No AI attribution in commits or pull requests.
6. **Deleting is a change too.** A removal justified only by reading is not finished until the thing has been run under the conditions the deleted code was written for.

## How work flows

- Open an issue before a branch, so the problem is stated before the solution. One branch per issue from `integration`, named `fix/<issue>-<topic>` or `feat/<issue>-<topic>`.
- Keep it buildable: `generate.bat`, then Release x64 with no new warnings.
- Test it in play. Install the build and exercise what the change touches: a dedicated server through a rotation, the menu, the console command and its error paths, the persisted files afterwards. [tests/RUNBOOK.md](tests/RUNBOOK.md) shows the shape of a live check. Where an offline harness exists under `tests/` (economy, rank, scripting, storage), it must pass, and it grows when the code it covers changes.
- Say what you did not test. That sentence is worth more than an implied full pass.
- A pull request body is a short paragraph, not a document: the cause, the change, what it was tested against, what was not tested, and the simpler option you rejected and why. Length in proportion to the diff.
- One pass of an automated reviewer is a thermometer, not a treadmill. A clean first pass is evidence the change is at the source. A review that keeps producing findings is telling you about the shape of the change, not its correctness; re-examine the design instead of working through the list.
- Fixes here may be offered to [Brentdevent/S2x](https://github.com/Brentdevent/S2x). Anything that goes upstream is held to this standard first, and only the fork's owner opens upstream pull requests.

## Code

- Match the surrounding style: tabs, brace placement, naming. The codebase is upstream's; keep diffs mergeable.
- An engine address gets a comment saying what it is and how it was found, so the next person can find it again after a game update.
- Loose files (Lua, GSC, CSV) live under `data/` and ship in the release zip. A change that needs a loose file says so in the pull request.
- No new dependencies without the reason written down.

## Reporting bugs

Use the bug report template. The useful parts are the build (release tag or commit), where it happened (client Multiplayer, client Zombies, dedicated server, the launcher, the status script), the steps, and `s2x\logs\console.log` from the game folder. For a crash, the `0x` code from the Windows event log and, if you have `s2x.pdb` installed, the symbolized stack.
