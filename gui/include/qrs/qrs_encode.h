/* Pixelbridge - C ABI of the Rust encoder (rust/qrs_encode, built as qrs_encode.dll).
 * Keep in sync with rust/qrs_encode/src/lib.rs. */
#ifndef QRS_ENCODE_H
#define QRS_ENCODE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define QRS_ENCODE_ABI_VERSION 1u
#define QRS_ENCODE_MAX_SIZE    177 /* modules per side of a version-40 symbol */

enum {
    QRS_ECL_L = 0, /* ~7% recovery */
    QRS_ECL_M = 1, /* ~15% */
    QRS_ECL_Q = 2, /* ~25% */
    QRS_ECL_H = 3  /* ~30% */
};

enum {
    QRS_ENC_ERR_NULL     = -1,
    QRS_ENC_ERR_ECL      = -2,
    QRS_ENC_ERR_TOO_LONG = -3,
    QRS_ENC_ERR_BUFFER   = -4,
    QRS_ENC_ERR_INTERNAL = -99
};

uint32_t qrs_encode_abi_version(void);

/* Writes one byte per module (1 = dark, row-major, no quiet zone) into `out`.
 * Returns the side length in modules (21..177) or a negative QRS_ENC_ERR_*. */
int32_t qrs_encode(const uint8_t *data, size_t len, int32_t ecl,
                   uint8_t *out, size_t out_cap);

#ifdef __cplusplus
}
#endif

#endif /* QRS_ENCODE_H */
