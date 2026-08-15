#include "kop_meta.h"

#include <string.h>

static uint16_t be16(const uint8_t *p) { return (uint16_t)((p[0] << 8) | p[1]); }
static uint32_t be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}
static uint16_t le16(const uint8_t *p) { return (uint16_t)(p[0] | (p[1] << 8)); }
static uint32_t le24(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
}
static uint32_t le32(const uint8_t *p)
{
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

/* Walk JPEG markers looking for a start-of-frame segment. */
static void sniff_jpeg(const uint8_t *buf, size_t len, KopImageHeader *hdr)
{
    size_t i = 2;
    while (i + 4 <= len) {
        if (buf[i] != 0xFF) { i++; continue; }
        uint8_t marker = buf[i + 1];
        if (marker == 0xFF) { i++; continue; }          /* fill byte */
        if (marker == 0xD8 || (marker >= 0xD0 && marker <= 0xD7) || marker == 0x01) {
            i += 2;                                     /* standalone marker */
            continue;
        }
        if (i + 4 > len) return;
        uint16_t seglen = be16(buf + i + 2);
        if (seglen < 2) return;
        int is_sof = (marker >= 0xC0 && marker <= 0xCF) &&
                     marker != 0xC4 && marker != 0xC8 && marker != 0xCC;
        if (is_sof) {
            if (i + 2 + 2 + 5 + 1 > len) return;
            const uint8_t *p = buf + i + 4;             /* precision, H, W, ncomp */
            hdr->height = be16(p + 1);
            hdr->width = be16(p + 3);
            hdr->channels = p[5];
            hdr->is_grayscale = (p[5] == 1);
            hdr->has_alpha = 0;
            return;
        }
        if (marker == 0xDA || marker == 0xD9) return;   /* SOS/EOI: past headers */
        i += 2 + seglen;
    }
}

static void sniff_png(const uint8_t *buf, size_t len, KopImageHeader *hdr)
{
    /* 8-byte signature, then IHDR: len(4) "IHDR"(4) W(4) H(4) depth(1) colortype(1) */
    if (len < 8 + 8 + 13) return;
    if (memcmp(buf + 12, "IHDR", 4) != 0) return;
    hdr->width = be32(buf + 16);
    hdr->height = be32(buf + 20);
    uint8_t ct = buf[25];
    switch (ct) {
    case 0: hdr->channels = 1; hdr->is_grayscale = 1; hdr->has_alpha = 0; break;
    case 2: hdr->channels = 3; hdr->is_grayscale = 0; hdr->has_alpha = 0; break;
    case 3: hdr->channels = 3; hdr->is_grayscale = 0; hdr->has_alpha = 0; break; /* palette */
    case 4: hdr->channels = 2; hdr->is_grayscale = 1; hdr->has_alpha = 1; break;
    case 6: hdr->channels = 4; hdr->is_grayscale = 0; hdr->has_alpha = 1; break;
    default: break;
    }
}

static void sniff_gif(const uint8_t *buf, size_t len, KopImageHeader *hdr)
{
    if (len < 10) return;
    hdr->width = le16(buf + 6);
    hdr->height = le16(buf + 8);
    hdr->channels = 3; /* palette-based; assume color until pixels prove otherwise */
    hdr->is_grayscale = 0;
    hdr->has_alpha = 0;
}

static void sniff_webp(const uint8_t *buf, size_t len, KopImageHeader *hdr)
{
    if (len < 12 + 8 + 10) return;
    const uint8_t *c = buf + 12;      /* first chunk header */
    if (memcmp(c, "VP8 ", 4) == 0) {  /* lossy: dims in the frame header */
        const uint8_t *p = c + 8;
        if (p + 10 > buf + len) return;
        if (p[3] != 0x9D || p[4] != 0x01 || p[5] != 0x2A) return; /* start code */
        hdr->width = (uint32_t)(le16(p + 6) & 0x3FFF);
        hdr->height = (uint32_t)(le16(p + 8) & 0x3FFF);
        hdr->channels = 3;
        hdr->has_alpha = 0;
    } else if (memcmp(c, "VP8L", 4) == 0) { /* lossless */
        const uint8_t *p = c + 8;
        if (p + 5 > buf + len || p[0] != 0x2F) return;
        uint32_t bits = le32(p + 1);
        hdr->width = (bits & 0x3FFF) + 1;
        hdr->height = ((bits >> 14) & 0x3FFF) + 1;
        hdr->channels = 3;
        hdr->has_alpha = (uint8_t)((bits >> 28) & 1);
    } else if (memcmp(c, "VP8X", 4) == 0) { /* extended */
        const uint8_t *p = c + 8;
        if (p + 10 > buf + len) return;
        hdr->has_alpha = (uint8_t)((p[0] >> 4) & 1);
        hdr->width = le24(p + 4) + 1;
        hdr->height = le24(p + 7) + 1;
        hdr->channels = 3;
    }
    hdr->is_grayscale = 0; /* webp has no grayscale mode in the header */
}

static int looks_like_xml(const uint8_t *buf, size_t len)
{
    size_t i = 0;
    if (len >= 3 && buf[0] == 0xEF && buf[1] == 0xBB && buf[2] == 0xBF) i = 3; /* BOM */
    while (i < len && (buf[i] == ' ' || buf[i] == '\t' || buf[i] == '\r' || buf[i] == '\n')) i++;
    return i < len && buf[i] == '<';
}

int kop_sniff(const uint8_t *buf, size_t len, KopFileKind *kind, KopImageHeader *hdr)
{
    *kind = KOP_FMT_UNKNOWN;
    memset(hdr, 0, sizeof(*hdr));
    if (!buf || len < 4) return -1;

    if (buf[0] == 0xFF && buf[1] == 0xD8 && buf[2] == 0xFF) {
        *kind = KOP_FMT_JPEG;
        sniff_jpeg(buf, len, hdr);
        return 0;
    }
    if (len >= 8 && memcmp(buf, "\x89PNG\r\n\x1a\n", 8) == 0) {
        *kind = KOP_FMT_PNG;
        sniff_png(buf, len, hdr);
        return 0;
    }
    if (len >= 6 && (memcmp(buf, "GIF87a", 6) == 0 || memcmp(buf, "GIF89a", 6) == 0)) {
        *kind = KOP_FMT_GIF;
        sniff_gif(buf, len, hdr);
        return 0;
    }
    if (len >= 12 && memcmp(buf, "RIFF", 4) == 0 && memcmp(buf + 8, "WEBP", 4) == 0) {
        *kind = KOP_FMT_WEBP;
        sniff_webp(buf, len, hdr);
        return 0;
    }
    if (looks_like_xml(buf, len)) {
        *kind = KOP_FMT_XML;
        return 0;
    }
    return -1;
}
