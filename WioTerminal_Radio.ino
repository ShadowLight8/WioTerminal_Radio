// Screen includes
#include"TFT_eSPI.h"
#include "Free_Fonts.h"

// Backlight control includes
#include "lcd_backlight.hpp"

// Radio includes
#include <Arduino.h>
#include <Wire.h>
#include <radio.h>
#include <RDA5807M.h>
#include <RDSParser.h>

// Global objects
TFT_eSPI tft;
static LCDBackLight backlight;
RDA5807M radio;
RDSParser rds;

void DisplayRDS(char *name) {
  //tft.print(" RDS: ");
  //tft.println(name);
  gui_RDS(name);
}


void DisplayRDSText(char *name) {
  //tft.print("Text: ");
  //tft.println(name);
  gui_Text(name);
}

void RDS_process(bool rds_ready, bool rds_sync, uint8_t blockA_err, uint8_t blockB_err, uint16_t block1, uint16_t block2, uint16_t block3, uint16_t block4) {
  tft.setTextFont(1);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  //  tft.print(rds_ready);
  //  tft.print(" - ");
  //  tft.print(rds_sync);
  //  tft.print(" - ");
  //  tft.print(blockA_err);
  //  tft.print(" - ");
  //  tft.println(blockB_err);

  //Serial.print(block1, HEX); Serial.print(' '); Serial.print(block2, HEX); Serial.print(' '); Serial.print(block3, HEX); Serial.print(' '); Serial.println(block4, HEX);
  //gui_RDSSignal(blockA_err, blockB_err);

  gui_RDSSync(rds_sync); // Shows if RDS is generally present
  if (rds_sync && rds_ready && blockA_err != 2 && blockB_err != 2)
    gui_RDSReady(rds_ready); // More like an RDS heartbeat indicator - While RDS is synced, only update on new rds_readys
  else if (!rds_sync)
    gui_RDSReady(0); // If not in sync, don't display

  if (rds_ready && blockA_err != 2 && blockB_err != 2) { // RDS data ready and the errors a minimal
    //tft.print("G - ");
    //tft.print((block2 & 0xF000) >> 12);
    //tft.print(" AB - ");
    //tft.print((block2 & 0x0800) >> 11);
    //tft.println("     ");

    // Get PI and display
    //tft.println(block1, HEX);
    if (blockA_err == 0)
      gui_PI(block1);

    if (blockB_err == 0)
      gui_PTY((block2 & 0x03E0) >> 5);

    rds.processData(block1, block2, block3, block4);
  }
}

// Radio environment variables
int env_vol = 0;
long env_vol_lastmillis;
bool env_scanTune = true;
long env_scanTune_lastmillis;
double env_frequency = 100.1;
double env_frequency_last = 100.1;
long env_frequency_lastmillis;
bool env_state_scanning = false;
RADIO_INFO env_radio_info;
long env_radio_info_lastmillis;
bool env_tuned;
bool env_monostereo;
int env_rssi;
int env_backlight = 10;
long env_topbuttons_lastmillis;
bool env_forceMono = false;
bool env_bassBoost = true;

void setup() {
  tft.begin();
  tft.setRotation(3);
  //digitalWrite(LCD_BACKLIGHT, HIGH); // turn on the backlight
  tft.fillScreen(TFT_BLACK);
  backlight.initialize();
  backlight.setBrightness(env_backlight);

  //tft.setFreeFont(FM9); // BUG: Free Font displayed by write function doesn't use background even when set
  tft.setTextFont(4);
  tft.setTextColor(TFT_DARKGREY, TFT_BLACK);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(1);
  tft.drawString("Wio Terminal FM Radio", 320 / 2, 240 / 2 - 15);
  tft.drawString("Initializing...", 320 / 2, 240 / 2 + 15);
  tft.setTextWrap(true, true);

  //Serial.begin(57600);
  //Serial.println("Radio...");
  //delay(500);

  // Check for Grove I2C FM Receiver
  radio.init();

  radio.debugEnable();
  radio.setMono(env_forceMono);
  radio.setMute(false);
  radio.setSoftMute(true);
  radio.setBassBoost(env_bassBoost);

  radio.setFrequency(env_frequency * 100);
  radio.setVolume(env_vol);

  radio.attachReceiveRDS_Ex(RDS_process);
  rds.attachServicenNameCallback(DisplayRDS);
  rds.attachTextCallback(DisplayRDSText);
  //rds.attachTimecallback(DisplayRDSTime);
  radio.clearRDS_Ex();

  // Setup Buttons
  pinMode(WIO_5S_UP, INPUT_PULLUP);
  pinMode(WIO_5S_DOWN, INPUT_PULLUP);
  pinMode(WIO_5S_LEFT, INPUT_PULLUP);
  pinMode(WIO_5S_RIGHT, INPUT_PULLUP);
  pinMode(WIO_5S_PRESS, INPUT_PULLUP);
  pinMode(WIO_KEY_A, INPUT_PULLUP);
  pinMode(WIO_KEY_B, INPUT_PULLUP);
  pinMode(WIO_KEY_C, INPUT_PULLUP);

  //tft.fillScreen(TFT_RED);
  //tft.fillScreen(TFT_BLUE);
  //tft.fillScreen(TFT_GREEN);
  tft.fillScreen(TFT_BLACK);

  drawStaticItems();

  gui_setVol(env_vol);
  gui_setFreq(env_frequency);
  gui_ScanTune(env_scanTune);
  gui_RDSReady(false);
  gui_RDSSync(false);
  gui_Tuned(env_tuned);
  gui_MonoStereo(env_monostereo);
  gui_ForceMono(env_forceMono);
  gui_BaseBoost(env_bassBoost);
}

