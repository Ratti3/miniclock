/***********************************************************************

Mini Clock v1.0, Jul 2014 by Nick Hall
Distributed under the terms of the GPL.

For help on how to build the clock see my blog:
http://123led.wordpress.com/

Tested on IDE v1.6.5

=======================================================================

Modified by Ratti3 - 3 Jul 2019
Mini Clock v1.1
Tested on IDE v1.8.9

24,340 bytes 79%
1,020 bytes 49%

https://github.com/Ratti3/miniclock
https://youtu.be/CpQsMjI3FL0

***********************************************************************/

//include libraries:
#include "ProgmemData.h"                 // Progmem Storage File, holds day, month and time names, frees up precious RAM
#include <LedControl.h>                  // v1.0.6 https://github.com/wayoda/LedControl
#include <FontLEDClock.h>                // https://github.com/javastraat/arduino/blob/master/libraries/FontLEDClock/FontLEDClock.h - however, it has been modified
#include <Wire.h>                        // Standard Arduino library
#include <RTClib.h>                      // v1.2.0 DS3231 RTC - https://github.com/adafruit/RTClib
#include <Button.h>                      // https://github.com/tigoe/Button
#include <Adafruit_BME280.h>             // v1.0.8 BME280 Environmental Sensor -  https://github.com/adafruit/Adafruit_BME280_Library
#include <BH1750FVI.h>                   // v1.1.1 BH1750 Light Sensor - https://github.com/PeterEmbedded/BH1750FVI

// Setup LED Matrix
// pin 12 is connected to the DataIn on the display
// pin 11 is connected to the CLK on the display
// pin 10 is connected to LOAD on the display
LedControl lc = LedControl(12, 11, 10, 4); //sets the 3 pins as 12, 11 & 10 and then sets 4 displays (max is 8 displays)

//global variables (changeable defaults)
byte intensity = 2;                      // Default intensity/brightness (0-15), can be set via menu
byte clock_mode = 0;                     // Default clock mode. Default = 0 (basic_mode)
bool random_mode = 0;                    // Define random mode - changes the display type every few hours. Default = 0 (off)
bool random_font_mode = 0;               // Define font random mode - changes the font every few hours. 1 = random font on
bool ampm = 0;                           // Define 12 or 24 hour time. 0 = 24 hour. 1 = 12 hour
byte display_mode = 5;                   // Default display on/off mode, used by light sensor. 0 = normal, 1 = always on, 2 - always off, 3 - 5 = defined by hour_off_1,2,3
bool auto_intensity = true;              // Default auto light intensity setting
byte hour_off_1 = 21;                    // These three define the hour light sensor can turn off display if dark enough, format is 24 hours, the routine for
byte hour_off_2 = 22;                    // this checks between 8.00 and one of these values
byte hour_off_3 = 23;
// These are set via the setup Font menu, see set_font_case() routine for all default values:
byte font_style = 2;                     // Default clock large font style
byte font_offset = 1;                    // Default clock large font offset adjustment
byte font_cols = 6;                      // Default clock large font columns adjustment

//global variables
bool shut = false;                       // Stores matrix sleep state
byte old_mode = clock_mode;              // Stores the previous clock mode, so if we go to date or whatever, we know what mode to go back to after.
byte change_mode_time = 0;               // Holds hour when clock mode will next change if in random mode.
unsigned long delaytime = 500;           // We always wait a bit between updates of the display
int rtc[7];                              // Holds real time clock output
int light_count = 0;                     // Counter for light routine
byte auto_intensity_value = 0;           // Stores the last intensity value set by the light sensor, this value is set automatically
char words[9];                           // Holds word clock words, retrieved from progmem

char suffix[4][3] = {
  "st", "nd", "rd", "th"
};  //date suffix array, used in slide, basic_mode and jumble modes. e,g, 1st 2nd ...

//define constants
#define NUM_DISPLAY_MODES  3                    // Number display modes (conting zero as the first mode)
#define NUM_SETTINGS_MODES 7                    // Number settings modes = 7 (counting zero as the first mode)
#define NUM_FONTS          7                    // Number of fonts, as defined in FontLEDClock.h
#define SLIDE_DELAY        20                   // The time in milliseconds for the slide effect per character in slide mode. Make this higher for a slower effect
#define cls                clear_display        // Clear display

RTC_DS3231 ds3231;                              // Create RTC object
Adafruit_BME280 bme;                            // BME280 object (pins 4 and 5 and 3.3v)
BH1750FVI lux(BH1750FVI::k_DevModeContHighRes); // BH1750 object (pins 4 and 5 and 3.3v)

Button buttonA = Button(2, BUTTON_PULLUP);      // Menu button
Button buttonB = Button(3, BUTTON_PULLUP);      // Display date / + button
Button buttonC = Button(4, BUTTON_PULLUP);      // Temp/Humidity/Pressure / - button
Button buttonD = Button(5, BUTTON_PULLUP);      // Display options button

void setup() {

  digitalWrite(2, HIGH);                        // turn on pullup resistor for button on pin 2
  digitalWrite(3, HIGH);                        // turn on pullup resistor for button on pin 3
  digitalWrite(4, HIGH);                        // turn on pullup resistor for button on pin 4
  digitalWrite(5, HIGH);                        // turn on pullup resistor for button on pin 5
  
  Serial.begin(9600); //start serial

  //initialize the 4 matrix panels
  //we have already set the number of devices when we created the LedControl
  int devices = lc.getDeviceCount();
  //we have to init all devices in a loop
  for (int address = 0; address < devices; address++) {
    /*The MAX72XX is in power-saving mode on startup*/
    lc.shutdown(address, false);
    /* Set the brightness to a medium values */
    lc.setIntensity(address, intensity);
    /* and clear the display */
    lc.clearDisplay(address);
  }

  //I2C
  Wire.begin();
  
  //Setup DS3231 RTC
  ds3231.begin();
  //ds3231.adjust(DateTime(2019, 6, 29, 12, 59, 40));  // Set time manually
  //ds3231.adjust(DateTime(__DATE__, __TIME__)); // sets the RTC to the date & time this sketch was compiled
  if (!ds3231.begin()) {
    Serial.println("Couldn't find RTC");
    while(1);
  }

  //BME280 environmental sensor, this sensor from Ebay has address 0x76
  bme.begin(0x76);
  //Reduce BME sampling rate to prevent overheating
  bme.setSampling(Adafruit_BME280::MODE_FORCED,
    Adafruit_BME280::SAMPLING_X1, // temperature
    Adafruit_BME280::SAMPLING_X1, // pressure
    Adafruit_BME280::SAMPLING_X1, // humidity
    Adafruit_BME280::FILTER_OFF);

  //BH1750 light sensor
  lux.begin();

  //what is this silliness, needed for random() to work properly
  randomSeed(analogRead(A0));

  //Show software version & startup message
  printver();
  
}

void loop() {

  //run the clock with whatever mode is set by clock_mode - the default is set at top of code.
  switch(clock_mode) {
  case 0:
    basic_mode();
    break;
  case 1:
   small_mode();
    break;
  case 2:
    slide();
    break;
  case 3:
    word_clock();
    break;
  case 4:
    setup_menu();
    break;
  }

}


//plot a point on the display
void plot(byte x, byte y, byte val) {

  //select which matrix depending on the x coord
  byte address;
  if (x >= 0 && x <= 7)   {
    address = 3;
  }
  if (x >= 8 && x <= 15)  {
    address = 2;
    x = x - 8;
  }
  if (x >= 16 && x <= 23) {
    address = 1;
    x = x - 16;
  }
  if (x >= 24 && x <= 31) {
    address = 0;
    x = x - 24;
  }

  if (val == 1) {
    lc.setLed(address, y, x, true);
  } else {
    lc.setLed(address, y, x, false);
  }
}


