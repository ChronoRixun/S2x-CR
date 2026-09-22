#!/usr/bin/env bash
cd "$(dirname "$0")"
for f in candidates/*.html; do
  echo "=== $f ($(wc -c < "$f") bytes) ==="
  printf '  doctype     : '; head -1 "$f" | grep -qi '<!doctype html>' && echo yes || echo "NO"
  printf '  viewport    : '; grep -qi 'name="viewport"' "$f" && echo yes || echo "NO"
  printf '  description : '; grep -qi 'name="description"' "$f" && echo yes || echo "NO"
  printf '  og tags     : '; echo "$(grep -oc 'property="og:' "$f" 2>/dev/null || echo 0)"
  printf '  twitter     : '; echo "$(grep -oc 'name="twitter:' "$f" 2>/dev/null || echo 0)"
  printf '  imgs        : '; echo "$(grep -oc '<img' "$f")"
  printf '  lazy        : '; echo "$(grep -oc 'loading="lazy"' "$f")"
  printf '  imgs no alt : '; echo "$(grep -o '<img[^>]*>' "$f" | grep -vc 'alt=')"
  printf '  imgs no dims: '; echo "$(grep -o '<img[^>]*>' "$f" | grep -vc 'width=')"
  printf '  closes html : '; tail -3 "$f" | grep -qi '</html>' && echo yes || echo "NO"
  # every referenced shot must exist on disk
  missing=0
  for s in $(grep -o '\.\./shots/[0-9_]*\.jpg' "$f" | sort -u); do
    p="${s#../}"
    [ -f "$p" ] || { echo "  MISSING IMAGE: $s"; missing=$((missing+1)); }
  done
  echo "  missing imgs: $missing"
done
