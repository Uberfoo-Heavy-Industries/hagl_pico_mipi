/*

MIT License

Copyright (c) 2019-2023 Mika Tuupola

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

#include <stdio.h>
#include <stdlib.h>
// #include <string.h>
// #include <stdatomic.h>

#include <hardware/spi.h>
#include <hardware/dma.h>
#include <hardware/gpio.h>
#include <hardware/clocks.h>
#include <pico/time.h>

#include "mipi_dcs.h"
#include "mipi_display.h"

static int dma_channel;

static inline uint16_t
htons(uint16_t i)
{
    __asm ("rev16 %0, %0" : "+l" (i) : : );
    return i;
}

static void
mipi_display_write_command(mipi_display_config_t *display_config, const uint8_t command)
{
    /* Set DC low to denote incoming command. */
    gpio_put(display_config->pin_dc, 0);

    /* Set CS low to reserve the SPI bus. */
    gpio_put(display_config->pin_cs, 0);

    spi_write_blocking(display_config->spi, &command, 1);

    /* Set CS high to ignore any traffic on SPI bus. */
    gpio_put(display_config->pin_cs, 1);
}

static void
mipi_display_write_data(mipi_display_config_t *display_config, const uint8_t *data, size_t length)
{
    size_t sent = 0;

    if (0 == length) {
        return;
    };

    /* Set DC high to denote incoming data. */
    gpio_put(display_config->pin_dc, 1);

    /* Set CS low to reserve the SPI bus. */
    gpio_put(display_config->pin_cs, 0);

    for (size_t i = 0; i < length; ++i) {
        while (!spi_is_writable(display_config->spi)) {};
        spi_get_hw(display_config->spi)->dr = (uint32_t) data[i];
    }

    /* Wait for shifting to finish. */
    while (spi_get_hw(display_config->spi)->sr & SPI_SSPSR_BSY_BITS) {};
    spi_get_hw(display_config->spi)->icr = SPI_SSPICR_RORIC_BITS;

    /* Set CS high to ignore any traffic on SPI bus. */
    gpio_put(display_config->pin_cs, 1);
}

static void
mipi_display_write_data_dma(mipi_display_config_t *display_config, const uint8_t *buffer, size_t length)
{
    if (0 == length) {
        return;
    };

    /* Set DC high to denote incoming data. */
    gpio_put(display_config->pin_dc, 1);

    /* Set CS low to reserve the SPI bus. */
    gpio_put(display_config->pin_cs, 0);

    dma_channel_wait_for_finish_blocking(dma_channel);
    dma_channel_set_trans_count(dma_channel, length, false);
    dma_channel_set_read_addr(dma_channel, buffer, true);
}

static void
mipi_display_dma_init(mipi_display_config_t *display_config)
{
    hagl_hal_debug("%s\n", "initialising DMA.");

    dma_channel = dma_claim_unused_channel(true);
    dma_channel_config channel_config = dma_channel_get_default_config(dma_channel);
    channel_config_set_transfer_data_size(&channel_config, DMA_SIZE_8);
    if (spi0 == display_config->spi) {
        channel_config_set_dreq(&channel_config, DREQ_SPI0_TX);
    } else {
        channel_config_set_dreq(&channel_config, DREQ_SPI1_TX);
    }
    dma_channel_set_config(dma_channel, &channel_config, false);
    dma_channel_set_write_addr(dma_channel, &spi_get_hw(display_config->spi)->dr, false);
}

static void
mipi_display_read_data(mipi_display_config_t *display_config, uint8_t *data, size_t length)
{
    if (0 == length) {
        return;
    };
}

