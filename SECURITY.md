# Security

S2x patches a game's anti-tamper so the client can run at all. Code like that is exactly what a cheat would build on, so anything that could turn this project into one, expose player data, or run code a player did not ask for is a security issue here.

## Reporting

Please do not open a public issue for it. Report privately:

- **Discord:** send a direct message to **chrono** on the CoD WWII Community server (invite in the [README](README.md#community)).
- **GitHub:** once this repository is public, use *Report a vulnerability* on the Security tab.

Include what you found, how to reproduce it, and the build (release tag or commit). Expect an acknowledgement within a few days. Fixes ship as a release, and reports are credited in the release notes if you want them to be.

## In scope

- `s2x.exe`: the memory patches, network handling (out-of-band packets, the master server, the party protocol), loose-file loading and script storage, and the Demonware emulation.
- The server scripting APIs: file storage confinement, `getip`, the roster snapshot.
- `tools/`: the server launcher and the status script, which holds a Discord bot token on the server box.
- The release zips.

## Out of scope

- Activision, Steam or Sledgehammer services and the game's own bugs.
- Issues in upstream S2x that this fork does not change; report those to [Brentdevent/S2x](https://github.com/Brentdevent/S2x).
- Requests to make cheating easier. They will not get a reply.

## Supported versions

Only the latest release is supported. Older zips are kept for reference and do not receive fixes.

## What the project already does

- The bot token for the status script is read from a file or an environment variable on the server box and is never logged or sent anywhere but Discord.
- Player addresses are available only to scripts the server owner installs, and S2x never sends them to a third party.
- Script storage accepts plain filenames only, refuses links and devices, caps files at 64 KiB, and writes atomically.
- The client requires a legitimate Steam copy of the game and never distributes game files.
