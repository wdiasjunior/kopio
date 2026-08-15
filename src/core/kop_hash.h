/* Grayscale conversion, box downscaling, and 64-bit difference hash. */
#ifndef KOP_HASH_H
#define KOP_HASH_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* rgba: w*h*4 bytes; gray out: w*h bytes (Rec.601 luma). */
void kop_rgba_to_gray(const uint8_t *rgba, int w, int h, uint8_t *gray);

/* Box-filter downscale of a grayscale image to ow x oh (any sizes >= 1). */
void kop_gray_downscale(const uint8_t *gray, int w, int h, uint8_t *out, int ow, int oh);

/* dHash over a 9x8 grayscale thumbnail (72 bytes, row-major):
 * bit r*8+c is set when px[r][c] > px[r][c+1]. */
uint64_t kop_dhash64(const uint8_t *gray9x8);

int kop_hamming64(uint64_t a, uint64_t b);

#ifdef __cplusplus
}
#endif

#endif /* KOP_HASH_H */
