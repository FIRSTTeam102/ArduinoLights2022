/***************************************************************************\
* @file ArduinoLights2022.ino (use_binary branch)
* 
* @brief 2022 Arduino code for Team 102's robot light strip(s) that aims to (hopefully) use binary pin input as an available control source
* 
* @remark There are 3 operation modes (by setting their global pre-compiled vars to true/false): USE_SERIAL & USE_BINARY (USE_SERIAL takes precedence) with a fallback to cycling through;
* @remark USE_SERIAL relies on numerical serial input to change the current pattern while USE_CYCLE switches between different patterns after some time
* @remark Sending 'b' or 'r' to the serial input will change the alliance mode, which will alter certain patterns
* @remark (for now, it's only implemented by P_TELEOP)
* @remark Reversing the strip (switching the pixel order e.g. 1 to 30, 2 to 29, etc.) is now as easy as setting REVERSE_DIR to true!
* 
* @see "/README.md"
* @see https://www.tweaking4all.com/hardware/arduino/adruino-led-strip-effects/
\***************************************************************************/

#define APPNAME "FRC102-LED-strip-2022-use-binary"

// Imports the Arduino Dotstar library from Adafruit; needs to be installed through Arduino's library manager
// (library is called "Adafruit Dotstar", to open the manager: Ctrl+Shift+I or "Tools" - > "Manage Libraries...")
#include <Adafruit_DotStar.h>
////////////////////// Strip configuration settings:
#define NUMPIXELS      23       // Number of LEDs in strip (usually 30, but we're using a shorter segment)
#define DATAPIN        13       // Pin for data input
#define CLOCKPIN       11       // Pin for clock/timer input
#define SERIAL_BAUD    9600     // Baud rate for Serial communications
////////////////////// Output options:
#define DIMMER         32       // Universal dimmer for all patterns; values greater than 0 will make the strip's colors less bright
#define LOOP_DELAY     40       // Delay (in milliseconds) between each loop/strip/serial update
#define FADE_STRIP     8        // Fade pixels after each loop by this value: 1-255 to fade the strip, 0 to do nothing, and -1 to clear the strip between each loop update
#define MIN_LIGHT      15       // The minimum light output for any R, G, or B value (try a larger value(s) if the strip's pixels aren't completely fading/dimming properly)
////////////////////// Binary input settings:
#define USE_BINARY     true     // Use 3 of specified input pins to receive a binary number for the current pattern and the 4th pin as a boolean for the current alliance
#define B_PIN_1        2        // USE_BINARY input pin #1
#define B_PIN_2        3        // USE_BINARY input pin #2
#define B_PIN_3        4        // USE_BINARY input pin #3
#define B_PIN_4        5        // USE_BINARY input pin #4 (used for changing alliance colors)
////////////////////// Operation modes/settings:
#define SM_PREFIX      "[led_strip_2022]"        // Prefix for printing to the serial monitor
// we should use this but we're already using pin 13...
#define SHOW_STATUS    0        // If greater than 0, toggle the built-in LED every SHOW_STATUS ticks (approx. every # / LOOP_DELAY milliseconds)
#define INIT_ALLIANCE  1        // Initial alliance; 0 for blue alliance, 1 for red alliance
#define INIT_PTN       1        // Initial light pattern (change this to change the first pattern when the program starts)
#define REVERSE_DIR    true     // Thanks to @caburum for helping debug this! If true, reverses the strip's pixel order (1 becomes last, 2 becomes second-to-last etc.)
#define USE_SERIAL     false    // Use serial input to determine pattern (serial mode takes precedence over cycle mode if both are true)
#define MAX_MSG_LEN    64       // Maximum number of bytes to read & parse from available serial input on each loop
#define USE_CYCLE      false    // Cycle through various patterns
#define CYCLE_DELAY    5000     // Delay between patterns (in ms)
#define CYCLE_MIN      1        // Number of the first pattern to show in cycle mode
#define CYCLE_MAX      7        // Number of the last pattern to show in cycle mode
////////////////////// Pattern names (see README file for more info):
#define P_DISABLED     0        // Alternating orange & black/off
#define P_TELEOP       1        // The fire pattern!
#define P_AUTO         2        // Opposing blue sliders
#define P_             3
#define P_INTAKE       4        // Purple sine slider on black/off background
#define P_LIMELIGHT    5        // Green (lime) slider on black/off background
#define P_SHOOTING     6        // Orange sine slider on black/off background
#define P_CLIMBING     7        // Double rainbow slider of black/off background
////////////////////// Major colors:
#define C_BLACK        0x00000000UL
#define C_GRAY         0x007e7e7eUL
#define C_WHITE        0x00eeeeeeUL
#define C_RED          0x00ff0000UL
#define C_ORANGE       0x00c82000UL
#define C_YELLOW       0x20808008UL
#define C_GREEN        0x0000ff00UL
#define C_BLUE         0x000814efUL
#define C_PURPLE       0x0025003cUL // i think this color was originally called "mahoney_purple" or something
#define OFFICIAL_RED   0x00ED1C24UL // (2020's) Official FIRST red color (may look different when displayed)
#define OFFICIAL_BLUE  0x000066B3UL // (2020's) Official FIRST blue color (may look different when displayed)

