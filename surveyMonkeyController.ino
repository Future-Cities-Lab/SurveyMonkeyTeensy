/*NOTES: Ripon DeLeon 11/15/16
******UPLOAD AT 24 MHZ******** (faster processor speed causes SPI conflict between Ethernet and LEDs leading to flickers

  Set DIP to 0 for test mode

  Library Notes:
  Ethernet Library is modified to W5200_RESET_PIN set to pin 23

  T3Mac Library updated to update mac[]

*/
#include <T3Mac.h>

#include "FastLED.h"
#include <SPI.h>
#include <Ethernet.h>
#include <EthernetUdp.h>

/* MUX SELECTOR SWITCH PINS */
#define s0 20
#define s1 21
#define s2 22

/* LED */
#define LED_1_DATA 14 //2
#define LED_1_CLOCK 15 //3

#define LED_2_DATA 16 //4
#define LED_2_CLOCK 17 //5

#define LED_3_DATA 18 //6
#define LED_3_CLOCK 19 //7

#define LED_4_DATA 2 //8
#define LED_4_CLOCK 3 //9

#define LED_5_DATA 4 //14
#define LED_5_CLOCK 5 //15

#define LED_6_DATA 6 //16
#define LED_6_CLOCK 7 //17

#define LED_7_DATA 8 //18
#define LED_7_CLOCK 9 //19

/* GLOBALS */
#define LEDS_PER_COLUMN 32
#define TOTAL_LEDS 224

/* TEST MODEL */
//SET DIP to 0 for test mode
int test_c = 0;
int test_v = 0;
int fadeValue = 3;

/* POT - BRIGHTNESS ADJUSTMENT */
int sigPin = A9;
int brightnessTestMode = 0;

/* NETWORKING */
EthernetUDP Udp;
char packetBuffer[UDP_TX_PACKET_MAX_SIZE];
IPAddress ip;
unsigned int localPort = 8888; //UDP Listening port
const unsigned int resetPin = 23; //Ethernet Reset Pin

/* COLOR DATA */
const int LED_IN_BUFFER_SIZE = TOTAL_LEDS * 3;
char ledBuffer[LED_IN_BUFFER_SIZE];
CRGB leds[TOTAL_LEDS];

/* PANEL */
int currentModule;


int state = 1;

//Functions
void testLEDs(); //LED TEST MODE
int readDIPAddress(); //GET PANEL ADDRESS FROM DIP SWITCH
int readBrightness(); //GET BRIGHTNESS FROM TRIMMER POT
void resetEthernet(); //RESET ETHERNET MODULE
void getMAC(); //GET MAC ADDRESS FROM TEENSY



void setup() {

  delay(100);
  // Set selector pins for Mux to output
  pinMode(s0, OUTPUT);
  pinMode(s1, OUTPUT);
  pinMode(s2, OUTPUT);
  delay(20);

  //Read address from DIP
  currentModule = readDIPAddress();

  //Read BRIGHTNESS from Trimmer Pot
  brightnessTestMode = readBrightness();
  Serial.print("Brightness = ");
  Serial.println(brightnessTestMode);
  FastLED.setBrightness(brightnessTestMode);

  //Set Mux to Ethernet Reset pin
  digitalWrite(s0, LOW);
  digitalWrite(s1, HIGH);
  digitalWrite(s2, HIGH);

  //Reset Ethernet Module
  resetEthernet();

  //Begin Network Setup
  ip = IPAddress(192, 168, 1, currentModule + 100);
  getMAC(); //Get MAC address from Teensy
  Ethernet.begin(mac, ip);
  delay(200);

  Udp.begin(localPort);
  Serial.begin(9600);
  Serial.println(Ethernet.localIP());

//Initialize LED Columns
  LEDS.addLeds<WS2801, LED_1_DATA, LED_1_CLOCK>(leds, LEDS_PER_COLUMN * 0, LEDS_PER_COLUMN);
  LEDS.addLeds<WS2801, LED_2_DATA, LED_2_CLOCK>(leds, LEDS_PER_COLUMN * 1, LEDS_PER_COLUMN);
  LEDS.addLeds<WS2801, LED_3_DATA, LED_3_CLOCK>(leds, LEDS_PER_COLUMN * 2, LEDS_PER_COLUMN);
  LEDS.addLeds<WS2801, LED_4_DATA, LED_4_CLOCK>(leds, LEDS_PER_COLUMN * 3, LEDS_PER_COLUMN);
  LEDS.addLeds<WS2801, LED_5_DATA, LED_5_CLOCK>(leds, LEDS_PER_COLUMN * 4, LEDS_PER_COLUMN);
  LEDS.addLeds<WS2801, LED_6_DATA, LED_6_CLOCK>(leds, LEDS_PER_COLUMN * 5, LEDS_PER_COLUMN);
  LEDS.addLeds<WS2801, LED_7_DATA, LED_7_CLOCK>(leds, LEDS_PER_COLUMN * 6, LEDS_PER_COLUMN);

  //Set all LEDs to off
  for (int i = 0; i < TOTAL_LEDS; i++) {
    leds[i] = CRGB(0, 0, 0);
  }
  FastLED.show();

}