//clear screen
void clear_display() {
  
  for (byte address = 0; address < 4; address++) {
    lc.clearDisplay(address);
  }

}


//fade screen down
void fade_down() {

  byte x = 0; //to hold temp intensity value
  if (auto_intensity) {
    x = auto_intensity_value;  //uses the last light sensor intensity settings, prevents display from constantly flicking between global and light sensor value
  }
  else {
    x = intensity;
  }

  //fade from global intensity to 1
  for (byte i = x; i > 0; i--) {
    for (byte address = 0; address < 4; address++) {
      lc.setIntensity(address, i);
    }
    delay(30); //change this to change fade down speed
  }

  clear_display(); //clear display completely (off)

  //reset intentsity to global val
  for (byte address = 0; address < 4; address++) {
    lc.setIntensity(address, x);
  }

}



//power up led test & display software version number
void printver() {

  byte i = 0;
  char ver_a[9] = "Vers 1.1";
  char ver_b[9] = " Ratti3 ";

  //test all leds.
  for (byte x = 0; x <= 31; x++) {
    for (byte y = 0; y <= 7; y++) {
      plot(x, y, 1);
    }
  }
  delay(500);
  fade_down();

  while (ver_a[i]) {
    puttinychar((i * 4), 1, ver_a[i]);
    delay(35);
    i++;
  }
  delay(700);
  fade_down();
  i = 0;
  while (ver_b[i]) {
    puttinychar((i * 4), 1, ver_b[i]);
    delay(35);
    i++;
  }
  delay(700);
  fade_down();
}


// puttinychar
// Copy a 3x5 character glyph from the myfont data structure to display memory, with its upper left at the given coordinate
// This is unoptimized and simply uses plot() to draw each dot.
void puttinychar(byte x, byte y, char c)
{
  byte dots;
  if (c >= 'A' && c <= 'Z' || (c >= 'a' && c <= 'z') ) {
    c &= 0x1F;   // A-Z maps to 1-26
  }
  else if (c >= '0' && c <= '9') {
    c = (c - '0') + 32;
  }
  else if (c == ' ') {
    c = 0; // space
  }
  else if (c == '.') {
    c = 27; // full stop
  }
  else if (c == ':') {
    c = 28; // colon
  }
  else if (c == '\'') {
    c = 29; // single quote mark
  }
  else if (c == '>') {
    c = 30; // >
  }
  else if (c == '/') {
    c = 31; // forward slash
  }

  for (byte col = 0; col < 3; col++) {
    dots = pgm_read_byte_near(&mytinyfont[c][col]);
    for (char row = 0; row < 5; row++) {
      if (dots & (16 >> row))
        plot(x + col, y + row, 1);
      else
        plot(x + col, y + row, 0);
    }
  }
}


// fs = font_style, fc = font_cols
void putnormalchar(byte x, byte y, char c, byte fs, byte fc)
{

  byte dots;
  if (c >= 'A' && c <= 'Z' ) {
    c = (c - 'A') + 27;   // A-Z maps to 27-52
  }
  else if (c >= 'a' && c <= 'z') {
    c &= 0x1F;   // a-z maps to 1-26
  }
  else if (c >= '0' && c <= '9') {
    switch(fs) {
      case 1:
        c = (c - '0') + 59;   // 0-9 maps to 59-68
        break;
      case 2:
        c = (c - '0') + 69;   // 0-9 maps to 69-78
        break;
      case 3:
        c = (c - '0') + 79;   // 0-9 maps to 79-88
        break;
      case 4:
        c = (c - '0') + 89;   // 0-9 maps to 89-98
        break;
      case 5:
        c = (c - '0') + 99;   // 0-9 maps to 99-108
        break;
      case 6:
        c = (c - '0') + 69;   // 0-9 maps to 69-78
        break;
      case 7:
        c = (c - '0') + 109;   // 0-9 maps to 109-108
        break;
    }
  }
  else if (c == ' ') {
    c = 0; // space
  }
  else if (c == '.') {
    c = 53; // full stop
  }
  else if (c == '\'') {
    c = 54; // single quote mark
  }
  else if (c == ':') {
    c = 55; // colon
  }
  else if (c == '>') {
    c = 56; // clock_mode selector arrow
  }
  else if (c == '~') {
    c = 57; // degrees
  }
  else if (c == '%') {
    c = 58; // percentage
  }
  else if (c >= -80 && c <= -67) {
    c *= -1;
  }

  for (char col = 0; col < fc; col++) {
    dots = pgm_read_byte_near(&myfont[c][col]);
    for (char row = 0; row < 7; row++) {
      //check coords are on screen before trying to plot
      //if ((x >= 0) && (x <= 31) && (y >= 0) && (y <= 7)){

      if (dots & (64 >> row)) {   // only 7 rows.
        plot(x + col, y + row, 1);
      } else {
        plot(x + col, y + row, 0);
      }
      //}
    }
  }
}


//small_mode
//show the time in small 3x5 characters with seconds display

void small_mode() {

  char textchar[8]; // the 16 characters on the display
  byte mins = 100; //mins
  byte secs = rtc[0]; //seconds
  byte old_secs = secs; //holds old seconds value - from last time seconds were updated o display - used to check if seconds have changed
  
  cls();

  //run clock main loop as long as run_mode returns true
  while (run_mode()) {

    //Check light levels for turning on/off matrix
    if (light_count > 100) {
      light();
      light_count = 0;
    }
    light_count++;

    get_time();
  
    //check for button press
    if (buttonA.uniquePress()) {
      switch_mode();
      return;
    }
    if (buttonB.uniquePress()) {
      display_date();
      return;
    }
    if (buttonC.uniquePress()) {
      display_thp();
      return;
    }
    
    //if secs changed then update them on the display
    secs = rtc[0];
    if (secs != old_secs) {

      //secs
      char buffer[3];
      itoa(secs, buffer, 10);

      //fix - as otherwise if num has leading zero, e.g. "03" secs, itoa coverts this to chars with space "3 ".
      if (secs < 10) {
        buffer[1] = buffer[0];
        buffer[0] = '0';
      }

      puttinychar( 20, 1, ':'); //seconds colon
      puttinychar( 24, 1, buffer[0]); //seconds
      puttinychar( 28, 1, buffer[1]); //seconds
      old_secs = secs;
    }

    //if minute changes change time
    if (mins != rtc[1]) {

      //reset these for comparison next time
      mins = rtc[1];
      byte hours = rtc[2];
      if (hours > 12) {
        hours = hours - ampm * 12;
      }
      if (hours < 1) {
        hours = hours + ampm * 12;
      }


      //byte dow  = rtc[3]; // the DS3231 outputs 0 - 6 where 0 = Sunday0 - 6 where 0 = Sunday.
      //byte date = rtc[4];

      //set characters
      char buffer[3];
      itoa(hours, buffer, 10);

      //fix - as otherwise if num has leading zero, e.g. "03" hours, itoa coverts this to chars with space "3 ".
      if (hours < 10) {
        buffer[1] = buffer[0];
        //if we are in 12 hour mode blank the leading zero.
        if (ampm) {
          buffer[0] = ' ';
        }
        else {
          buffer[0] = '0';
        }
      }
      //set hours chars
      textchar[0] = buffer[0];
      textchar[1] = buffer[1];
      textchar[2] = ':';

      itoa (mins, buffer, 10);
      if (mins < 10) {
        buffer[1] = buffer[0];
        buffer[0] = '0';
      }
      //set mins characters
      textchar[3] = buffer[0];
      textchar[4] = buffer[1];

      //do seconds
      textchar[5] = ':';
      buffer[3];
      secs = rtc[0];
      itoa(secs, buffer, 10);

      //fix - as otherwise if num has leading zero, e.g. "03" secs, itoa coverts this to chars with space "3 ".
      if (secs < 10) {
        buffer[1] = buffer[0];
        buffer[0] = '0';
      }
      //set seconds
      textchar[6] = buffer[0];
      textchar[7] = buffer[1];

      byte x = 0;
      byte y = 0;

      //print each char
      for (byte x = 0; x < 6 ; x++) {
        puttinychar( x * 4, 1, textchar[x]);
      }
    }
    delay(50);
  }
  fade_down();
}


