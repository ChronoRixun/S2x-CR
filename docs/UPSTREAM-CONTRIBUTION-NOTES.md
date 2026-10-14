# Future upstream contributions

The upstream rebuild is isolated on `codex/s2x-cr-upstream-rebuild`; upstream base `0048253`, CR reference `integration` at `6bfd9d6`, hosting port `2541f12`, economy restoration `242298b`. Keep the original checkout and deployed installations until candidate validation is complete.

Before submitting upstream, create a focused branch from the then-current upstream and cherry-pick or extract only generally useful fixes. Do not submit the complete CR policy restoration as a single upstream change.

Potential contributions to assess separately:

- Hosting and party lifecycle compatibility fixes, with direct-connect, dedicated-server and reconnect evidence.
- Server Manager compatibility improvements, including the measured WMI timeout adjustment; separate CR branding and defaults.
- Marketplace catalog visibility and paging corrections if the live CWL shop investigation establishes a general defect. The ready/cache-verified native CWL menu and purchase passed; the early blank probe did not establish a paging defect.
- Economy transaction replay, rollback and persistence validation improvements that apply independently of CR prices, rewards and catalog choices.
- Progression readiness and duplicate-reward protections, with MP/ZM and migration boundary coverage.

CR-specific acquisition policy, reward quantities, rotation catalog, custom bundles, loot weights, diagnostic commands and migration from legacy CR saves should remain identifiable as fork policy unless upstream explicitly wants them.

For each proposed contribution, record the triggering case, minimal source diff, original-call and lifecycle constraints, Debug/Release builds, relevant regression test and isolated runtime result. Rebase against current upstream and check its contribution requirements before opening a PR. Existing test fixtures are synthetic; no real saves or credentials should be included.

## Candidate validation in progress

Native MP purchases, rare-drop opening, contract activation, Headquarters loading and redemption of a synthetic claimable objective passed. Repeat reward claims were refused. CWL shop population/purchase, Zombies purchase/opening and synthetic MP/ZM native rank/replay checks subsequently passed. A gameplay melee kill also earned XP, promoted rank 3 and delivered the next Zombies drop; see tests/client-economy/README.md for fixture limitations. Nothing has been deployed.
