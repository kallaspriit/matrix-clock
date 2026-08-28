#include <Adafruit_GFX.h>
#include <Arduino.h>
#include <FastLED.h>

#include "clock_face.hpp"
#include "config.hpp"
#include "display_tests.hpp"
#include "led_matrix.hpp"
#include "stream_operators.hpp"

enum RenderMode {
    MODE_CLOCK,
    MODE_TEST,
};

CRGB leds[NUM_LEDS];
LedMatrix matrix(leds, MATRIX_WIDTH, MATRIX_HEIGHT);
TimeKeeper timeKeeper;
Notification notification;

RenderMode mode = MODE_TEST;
size_t testIndex = 0;
uint32_t frameCounter = 0;
uint8_t brightness = DEFAULT_BRIGHTNESS;
uint16_t clockColor = Color::CYAN;

char serialLine[SERIAL_LINE_LIMIT];
size_t serialLineLength = 0;

// Strips leading and trailing whitespace in place
char* trim(char* text) {
    while (*text == ' ' || *text == '\t' || *text == '\r') {
        text++;
    }

    char* end = text + strlen(text);

    while (end > text && (end[-1] == ' ' || end[-1] == '\t' || end[-1] == '\r')) {
        *--end = '\0';
    }

    return text;
}

// Splits off the first whitespace delimited word, leaving the rest in remainder
char* splitWord(char* text, char** remainder) {
    char* space = strpbrk(text, " \t");

    if (space == nullptr) {
        *remainder = text + strlen(text);

        return text;
    }

    *space = '\0';
    *remainder = trim(space + 1);

    return text;
}

void toLower(char* text) {
    for (char* c = text; *c != '\0'; c++) {
        *c = (char)tolower((unsigned char)*c);
    }
}

int findTest(const char* name) {
    for (size_t i = 0; i < DISPLAY_TEST_COUNT; i++) {
        if (strcmp(DISPLAY_TESTS[i].name, name) == 0) {
            return (int)i;
        }
    }

    return -1;
}

void reportTest() {
    Serial << "test '" << DISPLAY_TESTS[testIndex].name << "' (" << (testIndex + 1) << "/" << DISPLAY_TEST_COUNT << ")" << endl;
    Serial << "  expect: " << DISPLAY_TESTS[testIndex].expectation << endl;
}

void reportLayout() {
    char description[128];

    matrix.layout.describe(description, sizeof(description));

    Serial << "layout " << description << endl;
}

void printHelp() {
    Serial << endl;
    Serial << "led-clock commands" << endl;
    Serial << "  help                   this list" << endl;
    Serial << "  ping                   connectivity check, replies pong" << endl;
    Serial << "  status                 current mode, layout, brightness, sync state" << endl;
    Serial << "  mode <clock|test>      switch what gets rendered" << endl;
    Serial << "  test list              show every test and what it should look like" << endl;
    Serial << "  test <name|next>       run a specific test, or step to the following one" << endl;
    Serial << "  layout                 print the current wiring guess" << endl;
    Serial << "  layout next            step through all 16 wiring permutations" << endl;
    Serial << "  layout serp            toggle serpentine (zig-zag) wiring" << endl;
    Serial << "  layout colmajor        toggle columns instead of rows" << endl;
    Serial << "  layout flipx|flipy     toggle the starting edge" << endl;
    Serial << "  layout bits <0-15>     set the permutation directly" << endl;
    Serial << "  time <epochSeconds>    sync the clock, host sends LOCAL epoch" << endl;
    Serial << "  bright <0-255>         global brightness" << endl;
    Serial << "  color <r> <g> <b>      clock face color" << endl;
    Serial << "  notify <count> <text>  envelope, unread count, then scrolls the text" << endl;
    Serial << "  clear                  dismiss the current notification" << endl;
    Serial << endl;
}

void handleLayoutCommand(char* arguments) {
    if (arguments[0] == '\0') {
        reportLayout();

        return;
    }

    char* rest = nullptr;
    char* word = splitWord(arguments, &rest);

    toLower(word);

    if (strcmp(word, "next") == 0) {
        matrix.layout.next();
    } else if (strcmp(word, "serp") == 0) {
        matrix.layout.serpentine = !matrix.layout.serpentine;
    } else if (strcmp(word, "colmajor") == 0) {
        matrix.layout.columnMajor = !matrix.layout.columnMajor;
    } else if (strcmp(word, "flipx") == 0) {
        matrix.layout.flipX = !matrix.layout.flipX;
    } else if (strcmp(word, "flipy") == 0) {
        matrix.layout.flipY = !matrix.layout.flipY;
    } else if (strcmp(word, "bits") == 0) {
        matrix.layout.fromBits((uint8_t)(atoi(rest) & 0x0f));
    } else {
        Serial << "error: unknown layout option " << word << endl;

        return;
    }

    reportLayout();
}

void handleTestCommand(char* arguments) {
    toLower(arguments);

    if (strcmp(arguments, "list") == 0) {
        for (size_t i = 0; i < DISPLAY_TEST_COUNT; i++) {
            Serial << "  " << DISPLAY_TESTS[i].name << " - " << DISPLAY_TESTS[i].expectation << endl;
        }

        return;
    }

    if (arguments[0] == '\0' || strcmp(arguments, "next") == 0) {
        testIndex = (testIndex + 1) % DISPLAY_TEST_COUNT;
    } else {
        int found = findTest(arguments);

        if (found < 0) {
            Serial << "error: no test called " << arguments << ", try 'test list'" << endl;

            return;
        }

        testIndex = (size_t)found;
    }

    mode = MODE_TEST;
    frameCounter = 0;

    reportTest();
}

