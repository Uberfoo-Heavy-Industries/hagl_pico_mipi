/*

MIT License

Copyright (c) 2021-2023 Mika Tuupola

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

-cut-

This file is part of the Raspberry Pi Pico MIPI DCS backend for the HAGL
graphics library: https://github.com/tuupola/hagl_pico_mipi

SPDX-License-Identifier: MIT

*/

#ifndef _HAGL_HAL_H
#define _HAGL_HAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <hardware/spi.h>

#include <hagl/backend.h>

#include "hagl_hal_color.h"

#define hagl_hal_debug(fmt, ...) \
    do { if (HAGL_HAL_DEBUG) printf("[HAGL HAL] " fmt, __VA_ARGS__); } while (0)

/* Default config is ok for Waveshare RP2040-LCD-0.96 */
/* https://www.waveshare.com/wiki/RP2040-LCD-0.96     */
/* https://botland.store/search?s=5904422381578       */


#ifndef HAGL_HAL_PIXEL_SIZE
#define HAGL_HAL_PIXEL_SIZE         (1)
#endif

/* These are used internally. */
#define HAGL_PICO_MIPI_DISPLAY_WIDTH      (MIPI_DISPLAY_WIDTH / HAGL_HAL_PIXEL_SIZE)
#define HAGL_PICO_MIPI_DISPLAY_HEIGHT     (MIPI_DISPLAY_HEIGHT / HAGL_HAL_PIXEL_SIZE)
#define HAGL_PICO_MIPI_DISPLAY_DEPTH      (MIPI_DISPLAY_DEPTH)

#ifdef HAGL_HAL_USE_TRIPLE_BUFFER
#define HAGL_HAS_HAL_BACK_BUFFER
#endif

#ifdef HAGL_HAL_USE_DOUBLE_BUFFER
#define HAGL_HAS_HAL_BACK_BUFFER
#endif

#ifdef HAGL_HAL_USE_SINGLE_BUFFER
#undef HAGL_HAS_HAL_BACK_BUFFER
#endif

typedef struct {
    uint32_t    spi_freq;
    spi_inst_t  *spi;
    int16_t     pin_cs;
    int16_t     pin_dc;
    int16_t     pin_rst;
    int16_t     pin_bl;
    int16_t     pin_clk;
    int16_t     pin_mosi;
    int16_t     pin_miso;
    int16_t     pin_power;
    int16_t     pin_te;
    uint8_t     pixel_format;
    uint8_t     address_mode;
    uint16_t    width, height, offset_x, offset_y;
    uint8_t     depth;
    int8_t      invert;
    int8_t      init_spi;
    hagl_window_t prev_clip;
    hagl_bitmap_t *bb;
    void *(*haglCalloc)(size_t, size_t);
} mipi_display_config_t;

#define GET_MIPI_DISPLAY_CONFIG(self)         (mipi_display_config_t *)((hagl_backend_t *)self)->display_config
#define GET_BB(self)                          (GET_MIPI_DISPLAY_CONFIG(self))->bb

/**
 * Initialize the HAL
 */
void hagl_hal_init(hagl_backend_t *backend);

#ifdef __cplusplus
}
#endif
#endif /* _HAGL_HAL_H */