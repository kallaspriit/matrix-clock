#pragma once

#include <Arduino.h>

#include "led_matrix.hpp"

// Shared bitmap glyph renderer for every font in the project. Glyphs are one byte per row, left
// aligned so the leftmost pixel is bit 7, which works for any width up to 8.
//
// Colour is interpolated per row from top to bottom, so passing the same colour twice gives a flat
// glyph and passing two gives a vertical gradient. Blending is additive and rows falling outside
// the panel are simply skipped, which is what lets the digit roll transition draw a glyph partly
// above the panel and its replacement partly below without any manual clipping.
inline void drawGlyphRows(LedMatrix& matrix,
                          int16_t x,
                          int16_t y,
                          const uint8_t* rows,
                          int16_t width,
                          int16_t height,
                          const CRGB& topColor,
                          const CRGB& bottomColor) {
    bool flat = topColor == bottomColor;

    // The gradient is anchored to the panel, not to the glyph. That matters during the roll
    // transition: with a glyph relative gradient the outgoing digit carries its bottom colour up to
    // the top of the panel while the incoming digit brings its top colour in at the bottom, so the
    // seam between them inverts. Keying off the screen row instead keeps the gradient still while
    // the digits move through it.
    int16_t panelHeight = matrix.height();
    int16_t lastRow = panelHeight > 1 ? panelHeight - 1 : 1;

    for (int16_t row = 0; row < height; row++) {
        uint8_t bits = rows[row];

        if (bits == 0) {
            continue;
        }

        int16_t panelRow = constrain((int16_t)(y + row), (int16_t)0, (int16_t)(panelHeight - 1));

        CRGB rowColor = flat ? topColor : blend(topColor, bottomColor, (uint8_t)((panelRow * 255) / lastRow));

        for (int16_t column = 0; column < width; column++) {
            if (bits & (0x80 >> column)) {
                matrix.blendPixel(x + column, y + row, rowColor);
            }
        }
    }
}
