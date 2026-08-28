#pragma once

#include <Arduino.h>

// Hardware wiring
// constexpr uint8_t DATA_PIN = 19; // // EatSpot V3
constexpr uint8_t DATA_PIN = 29; // EatSpot V2
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
// Confirmed against the bare panel with the 'origin' test: LED 0 sits bottom right (next to the
// data input connector) and the chain finishes at bottom left, snaking up and down in columns of 8.
// On its own that is bits 0x0f, but the panel is mounted 180 degrees rotated in the enclosure. A
// 180 degree rotation is exactly what the two flip flags do, so it cancels them rather than adding
// them, which leaves plain serpentine column major.
constexpr uint8_t DEFAULT_LAYOUT_BITS = 0x03; // serpentine, column major, no flips

// Digit transition timings. The roll is slower than the fade because it has further to travel and
// reads as mechanical, the fade wants to be quick enough to feel like a glance rather than an event.
constexpr uint32_t DIGIT_ROLL_MS = 300;
constexpr uint32_t DIGIT_FADE_MS = 250;

// Digits that change together cascade right to left rather than flipping as a slab, which is most
// of the charm on a rollover like 19:59 -> 20:00.
constexpr uint32_t DIGIT_STAGGER_MS = 50;

// The date block occupies columns 0-5, two touching 3 wide digits, with column 6 as the separator
// before the time. Digits are told apart by colour rather than a gap column.
constexpr int16_t DATE_COLUMN_A = 0;
constexpr int16_t DATE_COLUMN_B = 3;

// The date renders at full brightness alongside the time. Dimming it was tried and made the small
// glyphs harder to read for no real gain, but the knob is kept for the 'date dim' command.
constexpr uint8_t DATE_BRIGHTNESS_SCALE = 255;
