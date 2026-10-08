/*
 * JSON configuration loading (cJSON).
 *
 * SPDX-License-Identifier: MIT
 */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cjson/cJSON.h>
#include <renovatio-waveshare.h>

#include "config.h"

static void set_err(char *err, size_t errlen, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(err, errlen, fmt, ap);
    va_end(ap);
}

static char *read_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (!f)
        return NULL;

    char *buf = NULL;
    size_t len = 0, cap = 0;
    for (;;) {
        if (len + 4096 + 1 > cap) {
            cap = cap ? cap * 2 : 8192;
            char *nb = realloc(buf, cap);
            if (!nb) {
                free(buf);
                fclose(f);
                return NULL;
            }
            buf = nb;
        }
        size_t n = fread(buf + len, 1, 4096, f);
        len += n;
        if (n < 4096)
            break;
    }
    fclose(f);
    buf[len] = '\0';
    return buf;
}

static void copy_str(char *dst, size_t dstlen, const char *src)
{
    snprintf(dst, dstlen, "%s", src);
}

/* Fetch an optional string member; returns -1 if present but not a string. */
static int get_string(const cJSON *obj, const char *key, char *dst,
                      size_t dstlen)
{
    const cJSON *it = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!it)
        return 0;
    if (!cJSON_IsString(it) || !it->valuestring)
        return -1;
    copy_str(dst, dstlen, it->valuestring);
    return 1;
}

static int get_number(const cJSON *obj, const char *key, double *dst)
{
    const cJSON *it = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!it)
        return 0;
    if (!cJSON_IsNumber(it))
        return -1;
    *dst = it->valuedouble;
    return 1;
}

static int get_bool(const cJSON *obj, const char *key, int *dst)
{
    const cJSON *it = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (!it)
        return 0;
    if (!cJSON_IsBool(it))
        return -1;
    *dst = cJSON_IsTrue(it);
    return 1;
}

static int get_minutes(const cJSON *obj, const char *key, int *dst,
                       char *err, size_t errlen)
{
    double v;
    int r = get_number(obj, key, &v);
    if (r < 0 || (r > 0 && (v < 1 || v > 1440 || v != (int)v))) {
        set_err(err, errlen, "\"%s\" must be a whole number of minutes "
                             "between 1 and 1440", key);
        return -1;
    }
    if (r > 0)
        *dst = (int)v;
    return 0;
}

static void set_defaults(clock_config *cfg)
{
    memset(cfg, 0, sizeof(*cfg));
    copy_str(cfg->display_type, sizeof(cfg->display_type), "2in15g");
    cfg->rotation = RW_ROTATE_AUTO;
    copy_str(cfg->timezone, sizeof(cfg->timezone), "UTC");
    cfg->clock_24h = 1;
    cfg->refresh_minutes = 1;
    cfg->full_refresh_minutes = 60;
    cfg->partial = RW_PARTIAL_OFF;
    cfg->weather_enabled = 1;
    cfg->weather_refresh_minutes = 30;
    copy_str(cfg->font_regular, sizeof(cfg->font_regular),
             CLOCK_DEFAULT_FONT);
    copy_str(cfg->font_bold, sizeof(cfg->font_bold), CLOCK_DEFAULT_FONT_BOLD);
}

static int parse_display(const cJSON *o, clock_config *cfg, char *err,
                         size_t errlen)
{
    if (get_string(o, "type", cfg->display_type,
                   sizeof(cfg->display_type)) < 0) {
        set_err(err, errlen, "\"display.type\" must be a string");
        return -1;
    }
    if (!rw_display_type_supported(cfg->display_type)) {
        set_err(err, errlen, "unsupported display type \"%s\" "
                             "(see --list-types)", cfg->display_type);
        return -1;
    }

    char partial[16] = "off";
    if (get_string(o, "partial_refresh", partial, sizeof(partial)) < 0 ||
        rw_partial_method_parse(partial, &cfg->partial) < 0) {
        set_err(err, errlen, "\"display.partial_refresh\" must be \"off\", "
                             "\"ptl\", \"ptl83\" or \"window\"");
        return -1;
    }

    const cJSON *rot = cJSON_GetObjectItemCaseSensitive(o, "rotation");
    if (!rot)
        return 0;
    if (cJSON_IsString(rot) && strcmp(rot->valuestring, "auto") == 0) {
        cfg->rotation = RW_ROTATE_AUTO;
    } else if (cJSON_IsNumber(rot) &&
               (rot->valueint == 0 || rot->valueint == 90 ||
                rot->valueint == 180 || rot->valueint == 270) &&
               rot->valuedouble == rot->valueint) {
        cfg->rotation = rot->valueint;
    } else {
        set_err(err, errlen,
                "\"display.rotation\" must be \"auto\", 0, 90, 180 or 270");
        return -1;
    }
    return 0;
}

