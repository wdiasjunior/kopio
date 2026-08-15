#include "kop_hash.h"

void kop_rgba_to_gray(const uint8_t *rgba, int w, int h, uint8_t *gray)
{
    long n = (long)w * h;
    for (long i = 0; i < n; i++) {
        const uint8_t *p = rgba + i * 4;
        /* Rec.601 integer approximation */
        gray[i] = (uint8_t)((77 * p[0] + 150 * p[1] + 29 * p[2]) >> 8);
    }
}

void kop_gray_downscale(const uint8_t *gray, int w, int h, uint8_t *out, int ow, int oh)
{
    for (int oy = 0; oy < oh; oy++) {
        int y0 = (int)((long)oy * h / oh);
        int y1 = (int)((long)(oy + 1) * h / oh);
        if (y1 <= y0) y1 = y0 + 1;
        if (y1 > h) y1 = h;
        for (int ox = 0; ox < ow; ox++) {
            int x0 = (int)((long)ox * w / ow);
            int x1 = (int)((long)(ox + 1) * w / ow);
            if (x1 <= x0) x1 = x0 + 1;
            if (x1 > w) x1 = w;
            long sum = 0;
            for (int y = y0; y < y1; y++)
                for (int x = x0; x < x1; x++)
                    sum += gray[(long)y * w + x];
            out[(long)oy * ow + ox] = (uint8_t)(sum / ((long)(y1 - y0) * (x1 - x0)));
        }
    }
}

uint64_t kop_dhash64(const uint8_t *gray9x8)
{
    uint64_t bits = 0;
    for (int r = 0; r < 8; r++)
        for (int c = 0; c < 8; c++)
            if (gray9x8[r * 9 + c] > gray9x8[r * 9 + c + 1])
                bits |= (uint64_t)1 << (r * 8 + c);
    return bits;
}

int kop_hamming64(uint64_t a, uint64_t b)
{
    uint64_t x = a ^ b;
    int n = 0;
    while (x) { x &= x - 1; n++; }
    return n;
}
