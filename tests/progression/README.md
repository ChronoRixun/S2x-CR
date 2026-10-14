# CR progression offline checks

Build and run from a VS 2022 x64 developer shell:

```powershell
New-Item -ItemType Directory -Force build\tests\progression | Out-Null
cl /nologo /std:c++20 /EHsc /W4 /I src\client tests\progression\policy.cpp /Fe:build\tests\progression\policy.exe /Fo:build\tests\progression\policy.obj
.\build\tests\progression\policy.exe
```

The harness compiles the production rank count/advance, cosmetic selection, and permanent item grant helpers. The in-memory store stub models commit/rollback and settled receipt replay. It covers rank down and re-earned rank suppression, next prestige, save rejection, MP helmets/cards, ten ZM cards, all eight division reward sets, all twelve master milestones, duplicate selectors, mode isolation, ownership conflicts, and rental/collision rejection. It does not modify profile or economy files.

Production routes through the existing admitted bdReward event path. Ordinary rank rare drops use per-rank achievement fulfillment high water and the upstream atomic achievement/inventory save. MP master ranks retain native player_master_prestige_ranks display-level high water, including CR migration. First ordinary rank observation pays one drop and baselines earlier prestige occurrences rather than paying historical drops.

The converter's cr_progression_rebaseline marker makes the main-thread readiness scheduler baseline all ranks reached by loaded native stats while the virtual lobby is loaded or the established frontend state is active without a client/server game, then write a per-mode completion marker before gameplay. Startup baseline grants no drops; the first actual rank-up after it pays normally. Reward events fail closed and remain retryable while baseline is pending; they never finalize migration themselves. Loading without idle or gameplay proof keeps readiness waiting. If loaded stats are observed in actual gameplay (CL_IsLocalClientInGame(0) without virtual lobby), readiness latches a blocked state and logs a restart-in-frontend requirement rather than baselining advanced gameplay stats. Native profile stats must match the converted CR save: CR prunes generic event receipts and cannot reconstruct prior ordinary rank history independently. Cosmetics retain CR prestige:* receipt identities; migrated sentinel fingerprints are treated as already settled.

ZM native table milestone achievements still complete, while only CR rank processing grants drops. Upstream quest and character grants remain in their original path. MP experience publication uses Ranked group 0 prestige/experience plus inventoryTotalXP minus inventoryXPAtLastReset, verified by CR stats.cpp's native calculation notes. ZM uses Coop group 3 prestigeLevel/totalXP.

The level-56 master camo/helmet placements retain CR's documented inference. No new engine hooks or addresses were introduced. The offline harness establishes policy behavior; actual engine event timing, inventory UI refresh and client runtime smoke checks still require a compatible game setup.
