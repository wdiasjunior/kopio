#include "kop_metrics.h"

#include <stdlib.h>
#include <string.h>

/* All metrics are computed on a nearest-neighbor sample of at most SW pixels
 * on the long edge, which keeps the cost independent of source resolution. */
#define SW 256

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

    free(gray);
    free(seen);
    return 0;
}