// basic_mode()
// show the time in 5x7 characters
void basic_mode()
{
  cls();

  char buffer[3];   //for int to char conversion to turn rtc values into chars we can print on screen
  byte offset = 0;  //used to offset the x postition of the digits and centre the display when we are in 12 hour mode and the clock shows only 3 digits. e.g. 3:21
  byte x, y;        //used to draw a clear box over the left hand "1" of the display when we roll from 12:59 -> 1:00am in 12 hour mode.

  //do 12/24 hour conversion if ampm set to 1
  byte hours = rtc[2];

  if (hours > 12) {
    hours = hours - ampm * 12;
  }
  if (hours < 1) {
    hours = hours + ampm * 12;
  }

  //do offset conversion
  if (ampm && hours < 10) {
    offset = 2;
  }
  
  //set the next minute we show the date at
  //set_next_date();
  
  // initially set mins to value 100 - so it wll never equal rtc[1] on the first loop of the clock, meaning we draw the clock display when we enter the function
  byte secs = 100;
  byte mins = 100;
  int count = 0;

  //run clock main loop as long as run_mode returns true
  while (run_mode()) {

    //Check light levels for turning on/off matrix
    if (light_count > 4000) {
      light();
      light_count = 0;
    }
    light_count++;

    //get the time from the clock chip
    get_time();
    
    //check for button press
    if (buttonA.uniquePress()) {
      switch_mode();
      return;
    }
    if (buttonB.uniquePress()) {
      display_date();
      return;
    }
    if (buttonC.uniquePress()) {
      display_thp();
      return;
    }
    if (buttonD.uniquePress()) {
      display_options();
      return;
    }

    //check whether it's time to automatically display the date
    //check_show_date();

    //draw the flashing : as on if the secs have changed.
    if (secs != rtc[0]) {

      //update secs with new value
      secs = rtc[0];

      //draw :
      plot (15 - offset, 2, 1); //top point
      plot (15 - offset, 5, 1); //bottom point
      count = 400;
    }

    //if count has run out, turn off the :
    if (count == 0) {
      plot (15 - offset, 2, 0); //top point
      plot (15 - offset, 5, 0); //bottom point
    }
    else {
      count--;
    }

    //re draw the display if button pressed or if mins != rtc[1] i.e. if the time has changed from what we had stored in mins, (also trigggered on first entering function when mins is 100)
    if (mins != rtc[1]) {

      //update mins and hours with the new values
      mins = rtc[1];
      hours = rtc[2];

      //adjust hours of ampm set to 12 hour mode
      if (hours > 12) {
        hours = hours - ampm * 12;
      }
      if (hours < 1) {
        hours = hours + ampm * 12;
      }

      itoa(hours, buffer, 10);

      //if hours < 10 the num e.g. "3" hours, itoa coverts this to chars with space "3 " which we dont want
      if (hours < 10) {
        buffer[1] = buffer[0];
        buffer[0] = '0';
      }

      //print hours
      //if we in 12 hour mode and hours < 10, then don't print the leading zero, and set the offset so we centre the display with 3 digits.
      if (ampm && hours < 10) {
        offset = 2;

        //if the time is 1:00am clear the entire display as the offset changes at this time and we need to blank out the old 12:59
        if ((hours == 1 && mins == 0) ) {
          cls();
        }
      }
      else {
        //else no offset and print hours tens digit
        offset = 0;

        //if the time is 10:00am clear the entire display as the offset changes at this time and we need to blank out the old 9:59
        if (hours == 10 && mins == 0) {
          cls();
        }

        putnormalchar(1,  0, buffer[0], font_style, font_cols);
      }
      //print hours ones digit
      putnormalchar(7 - offset + font_offset, 0, buffer[1], font_style, font_cols);


      //print mins
      //add leading zero if mins < 10
      itoa (mins, buffer, 10);
      if (mins < 10) {
        buffer[1] = buffer[0];
        buffer[0] = '0';
      }
      //print mins tens and ones digits
      putnormalchar(19 - offset - font_offset - font_offset, 0, buffer[0], font_style, font_cols);
      putnormalchar(25 - offset - font_offset, 0, buffer[1], font_style, font_cols);
    }
  }
  fade_down();
}


//like basic_mode but with slide effect
void slide() {

  byte digits_old[4] = {99, 99, 99, 99}; //old values  we store time in. Set to somthing that will never match the time initially so all digits get drawn wnen the mode starts
  byte digits_new[4]; //new digits time will slide to reveal
  byte digits_x_pos[4] = {25, 19, 7, 1}; //x pos for which to draw each digit at

  char old_char[2]; //used when we use itoa to transpose the current digit (type byte) into a char to pass to the animation function
  char new_char[2]; //used when we use itoa to transpose the new digit (type byte) into a char to pass to the animation function

  //old_chars - stores the 5 day and date suffix chars on the display. e.g. "mon" and "st". We feed these into the slide animation as the current char when these chars are updated.
  //We sent them as A initially, which are used when the clocl enters the mode and no last chars are stored.
  //char old_chars[6] = "AAAAA";

  //plot the clock colon on the display
  cls();
  putnormalchar(13, 0, ':', font_style, font_cols);

  byte old_secs = rtc[0]; //store seconds in old_secs. We compare secs and old secs. WHen they are different we redraw the display

  //run clock main loop as long as run_mode returns true
  while (run_mode()) {

    //Check light levels for turning on/offf matrix
    if (light_count > 5000) {
      light();
      light_count = 0;
    }
    light_count++;

    get_time();
    
    //check for button press
    if (buttonA.uniquePress()) {
      switch_mode();
      return;
    }
      if (buttonB.uniquePress()) {
      display_date();
      return;
    }
    if (buttonC.uniquePress()) {
      display_thp();
      return;
    }
    if (buttonD.uniquePress()) {
      display_options();
      return;
    }

    //if secs have changed then update the display
    if (rtc[0] != old_secs) {
      old_secs = rtc[0];

      //do 12/24 hour conversion if ampm set to 1
      byte hours = rtc[2];
      if (hours > 12) {
        hours = hours - ampm * 12;
      }
      if (hours < 1) {
        hours = hours + ampm * 12;
      }

      //split all date and time into individual digits - stick in digits_new array

      //rtc[0] = secs                        //array pos and digit stored
      //digits_new[0] = (rtc[0]%10);           //0 - secs ones
      //digits_new[1] = ((rtc[0]/10)%10);      //1 - secs tens
      //rtc[1] = mins
      digits_new[0] = (rtc[1] % 10);         //2 - mins ones
      digits_new[1] = ((rtc[1] / 10) % 10);  //3 - mins tens
      //rtc[2] = hours
      digits_new[2] = (hours % 10);         //4 - hour ones
      digits_new[3] = ((hours / 10) % 10);  //5 - hour tens
      //rtc[4] = date
      //digits_new[6] = (rtc[4]%10);           //6 - date ones
      //digits_new[7] = ((rtc[4]/10)%10);      //7 - date tens

      //draw initial screen of all chars. After this we just draw the changes.

      //compare digits 0 to 3 (mins and hours)
      for (byte i = 0; i <= 3; i++) {
        //see if digit has changed...
        if (digits_old[i] != digits_new[i]) {

          //run 9 step animation sequence for each in turn
          for (byte seq = 0; seq <= 8 ; seq++) {

            //convert digit to string
            itoa(digits_old[i], old_char, 10);
            itoa(digits_new[i], new_char, 10);

            //if set to 12 hour mode and we're on digit 2 (hours tens mode) then check to see if this is a zero. If it is, blank it instead so we get 2.00pm not 02.00pm
            if (ampm && i == 3) {
              if (digits_new[3] == 0) {
                new_char[0] = ' ';
              }
              if (digits_old[3] == 0) {
                old_char[0] = ' ';
              }
            }
            //draw the animation frame for each digit
            slideanim(digits_x_pos[i], 0, seq, old_char[0], new_char[0]);
            delay(SLIDE_DELAY);
          }
        }
      }

      //save digita array tol old for comparison next loop
      for (byte i = 0; i <= 3; i++) {
        digits_old[i] =  digits_new[i];
      }
    }//secs/oldsecs
  }//while loop
  fade_down();
}


