/*
 * Screen layout: date and location header, large time, weather band.
 *
 * Layout is designed for the 296x160 (landscape) 2.15" panel and scales
 * with the panel size. Text uses outline fonts through FreeType.
 *
 * SPDX-License-Identifier: MIT
 */
#include <math.h>
#include <stdio.h>
#include <string.h>

#include "render.h"

static int max_i(int a, int b)
{
    return a > b ? a : b;
}

/* ---- weather icons, drawn inside an S x S box at (x, y) ---- */

static void sun(rw_display *d, int cx, int cy, int r, int ray_len, int t)
{
    for (int i = 0; i < 8; i++) {
        double a = i * M_PI / 4;
        int r0 = r + 2 * t;
        int r1 = r0 + ray_len;
        rw_line(d, cx + (int)lround(cos(a) * r0), cy + (int)lround(sin(a) * r0),
                cx + (int)lround(cos(a) * r1), cy + (int)lround(sin(a) * r1),
                t, RW_BLACK);
    }
    rw_fill_circle(d, cx, cy, r, RW_BLACK);
    rw_fill_circle(d, cx, cy, r - t, RW_YELLOW);
}

/* Crescent moon: yellow, outlined in black, opening towards the top right. */
static void moon(rw_display *d, int cx, int cy, int r, int t)
{
    int ccx = cx + r * 5 / 10, ccy = cy - r * 4 / 10;
    int cr = r * 8 / 10;

    for (int y = -r; y <= r; y++)
        for (int x = -r; x <= r; x++) {
            double d1 = sqrt((double)x * x + (double)y * y);
            double d2 = hypot(cx + x - ccx, cy + y - ccy);
            if (d1 > r + 0.5 || d2 < cr - 0.5)
                continue;
            int edge = d1 > r - t + 0.5 || d2 < cr + t - 0.5;
            rw_pixel(d, cx + x, cy + y, edge ? RW_BLACK : RW_YELLOW);
        }
}

/*
 * Cloud of width w with its top-left corner at (x, y); height is 0.8 w.
 * Drawn as a union of circles and a rectangle: first grown by t in black,
 * then at normal size in white, which leaves a clean outline.
 */
static void cloud(rw_display *d, int x, int y, int w, int t)
{
    const struct {
        double cx, cy, r;
    } c[] = {
        { 0.30, 0.60, 0.20 },
        { 0.52, 0.42, 0.26 },
        { 0.74, 0.62, 0.18 },
    };

    for (int pass = 0; pass < 2; pass++) {
        int grow = pass == 0 ? t : 0;
        rw_color col = pass == 0 ? RW_BLACK : RW_WHITE;
        for (size_t i = 0; i < sizeof(c) / sizeof(c[0]); i++)
            rw_fill_circle(d, x + (int)(c[i].cx * w), y + (int)(c[i].cy * w),
                           (int)(c[i].r * w) + grow, col);
        rw_fill_rect(d, x + (int)(0.30 * w), y + (int)(0.45 * w) - grow,
                     (int)(0.44 * w), (int)(0.35 * w) + 2 * grow, col);
    }
}

