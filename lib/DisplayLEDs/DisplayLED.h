#include "FastLED.h"

#define DATA_PIN 18
#define NUM_LEDS 2

CRGB leds[NUM_LEDS];

class DisplayLED
{
public:
    DisplayLED();
    void on(int idx, CRGB colour);
    void off(int idx);
    void flash(int idx, CRGB colour, int duration_millis);
    void colour(float val, float min, float max, float brightness);
};

DisplayLED::DisplayLED()
{
    FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
}

void DisplayLED::on(int idx, CRGB colour)
{
    leds[idx] = colour;
    FastLED.show();
}

void DisplayLED::off(int idx)
{
    leds[idx] = CRGB::Black;
    FastLED.show();
}

void DisplayLED::flash(int idx, CRGB colour, int duration_millis)
{
    on(idx, colour);
    delay(duration_millis);
    off(idx);
    delay(duration_millis);
}

// 'breathing' function https://sean.voisen.org/blog/2011/10/breathing-led-with-arduino/
float breathe(float brightness)
{
    return (exp(sin(millis() / 2000.0 * PI)) - 0.36787944) * 108.0 * brightness;
}

void DisplayLED::colour(float val, float min, float max, float brightness)
{
    const int hue_max = 160; // blue in the FastLED 'rainbow' colour map
    CHSV pixelColour;

    val = constrain(val, min, max);

    pixelColour.sat = 255;
    pixelColour.hue = map(val, min, max, hue_max, 0);
    pixelColour.val = map(breathe(brightness), 0, 255, 30, 255);

    fill_solid(leds, NUM_LEDS, pixelColour);
    FastLED.show();
}