# Build:   make            -> build/rps.hex (flash to an Arduino Uno / ATmega328P)
# Flash:   make flash PORT=/dev/ttyUSB0
# Test:    make test       -> runs host-side unit tests for rules + protocol

MCU    = atmega328p
F_CPU  = 16000000UL
CXX    = avr-g++
OBJCOPY= avr-objcopy
SIZE   = avr-size
CXXFLAGS = -mmcu=$(MCU) -DF_CPU=$(F_CPU) -Os -std=gnu++11 -Wall -Wextra \
           -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti
LDFLAGS  = -mmcu=$(MCU) -Wl,--gc-sections

SRC = firmware/main.cpp firmware/protocol.cpp firmware/uart.cpp \
      firmware/timebase.cpp firmware/lcd_i2c.cpp
OBJ = $(SRC:firmware/%.cpp=build/%.o)

all: build/rps.hex

build/%.o: firmware/%.cpp firmware/*.h | build
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/rps.elf: $(OBJ)
	$(CXX) $(LDFLAGS) $^ -o $@
	$(SIZE) --mcu=$(MCU) -C $@ 2>/dev/null || $(SIZE) $@

build/rps.hex: build/rps.elf
	$(OBJCOPY) -O ihex -R .eeprom $< $@

build:
	mkdir -p build

PORT ?= /dev/ttyUSB0
flash: build/rps.hex
	avrdude -p $(MCU) -c arduino -P $(PORT) -b 115200 -U flash:w:$<:i

test: | build
	g++ -std=c++11 -Wall -Wextra -Ifirmware tests/test_protocol.cpp firmware/protocol.cpp -o build/test_protocol
	./build/test_protocol

clean:
	rm -rf build

.PHONY: all flash test clean
