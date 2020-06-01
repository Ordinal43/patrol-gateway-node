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
#include <RF24.h>
#include <printf.h>

// chip select and RF24 radio setup pins
#define CE_PIN 17
#define CSN_PIN 4
RF24 radio(CE_PIN,CSN_PIN);

// check how many times mesage has been received 
int messageCount = 0;
// String to store payload from master
String dataFromMaster = "";

struct payload {
   String message;
   int messageCount;
};

typedef struct payload Payload;

/*********************
  | SCREEN TFT ILI9225 176*220
*********************/
#include "SPI.h"
#include "TFT_22_ILI9225.h"
#include <../fonts/FreeSans9pt7b.h>
#include <../fonts/FreeSans12pt7b.h>
#define TFT_RST 26
#define TFT_RS  25
#define TFT_CLK 18
#define TFT_SDI 23
#define TFT_CS  5
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

#define LEDPIN 2

/*********************
  | METHOD DEFINITIONS
*********************/
String getValue(String data, char separator, int index);
void lcdStartup();
void nrfConnect(byte nodeAddress[]);
void radioCheckAndReply();
void printQR(String strData);

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
  Serial.println("[*][*][*] Beginning nRF24L01+ ack-payload slave device program [*][*][*]");

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
  
  pinMode(LEDPIN,OUTPUT);
  digitalWrite(LEDPIN,LOW);

  Serial.println("Read data from EEPROM");
  String readData = EEPROM.readString(0); 
  Serial.println(readData);

  strAddress = getValue(readData,'>',0); Serial.print("Node address  : "); Serial.println(strAddress);
  
  Serial.println("Finish read\n");

  byte byteArrNodeAddress[strAddress.length()];
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
  TFTscreen.setOrientation(2);
  TFTscreen.setBacklightBrightness(128);
}

void nrfConnect(byte nodeAddress[]) {
  radio.begin();
  
  // set power level of the radio
  radio.setPALevel(RF24_PA_LOW);

  // set RF datarate
  radio.setDataRate(RF24_250KBPS);

  radio.openWritingPipe(nodeAddress);
  radio.openReadingPipe(1, nodeAddress);     

  radio.enableAckPayload();

  Payload payloadReturn;
  payloadReturn.message = "Connected!";
  payloadReturn.messageCount = messageCount;
  
  radio.writeAckPayload(1, &payloadReturn, sizeof(payloadReturn));

  // print radio config details to console
  printf_begin();
  radio.printDetails();

  // start listening on radio
  radio.startListening();

  digitalWrite(LEDPIN,HIGH);
}


/* Function: radioCheckAndReply
 *    sends the preloaded node data over the nrf24l01+ radio when
 *    a message is received by the master
 */
void radioCheckAndReply() {
    // check for radio message and send sensor data using auto-ack
    if ( radio.available() ) {
          radio.read( &dataFromMaster, sizeof(dataFromMaster) );
          Serial.println("Received request from master - sending preloaded data.");
          Serial.print("The received payload from the master was: ");
          Serial.println(dataFromMaster);
          Serial.println("--------------------------------------------------------");

          printQR(dataFromMaster);
          
          if (messageCount < 500) messageCount++;
          else messageCount = 1;
          
          Payload payloadReturn;
          payloadReturn.message = "Received!";
          payloadReturn.messageCount = messageCount;
          
          radio.writeAckPayload(1, &payloadReturn, sizeof(payloadReturn));
    }
    delay(1000);
}

void printQR(String strData) {
  // Temporarily switch SPI bus to LCD 
  switchToLCD();
  
  TFTscreen.clear();

  if(strData!=""){
    /**********************
    | QR Code             |
    **********************/
    strData.toCharArray(dataCharArray, sizeof(dataCharArray));
    // Print to TFT
    uint8_t qrcodeData[qrcode_getBufferSize(3)];
    qrcode_initText(&qrcode, qrcodeData, vers, 0, dataCharArray); // your text in last parameter, e.g. "Hello World"

    //make border
    TFTscreen.fillRectangle((offset_x - borderWidth), (offset_y - borderWidth), ((qrcode.size * pixel) + offset_x + borderWidth), offset_y, COLOR_WHITE); //square1 horizontal atas
    TFTscreen.fillRectangle((offset_x - borderWidth), offset_y, offset_x, ((qrcode.size * pixel) + offset_y), COLOR_WHITE); //square2 vertical kiri
    TFTscreen.fillRectangle((offset_x - borderWidth), ((qrcode.size * pixel) + offset_y), ((qrcode.size * pixel) + offset_x + borderWidth), ((qrcode.size * pixel) + offset_y + borderWidth), COLOR_WHITE); //square3 horizontal bawah
    TFTscreen.fillRectangle(((qrcode.size * pixel) + offset_x), offset_y, ((qrcode.size * pixel) + offset_x + borderWidth), ((qrcode.size * pixel) + offset_y), COLOR_WHITE); //square4

    for (uint8_t y = 0; y < qrcode.size; y++) { //vertical
      for (uint8_t x = 0; x < qrcode.size; x++) { //horizontal
        if (!qrcode_getModule(&qrcode, x, y)) {
          uint16_t x1 = (x * pixel) + offset_x;
          uint16_t y1 = (y * pixel) + offset_y;
          uint16_t x2 = (x1 + pixel - 1);
          uint16_t y2 = (y1 + pixel - 1);
          TFTscreen.fillRectangle(x1, y1, x2, y2, COLOR_WHITE);
        }
      }
    }
  }

  strTextDisplay = "Slave Node"; // Create string object
  TFTscreen.setGFXFont(&FreeSans9pt7b);
  TFTscreen.getGFXTextExtent(strTextDisplay, x, y, &width, &height); // Get string extents
  x = 10;
  y = 160 + height; // Set y position to string height plus shift down 10 pixels
  TFTscreen.drawGFXText(x, y, strTextDisplay, COLOR_WHITE); // Print string

  strTextDisplay = "UKDW"; // Create string object
  TFTscreen.getGFXTextExtent(strTextDisplay, x, y, &width, &height); // Get string extents
  x = 10;
  y += height + 5; // Set y position to string height plus shift down 10 pixels
  TFTscreen.drawGFXText(x, y, strTextDisplay, COLOR_ORANGE); // Print string

  strTextDisplay = "Patrolee System"; // Create string object
  //TFTscreen.setGFXFont(&FreeSans9pt7b);
  TFTscreen.getGFXTextExtent(strTextDisplay, x, y, &width, &height); // Get string extents
  x = 10;
  y += height + 5; // Set y position to string height plus shift down 10 pixels
  TFTscreen.drawGFXText(x, y, strTextDisplay, COLOR_CYAN); // Print string

  // switch back to NRF
  switchToNRF();
}