//called by slide
//this draws the animation of one char sliding on and the other sliding off. There are 8 steps in the animation, we call the function to draw one of the steps from 0-7
//inputs are are char x and y, animation frame sequence (0-7) and the current and new chars being drawn.
void slideanim(byte x, byte y, byte sequence, char current_c, char new_c) {

  //  To slide one char off and another on we need 9 steps or frames in sequence...

  //  seq# 0123456 <-rows of the display
  //   |   |||||||
  //  seq0 0123456  START - all rows of the display 0-6 show the current characters rows 0-6
  //  seq1  012345  current char moves down one row on the display. We only see it's rows 0-5. There are at display positions 1-6 There is a blank row inserted at the top
  //  seq2 6 01234  current char moves down 2 rows. we now only see rows 0-4 at display rows 2-6 on the display. Row 1 of the display is blank. Row 0 shows row 6 of the new char
  //  seq3 56 0123
  //  seq4 456 012  half old / half new char
  //  seq5 3456 01
  //  seq6 23456 0
  //  seq7 123456
  //  seq8 0123456  END - all rows show the new char

  //from above we can see...
  //currentchar runs 0-6 then 0-5 then 0-4 all the way to 0. starting Y position increases by 1 row each time.
  //new char runs 6 then 5-6 then 4-6 then 3-6. starting Y position increases by 1 row each time.

  //if sequence number is below 7, we need to draw the current char
  if (sequence < 7) {
    byte dots;
    if (current_c >= 'A' && current_c <= 'Z' ) {
      current_c = (current_c - 'A') + 27;   // A-Z maps to 27-52
    }
    else if (current_c >= 'a' && current_c <= 'z') {
      current_c &= 0x1F;   // a-z maps to 1-26
    }
    else if (current_c >= '0' && current_c <= '9') {
      current_c = (current_c - '0') + 59;   // 0-9 maps to 59-68
    }
    else if (current_c == ' ') {
      current_c = 0; // space
    }
    else if (current_c == '.') {
      current_c = 53; // full stop
    }
    else if (current_c == '\'') {
      current_c = 54; // single quote mark
    }
    else if (current_c == ':') {
      current_c = 55; //colon
    }
    else if (current_c == '>') {
      current_c = 56; // >
    }

    byte curr_char_row_max = 7 - sequence; //the maximum number of rows to draw is 6 - sequence number
    byte start_y = sequence; //y position to start at - is same as sequence number. We inc this each loop

    //plot each row up to row maximum (calculated from sequence number)
    for (byte curr_char_row = 0; curr_char_row <= curr_char_row_max; curr_char_row++) {
      for (byte col = 0; col < 5; col++) {
        dots = pgm_read_byte_near(&myfont[current_c][col]);
        if (dots & (64 >> curr_char_row))
          plot(x + col, y + start_y, 1); //plot led on
        else
          plot(x + col, y + start_y, 0); //else plot led off
      }
      start_y++;//add one to y so we draw next row one down
    }
  }

  //draw a blank line between the characters if sequence is between 1 and 7. If we don't do this we get the remnants of the current chars last position left on the display
  if (sequence >= 1 && sequence <= 8) {
    for (byte col = 0; col < 5; col++) {
      plot(x + col, y + (sequence - 1), 0); //the y position to draw the line is equivalent to the sequence number - 1
    }
  }



  //if sequence is above 2, we also need to start drawing the new char
  if (sequence >= 2) {

    //work out char
    byte dots;
    if (new_c >= 'A' && new_c <= 'Z' ) {
      new_c = (new_c - 'A') + 27;   // A-Z maps to 27-52
    }
    else if (new_c >= 'a' && new_c <= 'z') {
      new_c &= 0x1F;   // a-z maps to 1-26
    }
    else if (new_c >= '0' && new_c <= '9') {
      new_c = (new_c - '0') + 59;   // 0-9 maps to 59-68
    }
    else if (new_c == ' ') {
      new_c = 0; // space
    }
    else if (new_c == '.') {
      new_c = 27; // full stop
    }
    else if (new_c == '\'') {
      new_c = 28; // single quote mark
    }
    else if (new_c == ':') {
      new_c = 29; // colon
    }
    else if (new_c == '>') {
      new_c = 30; // >
    }

    byte newcharrowmin = 6 - (sequence - 2); //minimumm row num to draw for new char - this generates an output of 6 to 0 when fed sequence numbers 2-8. This is the minimum row to draw for the new char
    byte start_y = 0; //y position to start at - is same as sequence number. we inc it each row

    //plot each row up from row minimum (calculated by sequence number) up to 6
    for (byte newcharrow = newcharrowmin; newcharrow <= 6; newcharrow++) {
      for (byte col = 0; col < 5; col++) {
        dots = pgm_read_byte_near(&myfont[new_c][col]);
        if (dots & (64 >> newcharrow))
          plot(x + col, y + start_y, 1); //plot led on
        else
          plot(x + col, y + start_y, 0); //else plot led off
      }
      start_y++;//add one to y so we draw next row one down
    }
  }
}



