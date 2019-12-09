// AVR High-voltage Serial Fuse Reprogrammer
// Arduino Pro Micro 5v version
//
// Gadget Reboot
//
// Rotary Encoder and menu based on example at
// https://educ8s.tv/arduino-rotary-encoder-menu/
//
// Fuse programmer adapted from the version by Ralph Bacon
// https://github.com/RalphBacon/ATTiny85_Fuse_Resetter
// Other History:
// Adapted from code and design by Paul Willoughby 03/20/2010
// http://www.rickety.us/2010/03/arduino-avr-high-voltage-serial-programmer/
// Inspired by Jeff Keyzer mightyohm.com
// Serial Programming routines from ATtiny25/45/85 datasheet
//
// Fuse Calc Tool useful to see available fuses for Atmel devices:
// http://www.engbedded.com/fusecalc/

#include <Wire.h>
#include <Adafruit_SSD1306.h>
#define OLED_RESET 4
Adafruit_SSD1306 display(OLED_RESET);

#include <ClickEncoder.h>
#include <TimerOne.h>

// Rotary Encoder Inputs
#define encA     1     // rotary encoder input A 
#define encB     0     // rotary encoder input B
#define encSW   21     // rotary encoder switch

// HVSP pins
#define  SCI      9    // Target Clock Input
#define  SDO     15    // Target Data Output
#define  SII     14    // Target Instruction Input
#define  SDI     16    // Target Data Input
#define  VCC     19    // Target VCC
#define  RST     10    // Output to control 12V prog voltage to target Reset pin
#define  VRST     4    // Output to control target Reset voltage. High = 12V Low = 5V
#define  VCC_MON A0    // Target VCC monitor on ADC A0 input

// required for accessing device fuse locations
#define  LFUSE  0x646C
#define  HFUSE  0x747C
#define  EFUSE  0x666E

// Digispark default fuses
#define  LVAL1  0xE1
#define  HVAL1  0x5D
#define  EVAL1  0xFE

// Digispark fuses with Reset pin enabled
#define  LVAL2  0xE1
#define  HVAL2  0xDD
#define  EVAL2  0xFE

byte targetHVal = 0;
byte targetLVal = 0;
byte targetEVal = 0;
bool readFuseOnly = true;  // just reading, not writing

// Define ATTiny series signatures
#define  ATTINY13   0x9007  // L: 0x6A, H: 0xFF             8 pin
#define  ATTINY24   0x910B  // L: 0x62, H: 0xDF, E: 0xFF   14 pin
#define  ATTINY25   0x9108  // L: 0x62, H: 0xDF, E: 0xFF    8 pin
#define  ATTINY44   0x9207  // L: 0x62, H: 0xDF, E: 0xFFF  14 pin
#define  ATTINY45   0x9206  // L: 0x62, H: 0xDF, E: 0xFF    8 pin
#define  ATTINY84   0x930C  // L: 0x62, H: 0xDF, E: 0xFFF  14 pin
#define  ATTINY85   0x930B  // L: 0x62, H: 0xDF, E: 0xFF    8 pin

int menuitem = 1;
int frame = 1;
int page = 1;
int lastMenuItem = 1;

String menuItem1 = "Read Fuses";
String menuItem2 = "T85 RST DIS";
String menuItem3 = "T85 RST ENA";
String menuItem4 = "Reserved";
String menuItem5 = "Reserved";
String menuItem6 = "Reserved";

boolean up = false;
boolean down = false;
boolean encButtonPress = false;

ClickEncoder *encoder;
int16_t last, value;

