/* Cheap header sniffers: identify jpg/png/gif/webp/xml from the first bytes
 * of a file, and extract dimensions + color info without decoding pixels. */
#ifndef KOP_META_H
#define KOP_META_H

#include <stddef.h>
#include "kop_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Sniff a file's leading bytes (a few KB is enough for most files; JPEGs with
 * huge EXIF blocks may need more — pass up to 64 KB when available).
 * Returns 0 on success with *kind set; *hdr is filled for image kinds when the
 * dimensions were found (width stays 0 otherwise). Returns -1 if the buffer
 * matches no known kind. */
int kop_sniff(const uint8_t *buf, size_t len, KopFileKind *kind, KopImageHeader *hdr);

#ifdef __cplusplus
}
#endif

#endif /* KOP_META_H */
