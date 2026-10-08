/*
 * renovatio-waveshare-clock - time, date and weather on a Waveshare
 * 4-colour e-Paper panel.
 *
 * SPDX-License-Identifier: MIT
 */
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <renovatio-waveshare.h>

#include "config.h"
#include "render.h"
#include "weather.h"

#define TIMESYNC_DIR "/run/systemd/timesync"
#define TIMESYNC_FLAG TIMESYNC_DIR "/synchronized"
#define TIMESYNC_WAIT_SECONDS 90
#define WEATHER_RETRY_SECONDS 120

static volatile sig_atomic_t stop_requested;
static volatile sig_atomic_t reload_requested;

static void on_signal(int sig)
{
    if (sig == SIGHUP)
        reload_requested = 1;
    else
        stop_requested = 1;
}

static void install_signals(void)
{
    struct sigaction sa;
    memset(&sa, 0, sizeof(sa));
    sa.sa_handler = on_signal; /* no SA_RESTART: interrupt sleep() */
    sigemptyset(&sa.sa_mask);
    sigaction(SIGTERM, &sa, NULL);
    sigaction(SIGINT, &sa, NULL);
    sigaction(SIGHUP, &sa, NULL);
}

static int interrupted(void)
{
    return stop_requested || reload_requested;
}

static void usage(FILE *f)
{
    fprintf(f,
            "Usage: renovatio-waveshare-clock [OPTION]...\n"
            "Show the time, date and weather on a Waveshare e-Paper panel.\n"
            "\n"
            "  -c, --config FILE   configuration file (default %s)\n"
            "  -1, --once          draw once and exit\n"
            "  -o, --output FILE   render once to a PPM image instead of the\n"
            "                      panel (no hardware access)\n"
            "  -C, --clear         clear the panel to white and exit\n"
            "  -T, --test-partial  interactively test the experimental\n"
            "                      partial refresh methods on the panel\n"
            "  -l, --list-types    list supported display types\n"
            "  -h, --help          show this help\n"
            "\n"
            "Send SIGHUP (systemctl reload) to re-read the configuration.\n",
            CLOCK_DEFAULT_CONFIG);
}

static void setup_timezone(const char *tz)
{
    setenv("TZ", tz, 1);
    tzset();

    char path[160];
    snprintf(path, sizeof(path), "/usr/share/zoneinfo/%s", tz);
    if (strcmp(tz, "UTC") != 0 && access(path, R_OK) != 0)
        fprintf(stderr, "warning: timezone \"%s\" not found in "
                        "/usr/share/zoneinfo, times will be UTC\n", tz);
}

/*
 * The Pi has no RTC, so wait (briefly) for systemd-timesyncd before the
 * first draw. Skipped when timesyncd is not in use.
 */
static void wait_for_time_sync(void)
{
    if (access(TIMESYNC_DIR, F_OK) != 0 || access(TIMESYNC_FLAG, F_OK) == 0)
        return;

    fprintf(stderr, "waiting up to %d s for time synchronisation\n",
            TIMESYNC_WAIT_SECONDS);
    for (int i = 0; i < TIMESYNC_WAIT_SECONDS && !interrupted(); i++) {
        if (access(TIMESYNC_FLAG, F_OK) == 0)
            return;
        sleep(1);
    }
    if (!interrupted())
        fprintf(stderr, "time not synchronised yet, continuing\n");
}

/* Fetch weather if due; returns the time of the next attempt. */
static time_t update_weather(clock_config *cfg, weather_data *wx,
                             time_t now, int attempts)
{
    for (int i = 0; i < attempts && !interrupted(); i++) {
        if (i > 0)
            sleep(5);
        if (!cfg->have_coords && weather_geocode(cfg) == 0)
            fprintf(stderr, "location \"%s\" is at %.4f, %.4f\n",
                    cfg->location_name, cfg->latitude, cfg->longitude);
        if (cfg->have_coords && weather_fetch(cfg, wx) == 0)
            return now + cfg->weather_refresh_minutes * 60;
    }

    /* Keep showing old data for a while, but not forever. */
    if (wx->valid &&
        now - wx->fetched > 3 * cfg->weather_refresh_minutes * 60) {
        fprintf(stderr, "weather data is stale, hiding it\n");
        wx->valid = 0;
    }
    return now + WEATHER_RETRY_SECONDS;
}

/* Sleep until the next multiple of period seconds of wall-clock time. */
static void sleep_until_next(int period)
{
    time_t now = time(NULL);
    time_t target = (now / period + 1) * period;

    while (!interrupted() && (now = time(NULL)) < target) {
        /* Recompute if the clock jumped backwards (e.g. NTP step). */
        if (target - now > period)
            target = (now / period + 1) * period;
        time_t left = target - now;
        sleep(left > 30 ? 30 : (unsigned)left);
    }
}

