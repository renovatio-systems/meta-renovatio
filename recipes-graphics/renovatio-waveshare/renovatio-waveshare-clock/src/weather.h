/*
 * SPDX-License-Identifier: MIT
 */
#ifndef CLOCK_WEATHER_H
#define CLOCK_WEATHER_H

#include <time.h>

#include "config.h"

typedef enum {
    WX_CLEAR,
    WX_PARTLY_CLOUDY,
    WX_CLOUDY,
    WX_FOG,
    WX_DRIZZLE,
    WX_RAIN,
    WX_SNOW,
    WX_THUNDER,
} weather_kind;

typedef struct {
    int valid;
    time_t fetched;
    double temperature;
    double temp_max;
    double temp_min;
    double wind_speed;
    int humidity;
    int code;           /* WMO weather interpretation code */
    int is_day;
} weather_data;

void weather_global_init(void);
void weather_global_cleanup(void);

/* Resolve cfg->location_name to coordinates (Open-Meteo geocoding). */
int weather_geocode(clock_config *cfg);

/* Fetch current conditions for the configured location. */
int weather_fetch(const clock_config *cfg, weather_data *out);

const char *weather_describe(int code);
weather_kind weather_classify(int code);

#endif /* CLOCK_WEATHER_H */
