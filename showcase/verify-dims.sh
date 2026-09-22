#!/usr/bin/env bash
# Check that every declared width/height on the page matches the real image.
cd "$(dirname "$0")"
page="${1:-master/index.html}"
bad=0
grep -oE 'src="\.\./shots/[0-9_]+\.jpg" width="[0-9]+" height="[0-9]+"' "$page" | while read -r line; do
  f=$(echo "$line" | grep -oE '[0-9]{14}_1\.jpg')
  dw=$(echo "$line" | grep -oE 'width="[0-9]+"' | grep -oE '[0-9]+')
  dh=$(echo "$line" | grep -oE 'height="[0-9]+"' | grep -oE '[0-9]+')
  real=$(ffprobe -v error -select_streams v:0 -show_entries stream=width,height -of csv=p=0 "shots/$f")
  if [ "$real" != "$dw,$dh" ]; then
    echo "MISMATCH $f declared ${dw}x${dh} actual ${real}"
    bad=1
  fi
done
echo "checked $(grep -oc 'src="\.\./shots/' "$page") image references in $page"
