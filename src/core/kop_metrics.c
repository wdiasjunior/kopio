#include "kop_metrics.h"

#include <stdlib.h>
#include <string.h>

/* All metrics are computed on a nearest-neighbor sample of at most SW pixels
 * on the long edge, which keeps the cost independent of source resolution. */
#define SW 256

/* Drawing coverage: fraction of 16x16 blocks that contain edges, measured on
 * a box-filtered (area-averaged) gray image of at most CW pixels on the long
 * edge. Unlike the nearest-neighbor sample, box filtering keeps thin line
 * art and panel borders of clean digital pages, which tell real drawn pages
 * apart from mostly empty credit/blank pages. Returns -1 on allocation
 * failure. */
#define CW 512
#define COV_BLOCK 16
#define COV_EDGE 40

static int compute_coverage(const uint8_t *rgba, int w, int h, float *out)
{
    int cw, ch;
    if (w >= h) { cw = w < CW ? w : CW; ch = (int)((long)h * cw / w); }
    else        { ch = h < CW ? h : CW; cw = (int)((long)w * ch / h); }
    if (cw < 1) cw = 1;
    if (ch < 1) ch = 1;
    *out = 0.0f;

    uint8_t *g = malloc((size_t)cw * ch);
    uint32_t *acc = calloc((size_t)cw, sizeof(uint32_t));
    uint32_t *cnt = calloc((size_t)cw, sizeof(uint32_t));
    int *xmap = malloc((size_t)w * sizeof(int));
    if (!g || !acc || !cnt || !xmap) { free(g); free(acc); free(cnt); free(xmap); return -1; }
    for (int x = 0; x < w; x++) xmap[x] = (int)((long)x * cw / w);

    int oy = 0;
    for (int y = 0; y < h; y++) {
        const uint8_t *p = rgba + (long)y * w * 4;
        for (int x = 0; x < w; x++, p += 4) {
            int ox = xmap[x];
            acc[ox] += (uint32_t)((77 * p[0] + 150 * p[1] + 29 * p[2]) >> 8);
            cnt[ox]++;
        }
        int next = (y + 1 < h) ? (int)((long)(y + 1) * ch / h) : ch;
        if (next != oy) {
            uint8_t *row = g + (long)oy * cw;
            for (int x = 0; x < cw; x++) {
                row[x] = cnt[x] ? (uint8_t)(acc[x] / cnt[x]) : 255;
                acc[x] = cnt[x] = 0;
            }
            oy = next;
        }
    }

    int bw = (cw - 1) / COV_BLOCK, bh = (ch - 1) / COV_BLOCK;
    if (bw > 0 && bh > 0) {
        long covered = 0;
        for (int by = 0; by < bh; by++) {
            for (int bx = 0; bx < bw; bx++) {
                int edges = 0;
                for (int y = by * COV_BLOCK; y < (by + 1) * COV_BLOCK; y++) {
                    const uint8_t *row = g + (long)y * cw;
                    for (int x = bx * COV_BLOCK; x < (bx + 1) * COV_BLOCK; x++) {
                        if (abs(row[x + 1] - row[x]) > COV_EDGE ||
                            abs(row[x + cw] - row[x]) > COV_EDGE)
                            edges++;
                    }
                }
                /* > 2% of the block's 256 pixels */
                if (edges * 50 > COV_BLOCK * COV_BLOCK) covered++;
            }
        }
        *out = (float)covered / (float)((long)bw * bh);
    }

    free(g);
    free(acc);
    free(cnt);
    free(xmap);
    return 0;
}

