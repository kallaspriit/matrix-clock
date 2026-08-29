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

// 3x7, used when only the day is shown. One row shorter than the panel on purpose: the units digit
// is drawn a row lower than the tens digit, so their horizontal bars never line up and the two
// touching digits separate structurally instead of relying on colour alone.
constexpr int16_t DATE_LARGE_HEIGHT = 7;
constexpr int16_t DATE_LARGE_UNITS_DROP = 1;

static const uint8_t DATE_LARGE_GLYPHS[10][DATE_LARGE_HEIGHT] = {
    {0xe0, 0xa0, 0xa0, 0xa0, 0xa0, 0xa0, 0xe0},  // 0
    {0x40, 0xc0, 0x40, 0x40, 0x40, 0x40, 0xe0},  // 1
    {0xe0, 0x20, 0x20, 0xe0, 0x80, 0x80, 0xe0},  // 2
    {0xe0, 0x20, 0x20, 0xe0, 0x20, 0x20, 0xe0},  // 3
    {0xa0, 0xa0, 0xa0, 0xe0, 0x20, 0x20, 0x20},  // 4
    {0xe0, 0x80, 0x80, 0xe0, 0x20, 0x20, 0xe0},  // 5
    {0xe0, 0x80, 0x80, 0xe0, 0xa0, 0xa0, 0xe0},  // 6
    {0xe0, 0x20, 0x20, 0x40, 0x40, 0x40, 0x40},  // 7
    {0xe0, 0xa0, 0xa0, 0xe0, 0xa0, 0xa0, 0xe0},  // 8
    {0xe0, 0xa0, 0xa0, 0xe0, 0x20, 0x20, 0xe0},  // 9
};

enum DateMode {
    DATE_OFF,
    DATE_DAY_MONTH,  // day over month, 3x4 each. Striking, but reads as decoration more than as a date
    DATE_DAY,        // day only, 3x7 staggered. The default, and the one that is actually readable
};

// The two ends of the shared palette, which is what keeps touching digits separable with no gap
// column between them. The month row swaps them, so in the stacked mode all four positions are
// still told apart by the combination of row and colour without introducing new hues.
static const CRGB DATE_DAY_TENS = PALETTE_TOP;
static const CRGB DATE_DAY_UNITS = PALETTE_BOTTOM;
static const CRGB DATE_MONTH_TENS = PALETTE_BOTTOM;
static const CRGB DATE_MONTH_UNITS = PALETTE_TOP;

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
            drawPair(matrix, 0, day, DATE_LARGE_GLYPHS[0], DATE_LARGE_HEIGHT, DATE_LARGE_UNITS_DROP, dayTens, dayUnits);

            return;
        }

        CRGB monthTens = DATE_MONTH_TENS;
        CRGB monthUnits = DATE_MONTH_UNITS;

        if (dimming < 255) {
            monthTens.nscale8(dimming);
            monthUnits.nscale8(dimming);
        }

        // Stacked there is no spare row to stagger into, so both rows sit flush
        drawPair(matrix, 0, day, DATE_SMALL_GLYPHS[0], DATE_SMALL_HEIGHT, 0, dayTens, dayUnits);
        drawPair(matrix, DATE_SMALL_HEIGHT, month, DATE_SMALL_GLYPHS[0], DATE_SMALL_HEIGHT, 0, monthTens, monthUnits);
    }

   private:
    DateMode mode = DATE_DAY;
    uint8_t dimming = DATE_BRIGHTNESS_SCALE;

    // Draws a two digit value into the 6 column block, tens then units, touching. unitsDrop shifts
    // the units digit down by that many rows to break up the shared horizontal bars.
    void drawPair(LedMatrix& matrix, int16_t y, uint8_t value, const uint8_t* table, int16_t height, int16_t unitsDrop, const CRGB& tensColor, const CRGB& unitsColor) {
        uint8_t tensDigit = (uint8_t)((value / 10) % 10);
        uint8_t unitsDigit = (uint8_t)(value % 10);

        drawGlyphRows(matrix, DATE_COLUMN_A, y, table + tensDigit * height, DATE_DIGIT_WIDTH, height, tensColor, tensColor);
        drawGlyphRows(matrix, DATE_COLUMN_B, y + unitsDrop, table + unitsDigit * height, DATE_DIGIT_WIDTH, height, unitsColor, unitsColor);
    }
};
