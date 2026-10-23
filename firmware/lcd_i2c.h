// lcd_i2c.h - HD44780 16x2 character LCD behind a PCF8574 I2C backpack.
#pragma once
#include <stdint.h>

bool lcd_init(uint8_t i2cAddress);              // false if the LCD did not ACK
void lcd_print_line(uint8_t row, const char* text);  // writes a full, padded row
