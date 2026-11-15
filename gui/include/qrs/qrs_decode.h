/* Pixelbridge - C decoding core (wraps the vendored quirc library).
 *
 * Pure C, no Qt and no global state: safe to call from several threads at once
 * as long as each call uses its own output buffers.
 */
#ifndef QRS_DECODE_H
#define QRS_DECODE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define QRS_MAX_PAYLOAD 8896 /* matches QUIRC_MAX_PAYLOAD */

/* Preprocessing strategies, tried in ascending bit order. */
enum {
    QRS_STRAT_RAW       = 1u << 0, /* untouched grayscale */
    QRS_STRAT_CONTRAST  = 1u << 1, /* percentile histogram stretch */
    QRS_STRAT_INVERT    = 1u << 2, /* light-on-dark symbols */
    QRS_STRAT_UPSCALE   = 1u << 3, /* 2x bilinear, small images only */
    QRS_STRAT_DOWNSCALE = 1u << 4, /* box filter, large images only */
    QRS_STRAT_BLUR      = 1u << 5, /* 3x3 box blur against noise / moire */
    QRS_STRAT_ALL       = 0x3Fu
};

/* Error codes (negative return values). */
enum {
    QRS_DEC_ERR_ARG    = -1,
    QRS_DEC_ERR_MEMORY = -2
};

typedef struct qrs_point {
    int x;
    int y;
} qrs_point;

typedef struct qrs_result {
    /* 0 = decoded. Non-zero = a QR-like region was located but could not be
     * decoded (value is the quirc error code); payload_len is 0 in that case. */
    int status;
    qrs_point corners[4]; /* in original image pixel coordinates */
    int version;          /* 1..40 */
    int ecc_level;        /* 0 = L, 1 = M, 2 = Q, 3 = H */
    int mask;
    int data_type;        /* highest-valued mode: 1 num, 2 alnum, 4 byte, 8 kanji */
    uint32_t eci;
    unsigned strategy;    /* QRS_STRAT_* bit that produced this result */
    int mirrored;         /* 1 if the symbol had to be flipped to decode */
    int payload_len;
    uint8_t payload[QRS_MAX_PAYLOAD]; /* NUL-terminated for convenience */
} qrs_result;

typedef struct qrs_scan_stats {
    unsigned strategies_tried; /* bitmask */
    unsigned winning_strategy; /* 0 when nothing decoded */
    int candidates;            /* QR-like regions seen across all passes */
} qrs_scan_stats;

/* Scans an 8-bit grayscale image for QR codes.
 *
 *   gray/stride   pixel rows, `stride` bytes apart (stride >= width)
 *   strategies    QRS_STRAT_* mask; scanning stops at the first strategy
 *                 that decodes at least one symbol
 *   out           array of `max_results` entries
 *   stats         optional (may be NULL)
 *
 * Returns the number of entries written (decoded symbols first; undecodable
 * candidates are only reported when nothing decoded), or a negative
 * QRS_DEC_ERR_* code. The input buffer is never modified. */
int qrs_decode_gray(const uint8_t *gray, int width, int height, int stride,
                    unsigned strategies, qrs_result *out, int max_results,
                    qrs_scan_stats *stats);

/* Human-readable name for a single QRS_STRAT_* bit. */
const char *qrs_strategy_name(unsigned strategy);

/* Human-readable text for a non-zero qrs_result.status. */
const char *qrs_status_text(int status);

#ifdef __cplusplus
}
#endif

#endif /* QRS_DECODE_H */
