#pragma once

#include <Arduino.h>

#include "config.hpp"
#include "glyph.hpp"
#include "led_matrix.hpp"

// Two date fonts, both 3 pixels wide, sharing the same 6 column block at the left of the panel.
// Adjacent digits touch with no gap between them and are told apart by colour instead, which the
// enclosure supports because each LED sits in its own walled cell with no bleed to its neighbours.
//
// One byte per row, left aligned, so only the top three bits of each byte are used.
constexpr int16_t DATE_DIGIT_WIDTH = 3;

// 3x4, used when day and month are stacked two rows deep
constexpr int16_t DATE_SMALL_HEIGHT = 4;

static const uint8_t DATE_SMALL_GLYPHS[10][DATE_SMALL_HEIGHT] = {
    {0xe0, 0xa0, 0xa0, 0xe0},  // 0
    {0x40, 0xc0, 0x40, 0xe0},  // 1
    {0xe0, 0x20, 0x80, 0xe0},  // 2
    {0xe0, 0x60, 0x20, 0xe0},  // 3
    {0xa0, 0xa0, 0xe0, 0x20},  // 4
    {0xe0, 0x80, 0x20, 0xe0},  // 5
    {0x80, 0xe0, 0xa0, 0xe0},  // 6
    {0xe0, 0x20, 0x40, 0x40},  // 7
    {0xe0, 0xe0, 0xa0, 0xe0},  // 8
    {0xe0, 0xa0, 0xe0, 0x20},  // 9
};

// 3x8, used when only the day is shown and it gets the full height of the panel
constexpr int16_t DATE_LARGE_HEIGHT = 8;

static const uint8_t DATE_LARGE_GLYPHS[10][DATE_LARGE_HEIGHT] = {
    {0xe0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xe0},  // 0
    {0x40, 0xc0, 0x40, 0x40, 0x40, 0x40, 0x40, 0xe0},  // 1
    {0xe0, 0x20, 0x20, 0xe0, 0x80, 0x80, 0x80, 0xe0},  // 2
    {0xe0, 0x20, 0x20, 0xe0, 0x20, 0x20, 0x20, 0xe0},  // 3
    {0xa0, 0xa0, 0xa0, 0xa0, 0xe0, 0x20, 0x20, 0x20},  // 4
    {0xe0, 0x80, 0x80, 0xe0, 0x20, 0x20, 0x20, 0xe0},  // 5
    {0xe0, 0x80, 0x80, 0xe0, 0xa0, 0xa0, 0xa0, 0xe0},  // 6
    {0xe0, 0x20, 0x20, 0x40, 0x40, 0x40, 0x40, 0x40},  // 7
    {0xe0, 0xa0, 0xa0, 0xe0, 0xa0, 0xa0, 0xa0, 0xe0},  // 8
    {0xe0, 0xa0, 0xa0, 0xe0, 0x20, 0x20, 0x20, 0xe0},  // 9
};

enum DateMode {
    DATE_OFF,
    DATE_DAY_MONTH,  // day over month, 3x4 each. Striking, but reads as decoration more than as a date
    DATE_DAY,        // day only, 3x8. The default, twice the glyph structure and actually readable
};

// Four distinct hues, one per digit. Sharing a colour between the day and month rows made the two
// rows echo each other, so every digit gets its own. Within a row the pair is roughly 100 degrees
// apart in hue, which is what keeps touching digits separable with no gap column between them.
// All four stay clear of the clock's cyan.
static const CRGB DATE_DAY_TENS = CRGB(255, 120, 0);     // orange
static const CRGB DATE_DAY_UNITS = CRGB(40, 210, 60);    // green
static const CRGB DATE_MONTH_TENS = CRGB(255, 40, 150);  // magenta
static const CRGB DATE_MONTH_UNITS = CRGB(230, 190, 0);  // yellow

class DateDisplay {
   public:
    void setMode(DateMode next) {
        mode = next;
    }

    DateMode getMode() const {
        return mode;
    }

    const char* modeName() const {
        switch (mode) {
            case DATE_DAY_MONTH:
                return "dm";
            case DATE_DAY:
                return "d";
            default:
                return "off";
        }
    }

    void setDimming(uint8_t scale) {
        dimming = scale;
    }

    uint8_t getDimming() const {
        return dimming;
    }

    void render(LedMatrix& matrix, uint8_t day, uint8_t month) {
        if (mode == DATE_OFF) {
            return;
        }

        CRGB dayTens = DATE_DAY_TENS;
        CRGB dayUnits = DATE_DAY_UNITS;

        if (dimming < 255) {
            dayTens.nscale8(dimming);
            dayUnits.nscale8(dimming);
        }

        if (mode == DATE_DAY) {
            drawPair(matrix, 0, day, DATE_LARGE_GLYPHS[0], DATE_LARGE_HEIGHT, dayTens, dayUnits);

            return;
        }

        CRGB monthTens = DATE_MONTH_TENS;
        CRGB monthUnits = DATE_MONTH_UNITS;

        if (dimming < 255) {
            monthTens.nscale8(dimming);
            monthUnits.nscale8(dimming);
        }

        drawPair(matrix, 0, day, DATE_SMALL_GLYPHS[0], DATE_SMALL_HEIGHT, dayTens, dayUnits);
        drawPair(matrix, DATE_SMALL_HEIGHT, month, DATE_SMALL_GLYPHS[0], DATE_SMALL_HEIGHT, monthTens, monthUnits);
    }

   private:
    DateMode mode = DATE_DAY;
    uint8_t dimming = DATE_BRIGHTNESS_SCALE;

    // Draws a two digit value into the 6 column block, tens then units, touching
    void drawPair(LedMatrix& matrix, int16_t y, uint8_t value, const uint8_t* table, int16_t height, const CRGB& tensColor, const CRGB& unitsColor) {
        uint8_t tensDigit = (uint8_t)((value / 10) % 10);
        uint8_t unitsDigit = (uint8_t)(value % 10);

        drawGlyphRows(matrix, DATE_COLUMN_A, y, table + tensDigit * height, DATE_DIGIT_WIDTH, height, tensColor, tensColor);
        drawGlyphRows(matrix, DATE_COLUMN_B, y, table + unitsDigit * height, DATE_DIGIT_WIDTH, height, unitsColor, unitsColor);
    }
};