//print a clock using words rather than numbers
void word_clock() {

  cls();
  
  //potentially 3 lines to display
  char str_a[8];
  char str_b[9];
  char str_c[8];

  //byte hours_y, mins_y; //hours and mins and positions for hours and mins lines

  byte hours = rtc[2];
  if (hours > 12) {
    hours = hours - ampm * 12;
  }
  if (hours < 1) {
    hours = hours + ampm * 12;
  }

  get_time(); //get the time from the clock chip
  byte old_mins = 100; //store mins in old_mins. We compare mins and old mins & when they are different we redraw the display. Set this to 100 initially so display is drawn when mode starts.
  byte mins;

  //run clock main loop as long as run_mode returns true
  while (run_mode()) {
    
    //check for button press
    if (buttonA.uniquePress()) {
      switch_mode();
      return;
    }
    if (buttonB.uniquePress()) {
      display_date();
    }
    if (buttonC.uniquePress()) {
      display_thp();
      return;
    }
    if (buttonD.uniquePress()) {
      display_options();
      return;
    }

    get_time(); //get the time from the clock chip
    mins = rtc[1];  //get mins


    //if mins is different from old_mins - redraw display
    if (mins != old_mins) {

      //update old_mins with current mins value
      old_mins = mins;

      //reset these for comparison next time
      mins = rtc[1];
      hours = rtc[2];

      //make hours into 12 hour format
      if (hours > 12) {
        hours = hours - 12;
      }
      if (hours == 0) {
        hours = 12;
      }

      //split mins value up into two separate digits
      int minsdigit = rtc[1] % 10;
      byte minsdigitten = (rtc[1] / 10) % 10;

      char past[5] = "PAST";
      char to[3] = "TO";
      char half[5] = "HALF";
      char quar[8] = "QUARTER";
      char oclk[8] = "O'CLOCK";

      //if both mins are zero, i.e. it is on the hour, the top line reads "hours" and bottom line reads "o'clock"
      if (minsdigitten == 0 && minsdigit == 0  ) {
        progmem_numbers(0, hours - 1);
        strcpy (str_a, words);
        strcpy (str_b, oclk);
        strcpy (str_c, "");
      }
      //if mins <= 10 , then top line has to read "minsdigti past" and bottom line reads hours
      else if (mins < 10) {
        progmem_numbers(0, minsdigit - 1);
        strcpy (str_a, words);
        strcpy (str_b, past);
        progmem_numbers(0, hours - 1);
        strcpy (str_c, words);
      }
      //if mins = 10, cant use minsdigit as above, so soecial case to print 10 past /n hour.
      if (mins == 10) {
        progmem_numbers(0, 9);
        strcpy (str_a, words);
        strcpy (str_b, past);
        progmem_numbers(0, hours - 1);
        strcpy (str_c, words);
      }
      else if (mins == 15) {
        strcpy (str_a, quar);
        strcpy (str_b, past);
        progmem_numbers(0, hours - 1);
        strcpy (str_c, words);
      }
      else if (mins == 20) {
        progmem_numbers(1, minsdigitten - 1);
        strcpy (str_a, words);
        strcpy (str_b, past);
        progmem_numbers(0, hours - 1);
        strcpy (str_c, words);
      }
      else if (mins == 30) {
        strcpy (str_a, half);
        strcpy (str_b, past);
        progmem_numbers(0, hours - 1);
        strcpy (str_c, words);
      }
      else if (mins == 40) {
        progmem_numbers(1, 1);
        strcpy (str_a, words);
        strcpy (str_b, to);
        if (hours == 12) {
          progmem_numbers(0, hours - hours);
        }
        else {
          progmem_numbers(0, hours);

        }
        strcpy (str_c, words);
      }
      else if (mins == 50) {
        progmem_numbers(0, 9);
        strcpy (str_a, words);
        strcpy (str_b, to);
        if (hours == 12) {
          progmem_numbers(0, hours - hours);
        }
        else {
          progmem_numbers(0, hours);

        }
        strcpy (str_c, words);
      }
      else if (mins == 45) {
        strcpy (str_a, quar);
        strcpy (str_b, to);
        if (hours == 12) {
          progmem_numbers(0, hours - hours);
        }
        else {
          progmem_numbers(0, hours);

        }
        strcpy (str_c, words);
      }

      //if time is not on the hour - i.e. both mins digits are not zero,
      //then make first line read "hours" and 2 & 3rd lines read "minstens"  "mins" e.g. "three /n twenty /n one"

      else if (minsdigitten != 0 && minsdigit != 0) {
        progmem_numbers(0, hours - 1);
        strcpy (str_a, words);

        //if mins is in the teens, use teens from the numbers array for the 2nd line, e.g. "fifteen"
        if (mins >= 11 && mins <= 19) {
          progmem_numbers(0, mins - 1);
          strcpy (str_a, words);
          strcpy (str_b, past);
          progmem_numbers(0, hours - 1);
          strcpy (str_c, words);
        }
        else if (mins > 50) {
          progmem_numbers(0, 60 - (mins + 1));
          strcpy (str_a, words);
          strcpy (str_b, to);
          if (hours == 12) {
            progmem_numbers(0, hours - 12);
            strcpy (str_c, words);
          }
          else {
            progmem_numbers(0, hours);
            strcpy (str_c, words);
          }
        }
        else {
          progmem_numbers(1, minsdigitten - 1);
          strcpy (str_b, words);
          progmem_numbers(0, minsdigit - 1);
          strcpy (str_c, words);
        }
      }
    }//end working out time

    //run in a loop
    //print line a "twelve"
    byte len = 0;
    while (str_a[len]) {
      len++;
    }; //get length of message
    byte offset_top = (31 - ((len - 1) * 4)) / 2; //

    //plot hours line
    byte i = 0;
    while (str_a[i]) {
      puttinychar((i * 4) + offset_top, 1, str_a[i]);
      i++;
    }
    
    //hold display but check for button presses
    int counter = 1000;
    while (counter > 0) {
      //check for button press
      if (buttonA.uniquePress()) {
        switch_mode();
        return;
      }
      if (buttonB.uniquePress()) {
        display_date();
      }
      if (buttonC.uniquePress()) {
        display_thp();
        return;
      }
      if (buttonD.uniquePress()) {
        display_options();
        return;
      }
    delay(1);
    counter--;    
    }
    fade_down();

    //print line b
    len = 0;
    while (str_b[len]) {
      len++;
    }; //get length of message
    offset_top = (31 - ((len - 1) * 4)) / 2; 

    i = 0;
    while (str_b[i]) {
      puttinychar((i * 4) + offset_top, 1, str_b[i]);
      i++;
    }

    //hold display but check for button presses
    counter = 1000;
    while (counter > 0){
      if (buttonA.uniquePress()) {
        switch_mode();
        return;
      }
      if (buttonB.uniquePress()) {
        display_date();
      }
      if (buttonC.uniquePress()) {
        display_thp();
        return;
      }
      if (buttonD.uniquePress()) {
        display_options();
        return;
      }
      delay(1);
      counter--;
    }
    fade_down();

    //print line c if there.
    len = 0;
    while (str_c[len]) {
      len++;
    }; //get length of message
    offset_top = (31 - ((len - 1) * 4)) / 2; 

    i = 0;
    while (str_c[i]) {
      puttinychar((i * 4) + offset_top, 1, str_c[i]);
      i++;
    }
    counter = 1000;
    while (counter > 0){
      //check for button press
      if (buttonA.uniquePress()) {
        switch_mode();
        return;
      }
      if (buttonB.uniquePress()) {
        display_date();
      }
      if (buttonC.uniquePress()) {
        display_thp();
        return;
      }
      if (buttonD.uniquePress()) {
        display_options();
        return;
      }
      delay(1);
      counter--;
    }
    fade_down();

    //hold display blank but check for button presses before starting again.
    counter = 1000;
    while (counter > 0){
       //check for button press
      if (buttonA.uniquePress()) {
        switch_mode();
        return;
      }
      if (buttonB.uniquePress()) {
        display_date();
      }
      if (buttonC.uniquePress()) {
        display_thp();
        return;
      }
      if (buttonD.uniquePress()) {
        display_options();
        return;
      }
      delay(1);
      counter--;

      //Check light levels for turning on/off matrix
      if (light_count > 2000) {
        light();
        light_count = 0;
      }
      light_count++;
    }
  }
  fade_down();
}


//used by word mode to retrieve words from progmem, m : 0 = numbers, 1 = numberstens. i = index
char progmem_numbers(byte m, byte i) {

  if (m == 0) {
    strcpy_P(words, (char *)pgm_read_word(&(numbers[i])));
  }
  else if (m == 1) {
    strcpy_P(words, (char *)pgm_read_word(&(numberstens[i])));
  }

}


//display_thp - print temperature, humidity and pressue
void display_thp()
{

  cls();
  bme.takeForcedMeasurement();
  char temp[5];
  char humi[3];
  char pres[5];
  dtostrf(bme.readTemperature(), 4, 1, temp);
  dtostrf(bme.readHumidity(), 3, 0, humi);
  dtostrf(bme.readPressure() / 100.0F, 4, 0, pres);

  byte i = 0;
  byte offset = 0;
  while (temp[i]) {
    if (i == 0) {
      offset = 2;
    }
    else if (i == 2) {
      offset = 4;
    }
    else if (i == 3) {
      offset = 1;
    }
    else {
      offset = 3;
    }
    putnormalchar(i * 6 + offset + font_offset, 0, temp[i], font_style, font_cols);
    i++;
  }
  putnormalchar(i * 6 + 2 + font_offset, 0, '~', font_style, font_cols);  // '~' translates to degress symbol via FontLEDClock.h

  delay(2000);
  fade_down();

  i = 0;
  while (humi[i]) {
    if (i < 2) {
      offset = 0;
    }
    else {
      offset = 1;
    }
    putnormalchar(i * 6 + offset + font_offset, 0, humi[i], font_style, font_cols);
    i++;
  }
  putnormalchar(i * 6 + 2 + font_offset, 0, '%', font_style, font_cols);

  delay(2000);
  fade_down();

  i = 0;
  while (pres[i]) {
    if (strlen(pres) < 4 && i == 0) {
      offset = 5;
    }
    else {
      offset = 0;
    }
    putnormalchar(i * 6 - offset, 0, pres[i], 1, 5);
    i++;
  }
  byte x = 0;
  char mb[3] = "mb";
  while (mb[x]) {
    puttinychar(i * 4 + 8, 1, mb[x]);
    x++;i++;
  }
  
  delay(2000);
  fade_down();
  cls();
    
}

