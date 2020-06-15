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

// set Chip-Enable (CE) and Chip-Select-Not (CSN) radio setup pins
#define CE_PIN 12
#define CSN_PIN 14

// set LED Pin
#define LED_PIN 32

// set transmission cycle send rate - in milliseconds
#define SEND_RATE 1000

// create RF24 radio object using selected CE and CSN pins
RF24 radio(CE_PIN,CSN_PIN);

#define MAX_NRF_CONNECTION 100

// counter for address amount
int addrAmount;

struct payload {
   String message;
   int messageCount;
};

typedef struct payload Payload;

// String array to store addresses for each sensor node
// used to find index of remoteNodeData array
String strArrNodeAddr[MAX_NRF_CONNECTION];

// array of Payloads for data from each slave node: { message, returned_count }
Payload remoteNodeData[MAX_NRF_CONNECTION];

// system operation timing variables - set SEND_RATE to limit transmit rate
unsigned long currentTime;
unsigned long lastSentTime;

/*********************
  | METHOD DEFINITIONS
*********************/
String getValue(String data, char separator, int index);
void mqttConnect();
void mqttMessageReceived(String &topic, String &payload);
void nrfConnect();
bool callAndReceiveNodeData(String targetQrNodeName, String payload);
void sendStatusToServer(String room_id, String sent, String time);

void setup() {
    // setup serial communications for basic program display
    Serial.begin(115200);
    Serial.println("[*][*][*] Beginning MQTT and nRF24L01+ master-multiple-slave program [*][*][*]");

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
    //bryan-poenya#lalelilolu#broker.shiftr.io#samuelricky-skripsi-coba#sukukata123#2>

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
    //EEPROM.writeString(0,"kingcrimson#lalilulelo#broker.shiftr.io#patrol_system#patrol_system#ESP_Gateway");
    //EPROM.commit();
    String readData = EEPROM.readString(0); 
    Serial.println(readData);

    strSSID = getValue(readData,'#',0); Serial.print("SSID     : "); Serial.println(strSSID);
    strPASS = getValue(readData,'#',1); Serial.print("PASS SSID: ");Serial.println(strPASS);
    strBrokerAdd = getValue(readData,'#',2); Serial.print("Broker Address: ");Serial.println(strBrokerAdd);
    usernameMQTT = getValue(readData,'#',3); Serial.print("username      : ");Serial.println(usernameMQTT);
    passwordMQTT = getValue(readData,'#',4); Serial.print("password      : ");Serial.println(passwordMQTT);
    gatewayName = getValue(readData,'#',5); Serial.print("client name   : ");Serial.println(gatewayName);

    Serial.println("Finish read\n");
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


void mqttConnect() {
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
  Serial.println(LIST_NODENAME_TOPIC);
  client.subscribe(LIST_NODENAME_TOPIC);

  // Resubscribe to subtopic 
  // for receiving qrcode when disconnected
  for(int i = 0; i < addrAmount; i++) {
    String qrSubtopic = LIST_QR_TOPIC + strArrNodeAddr[i];
    client.subscribe(qrSubtopic);
    Serial.print("Subscribed to: "); Serial.println(qrSubtopic); 
  }
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
  
  // check if topic is a /nodeqr subtopic
  else if(topic.indexOf(LIST_QR_TOPIC) > -1) {
    // ensure we dont collect data from nodes faster than selected rate
    currentTime = millis();
    while (currentTime - lastSentTime <= SEND_RATE) {}

    String targetQrNodeName = topic.substring(topic.lastIndexOf('/')+1);
    
    // send and receive sensor data from targeted node
    bool isSent = callAndReceiveNodeData(targetQrNodeName, payload);

    lastSentTime = millis();
    unsigned long diffMicros = lastSentTime - currentTime;
    sendStatusToServer(targetQrNodeName,
                       String(isSent? 1 : 0),
                       String(diffMicros));
  }
}


void nrfConnect() {
  digitalWrite(LED_PIN,LOW);
  Serial.println("Begin nRF24L01+ configuration...");
  // begin radio object
  radio.begin();

  // turn off auto acknowledgement since we'll be using it manually
  radio.setAutoAck(false);
  
  // set power level of the radio
  radio.setPALevel(RF24_PA_LOW);
  
  // set RF datarate - lowest rate for longest range capability
  radio.setDataRate(RF24_250KBPS);
  
  // set time between retries and max no. of retries
  radio.setRetries(4, 5);
  
  // enable ack payload - each slave replies with sensor data using this feature
  radio.enableAckPayload();

  // print radio config details to console
  radio.printDetails();

  digitalWrite(LED_PIN,HIGH);
  Serial.println("nRF24L01+ configured...");
}

/* Function: callAndReceiveNodeData
 *    Make a radio call to each node in turn and retreive a message from each
 */
bool callAndReceiveNodeData(String targetQrNodeName, String payload) {
    byte byteArrTargetQrNodeName[targetQrNodeName.length()];
    targetQrNodeName.getBytes(byteArrTargetQrNodeName, targetQrNodeName.length());
    
    radio.openWritingPipe(byteArrTargetQrNodeName);
    radio.openReadingPipe(1, byteArrTargetQrNodeName);
    
    radio.stopListening();
    
    Serial.print("[*] Attempting to transmit data to node ");
    Serial.println(targetQrNodeName);
    Serial.print("[*] The payload being sent is: ");
    Serial.println(payload);
    
    // boolean to indicate if radio.write() tx was successful
    bool tx_sent;
    tx_sent = radio.write(&payload, sizeof(payload));
    
    // if tx success - receive and read slave node ack reply
    if (tx_sent) {
        if (radio.isAckPayloadAvailable()) {
            
            int matchedIdx = 0;
            for(matchedIdx; matchedIdx < addrAmount; matchedIdx++) {
               if(targetQrNodeName == strArrNodeAddr[matchedIdx])
                  break;
            }
            // read ack payload and copy data to relevant remoteNodeData array
            radio.read(&remoteNodeData[matchedIdx], sizeof(remoteNodeData[matchedIdx]));
            
            Serial.print("[+] Successfully received data from node: ");
            Serial.print(targetQrNodeName + " with index of ");
            Serial.println(matchedIdx);
            Serial.print("  ---- The node count received was: ");
            Serial.println(remoteNodeData[matchedIdx].messageCount);
            Serial.print("  ---- The message was: ");
            Serial.println(remoteNodeData[matchedIdx].message);
        }
    }
    else {
        Serial.println("[-] The transmission to the selected node failed.");
    }

    Serial.println("--------------------------------------------------------");
    return tx_sent;
}


void sendStatusToServer(String room_id, String sent, String time)
{
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
  if (httpResponseCode>0) {
    Serial.print("HTTP Response code: ");
    Serial.println(httpResponseCode);
  }
  else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();
}
