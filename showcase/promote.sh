#!/usr/bin/env bash
# Promote the build to the GitHub Pages source directory (docs/).
# Pages serves branch `integration`, path /docs.
set -e
cd "$(dirname "$0")/.."

SRC=showcase/master/index.html
DEST=docs/index.html
SHOTDIR=docs/shots

mkdir -p "$SHOTDIR"

# Copy only the captures the page actually references.
n=0
for f in $(grep -oE '[0-9]{14}_1\.jpg' "$SRC" | sort -u); do
  cp "showcase/shots/$f" "$SHOTDIR/$f"
  n=$((n+1))
done
echo "copied $n captures to $SHOTDIR"

# Rewrite ../shots/ -> shots/ since the page now sits one level higher.
sed 's|\.\./shots/|shots/|g' "$SRC" > "$DEST"

echo "wrote $DEST ($(wc -c < "$DEST") bytes)"
echo "remaining ../ references: $(grep -c '\.\./' "$DEST" || true)"
echo "shots dir: $(du -sh "$SHOTDIR" | cut -f1)"
