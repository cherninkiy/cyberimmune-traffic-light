#ifndef __COLOR_DEFS_H__
#define __COLOR_DEFS_H__

typedef enum {
    ColorUnknown = 0, ColorRed, ColorYellow, ColorGreen,
    ColorRedYellow, ColorBlinkYellow, ColorBlinkGreen
} ColorIds;

static const char* ColorNames[] = {
    "unknown", "red", "yellow", "green",
    "red-yellow", "blink-yellow", "blink-green"
};

inline static const char* GetColorName(unsigned int mode) {
    switch(mode & 0xFF) {
        case 0x01: return ColorNames[ColorRed];
        case 0x02: return ColorNames[ColorYellow];
        case 0x04: return ColorNames[ColorGreen];
        case 0x03: return ColorNames[ColorRedYellow];
        case 0x0A: return ColorNames[ColorBlinkYellow];
        case 0x0C: return ColorNames[ColorBlinkGreen];
        default: return ColorNames[ColorUnknown];
    }
}

// #define KNRM  "\x1B[0m"
// #define KRED  "\x1B[31m"
// #define KGRN  "\x1B[32m"
// #define KYEL  "\x1B[33m"

static const char* ConsoleColors[] = {
    "\x1B[40munknown\x1B[0m",
    "\x1B[41mred\x1B[0m",
    "\x1B[43myellow\x1B[0m",
    "\x1B[42mgreen\x1B[0m",
    "\x1B[41mred\x1B[0m-\x1B[43myellow\x1B[0m",
    "\x1B[40mblink\x1B[0m-\x1B[43myellow\x1B[0m",
    "\x1B[40mblink\x1B[0m-\x1B[42mgreen\x1B[0m"
};

inline static const char* GetConsoleColor(unsigned int mode) {
    switch(mode & 0xFF) {
        case 0x01: return ConsoleColors[ColorRed];
        case 0x02: return ConsoleColors[ColorYellow];
        case 0x04: return ConsoleColors[ColorGreen];
        case 0x03: return ConsoleColors[ColorRedYellow];
        case 0x0A: return ConsoleColors[ColorBlinkYellow];
        case 0x0C: return ConsoleColors[ColorBlinkGreen];
        default: return ConsoleColors[ColorUnknown];
    }
}

inline unsigned int GetColorMode(const char* color) {
    if (strcmp(color, "red") == 0)
        return 0x01;
    if (strcmp(color, "yellow") == 0)
        return 0x02;
    if (strcmp(color, "green") == 0)
        return 0x04;
    if (strcmp(color, "blink") == 0)
        return 0x08;
    if (strcmp(color, "red-yellow") == 0)
        return 0x03;
    if (strcmp(color, "blink-yellow") == 0)
        return 0x0A;
    if (strcmp(color, "blink-green") == 0)
        return 0x0C;
    return 0x00;
}

#endif // __COLOR_DEFS_H__
