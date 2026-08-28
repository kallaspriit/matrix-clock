#pragma once

#include <Arduino.h>

// Hardware wiring
constexpr uint8_t DATA_PIN = 19;
constexpr int16_t MATRIX_WIDTH = 32;
constexpr int16_t MATRIX_HEIGHT = 8;
constexpr uint16_t NUM_LEDS = MATRIX_WIDTH * MATRIX_HEIGHT;

// Power budget. The onboard regulator is good for roughly 3A continuous so we leave some headroom.
// All 256 LEDs at full white would pull ~15A, FastLED scales brightness down to respect this.
constexpr uint8_t SUPPLY_VOLTS = 5;
constexpr uint32_t MAX_MILLIAMPS = 2500;

// Measured on the bench: 1.22W idle floor from the 256 controller ICs, plus a share that scales
// linearly with brightness. 24 draws 1.75W, 128 draws 4W, and full 255 would still only be ~6.9W.
// The 2.5A cap is there for full screen content, a clock face never gets near it.
constexpr uint8_t DEFAULT_BRIGHTNESS = 128;

// Rendering is frame limited so the main loop is not just spinning on FastLED.show()
constexpr uint32_t TARGET_FPS = 60;
constexpr uint32_t FRAME_INTERVAL_MS = 1000 / TARGET_FPS;

constexpr uint32_t SERIAL_BAUD = 115200;
constexpr size_t SERIAL_LINE_LIMIT = 128;

// Wiring permutation the panel actually uses. Boot into the "corners" test, run "layout next"
// until it reads correctly, then copy the reported bits here so it comes up right every time.
// Confirmed against the panel with the 'origin' test: LED 0 sits bottom right (next to the data
// input connector) and the chain finishes at bottom left, snaking up and down in columns of 8.
constexpr uint8_t DEFAULT_LAYOUT_BITS = 0x0f;  // serpentine, column major, flipX, flipY