////////////////////// Variable for the LED strip library (actual initialization is within void setup(){} after checking for pin conflicts)
Adafruit_DotStar strip = 0;

////////////////////// Other variables:
int tick = 0;
int pTick = 0;
int sTick = 0;
int alliance = INIT_ALLIANCE;
int pattern = INIT_PTN;
int oldPattern = pattern;
int pins[] = {0,0,0,0};
int pinValue = 0;
bool showStatus = false;

////////////////////// Other general functions:
// Read pattern from binary digital pin inputs
int readPins() {
  // Get pin values
  pins[0] = int(digitalRead(B_PIN_1));
  pins[1] = int(digitalRead(B_PIN_2));
  pins[2] = int(digitalRead(B_PIN_3));
  pins[3] = int(digitalRead(B_PIN_4));

  // Make sure alliance input matches internal setting
  if (alliance != pins[3]) alliance = pins[3];

  // Compute pattern from other 3 inputs
  // (thanks @robtillaart: https://forum.arduino.cc/t/converting-binary-strings-to-bytes-integers/212848/2)
  pinValue = 0;
  for (int i = 0; i < 3; i++) {
    pinValue *= 2; pinValue += (pins[i]==1);
  }

  // Return computed pattern if it's different from current pattern
  if (pattern != pinValue) return pinValue;
  else return pattern;
}

// Read serial input to update current pattern
// NOTE: if there's a non-digit character, the input stops reading until the next loop
int readSerial() {
  if (Serial.available() > 0) {
    int total = Serial.available(),
        i = 1, rtn;
    total=constrain(total,0,MAX_MSG_LEN)+1;
    char msg[total]; char inp;
    msg[0]='0'; // add a 0 in case input is empty or invalid
    while (i < total) {
      inp = Serial.read();
      // read alliance from serial if available (could cause issues with inputs like '123r')
      if (tolower(inp) == 'b') {alliance=2; return pattern;}
      else if (tolower(inp) == 'r') {alliance=1; return pattern;}
      else if (isDigit(inp)) {msg[i]=inp;}
      else if (!(inp == '\n' || inp == '\r')) {break;}
      i++;
    }
    msg[i] = '\0';
    rtn = (int) atoi(msg);
    return rtn;
  } else return pattern;
}

// Reverse a given pixel index, if the `reverse dir` value is true
int reverseIndex(int i) {
  if (REVERSE_DIR == true) return NUMPIXELS-i-1;
  else return i;
}

// Get the color value from a pixel (used for adding reverse functionality)
uint32_t getColorOfPixel(int i) {
  i=reverseIndex(i); return strip.getPixelColor(i);
}

// Set a pixel's color (using 3 integer values) & apply intensity
void colorPixel(int i, int r, int g, int b) {
  r=constrain(r,0,255); g=constrain(g,0,255); b=constrain(b,0,255);
  if (r<=MIN_LIGHT) r=0; if (g<=MIN_LIGHT) g=0; if (b<=MIN_LIGHT) b=0; // pixel coloring gets finnicky with values close but not equal to 0
  i=reverseIndex(i); strip.setPixelColor(i,r,g,b);
}
// Set all pixels' colors (using 3 integer values) & apply intensity
void colorStrip(int r, int g, int b) {
  for (int i=0; i<NUMPIXELS; i++) colorPixel(i,r,g,b);
}

// Set a pixel's color (using a hex-like value) & apply intensity
void colorPixel(int i, uint32_t c) {
  uint8_t r,g,b;
  r=(c&0x00ff0000UL)>>16; g=(c&0x0000ff00UL)>>8; b=(c&0x000000ffUL);
  colorPixel(i,r,g,b);
}
// Set all pixels' colors (using a hex-like value) & apply intensity
void colorStrip(uint32_t c) {
  for (int i=0; i<NUMPIXELS; i++) colorPixel(i,c);
}

// Fade a pixel by a byte (number) from 0 to 255
// Adapted from Hans Luitjen's fire effect (https://www.tweaking4all.com/hardware/arduino/adruino-led-strip-effects/) based on Mark Kriegsman's "Fire2012"
void fadePixel(int i, byte amt) {
  uint32_t c = getColorOfPixel(i);
  uint8_t r,g,b;
  r=(c&0x00ff0000UL)>>16; g=(c&0x0000ff00UL)>>8; b=(c&0x000000ffUL);
  r-=r*amt/256; g-=g*amt/256; b-=b*amt/256;
  colorPixel(i,r,g,b);
}
// Fade all pixels by a byte (number) from 0 to 255
void fadeStrip(byte amt) {
  for (int i = 0; i < NUMPIXELS; i++) fadePixel(i,amt);
}

