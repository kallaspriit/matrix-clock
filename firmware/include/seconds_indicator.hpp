#pragma once

#include <Arduino.h>
#include <FastLED.h>

#include "config.hpp"
#include "digit_font.hpp"
#include "led_matrix.hpp"

enum SecondsStyle {
    SECONDS_OFF,
    SECONDS_COLON,    // two dots, hard blink once a second
    SECONDS_BREATHE,  // the same two dots, fading in and out instead of switching
    SECONDS_SCAN,     // a Knight Rider sweep up and down the colon column
};

// Owns the single column between the hours and the minutes. Since the seconds bar was dropped this
// column is the only thing telling you the clock is running, so it is worth more than a blink.
class SecondsIndicator {
   public:
    void setStyle(SecondsStyle next) {
        style = next;
    }

    SecondsStyle getStyle() const {
        return style;
    }

    const char* styleName() const {
        switch (style) {
            case SECONDS_COLON:
                return "colon";
            case SECONDS_BREATHE:
                return "breathe";
            case SECONDS_SCAN:
                return "scan";
            default:
                return "off";
        }
    }

    void render(LedMatrix& matrix, const CRGB& color) {
        switch (style) {
            case SECONDS_COLON:
                renderColon(matrix, color);
                break;
            case SECONDS_BREATHE:
                renderBreathe(matrix, color);
                break;
            case SECONDS_SCAN:
                renderScan(matrix, color);
                break;
            default:
                break;
        }
    }

   private:
    SecondsStyle style = SECONDS_SCAN;

    void renderColon(LedMatrix& matrix, const CRGB& color) {
        if (millis() % 1000 >= 500) {
            return;
        }

        matrix.blendPixel(COLON_COLUMN, COLON_TOP_ROW, color);
        matrix.blendPixel(COLON_COLUMN, COLON_BOTTOM_ROW, color);
    }

    void renderBreathe(LedMatrix& matrix, const CRGB& color) {
        uint8_t phase = (uint8_t)((millis() % 1000) * 255 / 1000);

        // Floored rather than allowed to reach black, both so it never reads as a hard blink and
        // because the bottom of the range would be scaled away by global brightness anyway
        uint8_t level = (uint8_t)(SECONDS_BREATHE_FLOOR + scale8(cubicwave8(phase), 255 - SECONDS_BREATHE_FLOOR));

        CRGB dot = color;
        dot.nscale8(level);

        matrix.blendPixel(COLON_COLUMN, COLON_TOP_ROW, dot);
        matrix.blendPixel(COLON_COLUMN, COLON_BOTTOM_ROW, dot);
    }

    // A bright head with a fade trailing behind it, sweeping down the column and back up once a
    // second. The tail is what sells it, so pixels ahead of the head fall off several times faster
    // than the ones behind, which is what gives the sweep a direction rather than looking like a
    // blob sliding around.
    void renderScan(LedMatrix& matrix, const CRGB& color) {
        uint8_t phase = (uint8_t)((millis() % SECONDS_SCAN_PERIOD_MS) * 255 / SECONDS_SCAN_PERIOD_MS);
        bool movingDown = phase < 128;

        uint16_t headQ8 = (uint16_t)(((uint32_t)triwave8(phase) * (MATRIX_HEIGHT - 1) * 256) / 255);
        uint32_t tailQ8 = (uint32_t)SECONDS_SCAN_TAIL * 256;

        for (int16_t y = 0; y < MATRIX_HEIGHT; y++) {
            int32_t delta = (int32_t)(y * 256) - (int32_t)headQ8;
            int32_t behind = movingDown ? -delta : delta;

            uint32_t distance = behind >= 0 ? (uint32_t)behind : (uint32_t)(-behind) * SECONDS_SCAN_LEAD_RATIO;

            if (distance >= tailQ8) {
                continue;
            }

            CRGB pixel = color;
            pixel.nscale8((uint8_t)(255 - (distance * 255) / tailQ8));

            matrix.blendPixel(COLON_COLUMN, y, pixel);
        }
    }
};
