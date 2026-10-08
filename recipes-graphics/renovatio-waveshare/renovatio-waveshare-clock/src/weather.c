/*
 * Weather from Open-Meteo (https://open-meteo.com), which needs no API key.
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cjson/cJSON.h>
#include <curl/curl.h>

#include "weather.h"

#define USER_AGENT "renovatio-waveshare-clock/1.0"
#define MAX_RESPONSE (1024 * 1024)

typedef struct {
    char *data;
    size_t len;
} buffer;

static size_t write_cb(char *ptr, size_t size, size_t nmemb, void *userdata)
{
    buffer *b = userdata;
    size_t n = size * nmemb;
    if (b->len + n > MAX_RESPONSE)
        return 0;
    char *nd = realloc(b->data, b->len + n + 1);
    if (!nd)
        return 0;
    memcpy(nd + b->len, ptr, n);
    b->data = nd;
    b->len += n;
    b->data[b->len] = '\0';
    return n;
}

/* GET a URL and parse the body as JSON. Caller frees with cJSON_Delete. */
static cJSON *http_get_json(const char *url)
{
    CURL *curl = curl_easy_init();
    if (!curl)
        return NULL;

    buffer b = { 0 };
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &b);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);

    CURLcode rc = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (rc != CURLE_OK) {
        fprintf(stderr, "weather: request failed: %s\n",
                curl_easy_strerror(rc));
        free(b.data);
        return NULL;
    }

    cJSON *json = b.data ? cJSON_Parse(b.data) : NULL;
    if (!json)
        fprintf(stderr, "weather: invalid JSON response\n");
    free(b.data);
    return json;
}

static char *escape(const char *s)
{
    CURL *curl = curl_easy_init();
    if (!curl)
        return NULL;
    char *e = curl_easy_escape(curl, s, 0);
    char *copy = e ? strdup(e) : NULL;
    curl_free(e);
    curl_easy_cleanup(curl);
    return copy;
}

void weather_global_init(void)
{
    curl_global_init(CURL_GLOBAL_DEFAULT);
}

void weather_global_cleanup(void)
{
    curl_global_cleanup();
}

int weather_geocode(clock_config *cfg)
{
    char *name = escape(cfg->location_name);
    if (!name)
        return -1;

    char url[512];
    snprintf(url, sizeof(url),
             "https://geocoding-api.open-meteo.com/v1/search"
             "?name=%s&count=1&language=en&format=json", name);
    free(name);

    cJSON *root = http_get_json(url);
    if (!root)
        return -1;

    int ret = -1;
    const cJSON *results = cJSON_GetObjectItemCaseSensitive(root, "results");
    const cJSON *first = cJSON_GetArrayItem(results, 0);
    const cJSON *lat = cJSON_GetObjectItemCaseSensitive(first, "latitude");
    const cJSON *lon = cJSON_GetObjectItemCaseSensitive(first, "longitude");
    if (cJSON_IsNumber(lat) && cJSON_IsNumber(lon)) {
        cfg->latitude = lat->valuedouble;
        cfg->longitude = lon->valuedouble;
        cfg->have_coords = 1;
        ret = 0;
    } else {
        fprintf(stderr, "weather: location \"%s\" not found\n",
                cfg->location_name);
    }
    cJSON_Delete(root);
    return ret;
}

static int get_num(const cJSON *obj, const char *key, double *out)
{
    const cJSON *it = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!cJSON_IsNumber(it))
        return -1;
    *out = it->valuedouble;
    return 0;
}

static int get_first_num(const cJSON *obj, const char *key, double *out)
{
    const cJSON *arr = cJSON_GetObjectItemCaseSensitive(obj, key);
    const cJSON *it = cJSON_GetArrayItem(arr, 0);
    if (!cJSON_IsNumber(it))
        return -1;
    *out = it->valuedouble;
    return 0;
}

