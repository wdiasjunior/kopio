/* Unit tests for the pure C core. No Qt, runs under ctest. */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kop_classify.h"
#include "kop_hash.h"
#include "kop_meta.h"
#include "kop_metrics.h"

static int failures = 0;
#define CHECK(cond) do { \
    if (!(cond)) { \
        fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        failures++; \
    } \
} while (0)

static void test_sniff(void)
{
    KopFileKind kind;
    KopImageHeader hdr;

    /* minimal PNG IHDR: 640x480, RGB */
    uint8_t png[64] = { 0x89, 'P', 'N', 'G', '\r', '\n', 0x1a, '\n',
                        0, 0, 0, 13, 'I', 'H', 'D', 'R',
                        0, 0, 0x02, 0x80,  0, 0, 0x01, 0xE0,  8, 2, 0, 0, 0 };
    CHECK(kop_sniff(png, sizeof png, &kind, &hdr) == 0);
    CHECK(kind == KOP_FMT_PNG && hdr.width == 640 && hdr.height == 480 && !hdr.is_grayscale);

    /* grayscale PNG (color type 0) */
    png[25] = 0;
    CHECK(kop_sniff(png, sizeof png, &kind, &hdr) == 0 && hdr.is_grayscale);

    /* JPEG with an APP0 then SOF0: 100x200, 3 components */
    uint8_t jpg[64] = { 0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x04, 0x00, 0x00,
                        0xFF, 0xC0, 0x00, 0x11, 8, 0x00, 0xC8, 0x00, 0x64, 3 };
    CHECK(kop_sniff(jpg, sizeof jpg, &kind, &hdr) == 0);
    CHECK(kind == KOP_FMT_JPEG && hdr.width == 100 && hdr.height == 200 && !hdr.is_grayscale);

    uint8_t gif[16] = { 'G', 'I', 'F', '8', '9', 'a', 0x40, 0x01, 0xF0, 0x00 };
    CHECK(kop_sniff(gif, sizeof gif, &kind, &hdr) == 0);
    CHECK(kind == KOP_FMT_GIF && hdr.width == 320 && hdr.height == 240);

    const char *xml = "<?xml version=\"1.0\"?><ComicInfo/>";
    CHECK(kop_sniff((const uint8_t *)xml, strlen(xml), &kind, &hdr) == 0 && kind == KOP_FMT_XML);

    uint8_t junk[8] = { 1, 2, 3, 4, 5, 6, 7, 8 };
    CHECK(kop_sniff(junk, sizeof junk, &kind, &hdr) == -1);
}

static void test_hash(void)
{
    CHECK(kop_hamming64(0, 0) == 0);
    CHECK(kop_hamming64(0xFFFFFFFFFFFFFFFFull, 0) == 64);
    CHECK(kop_hamming64(0xF0ull, 0x0Full) == 8);

    /* gradient image: dHash must be invariant under downscale */
    int w = 64, h = 64;
    uint8_t *g = malloc((size_t)w * h);
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++)
            g[y * w + x] = (uint8_t)(x * 4);
    uint8_t small[9 * 8], half[32 * 32], small2[9 * 8];
    kop_gray_downscale(g, w, h, small, 9, 8);
    uint64_t d1 = kop_dhash64(small);
    kop_gray_downscale(g, w, h, half, 32, 32);
    kop_gray_downscale(half, 32, 32, small2, 9, 8);
    uint64_t d2 = kop_dhash64(small2);
    CHECK(kop_hamming64(d1, d2) <= 4);
    CHECK(d1 == 0); /* strictly increasing rows: left is never brighter */
    free(g);
}

static void test_metrics(void)
{
    int w = 100, h = 100;
    uint8_t *rgba = malloc((size_t)w * h * 4);
    KopPixelMetrics m;

    memset(rgba, 0xFF, (size_t)w * h * 4); /* all white */
    CHECK(kop_compute_metrics(rgba, w, h, &m) == 0);
    CHECK(m.white_ratio > 0.99f && m.ink_ratio < 0.01f && !m.is_color);
    CHECK(m.edge_density < 0.001f && m.border_white > 0.99f);
    CHECK(m.unique_colors == 1);

    /* checkerboard black/white */
    for (int y = 0; y < h; y++)
        for (int x = 0; x < w; x++) {
            uint8_t v = ((x + y) & 1) ? 255 : 0;
            uint8_t *p = rgba + ((size_t)y * w + x) * 4;
            p[0] = p[1] = p[2] = v; p[3] = 255;
        }
    CHECK(kop_compute_metrics(rgba, w, h, &m) == 0);
    CHECK(m.edge_density > 0.3f && !m.is_color);

    /* solid red = color */
    for (long i = 0; i < (long)w * h; i++) {
        uint8_t *p = rgba + i * 4;
        p[0] = 255; p[1] = 0; p[2] = 0; p[3] = 255;
    }
    CHECK(kop_compute_metrics(rgba, w, h, &m) == 0);
    CHECK(m.is_color);
    free(rgba);
}

