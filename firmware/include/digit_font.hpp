#pragma once

#include <Arduino.h>

#include "glyph.hpp"
#include "led_matrix.hpp"

// A hand drawn 5x8 digit set that uses the full height of the panel. The built in GFX font is 5x7
// and has to leave the bottom row empty, which wastes an eighth of a display this small.
//
// One byte per row, left aligned so the leftmost pixel is bit 7. That is exactly the layout
// Adafruit_GFX::drawBitmap expects for a 5 pixel wide bitmap, so these render with no custom blit.
//
//   .###.  ->  0111 0000  ->  0x70
//   #...#  ->  1000 1000  ->  0x88
constexpr int16_t DIGIT_WIDTH = 5;
constexpr int16_t DIGIT_HEIGHT = 8;

static const uint8_t DIGIT_GLYPHS[10][DIGIT_HEIGHT] = {
    {0x70, 0x88, 0x88, 0x88, 0x88, 0x88, 0x88, 0x70},  // 0
    {0x20, 0x60, 0x20, 0x20, 0x20, 0x20, 0x20, 0x70},  // 1
    {0x70, 0x88, 0x08, 0x10, 0x20, 0x40, 0x80, 0xf8},  // 2
    {0x70, 0x88, 0x08, 0x30, 0x08, 0x08, 0x88, 0x70},  // 3
    {0x10, 0x30, 0x50, 0x90, 0xf8, 0x10, 0x10, 0x10},  // 4
    {0xf8, 0x80, 0x80, 0xf0, 0x08, 0x08, 0x88, 0x70},  // 5
    {0x30, 0x40, 0x80, 0xf0, 0x88, 0x88, 0x88, 0x70},  // 6
    {0xf8, 0x08, 0x08, 0x10, 0x20, 0x40, 0x40, 0x40},  // 7
    {0x70, 0x88, 0x88, 0x70, 0x88, 0x88, 0x88, 0x70},  // 8
    {0x70, 0x88, 0x88, 0x88, 0x78, 0x08, 0x10, 0x60},  // 9
};

// Four digits plus a colon comes to 25 columns, which is the floor: 5+1+5 for the hours, 1+1+1
// around the colon, 5+1+5 for the minutes. That frees columns 0-5 for the date block and column
// 6 as its separator, so the panel is used edge to edge with nothing spare.
constexpr int16_t DIGIT_COLUMNS[4] = {7, 13, 21, 27};
constexpr int16_t COLON_COLUMN = 19;
constexpr int16_t COLON_TOP_ROW = 2;
constexpr int16_t COLON_BOTTOM_ROW = 5;

// Replaces the plain drawBitmap call this used to make. Two differences matter:
//
//   1. Colour is computed per row, interpolating top to bottom across the glyph. Passing the same
//      colour twice gives a flat digit, passing two gives a vertical gradient.
//   2. It blends additively and lets rows fall outside the panel, which is what makes the roll
//      transition free: draw the outgoing glyph above the panel and the incoming one below it, and
//      the clipping handles itself.
inline void drawGlyph(LedMatrix& matrix, int16_t x, int16_t y, uint8_t digit, const CRGB& topColor, const CRGB& bottomColor) {
    if (digit > 9) {
        return;
    }

    drawGlyphRows(matrix, x, y, DIGIT_GLYPHS[digit], DIGIT_WIDTH, DIGIT_HEIGHT, topColor, bottomColor);

}

inline void drawGlyph(LedMatrix& matrix, int16_t x, int16_t y, uint8_t digit, const CRGB& color) {
    drawGlyph(matrix, x, y, digit, color, color);
}
