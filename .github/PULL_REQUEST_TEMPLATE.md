<!-- Keep this short. A small diff gets a short paragraph, not a document. -->

**Cause.** What was actually wrong, with where you found it (file and line, log line, capture).

**Change.** What this does about it, at that level.

**Simpler options.** What smaller change you considered and why it was not enough. If this adds any complexity, why it is necessary.

**Tested.** What you ran it against (a dedicated rotation, the menu, the command's error paths, the harness under `tests/`).

**Not tested.** Say it plainly.

- [ ] Builds Release x64 with no new warnings
- [ ] Played it, not just built it
- [ ] Loose files (Lua, GSC, CSV) this needs are under `data/` and mentioned above
- [ ] No AI attribution in the commits
