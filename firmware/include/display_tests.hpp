#pragma once

#include <Arduino.h>
#include <Fonts/TomThumb.h>

#include "led_matrix.hpp"

// Each test renders one frame. Tests that need animation derive it from the frame counter, which
// ticks at TARGET_FPS. Anything a test changes globally (brightness, font) is reset by the caller
// before every frame, so tests cannot leak state into each other.
typedef void (*TestRenderFn)(LedMatrix& matrix, uint32_t frame);

struct DisplayTest {
    const char* name;
    const char* expectation;
    TestRenderFn render;
};

namespace DisplayTests {

    // Shows where the strip physically begins and which way it heads. Ignores the layout mapping
    // entirely, so it is true no matter how wrong the current layout guess is.
    inline void origin(LedMatrix& matrix, uint32_t frame) {
        matrix.clear();

        // Bright head plus a fading tail over the first few LEDs of the chain
        matrix.drawRawPixel(0, CRGB::Red);
        matrix.drawRawPixel(1, CRGB(80, 0, 0));
        matrix.drawRawPixel(2, CRGB(30, 0, 0));
        matrix.drawRawPixel(3, CRGB(12, 0, 0));

        // The very last LED in the chain
        matrix.drawRawPixel(NUM_LEDS - 1, CRGB::Blue);

        (void)frame;
    }

    // Crawls a single pixel along the physical strip so the snake pattern is visible by eye
    inline void walk(LedMatrix& matrix, uint32_t frame) {
        matrix.clear();

        uint16_t head = (uint16_t)((frame / 4) % NUM_LEDS);

        matrix.drawRawPixel(head, CRGB::White);

        // Short tail to make the direction of travel unambiguous
        for (uint8_t i = 1; i <= 3; i++) {
            if (head >= i) {
                matrix.drawRawPixel(head - i, CRGB(60 >> i, 60 >> i, 0));
            }
        }
    }

    // The decisive orientation test. Read the four corners against the expectation string.
    inline void corners(LedMatrix& matrix, uint32_t frame) {
        matrix.fillScreen(Color::BLACK);

        matrix.drawPixel(0, 0, Color::RED);
        matrix.drawPixel(MATRIX_WIDTH - 1, 0, Color::GREEN);
        matrix.drawPixel(0, MATRIX_HEIGHT - 1, Color::BLUE);
        matrix.drawPixel(MATRIX_WIDTH - 1, MATRIX_HEIGHT - 1, Color::WHITE);

        // A short tick running right from the origin, so a transposed panel is obvious even if you
        // cannot tell the corners apart at a glance
        matrix.drawFastHLine(1, 0, 4, LedMatrix::rgb(60, 0, 0));

        (void)frame;
    }

    // Eight solid horizontal stripes if rows are mapped correctly
    inline void rows(LedMatrix& matrix, uint32_t frame) {
        matrix.fillScreen(Color::BLACK);

        for (int16_t y = 0; y < MATRIX_HEIGHT; y++) {
            matrix.drawFastHLine(0, y, MATRIX_WIDTH, LedMatrix::hue((uint8_t)(y * 32)));
        }

        (void)frame;
    }

    // A smooth left to right hue sweep, every row identical
    inline void columns(LedMatrix& matrix, uint32_t frame) {
        matrix.fillScreen(Color::BLACK);

        for (int16_t x = 0; x < MATRIX_WIDTH; x++) {
            matrix.drawFastVLine(x, 0, MATRIX_HEIGHT, LedMatrix::hue((uint8_t)((x * 255) / MATRIX_WIDTH)));
        }

        (void)frame;
    }

    // Doubles as a preview of the clock face, if this reads correctly the layout is right
    inline void text(LedMatrix& matrix, uint32_t frame) {
        matrix.fillScreen(Color::BLACK);

        matrix.setTextColor(Color::CYAN);
        matrix.setCursor(2, 0);
        matrix.print("12");

        matrix.drawPixel(15, 2, Color::CYAN);
        matrix.drawPixel(15, 4, Color::CYAN);

        matrix.setCursor(18, 0);
        matrix.print("34");

        // Origin marker, should sit in the top left next to the "1"
        matrix.drawPixel(0, 0, LedMatrix::rgb(60, 0, 0));

        (void)frame;
    }

