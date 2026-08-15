#include "kop_classify.h"
#include "kop_hash.h"

#include <stdlib.h>
#include <string.h>

void kop_classify_defaults(KopClassifyParams *p)
{
    p->phash_ham_max = 6;
    p->cluster_blank_excl = 0.97f;
    p->freq_chapter_min = 3;
    p->freq_chapter_frac = 0.30f;
    p->freq_max_per_chapter = 2;
    p->guard_aspect_lo = 0.55f;
    p->guard_aspect_hi = 0.80f;
    p->guard_edge_min = 0.06f;
    p->spread_aspect_lo = 1.20f;
    p->spread_aspect_hi = 1.60f;
    p->banner_aspect = 1.60f;
    p->banner_small_dim = 1200;
    p->square_lo = 0.90f;
    p->square_hi = 1.15f;
    p->square_colors_max = 512;
    p->square_edge_max = 0.04f;
    p->blank_bpp_max = 0.03f;
    p->blank_white_min = 0.98f;
    p->blank_black_min = 0.98f;
    p->title_white_min = 0.92f;
    p->title_ink_max = 0.05f;
    p->title_border_min = 0.97f;
    p->notes_white_lo = 0.75f;
    p->notes_white_hi = 0.92f;
    p->notes_edge_lo = 0.03f;
    p->notes_edge_hi = 0.145f;
    p->flat_colors_max = 120; /* grayscale pages quantize to <=16; color manga
                                 art lands in the hundreds — flat credit art
                                 sits in between */
    p->flat_ink_max = 0.30f;
}

static int is_image_kind(KopFileKind k)
{
    return k == KOP_FMT_JPEG || k == KOP_FMT_PNG || k == KOP_FMT_GIF ||
           k == KOP_FMT_WEBP || k == KOP_FMT_UNKNOWN;
}

/* ---- sorting helpers -------------------------------------------------- */

static const KopRecord *g_recs; /* qsort context (single-threaded classify) */

static int cmp_by_chash(const void *a, const void *b)
{
    const KopRecord *ra = &g_recs[*(const int *)a];
    const KopRecord *rb = &g_recs[*(const int *)b];
    if (ra->chash_hi != rb->chash_hi) return ra->chash_hi < rb->chash_hi ? -1 : 1;
    if (ra->chash_lo != rb->chash_lo) return ra->chash_lo < rb->chash_lo ? -1 : 1;
    return 0;
}

static int cmp_i32_pairs(const void *a, const void *b)
{
    const int32_t *pa = a, *pb = b;
    if (pa[0] != pb[0]) return pa[0] < pb[0] ? -1 : 1;
    if (pa[1] != pb[1]) return pa[1] < pb[1] ? -1 : 1;
    return 0;
}

/* ---- union-find ------------------------------------------------------- */

static int uf_find(int *parent, int i)
{
    while (parent[i] != i) {
        parent[i] = parent[parent[i]];
        i = parent[i];
    }
    return i;
}

static void uf_union(int *parent, int a, int b)
{
    int ra = uf_find(parent, a), rb = uf_find(parent, b);
    if (ra != rb) parent[rb] = ra;
}

/* Distinct chapters per series, from sorted (series,chapter) pairs.
 * Returns the chapter count for `series` in the sorted pair table. */
static int chapters_in_series(const int32_t *pairs, int npairs, int32_t series)
{
    int n = 0;
    for (int i = 0; i < npairs; i++)
        if (pairs[i * 2] == series) n++;
    return n;
}

