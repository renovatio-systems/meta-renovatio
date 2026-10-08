/*
 * Panel registry, framebuffer and hardware access.
 *
 * All supported panels are Waveshare 4-colour (G) e-Paper displays. They
 * share the same driver API and framebuffer format: 2 bits per pixel,
 * 4 pixels per byte with the leftmost pixel in the top bits, and colour
 * values 0 = black, 1 = white, 2 = yellow, 3 = red.
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "DEV_Config.h"
#include "EPD_1in64g.h"
#include "EPD_2in13g.h"
#include "EPD_2in15g.h"
#include "EPD_2in36g.h"
#include "EPD_2in66g.h"
#include "EPD_3in0g.h"
#include "EPD_4in37g.h"
#include "EPD_7in3g.h"

#include "rw_internal.h"

#define DRIVER(n, W, H, P) \
    { n, W, H, P##_Init, P##_Clear, P##_Display, P##_Sleep, NULL }
#define DRIVER_PARTIAL(n, W, H, P, R) \
    { n, W, H, P##_Init, P##_Clear, P##_Display, P##_Sleep, R }

static const rw_driver drivers[] = {
    DRIVER("1in64g", EPD_1IN64G_WIDTH, EPD_1IN64G_HEIGHT, EPD_1IN64G),
    DRIVER("2in13g", EPD_2IN13G_WIDTH, EPD_2IN13G_HEIGHT, EPD_2IN13G),
    DRIVER_PARTIAL("2in15g", EPD_2IN15G_WIDTH, EPD_2IN15G_HEIGHT, EPD_2IN15G,
                   rw_epd2in15g_refresh_window),
    DRIVER("2in36g", EPD_2IN36G_WIDTH, EPD_2IN36G_HEIGHT, EPD_2IN36G),
    DRIVER("2in66g", EPD_2IN66g_WIDTH, EPD_2IN66g_HEIGHT, EPD_2IN66g),
    DRIVER("3in0g", EPD_3IN0G_WIDTH, EPD_3IN0G_HEIGHT, EPD_3IN0G),
    DRIVER("4in37g", EPD_4IN37G_WIDTH, EPD_4IN37G_HEIGHT, EPD_4IN37G),
    DRIVER("7in3g", EPD_7IN3G_WIDTH, EPD_7IN3G_HEIGHT, EPD_7IN3G),
};

#define N_DRIVERS (sizeof(drivers) / sizeof(drivers[0]))

/* Waveshare's 2-bit colour codes, indexed by rw_color. */
static const uint8_t panel_color[] = {
    [RW_BLACK] = 0x0,
    [RW_WHITE] = 0x1,
    [RW_YELLOW] = 0x2,
    [RW_RED] = 0x3,
};

static const rw_driver *find_driver(const char *type)
{
    if (!type)
        return NULL;
    for (size_t i = 0; i < N_DRIVERS; i++)
        if (strcmp(drivers[i].name, type) == 0)
            return &drivers[i];
    return NULL;
}

size_t rw_display_type_count(void)
{
    return N_DRIVERS;
}

const char *rw_display_type_name(size_t index)
{
    return index < N_DRIVERS ? drivers[index].name : NULL;
}

int rw_display_type_supported(const char *type)
{
    return find_driver(type) != NULL;
}

rw_display *rw_display_open(const char *type, int rotation, unsigned flags)
{
    const rw_driver *drv = find_driver(type);
    if (!drv) {
        fprintf(stderr, "renovatio-waveshare: unknown display type '%s'\n",
                type ? type : "(null)");
        return NULL;
    }

    if (rotation == RW_ROTATE_AUTO)
        rotation = drv->width < drv->height ? 90 : 0;
    if (rotation != 0 && rotation != 90 && rotation != 180 && rotation != 270) {
        fprintf(stderr, "renovatio-waveshare: invalid rotation %d\n", rotation);
        return NULL;
    }

    rw_display *d = calloc(1, sizeof(*d));
    if (!d)
        return NULL;

    d->drv = drv;
    d->rotation = rotation;
    if (rotation == 90 || rotation == 270) {
        d->width = drv->height;
        d->height = drv->width;
    } else {
        d->width = drv->width;
        d->height = drv->height;
    }
    d->row_bytes = (size_t)(drv->width + 3) / 4;
    d->fb_size = d->row_bytes * (size_t)drv->height;
    d->fb = malloc(d->fb_size);
    d->shown = malloc(d->fb_size);
    if (!d->fb || !d->shown) {
        free(d->fb);
        free(d->shown);
        free(d);
        return NULL;
    }
    rw_fill(d, RW_WHITE);

    if (!(flags & RW_OPEN_NO_HW)) {
        if (DEV_Module_Init() != 0) {
            fprintf(stderr, "renovatio-waveshare: hardware init failed "
                            "(running as root?)\n");
            free(d->fb);
            free(d->shown);
            free(d);
            return NULL;
        }
        d->hw = 1;
    }

    return d;
}

