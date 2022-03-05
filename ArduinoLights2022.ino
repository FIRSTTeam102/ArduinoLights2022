/***************************************************************************\
* @file ArduinoLights2022.ino
* 
* @brief 2022 Main Arduino code for Team 102's robot light strip(s)
* 
* @remark There are 2 operation modes (by setting their global pre-compiled vars to true/false): USE_SERIAL & USE_CYCLE (USE_SERIAL takes precedence);
* @remark USE_SERIAL relies on numerical serial input to change the current pattern while USE_CYCLE switches between different patterns after some time
* @remark Sending 'b' or 'r' to the serial input will change the alliance mode, which will alter certain patterns
* @remark (for now, it's only implemented by P_DEFAULT, P_ALLIANCE, and P_A_FIRE)
* 
* @see "/README.md"
* @see https://www.tweaking4all.com/hardware/arduino/adruino-led-strip-effects/
\***************************************************************************/

#define APPNAME "FRC102-LED-strip-2022"

// Imports the Arduino Dotstar library from Adafruit; needs to be installed through Arduino's library manager
// (library is called "Adafruit Dotstar", to open the manager: Ctrl+Shift+I or "Tools" - > "Manage Libraries...")
#include <Adafruit_DotStar.h>
////////////////////// Strip configuration settings:
#define NUMPIXELS      23       // Number of LEDs in strip (usually 30, but we're using a shorter segment)
#define DATAPIN        13       // Pin for data input
#define CLOCKPIN       11       // Pin for clock/timer input
////////////////////// Output options:
#define DIMMER         32       // Universal dimmer for all patterns; values greater than 0 will make the strip's colors less bright
#define LOOP_DELAY     40       // Delay (in milliseconds) between each loop/strip/serial update
#define FADE_STRIP     8        // Fade pixels after each loop by this value: 1-255 to fade the strip, 0 to do nothing, and -1 to clear the strip between each loop update
#define MIN_LIGHT      15       // The minimum light output for any R, G, or B value (use a larger value(s) if the strip's pixels aren't completely fading/dimming properly)
////////////////////// Operation modes/settings:
#define INIT_ALLIANCE  1        // Initial alliance; 1 for red alliance, 2 for blue alliance
#define INIT_PTN       1        // Initial light pattern (change this to change the first pattern when the program starts)
#define USE_SERIAL     true     // Use serial input to determine pattern (serial mode takes precedence over cycle mode if both are true)
#define MAX_MSG_LEN    64       // Maximum number of bytes to read & parse from available serial input on each loop
#define USE_CYCLE      false    // Cycle through various patterns
#define CYCLE_DELAY    5000     // Delay between patterns (in ms)
#define CYCLE_MIN      1        // Number of the first pattern to show in cycle mode
#define CYCLE_MAX      7        // Number of the last pattern to show in cycle mode
////////////////////// Pattern names (see README file for more info):
#define P_OFF          0        // (No pattern)
#define P_DEFAULT      1        // The fire pattern!
#define P_DISABLED     2        // Alternating orange & black/off
#define P_AUTO         3        // Opposing blue sliders
#define P_INTAKE       4        // Purple sine slider on black/off background
#define P_LIMELIGHT    5        // Green (lime) slider on black/off background
#define P_SHOOTING     6        // Orange sine slider on black/off background
#define P_CLIMBING     7        // Double rainbow slider of black/off background
#define P_ALLIANCE     8        // Similar to P_AUTO, but represents us on either alliance
#define P_A_FIRE       9        // Alliance-colored less varied fire for better alliance representation
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

////////////////////// Initialize the LED strip library
Adafruit_DotStar strip = Adafruit_DotStar(
  NUMPIXELS, DATAPIN, CLOCKPIN, DOTSTAR_BGR);
  
////////////////////// Other variables:
int tick = 0;
int pTick = 0;
int alliance = INIT_ALLIANCE;
int pattern = INIT_PTN;
int oldPattern = pattern;

////////////////////// Other general functions:
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
      else
        if ((inp == '\n' || inp == '\r')) {/*Serial.print("CR or NL received, msg="); Serial.println((int) atoi(msg));*/}
        else {break;}
      i++;
    }
    msg[i] = '\0';
    rtn = (int) atoi(msg);
    return rtn;
  } else return pattern;
}

