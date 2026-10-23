// timebase.h - 1 kHz system tick (Timer1) and debounced push buttons.
#pragma once
#include <stdint.h>

enum Button : uint8_t { kButtonCycle = 0, kButtonLock = 1, kButtonCount = 2 };

void timebase_init();
uint32_t millis();
bool button_pressed(Button b);  // true once per press (edge event, debounced)
