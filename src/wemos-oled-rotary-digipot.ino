#include <Arduino.h>

//
// Digital Potentiometer 1-100k with OLED display
//
// Developed and tested with following components:
// - KY-40
// - both Wemos D1 Mini & Mini Pro (3.3V)
// - Wemos OLED Shield (48x64px)
// - X9C104 digital potentiometer
//

#include <Wire.h> // Required for I2C support
#include <SPI.h> // Required by SFE MicroOLED (only)
#include <SFE_MicroOLED.h>

//////////////////////////
// MicroOLED Definition //
//////////////////////////

#define OLED_SDA 1
#define OLED_SCL 2

// use to disable OLED and debug to Serial instead
boolean useOLED = true;

MicroOLED oled(OLED_SDA, OLED_SCL);

///////////////////////////////
// Rotary Encoder Definition //
///////////////////////////////

volatile boolean TurnDetected;
volatile boolean pressed;
volatile boolean up;

volatile boolean clk_state;
volatile boolean dt_state;

volatile int virtualPosition = 0;

// Connections to KY-40 rotary controller
const int PinCLK = D7;  // Used for generating interrupts using CLK signal
const int PinDT  = D4;  // Used for reading DT signal, LED feedback
const int PinSW  = D5;  // Used for the push button switch

// Connections to X9C digital potentiometer
const int PotUD = D6; // Used for controlling movement direction
const int PotINC = D8; // Used for moving the pot position
const int PotCS = D0; // Used for saving state (optional)

int resistance = 50; // kOhms (default)

//////////////////////////
// Ohm-meter Definition //
//////////////////////////

int analogPin = 0;
int raw = 0;
int Vin = 3.3; // change this to 3,3 on Wemos D1 or use 5V VCC
float Vout = 0;
float R1 = 10000; // 10k reference resistor
float R2 = 0;
float buf = 0;

float readKOhms(int analogPin) {
  raw = analogRead(analogPin);
  if (raw) {
    buf = raw * Vin;
    Vout = (buf)/1024.0;
    buf = (Vin/Vout) -1;
    R2 = R1 * buf;
  }
  return R2;
}

long now = millis();

void setup ()  {

   Serial.begin (115200);
   Serial.println(""); // CR/LF

   Serial.println("Setting modes for input PINs:");

   Serial.println(PinCLK);
   pinMode(PinCLK,INPUT);
   Serial.println(PinDT);
   pinMode(PinDT,INPUT);
   Serial.println(PinSW);
   pinMode(PinSW,INPUT_PULLUP); // PULLUP may work

   Serial.println("Setting modes for output PINs:");
   Serial.println(PotINC);
   pinMode(PotINC,OUTPUT);
   Serial.println(PotUD);
   pinMode(PotUD,OUTPUT);
   Serial.println(PotCS);
   pinMode(PotCS,OUTPUT);

   digitalWrite(PotINC, HIGH);
   digitalWrite(PotCS, LOW); // should be connected to GND

   // Wemos D1 pin mapping
   Serial.println("Attaching interrupts...");
   Serial.println("CLK");
   attachInterrupt (PinCLK,isr_clk,RISING);   // D5 (CLK) = Arduino 13
   Serial.println("SW");
   attachInterrupt (PinSW,isr_push,FALLING);   // D5 (SW) = Arduino 14
   Serial.println("DONE");

   TurnDetected = false;
   pressed = false;

   if (useOLED == true) {
    delay(500);
    oled.begin();
    oled.clear(ALL);
    oled.display();
    delay(1000);
    oled.clear(PAGE);
    oled.println("READY");
    oled.display();
    delay(500);
  }
}

void measureAndDisplayOutput() {
   R2 = readKOhms(analogPin);   
  if (useOLED == true) {    
    oled.setCursor(0, 16);
    oled.print(R2/1000);
    oled.println("kOhm");
    oled.display();    
  } else {
    Serial.print(R2/1000);
    Serial.print(" kOhm");
    Serial.println("");
  }
}

void processRotary() {

   digitalWrite(PotINC, HIGH);  // hold pot wiper

  // Catch the 'press' event and restore default state
  if (pressed == true) {
    if (virtualPosition != 0) {
      buttonPressed();
    } else {
      displayCurrentPosition();
    }
    TurnDetected = false;
    pressed = false;
  }

  // Evaluate turn direction and restore default state
  if (TurnDetected == true)  {
    
    // Set the digipot direction and move
    digitalWrite(PotUD, !up); // (HIGH is Up, LOW is Down)
    digitalWrite(PotINC, LOW); // normally HIGH, toggle this pin to change pot wiper val
    delay(10);
     
    // displayCurrentPosition(); debug only, takes too much time
    measureAndDisplayOutput();

    TurnDetected = false; // do not repeat in loop until new rotation detected
  }
}

void displayCurrentPosition() {
  displayString(String(virtualPosition));
}

void displayString(String value) {
  if (useOLED == true) {
    oled.clear(PAGE);
    oled.setCursor(0, 0);
    oled.println(value);
    oled.display();
    delay(50);
  } else {
    Serial.println(value);
  }
}

void buttonPressed() {
  virtualPosition = 0; // TODO: unwind/reset the digipot here!
  pressed = false;
  if (useOLED == true) {
    oled.clear(PAGE);
    oled.println("RESET");
    oled.display();
  } else {
    Serial.println (virtualPosition);
  }
}

//
// Interrupt service routines
//

// Executed when a HIGH to LOW transition is detected on CLK
void isr_clk() {
  //clk_state = LOW;
  cli(); //stop interrupts happening before we read pin values
  clk_state = digitalRead(PinCLK);
  if (clk_state == digitalRead(PinDT)) {
    up = true;
    virtualPosition++;
  } else {
    up = false;
    virtualPosition--;
  }
  TurnDetected = true;
  sei(); //restart interrupts
}

// Executed when a HIGH to LOW transition is detected on Reset Switch
void isr_push()  {
 pressed = !digitalRead(PinSW);
}

void loop() {
  processRotary();  
}
