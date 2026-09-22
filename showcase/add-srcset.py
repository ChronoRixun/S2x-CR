"""Point every capture on the build at its responsive WebP variants.

Each <img src="../shots/NAME.jpg" ...> becomes a srcset over the variants that
mkthumbs.sh produced, with a `sizes` value matching the grid it sits in. The
original JPEG stays referenced by the lightbox (data-full) and nowhere else.
width/height are left alone: they carry the capture's aspect ratio.
"""
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
PAGE = os.path.join(HERE, "master", "index.html")
SHOTS = os.path.join(HERE, "shots")

# How wide each layout renders, keyed to the breakpoints in the page's CSS.
# .wrap caps at 1160px (+30px gutter => 1190px viewport).
SIZES = {
    "hero":   "100vw",
    "series": "(max-width:820px) calc(100vw - 30px), (max-width:1190px) calc(33.3vw - 23px), 373px",
    "g2":     "(max-width:600px) calc(100vw - 30px), (max-width:1190px) calc(50vw - 26px), 569px",
    "g3":     "(max-width:600px) calc(100vw - 30px), (max-width:880px) calc(50vw - 26px), "
              "(max-width:1190px) calc(33.3vw - 25px), 372px",
    "proof":  "(max-width:940px) calc(100vw - 30px), (max-width:1190px) calc(50vw - 31px), 564px",
}

LAYOUT = {
    "20260921153435_1": "hero",
    "20260920205442_1": "series", "20260921171022_1": "series", "20260921175918_1": "series",
    "20260917225123_1": "g2", "20260919172800_1": "g2",
    "20260921160211_1": "g2", "20260919174500_1": "g2",
    "20260919174935_1": "proof", "20260921174414_1": "proof",
    "20260919160555_1": "g3", "20260919160543_1": "g3", "20260919160549_1": "g3",
    "20260919160539_1": "g3", "20260919172920_1": "g3", "20260919191242_1": "g3",
    "20260920023143_1": "g3", "20260920023211_1": "g3", "20260920025023_1": "g3",
    "20260920034956_1": "g3", "20260920035108_1": "g3", "20260920031000_1": "g3",
}

VARIANT_WIDTHS = [640, 960, 1280, 1920, 2560]

with open(PAGE, encoding="utf-8", newline="") as fh:
    html = fh.read()

# Match the opening of each capture tag plus its declared width, so the
# original's size is known without re-probing the file.
tag = re.compile(r'<img src="\.\./shots/(\d{14}_1)\.jpg" width="(\d+)"')
seen = []


def rewrite(m):
    name, orig_w = m.group(1), int(m.group(2))
    if name not in LAYOUT:
        sys.exit(f"no layout mapping for {name} - add it to LAYOUT")
    seen.append(name)

    cands = []
    for w in VARIANT_WIDTHS:
        if os.path.exists(os.path.join(SHOTS, f"w{w}", f"{name}.webp")):
            cands.append((f"../shots/w{w}/{name}.webp", w))
    if not cands:
        sys.exit(f"no variants on disk for {name} - run mkthumbs.sh")
    # Captures narrower than 1280 have no top variant; let high-DPR screens
    # fall back to the original rather than upscaling a smaller one.
    if cands[-1][1] < orig_w:
        cands.append((f"../shots/{name}.jpg", orig_w))

    srcset = ", ".join(f"{url} {w}w" for url, w in cands)
    fallback = next((u for u, w in cands if w == 960), cands[0][0])
    return (f'<img src="{fallback}" srcset="{srcset}" '
            f'sizes="{SIZES[LAYOUT[name]]}" width="{orig_w}"')


html, n = tag.subn(rewrite, html)

missing = set(LAYOUT) - set(seen)
if missing:
    sys.exit(f"mapped but not found on the page: {sorted(missing)}")

with open(PAGE, "w", encoding="utf-8", newline="") as fh:
    fh.write(html)

print(f"rewrote {n} image tags")