static int inside(const render_area *a, int x, int y, int w, int h)
{
    return x >= a->x && y >= a->y && x + w <= a->x + a->w &&
           y + h <= a->y + a->h;
}

/*
 * Push the new frame to the panel. When only the time changed and a
 * partial method is configured, refresh just the time area; otherwise,
 * and at least every full_refresh_minutes, refresh the whole panel.
 */
static void update_panel(rw_display *d, const clock_config *cfg,
                         const render_area *time_area, time_t now,
                         time_t *last_full)
{
    int x, y, w, h;
    if (!rw_display_changed_region(d, &x, &y, &w, &h))
        return; /* nothing changed: leave the panel alone */

    if (cfg->partial != RW_PARTIAL_OFF && *last_full &&
        now - *last_full < cfg->full_refresh_minutes * 60 &&
        inside(time_area, x, y, w, h) &&
        rw_display_refresh_region(d, cfg->partial, time_area->x, time_area->y,
                                  time_area->w, time_area->h) == 0)
        return;

    rw_display_refresh(d);
    *last_full = now;
}

/* ---- --test-partial ---- */

static void draw_test_card(rw_display *d, const render_fonts *f,
                           const render_area *box, const char *letter,
                           const char *label)
{
    int w = rw_display_width(d), h = rw_display_height(d);
    rw_text_extents e;

    rw_fill(d, RW_WHITE);
    rw_rect(d, 0, 0, w, h, 2, RW_BLACK);
    rw_ttf_text(d, f->bold, 16, 8, 22, "Partial refresh test", RW_BLACK);

    rw_rect(d, box->x, box->y, box->w, box->h, 2, RW_RED);
    int size = box->h * 3 / 4;
    rw_ttf_measure(f->bold, size, letter, &e);
    rw_ttf_text(d, f->bold, size, box->x + (box->w - e.width) / 2,
                box->y + (box->h + e.ascent) / 2, letter, RW_BLACK);

    char buf[64];
    snprintf(buf, sizeof(buf), "label: %s", label);
    rw_ttf_text(d, f->bold, 14, 8, h - 8, buf, RW_BLACK);
}

static int wait_enter(const char *prompt)
{
    char line[16];
    printf("\n%s", prompt);
    fflush(stdout);
    return !interrupted() && fgets(line, sizeof(line), stdin) != NULL;
}

static int test_partial(rw_display *d, const render_fonts *f)
{
    static const struct {
        rw_partial_method method;
        const char *letter;
    } tests[] = {
        { RW_PARTIAL_PTL, "A" },
        { RW_PARTIAL_PTL83, "B" },
        { RW_PARTIAL_WINDOW, "C" },
    };

    if (!rw_display_supports_partial(d)) {
        fprintf(stderr, "no experimental partial refresh for %s\n",
                rw_display_type(d));
        return 1;
    }

    int w = rw_display_width(d), h = rw_display_height(d);
    render_area box = { w / 4, h / 4, w / 2, h / 2 };

    printf("Drawing the starting screen with a full refresh...\n");
    draw_test_card(d, f, &box, "0", "full");
    rw_display_refresh(d);

    printf("\nEach step updates ONLY the red box, using a different method.\n"
           "Watch the panel. For each step note:\n"
           "  1. does the box show the new letter?\n"
           "  2. does the rest of the screen stay still, with the bottom\n"
           "     label unchanged? (If the label changes, the whole screen\n"
           "     was refreshed and the window was ignored.)\n");

    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); i++) {
        const char *name = rw_partial_method_name(tests[i].method);
        char prompt[96];
        snprintf(prompt, sizeof(prompt),
                 "Press Enter to try method \"%s\" (box -> %s)... ", name,
                 tests[i].letter);
        if (!wait_enter(prompt))
            return 1;

        draw_test_card(d, f, &box, tests[i].letter, name);
        printf("refreshing...\n");
        rw_display_refresh_region(d, tests[i].method, box.x, box.y, box.w,
                                  box.h);
        printf("done: method \"%s\"\n", name);
    }

    if (wait_enter("Press Enter to restore the screen with a full refresh... ")) {
        draw_test_card(d, f, &box, "OK", "full");
        rw_display_refresh(d);
    }
    printf("\nIf a method worked, set \"display.partial_refresh\" to its "
           "name in the config.\n");
    return 0;
}

enum mode { MODE_RUN, MODE_ONCE, MODE_OUTPUT, MODE_CLEAR, MODE_TEST_PARTIAL };