//display_date - print the day of week, date and month with a flashing cursor effect
void display_date()
{

  cls();
  //read the date from the DS3231

  byte dow = rtc[3]; // day of week 0 = Sunday
  byte date = rtc[4];
  byte month = rtc[5] - 1;

  //print the day name
  
  //get length of text in pixels, that way we can centre it on the display by divindin the remaining pixels b2 and using that as an offset
  byte len = 0;
  char dayfullname[9];
  strcpy_P(dayfullname, (char *)pgm_read_word(&(daysfull[dow])));
  while(dayfullname[len]) {
    len++; 
  };
  byte offset = (31 - ((len - 1) * 4)) / 2; //our offset to centre up the text
      
  //print the name     
  int i = 0;
  while(dayfullname[i])
  {
    puttinychar((i * 4) + offset , 1, dayfullname[i]);
    i++;
  }
  delay(1000);
  fade_down();
  cls();
  
  // print date numerals
  char buffer[3];
  itoa(date, buffer, 10);
  offset = 10; //offset to centre text if 3 chars - e.g. 3rd
  
  // first work out date 2 letter suffix - eg st, nd, rd, th etc
  // char suffix[4][3]={"st", "nd", "rd", "th"  }; is defined at top of code
  byte s = 3; 
  if(date == 1 || date == 21 || date == 31) {
    s = 0;
  } 
  else if (date == 2 || date == 22) {
    s = 1;
  } 
  else if (date == 3 || date == 23) {
    s = 2;
  } 

  //print the 1st date number
  puttinychar(0 + offset, 1, buffer[0]);

  //if date is under 10 - then we only have 1 digit so set positions of sufix etc one character nearer
  byte suffixposx = 4;

  //if date over 9 then print second number and set xpos of suffix to be 1 char further away
  if (date > 9){
    suffixposx = 10;
    puttinychar(4 + offset, 1, buffer[1]);
    offset = 8; //offset to centre text if 4 chars
  }

  //print the 2 suffix characters
  puttinychar(suffixposx + offset, 1, suffix[s][0]); 
  puttinychar(suffixposx + 4 + offset, 1, suffix[s][1]); 
 
  delay(1000);
  fade_down();
  
  //print the month name 
  
  //get length of text in pixels, that way we can centre it on the display by divindin the remaining pixels b2 and using that as an offset
  len = 0;
  char monthfullname[9];
  strcpy_P(monthfullname, (char *)pgm_read_word(&(monthsfull[month])));
  while(monthfullname[len]) {
    len++;
  };
  offset = (31 - ((len - 1) * 4)) / 2; //our offset to centre up the text
  i = 0;
  while(monthfullname[i])
  {
    puttinychar((i * 4) + offset, 1, monthfullname[i]);
    i++;
  }
  
  delay(1000);
  fade_down();
}


//dislpay menu to change the clock mode
void switch_mode() {

  //remember mode we are in. We use this value if we go into settings mode, so we can change back from settings mode (6) to whatever mode we were in.
  old_mode = clock_mode;

  const char* modes[] = {
    ">Basic", ">Small", ">Slide", ">Words", ">Setup"
  };

  byte next_clock_mode;
  byte firstrun = 1;

  //loop waiting for button (timeout after 35 loops to return to mode X)
  for (int count = 0; count < 35 ; count++) {

    //if user hits button, change the clock_mode
    if (buttonA.uniquePress() || firstrun == 1) {

      count = 0;
      cls();

      if (firstrun == 0) {
        clock_mode++;
      }
      if (clock_mode > NUM_DISPLAY_MODES + 1) {
        clock_mode = 0;
      }

      //print arrown and current clock_mode name on line one and print next clock_mode name on line two
      char str_top[9];

      strcpy (str_top, modes[clock_mode]);

      next_clock_mode = clock_mode + 1;
      if (next_clock_mode >  NUM_DISPLAY_MODES + 1) {
        next_clock_mode = 0;
      }

      byte i = 0;
      while (str_top[i]) {
        puttinychar(i * 4 + 1, 1, str_top[i]);
        i++;
      }
      firstrun = 0;
    }
    delay(50);
  }
}


//run clock main loop as long as run_mode returns true
byte run_mode() {

  //if random mode is on... check the hour when we change mode.
  if (random_mode || random_font_mode) {
    //if hour value in change mode time = hours. then return false = i.e. exit mode.
    if (change_mode_time == rtc[2]) {
      //set the next random clock mode and time to change it
      set_next_random();
      //exit the current mode.
      return 0;
    }
  }
  //else return 1 - keep running in this mode
  return 1;
}


//set the next hour the clock will change mode when random mode is on, also does the random font mode
void set_next_random() {

  //set the next hour the clock mode will change - current time plus 1// - 4 hours
  get_time();
  change_mode_time = rtc[2] + 1;//random(1, 5);

  //if change_mode_time now happens to be over 23, then set it to between 1 and 3am
  if (change_mode_time > 23) {
    change_mode_time = 0;//random(1, 4);
  }

  if (random_mode) {
    //set the new clock mode
    clock_mode = random(0, NUM_DISPLAY_MODES + 1);  //pick new random clock mode
  }
  if (random_font_mode) {
    //set new random font
    set_font_case(random(1, NUM_FONTS + 1));  //pick new random font mode
  }

}



//dislpay menu to change the clock settings
void setup_menu() {

  const char* set_modes[] = {
    ">Rnd Clk", ">Rnd Fnt", ">24 Hr", ">Font", ">D/Time", ">Auto LX", ">Bright", ">Exit"}; 
  if (ampm == 0) { 
    set_modes[2] = (">12 Hr"); 
  }

  byte setting_mode = 0;
  byte next_setting_mode;
  byte firstrun = 1;

  //loop waiting for button (timeout after 35 loops to return to mode X)
  for(int count=0; count < 35 ; count++) {

    //if user hits button, change the clock_mode
    if(buttonA.uniquePress() || firstrun == 1){

      count = 0;
      cls();

      if (firstrun == 0) { 
        setting_mode++; 
      } 
      if (setting_mode > NUM_SETTINGS_MODES) { 
        setting_mode = 0; 
      }

      //print arrow and current clock_mode name on line one and print next clock_mode name on line two
      char str_top[9];
    
      strcpy (str_top, set_modes[setting_mode]);

      next_setting_mode = setting_mode + 1;
      if (next_setting_mode > NUM_SETTINGS_MODES) { 
        next_setting_mode = 0; 
      }
      
      byte i = 0;
      while(str_top[i]) {
        puttinychar(i * 4 + 1, 1, str_top[i]);
        i++;
      }

      firstrun = 0;
    }
    delay(50); 
  }
  
  //pick the mode 
  switch(setting_mode){
    case 0: 
      set_random(); 
      break;
    case 1: 
      set_random_font(); 
      break;
    case 2: 
      set_ampm(); 
      break;
    case 3: 
      set_font();
      break;
    case 4:
      set_time(); 
      break;
    case 5:
      set_auto_intensity(); 
      break;
    case 6:
      set_intensity(); 
      break;
    case 7: 
      //exit menu
      break;
  }
    
  //change the clock from mode 6 (settings) back to the one it was in before 
  clock_mode = old_mode;
}