static int parse_location(const cJSON *o, clock_config *cfg, char *err,
                          size_t errlen)
{
    if (get_string(o, "name", cfg->location_name,
                   sizeof(cfg->location_name)) < 0) {
        set_err(err, errlen, "\"location.name\" must be a string");
        return -1;
    }

    int rlat = get_number(o, "latitude", &cfg->latitude);
    int rlon = get_number(o, "longitude", &cfg->longitude);
    if (rlat < 0 || rlon < 0 || (rlat > 0) != (rlon > 0)) {
        set_err(err, errlen, "\"location.latitude\" and "
                             "\"location.longitude\" must both be numbers");
        return -1;
    }
    if (rlat > 0) {
        if (cfg->latitude < -90 || cfg->latitude > 90 ||
            cfg->longitude < -180 || cfg->longitude > 180) {
            set_err(err, errlen, "location coordinates out of range");
            return -1;
        }
        cfg->have_coords = 1;
    }
    return 0;
}

static int parse_clock(const cJSON *o, clock_config *cfg, char *err,
                       size_t errlen)
{
    if (get_bool(o, "24_hour", &cfg->clock_24h) < 0) {
        set_err(err, errlen, "\"clock.24_hour\" must be true or false");
        return -1;
    }
    if (get_minutes(o, "refresh_minutes", &cfg->refresh_minutes, err,
                    errlen) < 0)
        return -1;
    return get_minutes(o, "full_refresh_minutes", &cfg->full_refresh_minutes,
                       err, errlen);
}

static int parse_weather(const cJSON *o, clock_config *cfg, char *err,
                         size_t errlen)
{
    if (get_bool(o, "enabled", &cfg->weather_enabled) < 0) {
        set_err(err, errlen, "\"weather.enabled\" must be true or false");
        return -1;
    }

    char units[16] = "metric";
    if (get_string(o, "units", units, sizeof(units)) < 0 ||
        (strcmp(units, "metric") != 0 && strcmp(units, "imperial") != 0)) {
        set_err(err, errlen,
                "\"weather.units\" must be \"metric\" or \"imperial\"");
        return -1;
    }
    cfg->imperial = strcmp(units, "imperial") == 0;

    return get_minutes(o, "refresh_minutes", &cfg->weather_refresh_minutes,
                       err, errlen);
}

static int parse_fonts(const cJSON *o, clock_config *cfg, char *err,
                       size_t errlen)
{
    if (get_string(o, "regular", cfg->font_regular,
                   sizeof(cfg->font_regular)) < 0 ||
        get_string(o, "bold", cfg->font_bold, sizeof(cfg->font_bold)) < 0) {
        set_err(err, errlen, "\"fonts.regular\" and \"fonts.bold\" must be "
                             "font file paths");
        return -1;
    }
    return 0;
}

int config_load(const char *path, clock_config *cfg, char *err, size_t errlen)
{
    set_defaults(cfg);

    char *text = read_file(path);
    if (!text) {
        set_err(err, errlen, "cannot read %s", path);
        return -1;
    }

    cJSON *root = cJSON_Parse(text);
    if (!root) {
        const char *where = cJSON_GetErrorPtr();
        set_err(err, errlen, "%s: invalid JSON near \"%.20s\"", path,
                where ? where : "?");
        free(text);
        return -1;
    }
    free(text);

    int ret = -1;
    if (!cJSON_IsObject(root)) {
        set_err(err, errlen, "%s: top level must be an object", path);
        goto out;
    }

    static const struct {
        const char *key;
        int (*parse)(const cJSON *, clock_config *, char *, size_t);
    } sections[] = {
        { "display", parse_display },
        { "location", parse_location },
        { "clock", parse_clock },
        { "weather", parse_weather },
        { "fonts", parse_fonts },
    };

    for (size_t i = 0; i < sizeof(sections) / sizeof(sections[0]); i++) {
        const cJSON *o = cJSON_GetObjectItemCaseSensitive(root,
                                                          sections[i].key);
        if (!o)
            continue;
        if (!cJSON_IsObject(o)) {
            set_err(err, errlen, "\"%s\" must be an object", sections[i].key);
            goto out;
        }
        if (sections[i].parse(o, cfg, err, errlen) < 0)
            goto out;
    }

    if (get_string(root, "timezone", cfg->timezone,
                   sizeof(cfg->timezone)) < 0) {
        set_err(err, errlen, "\"timezone\" must be a string");
        goto out;
    }

    if (cfg->weather_enabled && !cfg->have_coords &&
        cfg->location_name[0] == '\0') {
        set_err(err, errlen, "weather needs \"location.name\" or "
                             "latitude/longitude");
        goto out;
    }

    ret = 0;
out:
    cJSON_Delete(root);
    return ret;
}
