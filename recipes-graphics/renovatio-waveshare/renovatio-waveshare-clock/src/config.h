/*
 * SPDX-License-Identifier: MIT
 */
#ifndef CLOCK_CONFIG_H
#define CLOCK_CONFIG_H

#include <stddef.h>

#include <renovatio-waveshare.h>

#define CLOCK_DEFAULT_CONFIG "/etc/renovatio-waveshare/clock.json"
#define CLOCK_DEFAULT_FONT "/usr/share/fonts/truetype/DejaVuSans-Bold.ttf"
#define CLOCK_DEFAULT_FONT_BOLD "/usr/share/fonts/truetype/DejaVuSans-Bold.ttf"

typedef struct {
    /* "display" */
    char display_type[32];
    int rotation;                 /* degrees, or RW_ROTATE_AUTO */
    rw_partial_method partial;    /* experimental time-only refresh */

    /* "location" */
    char location_name[64];
    int have_coords;
    double latitude;
    double longitude;

    /* "timezone" */
    char timezone[64];

    /* "clock" */
    int clock_24h;
    int refresh_minutes;
    int full_refresh_minutes;     /* max time between full refreshes */

    /* "weather" */
    int weather_enabled;
    int imperial;
    int weather_refresh_minutes;

    /* "fonts" */
    char font_regular[160];
    char font_bold[160];
} clock_config;

/* Returns 0 on success; on failure writes a message to err. */
int config_load(const char *path, clock_config *cfg, char *err, size_t errlen);

#endif /* CLOCK_CONFIG_H */