int kop_compute_metrics(const uint8_t *rgba, int w, int h, KopPixelMetrics *out)
{
    memset(out, 0, sizeof(*out));
    if (!rgba || w <= 0 || h <= 0) return -1;

    int sw, sh;
    if (w >= h) { sw = w < SW ? w : SW; sh = (int)((long)h * sw / w); }
    else        { sh = h < SW ? h : SW; sw = (int)((long)w * sh / h); }
    if (sw < 1) sw = 1;
    if (sh < 1) sh = 1;

    uint8_t *gray = malloc((size_t)sw * sh);
    uint8_t *seen = calloc(4096 / 8, 1); /* RGB444 presence bitmap */
    if (!gray || !seen) { free(gray); free(seen); return -1; }

    long n = (long)sw * sh;
    long ink = 0, white = 0, black = 0, chroma = 0;
    long border_total = 0, border_white = 0;
    int unique = 0;
    int bx = sw / 20 + 1, by = sh / 20 + 1; /* outer 5% frame (>=1 px) */

    for (int y = 0; y < sh; y++) {
        int sy = (int)((long)y * h / sh);
        for (int x = 0; x < sw; x++) {
            int sx = (int)((long)x * w / sw);
            const uint8_t *p = rgba + ((long)sy * w + sx) * 4;
            uint8_t r = p[0], g = p[1], b = p[2];
            uint8_t gr = (uint8_t)((77 * r + 150 * g + 29 * b) >> 8);
            gray[(long)y * sw + x] = gr;

            if (gr < 200) ink++;
            if (gr >= 245) white++;
            if (gr <= 10) black++;

            int dmax = abs(r - g);
            int d2 = abs(r - b); if (d2 > dmax) dmax = d2;
            int d3 = abs(g - b); if (d3 > dmax) dmax = d3;
            if (dmax > 16) chroma++;

            int key = ((r >> 4) << 8) | ((g >> 4) << 4) | (b >> 4);
            if (!(seen[key >> 3] & (1 << (key & 7)))) {
                seen[key >> 3] |= (uint8_t)(1 << (key & 7));
                unique++;
            }

            if (y < by || y >= sh - by || x < bx || x >= sw - bx) {
                border_total++;
                if (gr >= 245) border_white++;
            }
        }
    }

    long grad_sum = 0, grad_n = 0;
    for (int y = 0; y < sh; y++) {
        for (int x = 0; x + 1 < sw; x++) {
            grad_sum += abs(gray[(long)y * sw + x + 1] - gray[(long)y * sw + x]);
            grad_n++;
        }
    }
    for (int y = 0; y + 1 < sh; y++) {
        for (int x = 0; x < sw; x++) {
            grad_sum += abs(gray[(long)(y + 1) * sw + x] - gray[(long)y * sw + x]);
            grad_n++;
        }
    }

    out->ink_ratio = (float)ink / (float)n;
    out->white_ratio = (float)white / (float)n;
    out->black_ratio = (float)black / (float)n;
    out->edge_density = grad_n ? (float)grad_sum / ((float)grad_n * 255.0f) : 0.0f;
    out->unique_colors = unique;
    out->border_white = border_total ? (float)border_white / (float)border_total : 0.0f;
    out->is_color = (float)chroma / (float)n > 0.02f;

    /* page layout: per-row dark/light/white counts */
    long blank_rows = 0, dark_rows = 0, gutters = 0, inner_rows = 0, two_tone = 0;
    int iy0 = sh / 10, iy1 = sh - sh / 10;
    for (int y = 0; y < sh; y++) {
        const uint8_t *row = gray + (long)y * sw;
        long dark = 0, whiteish = 0;
        for (int x = 0; x < sw; x++) {
            uint8_t v = row[x];
            if (v < 128) dark++;
            if (v >= 235) whiteish++;
            if (v < 40 || v > 215) two_tone++;
        }
        long light = sw - dark;
        if (dark * 1000 < 5L * sw) blank_rows++;
        if (light * 1000 < 5L * sw) dark_rows++;
        if (y >= iy0 && y < iy1) {
            inner_rows++;
            if (whiteish * 100 > 97L * sw) gutters++;
        }
    }
    out->blank_rows = (float)blank_rows / (float)sh;
    out->dark_rows = (float)dark_rows / (float)sh;
    out->white_gutters = inner_rows ? (float)gutters / (float)inner_rows : 0.0f;
    out->two_tone = (float)two_tone / (float)n;

    free(gray);
    free(seen);
    return compute_coverage(rgba, w, h, &out->coverage);
}
