#!/usr/bin/env bash
# Build a throwaway Title/Chapter library tree from examples/ so that the
# frequency-across-chapters and exact-duplicate logic can be exercised
# (examples/ is sorted into flat junk|manga|"art and covers" dirs and
# contains no chapter structure of its own).
#
# Usage: tests/make-fixture.sh [dest]   (default: /tmp/kopio-fixture)
set -euo pipefail

here="$(cd "$(dirname "$0")/.." && pwd)"
src="$here/examples"
dest="${1:-/tmp/kopio-fixture}"

[ -d "$src/junk" ] && [ -d "$src/manga" ] || { echo "error: $src/junk or $src/manga not found" >&2; exit 1; }

rm -rf "$dest"
mkdir -p "$dest"

# Series A: 5 chapters of real manga pages, plus the SAME credit pages copied
# into every chapter (exact dupes + the frequency signal).
credits=("junk/001.png" "junk/013 (1).png" "junk/092.png" "junk/026.jpg")
mapfile -t pages < <(cd "$src/manga" && ls -1 | sort)

nch=5
for i in $(seq 1 $nch); do
    ch="$dest/Series A/Chapter $i"
    mkdir -p "$ch"
    n=1
    for c in "${credits[@]}"; do
        base="$(basename "$c")"
        [ -f "$src/$c" ] && cp "$src/$c" "$ch/$(printf 'z%02d' $n)-${base// /_}"
        n=$((n+1))
    done
    # Mihon leftovers in every chapter
    touch "$ch/.nomedia"
    [ -f "$src/junk/ComicInfo.xml" ] && cp "$src/junk/ComicInfo.xml" "$ch/ComicInfo.xml"
done

# spread the manga pages across the chapters
i=0
for f in "${pages[@]}"; do
    ch=$(( (i % nch) + 1 ))
    cp "$src/manga/$f" "$dest/Series A/Chapter $ch/${f// /_}"
    i=$((i+1))
done

# Series B: a small control series with no repeated credit pages
mkdir -p "$dest/Series B/Ch 1" "$dest/Series B/Ch 2"
i=0
for f in "${pages[@]:0:8}"; do
    ch=$(( (i % 2) + 1 ))
    cp "$src/manga/$f" "$dest/Series B/Ch $ch/${f// /_}"
    i=$((i+1))
done

echo "fixture written to: $dest"
find "$dest" -type f | wc -l | xargs echo "total files:"