void loop() {
  // Check for button states

  if (digitalRead(WIO_5S_UP) == LOW) {
    // Volumn Up
    if (millis() - env_vol_lastmillis > 250)
    {
      env_vol++;
      if (env_vol >= 16) env_vol = 15;
      if (env_vol == 0) radio.setMute(false);
      radio.setVolume(env_vol);
      gui_setVol(env_vol);
      env_vol_lastmillis = millis();
    }
  }
  else if (digitalRead(WIO_5S_DOWN) == LOW) {
    // Volumn Down
    if (millis() - env_vol_lastmillis > 250)
    {
      env_vol--;
      if (env_vol > -1) {
        radio.setVolume(env_vol);
      } else {
        env_vol = -1;
        radio.setMute(true);
      }
      gui_setVol(env_vol);
      env_vol_lastmillis = millis();
    }
  }
  else if (digitalRead(WIO_5S_LEFT) == LOW) {
    // Freq down
    if (millis() - env_frequency_lastmillis > 150) {
      if (env_scanTune) {
        // Scan mode
        radio.seekDown(true);
        env_state_scanning = true;
      } else {
        // Tune mode
        env_frequency -= 0.1;
        if (env_frequency < 87) env_frequency = 108.0;
        radio.setFrequency(env_frequency * 100);
        //gui_setFreq(env_frequency);
      }
      radio.clearRDS_Ex();
      gui_RDSSignal(-1, -1);
      gui_Tuned(false);
      gui_Signal(0);
      env_frequency_lastmillis = millis();
    }
  }
  else if (digitalRead(WIO_5S_RIGHT) == LOW) {
    // Freq up
    if (millis() - env_frequency_lastmillis > 150) {
      if (env_scanTune) {
        // Scan mode
        radio.seekUp(true);
        env_state_scanning = true;
      } else {
        // Tune mode
        env_frequency += 0.1;
        if (env_frequency > 108) env_frequency = 87.0;
        radio.setFrequency(env_frequency * 100);
        //gui_setFreq(env_frequency);
      }
      radio.clearRDS_Ex();
      gui_RDSSignal(-1, -1);
      gui_Tuned(false);
      gui_Signal(0);
      env_frequency_lastmillis = millis();
    }
  }
  else if (digitalRead(WIO_5S_PRESS) == LOW) {
    // Scan/Tune switch
    if (millis() - env_scanTune_lastmillis > 333) {
      env_scanTune = !env_scanTune;
      gui_ScanTune(env_scanTune);
      env_scanTune_lastmillis = millis();
    }

  }
  else if (digitalRead(WIO_KEY_A) == LOW) {
    // Right button
    // Force Mono
    if (millis() - env_topbuttons_lastmillis > 333) {
      env_forceMono = !env_forceMono;
      radio.setMono(env_forceMono);
      gui_ForceMono(env_forceMono);
      env_topbuttons_lastmillis = millis();
    }
  }
  else if (digitalRead(WIO_KEY_B) == LOW) {
    // Middle button
    // Bass Boost
    if (millis() - env_topbuttons_lastmillis > 333) {
      env_bassBoost = !env_bassBoost;
      radio.setBassBoost(env_bassBoost);
      gui_BaseBoost(env_bassBoost);
      env_topbuttons_lastmillis = millis();
    }

  }
  else if (digitalRead(WIO_KEY_C) == LOW) {
    // Left button
    // Backlight H/M/L/Off
    if (millis() - env_topbuttons_lastmillis > 333) {
      if (env_backlight == 10)
        env_backlight = 25;
      else if (env_backlight == 25)
        env_backlight = 100;
      else if (env_backlight == 100)
        env_backlight = 1;
      else if (env_backlight == 1)
        env_backlight = 10;
      backlight.setBrightness(env_backlight);
      env_topbuttons_lastmillis = millis();
    }
  }

  // Determine updates needed
  if (env_state_scanning) {
    env_frequency = radio.getFrequency() / 100.0;
    if (env_frequency != env_frequency_last) {
      gui_setFreq(env_frequency);
      env_frequency_last = env_frequency;
    }
    radio.getRadioInfo(&env_radio_info);
    if (env_radio_info.tuned) {
      gui_Tuned(true);
      env_state_scanning = false;
    }

    //    env_frequency = radio.getFrequency() / 100.0;
    //    if (env_frequency != env_frequency_last) {
    //      gui_setFreq(env_frequency);
    //    } else if (millis() - env_frequency_lastmillis > 500) {
    //      env_state_scanning = false;
    //    }
    //    env_frequency_last = env_frequency;
  } else {
    // Slow periodic status updates
    if (millis() - env_radio_info_lastmillis > 501) {
      env_radio_info_lastmillis = millis();
      env_frequency = radio.getFrequency() / 100.0;
      if (env_frequency != env_frequency_last) {
        gui_setFreq(env_frequency);
        env_frequency_last = env_frequency;
      }

      radio.getRadioInfo(&env_radio_info);
      if (env_radio_info.tuned != env_tuned) {
        gui_Tuned(env_radio_info.tuned);
        env_tuned = env_radio_info.tuned;
      }
      if (env_radio_info.stereo != env_monostereo) {
        gui_MonoStereo(env_radio_info.stereo);
        env_monostereo = env_radio_info.stereo;
      }
      if (env_radio_info.rssi != env_rssi) {
        gui_Signal(env_radio_info.rssi);
        env_rssi = env_radio_info.rssi;
      }
      if (env_radio_info.rds)
      {
        // Seems to be thrashing around even with a good signal
        // if rds, then check the error levels
        // New function needed to pull error levels
        //gui_RDSSignal(1, 1);
      } else {
        //gui_RDSSignal(-1, -1);
      }
    } else {
      radio.checkRDS();
    }
  }
  delay(10);
}