int weather_fetch(const clock_config *cfg, weather_data *out)
{
    if (!cfg->have_coords)
        return -1;

    char *tz = escape(cfg->timezone);
    if (!tz)
        return -1;

    char url[768];
    snprintf(url, sizeof(url),
             "https://api.open-meteo.com/v1/forecast"
             "?latitude=%.4f&longitude=%.4f"
             "&current=temperature_2m,relative_humidity_2m,weather_code,"
             "wind_speed_10m,is_day"
             "&daily=temperature_2m_max,temperature_2m_min"
             "&forecast_days=1&timezone=%s%s",
             cfg->latitude, cfg->longitude, tz,
             cfg->imperial ? "&temperature_unit=fahrenheit"
                             "&wind_speed_unit=mph"
                           : "");
    free(tz);

    cJSON *root = http_get_json(url);
    if (!root)
        return -1;

    weather_data w = { 0 };
    double code, hum, is_day;
    const cJSON *cur = cJSON_GetObjectItemCaseSensitive(root, "current");
    const cJSON *daily = cJSON_GetObjectItemCaseSensitive(root, "daily");

    int ok = get_num(cur, "temperature_2m", &w.temperature) == 0 &&
             get_num(cur, "relative_humidity_2m", &hum) == 0 &&
             get_num(cur, "weather_code", &code) == 0 &&
             get_num(cur, "wind_speed_10m", &w.wind_speed) == 0 &&
             get_num(cur, "is_day", &is_day) == 0 &&
             get_first_num(daily, "temperature_2m_max", &w.temp_max) == 0 &&
             get_first_num(daily, "temperature_2m_min", &w.temp_min) == 0;
    cJSON_Delete(root);

    if (!ok) {
        fprintf(stderr, "weather: unexpected response format\n");
        return -1;
    }

    w.humidity = (int)hum;
    w.code = (int)code;
    w.is_day = is_day != 0;
    w.fetched = time(NULL);
    w.valid = 1;
    *out = w;
    return 0;
}

/* WMO weather interpretation codes, as used by Open-Meteo. */
static const struct {
    int code;
    weather_kind kind;
    const char *text;
} codes[] = {
    { 0, WX_CLEAR, "Clear" },
    { 1, WX_CLEAR, "Mostly clear" },
    { 2, WX_PARTLY_CLOUDY, "Partly cloudy" },
    { 3, WX_CLOUDY, "Overcast" },
    { 45, WX_FOG, "Fog" },
    { 48, WX_FOG, "Freezing fog" },
    { 51, WX_DRIZZLE, "Light drizzle" },
    { 53, WX_DRIZZLE, "Drizzle" },
    { 55, WX_DRIZZLE, "Heavy drizzle" },
    { 56, WX_DRIZZLE, "Freezing drizzle" },
    { 57, WX_DRIZZLE, "Freezing drizzle" },
    { 61, WX_RAIN, "Light rain" },
    { 63, WX_RAIN, "Rain" },
    { 65, WX_RAIN, "Heavy rain" },
    { 66, WX_RAIN, "Freezing rain" },
    { 67, WX_RAIN, "Freezing rain" },
    { 71, WX_SNOW, "Light snow" },
    { 73, WX_SNOW, "Snow" },
    { 75, WX_SNOW, "Heavy snow" },
    { 77, WX_SNOW, "Snow grains" },
    { 80, WX_RAIN, "Showers" },
    { 81, WX_RAIN, "Heavy showers" },
    { 82, WX_RAIN, "Violent showers" },
    { 85, WX_SNOW, "Snow showers" },
    { 86, WX_SNOW, "Snow showers" },
    { 95, WX_THUNDER, "Thunderstorm" },
    { 96, WX_THUNDER, "Thunder, hail" },
    { 99, WX_THUNDER, "Thunder, hail" },
};

const char *weather_describe(int code)
{
    for (size_t i = 0; i < sizeof(codes) / sizeof(codes[0]); i++)
        if (codes[i].code == code)
            return codes[i].text;
    return "Unknown";
}

weather_kind weather_classify(int code)
{
    for (size_t i = 0; i < sizeof(codes) / sizeof(codes[0]); i++)
        if (codes[i].code == code)
            return codes[i].kind;
    return WX_CLOUDY;
}