void setup() {

  Serial.begin(9600);
  // while the serial stream is not open, do nothing:
  while (!Serial) ;

  Serial.println("AVR Programmer Debug Output");
  Serial.println();

  // configure programming pins
  digitalWrite(RST, LOW);    // don't apply reset voltage to target yet
  digitalWrite(VRST, HIGH);  // set reset voltage to 12v for HVSP
  digitalWrite(VCC, HIGH);   // don't apply VCC to target yet
  digitalWrite(SDI, LOW);
  digitalWrite(SII, LOW);
  digitalWrite(SDO, LOW);
  pinMode(VCC, OUTPUT);
  pinMode(RST, OUTPUT);
  pinMode(VRST, OUTPUT);
  pinMode(SDI, OUTPUT);
  pinMode(SII, OUTPUT);
  pinMode(SCI, OUTPUT);
  pinMode(SDO, OUTPUT);     // configured as input when in programming mode

  encoder = new ClickEncoder(encB, encA, encSW);
  encoder->setAccelerationEnabled(false);

  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C address of the display
  display.setTextSize(0);
  display.setTextColor(WHITE);
  display.clearDisplay();
  display.display();

  Timer1.initialize(1000);
  Timer1.attachInterrupt(timerIsr);

  last = encoder->getValue();
}

void loop() {

  drawMenu();

  readRotaryEncoder();

  ClickEncoder::Button b = encoder->getButton();
  if (b != ClickEncoder::Open) {
    switch (b) {
      case ClickEncoder::Clicked:
        encButtonPress = true;
        break;
    }
  }

  if (up && page == 1 ) {

    up = false;
    if (menuitem == 2 && frame == 2)
    {
      frame--;
    }

    if (menuitem == 4 && frame == 4)
    {
      frame--;
    }
    if (menuitem == 3 && frame == 3)
    {
      frame--;
    }
    lastMenuItem = menuitem;
    menuitem--;
    if (menuitem == 0)
    {
      menuitem = 1;
    }
  }
  else if (up && page == 2 && menuitem == 1 ) {
    up = false;
    readFuses();

  }
  else if (up && page == 2 && menuitem == 2 ) {
    up = false;
    //  volume--;
  }
  else if (up && page == 2 && menuitem == 3 ) {
    up = false;
    //  selectedLanguage--;
    //  if (selectedLanguage == -1)
    //   {
    //     selectedLanguage = 2;
    //  }
  }
  else if (up && page == 2 && menuitem == 4 ) {
    up = false;
    // selectedDifficulty--;
    // if (selectedDifficulty == -1)
    //  {
    //    selectedDifficulty = 1;
    //  }
  }

  if (down && page == 1) //We have turned the Rotary Encoder Clockwise
  {

    down = false;
    if (menuitem == 3 && lastMenuItem == 2)
    {
      frame ++;
    } else  if (menuitem == 4 && lastMenuItem == 3)
    {
      frame ++;
    }
    else  if (menuitem == 5 && lastMenuItem == 4 && frame != 4)
    {
      frame ++;
    }
    lastMenuItem = menuitem;
    menuitem++;
    if (menuitem == 7)
    {
      menuitem--;
    }

  } else if (down && page == 2 && menuitem == 1) {
    down = false;
    readFuses();
  }
  else if (down && page == 2 && menuitem == 2) {
    down = false;
  }
  else if (down && page == 2 && menuitem == 3 ) {
    down = false;
  }
  else if (down && page == 2 && menuitem == 4 ) {
    down = false;
  }

  if (encButtonPress) // encoder button is Pressed
  {
    encButtonPress = false;

    if (page == 1 && menuitem == 1) // Read Fuses
    {
      initializeHVSP();
      readFuses();
      endHVSP();
      delay(5000);  // keep last fuse read-back on OLED for a while
    }

    if (page == 1 && menuitem == 2) // T85 Reset Disabled
    {
      targetHVal = HVAL1;
      targetLVal = LVAL1;
      targetEVal = EVAL1;
      initializeHVSP();
      writeFuses();
      endHVSP();
      delay(5000);  // keep last fuse read-back on OLED for a while

    }

    if (page == 1 && menuitem == 3) // T85 Reset Enabled
    {
      targetHVal = HVAL2;
      targetLVal = LVAL2;
      targetEVal = EVAL2;
      initializeHVSP();
      writeFuses();
      endHVSP();
      delay(5000);  // keep last fuse read-back on OLED for a while

    }

    if (page == 1 && menuitem == 4) // Reserved
    {
      // reserved feature
    }

    if (page == 1 && menuitem == 5) // Reserved
    {
      // reserved feature
    }

    if (page == 1 && menuitem == 6) // Reserved
    {
      // reserved feature
    }

    /*  // sub-menu levels
        else if (page == 1 && menuitem <= 4) {
          page = 2;
        }
    */
    else if (page == 2)  // esc back to top level menu
    {
      page = 1;
    }
  }
}