void drawStaticItems()
{
  // Button related labels - TFT_BLUE
  // Static labels - TFT_DARKGREY
  // Dynamic values - TFT_WHITE
  // Toggle items - TFT_DARKGREY <-> TFT_CYAN

  // Volume
  // Down arrow
  tft.drawRect(320 - 17, 240 - 25, 2, 15, TFT_BLUE);
  tft.drawLine(320 - 17, 240 - 10, 320 - 22, 240 - 15, TFT_BLUE);
  tft.drawLine(320 - 17, 240 - 11, 320 - 22, 240 - 16, TFT_BLUE);
  tft.drawLine(320 - 16, 240 - 10, 320 - 11, 240 - 15, TFT_BLUE);
  tft.drawLine(320 - 16, 240 - 11, 320 - 11, 240 - 16, TFT_BLUE);

  // Vol label
  tft.setTextFont(2);
  tft.setTextColor(TFT_DARKGREY);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(1);
  tft.drawString("Vol", 320 - 16, 240 - 33);

  // Up arrow
  tft.drawRect(320 - 17, 240 - 78, 2, 15, TFT_BLUE);
  tft.drawLine(320 - 17, 240 - 78, 320 - 22, 240 - 73, TFT_BLUE);
  tft.drawLine(320 - 17, 240 - 77, 320 - 22, 240 - 72, TFT_BLUE);
  tft.drawLine(320 - 16, 240 - 78, 320 - 11, 240 - 73, TFT_BLUE);
  tft.drawLine(320 - 16, 240 - 77, 320 - 11, 240 - 72, TFT_BLUE);

  // Frequency
  // Left Arrow
  tft.drawRect(25, 190, 15, 2, TFT_BLUE);
  tft.drawLine(25, 190, 30, 185, TFT_BLUE);
  tft.drawLine(26, 190, 31, 185, TFT_BLUE);
  tft.drawLine(25, 191, 30, 196, TFT_BLUE);
  tft.drawLine(26, 191, 31, 196, TFT_BLUE);

  // Right Arrow
  tft.drawRect(230, 190, 15, 2, TFT_BLUE);
  tft.drawLine(245, 190, 240, 185, TFT_BLUE);
  tft.drawLine(244, 190, 239, 185, TFT_BLUE);
  tft.drawLine(245, 191, 240, 196, TFT_BLUE);
  tft.drawLine(244, 191, 239, 196, TFT_BLUE);

  // MHz label
  tft.setTextFont(4);
  tft.setTextColor(TFT_DARKGREY);
  tft.drawString("MHz", 200, 202);

  // Scan/Tune Mode
  // Dot
  tft.fillCircle(71, 219, 3, TFT_BLUE);

  tft.setTextDatum(TL_DATUM);

  // Status
  tft.setTextFont(2);
  tft.setTextColor(TFT_DARKGREY);
  tft.drawString("Status", 1, 150); // Was 87

  // Signal
  tft.drawString("Signal", 200, 150);

  // Above Radio info divider
  tft.drawRect(0, 145, 320, 2, TFT_DARKGREY);

  // RDS Station
  tft.setTextFont(4);
  tft.setTextColor(TFT_DARKGREY);
  tft.drawString("RDS", -1, 60);

  // Text
  tft.setTextFont(2);
  tft.drawString("Text", 1, 92);

  // PI
  tft.drawString("PI", 1, 124);

  // Decode
  tft.drawString("Decode", 70, 124);

  // PTY
  tft.drawString("PTY", 170, 124);

  // Top divider
  tft.drawRect(0, 50, 320, 2, TFT_DARKGREY);

  // Button C
  tft.drawRect(0, 0, 50, 2, TFT_BLUE);

  // Button B
  tft.drawRect(90, 0, 50, 2, TFT_BLUE);

  // Button A
  tft.drawRect(180, 0, 50, 2, TFT_BLUE);
  tft.setTextFont(1);
  tft.setTextColor(TFT_DARKGREY);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("Backlight", 1, 4);
  //tft.drawString("Low", 1, 12);
}

