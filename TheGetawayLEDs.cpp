#include "Arduino.h"
#define SMALLEST_CODESIZE
#include <ky-040.h>
#include "LEDAccessoryBoard.h"
#include <Adafruit_NeoPixel.h>
#include <math.h>
#ifdef __AVR__
  #include <avr/power.h>
#endif
#include <EEPROM.h>



/***************************************************************
 *  Set up these definitions based on install
 */

#define UNDERCAB_NUMBER_OF_RAILS    2
 
#define STRIP_1_NUMBER_OF_PIXELS    39
#define STRIP_2_NUMBER_OF_PIXELS    14
#define STRIP_3_NUMBER_OF_PIXELS    0
#define STRIP_4_NUMBER_OF_PIXELS    0
#define STRIP_5_NUMBER_OF_PIXELS    0

// Parameter 1 = number of pixels in strip
// Parameter 2 = Arduino pin number (most are valid)
// Parameter 3 = pixel type flags, add together as needed:
//   NEO_KHZ800  800 KHz bitstream (most NeoPixel products w/WS2812 LEDs)
//   NEO_KHZ400  400 KHz (classic 'v1' (not v2) FLORA pixels, WS2811 drivers)
//   NEO_GRB     Pixels are wired for GRB bitstream (most NeoPixel products)
//   NEO_RGB     Pixels are wired for RGB bitstream (v1 FLORA pixels, not v2)
//   NEO_RGBW    Pixels are wired for RGBW bitstream (NeoPixel RGBW products)
Adafruit_NeoPixel StripOfLEDs1 = Adafruit_NeoPixel(STRIP_1_NUMBER_OF_PIXELS, STRIP_1_CONTROL, NEO_BGR + NEO_KHZ800);
Adafruit_NeoPixel StripOfLEDs2 = Adafruit_NeoPixel(STRIP_2_NUMBER_OF_PIXELS, STRIP_2_CONTROL, NEO_GRB + NEO_KHZ800);

// Getaway Defines
// A13 (5V)  - TP2 5V Test Point
//     (GND) - TP5 GND
// A14 (L flipper) - n/a
// A15 (R flipper) - n/a
//
// Connector 1 (pins from top to bottom)
// 1:  A12 (D7)  - Q42: Right Bank Flasher J126Pin1
// 2:  A11 (D3)  - Q22: Mars Lamp J123Pin4
// 3:  A10 (D8)  - Q36: Freeride Flasher J126Pin4
// 4:  A9  (D4)  - Q38: Left Slingshot Flasher J126Pin3
// 5:  A8  (D9)  - Q28: Left Ramp Flasher J126Pin5
// 6:  A7  (D5)  - Q40: Supercharger Flasher J126Pin2
// 7:  KEY
// 8:  A6  (D10) - Q30: Left Bank Flasher J126Pin6
// 9:  A5  (D6)  - Q32: Right Slingshot Flasher J126Pin8
//
// Connector 2  (pin from top to bottom)
// 1:  A4  (D11)  - Q86: Stop Light Green
// 2:  A3  (D14)  - Q87: Stop Light Yellow
// 3:  Key
// 4:  A2  (D12)  - Q88: Stop Light Red
// 5:  A1  (D15)  - n/a
// 6:  A0  (D13)  - Q92 (lamp column 7) to Pin D2 on Arduino for interrupt

#define MARS_LAMP_INPUT_NUMBER                3
#define LEFT_SLINGSHOT_FLASHER_INPUT_NUMBER   4
#define SUPERCHARGER_FLASHER_INPUT_NUMBER     5
#define RIGHT_SLINGSHOT_FLASHER_INPUT_NUMBER  6
#define RIGHT_BANK_FLASHER_INPUT_NUMBER       7
#define FREERIDE_FLASHER_INPUT_NUMBER         8
#define LEFT_RAMP_FLASHER_INPUT_NUMBER        9
#define LEFT_BANK_FLASHER_INPUT_NUMBER        10


// The LightingNeedsToCheckMachineFor5V() function should return either:
//    false = No, nothing is hooked to the 5V sensor
//    true  = Yes, check the 5V sensor to see if the machine is on
boolean LightingNeedsToCheckMachineFor5V() {
  return true;
}

// The LightingNeedsToCheckMachineForFlipperActivity() function should return either:
//    false = No, the flipper sensing lines shouldn't dictate the machine state
//    true  = Yes, flipper activity should tell us if the machine is in attract or game play
boolean LightingNeedsToCheckMachineForFlipperActivity() {
  return false;
}


// The LightingNeedsToWatchI2C() function should return either:
//    false = No, the accessory doesn't respond to I2C messages
//    true  = Yes, the accessory responds to I2C messages
boolean LightingNeedsToWatchI2C() {
  return false;
}

byte IncomingI2CDeviceAddress() {
  return 0xFF;
}


byte Input11State = 0;
byte Input14State = 0;
byte Input12State = 0;

volatile unsigned long ISRCurrentTime;
volatile unsigned long NumInterruptsSeen = 0;

void Input13Triggered() {
  // Read values for 11, 14, and 12 (green, yellow, red)
  Input11State *= 2;
  Input14State *= 2;
  Input12State *= 2;
  if (!(PINF & 0x10)) {
    Input11State |= 1;
  }
  if (!(PINF & 0x08)) {
    Input14State |= 1;
  }
  if (!(PINF & 0x04)) {
    Input12State |= 1;
  }

  NumInterruptsSeen += 1;
}


void InitializeAllStrips() {
  StripOfLEDs1.begin();
  StripOfLEDs1.show(); // Initialize all pixels to 'off'
  StripOfLEDs2.begin();
  StripOfLEDs2.show(); // Initialize all pixels to 'off'

//  pinMode(2, INPUT);
  pinMode(2, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(2), Input13Triggered, RISING);
}

void InitializeRGBValues() {
  // not used in this game
  analogWrite(NON_ADDRESSABLE_RGB_RED_PIN, 0);
  analogWrite(NON_ADDRESSABLE_RGB_GREEN_PIN, 0);
  analogWrite(NON_ADDRESSABLE_RGB_BLUE_PIN, 0);
}



int AttractModeBrightness = 255;
int GameModeBrightness = 64;
int AnimationBrightness[16] = {255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255, 255};