static void weather_icon(rw_display *d, weather_kind kind, int is_day,
                         int x, int y, int s)
{
    int t = max_i(1, s / 28);

    switch (kind) {
    case WX_CLEAR:
        if (is_day)
            sun(d, x + s / 2, y + s / 2, s * 22 / 100, s * 12 / 100, t);
        else
            moon(d, x + s / 2, y + s / 2, s * 34 / 100, t);
        return;
    case WX_PARTLY_CLOUDY:
        if (is_day)
            sun(d, x + s * 36 / 100, y + s * 32 / 100, s * 15 / 100,
                s * 8 / 100, t);
        else
            moon(d, x + s * 36 / 100, y + s * 32 / 100, s * 22 / 100, t);
        cloud(d, x + s * 20 / 100, y + s * 26 / 100, s * 78 / 100, t);
        return;
    case WX_CLOUDY:
        cloud(d, x + s * 28 / 100, y + s * 6 / 100, s * 70 / 100, t);
        cloud(d, x + s * 2 / 100, y + s * 24 / 100, s * 84 / 100, t);
        return;
    default:
        break;
    }

    /* Precipitation and fog: a cloud at the top, detail underneath. */
    int cw = s * 88 / 100;
    cloud(d, x + s * 6 / 100, y, cw, t);
    int top = y + cw * 80 / 100 + 2 * t;
    int bottom = y + s;

    switch (kind) {
    case WX_FOG:
        for (int i = 0; i < 3; i++) {
            int ly = top + i * (bottom - top) / 3 + t;
            int off = (i % 2) * s / 10;
            rw_line(d, x + s / 10 + off, ly, x + s * 8 / 10 + off, ly, t,
                    RW_BLACK);
        }
        break;
    case WX_DRIZZLE:
    case WX_RAIN: {
        int len = kind == WX_RAIN ? (bottom - top) : (bottom - top) / 2;
        for (int i = 0; i < 4; i++) {
            int lx = x + s * (22 + i * 18) / 100;
            int ly = top + (i % 2) * (bottom - top - len) / 2;
            rw_line(d, lx, ly, lx - len / 3, ly + len - t, t, RW_BLACK);
        }
        break;
    }
    case WX_SNOW:
        for (int i = 0; i < 3; i++) {
            int fx = x + s * (28 + i * 22) / 100;
            int fy = top + (bottom - top) / 2 + ((i % 2) ? -1 : 1) * s / 20;
            int a = max_i(2, s / 14);
            rw_line(d, fx - a, fy, fx + a, fy, t, RW_BLACK);
            rw_line(d, fx - a / 2, fy - a, fx + a / 2, fy + a, t, RW_BLACK);
            rw_line(d, fx + a / 2, fy - a, fx - a / 2, fy + a, t, RW_BLACK);
        }
        break;
    case WX_THUNDER: {
        static const double bolt[][2] = {
            { 0.54, 0.50 }, { 0.36, 0.78 }, { 0.50, 0.78 }, { 0.42, 1.00 },
            { 0.70, 0.66 }, { 0.55, 0.66 }, { 0.66, 0.50 },
        };
        rw_point p[7];
        for (int i = 0; i < 7; i++) {
            p[i].x = x + (int)(bolt[i][0] * s);
            p[i].y = y + (int)(bolt[i][1] * s);
        }
        rw_fill_polygon(d, p, 7, RW_RED);
        break;
    }
    default:
        break;
    }
}

/* ---- text helpers ---- */

typedef struct {
    rw_display *d;
    const render_fonts *fonts;
    const clock_config *cfg;
    double k;  /* layout scale relative to the 296x160 design */
    int m;     /* margin */
} ctx;

static int px(const ctx *c, double design_px)
{
    return max_i(8, (int)lround(design_px * c->k));
}

static int text_width(rw_ttf *f, int size, const char *s)
{
    rw_text_extents e;
    rw_ttf_measure(f, size, s, &e);
    return e.width;
}

/* Copy s into buf, cutting it so it renders within max_w pixels. */
static const char *fit_text(char *buf, size_t len, const char *s, rw_ttf *f,
                            int size, int max_w)
{
    snprintf(buf, len, "%s", s);
    size_t n = strlen(buf);
    while (n > 0 && text_width(f, size, buf) > max_w) {
        /* Drop a whole UTF-8 character. */
        do
            n--;
        while (n > 0 && ((unsigned char)buf[n] & 0xC0) == 0x80);
        buf[n] = '\0';
    }
    return buf;
}

static long round_temp(double v)
{
    long r = lround(v);
    return r == 0 ? 0 : r; /* avoid "-0" */
}

/* ---- screen ---- */

