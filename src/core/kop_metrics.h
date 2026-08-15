/* Pixel-level heuristic metrics computed from a decoded RGBA buffer. */
#ifndef KOP_METRICS_H
#define KOP_METRICS_H

#include "kop_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Computes all KopPixelMetrics fields. Samples internally, so passing a
 * full-resolution buffer is fine. Returns 0 on success, -1 on bad input or
 * allocation failure. */
int kop_compute_metrics(const uint8_t *rgba, int w, int h, KopPixelMetrics *out);

#ifdef __cplusplus
}
#endif

#endif /* KOP_METRICS_H */