////////////////////// Extra functions specifically for patterns:
// adapted from Hans Luitjen's adaptation (https://www.tweaking4all.com/hardware/arduino/adruino-led-strip-effects/) of Mark Kriegsman's "Fire2012":
// https://github.com/FastLED/FastLED/blob/master/examples/Fire2012/Fire2012.ino
void temperaturePixel(int i, byte tmp, bool flat) {
  byte tmp2 = round((tmp/255.0)*191); // get tmp variant for checking
  byte h = map(tmp2,0,191,0,60)*4; // stretch values
  if (alliance == 1) {
    if (flat == true) {
      colorPixel(i, h,16,0);
    } else {
      if (tmp2>184) colorPixel(i, 245,245,h); // set yellow-white
      else if (tmp2>48) colorPixel(i, 255,h,0); // set red-yellow
      else if (tmp2>0) colorPixel(i, h,0,0); // set black-red/orange
    }
  } else {
    if (flat == true) {
      colorPixel(i, 0,16,h);
    } else {
      if (tmp2>184) colorPixel(i, h,245,245); // set blue-white
      else if (tmp2>48) colorPixel(i, 0,255,h); // set blue-green
      else if (tmp2>0) colorPixel(i, 0,0,h); // set black-blue/turquoise
    }
  }
}
// adapted from Hans Luitjen's adaptation (https://www.tweaking4all.com/hardware/arduino/adruino-led-strip-effects/) of Mark Kriegsman's "Fire2012":
// https://github.com/FastLED/FastLED/blob/master/examples/Fire2012/Fire2012.ino
void fire(bool flat) {
  static byte h[NUMPIXELS]; // temperature values
  static int r; // random cool down variable
  
  for(int i=0; i<NUMPIXELS; i++) { // decrease temperatures
    r = random(0, (540/NUMPIXELS) + 1);
    if(r>h[i]) h[i]=0; else h[i]=h[i]-r;
  }
  for(int i=NUMPIXELS-1; i>=2; i--) h[i]=(h[i-1] + (2 * h[i-2]))/3; // average temp
  if(random(255) < 100) h[random(4)]+=random(128, 192); // create random "sparks"
  for(int i=0; i<NUMPIXELS; i++) temperaturePixel(i, h[i], flat); // render pixels' temperatures
}

void alternate(uint32_t a, uint32_t b) {
	colorStrip(a);
	for (int i=(pTick%24>=12)?(0):(1); i<NUMPIXELS; i+=2) colorPixel(i,b);
}

void colorPixelUsingSine(uint32_t c, float speed = 1.7) {
  colorPixel((map(strip.sine8(pTick*speed),0,255,0,NUMPIXELS)+(int)floor(NUMPIXELS/2)-1)%NUMPIXELS,c);
}

// adapted from https://github.com/adafruit/Adafruit_DotStar/blob/master/Adafruit_DotStar.cpp#L658
void colorPixelFromIndex(int i) {
  colorPixel(i,strip.ColorHSV((i*65536*3)/NUMPIXELS, 220, 240));
}

void patterns(int p) {
// See "Pattern names (for reference)" at top of file for pattern descriptions
  if (pattern != oldPattern) {
    pTick = 0;
    oldPattern = pattern;
  }

//////////////////////
  if (p==P_DISABLED) {
    alternate(C_ORANGE,C_BLACK);
  } else
//////////////////////
  if (p==P_TELEOP) {
    fire(false);
    fadeStrip(32);
  } else
//////////////////////
  if (p==P_AUTO) {
    fadeStrip(72);
    colorPixel(int(floor(pTick/2))%NUMPIXELS,C_BLUE);
    //colorPixel(int(floor(pTick*2/2))%NUMPIXELS,C_BLUE);
    colorPixel(NUMPIXELS-int(floor(pTick/2))%NUMPIXELS,C_BLUE);
  } else
//////////////////////
//////////////////////
  if (p==P_INTAKE) {
    colorPixelUsingSine(C_PURPLE);
  } else
//////////////////////
  if (p==P_LIMELIGHT) {
    colorPixel(int(floor(pTick/2))%NUMPIXELS,C_GREEN);
  } else
//////////////////////
  if (p==P_SHOOTING) {
    colorPixelUsingSine(C_ORANGE, 2.4);
  } else
//////////////////////
  if (p==P_CLIMBING) {
    fadeStrip(72);
    colorPixelFromIndex(int(floor(pTick/2))%NUMPIXELS);
    colorPixelFromIndex(int(floor(pTick+NUMPIXELS)/2)%NUMPIXELS);
  } /*else
//////////////////////
  if (p==8) { // P_ALLIANCE
    // b.n. pattern:
    if (alliance == 1){
    fadeStrip(72);
    colorPixel(int(floor(pTick/0.9))%NUMPIXELS,C_ORANGE);
    colorPixel(NUMPIXELS-int(floor(pTick/0.8))%NUMPIXELS,C_RED);
    }
   else{
    fadeStrip(72);
    colorPixel(int(floor(pTick/0.9))%NUMPIXELS,C_ORANGE);
    colorPixel(NUMPIXELS-int(floor(pTick/0.8))%NUMPIXELS,C_BLUE);
   }
  }
//////////////////////
  if (p==9) { // P_ALLIANCE_FIRE
    fire(true);
    fadeStrip(16);
  }*/
////////////////////// Show pixels
  if (DIMMER > 0) fadeStrip(DIMMER);
  strip.show();
}

