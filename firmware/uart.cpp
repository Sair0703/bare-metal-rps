// uart.cpp - Interrupt-driven USART0 driver with RX/TX ring buffers.
// The main loop never blocks on the serial port: bytes are queued and the
// RX-complete / data-register-empty interrupts move them in and out.
#include "uart.h"

#include <avr/interrupt.h>
#include <avr/io.h>

namespace {
constexpr uint8_t kBufSize = 64;  // must be a power of two
constexpr uint8_t kMask = kBufSize - 1;

volatile uint8_t rxBuf[kBufSize];
volatile uint8_t rxHead = 0, rxTail = 0;
volatile uint8_t txBuf[kBufSize];
volatile uint8_t txHead = 0, txTail = 0;
volatile uint16_t rxOverflows = 0;
}  // namespace

void uart_init(uint32_t baud) {
    const uint16_t ubrr = static_cast<uint16_t>(F_CPU / (8UL * baud) - 1);  // double-speed mode
    UCSR0A = _BV(U2X0);
    UBRR0H = static_cast<uint8_t>(ubrr >> 8);
    UBRR0L = static_cast<uint8_t>(ubrr);
    UCSR0C = _BV(UCSZ01) | _BV(UCSZ00);               // 8 data bits, no parity, 1 stop bit
    UCSR0B = _BV(RXEN0) | _BV(TXEN0) | _BV(RXCIE0);   // enable RX, TX, RX interrupt
}

bool uart_read(uint8_t& byte) {
    if (rxHead == rxTail) return false;
    byte = rxBuf[rxTail];
    rxTail = (rxTail + 1) & kMask;
    return true;
}

bool uart_write(uint8_t byte) {
    const uint8_t next = (txHead + 1) & kMask;
    if (next == txTail) return false;  // queue full: caller decides what to do
    txBuf[txHead] = byte;
    txHead = next;
    UCSR0B |= _BV(UDRIE0);  // wake the TX interrupt
    return true;
}

uint16_t uart_rx_overflows() { return rxOverflows; }

ISR(USART_RX_vect) {
    const uint8_t byte = UDR0;
    const uint8_t next = (rxHead + 1) & kMask;
    if (next == rxTail) {
        ++rxOverflows;  // drop the byte; the protocol's resends recover
        return;
    }
    rxBuf[rxHead] = byte;
    rxHead = next;
}

ISR(USART_UDRE_vect) {
    if (txHead == txTail) {
        UCSR0B &= ~_BV(UDRIE0);  // nothing left to send
        return;
    }
    UDR0 = txBuf[txTail];
    txTail = (txTail + 1) & kMask;
}