#define EEPROM_CONFIRMATION_UL                0x4C454430
#define EEPROM_INIT_CONFIRMATION_ADDRESS      200
#define EEPROM_ATTRACT_BRIGHTNESS_ADDRESS     205
#define EEPROM_GAME_BRIGHTNESS_ADDRESS        206
#define EEPROM_INPUT_0_BRIGHTNESS_ADDRESS     210
#define EEPROM_INPUT_1_BRIGHTNESS_ADDRESS     211
#define EEPROM_INPUT_2_BRIGHTNESS_ADDRESS     212
#define EEPROM_INPUT_3_BRIGHTNESS_ADDRESS     213
#define EEPROM_INPUT_4_BRIGHTNESS_ADDRESS     214
#define EEPROM_INPUT_5_BRIGHTNESS_ADDRESS     215
#define EEPROM_INPUT_6_BRIGHTNESS_ADDRESS     216
#define EEPROM_INPUT_7_BRIGHTNESS_ADDRESS     217
#define EEPROM_INPUT_8_BRIGHTNESS_ADDRESS     218
#define EEPROM_INPUT_9_BRIGHTNESS_ADDRESS     219
#define EEPROM_INPUT_10_BRIGHTNESS_ADDRESS    220
#define EEPROM_INPUT_11_BRIGHTNESS_ADDRESS    221
#define EEPROM_INPUT_12_BRIGHTNESS_ADDRESS    222
#define EEPROM_INPUT_13_BRIGHTNESS_ADDRESS    223
#define EEPROM_INPUT_14_BRIGHTNESS_ADDRESS    224
#define EEPROM_INPUT_15_BRIGHTNESS_ADDRESS    225

boolean ReadSettings() {

  unsigned long testVal =   (((unsigned long)EEPROM.read(EEPROM_INIT_CONFIRMATION_ADDRESS+3))<<24) | 
                            ((unsigned long)(EEPROM.read(EEPROM_INIT_CONFIRMATION_ADDRESS+2))<<16) | 
                            ((unsigned long)(EEPROM.read(EEPROM_INIT_CONFIRMATION_ADDRESS+1))<<8) | 
                            ((unsigned long)(EEPROM.read(EEPROM_INIT_CONFIRMATION_ADDRESS)));
  if (testVal!=EEPROM_CONFIRMATION_UL) {
    // Write the defaults compiled above
    WriteSettings();
    EEPROM.write(EEPROM_INIT_CONFIRMATION_ADDRESS+3, EEPROM_CONFIRMATION_UL>>24);
    EEPROM.write(EEPROM_INIT_CONFIRMATION_ADDRESS+2, 0xFF & (EEPROM_CONFIRMATION_UL>>16));
    EEPROM.write(EEPROM_INIT_CONFIRMATION_ADDRESS+1, 0xFF & (EEPROM_CONFIRMATION_UL>>8));
    EEPROM.write(EEPROM_INIT_CONFIRMATION_ADDRESS+0, 0xFF & (EEPROM_CONFIRMATION_UL));
    //Serial.write("No Confirmation bytes: Inserting default values\n");
  } else {
    //Serial.write("Confirmation bytes okay\n");
  }               

  AttractModeBrightness = EEPROM.read(EEPROM_ATTRACT_BRIGHTNESS_ADDRESS);
  GameModeBrightness = EEPROM.read(EEPROM_GAME_BRIGHTNESS_ADDRESS);

  for (byte count=0; count<16; count++) {
    AnimationBrightness[count] = EEPROM.read(EEPROM_INPUT_0_BRIGHTNESS_ADDRESS+count);
  }

  return true;
}

boolean WriteSettings() {
  EEPROM.write(EEPROM_ATTRACT_BRIGHTNESS_ADDRESS, AttractModeBrightness);
  EEPROM.write(EEPROM_GAME_BRIGHTNESS_ADDRESS, GameModeBrightness);
  for (byte count=0; count<16; count++) {
    EEPROM.write(EEPROM_INPUT_0_BRIGHTNESS_ADDRESS+count, AnimationBrightness[count]);
  }

  return true;
}






/***************************************************************
 *  For each input, we have an animation to run based on the
 *  last time we saw it fired
 */
boolean ShowAnimationForInput3(unsigned long lastInputSeenTime, unsigned long currentTime, byte machineState, unsigned long machineStateChangedTime) {

  // MARS LAMP
  // if we haven't seen input 3 in over a second, we're not going to show anything
  if (lastInputSeenTime==0 || currentTime>(lastInputSeenTime+1000)) {
    return false;
  }

  byte marsLampPhase = ((currentTime/*-lastInputSeenTime*/)/13)%STRIP_1_NUMBER_OF_PIXELS;
  byte speakerLampPhaseL = (currentTime/60)%8;
  byte speakerLampPhaseR = (currentTime/80)%6;
//  Serial.write("Saw input 3\n");
  if (currentTime<(lastInputSeenTime+500)) {
    // animation for recent input
    StripOfLEDs1.setPixelColor(marsLampPhase, StripOfLEDs1.Color(250, 0, 0));
    StripOfLEDs2.setPixelColor(speakerLampPhaseR, StripOfLEDs2.Color(250, 0, 0));
    StripOfLEDs2.setPixelColor(speakerLampPhaseL+6, StripOfLEDs2.Color(250, 0, 0));
  } else {
    // animation for fading input
  }

  (void)machineState;
  (void)machineStateChangedTime;
  return true;
}