static int draw_header(const ctx *c, const struct tm *tm)
{
    int w = rw_display_width(c->d);
    int size = px(c, 17);
    int asc, desc;
    char date[32], loc[64];

    rw_ttf_line_metrics(c->fonts->regular, size, &asc, &desc);
    int baseline = c->m + asc - desc / 2;

    strftime(date, sizeof(date), "%a %d %b", tm);
    int dw = rw_ttf_text(c->d, c->fonts->regular, size, c->m, baseline, date,
                         RW_BLACK);

    if (c->cfg->location_name[0]) {
        int max_w = w - 2 * c->m - dw - px(c, 8);
        fit_text(loc, sizeof(loc), c->cfg->location_name, c->fonts->bold,
                 size, max_w);
        int lw = text_width(c->fonts->bold, size, loc);
        rw_ttf_text(c->d, c->fonts->bold, size, w - c->m - lw, baseline, loc,
                    RW_RED);
    }

    int line_y = baseline + desc;
    int t = max_i(1, (int)lround(c->k));
    rw_fill_rect(c->d, c->m, line_y, w - 2 * c->m, t, RW_BLACK);
    return line_y + t;
}

static void draw_time(const ctx *c, const struct tm *tm, int top, int bottom)
{
    int w = rw_display_width(c->d);
    rw_ttf *big = c->fonts->bold;
    char hm[8], suffix[4] = "";

    if (c->cfg->clock_24h) {
        strftime(hm, sizeof(hm), "%H:%M", tm);
    } else {
        int h = tm->tm_hour % 12;
        snprintf(hm, sizeof(hm), "%d:%02d", h ? h : 12, tm->tm_min);
        snprintf(suffix, sizeof(suffix), "%s", tm->tm_hour < 12 ? "AM" : "PM");
    }

    int avail_w = w - 2 * c->m;
    int avail_h = bottom - top;

    /* Largest size whose ink fits the time area (digits are tabular). */
    int size = avail_h * 2;
    rw_text_extents e;
    int sw = 0, gap = 0, ssize = 0;
    for (; size > 8; size--) {
        rw_ttf_measure(big, size, hm, &e);
        if (suffix[0]) {
            ssize = max_i(8, size / 4);
            gap = max_i(2, size / 16);
            sw = gap + text_width(c->fonts->regular, ssize, suffix);
        }
        if (e.width + sw <= avail_w && e.ascent + e.descent <= avail_h)
            break;
    }

    int x = (w - e.width - sw) / 2;
    int baseline = top + (avail_h - e.ascent - e.descent) / 2 + e.ascent;
    rw_ttf_text(c->d, big, size, x, baseline, hm, RW_BLACK);
    if (suffix[0])
        rw_ttf_text(c->d, c->fonts->regular, ssize, x + e.width + gap,
                    baseline, suffix, RW_BLACK);
}

/* Weather band text sizes, at the 296x160 design size. */
#define WX_TEMP_PX 26
#define WX_HILO_PX 14
#define WX_DESC_PX 16
#define WX_WIND_PX 13
#define WX_GAP_PX 4

/* Height of the weather band: temperature, description and wind lines. */
static int weather_band_height(const ctx *c)
{
    rw_text_extents e;
    int asc, desc;
    int h = 0;

    rw_ttf_measure(c->fonts->bold, px(c, WX_TEMP_PX), "0", &e);
    h += e.ascent + px(c, WX_GAP_PX);
    rw_ttf_line_metrics(c->fonts->regular, px(c, WX_DESC_PX), &asc, &desc);
    h += asc + desc;
    rw_ttf_line_metrics(c->fonts->regular, px(c, WX_WIND_PX), &asc, &desc);
    h += asc + desc;
    return h;
}

