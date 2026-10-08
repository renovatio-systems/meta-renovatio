/*
 * Drawing primitives and text rendering on top of the Waveshare fonts.
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdlib.h>

#include "fonts.h"

#include "rw_internal.h"

static sFONT *const fonts[] = {
    [RW_FONT_8] = &Font8,
    [RW_FONT_12] = &Font12,
    [RW_FONT_16] = &Font16,
    [RW_FONT_20] = &Font20,
    [RW_FONT_24] = &Font24,
};

static const sFONT *get_font(rw_font font)
{
    if ((unsigned)font >= sizeof(fonts) / sizeof(fonts[0]))
        font = RW_FONT_12;
    return fonts[font];
}

void rw_fill_rect(rw_display *d, int x, int y, int w, int h, rw_color color)
{
    for (int j = y; j < y + h; j++)
        for (int i = x; i < x + w; i++)
            rw_pixel(d, i, j, color);
}

void rw_rect(rw_display *d, int x, int y, int w, int h, int thickness,
             rw_color color)
{
    if (thickness < 1)
        thickness = 1;
    rw_fill_rect(d, x, y, w, thickness, color);
    rw_fill_rect(d, x, y + h - thickness, w, thickness, color);
    rw_fill_rect(d, x, y, thickness, h, color);
    rw_fill_rect(d, x + w - thickness, y, thickness, h, color);
}

void rw_line(rw_display *d, int x0, int y0, int x1, int y1, int thickness,
             rw_color color)
{
    if (thickness < 1)
        thickness = 1;

    int dx = abs(x1 - x0), sx = x0 < x1 ? 1 : -1;
    int dy = -abs(y1 - y0), sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    int r = thickness / 2;

    for (;;) {
        if (thickness == 1)
            rw_pixel(d, x0, y0, color);
        else
            rw_fill_circle(d, x0, y0, r, color);
        if (x0 == x1 && y0 == y1)
            break;
        int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void rw_fill_circle(rw_display *d, int cx, int cy, int r, rw_color color)
{
    int rr = r * r + r; /* +r rounds the edge nicely */
    for (int y = -r; y <= r; y++)
        for (int x = -r; x <= r; x++)
            if (x * x + y * y <= rr)
                rw_pixel(d, cx + x, cy + y, color);
}

void rw_circle(rw_display *d, int cx, int cy, int r, int thickness,
               rw_color color)
{
    if (thickness < 1)
        thickness = 1;
    int outer = r * r + r;
    int ri = r - thickness;
    int inner = ri < 0 ? -1 : ri * ri + ri;
    for (int y = -r; y <= r; y++)
        for (int x = -r; x <= r; x++) {
            int dd = x * x + y * y;
            if (dd <= outer && dd > inner)
                rw_pixel(d, cx + x, cy + y, color);
        }
}

void rw_fill_polygon(rw_display *d, const rw_point *pts, size_t n,
                     rw_color color)
{
    if (n < 3)
        return;

    int ymin = pts[0].y, ymax = pts[0].y;
    for (size_t i = 1; i < n; i++) {
        if (pts[i].y < ymin)
            ymin = pts[i].y;
        if (pts[i].y > ymax)
            ymax = pts[i].y;
    }

    int *xs = malloc(n * sizeof(*xs));
    if (!xs)
        return;

    /* Even-odd scanline fill, sampling at pixel centres. */
    for (int y = ymin; y <= ymax; y++) {
        size_t cnt = 0;
        double sy = y + 0.5;
        for (size_t i = 0; i < n; i++) {
            const rw_point *a = &pts[i], *b = &pts[(i + 1) % n];
            if ((a->y <= sy && b->y > sy) || (b->y <= sy && a->y > sy)) {
                double t = (sy - a->y) / (double)(b->y - a->y);
                xs[cnt++] = (int)(a->x + t * (b->x - a->x) + 0.5);
            }
        }
        for (size_t i = 1; i < cnt; i++)
            for (size_t j = i; j > 0 && xs[j - 1] > xs[j]; j--) {
                int t = xs[j];
                xs[j] = xs[j - 1];
                xs[j - 1] = t;
            }
        for (size_t i = 0; i + 1 < cnt; i += 2)
            for (int x = xs[i]; x < xs[i + 1]; x++)
                rw_pixel(d, x, y, color);
    }
    free(xs);
}

/* The degree sign is narrower than a full character cell. */
static int degree_advance(const sFONT *f, int scale)
{
    return (f->Width * scale * 3 + 4) / 5;
}

/* Degree sign: a small ring in the upper part of the cell. */
static void draw_degree(rw_display *d, int x, int y, const sFONT *f,
                        int scale, rw_color color)
{
    int r = f->Height * scale / 8;
    if (r < 2)
        r = 2;
    int t = r / 3 > 0 ? r / 3 : 1;
    rw_circle(d, x + degree_advance(f, scale) / 2, y + r + f->Height * scale / 8,
              r, t, color);
}

static void draw_glyph(rw_display *d, int x, int y, unsigned char ch,
                       const sFONT *f, int scale, rw_color color)
{
    if (ch < ' ' || ch > '~')
        ch = '?';

    int row_bytes = (f->Width + 7) / 8;
    const uint8_t *p = &f->table[(size_t)(ch - ' ') * f->Height * row_bytes];

    for (int row = 0; row < f->Height; row++, p += row_bytes)
        for (int col = 0; col < f->Width; col++)
            if (p[col / 8] & (0x80 >> (col % 8))) {
                if (scale == 1)
                    rw_pixel(d, x + col, y + row, color);
                else
                    rw_fill_rect(d, x + col * scale, y + row * scale,
                                 scale, scale, color);
            }
}

/* Decode one character; returns bytes consumed, sets *degree for U+00B0. */
static int next_char(const char *s, unsigned char *ch, int *degree)
{
    const unsigned char *u = (const unsigned char *)s;
    *degree = 0;
    if (u[0] < 0x80) {
        *ch = u[0];
        return 1;
    }
    if (u[0] == 0xC2 && u[1] == 0xB0) {
        *degree = 1;
        *ch = ' ';
        return 2;
    }
    /* Skip the rest of any other UTF-8 sequence. */
    int len = 1;
    while ((u[len] & 0xC0) == 0x80)
        len++;
    *ch = '?';
    return len;
}

int rw_text(rw_display *d, int x, int y, const char *s, rw_font font,
            int scale, rw_color color)
{
    const sFONT *f = get_font(font);
    if (scale < 1)
        scale = 1;

    int x0 = x;
    while (*s) {
        unsigned char ch;
        int degree;
        s += next_char(s, &ch, &degree);
        if (degree) {
            draw_degree(d, x, y, f, scale, color);
            x += degree_advance(f, scale);
        } else {
            draw_glyph(d, x, y, ch, f, scale, color);
            x += f->Width * scale;
        }
    }
    return x - x0;
}

int rw_text_width(const char *s, rw_font font, int scale)
{
    const sFONT *f = get_font(font);
    if (scale < 1)
        scale = 1;

    int w = 0;
    while (*s) {
        unsigned char ch;
        int degree;
        s += next_char(s, &ch, &degree);
        w += degree ? degree_advance(f, scale) : f->Width * scale;
    }
    return w;
}

int rw_text_height(rw_font font, int scale)
{
    return get_font(font)->Height * (scale < 1 ? 1 : scale);
}