/* Build a synthetic catalog and check grouping + frequency + heuristics. */
static void test_classify(void)
{
    KopClassifyParams p;
    kop_classify_defaults(&p);

    enum { N = 24 };
    KopRecord r[N];
    memset(r, 0, sizeof r);
    for (int i = 0; i < N; i++) {
        r[i].id = i;
        r[i].kind = KOP_FMT_PNG;
        r[i].decoded = 1;
        r[i].hdr.width = 1400; r[i].hdr.height = 2000;   /* normal page shape */
        r[i].file_size = 900000;                          /* bpp 0.32 */
        r[i].m.white_ratio = 0.30f; r[i].m.ink_ratio = 0.55f;
        r[i].m.edge_density = 0.15f; r[i].m.unique_colors = 900;
        r[i].m.border_white = 0.5f;
        r[i].chash_lo = 1000 + i; r[i].chash_hi = 7;      /* all distinct */
        r[i].dhash = 0x9E3779B97F4A7C15ull * (uint64_t)(i + 1); /* pairwise far apart */
        r[i].series_id = 0;
        r[i].chapter_id = i / 4;                          /* 6 chapters, 4 pages each */
    }

    /* 0: .nomedia (empty) and 1: xml */
    r[0].kind = KOP_FMT_NOMEDIA; r[0].file_size = 0; r[0].decoded = 0;
    r[1].kind = KOP_FMT_XML; r[1].decoded = 0;

    /* 2 and 6: exact copies in different chapters */
    r[6].chash_lo = r[2].chash_lo; r[6].chash_hi = r[2].chash_hi;
    r[6].dhash = r[2].dhash;

    /* credit page repeated (near-identical) in 4 of 6 chapters: 3,7,11,15 */
    r[7].dhash = r[3].dhash ^ 0x3;    /* hamming 2 */
    r[11].dhash = r[3].dhash ^ 0x30;  /* hamming 2 */
    r[15].dhash = r[3].dhash ^ 0x101; /* hamming 2 */

    /* 20: wide grayscale banner */
    r[20].hdr.width = 1366; r[20].hdr.height = 373;
    /* 21: blank page */
    r[21].file_size = 20000; r[21].m.white_ratio = 0.995f; r[21].m.ink_ratio = 0.001f;
    r[21].m.edge_density = 0.001f; r[21].dhash = 0;
    /* 22: color cover in normal page shape -> guarded, must stay clean */
    r[22].m.is_color = 1; r[22].m.edge_density = 0.12f;
    /* 23: translation notes: white-heavy, moderate edges, unclustered */
    r[23].m.white_ratio = 0.85f; r[23].m.ink_ratio = 0.10f; r[23].m.edge_density = 0.05f;

    CHECK(kop_classify(r, N, &p) == 0);

    CHECK(r[0].category == KOP_CAT_JUNK_FILE && (r[0].reasons & KOP_R_NOMEDIA));
    CHECK(r[1].category == KOP_CAT_JUNK_FILE && (r[1].reasons & KOP_R_XML));
    CHECK(r[2].category == KOP_CAT_EXACT_DUPE && r[6].category == KOP_CAT_EXACT_DUPE);
    CHECK(r[2].dupe_group == r[6].dupe_group && r[2].dupe_group >= 0);
    CHECK(r[3].category == KOP_CAT_JUNK_PAGE && (r[3].reasons & KOP_R_FREQ_CLUSTER));
    CHECK(r[7].category == KOP_CAT_JUNK_PAGE);
    CHECK(r[15].sim_cluster == r[3].sim_cluster && r[3].sim_cluster >= 0);
    CHECK(r[20].category == KOP_CAT_JUNK_PAGE && (r[20].reasons & KOP_R_WIDE_BANNER));
    CHECK(r[21].category == KOP_CAT_JUNK_PAGE && (r[21].reasons & KOP_R_BLANK));
    CHECK(r[22].category == KOP_CAT_CLEAN);
    CHECK(r[23].category == KOP_CAT_REVIEW && (r[23].reasons & KOP_R_TEXT_HEAVY));
    CHECK(r[4].category == KOP_CAT_CLEAN); /* ordinary page untouched */

    /* idempotent */
    CHECK(kop_classify(r, N, &p) == 0);
    CHECK(r[3].category == KOP_CAT_JUNK_PAGE && r[4].category == KOP_CAT_CLEAN);
}

int main(void)
{
    test_sniff();
    test_hash();
    test_metrics();
    test_classify();
    if (failures) {
        fprintf(stderr, "%d check(s) failed\n", failures);
        return 1;
    }
    printf("all core tests passed\n");
    return 0;
}
