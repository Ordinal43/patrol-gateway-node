/*********************
  | EEPROM
*********************/
#include "EEPROM.h"
#define EEPROM_SIZE 128
String strAddress;

/*********************
  | nRF24L01+
*********************/
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <printf.h>

// chip select and RF24 radio setup pins
#define CE_PIN 12
#define CSN_PIN 14
RF24 radio(CE_PIN,CSN_PIN);

/*********************
  | SCREEN TFT ILI9225 176*220
*********************/
#include "SPI.h"
#include "TFT_22_ILI9225.h"
#include <../fonts/FreeSans9pt7b.h>
#include <../fonts/FreeSans12pt7b.h>
#define TFT_CS  5
#define TFT_CLK 18
#define TFT_SDI 19
#define TFT_RS  2
#define TFT_RST 15
#define TFT_LED 0     // 0 if wired to +5V directly
#define TFT_BRIGHTNESS 200 // Initial brightness of TFT backlight (optional)
// Use hardware SPI (faster - on Uno: 13-SCK, 12-MISO, 11-MOSI)
TFT_22_ILI9225 TFTscreen = TFT_22_ILI9225(TFT_RST, TFT_RS, TFT_CS, TFT_LED, TFT_BRIGHTNESS);
int16_t x=0, y=0, width, height; //position;
String strTextDisplay;

#define LED_PIN 32

/*********************
  | TIMER
*********************/
// macros from DateTime.h
/* Useful Constants */
#define SECS_PER_MIN  (60UL)
#define SECS_PER_HOUR (3600UL)
#define SECS_PER_DAY  (SECS_PER_HOUR * 24L)

/* Useful Macros for getting elapsed time */
#define numberOfSeconds(_time_) (_time_ % SECS_PER_MIN)  
#define numberOfMinutes(_time_) ((_time_ / SECS_PER_MIN) % SECS_PER_MIN)
#define numberOfHours(_time_) (( _time_% SECS_PER_DAY) / SECS_PER_HOUR)
#define elapsedDays(_time_) ( _time_ / SECS_PER_DAY)

unsigned long startTime = millis();
unsigned long lastSentTime = startTime;
String lastMeasurements;

/*********************
  | METHOD DEFINITIONS
*********************/
void lcdStartup();
void nrfConnect(byte nodeAddress[]);
void radioCheckAndReply();
void printCurrentTime();
String getFormatted(byte value);

void switchToNRF() {
  digitalWrite(CSN_PIN,HIGH);
  digitalWrite(TFT_CS,HIGH);
}

void switchToLCD() {
  digitalWrite(CSN_PIN,LOW);
  digitalWrite(TFT_CS,LOW); 
}

void setup() {
  if (!EEPROM.begin(200)) {
    Serial.println("Failed to initialise EEPROM");
    Serial.println("Restarting...");
    delay(1000);
    ESP.restart();
  }
  // setup serial communications for basic program display
  Serial.begin(115200);
  Serial.println("[*][*][*] Beginning nRF24L01+ ack-payload node device program [*][*][*]");

  printf_begin();
  
  pinMode(LED_PIN,OUTPUT);
  digitalWrite(LED_PIN,LOW);

  Serial.println("Read data from EEPROM");
  lastMeasurements = EEPROM.readString(0); 
  Serial.println("Previous run measurement:");
  Serial.println(lastMeasurements);
  Serial.println();

  strAddress = "1node";
  byte byteArrNodeAddress[7];
  strAddress.getBytes(byteArrNodeAddress, strAddress.length());

  lcdStartup();
  nrfConnect(byteArrNodeAddress);

  pinMode(CSN_PIN, OUTPUT);
  pinMode(TFT_CS, OUTPUT);

  switchToNRF();
}


/* Function: loop
 *    main loop program for the slave node - repeats continuously during system operation
 */
void loop() {
  // if more than one minute passed since last sent time display no available shift
  if( ( (millis() - lastSentTime) > 60000 ) ) {
    printCurrentTime();
    lastSentTime = millis();
    
    // save to EEPROM;
    EEPROM.writeString(0, strTextDisplay);
    EEPROM.commit();

    Serial.println("Previous run measurement:");
    Serial.println(lastMeasurements);
    Serial.println();
    
  }

  // for first time print only
  if( lastSentTime == startTime) {
    printCurrentTime();
    lastSentTime = millis();
  }
  radioCheckAndReply();
}

void lcdStartup() {
  TFTscreen.begin();
  TFTscreen.setOrientation(0);
  TFTscreen.setBacklightBrightness(128);
}

void nrfConnect(byte nodeAddress[]) {
  radio.begin();

  radio.setChannel(108);
  radio.setPALevel(RF24_PA_MAX);
  radio.setDataRate(RF24_250KBPS);
  radio.setAutoAck(false);
  radio.enableDynamicPayloads();

  radio.openWritingPipe(nodeAddress);
  radio.openReadingPipe(1, nodeAddress);
  radio.startListening();

//  digitalWrite(LED_PIN,HIGH);
  Serial.println("nRF24L01+ configured...");
}


/* Function: radioCheckAndReply
 *    sends the preloaded node data over the nrf24l01+ radio when
 *    a message is received by the master
 */
String payload = "12345678901234567890123456789012";
void radioCheckAndReply() {
    radio.startListening();
    
    // mock receive attempt
    unsigned long started_waiting_at = millis();
    bool timeout = false;
    while ( !radio.available() && !timeout )
      if (millis() - started_waiting_at > 500)
        timeout = true;

    // mock Send
    
    char payloadChar[32] = "";
    payload.toCharArray(payloadChar, 32);
    radio.write(&payloadChar, sizeof(payloadChar));
}

void printCurrentTime() {
  switchToLCD();
  TFTscreen.clear();

  unsigned long diff = millis() - startTime;
  diff /= 1000;     // convert to seconds
  
  int days = elapsedDays(diff);
  int hours = numberOfHours(diff);
  int minutes = numberOfMinutes(diff);

  strTextDisplay = "";
  strTextDisplay += days;
  strTextDisplay += "D, ";  
  strTextDisplay += getFormatted(hours);
  strTextDisplay += ":";
  strTextDisplay += getFormatted(minutes);
  
  TFTscreen.setGFXFont(&FreeSans9pt7b);
  TFTscreen.getGFXTextExtent(strTextDisplay, x, y, &width, &height); // Get string extents
  x = 10;
  y = 100;
  TFTscreen.drawGFXText(x, y, strTextDisplay, COLOR_YELLOW); // Print string

  switchToNRF();
}

String getFormatted(byte digit) {
  String formatted = "";
  if(digit < 10) formatted += "0";
  return formatted + digit;
}