boolean ShowAnimationForInput4(unsigned long lastInputSeenTime, unsigned long currentTime, byte machineState, unsigned long machineStateChangedTime) {

  // LEFT SLINGSHOT FLASHER
  // if we haven't seen input 4 in over 500ms, we're not going to show anything
  if (lastInputSeenTime==0 || currentTime>(lastInputSeenTime+500)) {
    return false;
  }

#if (UNDERCAB_NUMBER_OF_RAILS==4) 
  if (currentTime<(lastInputSeenTime+50)) {
    for (byte count=4; count<16; count++) StripOfLEDs1.setPixelColor(count, StripOfLEDs1.Color(0, 200, 0));
  } else if (currentTime<(lastInputSeenTime+150)) {
  } else if (currentTime<(lastInputSeenTime+200)) {
    for (byte count=4; count<16; count++) StripOfLEDs1.setPixelColor(count, StripOfLEDs1.Color(0, 200, 0));
  } else if (currentTime<(lastInputSeenTime+300)) {
  } else if (currentTime<(lastInputSeenTime+350)) {
    for (byte count=4; count<16; count++) StripOfLEDs1.setPixelColor(count, StripOfLEDs1.Color(0, 200, 0));
  }
#else
  if (currentTime<(lastInputSeenTime+50)) {
    for (byte count=0; count<(STRIP_1_NUMBER_OF_PIXELS/2); count++) StripOfLEDs1.setPixelColor((STRIP_1_NUMBER_OF_PIXELS-1)-count, StripOfLEDs1.Color(0, 200, 0));
  } else if (currentTime<(lastInputSeenTime+150)) {
  } else if (currentTime<(lastInputSeenTime+200)) {
    for (byte count=0; count<(STRIP_1_NUMBER_OF_PIXELS/2); count++) StripOfLEDs1.setPixelColor((STRIP_1_NUMBER_OF_PIXELS-1)-count, StripOfLEDs1.Color(0, 200, 0));
  } else if (currentTime<(lastInputSeenTime+300)) {
  } else if (currentTime<(lastInputSeenTime+350)) {
    for (byte count=0; count<(STRIP_1_NUMBER_OF_PIXELS/2); count++) StripOfLEDs1.setPixelColor((STRIP_1_NUMBER_OF_PIXELS-1)-count, StripOfLEDs1.Color(0, 200, 0));
  }
#endif
  
  (void)machineState;
  (void)machineStateChangedTime;
  return true;
}


boolean ShowAnimationForInput5(unsigned long lastInputSeenTime, unsigned long currentTime, byte machineState, unsigned long machineStateChangedTime) {

  // SUPERCHARGER FLASHER
  // if we haven't seen input 5 in over 500ms, we're not going to show anything
  if (lastInputSeenTime==0 || currentTime>(lastInputSeenTime+500)) {
    return false;
  }

#if (UNDERCAB_NUMBER_OF_RAILS==4)
  byte backToFrontPhase = ((currentTime-lastInputSeenTime)/21)%12;
  StripOfLEDs1.setPixelColor(backToFrontPhase+4, StripOfLEDs1.Color(250, 250, 250));
  StripOfLEDs1.setPixelColor(31-backToFrontPhase, StripOfLEDs1.Color(250, 250, 250));
#else
  byte backToFrontPhase = ((currentTime-lastInputSeenTime)/21)%(STRIP_1_NUMBER_OF_PIXELS/2);
  StripOfLEDs1.setPixelColor(backToFrontPhase, StripOfLEDs1.Color(250, 250, 250));
  StripOfLEDs1.setPixelColor((STRIP_1_NUMBER_OF_PIXELS-1)-backToFrontPhase, StripOfLEDs1.Color(250, 250, 250));
#endif
  
  (void)machineState;
  (void)machineStateChangedTime;
  return true;
}


boolean ShowAnimationForInput6(unsigned long lastInputSeenTime, unsigned long currentTime, byte machineState, unsigned long machineStateChangedTime) {

  // RIGHT SLINGSHOT FLASHER
  // if we haven't seen input 6 in over 500ms, we're not going to show anything
  if (lastInputSeenTime==0 || currentTime>(lastInputSeenTime+500)) {
    return false;
  }

#if (UNDERCAB_NUMBER_OF_RAILS==4)
  if (currentTime<(lastInputSeenTime+50)) {
    for (byte count=20; count<32; count++) StripOfLEDs1.setPixelColor(count, StripOfLEDs1.Color(250, 0, 0));
  } else if (currentTime<(lastInputSeenTime+150)) {
  } else if (currentTime<(lastInputSeenTime+200)) {
    for (byte count=20; count<32; count++) StripOfLEDs1.setPixelColor(count, StripOfLEDs1.Color(250, 0, 0));
  } else if (currentTime<(lastInputSeenTime+300)) {
  } else if (currentTime<(lastInputSeenTime+350)) {
    for (byte count=20; count<32; count++) StripOfLEDs1.setPixelColor(count, StripOfLEDs1.Color(250, 0, 0));
  }
#else
  if (currentTime<(lastInputSeenTime+50)) {
    for (byte count=0; count<(STRIP_1_NUMBER_OF_PIXELS/2); count++) StripOfLEDs1.setPixelColor(count, StripOfLEDs1.Color(250, 0, 0));
  } else if (currentTime<(lastInputSeenTime+150)) {
  } else if (currentTime<(lastInputSeenTime+200)) {
    for (byte count=0; count<(STRIP_1_NUMBER_OF_PIXELS/2); count++) StripOfLEDs1.setPixelColor(count, StripOfLEDs1.Color(250, 0, 0));
  } else if (currentTime<(lastInputSeenTime+300)) {
  } else if (currentTime<(lastInputSeenTime+350)) {
    for (byte count=0; count<(STRIP_1_NUMBER_OF_PIXELS/2); count++) StripOfLEDs1.setPixelColor(count, StripOfLEDs1.Color(250, 0, 0));
  }
#endif  
  
  (void)machineState;
  (void)machineStateChangedTime;
  return true;
}


boolean ShowAnimationForInput7(unsigned long lastInputSeenTime, unsigned long currentTime, byte machineState, unsigned long machineStateChangedTime) {

  // RIGHT BANK FLASHER
  // if we haven't seen input 7 in over 750ms, we're not going to show anything
  if (lastInputSeenTime==0 || currentTime>(lastInputSeenTime+256)) {
    return false;
  }

#if (UNDERCAB_NUMBER_OF_RAILS==4)
  byte lampPhase = (1 + (currentTime-lastInputSeenTime)/(16))%16;
  for (byte count=0; count<lampPhase; count++) {
    if (count<6) StripOfLEDs1.setPixelColor(26+count, StripOfLEDs1.Color(200, 200, 0));
    StripOfLEDs1.setPixelColor(25-count, StripOfLEDs1.Color(200, 100, 0));
  }
#else
  byte lampPhase = (1 + (currentTime-lastInputSeenTime)/(16))%20;
  for (byte count=0; count<lampPhase; count++) {
    if (count<10) StripOfLEDs1.setPixelColor(9-count, StripOfLEDs1.Color(200, 200, 0));
    StripOfLEDs1.setPixelColor(20+count, StripOfLEDs1.Color(200, 100, 0));
  }
#endif  
  
  (void)machineState;
  (void)machineStateChangedTime;
  return true;
}

