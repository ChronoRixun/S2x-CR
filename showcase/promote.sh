#!/usr/bin/env bash
# Promote the build to the GitHub Pages source directory (docs/).
# Pages serves branch `integration`, path /docs.
#
# docs/shots is rebuilt from scratch on every run: it gets exactly the files
# the page references (originals for the lightbox, WebP variants for srcset)
# and nothing else, so a capture dropped from the page can't linger.
set -e
cd "$(dirname "$0")/.."

SRC=showcase/master/index.html
DEST=docs/index.html

rm -rf docs/shots
n=0
# Relative references (img/srcset/lightbox) and absolute ones (og:image and
# twitter:image point at the published URL, so a plain ../shots/ scan misses them).
refs=$( { grep -oE '\.\./shots/[A-Za-z0-9_/]+\.(jpg|webp)' "$SRC" | sed 's|^\.\./shots/||';
          grep -oE 'chronorixun\.github\.io/S2x/shots/[A-Za-z0-9_/]+\.(jpg|webp)' "$SRC" | sed 's|^.*/S2x/shots/||'; } | sort -u )
for rel in $refs; do
  if [ ! -f "showcase/shots/$rel" ]; then
    echo "MISSING showcase/shots/$rel - run mkthumbs.sh first" >&2
    exit 1
  fi
  mkdir -p "docs/shots/$(dirname "$rel")"
  cp "showcase/shots/$rel" "docs/shots/$rel"
  n=$((n+1))
done
echo "copied $n files into docs/shots"

# The page sits one level higher once published.
sed 's|\.\./shots/|shots/|g' "$SRC" > "$DEST"

echo "wrote $DEST ($(wc -c < "$DEST") bytes)"
echo "remaining ../ references: $(grep -c '\.\./' "$DEST" || true)"
echo "docs/shots total: $(du -sh docs/shots | cut -f1)"
