#!/usr/bin/env python3
"""Find metric thresholds that catch missed junk without touching keepers.

Usage: ./build/kopio-scan --classify examples | python3 tests/features.py tests/corpus-labels.tsv [gray|color] [--holdout]

For every numeric kopio-scan column, prints the keepers' range and how many
junk files the classifier left clean fall outside it (with a 10% margin), on
the tune half of the junk (or the holdout half with --holdout). Gray mode
compares against gray keepers (manga pages), color mode against color
keepers (covers, spreads). A column that separates well on the tune half and
still holds on the holdout half is a candidate for a rule in kop_classify.c.
"""
import hashlib
import sys

JUNK = {"credit", "blank", "title", "logo", "junk"}
KEEP = {"content", "cover", "spread"}
MARGIN = 0.10
NON_METRIC = {"path", "kind", "color", "dhash", "category", "score",
              "dupe_group", "sim_cluster", "reasons",
              # file geometry/size: separates this corpus by accident only
              "width", "height", "aspect", "bytes", "bpp"}


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    holdout = "--holdout" in sys.argv
    mode = args[1] if len(args) > 1 else "gray"
    want_color = mode == "color"

    labels = {}
    with open(args[0]) as f:
        for line in f:
            parts = line.rstrip("\n").split("\t")
            if len(parts) >= 2:
                labels[parts[0]] = parts[1]

    header = sys.stdin.readline().rstrip("\n").split("\t")
    cols = [c for c in header if c not in NON_METRIC]
    keep, miss = [], []
    for line in sys.stdin:
        r = dict(zip(header, line.rstrip("\n").split("\t")))
        if (r["color"] == "color") != want_color:
            continue
        gt = labels.get(r["path"])
        for c in cols:
            r[c] = float(r[c])
        if gt in KEEP:
            keep.append(r)
        elif (gt in JUNK and r["category"] == "clean" and
              (hashlib.md5(r["path"].encode()).digest()[0] & 1 == 1) == holdout):
            miss.append(r)

    half = "holdout" if holdout else "tune"
    print(f"{mode}: {len(keep)} keepers, {len(miss)} unflagged junk ({half} half)\n")
    print(f"{'metric':<12}{'keep min':>10}{'keep max':>10}{'below':>8}{'above':>8}")
    for c in cols:
        v = sorted(r[c] for r in keep)
        if not v:
            continue
        span = (v[-1] - v[0]) or 1.0
        lo, hi = v[0] - MARGIN * span, v[-1] + MARGIN * span
        below = sum(r[c] < lo for r in miss)
        above = sum(r[c] > hi for r in miss)
        print(f"{c:<12}{v[0]:>10.4g}{v[-1]:>10.4g}{below:>8}{above:>8}")


if __name__ == "__main__":
    main()
