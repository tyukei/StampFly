#ifndef LED_HPP
#define LED_HPP

#include <FastLED.h>
#include <stdint.h>

#define WHITE 0xffffff
#define BLUE 0x0000ff
#define RED 0xff0000
#define GREEN 0x00ff00
#define PERPLE 0xff00ff
#define POWEROFFCOLOR 0x18EBF9
#define FLIPCOLOR 0xFF9933

// EEG concentration level colors
#define EEG_LOW_FOCUS    0x0000FF  // Blue - Low concentration
#define EEG_MID_FOCUS    0x00FF00  // Green - Medium concentration  
#define EEG_HIGH_FOCUS   0xFF0000  // Red - High concentration
#define EEG_VERY_HIGH    0xFF00FF  // Magenta - Very high concentration
#define EEG_NO_SIGNAL    0x404040  // Gray - No EEG signal

#define PIN_LED_ONBORD 39
#define PIN_LED_ESP    21
#define NUM_LEDS   1

extern uint32_t Led_color;
extern float current_eeg_value;
extern unsigned long last_eeg_update;

void led_init(void);
void led_show(void);
void led_drive(void);
void onboard_led1(CRGB p, uint8_t state);
void onboard_led2(CRGB p, uint8_t state);
void esp_led(CRGB p, uint8_t state);
uint32_t get_eeg_color(float eeg_value);
void update_eeg_led(float eeg_value);

#endif