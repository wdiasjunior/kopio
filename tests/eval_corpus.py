#!/usr/bin/env python3
"""Evaluate the classifier against ground-truth labels.

Usage: ./build/kopio-scan --classify examples | python3 tests/eval_corpus.py tests/corpus-labels.tsv

Labels TSV: path<TAB>label, where label is one of
  junk kinds:  credit blank title logo junk (generic, from directory sorting)
  keep kinds:  content cover spread
  review-ish:  notes art (flagging is fine, junking is tolerated but reported)
  ignored:     unsure unreadable
"""
import sys
from collections import Counter, defaultdict

JUNK = {"credit", "blank", "title", "logo", "junk"}
KEEP = {"content", "cover", "spread"}

labels = {}
with open(sys.argv[1]) as f:
    for line in f:
        parts = line.rstrip("\n").split("\t")
        if len(parts) >= 2:
            labels[parts[0]] = parts[1]

conf = defaultdict(Counter)
header = sys.stdin.readline().rstrip("\n").split("\t")
for line in sys.stdin:
    row = dict(zip(header, line.rstrip("\n").split("\t")))
    gt = labels.get(row["path"])
    if gt in (None, "unsure", "unreadable"):
        continue
    conf[gt][row["category"]] += 1

cats = ["junk-page", "exact-dupe", "review", "clean", "junk-file"]
print(f"{'label':<10}" + "".join(f"{c:>11}" for c in cats) + f"{'total':>7}")
for gt in ["credit", "blank", "title", "logo", "junk", "notes", "art",
           "content", "cover", "spread"]:
    if gt not in conf:
        continue
    total = sum(conf[gt].values())
    print(f"{gt:<10}" + "".join(f"{conf[gt].get(c, 0):>11}" for c in cats) + f"{total:>7}")

junk_total = sum(sum(conf[g].values()) for g in JUNK)
junk_flagged = sum(conf[g][c] for g in JUNK for c in ("junk-page", "exact-dupe", "review"))
keep_total = sum(sum(conf[g].values()) for g in KEEP)
keep_junked = sum(conf[g][c] for g in KEEP for c in ("junk-page", "exact-dupe"))
keep_review = sum(conf[g]["review"] for g in KEEP)

print(f"\njunk flagged:   {junk_flagged}/{junk_total} ({100 * junk_flagged / max(1, junk_total):.0f}%)")
print(f"keepers junked: {keep_junked}/{keep_total}   keepers in review: {keep_review}/{keep_total}")
sys.exit(1 if keep_junked else 0)