static void
mipi_display_set_address_xyxy(mipi_display_config_t *display_config, uint16_t x1, uint16_t y1, uint16_t x2, uint16_t y2)
{
    uint8_t command;
    uint8_t data[4];

    x1 = x1 + display_config->offset_x;
    y1 = y1 + display_config->offset_y;
    x2 = x2 + display_config->offset_x;
    y2 = y2 + display_config->offset_y;

    /* Change column address only if it has changed. */
    if ((display_config->prev_clip.x0 != x1 || display_config->prev_clip.x1 != x2)) {
        mipi_display_write_command(display_config, MIPI_DCS_SET_COLUMN_ADDRESS);
        data[0] = x1 >> 8;
        data[1] = x1 & 0xff;
        data[2] = x2 >> 8;
        data[3] = x2 & 0xff;
        mipi_display_write_data(display_config, data, 4);

        display_config->prev_clip.x0 = x1;
        display_config->prev_clip.x1 = x2;
    }

    /* Change page address only if it has changed. */
    if ((display_config->prev_clip.y0 != y1 || display_config->prev_clip.y1 != y2)) {
        mipi_display_write_command(display_config, MIPI_DCS_SET_PAGE_ADDRESS);
        data[0] = y1 >> 8;
        data[1] = y1 & 0xff;
        data[2] = y2 >> 8;
        data[3] = y2 & 0xff;
        mipi_display_write_data(display_config, data, 4);

        display_config->prev_clip.y0 = y1;
        display_config->prev_clip.y1 = y2;
    }

    mipi_display_write_command(display_config, MIPI_DCS_WRITE_MEMORY_START);
}

static void
mipi_display_set_address_xy(mipi_display_config_t *display_config, uint16_t x1, uint16_t y1)
{
    uint8_t command;
    uint8_t data[2];

    x1 = x1 + display_config->offset_x;
    y1 = y1 + display_config->offset_y;

    mipi_display_write_command(display_config, MIPI_DCS_SET_COLUMN_ADDRESS);
    data[0] = x1 >> 8;
    data[1] = x1 & 0xff;
    mipi_display_write_data(display_config, data, 2);

    mipi_display_write_command(display_config, MIPI_DCS_SET_PAGE_ADDRESS);
    data[0] = y1 >> 8;
    data[1] = y1 & 0xff;
    mipi_display_write_data(display_config, data, 2);

    mipi_display_write_command(display_config, MIPI_DCS_WRITE_MEMORY_START);
}