void handleNotifyCommand(char* arguments) {
    char* rest = nullptr;
    char* countWord = splitWord(arguments, &rest);

    notification.show((uint16_t)atoi(countWord), rest);

    Serial << "ok notify" << endl;
}

void handleModeCommand(char* arguments) {
    toLower(arguments);

    if (strcmp(arguments, "clock") == 0) {
        mode = MODE_CLOCK;
    } else if (strcmp(arguments, "test") == 0) {
        mode = MODE_TEST;
    } else {
        Serial << "error: mode must be clock or test" << endl;

        return;
    }

    frameCounter = 0;

    Serial << "ok mode " << arguments << endl;
}

void handleColorCommand(char* arguments) {
    int r = 0;
    int g = 0;
    int b = 0;

    if (sscanf(arguments, "%d %d %d", &r, &g, &b) != 3) {
        Serial << "error: color needs three values 0-255" << endl;

        return;
    }

    clockColor = LedMatrix::rgb((uint8_t)constrain(r, 0, 255), (uint8_t)constrain(g, 0, 255), (uint8_t)constrain(b, 0, 255));

    Serial << "ok color" << endl;
}

void handleCommand(char* line) {
    char* trimmed = trim(line);

    if (trimmed[0] == '\0') {
        return;
    }

    char* arguments = nullptr;
    char* command = splitWord(trimmed, &arguments);

    toLower(command);

    if (strcmp(command, "help") == 0) {
        printHelp();
    } else if (strcmp(command, "ping") == 0) {
        Serial << "pong" << endl;
    } else if (strcmp(command, "status") == 0) {
        Serial << "mode=" << (mode == MODE_CLOCK ? "clock" : "test") << " test=" << DISPLAY_TESTS[testIndex].name << " brightness=" << brightness << " synced=" << (timeKeeper.isSynced() ? "yes" : "no")
               << " uptimeS=" << (millis() / 1000) << endl;

        reportLayout();
    } else if (strcmp(command, "mode") == 0) {
        handleModeCommand(arguments);
    } else if (strcmp(command, "test") == 0) {
        handleTestCommand(arguments);
    } else if (strcmp(command, "layout") == 0) {
        handleLayoutCommand(arguments);
    } else if (strcmp(command, "time") == 0) {
        timeKeeper.sync((uint32_t)strtoul(arguments, nullptr, 10));
        mode = MODE_CLOCK;

        Serial << "ok time" << endl;
    } else if (strcmp(command, "bright") == 0) {
        brightness = (uint8_t)constrain(atoi(arguments), 0, 255);

        Serial << "ok bright " << brightness << endl;
    } else if (strcmp(command, "color") == 0) {
        handleColorCommand(arguments);
    } else if (strcmp(command, "notify") == 0) {
        handleNotifyCommand(arguments);
    } else if (strcmp(command, "clear") == 0) {
        notification.dismiss();

        Serial << "ok clear" << endl;
    } else {
        Serial << "error: unknown command " << command << ", try 'help'" << endl;
    }
}

void readSerial() {
    while (Serial.available() > 0) {
        char received = (char)Serial.read();

        if (received == '\n') {
            serialLine[serialLineLength] = '\0';
            serialLineLength = 0;

            handleCommand(serialLine);
        } else if (serialLineLength < SERIAL_LINE_LIMIT - 1) {
            serialLine[serialLineLength++] = received;
        }
    }
}

void setup() {
    Serial.begin(SERIAL_BAUD);

    FastLED.addLeds<WS2812B, DATA_PIN, GRB>(leds, NUM_LEDS);
    FastLED.setCorrection(TypicalLEDStrip);
    FastLED.setBrightness(brightness);
    FastLED.setMaxPowerInVoltsAndMilliamps(SUPPLY_VOLTS, MAX_MILLIAMPS);
    FastLED.clear(true);

    matrix.layout.fromBits(DEFAULT_LAYOUT_BITS);

    // Boot into the orientation test, it is the first thing worth confirming on new hardware
    int corners = findTest("corners");
    testIndex = corners < 0 ? 0 : (size_t)corners;

    Serial << endl << "led-clock ready, " << NUM_LEDS << " LEDs on pin " << DATA_PIN << ", limited to " << MAX_MILLIAMPS << " mA" << endl;

    printHelp();
    reportLayout();
    reportTest();
}

void loop() {
    readSerial();

    static uint32_t lastFrameTimeMs = 0;
    uint32_t currentTimeMs = millis();

    if (currentTimeMs - lastFrameTimeMs < FRAME_INTERVAL_MS) {
        return;
    }

    lastFrameTimeMs = currentTimeMs;

    // Reset everything a renderer is allowed to change, so no renderer can leak state into the next
    FastLED.setBrightness(brightness);
    matrix.setFont(nullptr);
    matrix.setTextSize(1);
    matrix.setTextWrap(false);

    if (notification.isActive()) {
        notification.render(matrix, Color::YELLOW);
    } else if (mode == MODE_TEST) {
        DISPLAY_TESTS[testIndex].render(matrix, frameCounter);
    } else {
        ClockFace::render(matrix, timeKeeper, clockColor);
    }

    FastLED.show();

    frameCounter++;
}