unsigned long Animation8StartTime = 0;
byte FreeRidePhase = 0;
boolean ShowAnimationForInput8(unsigned long lastInputSeenTime, unsigned long currentTime, byte machineState, unsigned long machineStateChangedTime) {

  // FREE RIDE FLASHER
  if (lastInputSeenTime==0 || currentTime>(lastInputSeenTime+500)) {
    Animation8StartTime = 0;
    return false;
  }

  if (Animation8StartTime==0) {
    Animation8StartTime = lastInputSeenTime;
  } else if (currentTime>(Animation8StartTime+500)) {
    Animation8StartTime = currentTime;
    FreeRidePhase = (FreeRidePhase + 1)%4;
  }

  byte lampPhase = ((currentTime-Animation8StartTime)/50)%10;

#if (UNDERCAB_NUMBER_OF_RAILS==4)
  if (FreeRidePhase==0) {
    if (lampPhase==0) {
      for (byte count=4; count<16; count++) StripOfLEDs1.setPixelColor(count, StripOfLEDs1.Color(200, 0, 0));
    } else if (lampPhase>0 && lampPhase<5) {
      StripOfLEDs1.setPixelColor(16+lampPhase, StripOfLEDs1.Color(200, 0, 0));
    } else if (lampPhase==5) {
      for (byte count=20; count<32; count++) StripOfLEDs1.setPixelColor(count, StripOfLEDs1.Color(200, 0, 0));
    } else {
      StripOfLEDs1.setPixelColor(16+(9-lampPhase), StripOfLEDs1.Color(200, 0, 0));
    }
  } else if (FreeRidePhase==1) {
    if (lampPhase==0) {
      for (byte count=20; count<32; count++) StripOfLEDs1.setPixelColor(count, StripOfLEDs1.Color(0, 200, 0));
    } else if (lampPhase>0 && lampPhase<5) {
      StripOfLEDs1.setPixelColor(16+(4-lampPhase), StripOfLEDs1.Color(0, 200, 0));
    } else if (lampPhase==5) {
      for (byte count=4; count<16; count++) StripOfLEDs1.setPixelColor(count, StripOfLEDs1.Color(0, 200, 0));
    } else {
      StripOfLEDs1.setPixelColor(16+(lampPhase-5), StripOfLEDs1.Color(0, 200, 0));
    }
  } else if (FreeRidePhase==2) {
    if (lampPhase==0) {
      for (byte count=4; count<16; count++) StripOfLEDs1.setPixelColor(count, StripOfLEDs1.Color(0, 200, 0));
    } else if (lampPhase>0 && lampPhase<5) {
      StripOfLEDs1.setPixelColor(16+lampPhase, StripOfLEDs1.Color(0, 200, 0));
    } else if (lampPhase==5) {
      for (byte count=20; count<32; count++) StripOfLEDs1.setPixelColor(count, StripOfLEDs1.Color(0, 200, 0));
    } else {
      StripOfLEDs1.setPixelColor(16+(9-lampPhase), StripOfLEDs1.Color(0, 200, 0));
    }
  } else if (FreeRidePhase==3) {
    if (lampPhase==0) {
      for (byte count=20; count<32; count++) StripOfLEDs1.setPixelColor(count, StripOfLEDs1.Color(200, 0, 0));
    } else if (lampPhase>0 && lampPhase<5) {
      StripOfLEDs1.setPixelColor(16+(4-lampPhase), StripOfLEDs1.Color(200, 0, 0));
    } else if (lampPhase==5) {
      for (byte count=4; count<16; count++) StripOfLEDs1.setPixelColor(count, StripOfLEDs1.Color(200, 0, 0));
    } else {
      StripOfLEDs1.setPixelColor(16+(lampPhase-5), StripOfLEDs1.Color(200, 0, 0));
    }
  }
#else
  if (FreeRidePhase==0) {
    if (lampPhase==0) {
      for (byte count=0; count<(STRIP_1_NUMBER_OF_PIXELS/2); count++) StripOfLEDs1.setPixelColor(count, StripOfLEDs1.Color(200, 0, 0));
    } else if (lampPhase>0 && lampPhase<5) {
      StripOfLEDs1.setPixelColor((STRIP_1_NUMBER_OF_PIXELS/2)+lampPhase, StripOfLEDs1.Color(200, 0, 0));
    } else if (lampPhase==5) {
      for (byte count=(STRIP_1_NUMBER_OF_PIXELS/2); count<(STRIP_1_NUMBER_OF_PIXELS); count++) StripOfLEDs1.setPixelColor(count, StripOfLEDs1.Color(200, 0, 0));
    } else {
      StripOfLEDs1.setPixelColor((STRIP_1_NUMBER_OF_PIXELS/2)+(9-lampPhase), StripOfLEDs1.Color(200, 0, 0));
    }
  } else if (FreeRidePhase==1) {
    if (lampPhase==0) {
      for (byte count=(STRIP_1_NUMBER_OF_PIXELS/2); count<(STRIP_1_NUMBER_OF_PIXELS); count++) StripOfLEDs1.setPixelColor(count, StripOfLEDs1.Color(0, 200, 0));
    } else if (lampPhase>0 && lampPhase<5) {
      StripOfLEDs1.setPixelColor((STRIP_1_NUMBER_OF_PIXELS/2)+(4-lampPhase), StripOfLEDs1.Color(0, 200, 0));
    } else if (lampPhase==5) {
      for (byte count=0; count<(STRIP_1_NUMBER_OF_PIXELS/2); count++) StripOfLEDs1.setPixelColor(count, StripOfLEDs1.Color(0, 200, 0));
    } else {
      StripOfLEDs1.setPixelColor((STRIP_1_NUMBER_OF_PIXELS/2)+(lampPhase-5), StripOfLEDs1.Color(0, 200, 0));
    }
  } else if (FreeRidePhase==2) {
    if (lampPhase==0) {
      for (byte count=0; count<(STRIP_1_NUMBER_OF_PIXELS/2); count++) StripOfLEDs1.setPixelColor(count, StripOfLEDs1.Color(0, 200, 0));
    } else if (lampPhase>0 && lampPhase<5) {
      StripOfLEDs1.setPixelColor((STRIP_1_NUMBER_OF_PIXELS/2)+lampPhase, StripOfLEDs1.Color(0, 200, 0));
    } else if (lampPhase==5) {
      for (byte count=(STRIP_1_NUMBER_OF_PIXELS/2); count<(STRIP_1_NUMBER_OF_PIXELS); count++) StripOfLEDs1.setPixelColor(count, StripOfLEDs1.Color(0, 200, 0));
    } else {
      StripOfLEDs1.setPixelColor((STRIP_1_NUMBER_OF_PIXELS/2)+(9-lampPhase), StripOfLEDs1.Color(0, 200, 0));
    }
  } else if (FreeRidePhase==3) {
    if (lampPhase==0) {
      for (byte count=(STRIP_1_NUMBER_OF_PIXELS/2); count<(STRIP_1_NUMBER_OF_PIXELS); count++) StripOfLEDs1.setPixelColor(count, StripOfLEDs1.Color(200, 0, 0));
    } else if (lampPhase>0 && lampPhase<5) {
      StripOfLEDs1.setPixelColor((STRIP_1_NUMBER_OF_PIXELS/2)+(4-lampPhase), StripOfLEDs1.Color(200, 0, 0));
    } else if (lampPhase==5) {
      for (byte count=0; count<(STRIP_1_NUMBER_OF_PIXELS/2); count++) StripOfLEDs1.setPixelColor(count, StripOfLEDs1.Color(200, 0, 0));
    } else {
      StripOfLEDs1.setPixelColor((STRIP_1_NUMBER_OF_PIXELS/2)+(lampPhase-5), StripOfLEDs1.Color(200, 0, 0));
    }
  }
#endif  


/*
  if (currentTime<(lastInputSeenTime+25)) {
    for (byte count=0; count<8; count++) StripOfLEDs1.setPixelColor(count + (FreeRidePhase*8), StripOfLEDs1.Color(250, 250, 250));
  } else if (currentTime<(lastInputSeenTime+100)) {
  } else if (currentTime<(lastInputSeenTime+125)) {
    for (byte count=0; count<8; count++) StripOfLEDs1.setPixelColor(count + (FreeRidePhase*8), StripOfLEDs1.Color(250, 250, 250));
  } else if (currentTime<(lastInputSeenTime+200)) {
  } else if (currentTime<(lastInputSeenTime+225)) {
    for (byte count=0; count<8; count++) StripOfLEDs1.setPixelColor(count + (FreeRidePhase*8), StripOfLEDs1.Color(250, 250, 250));
  } else if (currentTime<(lastInputSeenTime+300)) {
  } else if (currentTime<(lastInputSeenTime+325)) {    
    for (byte count=0; count<8; count++) StripOfLEDs1.setPixelColor(count + (FreeRidePhase*8), StripOfLEDs1.Color(250, 250, 250));
  } else if (currentTime<(lastInputSeenTime+375)) {        
  } else {    
    for (byte count=0; count<8; count++) StripOfLEDs1.setPixelColor(count + (FreeRidePhase*8), StripOfLEDs1.Color(250, 250, 250));
  }
*/  
  
  (void)machineState;
  (void)machineStateChangedTime;
  return true;
}

