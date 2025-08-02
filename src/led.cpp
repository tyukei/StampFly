#include "led.hpp"
#include "sensor.hpp"
#include "rc.hpp"
#include "flight_control.hpp"

uint32_t Led_color = 0x000000;
uint32_t Led_color2 = 255;
uint32_t Led_color3 = 0x000000;
uint16_t LedBlinkCounter=0;
CRGB led_esp[1];
CRGB led_onboard[2];

// EEG variables
float current_eeg_value = 0.0f;
unsigned long last_eeg_update = 0;
bool eeg_led_mode = false;

void led_drive(void);
void onboard_led1(CRGB p, uint8_t state);
void onboard_led2(CRGB p, uint8_t state);
void esp_led(CRGB p, uint8_t state);


void led_init(void)
{
  FastLED.addLeds<WS2812, PIN_LED_ONBORD, GRB>(led_onboard, 2);
  FastLED.addLeds<WS2812, PIN_LED_ESP, GRB>(led_esp, 1);
}

void led_show(void)
{
  FastLED.show();
}

void led_drive(void)
{
  // Check for EEG signal timeout (5 seconds)
  if (eeg_led_mode && (millis() - last_eeg_update > 5000)) {
    eeg_led_mode = false;
    Led_color = EEG_NO_SIGNAL;
  }
  
  if (Mode == AVERAGE_MODE)
  {
    onboard_led1(PERPLE, 1);
    onboard_led2(PERPLE, 1);
  }
  else if(Mode == FLIGHT_MODE)
  {
    // Priority: EEG mode overrides normal flight LED colors
    if (eeg_led_mode) {
      // EEG value has been updated, color already set in update_eeg_led()
      // Keep current Led_color from EEG
    } else {
      // Normal flight mode LED colors
      if(Control_mode == ANGLECONTROL)
      {
        if(Flip_flag==0)Led_color=0xffff00;
        else Led_color = 0xFF9933;
      }
      else Led_color = 0xDC669B;

      if(Alt_flag == 1) Led_color = 0x331155;
    }
    
    // Error conditions override EEG colors
    if(Rc_err_flag == 1) Led_color = 0xff0000;

    if (Under_voltage_flag < UNDER_VOLTAGE_COUNT) {onboard_led1(Led_color, 1);onboard_led2(Led_color, 1);}
    else {onboard_led1(POWEROFFCOLOR,1);onboard_led1(POWEROFFCOLOR,1);}
  }

  else if (Mode == PARKING_MODE)
  {
    if(Under_voltage_flag < UNDER_VOLTAGE_COUNT)
    {
      //イルミネーション
      if(LedBlinkCounter==0){//<10
        if (Led_color2&0x800000)Led_color2 = (Led_color2<<1)|1;
        else Led_color2=Led_color2<<1; 

        if (Under_voltage_flag < UNDER_VOLTAGE_COUNT) {onboard_led1(Led_color2, 1);onboard_led2(Led_color2, 1);}
        //else onboard_led(POWEROFFCOLOR,1);
        LedBlinkCounter++;
      }
      LedBlinkCounter++;
      if (LedBlinkCounter>20)LedBlinkCounter=0;
    }
    else
    {
      //水色点滅
      if (LedBlinkCounter < 10) { onboard_led1(POWEROFFCOLOR,1);onboard_led2(POWEROFFCOLOR,1);}
      else if (LedBlinkCounter < 200) { onboard_led1(POWEROFFCOLOR,0);onboard_led2(POWEROFFCOLOR,0);}
      else LedBlinkCounter = 0;
      LedBlinkCounter ++;
    }
  }

  //LED show
  FastLED.show();
}

void onboard_led1(CRGB p, uint8_t state)
{
  if (state ==1)
  {
    led_onboard[0]=p;
  } 
  else {
    led_onboard[0]=0;
  }
  return;
}

void onboard_led2(CRGB p, uint8_t state)
{
  if (state ==1)
  {
    led_onboard[1]=p;
  } 
  else {
    led_onboard[1]=0;
  }
  return;
}

void esp_led(CRGB p, uint8_t state)
{
  if (state ==1) led_esp[0]=p;
  else led_esp[0]=0;
  return;
}

// EEG concentration level to color mapping
uint32_t get_eeg_color(float eeg_value) {
    if (eeg_value < 0.5f) {
        return EEG_LOW_FOCUS;      // Blue - Very low concentration
    } else if (eeg_value < 1.5f) {
        return EEG_MID_FOCUS;      // Green - Low to medium concentration
    } else if (eeg_value < 2.5f) {
        return EEG_HIGH_FOCUS;     // Red - High concentration
    } else if (eeg_value < 4.0f) {
        return EEG_VERY_HIGH;      // Magenta - Very high concentration
    } else {
        return WHITE;              // White - Extreme concentration
    }
}

// Update EEG value and enable EEG LED mode
void update_eeg_led(float eeg_value) {
    current_eeg_value = eeg_value;
    last_eeg_update = millis();
    eeg_led_mode = true;
    
    // Update LED color based on EEG value
    Led_color = get_eeg_color(eeg_value);
}
