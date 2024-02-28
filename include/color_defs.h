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

#endif // __COLOR_DEFS_H__
