# Bare-Metal Two-Board Rock-Paper-Scissors (ATmega328P)

Two microcontrollers play rock-paper-scissors against each other over a wired
serial link. The firmware is register-level C++ for the ATmega328P (Arduino Uno)
with **no Arduino libraries**: every peripheral driver is written from scratch.

**[Try the live demo](https://sair0703.github.io/bare-metal-rps/)**: a browser simulation of both boards running the same protocol and game logic, with a line-noise control to watch the link recover from lost and corrupted bytes.

## What's inside

| Module | What it does |
|---|---|
| `firmware/uart.cpp` | Interrupt-driven USART0 driver with RX/TX ring buffers, so the main loop never blocks on serial I/O |
| `firmware/timebase.cpp` | Timer1 compare-match interrupt at 1 kHz: system clock plus an integrating debouncer for the buttons |
| `firmware/lcd_i2c.cpp` | Bare-metal TWI (I2C) master with timeouts, and an HD44780 LCD driver in 4-bit mode via a PCF8574 expander |
| `firmware/protocol.cpp` | Game rules, CRC-8 checksum, frame encoding, and a byte-at-a-time frame parser |
| `firmware/main.cpp` | Game state machine and link logic |
| `tests/test_protocol.cpp` | Host-side unit tests for the rules and the protocol (no hardware needed) |
| `docs/index.html` | Interactive browser demo, served by GitHub Pages |

## How the two boards talk

Each message is a 5-byte frame: `[0xA5 sync][type][round][payload][CRC-8]`.

- **Lock before reveal.** A board first sends `READY`, which does not contain its move. It sends `REVEAL` with the move only after it has heard the opponent is ready too. Neither board can see the other's move before committing to its own.
- **Self-healing link.** Messages are re-sent every 100 ms until the game moves on. Every frame carries the round number, so stale or duplicate messages are ignored, and a dropped or corrupted byte only causes a short delay.
- **Can't deadlock.** If one board moves to the next round before the other has its move, it keeps re-sending that move until the other board catches up.
- **Resynchronizing parser.** If a lost byte knocks the parser out of step, it rescans what it has buffered for the next sync byte instead of discarding the next frame too.
- **No master/slave.** Both boards run the identical firmware, and the protocol is symmetric.

Game flow: pick a move with **Cycle**, commit with **Lock**, see the result, and play to 3 points.

## Wiring (per board)

| Part | Pin |
|---|---|
| Cycle button | D2 to GND (internal pull-up) |
| Lock button | D3 to GND (internal pull-up) |
| 16x2 LCD with I2C backpack (address 0x27) | SDA to A4, SCL to A5, 5V, GND |
| Link to the other board | TX (D1) to the other board's RX (D0), and vice versa, plus a shared GND |

The on-board LED (pin 13) toggles on every valid frame received, which is handy for checking the link.

## Build, test, run

Requires `avr-gcc` / `avr-libc` (`sudo apt install gcc-avr avr-libc avrdude` on Linux, or `brew tap osx-cross/avr && brew install avr-gcc avrdude` on macOS).

```
make            # builds build/rps.hex (about 3 KB flash)
make test       # runs unit tests on your computer
make flash PORT=/dev/ttyUSB0   # flash a real Arduino Uno
```

**Simulate:** `diagram.json` and `wokwi.toml` describe the two-board circuit for the [Wokwi](https://wokwi.com) simulator (VS Code extension). Note: disconnect the board-to-board TX/RX wires while flashing real Unos over USB, since D0/D1 are shared with the USB serial chip.
