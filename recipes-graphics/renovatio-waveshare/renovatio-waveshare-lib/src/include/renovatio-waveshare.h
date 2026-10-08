/*
 * renovatio-waveshare - display abstraction for Waveshare 4-colour (G)
 * e-Paper panels on the Raspberry Pi.
 *
 * Drawing happens into an in-memory framebuffer in logical (rotated)
 * coordinates; rw_display_refresh() pushes it to the panel. The panel is
 * put to deep sleep after every refresh, as Waveshare recommends.
 *
 * SPDX-License-Identifier: MIT
 */
#ifndef RENOVATIO_WAVESHARE_H
#define RENOVATIO_WAVESHARE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    RW_BLACK = 0,
    RW_WHITE,
    RW_YELLOW,
    RW_RED,
} rw_color;

typedef enum {
    RW_FONT_8 = 0,  /*  5 x  8 */
    RW_FONT_12,     /*  7 x 12 */
    RW_FONT_16,     /* 11 x 16 */
    RW_FONT_20,     /* 14 x 20 */
    RW_FONT_24,     /* 17 x 24 */
} rw_font;

/* Rotation value that picks landscape orientation for the panel. */
#define RW_ROTATE_AUTO (-1)

/* rw_display_open() flags */
/*
 * Framebuffer only, no GPIO/SPI access. Refreshes then only update the
 * library's record of what the panel shows, which is useful for testing.
 */
#define RW_OPEN_NO_HW 0x1

typedef struct rw_display rw_display;

typedef struct {
    int x;
    int y;
} rw_point;

/* Supported panel types, e.g. "2in15g". */
size_t rw_display_type_count(void);
const char *rw_display_type_name(size_t index);
int rw_display_type_supported(const char *type);

/*
 * Open a panel. rotation is 0, 90, 180, 270 or RW_ROTATE_AUTO.
 * Returns NULL on error (unknown type, bad rotation, hardware init failure).
 */
rw_display *rw_display_open(const char *type, int rotation, unsigned flags);
void rw_display_close(rw_display *d);

const char *rw_display_type(const rw_display *d);
int rw_display_width(const rw_display *d);  /* logical, after rotation */
int rw_display_height(const rw_display *d);

/* Push the framebuffer to the panel (full refresh), then sleep the panel. */
int rw_display_refresh(rw_display *d);
/* Fill the physical panel with one colour, then sleep the panel. */
int rw_display_clear_panel(rw_display *d, rw_color color);

/*
 * Region updates.
 *
 * The library remembers what was last pushed to the panel, so it can tell
 * which part of the framebuffer has changed since.
 *
 * None of Waveshare's 4-colour panels officially support partial refresh.
 * rw_display_refresh_region() therefore offers experimental methods that
 * drive the controller's window commands directly; whether any of them
 * works depends on the panel. Only the 2in15g implements them.
 */
typedef enum {
    RW_PARTIAL_OFF = 0,
    RW_PARTIAL_PTL,     /* partial in 0x91, window 0x90, partial out 0x92 */
    RW_PARTIAL_PTL83,   /* window command 0x83 */
    RW_PARTIAL_WINDOW,  /* temporary resolution 0x61 + start offset 0x65 */
} rw_partial_method;

int rw_partial_method_parse(const char *name, rw_partial_method *method);
const char *rw_partial_method_name(rw_partial_method method);
int rw_display_supports_partial(const rw_display *d);

/*
 * Bounding box (logical coordinates) of pixels that differ from what is on
 * the panel. Returns 1 and fills the box if anything changed, 0 otherwise.
 * Before the first refresh the whole display counts as changed.
 */
int rw_display_changed_region(const rw_display *d, int *x, int *y, int *w,
                              int *h);

/*
 * Refresh only the given rectangle (logical coordinates; it may be widened
 * to suit the controller), then sleep the panel. Returns -1 if the panel
 * or method is not supported.
 */
int rw_display_refresh_region(rw_display *d, rw_partial_method method,
                              int x, int y, int w, int h);

/* Write the framebuffer as a binary PPM (P6) image, in logical orientation. */
int rw_display_save_ppm(const rw_display *d, const char *path);

/* Drawing. Everything is clipped to the display. */
void rw_fill(rw_display *d, rw_color color);
void rw_pixel(rw_display *d, int x, int y, rw_color color);
void rw_line(rw_display *d, int x0, int y0, int x1, int y1, int thickness,
             rw_color color);
void rw_rect(rw_display *d, int x, int y, int w, int h, int thickness,
             rw_color color);
void rw_fill_rect(rw_display *d, int x, int y, int w, int h, rw_color color);
void rw_circle(rw_display *d, int cx, int cy, int r, int thickness,
               rw_color color);
void rw_fill_circle(rw_display *d, int cx, int cy, int r, rw_color color);
void rw_fill_polygon(rw_display *d, const rw_point *pts, size_t n,
                     rw_color color);

/*
 * Text. Only printable ASCII is available, plus the UTF-8 degree sign
 * (U+00B0). Other characters are drawn as '?'. scale is an integer
 * magnification (1 = native font size). Text is drawn with a transparent
 * background; rw_text returns the width drawn.
 */
int rw_text(rw_display *d, int x, int y, const char *s, rw_font font,
            int scale, rw_color color);
int rw_text_width(const char *s, rw_font font, int scale);
int rw_text_height(rw_font font, int scale);

/*
 * Outline (TrueType/OpenType) text via FreeType, rendered at any pixel
 * size. Text is UTF-8. The panel has no grey levels, so glyph edges are
 * thresholded rather than anti-aliased. Positions are given by baseline.
 */
typedef struct rw_ttf rw_ttf;

typedef struct {
    int width;    /* advance width of the whole string */
    int ascent;   /* ink extent above the baseline */
    int descent;  /* ink extent below the baseline */
} rw_text_extents;

rw_ttf *rw_ttf_open(const char *path);
void rw_ttf_close(rw_ttf *font);

/* Draw s with its baseline at y; returns the advance width. */
int rw_ttf_text(rw_display *d, rw_ttf *font, int px, int x, int y,
                const char *s, rw_color color);
/* Ink extents of s at px pixels. */
void rw_ttf_measure(rw_ttf *font, int px, const char *s, rw_text_extents *e);
/* Font-wide ascender and descender (positive) at px pixels. */
void rw_ttf_line_metrics(rw_ttf *font, int px, int *ascent, int *descent);

#ifdef __cplusplus
}
#endif

#endif /* RENOVATIO_WAVESHARE_H */
