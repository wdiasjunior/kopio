/* kopio core types — pure C11, no Qt, no POSIX, no paths. */
#ifndef KOP_TYPES_H
#define KOP_TYPES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    KOP_FMT_UNKNOWN = 0,
    KOP_FMT_JPEG,
    KOP_FMT_PNG,
    KOP_FMT_GIF,
    KOP_FMT_WEBP,
    KOP_FMT_XML,
    KOP_FMT_NOMEDIA
} KopFileKind;

typedef enum {
    KOP_CAT_UNSCANNED = 0,
    KOP_CAT_JUNK_FILE,   /* .nomedia / ComicInfo.xml and friends */
    KOP_CAT_EXACT_DUPE,  /* byte-identical copy elsewhere */
    KOP_CAT_JUNK_PAGE,   /* heuristic/frequency verdict: junk */
    KOP_CAT_REVIEW,      /* suspicious but needs a human */
    KOP_CAT_CLEAN
} KopCategory;

typedef enum {
    KOP_ST_PENDING = 0,
    KOP_ST_ALLOWED,
    KOP_ST_REVIEW,
    KOP_ST_TRASHED
} KopUserStatus;

/* reason bitflags, surfaced as UI tooltips */
#define KOP_R_NOMEDIA      (1u << 0)
#define KOP_R_XML          (1u << 1)
#define KOP_R_EXACT_DUPE   (1u << 2)
#define KOP_R_FREQ_CLUSTER (1u << 3)  /* similarity cluster spans many chapters */
#define KOP_R_WIDE_BANNER  (1u << 4)
#define KOP_R_SQUARE       (1u << 5)
#define KOP_R_BLANK        (1u << 6)
#define KOP_R_TITLE_ONLY   (1u << 7)
#define KOP_R_TEXT_HEAVY   (1u << 8)  /* translation-notes shape */
#define KOP_R_COLOR_GUARD  (1u << 9)  /* looks like a cover/spread — protected */
#define KOP_R_FLAT_COLOR   (1u << 10) /* flat color art, few colors, little ink */

typedef struct {
    uint32_t width, height;
    uint8_t channels;      /* declared channels from the header, 0 if unknown */
    uint8_t is_grayscale;  /* header says no color channels */
    uint8_t has_alpha;
} KopImageHeader;

typedef struct {
    float ink_ratio;    /* fraction of pixels with gray < 200 */
    float white_ratio;  /* gray >= 245 */
    float black_ratio;  /* gray <= 10 */
    float edge_density; /* mean gradient magnitude / 255 on ~256px-wide gray */
    int   unique_colors;/* RGB444-quantized, capped at 4096 */
    float border_white; /* whiteness of the outer 5% frame */
    uint8_t is_color;   /* pixel-verified chroma present */
} KopPixelMetrics;

typedef struct {
    int32_t id;          /* index assigned by the caller; paths stay outside the core */
    int32_t series_id;
    int32_t chapter_id;
    KopFileKind kind;
    uint64_t file_size;
    KopImageHeader hdr;
    uint8_t decoded;     /* pixel analysis (dhash + metrics) is valid */
    uint64_t chash_lo, chash_hi; /* XXH3-128 of the file bytes */
    uint64_t dhash;
    KopPixelMetrics m;
    /* outputs of kop_classify: */
    KopCategory category;
    uint32_t reasons;
    float score;         /* 0..1 junk confidence */
    int32_t dupe_group;  /* -1 = none */
    int32_t sim_cluster; /* -1 = none */
} KopRecord;

#ifdef __cplusplus
}
#endif

#endif /* KOP_TYPES_H */
