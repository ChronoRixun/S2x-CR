# Font licences

One licence file per theme pack font family, copied unchanged from that family's folder in
github.com/google/fonts (2026-09-28):

    BlackOpsOne-OFL.txt      BarlowCondensed-OFL.txt   SpecialElite-LICENSE.txt (Apache-2.0)
    Creepster-OFL.txt        Oswald-OFL.txt            JetBrainsMono-OFL.txt
    VT323-OFL.txt            Orbitron-OFL.txt          Rajdhani-OFL.txt
    ShareTechMono-OFL.txt    RussoOne-OFL.txt          ChakraPetch-OFL.txt
    PressStart2P-OFL.txt     PixelifySans-OFL.txt      SpaceMono-OFL.txt
    Cinzel-OFL.txt           Manrope-OFL.txt           IBMPlexMono-OFL.txt

The SIL Open Font License 1.1 lets the fonts be bundled with software, including inside an
executable, provided the licence text travels with them and the fonts are not sold on their own.
Special Elite is under the Apache License 2.0, which also allows redistribution with its licence.
S2xServerManager.csproj builds `licenses\*.txt` into the exe as resources, and the release ZIP
carries this folder beside the exe.