void gui_setVol(int value)
{
  tft.fillRect(290, 180, 30, 18, TFT_BLACK);
  // Vol Value - 0-15 for RDA5807M
  tft.setTextFont(4);
  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  if (value != -1)
    tft.drawString(String(value + 1), 320 - 16, 240 - 48);
  else
    tft.drawString("-", 320 - 16, 240 - 48);
  gui_static_volLabel();
}

void gui_static_volLabel()
{
  // Vol label
  tft.setTextFont(2);
  tft.setTextColor(TFT_DARKGREY);
  tft.setTextDatum(MC_DATUM);
  tft.setTextSize(1);
  tft.drawString("Vol", 320 - 16, 240 - 33);
}

void gui_setFreq(double value)
{
  tft.fillRect(50, 170, 125, 38, TFT_BLACK);

  // Frequency Value
  tft.setTextFont(6);
  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum(MC_DATUM);
  tft.drawString(String(value, 1), 110, 195);
}

void gui_ScanTune(bool value)
{
  // True - Scan
  // False - Tune
  // Scan/Tune Mode
  // REMOVE - Scan/Tune Value
  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextDatum(ML_DATUM);
  if (value)
  {
    tft.drawString("Scan Mode", 78, 220);
  } else {
    tft.drawString("Tune Mode", 78, 220);
  }
}

void gui_Text(char * value)
{
  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum(TL_DATUM);
  tft.setCursor(35, 92);
  tft.fillRect(35, 92, 285, 16, TFT_BLACK);
  tft.fillRect(0, 108, 320, 16, TFT_BLACK);
  tft.print(value);
}

void gui_RDS(char * value)
{
  tft.setTextFont(4);
  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum(TL_DATUM);
  tft.fillRect(57, 60, 237, 26, TFT_BLACK);
  tft.drawString(value, 57, 60);
}