void rw_display_close(rw_display *d)
{
    if (!d)
        return;
    if (d->hw)
        DEV_Module_Exit();
    free(d->fb);
    free(d->shown);
    free(d);
}

const char *rw_display_type(const rw_display *d)
{
    return d->drv->name;
}

int rw_display_width(const rw_display *d)
{
    return d->width;
}

int rw_display_height(const rw_display *d)
{
    return d->height;
}

int rw_display_refresh(rw_display *d)
{
    if (d->hw) {
        d->drv->init();
        d->drv->display(d->fb);
        d->drv->sleep();
    }
    memcpy(d->shown, d->fb, d->fb_size);
    d->shown_valid = 1;
    return 0;
}

int rw_display_clear_panel(rw_display *d, rw_color color)
{
    if (!d->hw)
        return -1;
    d->drv->init();
    d->drv->clear(panel_color[color]);
    d->drv->sleep();
    uint8_t c = panel_color[color];
    memset(d->shown, (c << 6) | (c << 4) | (c << 2) | c, d->fb_size);
    d->shown_valid = 1;
    return 0;
}

/* ---- region updates ---- */

static inline void to_native(const rw_display *d, int x, int y,
                             int *nx, int *ny);

static const char *const partial_names[] = {
    [RW_PARTIAL_OFF] = "off",
    [RW_PARTIAL_PTL] = "ptl",
    [RW_PARTIAL_PTL83] = "ptl83",
    [RW_PARTIAL_WINDOW] = "window",
};

#define N_PARTIAL (sizeof(partial_names) / sizeof(partial_names[0]))

int rw_partial_method_parse(const char *name, rw_partial_method *method)
{
    for (size_t i = 0; i < N_PARTIAL; i++)
        if (name && strcmp(name, partial_names[i]) == 0) {
            *method = (rw_partial_method)i;
            return 0;
        }
    return -1;
}

const char *rw_partial_method_name(rw_partial_method method)
{
    return (unsigned)method < N_PARTIAL ? partial_names[method] : "?";
}

int rw_display_supports_partial(const rw_display *d)
{
    return d->drv->refresh_window != NULL;
}

/* Convert a native rectangle to logical coordinates (inverse rotation). */
static void native_rect_to_logical(const rw_display *d, int nx0, int ny0,
                                   int nx1, int ny1, int *x, int *y, int *w,
                                   int *h)
{
    int W = d->drv->width, H = d->drv->height;
    int lx0, ly0, lx1, ly1;

    switch (d->rotation) {
    case 90: /* nx = W-1-y, ny = x */
        lx0 = ny0; lx1 = ny1;
        ly0 = W - 1 - nx1; ly1 = W - 1 - nx0;
        break;
    case 180:
        lx0 = W - 1 - nx1; lx1 = W - 1 - nx0;
        ly0 = H - 1 - ny1; ly1 = H - 1 - ny0;
        break;
    case 270: /* nx = y, ny = H-1-x */
        lx0 = H - 1 - ny1; lx1 = H - 1 - ny0;
        ly0 = nx0; ly1 = nx1;
        break;
    default:
        lx0 = nx0; lx1 = nx1;
        ly0 = ny0; ly1 = ny1;
        break;
    }
    *x = lx0;
    *y = ly0;
    *w = lx1 - lx0 + 1;
    *h = ly1 - ly0 + 1;
}

int rw_display_changed_region(const rw_display *d, int *x, int *y, int *w,
                              int *h)
{
    if (!d->shown_valid) {
        *x = *y = 0;
        *w = d->width;
        *h = d->height;
        return 1;
    }

    int nx0 = d->drv->width, ny0 = d->drv->height, nx1 = -1, ny1 = -1;
    for (int ny = 0; ny < d->drv->height; ny++) {
        const uint8_t *a = d->fb + (size_t)ny * d->row_bytes;
        const uint8_t *b = d->shown + (size_t)ny * d->row_bytes;
        for (size_t i = 0; i < d->row_bytes; i++) {
            uint8_t diff = a[i] ^ b[i];
            if (!diff)
                continue;
            for (int p = 0; p < 4; p++)
                if (diff & (0xC0 >> (2 * p))) {
                    int nx = (int)i * 4 + p;
                    if (nx < nx0)
                        nx0 = nx;
                    if (nx > nx1)
                        nx1 = nx;
                }
            if (ny < ny0)
                ny0 = ny;
            ny1 = ny;
        }
    }
    if (nx1 < 0)
        return 0;

    native_rect_to_logical(d, nx0, ny0, nx1, ny1, x, y, w, h);
    return 1;
}

