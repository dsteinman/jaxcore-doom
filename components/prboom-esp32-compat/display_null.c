/* SPDX-License-Identifier: GPL-2.0-or-later */
#include "spi_lcd.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

/* Build/engine bring-up only: no display GPIOs are touched. */
void spi_lcd_init(void) {}
void spi_lcd_wait_finish(void) {}
void spi_lcd_send(uint16_t *pixels)
{
    (void)pixels;
    vTaskDelay(1); /* let the idle task/watchdog run in headless mode */
}
