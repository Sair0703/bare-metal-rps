// uart.h - Interrupt-driven USART0 driver (ATmega328P).
#pragma once
#include <stdint.h>

void uart_init(uint32_t baud);
bool uart_read(uint8_t& byte);   // non-blocking; false if nothing received
bool uart_write(uint8_t byte);   // non-blocking; false if TX queue is full
uint16_t uart_rx_overflows();
