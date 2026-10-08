/*
 * EXPERIMENTAL window ("partial") refresh for the 2.15" 4-colour panel.
 *
 * Waveshare does not support partial refresh on its 4-colour panels and
 * publishes no commands for it. The panel's init sequence matches the
 * UC81xx-style command set, so this tries the window mechanisms such
 * controllers commonly provide. Each method resets and initialises the
 * panel, sends only the window's pixels, refreshes and sleeps again.
 *
 * SPDX-License-Identifier: MIT
 */
#include "DEV_Config.h"
#include "EPD_2in15g.h"

#include "rw_internal.h"

static void send_command(uint8_t reg)
{
    DEV_Digital_Write(EPD_DC_PIN, 0);
    DEV_Digital_Write(EPD_CS_PIN, 0);
    DEV_SPI_WriteByte(reg);
    DEV_Digital_Write(EPD_CS_PIN, 1);
}

static void send_data(uint8_t data)
{
    DEV_Digital_Write(EPD_DC_PIN, 1);
    DEV_Digital_Write(EPD_CS_PIN, 0);
    DEV_SPI_WriteByte(data);
    DEV_Digital_Write(EPD_CS_PIN, 1);
}

static void send_u16(int v)
{
    send_data((uint8_t)(v >> 8));
    send_data((uint8_t)(v & 0xFF));
}

/* BUSY is high when idle on this panel. */
static void wait_idle(void)
{
    while (!DEV_Digital_Read(EPD_BUSY_PIN))
        DEV_Delay_ms(5);
}

/* Window coordinates: x0..x1, y0..y1 inclusive, x0/x1 8-pixel aligned. */
static void send_window_pixels(const uint8_t *fb, size_t row_bytes, int x0,
                               int x1, int y0, int y1)
{
    send_command(0x10); /* data start transmission */
    for (int y = y0; y <= y1; y++)
        for (int b = x0 / 4; b <= x1 / 4; b++)
            send_data(fb[(size_t)y * row_bytes + (size_t)b]);
}

static void send_ptl_params(int x0, int x1, int y0, int y1)
{
    send_u16(x0);
    send_u16(x1);
    send_u16(y0);
    send_u16(y1);
    send_data(0x01); /* PT_SCAN: scan inside the window only */
}

static void refresh(void)
{
    send_command(0x12); /* display refresh */
    send_data(0x00);
    wait_idle();
}

int rw_epd2in15g_refresh_window(const uint8_t *fb, size_t row_bytes, int x,
                                int y, int w, int h, rw_partial_method method)
{
    int x0 = x, x1 = x + w - 1, y0 = y, y1 = y + h - 1;

    switch (method) {
    case RW_PARTIAL_PTL:
        EPD_2IN15G_Init();
        send_command(0x91); /* partial in */
        send_command(0x90); /* partial window */
        send_ptl_params(x0, x1, y0, y1);
        send_window_pixels(fb, row_bytes, x0, x1, y0, y1);
        refresh();
        send_command(0x92); /* partial out */
        break;

    case RW_PARTIAL_PTL83:
        EPD_2IN15G_Init();
        send_command(0x83); /* partial window */
        send_ptl_params(x0, x1, y0, y1);
        send_window_pixels(fb, row_bytes, x0, x1, y0, y1);
        refresh();
        break;

    case RW_PARTIAL_WINDOW:
        EPD_2IN15G_Init();
        send_command(0x61); /* resolution = window size */
        send_u16(w);
        send_u16(h);
        send_command(0x65); /* start position = window origin */
        send_u16(x0);
        send_u16(y0);
        send_window_pixels(fb, row_bytes, x0, x1, y0, y1);
        refresh();
        break;

    default:
        return -1;
    }

    EPD_2IN15G_Sleep();
    return 0;
}
