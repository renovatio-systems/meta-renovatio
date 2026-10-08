/*
 * Outline font rendering with FreeType.
 *
 * The panel cannot show grey levels, so glyphs must end up as 1-bit:
 *  - small sizes use FreeType's monochrome rendering with full hinting,
 *    which snaps stems to the pixel grid and keeps stroke widths even;
 *  - large sizes are rendered as 8-bit coverage with light hinting and
 *    thresholded, which keeps their curves smooth.
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdio.h>
#include <stdlib.h>

#include <ft2build.h>
#include FT_FREETYPE_H

#include "rw_internal.h"

/* Coverage (0-255) at or above which a pixel is set. */
#ifndef RW_TTF_THRESHOLD
#define RW_TTF_THRESHOLD 112
#endif

/* Sizes up to this many pixels use hinted monochrome rendering. */
#ifndef RW_TTF_MONO_MAX_PX
#define RW_TTF_MONO_MAX_PX 24
#endif

struct rw_ttf {
    FT_Library lib;
    FT_Face face;
    int px;
};

rw_ttf *rw_ttf_open(const char *path)
{
    rw_ttf *f = calloc(1, sizeof(*f));
    if (!f)
        return NULL;

    if (FT_Init_FreeType(&f->lib)) {
        free(f);
        return NULL;
    }
    if (FT_New_Face(f->lib, path, 0, &f->face)) {
        fprintf(stderr, "renovatio-waveshare: cannot load font %s\n", path);
        FT_Done_FreeType(f->lib);
        free(f);
        return NULL;
    }
    FT_Select_Charmap(f->face, FT_ENCODING_UNICODE);
    return f;
}

void rw_ttf_close(rw_ttf *f)
{
    if (!f)
        return;
    FT_Done_Face(f->face);
    FT_Done_FreeType(f->lib);
    free(f);
}

static void set_size(rw_ttf *f, int px)
{
    if (px < 1)
        px = 1;
    if (f->px != px) {
        FT_Set_Pixel_Sizes(f->face, 0, (FT_UInt)px);
        f->px = px;
    }
}

/* Decode one UTF-8 code point; malformed input yields U+FFFD. */
static uint32_t next_codepoint(const char **sp)
{
    const unsigned char *s = (const unsigned char *)*sp;
    uint32_t cp;
    int extra;

    if (s[0] < 0x80) {
        cp = s[0];
        extra = 0;
    } else if ((s[0] & 0xE0) == 0xC0) {
        cp = s[0] & 0x1F;
        extra = 1;
    } else if ((s[0] & 0xF0) == 0xE0) {
        cp = s[0] & 0x0F;
        extra = 2;
    } else if ((s[0] & 0xF8) == 0xF0) {
        cp = s[0] & 0x07;
        extra = 3;
    } else {
        *sp += 1;
        return 0xFFFD;
    }

    int i;
    for (i = 1; i <= extra; i++) {
        if ((s[i] & 0xC0) != 0x80) {
            *sp += i;
            return 0xFFFD;
        }
        cp = (cp << 6) | (s[i] & 0x3F);
    }
    *sp += i;
    return cp;
}

/*
 * Lay out s with its pen starting at (x, y = baseline). Draws when d is
 * non-NULL; always accumulates the ink extents. Returns the advance width.
 */
static int layout(rw_display *d, rw_ttf *f, int px, int x, int y,
                  const char *s, rw_color color, rw_text_extents *e)
{
    set_size(f, px);

    FT_Face face = f->face;
    FT_Int32 load = FT_LOAD_RENDER | (px <= RW_TTF_MONO_MAX_PX
                                          ? FT_LOAD_TARGET_MONO
                                          : FT_LOAD_TARGET_LIGHT);
    FT_UInt prev = 0;
    int pen = 0;
    int ascent = 0, descent = 0;

    while (*s) {
        uint32_t cp = next_codepoint(&s);
        FT_UInt idx = FT_Get_Char_Index(face, cp);

        if (prev && idx && FT_HAS_KERNING(face)) {
            FT_Vector k;
            if (FT_Get_Kerning(face, prev, idx, FT_KERNING_DEFAULT, &k) == 0)
                pen += (int)((k.x + 32) >> 6);
        }
        prev = idx;

        if (FT_Load_Glyph(face, idx, load))
            continue;

        FT_GlyphSlot g = face->glyph;
        const FT_Bitmap *bm = &g->bitmap;
        int gx = x + pen + g->bitmap_left;
        int gy = y - g->bitmap_top;

        for (unsigned row = 0; row < bm->rows; row++) {
            const unsigned char *line = bm->buffer + (long)row * bm->pitch;
            for (unsigned col = 0; col < bm->width; col++) {
                int on;
                if (bm->pixel_mode == FT_PIXEL_MODE_MONO)
                    on = line[col / 8] & (0x80 >> (col % 8));
                else
                    on = line[col] >= RW_TTF_THRESHOLD;
                if (!on)
                    continue;
                if (d)
                    rw_pixel(d, gx + (int)col, gy + (int)row, color);
                int up = g->bitmap_top - (int)row;
                if (up > ascent)
                    ascent = up;
                if (1 - up > descent)
                    descent = 1 - up;
            }
        }

        pen += (int)((g->advance.x + 32) >> 6);
    }

    if (e) {
        e->width = pen;
        e->ascent = ascent;
        e->descent = descent;
    }
    return pen;
}

int rw_ttf_text(rw_display *d, rw_ttf *f, int px, int x, int y,
                const char *s, rw_color color)
{
    return layout(d, f, px, x, y, s, color, NULL);
}

void rw_ttf_measure(rw_ttf *f, int px, const char *s, rw_text_extents *e)
{
    layout(NULL, f, px, 0, 0, s, RW_BLACK, e);
}

void rw_ttf_line_metrics(rw_ttf *f, int px, int *ascent, int *descent)
{
    set_size(f, px);
    const FT_Size_Metrics *m = &f->face->size->metrics;
    if (ascent)
        *ascent = (int)((m->ascender + 63) >> 6);
    if (descent)
        *descent = (int)((-m->descender + 63) >> 6);
}