void setup() {
  // start serial monitor (communication between arduino & pc)
  Serial.begin(SERIAL_BAUD);

  // if using cycle, make sure it starts within correct range
  if (USE_SERIAL != true && USE_CYCLE == true) {
    if (pattern > CYCLE_MAX || pattern < CYCLE_MIN) pattern=CYCLE_MIN;
  }

  // init binary pin modes
  pinMode(B_PIN_1, INPUT_PULLUP);
  pinMode(B_PIN_2, INPUT_PULLUP);
  pinMode(B_PIN_3, INPUT_PULLUP);
  pinMode(B_PIN_4, INPUT_PULLUP);

  // make sure no other modes etc. are using pin 13 to prevent interference with setting the status LED
  if (
    (SHOW_STATUS > 0) && ( // if SHOW_STATUS is enabled...
      DATAPIN == 13 ||     // and also used by any core strip library pins...
      CLOCKPIN == 13 || (
        (USE_BINARY) && (  // or interfering with any binary input pins if enabled...
          B_PIN_1 == 13 ||
          B_PIN_2 == 13 ||
          B_PIN_3 == 13 ||
          B_PIN_4 == 13
        )
      )
    )
  ) { // then print a warning to the serial monitor
    Serial.print(SM_PREFIX);
    Serial.println(" - ! - WARNING - ! - One or more pins have been set to use pin #13, but SHOW_STATUS is also true! Refusing to both set pin 13's pinMode AND start LED library to prevent possible interference/damage to components, sorry for the inconvenience...");
  } else { // or continue as normal if there is no issue
    // Initialize the LED strip library
    strip = Adafruit_DotStar(
  NUMPIXELS, DATAPIN, CLOCKPIN, DOTSTAR_BGR);
    if (SHOW_STATUS > 0) pinMode(13, OUTPUT);
  }
  
  // start LED strip library
  strip.begin();
  strip.clear();
  strip.show();
}

void loop() {
  // increment tick(s) (cycles after 1 hr a.k.a. 3600000 ms)
  if ((tick*LOOP_DELAY) > 3600000) tick=0; else tick++;
  if ((pTick*LOOP_DELAY) > 3600000) pTick=0; else pTick++;
  // check mode
  if (USE_SERIAL == true) {
    pattern = readSerial();
    patterns(pattern);
  } else if (USE_BINARY == true) {
    //Serial.print(SM_PREFIX); Serial.print(" Binary mode: old pattern: #"); Serial.print(pattern); 
    pattern = readPins();
    //Serial.print(SM_PREFIX); Serial.print(" Binary mode: current pattern: #"); Serial.println(pattern);
    patterns(pattern);
  } else {
    // update cycle pattern if necessary
    if ((tick*LOOP_DELAY)/2 > CYCLE_DELAY) {
      tick=0; pattern++;
      if (pattern >= CYCLE_MAX || pattern < CYCLE_MIN) pattern=CYCLE_MIN;
      Serial.print(SM_PREFIX); Serial.print(" Cycle mode: Switching to next pattern: #");
      Serial.println(pattern);
    } else tick++;
    patterns(pattern);
  }
  
  // delay between loops
  delay(LOOP_DELAY);
  // if SHOW_STATUS is enabled (by being above 0), toggle the built-in LED
  if (SHOW_STATUS > 0) {
    sTick += 1; if (!(sTick < SHOW_STATUS)) {sTick = 0; showStatus = !showStatus;}
    if (showStatus) digitalWrite(13,HIGH); else digitalWrite(13,LOW);
    
  }
  if (FADE_STRIP > 0) fadeStrip(FADE_STRIP);
  else if (FADE_STRIP == -1) strip.clear();
}
