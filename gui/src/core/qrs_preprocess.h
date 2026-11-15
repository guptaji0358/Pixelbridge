/* Internal image helpers for the decoding core. All buffers are tightly
 * packed 8-bit grayscale (stride == width). Not part of the public API. */
#ifndef QRS_PREPROCESS_H
#define QRS_PREPROCESS_H

#include <stdint.h>

/* Copies a strided image into a packed buffer of width * height bytes. */
void qrs_pp_pack(const uint8_t *src, int width, int height, int stride, uint8_t *dst);

/* Stretches the 1st..99th percentile range to 0..255.
 * Returns 0 (dst untouched) when the image already uses the full range or is
 * essentially flat, so the caller can skip a pointless pass. */
int qrs_pp_contrast(const uint8_t *src, uint8_t *dst, int width, int height);

void qrs_pp_invert(const uint8_t *src, uint8_t *dst, int width, int height);

/* dst must hold (2 * width) * (2 * height) bytes. */
void qrs_pp_upscale2(const uint8_t *src, int width, int height, uint8_t *dst);

/* Box-filter reduction by an integer factor >= 2.
 * dst must hold (width / factor) * (height / factor) bytes. */
void qrs_pp_downscale(const uint8_t *src, int width, int height, int factor, uint8_t *dst);

void qrs_pp_blur3(const uint8_t *src, uint8_t *dst, int width, int height);

#endif /* QRS_PREPROCESS_H */
