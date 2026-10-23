// lcd_i2c.cpp - Bare-metal TWI (I2C) master plus an HD44780 driver in 4-bit
// mode, talking through a PCF8574 I/O expander (P0=RS, P2=EN, P3=backlight,
// P4..P7=D4..D7).
#include "lcd_i2c.h"

#include <avr/io.h>
#include <util/delay.h>

namespace {
uint8_t lcdAddr = 0x27;
constexpr uint8_t kRS = 0x01, kEN = 0x04, kBacklight = 0x08;
constexpr uint16_t kTwiTimeout = 2000;

// ---- TWI master (polling, with timeouts so a missing device can't hang us) ----
bool twi_wait() {
    for (uint16_t i = 0; i < kTwiTimeout; ++i)
        if (TWCR & _BV(TWINT)) return true;
    return false;
}

bool twi_start(uint8_t addr) {
    TWCR = _BV(TWINT) | _BV(TWSTA) | _BV(TWEN);
    if (!twi_wait()) return false;
    TWDR = static_cast<uint8_t>(addr << 1);  // write mode
    TWCR = _BV(TWINT) | _BV(TWEN);
    if (!twi_wait()) return false;
    return (TWSR & 0xF8) == 0x18;  // SLA+W transmitted, ACK received
}

bool twi_write(uint8_t data) {
    TWDR = data;
    TWCR = _BV(TWINT) | _BV(TWEN);
    if (!twi_wait()) return false;
    return (TWSR & 0xF8) == 0x28;  // data transmitted, ACK received
}

void twi_stop() { TWCR = _BV(TWINT) | _BV(TWSTO) | _BV(TWEN); }

bool expander_write(uint8_t value) {
    const bool ok = twi_start(lcdAddr) && twi_write(value | kBacklight);
    twi_stop();
    return ok;
}

// ---- HD44780 in 4-bit mode ----
void pulse_nibble(uint8_t nibbleWithFlags) {
    expander_write(nibbleWithFlags | kEN);
    _delay_us(1);
    expander_write(nibbleWithFlags & ~kEN);
    _delay_us(50);
}

void lcd_send(uint8_t value, uint8_t flags) {
    pulse_nibble((value & 0xF0) | flags);
    pulse_nibble(static_cast<uint8_t>((value << 4) & 0xF0) | flags);
}

void lcd_command(uint8_t cmd) { lcd_send(cmd, 0); }
void lcd_data(uint8_t ch) { lcd_send(ch, kRS); }
}  // namespace

bool lcd_init(uint8_t i2cAddress) {
    lcdAddr = i2cAddress;
    TWSR = 0;                                               // prescaler = 1
    TWBR = static_cast<uint8_t>((F_CPU / 100000UL - 16) / 2);  // 100 kHz SCL

    _delay_ms(50);  // LCD power-up time
    if (!expander_write(0)) return false;

    // HD44780 datasheet init sequence: force 8-bit mode three times, then switch to 4-bit.
    pulse_nibble(0x30); _delay_ms(5);
    pulse_nibble(0x30); _delay_us(150);
    pulse_nibble(0x30);
    pulse_nibble(0x20);

    lcd_command(0x28);  // 4-bit bus, 2 lines, 5x8 font
    lcd_command(0x0C);  // display on, cursor off
    lcd_command(0x06);  // auto-increment cursor
    lcd_command(0x01);  // clear
    _delay_ms(2);
    return true;
}

void lcd_print_line(uint8_t row, const char* text) {
    lcd_command(static_cast<uint8_t>(0x80 | (row ? 0x40 : 0x00)));  // DDRAM address of row start
    uint8_t col = 0;
    for (; text[col] && col < 16; ++col) lcd_data(static_cast<uint8_t>(text[col]));
    for (; col < 16; ++col) lcd_data(' ');  // pad so old text is overwritten
}