boolean ShowAnimationForInput9(unsigned long lastInputSeenTime, unsigned long currentTime, byte machineState, unsigned long machineStateChangedTime) {

  // LEFT RAMP FLASHER
  // if we haven't seen input 8 in over a second, we're not going to show anything
  if (lastInputSeenTime==0 || currentTime>(lastInputSeenTime+250)) {
    return false;
  }

#if (UNDERCAB_NUMBER_OF_RAILS==4)
  byte lampPhase = ((currentTime-lastInputSeenTime)/(25))%8;
  for (byte count=0; count<8; count++) {
    StripOfLEDs1.setPixelColor(count+lampPhase, StripOfLEDs1.Color(250, 0, 0));
    StripOfLEDs1.setPixelColor(count+8+lampPhase, StripOfLEDs1.Color(250, 250, 250));
    StripOfLEDs1.setPixelColor(count+16+lampPhase, StripOfLEDs1.Color(0, 250, 0));
  }
#else
  byte lampPhase = ((currentTime-lastInputSeenTime)/(25))%8;
  for (byte count=0; count<8; count++) {
    StripOfLEDs1.setPixelColor((STRIP_1_NUMBER_OF_PIXELS-1)-lampPhase, StripOfLEDs1.Color(250, 0, 0));
    StripOfLEDs1.setPixelColor((STRIP_1_NUMBER_OF_PIXELS-9)-lampPhase, StripOfLEDs1.Color(250, 250, 250));
    StripOfLEDs1.setPixelColor((STRIP_1_NUMBER_OF_PIXELS-17)-lampPhase, StripOfLEDs1.Color(0, 250, 0));
  }
#endif  
  
  (void)machineState;
  (void)machineStateChangedTime;
  return true;
}


boolean ShowAnimationForInput10(unsigned long lastInputSeenTime, unsigned long currentTime, byte machineState, unsigned long machineStateChangedTime) {

  // LEFT BANK FLASHER
  // if we haven't seen input 10 in over 750ms, we're not going to show anything
  if (lastInputSeenTime==0 || currentTime>(lastInputSeenTime+256)) {
    return false;
  }

#if (UNDERCAB_NUMBER_OF_RAILS==4)
  byte lampPhase = (1 + (currentTime-lastInputSeenTime)/(16))%16;
  for (byte count=0; count<lampPhase; count++) {
    StripOfLEDs1.setPixelColor(10+count, StripOfLEDs1.Color(200, 200, 0));
    if (count<10) StripOfLEDs1.setPixelColor(9-count, StripOfLEDs1.Color(100, 200, 0));
  }
#else
  byte lampPhase = (1 + (currentTime-lastInputSeenTime)/(16))%16;
  for (byte count=0; count<lampPhase; count++) {
    StripOfLEDs1.setPixelColor((STRIP_1_NUMBER_OF_PIXELS-10)-count, StripOfLEDs1.Color(200, 200, 0));
    if (count<10) StripOfLEDs1.setPixelColor((STRIP_1_NUMBER_OF_PIXELS-10)+count, StripOfLEDs1.Color(100, 200, 0));
  }
#endif
  
  (void)machineState;
  (void)machineStateChangedTime;
  return true;
}

