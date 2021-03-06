/*********************
  | WIFI ESP32
*********************/
#include <WiFi.h>
//const char ssid[] = "kingcrimson";
//const char pass[] = "lalilulelo";
WiFiClient net;

/*********************
  | API REQUEST
*********************/
#include <HTTPClient.h>
String serverName = "http://192.168.43.140:8000";

/*********************
  | MQTT CLIENT
*********************/
#include <MQTT.h>
#include <MQTTClient.h>
MQTTClient client;
String LIST_NODENAME_TOPIC;
String LIST_QR_TOPIC = "/qrcode/";

/*********************
  | EEPROM
*********************/
#include "EEPROM.h"
#define EEPROM_SIZE 128
String strSSID;
String strPASS;
String strBrokerAdd;
String usernameMQTT;
String passwordMQTT;
String gatewayName;

char buff1[10];
char buff2[25];
char buff3[50];
char buff4[150];

/*********************
  | nRF24L01+
*********************/
#include <SPI.h>
#include <nRF24L01.h>
#include <RF24.h>
#include <printf.h>

#define CE_PIN 12
#define CSN_PIN 14
RF24 radio(CE_PIN,CSN_PIN);

#define MAX_NRF_CONNECTION 100

// counter for address amount
int addrAmount;

// String array to store addresses for each sensor node
String strArrNodeAddr[MAX_NRF_CONNECTION];

unsigned long currentTime;
unsigned long lastSentTime;

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
  | METHOD DEFINITIONS
*********************/
String getValue(String data, char separator, int index);
void lcdStartup();
void printWelcome(String gatewayName);
void mqttConnect();
void mqttMessageReceived(String &topic, String &payload);
void nrfConnect();
bool callAndReceiveNodeData(String targetQrNodeName, String payload);
void sendStatusToServer(String room_id, String sent, String time);

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
    Serial.println("[*][*][*] Beginning MQTT and nRF24L01+ gateway device program [*][*][*]");

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

    //strSSID#strPASS#Broker_Add#usernameMQTT#passwordMQTT#gatewayName>
    //bryan-poenya#lalelilolu#broker.shiftr.io#patrol_system#patrol_system#ESP_Gateway_1>
    //ricky#abcdefgh#broker.shiftr.io#samuelricky-skripsi-coba#sukukata123#2>

    Serial.println("");
    String outMessage = "";
    while(Serial.available()>0){
      char inChar = Serial.read();
      if(inChar == '>'){
        Serial.println("Receive serial data");
        Serial.println(outMessage);
        Serial.println("Write to EEPROM");
        EEPROM.writeString(0, outMessage);
        EEPROM.commit();
        Serial.println("Writing data success");
        break;
      }
      else
        outMessage.concat(inChar);
    }

    pinMode(LED_PIN,OUTPUT);
    digitalWrite(LED_PIN,LOW);

    Serial.println("Read data from EEPROM");
    String readData = EEPROM.readString(0); 
    Serial.println(readData);

    strSSID = getValue(readData,'#',0); Serial.print("SSID     : "); Serial.println(strSSID);
    strPASS = getValue(readData,'#',1); Serial.print("PASS SSID: ");Serial.println(strPASS);
    strBrokerAdd = getValue(readData,'#',2); Serial.print("Broker Address: ");Serial.println(strBrokerAdd);
    usernameMQTT = getValue(readData,'#',3); Serial.print("username      : ");Serial.println(usernameMQTT);
    passwordMQTT = getValue(readData,'#',4); Serial.print("password      : ");Serial.println(passwordMQTT);
    gatewayName = getValue(readData,'#',5); Serial.print("client name   : ");Serial.println(gatewayName);

    Serial.println("Finish read\n");

    lcdStartup();
    pinMode(CSN_PIN, OUTPUT);
    pinMode(TFT_CS, OUTPUT);
    printWelcome(gatewayName);
    
    strSSID.toCharArray(buff2,(strSSID.length()+1));
    Serial.println(buff2);
    strPASS.toCharArray(buff1,(strPASS.length()+1));
    Serial.println(buff1);
    WiFi.begin(buff2, buff1);

    // Note: Local domain names (e.g. "Computer.local" on OSX) are not supported by Arduino.
    // You need to set the IP address directly.
    //client.begin("192.168.43.182", net); //mosquitto broker
    strBrokerAdd.toCharArray(buff2,(strBrokerAdd.length()+1));
    client.begin(buff2, net);
    client.onMessage(mqttMessageReceived);
    
    mqttConnect();
    nrfConnect();
}