void drawMenu()
{

  if (page == 1)
  {
    display.setTextSize(0);
    display.clearDisplay();
    display.setTextColor(BLACK, WHITE);
    display.setCursor(15, 0);
    display.print("MAIN MENU");
    display.drawFastHLine(0, 10, 83, BLACK);

    if (menuitem == 1 && frame == 1)
    {
      displayMenuItem(menuItem1, 8, true);
      displayMenuItem(menuItem2, 16, false);
      displayMenuItem(menuItem3, 24, false);
    }
    else if (menuitem == 2 && frame == 1)
    {
      displayMenuItem(menuItem1, 8, false);
      displayMenuItem(menuItem2, 16, true);
      displayMenuItem(menuItem3, 24, false);
    }
    else if (menuitem == 3 && frame == 1)
    {
      displayMenuItem(menuItem1, 8, false);
      displayMenuItem(menuItem2, 16, false);
      displayMenuItem(menuItem3, 24, true);
    }
    else if (menuitem == 4 && frame == 2)
    {
      displayMenuItem(menuItem2, 8, false);
      displayMenuItem(menuItem3, 16, false);
      displayMenuItem(menuItem4, 24, true);
    }

    else if (menuitem == 3 && frame == 2)
    {
      displayMenuItem(menuItem2, 8, false);
      displayMenuItem(menuItem3, 16, true);
      displayMenuItem(menuItem4, 24, false);
    }
    else if (menuitem == 2 && frame == 2)
    {
      displayMenuItem(menuItem2, 8, true);
      displayMenuItem(menuItem3, 16, false);
      displayMenuItem(menuItem4, 24, false);
    }

    else if (menuitem == 5 && frame == 3)
    {
      displayMenuItem(menuItem3, 8, false);
      displayMenuItem(menuItem4, 16, false);
      displayMenuItem(menuItem5, 24, true);
    }

    else if (menuitem == 6 && frame == 4)
    {
      displayMenuItem(menuItem4, 8, false);
      displayMenuItem(menuItem5, 16, false);
      displayMenuItem(menuItem6, 24, true);
    }

    else if (menuitem == 5 && frame == 4)
    {
      displayMenuItem(menuItem4, 8, false);
      displayMenuItem(menuItem5, 16, true);
      displayMenuItem(menuItem6, 24, false);
    }
    else if (menuitem == 4 && frame == 4)
    {
      displayMenuItem(menuItem4, 8, true);
      displayMenuItem(menuItem5, 16, false);
      displayMenuItem(menuItem6, 24, false);
    }
    else if (menuitem == 3 && frame == 3)
    {
      displayMenuItem(menuItem3, 8, true);
      displayMenuItem(menuItem4, 16, false);
      displayMenuItem(menuItem5, 24, false);
    }
    else if (menuitem == 2 && frame == 2)
    {
      displayMenuItem(menuItem2, 8, true);
      displayMenuItem(menuItem3, 16, false);
      displayMenuItem(menuItem4, 24, false);
    }
    else if (menuitem == 4 && frame == 3)
    {
      displayMenuItem(menuItem3, 8, false);
      displayMenuItem(menuItem4, 16, true);
      displayMenuItem(menuItem5, 24, false);
    }
    display.display();
  }
  else if (page == 2 && menuitem == 1)
  {
    displayStringMenuPage(menuItem1, "test");
  }

  else if (page == 2 && menuitem == 2)
  {
    displayIntMenuPage(menuItem2, 10);
  }
  else if (page == 2 && menuitem == 3)
  {
    displayStringMenuPage(menuItem3, "test");
  }
  else if (page == 2 && menuitem == 4)
  {
    displayStringMenuPage(menuItem4, "test");
  }
  else if (page == 2 && menuitem == 5)
  {
    displayStringMenuPage(menuItem5, "test");
  }

}