int last_value1 = -1;
int last_value2 = -1;
void gui_RDSSignal(int value1, int value2)
{
  // RDS Signal/Error Level
  // 0 - 0 errors - 4 blocks
  // 1 - 1-2 errors - 3 blocks
  // 2 - 3-5 errors - 2 blocks
  // 3 - 6+ errors - 1 blocks
  // No RDS - 1 Large X block

  if (value1 != last_value1 || value2 != last_value2) {
    last_value1 = value1;
    last_value2 = value2;
    // Clear area
    tft.fillRect(294, 104, 23, 11, TFT_BLACK);
    if (value1 == -1 && value2 == -1) {
      // No RDS indicator
      tft.drawRect(306, 104, 11, 11, TFT_RED);
      tft.drawLine(307, 105, 316, 114, TFT_RED);
      tft.drawLine(307, 113, 315, 105, TFT_RED);
    }

    // Indicator for A Blocks
    if (value1 != -1)
      tft.fillRect(312, 104, 5, 5, TFT_GREEN);
    if (value1 >= 0 && value1 <= 2)
      tft.fillRect(306, 104, 5, 5, TFT_GREEN);
    if (value1 >= 0 && value1 <= 1)
      tft.fillRect(300, 104, 5, 5, TFT_GREEN);
    if (value1 == 0)
      tft.fillRect(294, 104, 5, 5, TFT_GREEN);

    // Indicator for B Blocks
    if (value2 != -1)
      tft.fillRect(312, 110, 5, 5, TFT_GREEN);
    if (value2 >= 0 && value2 <= 2)
      tft.fillRect(306, 110, 5, 5, TFT_GREEN);
    if (value2 >= 0 && value2 <= 1)
      tft.fillRect(300, 110, 5, 5, TFT_GREEN);
    if (value2 == 0)
      tft.fillRect(294, 110, 5, 5, TFT_GREEN);
  }
}

bool last_rdsSync = false;
void gui_RDSSync(bool sync)
{ //54
  if (sync != last_rdsSync) {
    last_rdsSync = sync;
    if (sync) {
      // Show RDS present
      tft.fillRect(306, 54, 11, 11, TFT_GREEN);
      tft.drawPixel(306, 54, TFT_BLACK);
      tft.drawPixel(316, 54, TFT_BLACK);
      tft.drawPixel(306, 64, TFT_BLACK);
      tft.drawPixel(316, 64, TFT_BLACK);
    } else {
      // No RDS found
      tft.drawRect(306, 54, 11, 11, TFT_RED);
      tft.fillRect(307, 55, 9, 9, TFT_BLACK);
      tft.drawLine(307, 55, 316, 64, TFT_RED);
      tft.drawLine(307, 63, 315, 55, TFT_RED);
    }
  }
}

int last_rdsReady = 306;
void gui_RDSReady(bool ready)
{
  if (ready) {
    last_rdsReady++;
    if (last_rdsReady > 316) {
      last_rdsReady = 306;
      tft.drawFastVLine(316, 66, 2, TFT_BLACK);
    }
    tft.drawFastVLine(last_rdsReady, 66, 2, TFT_GREEN);
    tft.drawFastVLine(last_rdsReady - 1, 66, 2, TFT_BLACK);
  } else {
    tft.drawFastVLine(last_rdsReady, 66, 2, TFT_BLACK);
  }
}

void gui_Signal(int value)
{
  tft.fillRect(242, 150, 20, 16, TFT_BLACK);
  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum(TL_DATUM);
  tft.drawString(String(value), 242, 150);
}

void gui_Tuned(bool tuned)
{
  tft.fillRect(65, 150, 40, 16, TFT_BLACK);
  if (tuned) {
    tft.setTextFont(2);
    tft.setTextColor(TFT_WHITE);
    tft.setTextDatum(TL_DATUM);
    tft.drawString("Tuned", 65, 150);
  }
}
void gui_MonoStereo(bool stereo)
{
  tft.fillRect(130, 150, 40, 16, TFT_BLACK);
  tft.setTextFont(2);
  tft.setTextColor(TFT_WHITE);
  tft.setTextDatum(TL_DATUM);
  if (stereo)
    tft.drawString("Stereo", 130, 150);
  else
    tft.drawString("Mono", 130, 150);
}

void gui_ForceMono(bool forceMono)
{
  tft.setTextFont(1);
  if (forceMono)
    tft.setTextColor(TFT_CYAN);
  else
    tft.setTextColor(TFT_DARKGREY);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("Force Mono", 181, 4);
}

void gui_BaseBoost(bool bassBoosted)
{
  tft.setTextFont(1);
  if (bassBoosted)
    tft.setTextColor(TFT_CYAN);
  else
    tft.setTextColor(TFT_DARKGREY);
  tft.setTextDatum(TL_DATUM);
  tft.drawString("Bass Boost", 91, 4);
}