int kop_classify(KopRecord *recs, int count, const KopClassifyParams *p)
{
    if (count <= 0) return 0;

    /* reset outputs so classify is idempotent */
    for (int i = 0; i < count; i++) {
        recs[i].category = KOP_CAT_UNSCANNED;
        recs[i].reasons = 0;
        recs[i].score = 0.0f;
        recs[i].dupe_group = -1;
        recs[i].sim_cluster = -1;
    }

    /* ---- Stage A: junk metadata files --------------------------------- */
    for (int i = 0; i < count; i++) {
        if (recs[i].kind == KOP_FMT_NOMEDIA) {
            recs[i].reasons |= KOP_R_NOMEDIA;
            recs[i].category = recs[i].file_size == 0 ? KOP_CAT_JUNK_FILE : KOP_CAT_REVIEW;
        } else if (recs[i].kind == KOP_FMT_XML) {
            recs[i].reasons |= KOP_R_XML;
            recs[i].category = KOP_CAT_JUNK_FILE;
        }
    }

    int *idx = malloc(sizeof(int) * (size_t)count);
    int *parent = malloc(sizeof(int) * (size_t)count);
    if (!idx || !parent) { free(idx); free(parent); return -1; }
    for (int i = 0; i < count; i++) { idx[i] = i; parent[i] = i; }

    /* ---- Stage B1: exact duplicate groups ----------------------------- */
    g_recs = recs;
    qsort(idx, (size_t)count, sizeof(int), cmp_by_chash);
    int next_group = 0;
    for (int i = 0; i < count;) {
        int j = i + 1;
        while (j < count && cmp_by_chash(&idx[i], &idx[j]) == 0) j++;
        /* only image files form dupe groups; metadata junk is already handled */
        int nimg = 0;
        for (int k = i; k < j; k++)
            if (is_image_kind(recs[idx[k]].kind) && recs[idx[k]].file_size > 0) nimg++;
        if (nimg >= 2) {
            int gid = next_group++;
            for (int k = i; k < j; k++) {
                KopRecord *r = &recs[idx[k]];
                if (!is_image_kind(r->kind) || r->file_size == 0) continue;
                r->dupe_group = gid;
                r->reasons |= KOP_R_EXACT_DUPE;
                if (r->category == KOP_CAT_UNSCANNED) r->category = KOP_CAT_EXACT_DUPE;
            }
        }
        i = j;
    }

    /* ---- Stage B2: dHash similarity clusters -------------------------- */
    /* Near-featureless pages all share dhash≈0; excluding them keeps blanks
     * from welding text-light pages into one giant bogus cluster. */
    int nc = 0; /* clusterable subset */
    for (int i = 0; i < count; i++) {
        KopRecord *r = &recs[i];
        if (is_image_kind(r->kind) && r->decoded &&
            r->m.white_ratio <= p->cluster_blank_excl &&
            r->m.black_ratio <= p->cluster_blank_excl)
            idx[nc++] = i;
    }
    for (int a = 0; a < nc; a++)
        for (int b = a + 1; b < nc; b++)
            if (kop_hamming64(recs[idx[a]].dhash, recs[idx[b]].dhash) <= p->phash_ham_max)
                uf_union(parent, idx[a], idx[b]);

    /* assign stable cluster ids to clusters of size >= 2 */
    int *csize = calloc((size_t)count, sizeof(int));
    int *cid = malloc(sizeof(int) * (size_t)count);
    if (!csize || !cid) { free(idx); free(parent); free(csize); free(cid); return -1; }
    for (int i = 0; i < nc; i++) csize[uf_find(parent, idx[i])]++;
    int next_cluster = 0;
    for (int i = 0; i < count; i++) cid[i] = -1;
    for (int i = 0; i < nc; i++) {
        int root = uf_find(parent, idx[i]);
        if (csize[root] >= 2) {
            if (cid[root] < 0) cid[root] = next_cluster++;
            recs[idx[i]].sim_cluster = cid[root];
        }
    }

    /* ---- chapters-per-series table ------------------------------------ */
    int32_t *pairs = malloc(sizeof(int32_t) * 2 * (size_t)count);
    if (!pairs) { free(idx); free(parent); free(csize); free(cid); return -1; }
    for (int i = 0; i < count; i++) {
        pairs[i * 2] = recs[i].series_id;
        pairs[i * 2 + 1] = recs[i].chapter_id;
    }
    qsort(pairs, (size_t)count, sizeof(int32_t) * 2, cmp_i32_pairs);
    int npairs = 0;
    for (int i = 0; i < count; i++) {
        if (npairs == 0 || pairs[npairs * 2 - 2] != pairs[i * 2] ||
            pairs[npairs * 2 - 1] != pairs[i * 2 + 1]) {
            pairs[npairs * 2] = pairs[i * 2];
            pairs[npairs * 2 + 1] = pairs[i * 2 + 1];
            npairs++;
        }
    }

    /* ---- Stage B3: frequency-across-chapters verdict ------------------ */
    if (next_cluster > 0) {
        /* sort member indices by (cluster, series, chapter) */
        int nm = 0;
        for (int i = 0; i < count; i++)
            if (recs[i].sim_cluster >= 0) idx[nm++] = i;
        /* insertion-style key sort via qsort on packed keys */
        for (int ci = 0; ci < next_cluster; ci++) {
            int junk = 0;
            /* per-series stats inside this cluster */
            for (int i = 0; i < nm && !junk; i++) {
                if (recs[idx[i]].sim_cluster != ci) continue;
                int32_t series = recs[idx[i]].series_id;
                /* count distinct chapters and the max members in one chapter */
                int spread = 0, max_per_chapter = 0;
                for (int j = 0; j < nm; j++) {
                    KopRecord *rj = &recs[idx[j]];
                    if (rj->sim_cluster != ci || rj->series_id != series) continue;
                    int dup_chapter = 0, members_here = 1;
                    for (int k = 0; k < j; k++) {
                        KopRecord *rk = &recs[idx[k]];
                        if (rk->sim_cluster == ci && rk->series_id == series &&
                            rk->chapter_id == rj->chapter_id) { dup_chapter = 1; }
                    }
                    for (int k = 0; k < nm; k++) {
                        KopRecord *rk = &recs[idx[k]];
                        if (k != j && rk->sim_cluster == ci && rk->series_id == series &&
                            rk->chapter_id == rj->chapter_id) members_here++;
                    }
                    if (!dup_chapter) spread++;
                    if (members_here > max_per_chapter) max_per_chapter = members_here;
                }
                int total = chapters_in_series(pairs, npairs, series);
                int need = (int)(p->freq_chapter_frac * (float)total + 0.999f);
                if (need < p->freq_chapter_min) need = p->freq_chapter_min;
                if (spread >= need && max_per_chapter <= p->freq_max_per_chapter)
                    junk = 1;
            }
            if (junk) {
                for (int i = 0; i < nm; i++) {
                    KopRecord *r = &recs[idx[i]];
                    if (r->sim_cluster != ci) continue;
                    r->reasons |= KOP_R_FREQ_CLUSTER;
                    if (r->category == KOP_CAT_UNSCANNED) r->category = KOP_CAT_JUNK_PAGE;
                }
            }
        }
    }

    /* ---- Stage C: per-image heuristics -------------------------------- */
    for (int i = 0; i < count; i++) {
        KopRecord *r = &recs[i];
        if (r->category != KOP_CAT_UNSCANNED) continue;
        if (!is_image_kind(r->kind) || !r->decoded ||
            r->hdr.width == 0 || r->hdr.height == 0) {
            r->category = KOP_CAT_CLEAN;
            continue;
        }
        float aspect = (float)r->hdr.width / (float)r->hdr.height;
        float bpp = (float)r->file_size / ((float)r->hdr.width * (float)r->hdr.height);
        int maxdim = (int)(r->hdr.width > r->hdr.height ? r->hdr.width : r->hdr.height);
        const KopPixelMetrics *m = &r->m;

        int guard =
            (m->is_color && aspect >= p->guard_aspect_lo && aspect <= p->guard_aspect_hi &&
             m->edge_density > p->guard_edge_min) ||
            (aspect >= p->spread_aspect_lo && aspect < p->spread_aspect_hi);
        if (guard) r->reasons |= KOP_R_COLOR_GUARD;

        KopCategory cat = KOP_CAT_CLEAN;

        if (aspect >= p->banner_aspect) {
            r->reasons |= KOP_R_WIDE_BANNER;
            cat = (!m->is_color || maxdim <= p->banner_small_dim)
                      ? KOP_CAT_JUNK_PAGE : KOP_CAT_REVIEW;
        } else if (aspect >= p->square_lo && aspect <= p->square_hi) {
            r->reasons |= KOP_R_SQUARE;
            cat = (m->unique_colors < p->square_colors_max ||
                   m->edge_density < p->square_edge_max)
                      ? KOP_CAT_JUNK_PAGE : KOP_CAT_REVIEW;
        }

        if (bpp < p->blank_bpp_max &&
            (m->white_ratio > p->blank_white_min || m->black_ratio > p->blank_black_min)) {
            r->reasons |= KOP_R_BLANK;
            if (cat != KOP_CAT_JUNK_PAGE) cat = KOP_CAT_JUNK_PAGE;
        } else if (m->white_ratio > p->title_white_min && m->ink_ratio < p->title_ink_max &&
                   m->border_white > p->title_border_min && !m->is_color) {
            r->reasons |= KOP_R_TITLE_ONLY;
            if (cat == KOP_CAT_CLEAN) cat = KOP_CAT_REVIEW;
        } else if (m->white_ratio >= p->notes_white_lo && m->white_ratio <= p->notes_white_hi &&
                   m->edge_density >= p->notes_edge_lo && m->edge_density <= p->notes_edge_hi &&
                   r->sim_cluster < 0) {
            r->reasons |= KOP_R_TEXT_HEAVY;
            if (cat == KOP_CAT_CLEAN) cat = KOP_CAT_REVIEW;
        } else if (m->is_color && m->unique_colors < p->flat_colors_max &&
                   m->ink_ratio < p->flat_ink_max) {
            r->reasons |= KOP_R_FLAT_COLOR;
            if (cat == KOP_CAT_CLEAN) cat = KOP_CAT_REVIEW;
        }

        if (guard && cat == KOP_CAT_JUNK_PAGE) cat = KOP_CAT_REVIEW;
        r->category = cat;
    }

    /* ---- score: orders items within a category ------------------------ */
    for (int i = 0; i < count; i++) {
        KopRecord *r = &recs[i];
        float s = 0.0f;
        switch (r->category) {
        case KOP_CAT_JUNK_FILE:
        case KOP_CAT_EXACT_DUPE:
        case KOP_CAT_JUNK_PAGE: s = 0.5f; break;
        case KOP_CAT_REVIEW: s = 0.2f; break;
        default: s = 0.0f; break;
        }
        uint32_t flags = r->reasons & ~KOP_R_COLOR_GUARD;
        while (flags) { flags &= flags - 1; s += 0.1f; }
        if (r->reasons & KOP_R_COLOR_GUARD) s -= 0.3f;
        if (s < 0.0f) s = 0.0f;
        if (s > 1.0f) s = 1.0f;
        r->score = s;
    }

    free(idx);
    free(parent);
    free(csize);
    free(cid);
    free(pairs);
    return 0;
}
