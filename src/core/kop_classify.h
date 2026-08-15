/* Whole-catalog classification: exact-duplicate grouping, dHash similarity
 * clustering, frequency-across-chapters analysis, and per-image heuristics. */
#ifndef KOP_CLASSIFY_H
#define KOP_CLASSIFY_H

#include "kop_types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    /* similarity clustering */
    int   phash_ham_max;        /* max hamming distance to join a cluster */
    float cluster_blank_excl;   /* white/black ratio above which an image is
                                   too featureless for dHash clustering */
    /* frequency-across-chapters rule */
    int   freq_chapter_min;     /* cluster must span at least this many chapters */
    float freq_chapter_frac;    /* ...or this fraction of the series' chapters */
    int   freq_max_per_chapter; /* and at most this many members per chapter */
    /* per-image heuristics */
    float guard_aspect_lo, guard_aspect_hi;   /* color cover/page zone */
    float guard_edge_min;
    float spread_aspect_lo, spread_aspect_hi; /* two-page-spread zone */
    float banner_aspect;        /* wide credit banner threshold */
    int   banner_small_dim;     /* small enough to junk even if colored */
    float square_lo, square_hi; /* square logo zone */
    int   square_colors_max;
    float square_edge_max;
    float blank_bpp_max;        /* bytes per pixel */
    float blank_white_min, blank_black_min;
    float title_white_min, title_ink_max, title_border_min;
    float notes_white_lo, notes_white_hi;     /* translation-notes shape */
    float notes_edge_lo, notes_edge_hi;
    int   flat_colors_max;                    /* flat color art (credit pages) */
    float flat_ink_max;
} KopClassifyParams;

void kop_classify_defaults(KopClassifyParams *p);

/* Fills category, reasons, score, dupe_group, sim_cluster on every record.
 * Idempotent: safe to call again on the same array. Records the caller marked
 * as XML/NOMEDIA kinds are categorized from their kind and size alone.
 * Returns 0 on success, -1 on allocation failure. */
int kop_classify(KopRecord *recs, int count, const KopClassifyParams *p);

#ifdef __cplusplus
}
#endif

#endif /* KOP_CLASSIFY_H */