//toggle random mode - pick a different clock mode every few hours
void set_random() {
  
  cls();

  char text_a[4] = "Off";
  char text_b[3] = "On";
  byte i = 0;

  //if random mode is on, turn it off
  if (random_mode) {

    //turn random mode off
    random_mode = 0;

    //print a message on the display
    while(text_a[i]) {
      puttinychar(i * 4, 1, text_a[i]);
      i++;
    }
  } else {
    //turn randome mode on. 
    random_mode = 1;
    
    //set hour mode will change
    set_next_random();
  
    //print a message on the display
    while(text_b[i]) {
      puttinychar(i * 4, 1, text_b[i]);
      i++;
    }  
  } 
  delay(1500); //leave the message up for a second or so
  
}


//toggle random font
void set_random_font() {
  
  cls();
  
  char text_a[4] = "Off";
  char text_b[3] = "On";
  byte i = 0;
  //if random font mode is on, turn it off
  if (random_font_mode) {

    //turn random font mode off
    random_font_mode = 0;

    //print a message on the display
    while(text_a[i]) {
      puttinychar(i * 4, 1, text_a[i]);
      i++;
    }
  } else {
    //turn random font mode on. 
    random_font_mode = 1;
    
    //set hour mode will change
    set_next_random();
  
    //print a message on the display
    while(text_b[i]) {
      puttinychar(i * 4, 1, text_b[i]);
      i++;
    }  
  } 
  delay(1500);
  
}


//set 12 or 24 hour clock
void set_ampm() {

  // AM/PM or 24 hour clock mode - flip the bit (makes 0 into 1, or 1 into 0 for ampm mode)
  ampm = (ampm ^ 1);
  cls();
}


//set font style
void set_font() {

  cls();

  byte i = 0;
  char text[10] = ">Set Fnt";
  while(text[i]) {
    puttinychar(i * 4, 1, text[i]);
    i++;
  }
  
  delay(1500);
  cls();

  byte set_font_value;
  if (font_style == 2 && font_cols == 5) {
    set_font_value = 6;
  }
  else {
    set_font_value = font_style;
  }

  get_font_value(set_font_value, 1, NUM_FONTS);
  //set_font_value = get_font_value(set_font_value, 1, NUM_FONTS);

  //set the font
  //set_font_case(set_font_value);

}


//set font_style, font_offset & font_cols variables, used by set_font()
int set_font_case(int value) {
  
  switch(value) {
    case 1:
      font_style = 1;
      font_offset = 0;
      font_cols = 5;
      break;
    case 2:
      font_style = 2;
      font_offset = 1;
      font_cols = 6;
      break;
    case 3:
      font_style = 3;
      font_offset = 1;
      font_cols = 6;
      break;
    case 4:
      font_style = 4;
      font_offset = 1;
      font_cols = 6;
      break;
    case 5:
      font_style = 5;
      font_offset = 0;
      font_cols = 5;
      break;
    case 6:
      font_style = 2;
      font_offset = 0;
      font_cols = 5;  //cheap way to create a new font (crop 1 column right side of font 2)
      break;
    case 7:
      font_style = 7;
      font_offset = 1;
      font_cols = 6;
      break;
  }
  //return value;
}

//get user values for setting font
int get_font_value(int current_value, int min_value, int max_value) {
  
  //print digits bottom line
  char buffer[2] = " ";
  itoa(current_value, buffer ,10);
  puttinychar(0, 1, '>'); 
  puttinychar(4, 1, buffer[0]); 
  delay(300);
  //wait for button input
  while (!buttonA.uniquePress()) {
    char preview[4] = "   ";
    //font preview numbers
    itoa(123, preview, 10);
    while (buttonB.isPressed()){

      if(current_value < max_value) { 
        current_value++;
      } 
      else {
        current_value = min_value;
      }
      //print the new value
      cls();
      itoa(current_value, buffer, 10);
      puttinychar(0, 1, '>'); 
      puttinychar(4, 1, buffer[0]);
      //preview the font and set the font
      set_font_case(current_value);
      byte i = 0;
      while(preview[i]) {
        putnormalchar(i * (font_cols + 1) + 10, 0, preview[i], font_style, font_cols);
        i++;
      }
      delay(150);
    }

    while (buttonC.isPressed()){

      if(current_value > min_value) {
        current_value--;
      } 
      else {
        current_value = max_value;
      }
      //print the new value
      cls();
      itoa(current_value, buffer ,10);
      puttinychar(0, 1, '>');
      puttinychar(4, 1, buffer[0]);
      //preview the font and set the font
      set_font_case(current_value);
      byte i = 0;
      while(preview[i]) {
        putnormalchar(i * (font_cols + 1) + 10, 0, preview[i], font_style, font_cols);
        i++;
      }
      delay(150);
    }

  }
  //return current_value;
}


//change screen intensity
void set_intensity() {

  cls();
  
  byte i = 0;
  char text[8] = ">Bright";
  while(text[i]) {
    puttinychar((i * 4) + 3, 0, text[i]);
    i++;
  }

  //wait for button input
  while (!buttonA.uniquePress()) {

    levelbar (0, 6,(intensity * 2) + 2, 2);    //display the intensity level as a bar
    while (buttonB.isPressed()) {
      if(intensity == 15) { 
        intensity = 0;
        cls ();
      }
      else {
        intensity++;
      }
      //print the new value 
      i = 0;
      while(text[i]) {
        puttinychar((i * 4) + 4, 0, text[i]);
        i++;
      }
      
      //display the intensity level as a bar
      levelbar (0, 6, (intensity * 2) + 2, 2);    
      
      //change the brightness setting on the displays
      for (byte address = 0; address < 4; address++) {
        lc.setIntensity(address, intensity);
      }
      delay(150);
    }

    while (buttonC.isPressed()) {
      if(intensity == 0) { 
        intensity = 15;
      }
      else {
        intensity--;
      }
      //display the intensity level as a bar
      cls ();
      levelbar (0, 6,(intensity * 2) + 2, 2);
      
      //print the new value 
      i = 0;
      while(text[i]) {
        puttinychar((i * 4) + 4, 0, text[i]);
        i++;
      }
      
      //change the brightness setting on the displays
      for (byte address = 0; address < 4; address++) {
        lc.setIntensity(address, intensity);
      }
      delay(150);
    }

  }
}


void set_auto_intensity() {

  cls();
  
  char text_a[4] = "Off";
  char text_b[3] = "On";
  byte i = 0;
  //if auto intensity is on, turn it off
  if (auto_intensity) {

    //turn auto intensity off
    auto_intensity = false;
    //revert to default intensity level
    set_devices(true, intensity);

    //print a message on the display
    while(text_a[i]) {
      puttinychar(i * 4, 1, text_a[i]);
      i++;
    }
  } else {
    //turn auto intensity on. 
    auto_intensity = true;
  
    //print a message on the display
    while(text_b[i]) {
      puttinychar(i * 4, 1, text_b[i]);
      i++;
    }  
  } 
  delay(1500);

}


// display a horizontal bar on the screen at offset xposr by ypos with height and width of xbar, ybar
void levelbar (byte xpos, byte ypos, byte xbar, byte ybar) {
  for (byte x = 0; x < xbar; x++) {
    for (byte y = 0; y <= ybar; y++) {
      plot(x + xpos, y + ypos, 1);
    }
  }
}