unsigned long Input11StartTime = 0;
unsigned long Input14StartTime = 0;
unsigned long Input12StartTime = 0;

boolean ShowSpeakerLamps(unsigned long  *lastInputSeenTime, unsigned long currentTime, unsigned long currentAnimationFrame) {

  byte lampPhase = (currentAnimationFrame%30);
  int lampScale = 0;
  if (lampPhase<=15) {
    lampScale = 17 * lampPhase;
  } else {
    lampScale = (30-lampPhase) * 17;    
  }

  if (Input12State) {
    if (Input12StartTime==0) Input12StartTime = currentTime;
  } else {
    Input12StartTime = 0;
  }

  if (Input14State) {
    if (Input14StartTime==0) Input14StartTime = currentTime;
  } else {
    Input14StartTime = 0;
  }

  if (Input11State) {
    if (Input11StartTime==0) Input11StartTime = currentTime;
  } else {
    Input11StartTime = 0;
  }

  if (lampScale==0) {
//    char buf[128];
//    sprintf(buf, "11=%d, 14=%d, 12=%d\n", Input11State, Input14State, Input12State);
//    Serial.write(buf);
  }
  
  if (Input12State > 0x40 && Input12StartTime && currentTime>(Input12StartTime+1000)) {
    int curRed = (180 * lampScale)/255;
    int curGreen = (0 * lampScale)/255;
    int curBlue = (0 * lampScale)/255;
    for (byte count=0; count<STRIP_2_NUMBER_OF_PIXELS; count++) StripOfLEDs2.setPixelColor(count, StripOfLEDs1.Color(curRed, curBlue, curGreen));
  } else if (Input14State > 0x40 && Input14StartTime && currentTime>(Input14StartTime+1000)) {
    int curRed = (250 * lampScale)/255;
    int curGreen = (80 * lampScale)/255;
    int curBlue = (0 * lampScale)/255;
    for (byte count=0; count<STRIP_2_NUMBER_OF_PIXELS; count++) StripOfLEDs2.setPixelColor(count, StripOfLEDs1.Color(curRed, curBlue, curGreen));
  } else if (Input11State > 0x40 && Input11StartTime && currentTime>(Input11StartTime+1000)) {
    int curRed = (0 * lampScale)/255;
    int curGreen = (100 * lampScale)/255;
    int curBlue = (0 * lampScale)/255;
    for (byte count=0; count<STRIP_2_NUMBER_OF_PIXELS; count++) StripOfLEDs2.setPixelColor(count, StripOfLEDs1.Color(curRed, curBlue, curGreen));
  } else if ((Input12State&0x3F)==0x3F) {
//    for (byte count=0; count<STRIP_2_NUMBER_OF_PIXELS; count++) StripOfLEDs2.setPixelColor(count, StripOfLEDs1.Color(70, 0, 0));
  } else if ((Input14State&0x3F)==0x3F) {
//    for (byte count=0; count<STRIP_2_NUMBER_OF_PIXELS; count++) StripOfLEDs2.setPixelColor(count, StripOfLEDs1.Color(70, 0, 10));
  } else if ((Input11State&0x3F)==0x3F) {
//    for (byte count=0; count<STRIP_2_NUMBER_OF_PIXELS; count++) StripOfLEDs2.setPixelColor(count, StripOfLEDs1.Color(0, 0, 40));
  } else {


    // If we're not showing the traffic signal status, we can respond to other flashers
    if (currentTime < (lastInputSeenTime[LEFT_SLINGSHOT_FLASHER_INPUT_NUMBER]+120)) {
      if (currentAnimationFrame & 0x01) for (byte count=6; count<STRIP_2_NUMBER_OF_PIXELS; count++) StripOfLEDs2.setPixelColor(count, StripOfLEDs1.Color(0, 250, 0));
    } else if (currentTime < (lastInputSeenTime[RIGHT_SLINGSHOT_FLASHER_INPUT_NUMBER]+150)) {
      if (currentAnimationFrame & 0x01) for (byte count=0; count<6; count++) StripOfLEDs2.setPixelColor(count, StripOfLEDs1.Color(250, 0, 0));
    } else if (currentTime < (lastInputSeenTime[SUPERCHARGER_FLASHER_INPUT_NUMBER]+60)) {  
      for (byte count=0; count<STRIP_2_NUMBER_OF_PIXELS; count++) StripOfLEDs2.setPixelColor(count, StripOfLEDs1.Color(250, 250, 250));
    } else if (currentTime < (lastInputSeenTime[LEFT_BANK_FLASHER_INPUT_NUMBER]+60)) {
      for (byte count=6; count<STRIP_2_NUMBER_OF_PIXELS; count++) StripOfLEDs2.setPixelColor(count, StripOfLEDs1.Color(100, 100, 0));
    } else if (currentTime < (lastInputSeenTime[RIGHT_BANK_FLASHER_INPUT_NUMBER]+60)) {
      for (byte count=0; count<6; count++) StripOfLEDs2.setPixelColor(count, StripOfLEDs1.Color(100, 100, 0));
    } else {
      return false;
    }
  }

  (void)lastInputSeenTime;
  (void)currentTime;
  return true;
}


boolean RGBHasBeenShown = true;

boolean UpdateRGBBasedOnInputs(unsigned long  *lastInputSeenTime, unsigned long currentTime, byte machineState, unsigned long machineStateChangedTime) {
  // not used in this game

  if (machineState==MACHINE_STATE_OFF) {
    if (RGBHasBeenShown) {
      analogWrite(NON_ADDRESSABLE_RGB_RED_PIN, 0);
      analogWrite(NON_ADDRESSABLE_RGB_GREEN_PIN, 0);
      analogWrite(NON_ADDRESSABLE_RGB_BLUE_PIN, 0);
      RGBHasBeenShown = false;
    }
    return false;
  }

  (void)lastInputSeenTime;
  (void)currentTime;
  (void)machineStateChangedTime;
  return false;
}