void timerIsr() {
  encoder->service();
}

void displayIntMenuPage(String menuItem, int value)
{
  display.setTextSize(0);
  display.clearDisplay();
  display.setTextColor(WHITE, BLACK);
  display.setCursor(15, 0);
  display.print(menuItem);
  display.drawFastHLine(0, 7, 83, WHITE);
  display.setCursor(5, 8);
  display.print("Value");
  display.setTextSize(0);
  display.setCursor(5, 16);
  display.print(value);
  display.setTextSize(0);
  display.display();
}

void displayStringMenuPage(String menuItem, String value)
{
  display.setTextSize(0);
  display.clearDisplay();
  display.setTextColor(WHITE, BLACK);
  display.setCursor(15, 0);
  display.print(menuItem);
  display.drawFastHLine(0, 7, 83, WHITE);
  display.setCursor(5, 8);
  display.print("Value");
  display.setTextSize(0);
  display.setCursor(5, 16);
  display.print(value);
  display.setTextSize(0);
  display.display();
}

void displayMenuItem(String item, int position, boolean selected)
{
  if (selected)
  {
    display.setTextColor(BLACK, WHITE);
  } else
  {
    display.setTextColor(WHITE, BLACK);
  }
  display.setCursor(0, position);
  display.print(">" + item);
}

void readRotaryEncoder()
{
  value += encoder->getValue();

  if (value / 2 > last) {
    last = value / 2;
    down = true;
    delay(150);
  } else   if (value / 2 < last) {
    last = value / 2;
    up = true;
    delay(150);
  }
}

/*
   From ATTiny85 Data Sheet
   ------------------------
   20.7.1 Enter High-voltage Serial Programming Mode

   The following algorithm puts the device in High-voltage Serial Programming mode:
   1. Set Prog_enable pins listed in Table 20-14 to “000”, RESET pin and VCC to 0V.
   2. Apply 4.5 - 5.5V between VCC and GND. Ensure that VCC reaches at least 1.8V within the next 20 µs.
   3. Wait 20 - 60 µs, and apply 11.5 - 12.5V to RESET.
   4. Keep the Prog_enable pins unchanged for at least 10 µs after the High-voltage has been applied to
   ensure the Prog_enable Signature has been latched.
   5. Release the Prog_enable[2] pin to avoid drive contention on the Prog_enable[2]/SDO pin.
   6. Wait at least 300 µs before giving any serial instructions on SDI/SII.
   7. Exit Programming mode by power the device down or by bringing RESET pin to 0V.

   If the rise time of the VCC is unable to fulfill the requirements listed above, the following alternative algorithm can be
   used:
   1. Set Prog_enable pins listed in Table 20-14 to “000”, RESET pin and VCC to 0V.
   2. Apply 4.5 - 5.5V between VCC and GND.
   3. Monitor VCC, and as soon as VCC reaches 0.9 - 1.1V, apply 11.5 - 12.5V to RESET.
   4. Keep the Prog_enable pins unchanged for at least 10 µs after the High-voltage has been applied to
   ensure the Prog_enable Signature has been latched.
   5. Release the Prog_enable[2] pin to avoid drive contention on the Prog_enable[2]/SDO pin.
   6. Wait until VCC actually reaches 4.5 - 5.5V before giving any serial instructions on SDI/SII.
   7. Exit Programming mode by power the device down or by bringing RESET pin to 0V

  Future enhancement:  monitor VCC with analog input and follow protocol more directly
                      for now, 100uS works to allow VCC to rise on Digispark board
                      powered by a high side P-ch FET switch
*/

