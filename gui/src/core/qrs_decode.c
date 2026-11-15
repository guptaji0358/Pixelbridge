#include "qrs/qrs_decode.h"

#include "qrs_preprocess.h"
#include "quirc.h"

#include <stdlib.h>
#include <string.h>

#define UPSCALE_MAX_DIM   1200 /* only enlarge images up to this size */
#define DOWNSCALE_MIN_DIM 1600 /* only shrink images larger than this */
#define MAX_FAILED        8

typedef struct scan_ctx {
    qrs_result *out;
    int max_results;
    int decoded;
    qrs_result *failed; /* heap: undecodable candidates of the first pass that saw any */
    int failed_count;
    int candidates;
} scan_ctx;

/* Runs quirc over one packed buffer. Corner coordinates are mapped back to
 * the original image with orig = coord * mul / div.
 * Returns the number of symbols decoded in this pass, or a negative error. */
static int run_pass(scan_ctx *ctx, const uint8_t *buf, int w, int h,
                    int mul, int div, unsigned strategy)
{
    struct quirc *q = quirc_new();
    if (!q)
        return QRS_DEC_ERR_MEMORY;
    if (quirc_resize(q, w, h) < 0) {
        quirc_destroy(q);
        return QRS_DEC_ERR_MEMORY;
    }

    uint8_t *dst = quirc_begin(q, NULL, NULL);
    memcpy(dst, buf, (size_t)w * h);
    quirc_end(q);

    struct quirc_code *code = malloc(sizeof(*code));
    struct quirc_data *data = malloc(sizeof(*data));
    if (!code || !data) {
        free(code);
        free(data);
        quirc_destroy(q);
        return QRS_DEC_ERR_MEMORY;
    }

    int found = 0;
    int keep_failed = ctx->failed_count == 0;
    int count = quirc_count(q);
    ctx->candidates += count;

    for (int i = 0; i < count; i++) {
        quirc_extract(q, i, code);
        int mirrored = 0;
        quirc_decode_error_t err = quirc_decode(code, data);
        if (err == QUIRC_ERROR_DATA_ECC) {
            quirc_flip(code);
            err = quirc_decode(code, data);
            mirrored = (err == QUIRC_SUCCESS);
        }

        qrs_result *r = NULL;
        if (err == QUIRC_SUCCESS) {
            if (ctx->decoded >= ctx->max_results)
                break;
            r = &ctx->out[ctx->decoded++];
            found++;
        } else if (keep_failed && ctx->failed_count < MAX_FAILED) {
            r = &ctx->failed[ctx->failed_count++];
        } else {
            continue;
        }

        memset(r, 0, sizeof(*r));
        r->strategy = strategy;
        for (int c = 0; c < 4; c++) {
            r->corners[c].x = code->corners[c].x * mul / div;
            r->corners[c].y = code->corners[c].y * mul / div;
        }
        if (err == QUIRC_SUCCESS) {
            r->status = 0;
            r->version = data->version;
            /* quirc reports the raw format bits (M=0, L=1, H=2, Q=3). */
            static const int ecc_map[4] = {1, 0, 3, 2};
            r->ecc_level = ecc_map[data->ecc_level & 3];
            r->mask = data->mask;
            r->data_type = data->data_type;
            r->eci = data->eci;
            r->mirrored = mirrored;
            r->payload_len = data->payload_len;
            memcpy(r->payload, data->payload, (size_t)data->payload_len);
            r->payload[data->payload_len < QRS_MAX_PAYLOAD ? data->payload_len : QRS_MAX_PAYLOAD - 1] = 0;
        } else {
            r->status = (int)err;
        }
    }

    free(code);
    free(data);
    quirc_destroy(q);
    return found;
}