void loop() {
  if (currentModule == 0) {
    testLEDs();
  } else {
    int packetSize = Udp.parsePacket();
    if (packetSize == 2) {
      Udp.beginPacket(Udp.remoteIP(), Udp.remotePort());
      Udp.write((char)(currentModule + 100));
      Udp.endPacket();
      state = 2;
    } else if ((packetSize <= LED_IN_BUFFER_SIZE) && (packetSize > 2)) {

      Udp.read((char*)ledBuffer, LED_IN_BUFFER_SIZE);
      memcpy(leds, ledBuffer, LED_IN_BUFFER_SIZE);
    } else if (packetSize == 1 || (state == 1 && packetSize != 2)) {

      FastLED.show();

    }
  }
}

void testLEDs() {
  CRGB current_value;

  switch (test_c) {
    case 0:
      current_value = CRGB(test_v, 0, 0);
      break;
    case 1:
      current_value = CRGB(0, test_v, 0);
      break;
    case 2:
      current_value = CRGB(0, 0, test_v);
      break;
    case 3:
      current_value = CRGB(test_v, test_v, test_v);
      break;
  }

  for (int i = 0; i < TOTAL_LEDS; i++) {
    leds[i] = current_value;
  }

  FastLED.show();
  test_v = test_v + fadeValue;

  if (test_v >= 255) {
    test_v = 255;
    fadeValue = fadeValue * -1;
  } else if (test_v <= 0) {
    test_v = 0;
    fadeValue = fadeValue * -1;
    test_c++;
  }

  if (test_c > 3) {
    test_c = 0;
  }

}

int readDIPAddress() {
  int address = 0;
  int r0[] = {0, 1, 0, 1, 0, 1, 0, 1}; //value of select pin at the 4051 (s0)
  int r1[] = {0, 0, 1, 1, 0, 0, 1, 1}; //value of select pin at the 4051 (s1)
  int r2[] = {0, 0, 0, 0, 1, 1, 1, 1}; //value of select pin at the 4051 (s2)


  for (int i = 0; i < 5; i++) {

    digitalWrite(s0, r0[i]);
    digitalWrite(s1, r1[i]);
    digitalWrite(s2, r2[i]);
    delay(100);

    if ((i == 0) && (analogRead(sigPin) > 512)) {
      address += 2;
    }
    if ((i == 1) && (analogRead(sigPin) > 512)) {
      address += 4;
    }
    if ((i == 2) && (analogRead(sigPin) > 512)) {
      address += 8;
    }
    if ((i == 3) && (analogRead(sigPin) > 512)) {
      address += 1;
    }
    if ((i == 4) && (analogRead(sigPin) > 512)) {
      address += 16;
    }


  }
  Serial.print("DIP Address =>  ");
  Serial.println(address);
  //delay(2000);
  //    Serial.println(muxValue);

  return address;

}

int readBrightness() {

  int val = 0;

  digitalWrite(s0, HIGH);
  digitalWrite(s1, HIGH);
  digitalWrite(s2, HIGH);
  delay(100);
  val = analogRead(sigPin);
  val = map(constrain(val, 100, 1023), 100, 1023, 10, 255);
  return val;
}


//Reset Ethernet Module
void resetEthernet() {

  //set MUX to resetpin channel 6


  Serial.println("Reset Ready");
  digitalWrite(s0, LOW);
  digitalWrite(s1, HIGH);
  digitalWrite(s2, HIGH);
  delay(1000);
  Serial.println("Reset Ethernet");


  pinMode(resetPin, OUTPUT);
  digitalWrite(resetPin, LOW);
  delayMicroseconds(10);
  //delay(1000);
  pinMode(resetPin, INPUT);

  Serial.println("Reset Done");
  delay(1000);
}

void getMAC() {
  delay(1000);

  Serial.println("Reading MAC from hardware...");
  read_mac();

  Serial.print("MAC: ");
  print_mac();
  Serial.println();


  Serial.println("Finished.");

}


static inline void fps(const int seconds) {
  // Create static variables so that the code and variables can
  // all be declared inside a function
  static unsigned long lastMillis;
  static unsigned long frameCount;
  static unsigned int framesPerSecond;

  // It is best if we declare millis() only once
  unsigned long now = millis();
  frameCount ++;
  if (now - lastMillis >= seconds * 1000) {
    framesPerSecond = frameCount / seconds;
    Serial.println(framesPerSecond);
    frameCount = 0;
    lastMillis = now;
  }
}
