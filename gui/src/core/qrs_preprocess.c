#include "qrs_preprocess.h"

#include <string.h>

void qrs_pp_pack(const uint8_t *src, int width, int height, int stride, uint8_t *dst)
{
    for (int y = 0; y < height; y++)
        memcpy(dst + (size_t)y * width, src + (size_t)y * stride, (size_t)width);
}

int qrs_pp_contrast(const uint8_t *src, uint8_t *dst, int width, int height)
{
    size_t n = (size_t)width * height;
    size_t hist[256] = {0};
    for (size_t i = 0; i < n; i++)
        hist[src[i]]++;

    size_t clip = n / 100;
    size_t acc = 0;
    int lo = 0, hi = 255;
    for (lo = 0; lo < 255; lo++) {
        acc += hist[lo];
        if (acc > clip)
            break;
    }
    acc = 0;
    for (hi = 255; hi > 0; hi--) {
        acc += hist[hi];
        if (acc > clip)
            break;
    }

    if (hi - lo < 8)
        return 0; /* flat image: stretching would only amplify noise */
    if (lo <= 4 && hi >= 251)
        return 0; /* already full range */

    uint8_t lut[256];
    int range = hi - lo;
    for (int v = 0; v < 256; v++) {
        int s = (v - lo) * 255 / range;
        lut[v] = (uint8_t)(s < 0 ? 0 : (s > 255 ? 255 : s));
    }
    for (size_t i = 0; i < n; i++)
        dst[i] = lut[src[i]];
    return 1;
}

void qrs_pp_invert(const uint8_t *src, uint8_t *dst, int width, int height)
{
    size_t n = (size_t)width * height;
    for (size_t i = 0; i < n; i++)
        dst[i] = (uint8_t)(255 - src[i]);
}

void qrs_pp_upscale2(const uint8_t *src, int width, int height, uint8_t *dst)
{
    int dw = width * 2;
    for (int y = 0; y < height * 2; y++) {
        /* Sample positions sit at quarter-pixel offsets: weights 3:1 / 1:3. */
        int sy = y / 2;
        int ny = (y & 1) ? sy + 1 : sy - 1;
        if (ny < 0) ny = 0;
        if (ny >= height) ny = height - 1;
        const uint8_t *r0 = src + (size_t)sy * width;
        const uint8_t *r1 = src + (size_t)ny * width;
        uint8_t *d = dst + (size_t)y * dw;
        for (int x = 0; x < dw; x++) {
            int sx = x / 2;
            int nx = (x & 1) ? sx + 1 : sx - 1;
            if (nx < 0) nx = 0;
            if (nx >= width) nx = width - 1;
            int v = 9 * r0[sx] + 3 * r0[nx] + 3 * r1[sx] + r1[nx];
            d[x] = (uint8_t)((v + 8) >> 4);
        }
    }
}

void qrs_pp_downscale(const uint8_t *src, int width, int height, int factor, uint8_t *dst)
{
    int dw = width / factor;
    int dh = height / factor;
    unsigned area = (unsigned)(factor * factor);
    for (int y = 0; y < dh; y++) {
        for (int x = 0; x < dw; x++) {
            unsigned sum = 0;
            for (int j = 0; j < factor; j++) {
                const uint8_t *row = src + (size_t)(y * factor + j) * width + (size_t)x * factor;
                for (int i = 0; i < factor; i++)
                    sum += row[i];
            }
            dst[(size_t)y * dw + x] = (uint8_t)((sum + area / 2) / area);
        }
    }
}

void qrs_pp_blur3(const uint8_t *src, uint8_t *dst, int width, int height)
{
    for (int y = 0; y < height; y++) {
        int y0 = y > 0 ? y - 1 : y;
        int y1 = y < height - 1 ? y + 1 : y;
        const uint8_t *a = src + (size_t)y0 * width;
        const uint8_t *b = src + (size_t)y * width;
        const uint8_t *c = src + (size_t)y1 * width;
        uint8_t *d = dst + (size_t)y * width;
        for (int x = 0; x < width; x++) {
            int x0 = x > 0 ? x - 1 : x;
            int x1 = x < width - 1 ? x + 1 : x;
            int sum = a[x0] + a[x] + a[x1] + b[x0] + b[x] + b[x1] + c[x0] + c[x] + c[x1];
            d[x] = (uint8_t)((sum + 4) / 9);
        }
    }
}
