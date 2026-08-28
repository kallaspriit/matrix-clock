#pragma once

#include <Arduino.h>

#include "config.hpp"
#include "digit_font.hpp"
#include "led_matrix.hpp"

enum DigitTransition {
    TRANSITION_NONE,
    TRANSITION_ROLL,
    TRANSITION_FADE,
};

// Tracks the four clock digits and animates the ones that change. Each digit owns its own timeline,
// so a rollover where several change at once cascades instead of moving as a single block.
class DigitAnimator {
   public:
    void setTransition(DigitTransition next) {
        transition = next;
    }

    DigitTransition getTransition() const {
        return transition;
    }

    const char* transitionName() const {
        switch (transition) {
            case TRANSITION_ROLL:
                return "roll";
            case TRANSITION_FADE:
                return "fade";
            default:
                return "none";
        }
    }

    // Feed the current digits in every frame. Any that differ from what is on screen start a
    // transition, staggered so the rightmost digit leads.
    void update(const uint8_t values[4]) {
        uint32_t now = millis();

        if (!initialized) {
            // Never animate the very first frame, there is nothing meaningful to animate away from
            for (uint8_t i = 0; i < 4; i++) {
                current[i] = values[i];
                previous[i] = values[i];
                startMs[i] = now - DIGIT_ROLL_MS;
            }

            initialized = true;

            return;
        }

        for (uint8_t i = 0; i < 4; i++) {
            if (values[i] == current[i]) {
                continue;
            }

            previous[i] = current[i];
            current[i] = values[i];
            startMs[i] = now + (3 - i) * DIGIT_STAGGER_MS;
        }
    }

    void render(LedMatrix& matrix, const CRGB& topColor, const CRGB& bottomColor) {
        for (uint8_t i = 0; i < 4; i++) {
            renderDigit(matrix, i, topColor, bottomColor);
        }
    }

    void render(LedMatrix& matrix, const CRGB& color) {
        render(matrix, color, color);
    }

   private:
    uint8_t current[4] = {0, 0, 0, 0};
    uint8_t previous[4] = {0, 0, 0, 0};
    uint32_t startMs[4] = {0, 0, 0, 0};
    bool initialized = false;
    DigitTransition transition = TRANSITION_ROLL;

    uint32_t durationMs() const {
        return transition == TRANSITION_FADE ? DIGIT_FADE_MS : DIGIT_ROLL_MS;
    }

    void renderDigit(LedMatrix& matrix, uint8_t index, const CRGB& topColor, const CRGB& bottomColor) {
        int16_t x = DIGIT_COLUMNS[index];

        // Signed so the stagger delay, which puts the start time in the future, reads as negative
        int32_t elapsed = (int32_t)(millis() - startMs[index]);
        uint32_t duration = durationMs();

        if (transition == TRANSITION_NONE || elapsed >= (int32_t)duration) {
            drawGlyph(matrix, x, 0, current[index], topColor, bottomColor);

            return;
        }

        if (elapsed < 0) {
            // Waiting for this digit's turn in the cascade, still showing the old value
            drawGlyph(matrix, x, 0, previous[index], topColor, bottomColor);

            return;
        }

        uint8_t progress = (uint8_t)((elapsed * 255) / (int32_t)duration);

        if (transition == TRANSITION_FADE) {
            renderFade(matrix, index, x, progress, topColor, bottomColor);
        } else {
            renderRoll(matrix, index, x, progress, topColor, bottomColor);
        }
    }

    // Old value slides up and out of the top, new value follows it up from below. Nothing is
    // clipped by hand, drawGlyph simply skips rows that land off the panel.
    void renderRoll(LedMatrix& matrix, uint8_t index, int16_t x, uint8_t progress, const CRGB& topColor, const CRGB& bottomColor) {
        int16_t offset = (int16_t)((ease8InOutQuad(progress) * DIGIT_HEIGHT) / 255);

        drawGlyph(matrix, x, -offset, previous[index], topColor, bottomColor);
        drawGlyph(matrix, x, DIGIT_HEIGHT - offset, current[index], topColor, bottomColor);
    }

    // Both glyphs stay in place and trade brightness. drawGlyph blends additively, so pixels the
    // two digits share stay at full brightness right through the crossover.
    void renderFade(LedMatrix& matrix, uint8_t index, int16_t x, uint8_t progress, const CRGB& topColor, const CRGB& bottomColor) {
        CRGB outgoingTop = topColor;
        CRGB outgoingBottom = bottomColor;
        CRGB incomingTop = topColor;
        CRGB incomingBottom = bottomColor;

        outgoingTop.nscale8(255 - progress);
        outgoingBottom.nscale8(255 - progress);
        incomingTop.nscale8(progress);
        incomingBottom.nscale8(progress);

        drawGlyph(matrix, x, 0, previous[index], outgoingTop, outgoingBottom);
        drawGlyph(matrix, x, 0, current[index], incomingTop, incomingBottom);
    }
};