static void draw_weather(const ctx *c, const weather_data *wx, int top,
                         int size)
{
    int w = rw_display_width(c->d);
    rw_ttf *reg = c->fonts->regular, *bold = c->fonts->bold;

    if (!wx || !wx->valid) {
        const char *msg = "Weather unavailable";
        int s = px(c, WX_DESC_PX);
        int asc, desc;
        rw_ttf_line_metrics(reg, s, &asc, &desc);
        rw_ttf_text(c->d, reg, s, (w - text_width(reg, s, msg)) / 2,
                    top + (size - asc - desc) / 2 + asc, msg, RW_BLACK);
        return;
    }

    weather_icon(c->d, weather_classify(wx->code), wx->is_day, c->m, top,
                 size);

    int x = c->m + size + px(c, 8);
    int max_w = w - c->m - x;
    char buf[64], fitted[64];
    rw_text_extents e;
    int asc, desc;

    /* Current temperature, then today's high/low beside it. */
    int ts = px(c, WX_TEMP_PX);
    rw_ttf_measure(bold, ts, "0", &e);
    int baseline = top + e.ascent;
    snprintf(buf, sizeof(buf), "%ld\xc2\xb0%s", round_temp(wx->temperature),
             c->cfg->imperial ? "F" : "C");
    int tw = rw_ttf_text(c->d, bold, ts, x, baseline, buf, RW_RED);

    int hs = px(c, WX_HILO_PX);
    snprintf(buf, sizeof(buf), "H %ld\xc2\xb0  L %ld\xc2\xb0",
             round_temp(wx->temp_max), round_temp(wx->temp_min));
    int gap = px(c, 10);
    if (text_width(reg, hs, buf) <= max_w - tw - gap)
        rw_ttf_text(c->d, reg, hs, x + tw + gap, baseline, buf, RW_BLACK);

    /* Description. */
    int ds = px(c, WX_DESC_PX);
    rw_ttf_line_metrics(reg, ds, &asc, &desc);
    baseline += px(c, WX_GAP_PX) + asc;
    rw_ttf_text(c->d, reg, ds, x, baseline,
                fit_text(fitted, sizeof(fitted), weather_describe(wx->code),
                         reg, ds, max_w),
                RW_BLACK);

    /* Wind and humidity. */
    int ws = px(c, WX_WIND_PX);
    baseline += desc;
    rw_ttf_line_metrics(reg, ws, &asc, &desc);
    baseline += asc;
    const char *unit = c->cfg->imperial ? "mph" : "km/h";
    snprintf(buf, sizeof(buf), "Wind %ld %s   Humidity %d%%",
             round_temp(wx->wind_speed), unit, wx->humidity);
    if (text_width(reg, ws, buf) > max_w)
        snprintf(buf, sizeof(buf), "Wind %ld %s  Hum %d%%",
                 round_temp(wx->wind_speed), unit, wx->humidity);
    rw_ttf_text(c->d, reg, ws, x, baseline,
                fit_text(fitted, sizeof(fitted), buf, reg, ws, max_w),
                RW_BLACK);
}

int render_fonts_open(const clock_config *cfg, render_fonts *fonts)
{
    fonts->regular = rw_ttf_open(cfg->font_regular);
    fonts->bold = rw_ttf_open(cfg->font_bold);
    if (!fonts->regular || !fonts->bold) {
        render_fonts_close(fonts);
        return -1;
    }
    return 0;
}

void render_fonts_close(render_fonts *fonts)
{
    rw_ttf_close(fonts->regular);
    rw_ttf_close(fonts->bold);
    fonts->regular = fonts->bold = NULL;
}

void render_screen(rw_display *d, const render_fonts *fonts,
                   const clock_config *cfg, time_t now,
                   const weather_data *wx, render_area *time_area)
{
    int w = rw_display_width(d);
    int h = rw_display_height(d);
    double k = fmin(w / 296.0, h / 160.0);
    ctx c = { d, fonts, cfg, k, max_i(3, (int)lround(4 * k)) };

    struct tm tm;
    localtime_r(&now, &tm);

    rw_fill(d, RW_WHITE);

    int header_bottom = draw_header(&c, &tm);

    int band = cfg->weather_enabled ? weather_band_height(&c) : 0;
    int band_top = h - c.m - band;

    int time_top = header_bottom + px(&c, 7);
    int time_bottom = band_top - px(&c, 12);
    draw_time(&c, &tm, time_top, time_bottom);
    if (time_area) {
        /* Everything between the header line and the weather band. */
        time_area->x = 0;
        time_area->y = header_bottom + 1;
        time_area->w = w;
        time_area->h = band_top - 1 - time_area->y;
    }

    if (cfg->weather_enabled)
        draw_weather(&c, wx, band_top, band);
}
