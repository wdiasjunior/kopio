#!/usr/bin/env bash
# Build a throwaway Title/Chapter library tree from examples/ so that the
# frequency-across-chapters and exact-duplicate logic can be exercised
# (examples/ itself is flat and contains no byte-identical duplicates).
#
# Usage: tests/make-fixture.sh [dest]   (default: /tmp/kopio-fixture)
set -euo pipefail

here="$(cd "$(dirname "$0")/.." && pwd)"
src="$here/examples"
dest="${1:-/tmp/kopio-fixture}"

[ -d "$src" ] || { echo "error: $src not found" >&2; exit 1; }

rm -rf "$dest"
mkdir -p "$dest"

# Series A: 5 chapters. Each chapter gets a slice of ordinary pages, plus the
# SAME credit pages copied into every chapter (exact dupes + frequency signal).
credits=("001.jpg" "013 (1).png" "092.png" "001 (1).jpg")
mapfile -t all < <(cd "$src" && ls -1 *.png *.jpg *.gif 2>/dev/null | sort)

# pages that are not credit pages, distributed round-robin over chapters
declare -a rest=()
for f in "${all[@]}"; do
    skip=0
    for c in "${credits[@]}"; do [ "$f" = "$c" ] && skip=1; done
    [ $skip -eq 0 ] && rest+=("$f")
done

nch=5
for i in $(seq 1 $nch); do
    ch="$dest/Series A/Chapter $i"
    mkdir -p "$ch"
    # credit pages appear in every chapter
    n=1
    for c in "${credits[@]}"; do
        [ -f "$src/$c" ] && cp "$src/$c" "$ch/$(printf 'z%02d' $n)-${c// /_}"
        n=$((n+1))
    done
    # Mihon leftovers in every chapter
    touch "$ch/.nomedia"
    [ -f "$src/ComicInfo.xml" ] && cp "$src/ComicInfo.xml" "$ch/ComicInfo.xml"
done

# spread ordinary pages across the chapters
i=0
for f in "${rest[@]}"; do
    ch=$(( (i % nch) + 1 ))
    cp "$src/$f" "$dest/Series A/Chapter $ch/${f// /_}"
    i=$((i+1))
done

# Series B: a small control series with no repeated credit pages
mkdir -p "$dest/Series B/Ch 1" "$dest/Series B/Ch 2"
i=0
for f in "${rest[@]:0:8}"; do
    ch=$(( (i % 2) + 1 ))
    cp "$src/$f" "$dest/Series B/Ch $ch/${f// /_}"
    i=$((i+1))
done

echo "fixture written to: $dest"
find "$dest" -type f | wc -l | xargs echo "total files:"
