#
# Waveshare Pico-LCD-0.96 (80x160 ST7735S)
# https://www.waveshare.com/wiki/Pico-LCD-0.96
# https://botland.store/search?s=5904422371869
#
target_compile_definitions(firmware PRIVATE
    MIPI_DISPLAY_PIN_CS=9
    MIPI_DISPLAY_PIN_DC=8
    MIPI_DISPLAY_PIN_RST=12
    MIPI_DISPLAY_PIN_BL=13
    MIPI_DISPLAY_PIN_CLK=10
    MIPI_DISPLAY_PIN_MOSI=11
    MIPI_DISPLAY_PIN_MISO=-1
    MIPI_DISPLAY_PIN_POWER=-1

    MIPI_DISPLAY_SPI_PORT=spi1
    MIPI_DISPLAY_SPI_CLOCK_SPEED_HZ=62500000

    MIPI_DISPLAY_PIXEL_FORMAT=MIPI_DCS_PIXEL_FORMAT_16BIT
    MIPI_DISPLAY_ADDRESS_MODE=MIPI_DCS_ADDRESS_MODE_BGR|MIPI_DCS_ADDRESS_MODE_MIRROR_Y|MIPI_DCS_ADDRESS_MODE_MIRROR_X
    MIPI_DISPLAY_WIDTH=80
    MIPI_DISPLAY_HEIGHT=160
    MIPI_DISPLAY_OFFSET_X=26
    MIPI_DISPLAY_OFFSET_Y=1
    # MIPI_DISPLAY_ADDRESS_MODE=MIPI_DCS_ADDRESS_MODE_BGR|MIPI_DCS_ADDRESS_MODE_SWAP_XY|MIPI_DCS_ADDRESS_MODE_MIRROR_Y
    # MIPI_DISPLAY_WIDTH=160
    # MIPI_DISPLAY_HEIGHT=80
    # MIPI_DISPLAY_OFFSET_X=1
    # MIPI_DISPLAY_OFFSET_Y=26
    MIPI_DISPLAY_INVERT=1
)