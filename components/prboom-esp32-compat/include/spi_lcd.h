#ifndef DOOM_SPI_LCD_H
#define DOOM_SPI_LCD_H
#include <stdint.h>
void spi_lcd_wait_finish(void);
void spi_lcd_send(uint16_t *scr);
void spi_lcd_init(void);
#endif
