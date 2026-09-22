#!/usr/bin/env bash
# Build responsive WebP variants for every capture the build references.
# The originals stay untouched and are only loaded by the lightbox.
set -e
cd "$(dirname "$0")"

PAGE=master/index.html
HERO=20260921153435_1.jpg
WIDTHS="640 960 1280"

for f in $(grep -oE '[0-9]{14}_1\.jpg' "$PAGE" | sort -u); do
  src="shots/$f"
  base="${f%.jpg}"
  w=$(ffprobe -v error -select_streams v:0 -show_entries stream=width -of csv=p=0 "$src")
  widths="$WIDTHS"
  [ "$f" = "$HERO" ] && widths="$WIDTHS 1920 2560"
  for tw in $widths; do
    # Never upscale: a capture narrower than the target gets no variant at that width.
    [ "$w" -lt "$tw" ] && continue
    mkdir -p "shots/w$tw"
    ffmpeg -loglevel error -y -i "$src" -vf "scale=$tw:-2" \
      -c:v libwebp -quality 78 -compression_level 6 "shots/w$tw/$base.webp"
  done
done

for d in shots/w*; do
  echo "$d: $(ls "$d" | wc -l) files, $(du -sh "$d" | cut -f1)"
done