int rw_display_refresh_region(rw_display *d, rw_partial_method method,
                              int x, int y, int w, int h)
{
    if (!d->drv->refresh_window || method == RW_PARTIAL_OFF)
        return -1;

    /* Clip to the display. */
    if (x < 0) {
        w += x;
        x = 0;
    }
    if (y < 0) {
        h += y;
        y = 0;
    }
    if (x + w > d->width)
        w = d->width - x;
    if (y + h > d->height)
        h = d->height - y;
    if (w <= 0 || h <= 0)
        return 0;

    /* Native rectangle from two opposite corners. */
    int ax, ay, bx, by;
    to_native(d, x, y, &ax, &ay);
    to_native(d, x + w - 1, y + h - 1, &bx, &by);
    int nx0 = ax < bx ? ax : bx, nx1 = ax < bx ? bx : ax;
    int ny0 = ay < by ? ay : by, ny1 = ay < by ? by : ay;

    /* Controllers address columns in groups of 8 pixels. */
    nx0 &= ~7;
    nx1 |= 7;
    if (nx1 >= d->drv->width)
        nx1 = d->drv->width - 1;

    if (d->hw && d->drv->refresh_window(d->fb, d->row_bytes, nx0, ny0,
                                        nx1 - nx0 + 1, ny1 - ny0 + 1,
                                        method) < 0)
        return -1;

    /* Only the window changed on the panel; nx0 and nx1+1 are byte aligned. */
    if (d->shown_valid)
        for (int ny = ny0; ny <= ny1; ny++) {
            size_t off = (size_t)ny * d->row_bytes + (size_t)nx0 / 4;
            memcpy(d->shown + off, d->fb + off, (size_t)(nx1 - nx0 + 1) / 4);
        }
    return 0;
}

/* Map logical coordinates to the native framebuffer. */
static inline void to_native(const rw_display *d, int x, int y,
                             int *nx, int *ny)
{
    switch (d->rotation) {
    case 90:
        *nx = d->drv->width - 1 - y;
        *ny = x;
        break;
    case 180:
        *nx = d->drv->width - 1 - x;
        *ny = d->drv->height - 1 - y;
        break;
    case 270:
        *nx = y;
        *ny = d->drv->height - 1 - x;
        break;
    default:
        *nx = x;
        *ny = y;
        break;
    }
}

void rw_pixel(rw_display *d, int x, int y, rw_color color)
{
    if (x < 0 || y < 0 || x >= d->width || y >= d->height)
        return;

    int nx, ny;
    to_native(d, x, y, &nx, &ny);

    uint8_t *p = &d->fb[(size_t)ny * d->row_bytes + (size_t)nx / 4];
    int shift = 6 - 2 * (nx % 4);
    *p = (uint8_t)((*p & ~(0x3 << shift)) | (panel_color[color] << shift));
}

static rw_color get_pixel(const rw_display *d, int x, int y)
{
    int nx, ny;
    to_native(d, x, y, &nx, &ny);

    uint8_t v = d->fb[(size_t)ny * d->row_bytes + (size_t)nx / 4];
    v = (v >> (6 - 2 * (nx % 4))) & 0x3;
    for (int c = RW_BLACK; c <= RW_RED; c++)
        if (panel_color[c] == v)
            return (rw_color)c;
    return RW_WHITE;
}

void rw_fill(rw_display *d, rw_color color)
{
    uint8_t c = panel_color[color];
    memset(d->fb, (c << 6) | (c << 4) | (c << 2) | c, d->fb_size);
}

int rw_display_save_ppm(const rw_display *d, const char *path)
{
    static const uint8_t rgb[][3] = {
        [RW_BLACK] = { 0, 0, 0 },
        [RW_WHITE] = { 255, 255, 255 },
        [RW_YELLOW] = { 255, 215, 0 },
        [RW_RED] = { 200, 20, 20 },
    };

    FILE *f = fopen(path, "wb");
    if (!f)
        return -1;
    fprintf(f, "P6\n%d %d\n255\n", d->width, d->height);
    for (int y = 0; y < d->height; y++)
        for (int x = 0; x < d->width; x++)
            fwrite(rgb[get_pixel(d, x, y)], 1, 3, f);
    return fclose(f) == 0 ? 0 : -1;
}