int qrs_decode_gray(const uint8_t *gray, int width, int height, int stride,
                    unsigned strategies, qrs_result *out, int max_results,
                    qrs_scan_stats *stats)
{
    if (stats)
        memset(stats, 0, sizeof(*stats));
    if (!gray || !out || width < 1 || height < 1 || stride < width || max_results < 1)
        return QRS_DEC_ERR_ARG;
    if (width > 32767 || height > 32767)
        return QRS_DEC_ERR_ARG;
    if (!(strategies & QRS_STRAT_ALL))
        strategies = QRS_STRAT_RAW;

    size_t n = (size_t)width * height;
    int max_dim = width > height ? width : height;
    int can_up = max_dim <= UPSCALE_MAX_DIM;
    int down_factor = max_dim > DOWNSCALE_MIN_DIM ? (max_dim + DOWNSCALE_MIN_DIM - 1) / DOWNSCALE_MIN_DIM : 0;

    uint8_t *packed = malloc(n);
    uint8_t *work = malloc(can_up ? n * 4 : n);
    scan_ctx ctx = {out, max_results, 0, NULL, 0, 0};
    ctx.failed = calloc(MAX_FAILED, sizeof(qrs_result));
    if (!packed || !work || !ctx.failed) {
        free(packed);
        free(work);
        free(ctx.failed);
        return QRS_DEC_ERR_MEMORY;
    }
    qrs_pp_pack(gray, width, height, stride, packed);

    unsigned tried = 0, winner = 0;
    int rc = 0;

    for (unsigned bit = 1; bit <= QRS_STRAT_BLUR && rc == 0; bit <<= 1) {
        if (!(strategies & bit))
            continue;

        switch (bit) {
        case QRS_STRAT_RAW:
            rc = run_pass(&ctx, packed, width, height, 1, 1, bit);
            break;
        case QRS_STRAT_CONTRAST:
            if (!qrs_pp_contrast(packed, work, width, height))
                continue;
            rc = run_pass(&ctx, work, width, height, 1, 1, bit);
            break;
        case QRS_STRAT_INVERT:
            qrs_pp_invert(packed, work, width, height);
            rc = run_pass(&ctx, work, width, height, 1, 1, bit);
            break;
        case QRS_STRAT_UPSCALE:
            if (!can_up)
                continue;
            qrs_pp_upscale2(packed, width, height, work);
            rc = run_pass(&ctx, work, width * 2, height * 2, 1, 2, bit);
            break;
        case QRS_STRAT_DOWNSCALE:
            if (down_factor < 2)
                continue;
            qrs_pp_downscale(packed, width, height, down_factor, work);
            rc = run_pass(&ctx, work, width / down_factor, height / down_factor, down_factor, 1, bit);
            break;
        case QRS_STRAT_BLUR:
            qrs_pp_blur3(packed, work, width, height);
            rc = run_pass(&ctx, work, width, height, 1, 1, bit);
            break;
        default:
            continue;
        }

        tried |= bit;
        if (rc > 0)
            winner = bit;
    }

    int total;
    if (rc < 0) {
        total = rc;
    } else {
        total = ctx.decoded;
        if (total == 0) {
            /* Nothing decoded: report where QR-like regions were seen. */
            int k = ctx.failed_count < max_results ? ctx.failed_count : max_results;
            memcpy(out, ctx.failed, (size_t)k * sizeof(qrs_result));
            total = k;
        }
    }

    if (stats) {
        stats->strategies_tried = tried;
        stats->winning_strategy = winner;
        stats->candidates = ctx.candidates;
    }
    free(packed);
    free(work);
    free(ctx.failed);
    return total;
}

const char *qrs_strategy_name(unsigned strategy)
{
    switch (strategy) {
    case QRS_STRAT_RAW:       return "raw";
    case QRS_STRAT_CONTRAST:  return "contrast";
    case QRS_STRAT_INVERT:    return "invert";
    case QRS_STRAT_UPSCALE:   return "upscale";
    case QRS_STRAT_DOWNSCALE: return "downscale";
    case QRS_STRAT_BLUR:      return "blur";
    default:                  return "none";
    }
}

const char *qrs_status_text(int status)
{
    if (status == 0)
        return "decoded";
    return quirc_strerror((quirc_decode_error_t)status);
}
