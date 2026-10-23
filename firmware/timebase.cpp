// timebase.cpp - Timer1 compare-match interrupt every 1 ms drives the
// system clock and samples the buttons with an integrating debouncer.
#include "timebase.h"

#include <avr/interrupt.h>
#include <avr/io.h>
#include <util/atomic.h>

namespace {
// Buttons on PD2 (cycle) and PD3 (lock), active-low with internal pull-ups.
constexpr uint8_t kPins[kButtonCount] = {PD2, PD3};
constexpr uint8_t kDebounceMs = 20;

volatile uint32_t ticks = 0;
volatile uint8_t integrator[kButtonCount] = {};
volatile bool pressed[kButtonCount] = {};
volatile bool event[kButtonCount] = {};
}  // namespace

void timebase_init() {
    DDRD &= ~(_BV(PD2) | _BV(PD3));   // inputs
    PORTD |= _BV(PD2) | _BV(PD3);     // pull-ups on

    TCCR1A = 0;
    TCCR1B = _BV(WGM12) | _BV(CS11) | _BV(CS10);           // CTC mode, clk/64
    OCR1A = static_cast<uint16_t>(F_CPU / 64 / 1000 - 1);  // 1 ms period
    TIMSK1 = _BV(OCIE1A);
}

uint32_t millis() {
    uint32_t t;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) { t = ticks; }  // 32-bit read is not atomic on AVR
    return t;
}

bool button_pressed(Button b) {
    bool e;
    ATOMIC_BLOCK(ATOMIC_RESTORESTATE) {
        e = event[b];
        event[b] = false;
    }
    return e;
}

ISR(TIMER1_COMPA_vect) {
    ++ticks;
    const uint8_t pins = PIND;
    for (uint8_t i = 0; i < kButtonCount; ++i) {
        const bool raw = !(pins & _BV(kPins[i]));  // low = pressed
        // Integrator must see a stable level for kDebounceMs before the state flips.
        if (raw && integrator[i] < kDebounceMs) ++integrator[i];
        if (!raw && integrator[i] > 0) --integrator[i];

        if (!pressed[i] && integrator[i] == kDebounceMs) {
            pressed[i] = true;
            event[i] = true;  // report the press edge once
        } else if (pressed[i] && integrator[i] == 0) {
            pressed[i] = false;
        }
    }
}