// Set a pixel's color (using 3 integer values) & apply intensity
void colorPixel(int i, int r, int g, int b) {
  r=constrain(r,0,255); g=constrain(g,0,255); b=constrain(b,0,255);
  if (r<=MIN_LIGHT) r=0; if (g<=MIN_LIGHT) g=0; if (b<=MIN_LIGHT) b=0; // pixel coloring gets finnicky with values close but not equal to 0
  strip.setPixelColor(i,r,g,b);
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
  uint32_t c = strip.getPixelColor(i);
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

void colorPixelUsingSine(uint32_t c) {
  colorPixel((map(strip.sine8(pTick*1.4),0,255,0,NUMPIXELS)+(int)floor(NUMPIXELS/2)-1)%NUMPIXELS,c);
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
////////////////////// Pattern 0: (no pattern; if you want the strip to clear when the strip is at pattern 0, set FADE_STRIP to -1)
////////////////////// Pattern 1:
  if (p==1) {
    fire(false);
    fadeStrip(32);
  } else
////////////////////// Pattern 2:
  if (p==2) {
    alternate(C_ORANGE,C_BLACK);
  } else
////////////////////// Pattern 3:
  if (p==3) {
    fadeStrip(72);
    colorPixel(int(floor(pTick/2))%NUMPIXELS,C_BLUE);
    //colorPixel(int(floor(pTick*2/2))%NUMPIXELS,C_BLUE);
    colorPixel(NUMPIXELS-int(floor(pTick/2))%NUMPIXELS,C_BLUE);
  } else
////////////////////// Pattern 4:
  if (p==4) {
    colorPixelUsingSine(C_PURPLE);
  } else
////////////////////// Pattern 5:
  if (p==5) {
    colorPixel(int(floor(pTick/2))%NUMPIXELS,C_GREEN);
  } else
////////////////////// Pattern 6:
  if (p==6) {
    colorPixelUsingSine(C_ORANGE);
  } else
////////////////////// Pattern 7:
  if (p==7) {
    fadeStrip(72);
    colorPixelFromIndex(int(floor(pTick/2))%NUMPIXELS);
    colorPixelFromIndex(int(floor(pTick+NUMPIXELS)/2)%NUMPIXELS);
  } else
////////////////////// Pattern 8:
  if (p==8) {
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
////////////////////// Pattern 9:
  if (p==9) {
    fire(true);
    fadeStrip(16);
  }
////////////////////// Show pixels
  if (DIMMER > 0) fadeStrip(DIMMER);
  strip.show();
}

void setup() {
  // start serial monitor (communication between arduino & pc)
  Serial.begin(9600);
  // start LED strip library
  strip.begin();
  strip.clear();
  strip.show();
  // if using cycle, make sure it starts within correct range
  if (USE_SERIAL != true && USE_CYCLE == true) {
    if (pattern > CYCLE_MAX || pattern < CYCLE_MIN) pattern=CYCLE_MIN;
  }
}

void loop() {
  // increment tick(s) (cycles after 1 hr a.k.a. 3600000 ms)
  if ((tick*LOOP_DELAY) > 3600000) tick=0; else tick++;
  if ((pTick*LOOP_DELAY) > 3600000) pTick=0; else pTick++;
  // check mode
  if (USE_SERIAL == true) {
    pattern = readSerial();
    patterns(pattern);
    //Serial.println(pattern);
  }
  else if (USE_CYCLE == true) {
    // update cycle pattern if necessary
    if ((tick*LOOP_DELAY)/2 > CYCLE_DELAY) {
      tick=0; pattern++;
      if (pattern >= CYCLE_MAX || pattern < CYCLE_MIN) pattern=CYCLE_MIN;
      Serial.print("[led_strip_2022 Cycle Mode] Switching to next pattern: #");
      Serial.println(pattern);
    } else tick++;
    patterns(pattern);
  }
  
  // delay between loops
  delay(LOOP_DELAY);
  if (FADE_STRIP > 0) fadeStrip(FADE_STRIP);
  else if (FADE_STRIP == -1) strip.clear();
}