void initializeHVSP()
{
  // power up target device in high voltage programming mode
  pinMode(SDO, OUTPUT);    // Set SDO to output
  digitalWrite(SDI, LOW);
  digitalWrite(SII, LOW);
  digitalWrite(SDO, LOW);
  digitalWrite(RST, LOW);  // 12v Off
  digitalWrite(VCC, LOW); // Vcc On
  delayMicroseconds(100);  // Ensure VCC has reached at least 1.1v before applying 12v to reset
  digitalWrite(RST, HIGH); // 12v On
  delayMicroseconds(10);
  pinMode(SDO, INPUT);     // Set SDO to input
  delayMicroseconds(300);  // Ensure VCC has reached at least 4.5v before issuing instructions

  unsigned int sig = readSignature();
  Serial.print("Signature: ");
  Serial.println(sig, HEX);
}

void endHVSP()
{
  digitalWrite(SCI, LOW);
  digitalWrite(RST, LOW);   // 12v Off
  delayMicroseconds(40);
  digitalWrite(VCC, HIGH);    // Vcc Off
}


byte shiftOut (byte val1, byte val2) {
  int inBits = 0;
  //Wait until SDO goes high
  while (!digitalRead(SDO))
    ;
  unsigned int dout = (unsigned int) val1 << 2;
  unsigned int iout = (unsigned int) val2 << 2;
  for (int ii = 10; ii >= 0; ii--)  {
    digitalWrite(SDI, !!(dout & (1 << ii)));
    digitalWrite(SII, !!(iout & (1 << ii)));
    inBits <<= 1;         inBits |= digitalRead(SDO);
    digitalWrite(SCI, HIGH);
    digitalWrite(SCI, LOW);
  }
  return inBits >> 2;
}

void writeFuse (unsigned int fuse, byte val) {
  shiftOut(0x40, 0x4C);
  shiftOut( val, 0x2C);
  shiftOut(0x00, (byte) (fuse >> 8));
  shiftOut(0x00, (byte) fuse);
}

void readFuses () {
  byte val;

  shiftOut(0x04, 0x4C);  // LFuse
  shiftOut(0x00, 0x68);
  val = shiftOut(0x00, 0x6C);
  Serial.print("LFuse:");
  Serial.print(val, HEX);
  display.clearDisplay();
  display.setCursor(0, 8);
  display.print("LFuse: ");
  display.println(val, HEX);
  display.display();
  delay(1);

  shiftOut(0x04, 0x4C);  // HFuse
  shiftOut(0x00, 0x7A);
  val = shiftOut(0x00, 0x7E);
  Serial.print(", HFuse: ");
  Serial.print(val, HEX);
  display.setCursor(0, 16);
  display.print("HFuse: ");
  display.println(val, HEX);
  display.display();
  delay(1);

  unsigned int sig = readSignature();
  if (sig != 0x9007) {  // if not ATTiny13, read an extended fuse
    shiftOut(0x04, 0x4C);  // EFuse
    shiftOut(0x00, 0x6A);
    val = shiftOut(0x00, 0x6E);
    Serial.print(", EFuse: ");
    Serial.println(val, HEX);
    display.setCursor(0, 24);
    display.print("EFuse: ");
    display.println(val, HEX);
    display.display();
    delay(1);
  }
  Serial.println();
}

void writeFuses () {
  Serial.println("Writing Fuses...");

  unsigned int sig = readSignature();
  writeFuse(LFUSE, targetLVal);
  writeFuse(HFUSE, targetHVal);
  if (sig != 0x9007)  // if not ATTiny13, write an extended fuse
    writeFuse(EFUSE, targetEVal);

  Serial.println("Reading Fuses...");
  readFuses();
}

unsigned int readSignature () {
  unsigned int sig = 0;
  byte val;
  for (int ii = 1; ii < 3; ii++) {
    shiftOut(0x08, 0x4C);
    shiftOut(  ii, 0x0C);
    shiftOut(0x00, 0x68);
    val = shiftOut(0x00, 0x6C);
    sig = (sig << 8) + val;
  }
  return sig;
}