    // Verifies the WS2812B color order. If "red" comes out green the GRB in addLeds is wrong.
    inline void colors(LedMatrix& matrix, uint32_t frame) {
        static const uint16_t sequence[] = {Color::RED, Color::GREEN, Color::BLUE, Color::WHITE, Color::BLACK};

        uint32_t step = (frame / TARGET_FPS) % (sizeof(sequence) / sizeof(sequence[0]));

        matrix.fillScreen(sequence[step]);
    }

    // Exercises both axes at once, hue horizontally and brightness vertically
    inline void gradient(LedMatrix& matrix, uint32_t frame) {
        uint8_t drift = (uint8_t)(frame / 2);

        for (int16_t y = 0; y < MATRIX_HEIGHT; y++) {
            for (int16_t x = 0; x < MATRIX_WIDTH; x++) {
                uint8_t h = (uint8_t)(((x * 255) / MATRIX_WIDTH) + drift);
                uint8_t value = (uint8_t)(40 + (y * 215) / (MATRIX_HEIGHT - 1));

                matrix.drawPixel(x, y, LedMatrix::hue(h, 255, value));
            }
        }
    }

    // Confirms the GFX primitives and edge clipping behave on a panel this small
    inline void shapes(LedMatrix& matrix, uint32_t frame) {
        matrix.fillScreen(Color::BLACK);

        matrix.drawRect(0, 0, MATRIX_WIDTH, MATRIX_HEIGHT, Color::DIM_BLUE);
        matrix.drawLine(0, 0, MATRIX_WIDTH - 1, MATRIX_HEIGHT - 1, LedMatrix::rgb(40, 20, 0));
        matrix.fillCircle(8, 4, 3, Color::MAGENTA);
        matrix.drawCircle(16, 4, 3, Color::GREEN);
        matrix.fillRect(23, 2, 6, 4, Color::YELLOW);

        (void)frame;
    }

    // Ramps an all white panel up and down so you can watch actual current draw on a meter, and see
    // FastLED's power limiter clamp it. This is the only test that deliberately pushes the supply.
    inline void power(LedMatrix& matrix, uint32_t frame) {
        uint32_t phase = frame % (TARGET_FPS * 20);
        uint32_t half = TARGET_FPS * 10;

        uint8_t level = phase < half ? (uint8_t)((phase * 255) / half) : (uint8_t)(((TARGET_FPS * 20 - phase) * 255) / half);

        FastLED.setBrightness(level);

        matrix.fillScreen(Color::WHITE);
    }

}  // namespace DisplayTests

static const DisplayTest DISPLAY_TESTS[] = {
    {"origin", "Red pixel with a fading tail at the corner where the strip starts, blue pixel at the last LED. Layout independent.", DisplayTests::origin},
    {"walk", "One white pixel crawling along the physical strip. Watch which way it snakes at the end of each run.", DisplayTests::walk},
    {"corners", "Red=top-left Green=top-right Blue=bottom-left White=bottom-right, plus a dim red tick running right from the red one.", DisplayTests::corners},
    {"rows", "Eight solid horizontal stripes, each a different color. Vertical stripes mean columnMajor is wrong.", DisplayTests::rows},
    {"cols", "Smooth red-to-magenta hue sweep left to right, all eight rows identical.", DisplayTests::columns},
    {"text", "Readable '12:34' with a dim red dot at the top left. Mirrored or upside down means flipX/flipY need changing.", DisplayTests::text},
    {"colors", "Whole panel cycling red, green, blue, white, off at one second each. Wrong colors mean the GRB order is wrong.", DisplayTests::colors},
    {"gradient", "Drifting hue left to right, dim at the top fading to bright at the bottom.", DisplayTests::gradient},
    {"shapes", "Blue border, filled magenta circle, green circle outline, yellow rectangle, dim diagonal line.", DisplayTests::shapes},
    {"power", "All white ramping up and down over 20s. Watch your ammeter, FastLED should clamp it near the configured limit.", DisplayTests::power},
};

constexpr size_t DISPLAY_TEST_COUNT = sizeof(DISPLAY_TESTS) / sizeof(DISPLAY_TESTS[0]);