int last_block1_1, last_block1_2, last_block1_disp;
void gui_PI(int block1)
{
  if (block1 == 0) {
    tft.fillRect(20, 124, 50, 16, TFT_BLACK);
    tft.fillRect(120, 124, 50, 16, TFT_BLACK);
  } else if (block1 == last_block1_1 && block1 == last_block1_2 && block1 != last_block1_disp) {
    last_block1_disp = block1;
    tft.setTextFont(2);
    tft.setTextColor(TFT_WHITE);
    tft.setTextDatum(TL_DATUM);
    
    // Reverse PI code to get Callsign - Sometimes is right in North America
    int pi_decoding = block1;
    char pi_decoded[4];
    if (pi_decoding >= 21672) {
      // First letter W
      pi_decoded[0] = 'W';
      pi_decoding -= 21672;
    } else {
      // First letter K
      pi_decoded[0] = 'K';
      pi_decoding -= 4096;
    }
    pi_decoded[1] = pi_decoding / 676 + 65;
    pi_decoding %= 676;
    pi_decoded[2] = pi_decoding / 26 + 65;
    pi_decoded[3] = pi_decoding % 26 + 65;
    
    tft.fillRect(20, 124, 50, 16, TFT_BLACK);
    tft.drawString(String(block1, HEX), 20, 124);
    tft.fillRect(120, 124, 50, 16, TFT_BLACK);
    tft.drawString(pi_decoded, 120, 124);
  } else {
    last_block1_2 = last_block1_1;
    last_block1_1 = block1;
  }
}

int last_pty_1, last_pty_2, last_pty_disp;
void gui_PTY(int pty)
{
  if (pty == 0) {
    tft.fillRect(203, 124, 137, 16, TFT_BLACK);
  } else if (pty == last_pty_1 && pty == last_pty_2 && pty != last_pty_disp) {
    last_pty_disp = pty;
    tft.setTextFont(2);
    tft.setTextColor(TFT_WHITE);
    tft.setTextDatum(TL_DATUM);
    tft.fillRect(203, 124, 137, 16, TFT_BLACK);
    switch (pty) {
      case 1:
        tft.drawString("News", 203, 124);
        break;
      case 2:
        tft.drawString("Information", 203, 124);
        break;
      case 3:
        tft.drawString("Sports", 203, 124);
        break;
      case 4:
        tft.drawString("Talk", 203, 124);
        break;
      case 5:
        tft.drawString("Rock", 203, 124);
        break;
      case 6:
        tft.drawString("Classic Rock", 203, 124);
        break;
      case 7:
        tft.drawString("Adult Hits", 203, 124);
        break;
      case 8:
        tft.drawString("Soft Rock", 203, 124);
        break;
      case 9:
        tft.drawString("Top 40", 203, 124);
        break;
      case 10:
        tft.drawString("Country", 203, 124);
        break;
      case 11:
        tft.drawString("Oldies", 203, 124);
        break;
      case 12:
        tft.drawString("Soft", 203, 124);
        break;
      case 13:
        tft.drawString("Nostalgia", 203, 124);
        break;
      case 14:
        tft.drawString("Jazz", 203, 124);
        break;
      case 15:
        tft.drawString("Classical", 203, 124);
        break;
      case 16:
        tft.drawString("Rhythm and Blues", 203, 124);
        break;
      case 17:
        tft.drawString("Soft R & B", 203, 124);
        break;
      case 18:
        tft.drawString("Foreign Language", 203, 124);
        break;
      case 19:
        tft.drawString("Religious Music", 203, 124);
        break;
      case 20:
        tft.drawString("Religious Talk", 203, 124);
        break;
      case 21:
        tft.drawString("Personality", 203, 124);
        break;
      case 22:
        tft.drawString("Public", 203, 124);
        break;
      case 23:
        tft.drawString("College", 203, 124);
        break;
      case 24:
        tft.drawString("Hablar Espanol", 203, 124);
        break;
      case 25:
        tft.drawString("Musica Espanol", 203, 124);
        break;
      case 26:
        tft.drawString("Hip Hop", 203, 124);
        break;
      case 27:
        tft.drawString("#27", 203, 124);
        break;
      case 28:
        tft.drawString("#28", 203, 124);
        break;
      case 29:
        tft.drawString("Weather", 203, 124);
        break;
      case 30:
        tft.drawString("Emergency Test", 203, 124);
        break;
      case 31:
        tft.drawString("ALERT! ALERT!", 203, 124);
        break;
    }
  }  else {
    last_pty_2 = last_pty_1;
    last_pty_1 = pty;
  }
}
