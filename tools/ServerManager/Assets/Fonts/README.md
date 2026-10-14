# Theme pack fonts

The theme pack (design/S2x Theme Pack.dc.html) names 18 Google Fonts families. All 23 files
below are checked in (downloaded from github.com/google/fonts on 2026-09-28 with the owner's
approval, each checked against the repository's blob hash) and built into the exe. A theme role
whose family is missing still falls back to the installed Windows font in the fallback column.

## Adding them

1. Download the files below from github.com/google/fonts (the `Path` column, under `main/`).
2. Save each one in this folder under the name in the `Save as` column. Variable fonts are named
   `Family[wght].ttf` in that repository; **rename them without the square brackets**
   (`Family-VariableFont_wght.ttf`, the name fonts.google.com's own download uses). WPF cannot
   open a resource whose name contains `[` or `]`: such a file is built into the exe and then
   silently never used. `tests/server-manager/themes.ps1` fails on a bracketed name.
3. Put each family's licence text in `licenses/` (see below).
4. Rebuild. `S2xServerManager.csproj` already includes `Assets\Fonts\*.ttf`, `*.otf` and
   `licenses\*.txt` as WPF Resources, so the exe stays a single file and no code changes.
   After renaming a file that was already built in, build with `--no-incremental` once.

`Services/ThemeFonts.cs` reads the family name out of every font file built in; a theme role
whose family is found uses it (with the pack's own scale), otherwise its fallback (with a scale
correction for the fallback's metrics). Any file name works as long as the family inside matches.

## Files

| Family | Save as | Path in github.com/google/fonts | Licence | Used by (role) | Fallback without it |
| --- | --- | --- | --- | --- | --- |
| Black Ops One | BlackOpsOne-Regular.ttf | ofl/blackopsone/BlackOpsOne-Regular.ttf | OFL-1.1 | Field Ops (display, label) | Bahnschrift Bold |
| Barlow Condensed | BarlowCondensed-Regular.ttf, BarlowCondensed-SemiBold.ttf, BarlowCondensed-Bold.ttf | ofl/barlowcondensed/ (same names) | OFL-1.1 | Field Ops (body) | Bahnschrift Condensed |
| Special Elite | SpecialElite-Regular.ttf | apache/specialelite/SpecialElite-Regular.ttf | Apache-2.0 | Field Ops (mono) | Courier New |
| Creepster | Creepster-Regular.ttf | ofl/creepster/Creepster-Regular.ttf | OFL-1.1 | Undead (display) | Impact |
| Oswald | Oswald-VariableFont_wght.ttf | ofl/oswald/Oswald[wght].ttf | OFL-1.1 | Undead (body, label) | Bahnschrift SemiCondensed |
| JetBrains Mono | JetBrainsMono-VariableFont_wght.ttf | ofl/jetbrainsmono/JetBrainsMono[wght].ttf | OFL-1.1 | Undead, Pack-a-Punch (mono) | Cascadia Mono, then Consolas |
| VT323 | VT323-Regular.ttf | ofl/vt323/VT323-Regular.ttf | OFL-1.1 | Phosphor (all) | Consolas |
| Orbitron | Orbitron-VariableFont_wght.ttf | ofl/orbitron/Orbitron[wght].ttf | OFL-1.1 | Outrun (display, label) | Bahnschrift SemiBold |
| Rajdhani | Rajdhani-Medium.ttf, Rajdhani-Bold.ttf | ofl/rajdhani/ (same names) | OFL-1.1 | Outrun (body) | Bahnschrift SemiCondensed |
| Share Tech Mono | ShareTechMono-Regular.ttf | ofl/sharetechmono/ShareTechMono-Regular.ttf | OFL-1.1 | Outrun (mono), Night Vision (all) | Consolas |
| Russo One | RussoOne-Regular.ttf | ofl/russoone/RussoOne-Regular.ttf | OFL-1.1 | Pack-a-Punch (display, label) | Bahnschrift Bold |
| Chakra Petch | ChakraPetch-Regular.ttf, ChakraPetch-SemiBold.ttf | ofl/chakrapetch/ (same names) | OFL-1.1 | Pack-a-Punch (body) | Bahnschrift |
| Press Start 2P | PressStart2P-Regular.ttf | ofl/pressstart2p/PressStart2P-Regular.ttf | OFL-1.1 | Arcade (display, label) | Lucida Console |
| Pixelify Sans | PixelifySans-VariableFont_wght.ttf | ofl/pixelifysans/PixelifySans[wght].ttf | OFL-1.1 | Arcade (body) | Bahnschrift |
| Space Mono | SpaceMono-Regular.ttf, SpaceMono-Bold.ttf | ofl/spacemono/ (same names) | OFL-1.1 | Arcade (mono) | Cascadia Mono, then Consolas |
| Cinzel | Cinzel-VariableFont_wght.ttf | ofl/cinzel/Cinzel[wght].ttf | OFL-1.1 | Prestige (display, label) | Palatino Linotype, then Georgia |
| Manrope | Manrope-VariableFont_wght.ttf | ofl/manrope/Manrope[wght].ttf | OFL-1.1 | Prestige (body) | Segoe UI |
| IBM Plex Mono | IBMPlexMono-Regular.ttf | ofl/ibmplexmono/IBMPlexMono-Regular.ttf | OFL-1.1 | Prestige (mono) | Cascadia Mono, then Consolas |

23 files in all. High contrast keeps Segoe UI and Consolas and needs nothing here. WPF picks the
named weights (SemiBold, Bold) out of a variable font itself; no static instances are needed.
Check each family's page on fonts.google.com for the current file names and licence before
downloading: the table was written without network access.

## Licences

Every family folder in google/fonts carries its licence: `OFL.txt` for the ofl/ families,
`LICENSE.txt` for apache/specialelite. Copy each one into `licenses/` as
`<Family>-OFL.txt` (for example `BlackOpsOne-OFL.txt`) or `SpecialElite-LICENSE.txt`. They are
built into the exe beside the fonts, and the release zip should carry them too; see
licenses/README.md.