static void
mipi_display_spi_master_init(mipi_display_config_t *display_config)
{
    if (display_config->init_spi > 0) {
        hagl_hal_debug("%s\n", "Initialising SPI.");

        gpio_set_function(display_config->pin_dc, GPIO_FUNC_SIO);
        gpio_set_dir(display_config->pin_dc, GPIO_OUT);

        gpio_set_function(display_config->pin_clk,  GPIO_FUNC_SPI);
        gpio_set_function(display_config->pin_mosi, GPIO_FUNC_SPI);

        if (display_config->pin_miso > 0) {
            gpio_set_function(display_config->pin_miso, GPIO_FUNC_SPI);
        }

        /* Set CS high to ignore any traffic on SPI bus. */
        gpio_put(display_config->pin_cs, 1);

        spi_init(display_config->spi, display_config->spi_freq);
        spi_set_format(display_config->spi, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
        uint32_t baud = spi_set_baudrate(display_config->spi, display_config->spi_freq);
        uint32_t peri = clock_get_hz(clk_peri);
        uint32_t sys = clock_get_hz(clk_sys);
        hagl_hal_debug("Baudrate is set to %d.\n", baud);
        hagl_hal_debug("clk_peri %d.\n", peri);
        hagl_hal_debug("clk_sys %d.\n", sys);
    } else {
        hagl_hal_debug("skipping spy init %d.\n", (void *)display_config);
    }

}

void
mipi_display_init(mipi_display_config_t *display_config)
{
#ifdef HAGL_HAL_USE_SINGLE_BUFFER
    hagl_hal_debug("%s\n", "Initialising single buffered display.");
#endif /* HAGL_HAL_USE_SINGLE_BUFFER */

#ifdef HAGL_HAL_USE_DOUBLE_BUFFER
    hagl_hal_debug("%s\n", "Initialising double buffered display.");
#endif /* HAGL_HAL_USE_DOUBLE_BUFFER */

#ifdef HAGL_HAL_USE_TRIPLE_BUFFER
    hagl_hal_debug("%s\n", "Initialising triple buffered display.");
#endif /* HAGL_HAL_USE_DOUBLE_BUFFER */

    /* Init the spi driver. */
    mipi_display_spi_master_init(display_config);
    sleep_ms(100);

    /* Reset the display. */
    if (display_config->pin_rst > 0) {
        gpio_set_function(display_config->pin_rst, GPIO_FUNC_SIO);
        gpio_set_dir(display_config->pin_rst, GPIO_OUT);

        gpio_put(display_config->pin_rst, 0);
        sleep_ms(100);
        gpio_put(display_config->pin_rst, 1);
        sleep_ms(100);
    }

    /* Send minimal init commands. */
    mipi_display_write_command(display_config, MIPI_DCS_SOFT_RESET);
    sleep_ms(200);

    mipi_display_write_command(display_config, MIPI_DCS_SET_ADDRESS_MODE);
    mipi_display_write_data(display_config, &(uint8_t) {display_config->address_mode}, 1);

    mipi_display_write_command(display_config, MIPI_DCS_SET_PIXEL_FORMAT);
    mipi_display_write_data(display_config, &(uint8_t) {display_config->pixel_format}, 1);

    if (display_config->pin_te > 0) {
        mipi_display_write_command(display_config, MIPI_DCS_SET_TEAR_ON);
        mipi_display_write_data(display_config, &(uint8_t) {MIPI_DCS_SET_TEAR_ON_VSYNC}, 1);
        hagl_hal_debug("Enable vsync notification on pin %d\n", display_config->pin_te);
    }

    if (display_config->invert > 0) {
        mipi_display_write_command(display_config, MIPI_DCS_ENTER_INVERT_MODE);
        hagl_hal_debug("%s\n", "Inverting display.");
    } else {
        mipi_display_write_command(display_config, MIPI_DCS_EXIT_INVERT_MODE);
    }

    mipi_display_write_command(display_config, MIPI_DCS_EXIT_SLEEP_MODE);
    sleep_ms(200);

    mipi_display_write_command(display_config, MIPI_DCS_SET_DISPLAY_ON);
    sleep_ms(200);

    /* Enable backlight */
    if (display_config->pin_bl > 0) {
        gpio_set_function(display_config->pin_bl, GPIO_FUNC_SIO);
        gpio_set_dir(display_config->pin_bl, GPIO_OUT);
        gpio_put(display_config->pin_bl, 1);
    }

    /* Enable power */
    if (display_config->pin_power > 0) {
        gpio_set_function(display_config->pin_power, GPIO_FUNC_SIO);
        gpio_set_dir(display_config->pin_power, GPIO_OUT);
        gpio_put(display_config->pin_power, 1);
    }

    /* Initialise vsync pin */
    if (display_config->pin_te > 0) {
        gpio_set_function(display_config->pin_te, GPIO_FUNC_SIO);
        gpio_set_dir(display_config->pin_te, GPIO_IN);
        gpio_pull_up(display_config->pin_te);
    }

    /* Set the default viewport to full screen. */
    mipi_display_set_address_xyxy(display_config, 0, 0, display_config->width - 1, display_config->height - 1);

#ifdef HAGL_HAS_HAL_BACK_BUFFER
#ifdef HAGL_HAL_USE_DMA
    mipi_display_dma_init();
#endif /* HAGL_HAL_USE_DMA */
#endif /* HAGL_HAS_HAL_BACK_BUFFER */
}

size_t
mipi_display_fill_xywh(mipi_display_config_t *display_config, uint16_t x1, uint16_t y1, uint16_t w, uint16_t h, void *_color)
{
    if (0 == w || 0 == h) {
        return 0;
    }

    int32_t x2 = x1 + w - 1;
    int32_t y2 = y1 + h - 1;
    size_t size = w * h;
    uint16_t *color = _color;

    mipi_display_set_address_xyxy(display_config, x1, y1, x2, y2);

    /* Set DC high to denote incoming data. */
    gpio_put(display_config->pin_dc, 1);

    /* Set CS low to reserve the SPI bus. */
    gpio_put(display_config->pin_cs, 0);

    /* TODO: This assumes 16 bit colors. */
    spi_set_format(display_config->spi, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    while (size--) {
        while (!spi_is_writable(display_config->spi)) {};
        spi_get_hw(display_config->spi)->dr = (uint32_t) htons(*color);
    }

    /* Wait for shifting to finish. */
    while (spi_get_hw(display_config->spi)->sr & SPI_SSPSR_BSY_BITS) {};
    spi_get_hw(display_config->spi)->icr = SPI_SSPICR_RORIC_BITS;

    spi_set_format(display_config->spi, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

    /* Set CS high to ignore any traffic on SPI bus. */
    gpio_put(display_config->pin_cs, 1);

    return size;
}

size_t
mipi_display_write_xywh(mipi_display_config_t *display_config, uint16_t x1, uint16_t y1, uint16_t w, uint16_t h, uint8_t *buffer)
{
    if (0 == w || 0 == h) {
        return 0;
    }

    int32_t x2 = x1 + w - 1;
    int32_t y2 = y1 + h - 1;
    uint32_t size = w * h;

#ifdef HAGL_HAL_USE_SINGLE_BUFFER
    mipi_display_set_address_xyxy(display_config, x1, y1, x2, y2);
    mipi_display_write_data(display_config, buffer, size * display_config->depth / 8);
#endif /* HAGL_HAL_SINGLE_BUFFER */

#ifdef HAGL_HAS_HAL_BACK_BUFFER
    mipi_display_set_address_xyxy(display_config, x1, y1, x2, y2);
#ifdef HAGL_HAL_USE_DMA
    mipi_display_write_data_dma(display_config, buffer, size * display_config->depth / 8);
#else
    mipi_display_write_data(display_config, buffer, size * display_config->depth / 8);
#endif /* HAGL_HAL_USE_DMA */
#endif /* HAGL_HAS_HAL_BACK_BUFFER */
    /* This should also include the bytes for writing the commands. */
    return size * (display_config->depth / 8);
}

size_t
mipi_display_write_xy(mipi_display_config_t *display_config, uint16_t x1, uint16_t y1, uint8_t *buffer)
{
    mipi_display_set_address_xy(display_config, x1, y1);
    mipi_display_write_data(display_config, buffer, display_config->depth / 8);

    /* This should also include the bytes for writing the commands. */
    return display_config->depth / 8;
}

/* TODO: This most likely does not work with dma atm. */
void
mipi_display_ioctl(mipi_display_config_t *display_config, const uint8_t command, uint8_t *data, size_t size)
{
    switch (command) {
        case MIPI_DCS_GET_COMPRESSION_MODE:
        case MIPI_DCS_GET_DISPLAY_ID:
        case MIPI_DCS_GET_RED_CHANNEL:
        case MIPI_DCS_GET_GREEN_CHANNEL:
        case MIPI_DCS_GET_BLUE_CHANNEL:
        case MIPI_DCS_GET_DISPLAY_STATUS:
        case MIPI_DCS_GET_POWER_MODE:
        case MIPI_DCS_GET_ADDRESS_MODE:
        case MIPI_DCS_GET_PIXEL_FORMAT:
        case MIPI_DCS_GET_DISPLAY_MODE:
        case MIPI_DCS_GET_SIGNAL_MODE:
        case MIPI_DCS_GET_DIAGNOSTIC_RESULT:
        case MIPI_DCS_GET_SCANLINE:
        case MIPI_DCS_GET_DISPLAY_BRIGHTNESS:
        case MIPI_DCS_GET_CONTROL_DISPLAY:
        case MIPI_DCS_GET_POWER_SAVE:
        case MIPI_DCS_READ_DDB_START:
        case MIPI_DCS_READ_DDB_CONTINUE:
            mipi_display_write_command(display_config, command);
            mipi_display_read_data(display_config, data, size);
            break;
        default:
            mipi_display_write_command(display_config, command);
            mipi_display_write_data(display_config, data, size);
    }
}

void
mipi_display_close(mipi_display_config_t *display_config)
{
    spi_deinit(display_config->spi);
}