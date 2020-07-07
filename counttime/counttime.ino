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

/*********************
  | QR CODE
*********************/
#include "qrcode.h"
QRCode qrcode;
uint8_t vers = 3;
uint8_t pixel = 5;
uint8_t offset_x = 15;
uint8_t offset_y = 5;
uint8_t borderWidth = 5;
char dataCharArray[100];
String strQrcode="";

#define LED_PIN 32

// To store what time the last data is sent
unsigned long lastSentTime = millis();
bool shiftEmptyShown = false;
String qrStr = "AgIyv5iQR5UBQUBX4hMfHLEtcwnPBif8mI2yOikJgKs=";

/*********************
  | METHOD DEFINITIONS
*********************/
String getValue(String data, char separator, int index);
void lcdStartup();
void nrfConnect(byte nodeAddress[]);
void radioCheckAndReply();
void printNoShift();
void printFailed();

void switchToNRF() {
  digitalWrite(CSN_PIN,HIGH);
  digitalWrite(TFT_CS,HIGH);
}

void switchToLCD() {
  digitalWrite(CSN_PIN,LOW);
  digitalWrite(TFT_CS,LOW); 
}

void setup() {

  // setup serial communications for basic program display
  Serial.begin(115200);
  Serial.println("[*][*][*] Beginning nRF24L01+ ack-payload node device program [*][*][*]");

  printf_begin();
  
  if (!EEPROM.begin(200)) {
    Serial.println("Failed to initialise EEPROM");
    Serial.println("Restarting...");
    delay(1000);
    ESP.restart();
  }

  byte counter = 0;
  Serial.print("Waiting serial data in 10 seconds");
  while(!Serial.available()) {
    Serial.print('.');
    delay(1000);
    counter++;
    if(counter>=10) break;
  }

  // strAddress>
  // QR_Node>
    
  Serial.println("");
  String outMessage = "";
  while(Serial.available()>0){
    char inChar = Serial.read();
    outMessage.concat(inChar);
    if(inChar == '>'){
      Serial.println("Receive serial data");
      Serial.println(outMessage);
      Serial.println("Write to EEPROM");
      EEPROM.writeString(0,outMessage);
      EEPROM.commit();
      Serial.println("Writing data success");
      break;
    }
  }
  
  pinMode(LED_PIN,OUTPUT);
  digitalWrite(LED_PIN,LOW);

  Serial.println("Read data from EEPROM");
  String readData = EEPROM.readString(0); 
  Serial.println(readData);

  strAddress = getValue(readData,'>',0);
  strAddress = strAddress.substring(0, strAddress.length()-1);
  strAddress = strAddress + "node";
  Serial.print("Node address  : "); Serial.println(strAddress);
  
  Serial.println("Finish read\n");

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
unsigned long startTime = millis();
void loop() {
  // if more than one minute passed since last sent time display no available shift
  if( ( (millis() - lastSentTime) > 60000 ) && !shiftEmptyShown ) {
    printCurrentTime();
    lastSentTime = millis();
  }
  radioCheckAndReply();
}

String getValue(String data, char separator, int index) {
  int found = 0;
  int strIndex[] = {0, -1};
  int maxIndex = data.length()-1;

  for(int i=0; i<=maxIndex && found<=index; i++){
    if(data.charAt(i)==separator || i==maxIndex){
        found++;
        strIndex[0] = strIndex[1]+1;
        strIndex[1] = (i == maxIndex) ? i+1 : i;
    }
  }

  return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
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

  radio.printDetails();

  radio.openWritingPipe(nodeAddress);
  radio.openReadingPipe(1, nodeAddress);
  radio.startListening();

  digitalWrite(LED_PIN,HIGH);
  Serial.println("nRF24L01+ configured...");
}


/* Function: radioCheckAndReply
 *    sends the preloaded node data over the nrf24l01+ radio when
 *    a message is received by the master
 */

bool firstPartSent = false;
char message[10] = "Received!";
char completeData[70] = "";

void radioCheckAndReply() {
    radio.startListening();
    
    unsigned long started_waiting_at = millis();
    bool timeout = false;
    while ( !radio.available() && !timeout )
      if (millis() - started_waiting_at > 500)
        timeout = true;
}

void printCurrentTime() {
  switchToLCD();
  TFTscreen.clear();

  strTextDisplay = strAddress;
  TFTscreen.setGFXFont(&FreeSans9pt7b);
  TFTscreen.getGFXTextExtent(strTextDisplay, x, y, &width, &height); // Get string extents
  x = 10;
  y = 100;
  TFTscreen.drawGFXText(x, y, strTextDisplay, COLOR_YELLOW); // Print string
  
  switchToNRF();
}

void printFailed() {
  switchToLCD();
  TFTscreen.clear();

  strTextDisplay = strAddress; // Create string object
  strTextDisplay.toUpperCase();
  TFTscreen.setGFXFont(&FreeSans9pt7b);
  TFTscreen.getGFXTextExtent(strTextDisplay, x, y, &width, &height); // Get string extents
  x = 10;
  y = 100;
  TFTscreen.drawGFXText(x, y, strTextDisplay, COLOR_CYAN); // Print string

  strTextDisplay = "RECEIVE FAILURE"; // Create string object
  TFTscreen.getGFXTextExtent(strTextDisplay, x, y, &width, &height); // Get string extents
  x = 10;
  y += height + 10; // Set y position to string height plus shift down 10 pixels
  TFTscreen.drawGFXText(x, y, strTextDisplay, COLOR_RED); // Print string
  
  switchToNRF();
}