boolean UpdateRGBBasedOnInputs(unsigned long  *lastInputSeenTime, unsigned long currentTime, unsigned long currentAnimationFrame, byte machineState, unsigned long machineStateChangedTime) {
  // This accessory doesn't use this function
  (void)lastInputSeenTime;
  (void)currentTime;
  (void)currentAnimationFrame;
  (void)machineState;
  (void)machineStateChangedTime;
  return false;
}
boolean UpdateStripsBasedOnI2C(unsigned long lastMessageSeenTime, byte lastMessage, byte lastParameter, byte red, byte green, byte blue, short duration, unsigned long currentTime, unsigned long currentAnimationFrame, byte machineState, unsigned long machineStateChangedTime) {
  // This accessory doesn't use this function
  (void)lastMessageSeenTime;
  (void)lastMessage;
  (void)lastParameter;
  (void)currentTime;
  (void)currentAnimationFrame;
  (void)machineState;
  (void)machineStateChangedTime;
  (void)red;
  (void)green;
  (void)blue;
  (void)duration;
  return false;
}
boolean UpdateRGBBasedOnI2C(unsigned long lastMessageSeenTime, byte lastMessage, byte lastParameter, byte red, byte green, byte blue, short duration, unsigned long currentTime, unsigned long currentAnimationFrame, byte machineState, unsigned long machineStateChangedTime) {
  // This accessory doesn't use this function
  (void)lastMessageSeenTime;
  (void)lastMessage;
  (void)lastParameter;
  (void)currentTime;
  (void)currentAnimationFrame;
  (void)machineState;
  (void)machineStateChangedTime;
  (void)red;
  (void)green;
  (void)blue;
  (void)duration;
  return false;
}


boolean StripHasBeenShown = true;
unsigned long LastTimeUpdated = 0;
unsigned long LastTimeInterruptsReported = 0;

boolean UpdateStripsBasedOnInputs(unsigned long  *lastInputSeenTime, unsigned long currentTime, unsigned long currentAnimationFrame, byte machineState, unsigned long machineStateChangedTime) {

  ISRCurrentTime = currentTime;

  if (machineState==MACHINE_STATE_OFF) {
    // If the machine is off, we'll clear the strip, show it (turn all off), and return
    if (StripHasBeenShown) {
      StripOfLEDs1.clear();
      StripOfLEDs1.show();
      StripOfLEDs2.clear();
      StripOfLEDs2.show();
      StripHasBeenShown = false;
    }
    return false;
  } else if (machineState==MACHINE_STATE_ATTRACT_MODE && currentTime<(machineStateChangedTime+3000)) {
    // If the machine has been turned on in the past 3 seconds, we won't show anything yet
    // (give it a chance to boot and come up to power)
    if (StripHasBeenShown) {
      StripOfLEDs1.clear();
      StripOfLEDs1.show();
      StripOfLEDs2.clear();
      StripOfLEDs2.show();
      StripHasBeenShown = false;
    }
    return false;
  }

  if (currentTime > (LastTimeInterruptsReported+1000)) {
    char buf[128];

    sprintf(buf, "IPS=%lu, 0x%02X, 0x%02X, 0x%02X\n", NumInterruptsSeen, Input11State, Input14State, Input12State);
    NumInterruptsSeen = 0;
    Serial.write(buf);

    LastTimeInterruptsReported = currentTime;
  }

  if (currentTime < (LastTimeUpdated+50)) {
    return false;
  }
  LastTimeUpdated = currentTime;

  StripOfLEDs1.clear();
  StripOfLEDs2.clear();

  // These functions should be called in reverse order of importance
  // (so the most important function updates the strips last)
  ShowAnimationForInput4(lastInputSeenTime[LEFT_SLINGSHOT_FLASHER_INPUT_NUMBER], currentTime, machineState, machineStateChangedTime);
  ShowAnimationForInput5(lastInputSeenTime[SUPERCHARGER_FLASHER_INPUT_NUMBER], currentTime, machineState, machineStateChangedTime);
  ShowAnimationForInput6(lastInputSeenTime[RIGHT_SLINGSHOT_FLASHER_INPUT_NUMBER], currentTime, machineState, machineStateChangedTime);
  ShowAnimationForInput7(lastInputSeenTime[RIGHT_BANK_FLASHER_INPUT_NUMBER], currentTime, machineState, machineStateChangedTime);
  ShowAnimationForInput8(lastInputSeenTime[FREERIDE_FLASHER_INPUT_NUMBER], currentTime, machineState, machineStateChangedTime);
  ShowAnimationForInput9(lastInputSeenTime[LEFT_RAMP_FLASHER_INPUT_NUMBER], currentTime, machineState, machineStateChangedTime);
  ShowAnimationForInput10(lastInputSeenTime[LEFT_BANK_FLASHER_INPUT_NUMBER], currentTime, machineState, machineStateChangedTime);

  // Green, yellow, and red shown on grills
  ShowSpeakerLamps(lastInputSeenTime, currentTime, currentAnimationFrame);

  // Mars lamp done last so it will overlay onto other effects
  ShowAnimationForInput3(lastInputSeenTime[MARS_LAMP_INPUT_NUMBER], currentTime, machineState, machineStateChangedTime);
  
  StripOfLEDs1.show();
  StripOfLEDs2.show();
  StripHasBeenShown = true;

  (void)currentAnimationFrame;
  return true;
}






byte AdvanceSettingsMode(byte oldSettingsMode) {
  oldSettingsMode += 1;
  if (oldSettingsMode>16) {
    StripOfLEDs1.clear();
    StripOfLEDs2.clear();
    StripOfLEDs1.show();
    StripOfLEDs2.show();
    oldSettingsMode = SETTINGS_MODE_OFF;
  }
  return oldSettingsMode;
}

