# Font licences go here

PLACEHOLDER: no font files are in Assets/Fonts yet, so there is no licence text to carry.

When the theme pack fonts are added (see ../README.md for the list), copy each family's licence
from its google/fonts folder into this folder, one file per family:

    BlackOpsOne-OFL.txt      BarlowCondensed-OFL.txt   SpecialElite-LICENSE.txt (Apache-2.0)
    Creepster-OFL.txt        Oswald-OFL.txt            JetBrainsMono-OFL.txt
    VT323-OFL.txt            Orbitron-OFL.txt          Rajdhani-OFL.txt
    ShareTechMono-OFL.txt    RussoOne-OFL.txt          ChakraPetch-OFL.txt
    PressStart2P-OFL.txt     PixelifySans-OFL.txt      SpaceMono-OFL.txt
    Cinzel-OFL.txt           Manrope-OFL.txt           IBMPlexMono-OFL.txt

The SIL Open Font License 1.1 lets the fonts be bundled with software, including inside an
executable, provided the licence text travels with them and the fonts are not sold on their own.
S2xServerManager.csproj builds `licenses\*.txt` into the exe as resources; the release packaging
(build\diagnostics\package-release.ps1) should also copy this folder beside the exe once it holds
real licence files.
