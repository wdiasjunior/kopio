#!/usr/bin/env python3
"""Evaluate the classifier against ground-truth labels.

Usage: ./build/kopio-scan --classify examples | python3 tests/eval_corpus.py tests/corpus-labels.tsv [--holdout]

Labels TSV: path<TAB>label, where label is one of
  junk kinds:  credit blank title logo junk (generic, from directory sorting)
  keep kinds:  content cover spread
  review-ish:  notes art (flagging is fine, junking is tolerated but reported)
  ignored:     unsure unreadable

Junk files are split into a stable tune/holdout half (hash of the path), so
thresholds tuned on one half can be checked on the other. --holdout prints the
holdout half's per-directory numbers; the default prints the tune half's.
Exit status is 1 if any keeper is junked.
"""
import hashlib
import sys
from collections import Counter, defaultdict

JUNK = {"credit", "blank", "title", "logo", "junk"}
KEEP = {"content", "cover", "spread"}
CATS = ["junk-page", "exact-dupe", "review", "clean", "junk-file"]
FLAGGED = ("junk-page", "exact-dupe", "review")


def is_holdout(path):
    return hashlib.md5(path.encode()).digest()[0] & 1 == 1


def top_dir(path):
    parts = path.split("/")
    return parts[1] if len(parts) > 2 else parts[0]


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    holdout = "--holdout" in sys.argv

    labels = {}
    with open(args[0]) as f:
        for line in f:
            parts = line.rstrip("\n").split("\t")
            if len(parts) >= 2:
                labels[parts[0]] = parts[1]

    header = sys.stdin.readline().rstrip("\n").split("\t")
    rows = []
    for line in sys.stdin:
        row = dict(zip(header, line.rstrip("\n").split("\t")))
        gt = labels.get(row["path"])
        if gt in (None, "unsure", "unreadable"):
            continue
        row["gt"] = gt
        rows.append(row)

    conf = defaultdict(Counter)
    for r in rows:
        conf[r["gt"]][r["category"]] += 1

    print(f"{'label':<10}" + "".join(f"{c:>11}" for c in CATS) + f"{'total':>7}")
    for gt in ["credit", "blank", "title", "logo", "junk", "notes", "art",
               "content", "cover", "spread"]:
        if gt not in conf:
            continue
        total = sum(conf[gt].values())
        print(f"{gt:<10}" + "".join(f"{conf[gt].get(c, 0):>11}" for c in CATS) + f"{total:>7}")

    # per directory; junk files restricted to the chosen half
    half = "holdout" if holdout else "tune"
    print(f"\nper directory (junk: {half} half; unique = not an exact dupe)")
    print(f"{'dir':<18}{'n':>6}{'junked':>8}{'review':>8}{'clean':>7}"
          f"{'uniq n':>8}{'uniq flagged':>14}")
    by_dir = defaultdict(list)
    for r in rows:
        if r["gt"] in JUNK and is_holdout(r["path"]) != holdout:
            continue
        by_dir[top_dir(r["path"])].append(r)
    for d in sorted(by_dir):
        rs = by_dir[d]
        junked = sum(r["category"] in ("junk-page", "exact-dupe") for r in rs)
        review = sum(r["category"] == "review" for r in rs)
        clean = sum(r["category"] == "clean" for r in rs)
        uniq = [r for r in rs if r["category"] != "exact-dupe"]
        uf = sum(r["category"] in ("junk-page", "review") for r in uniq)
        print(f"{d:<18}{len(rs):>6}{junked:>8}{review:>8}{clean:>7}"
              f"{len(uniq):>8}{uf:>8} ({100 * uf / max(1, len(uniq)):.0f}%)")

    junk_rows = [r for r in rows if r["gt"] in JUNK]
    junk_flagged = sum(r["category"] in FLAGGED for r in junk_rows)
    keep_rows = [r for r in rows if r["gt"] in KEEP]
    keep_junked = sum(r["category"] in ("junk-page", "exact-dupe") for r in keep_rows)
    keep_review = sum(r["category"] == "review" for r in keep_rows)

    print(f"\njunk flagged:   {junk_flagged}/{len(junk_rows)} "
          f"({100 * junk_flagged / max(1, len(junk_rows)):.0f}%)")
    print(f"keepers junked: {keep_junked}/{len(keep_rows)}   "
          f"keepers in review: {keep_review}/{len(keep_rows)}")
    sys.exit(1 if keep_junked else 0)


if __name__ == "__main__":
    main()