unsigned long LastTimeAnimationTriggered = 0;
boolean ShowSettingsMode(byte settingsMode, unsigned long currentAnimationFrame) {
  if (settingsMode==SETTINGS_MODE_OFF) return false;

  StripOfLEDs1.clear();
  StripOfLEDs2.clear();

  byte lampPhase = (currentAnimationFrame%30);
  int lampScale = 0;
  if (lampPhase<=15) {
    lampScale = 17 * lampPhase;
  } else {
    lampScale = (30-lampPhase) * 17;    
  }  

  int curRed;
  int curGreen;
  int curBlue;

  if (LastTimeAnimationTriggered==0) LastTimeAnimationTriggered = millis();
  switch (settingsMode) {
    case 1:
      if (!ShowAnimationForInput3(LastTimeAnimationTriggered, millis(), MACHINE_STATE_GAME_MODE, 0)) LastTimeAnimationTriggered = 0;
      break;
    case 2:
      if (!ShowAnimationForInput4(LastTimeAnimationTriggered, millis(), MACHINE_STATE_GAME_MODE, 0)) LastTimeAnimationTriggered = 0;
      break;
    case 3:
      if (!ShowAnimationForInput5(LastTimeAnimationTriggered, millis(), MACHINE_STATE_GAME_MODE, 0)) LastTimeAnimationTriggered = 0;
      break;
    case 4:
      if (!ShowAnimationForInput6(LastTimeAnimationTriggered, millis(), MACHINE_STATE_GAME_MODE, 0)) LastTimeAnimationTriggered = 0;
      break;
    case 5:
      if (!ShowAnimationForInput7(LastTimeAnimationTriggered, millis(), MACHINE_STATE_GAME_MODE, 0)) LastTimeAnimationTriggered = 0;
      break;
    case 6:
      if (!ShowAnimationForInput8(LastTimeAnimationTriggered, millis(), MACHINE_STATE_GAME_MODE, 0)) LastTimeAnimationTriggered = 0;
      break;
    case 7:
      if (!ShowAnimationForInput9(LastTimeAnimationTriggered, millis(), MACHINE_STATE_GAME_MODE, 0)) LastTimeAnimationTriggered = 0;
      break;
    case 8:
      if (!ShowAnimationForInput10(LastTimeAnimationTriggered, millis(), MACHINE_STATE_GAME_MODE, 0)) LastTimeAnimationTriggered = 0;
      break;
    case 9:
      curRed = (0 * lampScale)/255;
      curGreen = (100 * lampScale)/255;
      curBlue = (0 * lampScale)/255;
      for (byte count=0; count<STRIP_2_NUMBER_OF_PIXELS; count++) StripOfLEDs2.setPixelColor(count, StripOfLEDs1.Color(curRed, curBlue, curGreen));
      break;
    case 10:
      curRed = (250 * lampScale)/255;
      curGreen = (80 * lampScale)/255;
      curBlue = (0 * lampScale)/255;
      for (byte count=0; count<STRIP_2_NUMBER_OF_PIXELS; count++) StripOfLEDs2.setPixelColor(count, StripOfLEDs1.Color(curRed, curBlue, curGreen));
      break;
    case 11:
      curRed = (180 * lampScale)/255;
      curGreen = (0 * lampScale)/255;
      curBlue = (0 * lampScale)/255;
      for (byte count=0; count<STRIP_2_NUMBER_OF_PIXELS; count++) StripOfLEDs2.setPixelColor(count, StripOfLEDs1.Color(curRed, curBlue, curGreen));
      break;
    case 12:
      if (currentAnimationFrame & 0x01) for (byte count=6; count<STRIP_2_NUMBER_OF_PIXELS; count++) StripOfLEDs2.setPixelColor(count, StripOfLEDs1.Color(0, 250, 0));
      break;
    case 13:
      if (currentAnimationFrame & 0x01) for (byte count=0; count<6; count++) StripOfLEDs2.setPixelColor(count, StripOfLEDs1.Color(250, 0, 0));
      break;
    case 14:
      for (byte count=0; count<STRIP_2_NUMBER_OF_PIXELS; count++) StripOfLEDs2.setPixelColor(count, StripOfLEDs1.Color(250, 250, 250));
      break;
    case 15:
      for (byte count=6; count<STRIP_2_NUMBER_OF_PIXELS; count++) StripOfLEDs2.setPixelColor(count, StripOfLEDs1.Color(100, 100, 0));
      break;
    case 16:
      for (byte count=0; count<6; count++) StripOfLEDs2.setPixelColor(count, StripOfLEDs1.Color(100, 100, 0));
      break;
  }
  
  StripOfLEDs1.show();
  StripOfLEDs2.show();

  return true;
}

boolean IncreaseBrightness(byte settingsMode) {
  switch (settingsMode) {
    case 1: 
      AttractModeBrightness += 20;
      if (AttractModeBrightness>255) AttractModeBrightness = 255;
      break;
    case 2: 
      GameModeBrightness += 20;
      if (GameModeBrightness>255) GameModeBrightness = 255;
      break;
    case 3: 
      AnimationBrightness[3] += 40;
      if (AnimationBrightness[3]>255) AnimationBrightness[3] = 255;
      break;
    case 4: 
      AnimationBrightness[4] += 40;
      if (AnimationBrightness[4]>255) AnimationBrightness[4] = 255;
      break;
    case 5: 
      AnimationBrightness[7] += 40;
      if (AnimationBrightness[7]>255) AnimationBrightness[7] = 255;
      break;
    case 6: 
      AnimationBrightness[8] += 40;
      if (AnimationBrightness[8]>255) AnimationBrightness[8] = 255;
      break;
    case 7: 
      AnimationBrightness[9] += 40;
      if (AnimationBrightness[9]>255) AnimationBrightness[9] = 255;
      break;
  }

  return true;
}

boolean DecreaseBrightness(byte settingsMode) {
  switch (settingsMode) {
    case 1: 
      AttractModeBrightness -= 20;
      if (AttractModeBrightness<0) AttractModeBrightness = 0;
      break;
    case 2: 
      GameModeBrightness -= 20;
      if (GameModeBrightness<0) GameModeBrightness = 0;
      break;
    case 3: 
      AnimationBrightness[3] -= 40;
      if (AnimationBrightness[3]<0) AnimationBrightness[3] = 0;
      break;
    case 4: 
      AnimationBrightness[4] -= 40;
      if (AnimationBrightness[4]<0) AnimationBrightness[4] = 0;
      break;
    case 5: 
      AnimationBrightness[7] -= 40;
      if (AnimationBrightness[7]<0) AnimationBrightness[7] = 0;
      break;
    case 6: 
      AnimationBrightness[8] -= 40;
      if (AnimationBrightness[8]<0) AnimationBrightness[8] = 0;
      break;
    case 7: 
      AnimationBrightness[9] -= 40;
      if (AnimationBrightness[9]<0) AnimationBrightness[9] = 0;
      break;
  }  

  return true;
}
