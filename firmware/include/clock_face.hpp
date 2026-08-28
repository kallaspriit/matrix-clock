#pragma once

#include <Arduino.h>
#include <Fonts/TomThumb.h>

#include "config.hpp"
#include "date_display.hpp"
#include "digit_animator.hpp"
#include "digit_font.hpp"
#include "led_matrix.hpp"
#include "seconds_indicator.hpp"

// The RP2040 has no battery backed clock, so time comes from the host over serial and is carried
// forward with millis() until the next sync. The host sends local epoch seconds, which keeps all
// timezone and DST handling on the PC side where it belongs.
class TimeKeeper {
   public:
    void sync(uint32_t epochSeconds) {
        this->epochAtSync = epochSeconds;
        this->millisAtSync = millis();
        this->synced = true;
    }

    uint32_t now() const {
        if (!synced) {
            return 0;
        }

        // Unsigned subtraction stays correct across the ~49 day millis() rollover
        return epochAtSync + (millis() - millisAtSync) / 1000;
    }

    bool isSynced() const {
        return synced;
    }

    uint32_t secondsSinceSync() const {
        return synced ? (millis() - millisAtSync) / 1000 : 0;
    }

    uint8_t dayOfMonth() const {
        uint8_t day = 1;
        uint8_t month = 1;

        civilFromEpoch(now(), day, month);

        return day;
    }

    uint8_t monthOfYear() const {
        uint8_t day = 1;
        uint8_t month = 1;

        civilFromEpoch(now(), day, month);

        return month;
    }

   private:
    uint32_t epochAtSync = 0;
    uint32_t millisAtSync = 0;
    bool synced = false;

    // Howard Hinnant's civil_from_days. Turns a day count since the epoch into a calendar date
    // without any lookup tables or leap year special casing, by shifting the year to start in March
    // so the leap day lands at the end of the year rather than in the middle of it.
    static void civilFromEpoch(uint32_t epochSeconds, uint8_t& day, uint8_t& month) {
        int32_t z = (int32_t)(epochSeconds / 86400UL) + 719468;

        int32_t era = (z >= 0 ? z : z - 146096) / 146097;
        uint32_t doe = (uint32_t)(z - era * 146097);
        uint32_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
        uint32_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
        uint32_t mp = (5 * doy + 2) / 153;

        day = (uint8_t)(doy - (153 * mp + 2) / 5 + 1);
        month = (uint8_t)(mp < 10 ? mp + 3 : mp - 9);
    }
};

namespace ClockFace {

    // Digits come from the custom 5x8 set so they fill all eight rows. The blinking colon carries
    // the seconds on its own, which reads better than giving up a row to a progress bar.
    inline void render(LedMatrix& matrix, const TimeKeeper& timeKeeper, DigitAnimator& animator, DateDisplay& date, SecondsIndicator& seconds, const CRGB& color) {
        matrix.fillScreen(Color::BLACK);
        matrix.setFont(nullptr);
        matrix.setTextSize(1);
        matrix.setTextWrap(false);

        if (!timeKeeper.isSynced()) {
            // Breathe a placeholder until the host sends a "time" command
            uint8_t pulse = (uint8_t)(60 + 40 * sin8(millis() / 8) / 255);

            matrix.setFont(&TomThumb);
            matrix.setTextColor(LedMatrix::rgb(0, pulse, pulse));
            matrix.setCursor(3, 6);
            matrix.print("NO TIME");

            return;
        }

        uint32_t secondsOfDay = timeKeeper.now() % 86400UL;
        uint8_t hours = (uint8_t)(secondsOfDay / 3600);
        uint8_t minutes = (uint8_t)((secondsOfDay % 3600) / 60);

        uint8_t digits[4] = {
            (uint8_t)(hours / 10),
            (uint8_t)(hours % 10),
            (uint8_t)(minutes / 10),
            (uint8_t)(minutes % 10),
        };

        animator.update(digits);
        animator.render(matrix, color);

        date.render(matrix, timeKeeper.dayOfMonth(), timeKeeper.monthOfYear());

        seconds.render(matrix, color);
    }

}  // namespace ClockFace

// 8x6 envelope, one byte per row, MSB is the leftmost pixel
static const uint8_t NOTIFICATION_ENVELOPE[6] = {0xff, 0xc3, 0xa5, 0x99, 0x81, 0xff};

// An overlay that takes over the panel when the host pushes a notification. Shows an envelope and
// the unread count first, then scrolls the message, then hands the panel back to the clock.
class Notification {
   public:
    void show(uint16_t count, const char* text) {
        this->count = count;

        strncpy(this->text, text != nullptr ? text : "", sizeof(this->text) - 1);
        this->text[sizeof(this->text) - 1] = '\0';

        this->frame = 0;
        this->textWidth = 0;
        this->active = true;
    }

    void dismiss() {
        active = false;
    }

    bool isActive() const {
        return active;
    }

    void render(LedMatrix& matrix, uint16_t color) {
        matrix.fillScreen(Color::BLACK);
        matrix.setFont(&TomThumb);
        matrix.setTextSize(1);
        matrix.setTextWrap(false);
        matrix.setTextColor(color);

        if (frame < BADGE_FRAMES) {
            renderBadge(matrix, color);
        } else {
            if (!renderScroll(matrix)) {
                active = false;
            }
        }

        frame++;
    }

   private:
    static constexpr uint32_t BADGE_FRAMES = TARGET_FPS * 2;
    static constexpr uint32_t SCROLL_FRAMES_PER_PIXEL = 3;

    char text[96] = {0};
    uint16_t count = 0;
    uint32_t frame = 0;
    int16_t textWidth = 0;
    bool active = false;

    void renderBadge(LedMatrix& matrix, uint16_t color) {
        // Gentle pulse so a notification is noticeable out of the corner of your eye
        bool bright = (frame / (TARGET_FPS / 3)) % 2 == 0;

        matrix.drawBitmap(9, 1, NOTIFICATION_ENVELOPE, 8, 6, bright ? color : LedMatrix::rgb(0, 60, 90));

        if (count > 0) {
            char buffer[8];
            snprintf(buffer, sizeof(buffer), "%u", count > 99 ? 99 : count);

            matrix.setCursor(21, 6);
            matrix.print(buffer);
        }
    }

    // Returns false once the message has scrolled completely off the left edge
    bool renderScroll(LedMatrix& matrix) {
        if (text[0] == '\0') {
            return false;
        }

        if (textWidth == 0) {
            int16_t boundsX = 0;
            int16_t boundsY = 0;
            uint16_t width = 0;
            uint16_t height = 0;

            matrix.getTextBounds(text, 0, 6, &boundsX, &boundsY, &width, &height);

            textWidth = (int16_t)width;
        }

        int16_t offset = (int16_t)((frame - BADGE_FRAMES) / SCROLL_FRAMES_PER_PIXEL);
        int16_t cursorX = (int16_t)(MATRIX_WIDTH - offset);

        if (cursorX < -textWidth) {
            return false;
        }

        matrix.setCursor(cursorX, 6);
        matrix.print(text);

        return true;
    }
};