//set time and date routine
void set_time() {

  cls();

  //fill settings with current clock values read from clock
  get_time();
  byte set_min   = rtc[1];
  byte set_hr    = rtc[2];
  byte set_date  = rtc[4];
  byte set_mnth  = rtc[5];
  int  set_yr    = rtc[6]; 

  //Set function - we pass in: which 'set' message to show at top, current value, reset value, and rollover limit.
  set_date = set_value(2, set_date, 1, 31);
  set_mnth = set_value(3, set_mnth, 1, 12);
  set_yr   = set_value(4, set_yr, 2019, 2099);
  set_hr   = set_value(1, set_hr, 0, 23);
  set_min  = set_value(0, set_min, 0, 59);

  ds3231.adjust(DateTime(set_yr, set_mnth, set_date, set_hr, set_min));
  
  cls();
}


//used to set min, hr, date, month, year values. pass 
//message = which 'set' message to print, 
//current value = current value of property we are setting
//reset_value = what to reset value to if to rolls over. E.g. mins roll from 60 to 0, months from 12 to 1
//rollover limit = when value rolls over
int set_value(byte message, int current_value, int reset_value, int rollover_limit){

  cls();
  char messages[6][17]   = {
    ">Set Min", ">Set Hr", ">Set Day", ">Set Mth", ">Set Yr"};

  //Print "set xyz" top line
  byte i = 0;
  while(messages[message][i])
  {
    puttinychar(i * 4, 1, messages[message][i]); 
    i++;
  }

  delay(1500);
  cls();

  //print digits bottom line
  char buffer[5] = "    ";
  itoa(current_value,buffer,10);
  puttinychar(0 , 1, '>');
  puttinychar(4 , 1, buffer[0]);
  puttinychar(8 , 1, buffer[1]);
  puttinychar(12, 1, buffer[2]);
  puttinychar(16, 1, buffer[3]);
  delay(300);
  //wait for button input
  while (!buttonA.uniquePress()) {

    while (buttonB.isPressed()) {

      if(current_value < rollover_limit) {
        current_value++;
      }
      else {
        current_value = reset_value;
      }
      //print the new value
      itoa(current_value, buffer ,10);
      puttinychar(0 , 1, '>');
      puttinychar(4 , 1, buffer[0]);
      puttinychar(8 , 1, buffer[1]);
      puttinychar(12, 1, buffer[2]);
      puttinychar(16, 1, buffer[3]);
      delay(150);
    }

    while (buttonC.isPressed()) {

      if(current_value > reset_value) {
        current_value--;
      }
      else {
        current_value = rollover_limit;
      }
      //print the new value
      itoa(current_value, buffer, 10);
      puttinychar(0 , 1, '>');
      puttinychar(4 , 1, buffer[0]);
      puttinychar(8 , 1, buffer[1]);
      puttinychar(12, 1, buffer[2]);
      puttinychar(16, 1, buffer[3]);
      delay(150);
    }
    
  }
  return current_value;
}


void get_time()
{
  //get time
  DateTime now = ds3231.now();
  //save time to array
  rtc[6] = now.year();
  rtc[5] = now.month();
  rtc[4] = now.day();
  rtc[3] = now.dayOfTheWeek(); //returns 0-6 where 0 = Sunday
  rtc[2] = now.hour();
  rtc[1] = now.minute();
  rtc[0] = now.second();

  //print the time to the serial port - useful for debuging RTC issues
  /*
  Serial.print(rtc[2]);
  Serial.print(":");
  Serial.print(rtc[1]);
  Serial.print(":");
  Serial.println(rtc[0]);
  */
}


//Routine to check light level and turn on/off matrix
void light()
{
  //Get light reading
  uint16_t lx = lux.GetLightIntensity();

  //checks if display can be turned off if option to keep it on until a certain time is met
  bool dont_turn_off = false;
  if (display_mode > 2) {
    byte hr = rtc[2];
    switch(display_mode) {
      case 3:
      if (hr > 7 && hr < hour_off_1) {
        dont_turn_off = true;
      }
      else {
        dont_turn_off = false;
      }
      break;
      case 4:
      if (hr > 7 && hr < hour_off_2) {
        dont_turn_off = true;
      }
      else {
        dont_turn_off = false;
      }
      break;
      case 5:
      if (hr > 7 && hr < hour_off_3) {
        dont_turn_off = true;
      }
      else {
        dont_turn_off = false;
      }
      break;
    }
  }

  if (display_mode == 2) {
    shut = true;
    set_devices(false, 0); //Call sleep routine to turn off matrix, applies only when 4th button is used to turn it always off
  }
  else if (lx == 0 && !shut && !dont_turn_off && (display_mode == 0 || display_mode > 2)) {
    shut = true;
    set_devices(false, 0); //Call sleep routine to turn off matrix, applies when light is low enough and 4th button option is normal
  }
  else if (lx > 0 && shut && display_mode != 2) {
    shut = false;
    set_devices(false, 0); //Call sleep routine to turn on matrix, applies when light is high enough and 4th button is not set to always off
  }

  //this runs if auto_intensity is true and display is not off, it defines the intensity based on the light sensor and calls set_devices to set intensity.
  if (auto_intensity && !shut) {
    switch(lx) {
      case 0:
      auto_intensity_value = 0;
      break;
      case 2:
      auto_intensity_value = 1;
      break;
      case 3 ... 4:
      auto_intensity_value = 2;
      break;
      case 5 ... 6:
      auto_intensity_value = 3;
      break;
      case 7 ... 10:
      auto_intensity_value = 4;
      break;
      case 11 ... 20:
      auto_intensity_value = 5;
      break;
      case 21 ... 40:
      auto_intensity_value = 6;
      break;
      case 41 ... 60:
      auto_intensity_value = 7;
      break;
      case 61 ... 100:
      auto_intensity_value = 8;
      break;
      case 101 ... 150:
      auto_intensity_value = 9;
      break;
      case 151 ... 200:
      auto_intensity_value = 10;
      break;
      case 201 ... 250:
      auto_intensity_value = 11;
      break;
      case 251 ... 300:
      auto_intensity_value = 12;
      break;
      case 301 ... 350:
      auto_intensity_value = 13;
      break;
      case 351 ... 400:
      auto_intensity_value = 14;
      break;
      case 401 ... 65535:
      auto_intensity_value = 15;
      break;
    }
    set_devices(true, auto_intensity_value);
    //this is useful for help setting the values above
    //Serial.println(lx);
  }

}


//Routine called by light() to turn on/off matrix and by auto light intensity to adjust device intensity. bool m = true (light intensity), false (matrix on/off), byte i = intensity
void set_devices(bool m, byte i)
{

  int devices = lc.getDeviceCount();
  for (int address = 0; address < devices; address++) {
    if (!m) {
      //turns on/off matrix
      lc.shutdown(address, shut);
    }
    else {
      //sets matrix intensity
      lc.setIntensity(address, i);
    }
  }

}


//Routine to set display on/off options (0 = normal, 1 = always on, 2 = always off, 3 - 5 = after specific time)
void display_options() {

  cls();

  char options[6][9] = {
    "Disp Nrm", "Disp On", "Disp Off", "9.00 pm", "10.00 pm", "11.00 pm"
  };

  display_mode++;
  if (display_mode == 6) {
    display_mode = 0;
  }

  byte i = 0;
  while(options[display_mode][i])
  {
    puttinychar(i * 4, 1, options[display_mode][i]); 
    i++;
  }

  delay(1000);
  cls();

  //display current lux value
  char msg[4] = "LX:";
  i = 0;
  while(msg[i])
  {
    puttinychar(i * 4, 1, msg[i]);
    i++;
  }

  char buffer[6];
  dtostrf(lux.GetLightIntensity(), 5, 0, buffer);
  i = 0;
  while(buffer[i])
  {
    puttinychar(i * 4 + 12, 1, buffer[i]);
    i++;
  }
  
  delay(1000);

}
