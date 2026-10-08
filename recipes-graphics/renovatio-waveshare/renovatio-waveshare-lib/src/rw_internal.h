/*
 * SPDX-License-Identifier: MIT
 */
#ifndef RW_INTERNAL_H
#define RW_INTERNAL_H

#include <stdint.h>

#include "renovatio-waveshare.h"

typedef struct {
    const char *name;
    int width;   /* native panel width, in pixels */
    int height;  /* native panel height, in pixels */
    void (*init)(void);
    void (*clear)(uint8_t color);
    void (*display)(uint8_t *image);
    void (*sleep)(void);
    /* Optional, experimental: refresh a native-coordinate window. */
    int (*refresh_window)(const uint8_t *fb, size_t row_bytes, int x, int y,
                          int w, int h, rw_partial_method method);
} rw_driver;

struct rw_display {
    const rw_driver *drv;
    int rotation;
    int width;           /* logical */
    int height;          /* logical */
    size_t row_bytes;    /* native row stride, 4 pixels per byte */
    uint8_t *fb;
    uint8_t *shown;      /* what the panel currently shows */
    int shown_valid;
    size_t fb_size;
    int hw;              /* hardware initialised */
};

int rw_epd2in15g_refresh_window(const uint8_t *fb, size_t row_bytes, int x,
                                int y, int w, int h, rw_partial_method method);

#endif /* RW_INTERNAL_H */