void loop() {
  // wait for any configuration payload from MQTT
  client.loop();
  if (!client.connected()) {
    mqttConnect();
  }
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

void printWelcome(String gatewayName) {
  switchToLCD();
  TFTscreen.clear();

  strTextDisplay = "Gateway " + gatewayName;
  TFTscreen.setGFXFont(&FreeSans9pt7b);
  TFTscreen.getGFXTextExtent(strTextDisplay, x, y, &width, &height);
  x = 10;
  y = 100;
  TFTscreen.drawGFXText(x, y, strTextDisplay, COLOR_GREEN);
  
  switchToNRF();
}


void mqttConnect() {
  Serial.println("Begin MQTT config...");
  
  Serial.print("checking wifi...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }
  Serial.print("WiFi connected");
  Serial.print("\n" + strBrokerAdd + " connecting...");

  gatewayName.toCharArray(buff2,gatewayName.length()+1);
  usernameMQTT.toCharArray(buff3,usernameMQTT.length()+1);
  passwordMQTT.toCharArray(buff4,passwordMQTT.length()+1);
  
  while (!client.connect(buff2, buff3, buff4)) {
    Serial.print(".");
    delay(1000);
  }

  Serial.println("\nbroker connected!");

  LIST_NODENAME_TOPIC = "/config-gateway/" + gatewayName;
  client.subscribe(LIST_NODENAME_TOPIC);
  Serial.print("Subscribed to: "); Serial.println(LIST_NODENAME_TOPIC);

  // Resubscribe to subtopic 
  // for receiving qrcode when disconnected
  for(int i = 0; i < addrAmount; i++) {
    String qrSubtopic = LIST_QR_TOPIC + strArrNodeAddr[i];
    client.subscribe(qrSubtopic);
    Serial.print("Subscribed to: "); Serial.println(qrSubtopic); 
  }

  Serial.println("MQTT configured...");
  Serial.println();
}

void mqttMessageReceived(String &topic, String &payload) {
  Serial.println("incoming: " + topic + " - " + payload);
  if(topic == LIST_NODENAME_TOPIC){

    //nodeAddr1#nodeAddr2#nodeAddr3#....#nodeAddr[n]
    //NODE1#NODE2#NODE3#...#NODE[N]
    
    // count how many occurences of '#' to determine array size
    addrAmount = 1;
    for(int i = 0; i < payload.length(); i++) {
      if(payload[i] == '#') addrAmount++;
    }
    Serial.print("Received address amount : "); Serial.println(addrAmount);
    
    for(int i = 0; i < addrAmount; i++) {
      String tempNodeAddr = getValue(payload,'#',i);
      Serial.print("Node QR address : "); Serial.println(tempNodeAddr);
      // store QR node address in array for index checking later
      strArrNodeAddr[i] = tempNodeAddr;
    }

    // reconnect with new topic
    client.disconnect();
    mqttConnect();
  } 
  
  else if(topic.startsWith(LIST_QR_TOPIC)) {
    // ensure we dont collect data from nodes faster than selected rate
    String targetQrNodeName = topic.substring(8);
    Serial.println("Send to: " + targetQrNodeName);
    Serial.println("Payload: " + payload);
    // send and receive sensor data from targeted node

    lastSentTime = micros();
    bool isSent = callAndReceiveNodeData(targetQrNodeName, payload);

    unsigned long diffMicros = micros() - lastSentTime;
//    sendStatusToServer(targetQrNodeName,
//                       String(isSent? 1 : 0),
//                       String(diffMicros));
  }
}


void nrfConnect() {
  digitalWrite(LED_PIN,LOW);
  Serial.println("Begin nRF24L01+ config...");

  radio.begin();
  radio.setChannel(108);
  radio.setPALevel(RF24_PA_LOW);
  radio.setDataRate(RF24_250KBPS);
  radio.setRetries(4, 5);
  radio.setAutoAck(false);
  radio.enableDynamicPayloads();
  
  radio.printDetails();

  digitalWrite(LED_PIN, HIGH);
  Serial.println("nRF24L01+ configured...");
  Serial.println();
}

/* Function: callAndReceiveNodeData
 *    Make a radio call to each node in turn and retreive a message from each
 */
bool callAndReceiveNodeData(String targetQrNodeName, String payload) {
    targetQrNodeName = targetQrNodeName + "node";
    byte byteArrTargetQrNodeName[7];
    targetQrNodeName.getBytes(byteArrTargetQrNodeName, targetQrNodeName.length());
    
    radio.openWritingPipe(byteArrTargetQrNodeName);
    radio.openReadingPipe(1, byteArrTargetQrNodeName);
    radio.stopListening();

    char payloadChar[32] = "";
    payload.toCharArray(payloadChar, 32);

    Serial.print("[*] Attempting to transmit data to node ");
    Serial.println(targetQrNodeName);
    Serial.print("[*] The payload being sent is: ");
    Serial.println(payload);
    
    bool tx_sent;
    char ackPayload[10] = "";
    tx_sent = radio.write(&payloadChar, sizeof(payloadChar));
    
    if (tx_sent) {
        // wait for ack packet
        radio.startListening();

        unsigned long started_waiting_at = millis();
        bool timeout = false;
        while ( !radio.available() && !timeout )
          if (millis() - started_waiting_at > 500)
            timeout = true;
            
        if(!timeout){
            radio.read( &ackPayload, sizeof(ackPayload) );
            Serial.print("[+] Successfully sent first part to node: ");
            Serial.println(targetQrNodeName);
            Serial.print("  ---- Acknowledgement message  (1/2): ");
            Serial.println(ackPayload);

            radio.stopListening();
            String payload2 = payload.substring(32-1);
            payload2.toCharArray(payloadChar, 32);
            tx_sent = radio.write(&payloadChar, sizeof(payloadChar));
      
            if(tx_sent) {
                radio.startListening();

                started_waiting_at = millis();
                while ( !radio.available() && !timeout )
                  if (millis() - started_waiting_at > 500)
                    timeout = true;
                    
                if (!timeout) {
                  radio.read( &ackPayload, sizeof(ackPayload) );
                  Serial.print("[+] Successfully completed transfer to node: ");
                  Serial.println(targetQrNodeName);
                  Serial.print("  ---- Acknowledgement message (2/2): ");
                  Serial.println(ackPayload);
                  
                  radio.stopListening();
                  Serial.println("\n\n--------------------------------------------------------");
                  return tx_sent;
                }
            }
        }
    }

    Serial.println("[-] The transmission to the selected node failed.");
    Serial.println("\n\n--------------------------------------------------------");
    return tx_sent;
}


void sendStatusToServer(String room_id, String sent, String time)
{
  Serial.println("Sending status to server");
  HTTPClient http;

  String serverPath = serverName + "/api/acknowledges";

  http.begin(serverPath.c_str());
  http.addHeader("Content-Type", "application/json");

  //String httpRequestData = "room_id=" + room_id + "&sent=" + sent + "&time" + time;
  String httpRequestData = "{\"room_id\":\"" + room_id +"\",\"sent\":\"" + sent + "\",\"time\":\"" + time + "\"}";
  
  // Send HTTP GET request
  int httpResponseCode = http.POST(httpRequestData);
  String payload = http.getString();
  Serial.println(payload);
  if (httpResponseCode>0){
    Serial.println("[+] HTTP Request successful.");
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
  }
  else {
    Serial.println("[-] HTTP Request failed.");
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();
}
