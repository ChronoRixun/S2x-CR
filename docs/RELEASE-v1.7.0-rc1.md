# S2x CR v1.7.0 rc1 release preparation

This release candidate rebuilds the ordinary S2x CR fork on Brentdevent/S2x commit 0048253596962e1b348a32394d5d808480b0644b, retaining CR hosting and restoring CR acquisition and progression policies on upstream's economy backend. It contains the fork client/server, matching loose files, Server Manager 1.2.2, host tools and documentation. The experimental Zombies companion-bot native component, bot gameplay scripts and bot kits are excluded. Ordinary Zombies economy/progression support remains part of the CR fork.

## Hosting and player changes

- Current-base browser/raw UDP/session handling plus CR player caps, bot fill/name pools, server script APIs, local Manager administration, auto-balance, chat commands and map voting.
- Restored CR marketplace/acquisition, supply-drop, contract and progression reward policies on the upstream backend. See CR-ECONOMY-RESTORATION.md for precise coverage.
- Discord status counts human players and active bots separately, reports actual total slots, uses compact rotations and removes artificial blank-row spacing. The companion hint is restricted to explicitly named Zombie Bots servers; the ordinary fork does not provide that companion component.
- Updates remain opt-in. The upstream updater is not a matching CR release channel.

## Upgrade guidance

Back up game-local s2x files, local AppData s2x data, presets/launch-profile registrations and player/economy stores. Install first into a separate game/server folder with matching retail assets. Old loose Lua initialization folders can still execute when simply overlaid. Keep matching executable/data/tools together and preserve chosen server names, ports and profile paths.

The old CR and upstream economy stores both use schema version 1 but incompatible layouts. Do not copy or rename an old CR store into the new backend. Migration is offline, explicitly requested, and writes a new output file; review CR-ECONOMY-MIGRATION.md and work from backups. Representative real-save migration remains an operator acceptance task.

For hosts already running the tested rebuilt candidate, the card correction requires replacing only s2x/tools/server-status.ps1. No game-server restart or scheduled-task reinstallation is needed for that tool update.

## Validation and scope

Native Debug and Release builds, restored catalog/progression/migration regression checks and isolated native economy UI/gameplay fixtures are recorded in the existing validation documentation. Brent's unchanged published client 0048253 passed ordinary MP browser join, actual spawn and intentional disconnect/rejoin. The operator reported real MP play and map voting without issues. The status suite passes under Windows PowerShell 5.1, including raw/legacy network probes, human/bot counts, dry-run behavior and static embed preservation.

No blanket compatibility claim is made for every older CR release or future upstream client. Longer normal matches, representative real-save migration and comprehensive remote/master-list discovery remain release acceptance checks.

## Source and artifact provenance

The distributed native executable remains the tested build at native commit 242298b; subsequent commits add QA/contribution documentation and host-status changes without native implementation changes. Native SHA256: 5291E8A727C6B7A2B5197DD1380FB0DC1431375D6F69EE81C45C175259782E73. Manager SHA256: C195AD9D85AEDF9F6736909827FC365637FF7C3B0BD1D28B31D5AB63A1842018. The Manager source matches the ordinary fork; copying its tested build does not add the companion-bot game component.

Current upstream reached 26358ffa5091427705a416ef62472ad793bdaf68 on 2026-10-14, adding a mod manager and muzzle-flash toggle after the tested base. Those two changes are not in this RC. Contribution branches are evaluated separately against that current upstream head. The GitHub draft is not a publication or a merge into integration.