static int run(const char *config_path, enum mode mode, const char *output)
{
    clock_config cfg;
    char err[256];

    if (config_load(config_path, &cfg, err, sizeof(err)) < 0) {
        fprintf(stderr, "config: %s\n", err);
        return 1;
    }
    setup_timezone(cfg.timezone);

    render_fonts fonts = { 0 };
    if (mode != MODE_CLEAR && render_fonts_open(&cfg, &fonts) < 0)
        return 1;

    rw_display *d = rw_display_open(cfg.display_type, cfg.rotation,
                                    mode == MODE_OUTPUT ? RW_OPEN_NO_HW : 0);
    if (!d) {
        render_fonts_close(&fonts);
        return 1;
    }
    fprintf(stderr, "display %s, %dx%d\n", rw_display_type(d),
            rw_display_width(d), rw_display_height(d));

    if (mode == MODE_CLEAR) {
        rw_display_clear_panel(d, RW_WHITE);
        rw_display_close(d);
        return 0;
    }

    if (mode == MODE_TEST_PARTIAL) {
        int r = test_partial(d, &fonts);
        rw_display_close(d);
        render_fonts_close(&fonts);
        return r;
    }

    if (cfg.partial != RW_PARTIAL_OFF && !rw_display_supports_partial(d))
        fprintf(stderr, "warning: partial refresh is not available for %s, "
                        "using full refreshes\n", rw_display_type(d));

    if (mode == MODE_RUN)
        wait_for_time_sync();

    weather_data wx = { 0 };
    time_t next_weather = 0;
    /* At boot the network may still be coming up: retry the first fetch. */
    int attempts = mode == MODE_RUN ? 6 : 1;
    int ret = 0;
    time_t last_full = 0;
    render_area time_area;

    while (!interrupted()) {
        time_t now = time(NULL);
        if (cfg.weather_enabled && now >= next_weather) {
            next_weather = update_weather(&cfg, &wx, now, attempts);
            attempts = 1;
            now = time(NULL);
        }

        render_screen(d, &fonts, &cfg, now, &wx, &time_area);

        if (mode == MODE_OUTPUT) {
            if (rw_display_save_ppm(d, output) < 0) {
                perror(output);
                ret = 1;
            }
            break;
        }

        if (!interrupted())
            update_panel(d, &cfg, &time_area, now, &last_full);
        if (mode == MODE_ONCE)
            break;

        sleep_until_next(cfg.refresh_minutes * 60);
    }

    rw_display_close(d);
    render_fonts_close(&fonts);
    return ret;
}

int main(int argc, char **argv)
{
    static const struct option opts[] = {
        { "config", required_argument, NULL, 'c' },
        { "once", no_argument, NULL, '1' },
        { "output", required_argument, NULL, 'o' },
        { "clear", no_argument, NULL, 'C' },
        { "test-partial", no_argument, NULL, 'T' },
        { "list-types", no_argument, NULL, 'l' },
        { "help", no_argument, NULL, 'h' },
        { NULL, 0, NULL, 0 },
    };

    const char *config_path = CLOCK_DEFAULT_CONFIG;
    const char *output = NULL;
    enum mode mode = MODE_RUN;
    int c;

    while ((c = getopt_long(argc, argv, "c:1o:CTlh", opts, NULL)) != -1) {
        switch (c) {
        case 'c':
            config_path = optarg;
            break;
        case '1':
            mode = MODE_ONCE;
            break;
        case 'o':
            mode = MODE_OUTPUT;
            output = optarg;
            break;
        case 'C':
            mode = MODE_CLEAR;
            break;
        case 'T':
            mode = MODE_TEST_PARTIAL;
            break;
        case 'l':
            for (size_t i = 0; i < rw_display_type_count(); i++)
                printf("%s\n", rw_display_type_name(i));
            return 0;
        case 'h':
            usage(stdout);
            return 0;
        default:
            usage(stderr);
            return 2;
        }
    }
    if (optind < argc) {
        usage(stderr);
        return 2;
    }

    /* Line-buffer output so it reaches the journal promptly. */
    setvbuf(stdout, NULL, _IOLBF, 0);
    setvbuf(stderr, NULL, _IOLBF, 0);

    install_signals();
    weather_global_init();

    int ret;
    do {
        reload_requested = 0;
        ret = run(config_path, mode, output);
        if (reload_requested && !stop_requested)
            fprintf(stderr, "reloading configuration\n");
    } while (reload_requested && !stop_requested && mode == MODE_RUN);

    weather_global_cleanup();
    return ret;
}
