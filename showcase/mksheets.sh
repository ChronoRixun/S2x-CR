#!/usr/bin/env bash
set -e
cd "$(dirname "$0")"

FONT="C\\:/Windows/Fonts/arialbd.ttf"

rm -rf thumbs sheets
mkdir -p thumbs sheets

# 1. Normalise every shot to an identical thumbnail with its index burned in.
n=1
while [ $n -le 103 ]; do
  src=$(printf 'seq/%04d.jpg' $n)
  dst=$(printf 'thumbs/%04d.jpg' $n)
  label=$(printf '%03d' $n)
  ffmpeg -loglevel error -y -i "$src" \
    -vf "scale=420:236:force_original_aspect_ratio=decrease,pad=420:236:(ow-iw)/2:(oh-ih)/2:color=0x101010,drawtext=fontfile='${FONT}':text='${label}':x=8:y=6:fontsize=30:fontcolor=yellow:box=1:boxcolor=black@0.85:boxborderw=6" \
    -frames:v 1 "$dst"
  n=$(( n + 1 ))
done
echo "thumbs: $(ls thumbs | wc -l)"

# 2. Tile them 4x3 into sheets, twelve at a time.
k=0
while [ $k -lt 9 ]; do
  rm -rf batch
  mkdir -p batch
  i=1
  while [ $i -le 12 ]; do
    n=$(( k * 12 + i ))
    src=$(printf 'thumbs/%04d.jpg' $n)
    if [ -f "$src" ]; then
      cp "$src" "$(printf 'batch/%04d.jpg' $i)"
    fi
    i=$(( i + 1 ))
  done
  if [ "$(ls batch | wc -l)" -gt 0 ]; then
    out=$(printf 'sheets/sheet%02d.jpg' $(( k + 1 )))
    ffmpeg -loglevel error -y -start_number 1 -i batch/%04d.jpg \
      -vf "tile=4x3:padding=6:color=0x202020" -frames:v 1 "$out"
  fi
  k=$(( k + 1 ))
done
rm -rf batch
echo "sheets: $(ls sheets | wc -l) unique: $(md5sum sheets/*.jpg | awk '{print $1}' | sort -u | wc -l)"
