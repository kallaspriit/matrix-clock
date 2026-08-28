#pragma once

#include <Adafruit_GFX.h>
#include <Arduino.h>
#include <FastLED.h>

// Describes how the physical strip snakes through the panel. Nearly every 8x32 panel is wired a
// little differently, so rather than guessing, all four degrees of freedom are runtime switchable.
// Run the "corners" or "text" test and use "layout next" until it looks right, then bake the
// resulting bits into DEFAULT_LAYOUT_BITS below.
struct MatrixLayout {
    bool serpentine = true;    // every other row/column is wired backwards (zig-zag)
    bool columnMajor = false;  // strip runs down columns instead of across rows
    bool flipX = false;        // strip starts on the right edge instead of the left
    bool flipY = false;        // strip starts on the bottom edge instead of the top

    // Translates a logical pixel coordinate into an index along the physical strip
    uint16_t index(int16_t x, int16_t y, int16_t width, int16_t height) const {
        if (flipX) {
            x = width - 1 - x;
        }

        if (flipY) {
            y = height - 1 - y;
        }

        if (columnMajor) {
            if (serpentine && (x & 1)) {
                y = height - 1 - y;
            }

            return (uint16_t)(x * height + y);
        }

        if (serpentine && (y & 1)) {
            x = width - 1 - x;
        }

        return (uint16_t)(y * width + x);
    }

    uint8_t toBits() const {
        return (serpentine ? 0x01 : 0) | (columnMajor ? 0x02 : 0) | (flipX ? 0x04 : 0) | (flipY ? 0x08 : 0);
    }

    void fromBits(uint8_t bits) {
        serpentine = bits & 0x01;
        columnMajor = bits & 0x02;
        flipX = bits & 0x04;
        flipY = bits & 0x08;
    }

    // Steps to the next of the 16 possible wiring permutations
    void next() {
        fromBits((toBits() + 1) & 0x0f);
    }

    void describe(char* buffer, size_t size) const {
        snprintf(buffer,
                 size,
                 "bits=%u serpentine=%s columnMajor=%s flipX=%s flipY=%s",
                 toBits(),
                 serpentine ? "yes" : "no",
                 columnMajor ? "yes" : "no",
                 flipX ? "yes" : "no",
                 flipY ? "yes" : "no");
    }
};

// Adafruit_GFX only requires a subclass to implement drawPixel(), everything else (lines, rects,
// circles, bitmaps, fonts, print()) is built on top of it. So this is all it takes to turn the
// LED panel into something the whole GFX ecosystem can draw on.
class LedMatrix : public Adafruit_GFX {
   public:
    MatrixLayout layout;

    LedMatrix(CRGB* leds, int16_t width, int16_t height)
        : Adafruit_GFX(width, height)
        , leds(leds) {
    }

    void drawPixel(int16_t x, int16_t y, uint16_t color) override {
        if (x < 0 || x >= WIDTH || y < 0 || y >= HEIGHT) {
            return;
        }

        leds[layout.index(x, y, WIDTH, HEIGHT)] = toCRGB(color);
    }

    // Writes straight to a strip index, bypassing the layout mapping. The wiring tests use this to
    // show where the strip physically starts and which way it travels.
    void drawRawPixel(uint16_t index, const CRGB& color) {
        if (index >= (uint16_t)(WIDTH * HEIGHT)) {
            return;
        }

        leds[index] = color;
    }

    void clear() {
        fill_solid(leds, (uint16_t)(WIDTH * HEIGHT), CRGB::Black);
    }

    // Adafruit_GFX passes colors around as RGB565, these convert to and from it. The lost bits are
    // irrelevant here, the LEDs quantize far harder than that once global brightness is applied.
    static constexpr uint16_t rgb(uint8_t r, uint8_t g, uint8_t b) {
        return (uint16_t)(((r & 0xf8) << 8) | ((g & 0xfc) << 3) | (b >> 3));
    }

    static CRGB toCRGB(uint16_t color) {
        return CRGB((color >> 8) & 0xf8, (color >> 3) & 0xfc, (color << 3) & 0xf8);
    }

    static uint16_t hue(uint8_t h, uint8_t saturation = 255, uint8_t value = 255) {
        CRGB color = CHSV(h, saturation, value);

        return rgb(color.r, color.g, color.b);
    }

   private:
    CRGB* leds;
};

namespace Color {
    constexpr uint16_t BLACK = LedMatrix::rgb(0, 0, 0);
    constexpr uint16_t RED = LedMatrix::rgb(255, 0, 0);
    constexpr uint16_t GREEN = LedMatrix::rgb(0, 255, 0);
    constexpr uint16_t BLUE = LedMatrix::rgb(0, 0, 255);
    constexpr uint16_t WHITE = LedMatrix::rgb(255, 255, 255);
    constexpr uint16_t YELLOW = LedMatrix::rgb(255, 200, 0);
    constexpr uint16_t CYAN = LedMatrix::rgb(0, 200, 255);
    constexpr uint16_t MAGENTA = LedMatrix::rgb(255, 0, 200);
    constexpr uint16_t DIM_BLUE = LedMatrix::rgb(0, 40, 90);
}  // namespace Color
