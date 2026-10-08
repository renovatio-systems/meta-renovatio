/*
 * SPDX-License-Identifier: MIT
 */
#ifndef CLOCK_RENDER_H
#define CLOCK_RENDER_H

#include <time.h>

#include <renovatio-waveshare.h>

#include "config.h"
#include "weather.h"

typedef struct {
    rw_ttf *regular;
    rw_ttf *bold;
} render_fonts;

int render_fonts_open(const clock_config *cfg, render_fonts *fonts);
void render_fonts_close(render_fonts *fonts);

/* Screen area holding the time, in logical display coordinates. */
typedef struct {
    int x, y, w, h;
} render_area;

/* Draw the whole screen into the display framebuffer. */
void render_screen(rw_display *d, const render_fonts *fonts,
                   const clock_config *cfg, time_t now,
                   const weather_data *w, render_area *time_area);

#endif /* CLOCK_RENDER_H */
