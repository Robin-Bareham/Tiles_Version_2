///// REMOVE ALL AUDIO FUNCTIONS AND KEEP GAME FUNCTOONS (SENDING AND RECIEVING DATA)
//// THIS IS WHAT IS CONNECTED TO THE LAPTOP


// yb CODE SENDING AND RECEIVING
#include <Arduino.h>
#include "ESPNowStruct.h"

#include <Adafruit_NeoPixel.h>
#define PIN_NEO_PIXEL 17
#define NUM_PIXELS 61

#include <esp_now.h>  //===============================================ESP
#include <WiFi.h>
#include <vector>
#include <memory>

// 1. THE MAC ADDRESS OF THE 1Tile LED ESP BOARD (=receiver). we send to this address.
uint8_t receiverAddress1[] = { 0xEC, 0xDA, 0x3B, 0x95, 0xC5, 0xC8 }; //Tile Address (ec:da:3b:95:c5:c8)
uint8_t receiverAddress2[] = {0x24,0x58,0x7C,0x65,0x76,0xF8}; //YelloByte Address (24:58:7C:65:76:F8)
uint8_t tileAddress[6];
uint8_t ybAddress[6];

struct_message_all myGame;     // Create an outgoing struct_message from game ESP called myGame
struct_message_all myResults;  // Create an incoming struct_message called myResults

String serialBuffer = "";

//variables incoming and outgoing
bool dataReceived = false;
int airtime = 550;    //default if no data coming in
int buttonInput = 5;  // default, outgoing from yellobyte ESP = 0, 91, 92, 93, 94, 95, 96, 97, 98
int jumpState;    
int jumpCount;
int stepDelay = 500;  // smallest timing unit. 666 would be equivalent to taking 3 steps in 2 seconds at 90bpm
int leftToe;
int leftHeel;
int rightToe;
int rightHeel;
int gameSuccess = 5;

// Delta Time
unsigned long previousTime = 0;
unsigned long deltaTime = 0;

// --- CALLBACK: DATA SENT ---
void OnDataSent(const esp_now_send_info_t *info, esp_now_send_status_t status) {
  Serial.print("\r\nLast Packet Send Status:\t");
  Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

// --- CALLBACK: DATA RECEIVED ---
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  //const uint8_t *mac_addr = info->src_addr;// Extract MAC address of sender
  memcpy(&myResults, incomingData, sizeof(myResults));
  // Serial.print("\r\nBytes received: ");
  // Serial.println(len);
  // Serial.print("From MAC: ");
  // for (int i = 0; i < 6; i++) {
  //   Serial.printf("%02X%s", mac_addr[i], (i < 5) ? ":" : "");
  // }
  // Serial.printf("\nMessage: %s | Value: %d\n", myResults.msg, myResults.value);
  leftToe = myResults.dA; // fill myResults struct with data
  leftHeel = myResults.dB;
  rightToe = myResults.eA;
  rightHeel = myResults.eB;
  gameSuccess = myResults.gB;
  dataReceived = true;
}

//=========================================================================================
void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Register Callbacks using explicit casting to prevent v3.0 errors
  esp_now_register_send_cb((esp_now_send_cb_t)OnDataSent);
  esp_now_register_recv_cb((esp_now_recv_cb_t)OnDataRecv);

  // Register Peer
  //esp_now_peer_info_t peerInfo = {};
  //memcpy(peerInfo.peer_addr, receiverAddress1, 6); // Current Peer is to the tile only 
  
  //Adds Multiple Peers?
  memcpy(tileAddress, receiverAddress1, 6);
  memcpy(ybAddress, receiverAddress2, 6);

  addPeer(receiverAddress1);
  addPeer(receiverAddress2);

  

  //peerInfo.channel = 0;
  //peerInfo.encrypt = false;

  // Add peer to network
  // if (esp_now_add_peer(&peerInfo) != ESP_OK) {
  //   Serial.println("Failed to add peer");
  //   return;
  // }
}  // end of void setup

//=====================================================================================
void loop() {
  //check for messages from game esp (not keyboard) - for game success sounds
  if (dataReceived == true) {
    loadData();  // load gameSuccess data from game esp, sets dataReceived to false again
    Serial.print("gameSuccess is ");
    Serial.println(gameSuccess);
    /*Serial.print(", LT: ");
      Serial.print(leftToe);  //print the number
      Serial.print(", LH: ");
      Serial.print(leftHeel);  //print the number
      Serial.print(", RT: ");
      Serial.print(rightToe);  //print the number
      Serial.print(", RH: ");
      Serial.println(rightHeel);  //print the number
    */
  }
  //possible switch case for calling games

  // Read serial input -> send number on to game ESP to play a game. once done, we play sound.
  // we need to input 91, 92, 93, 95, 97, or 98 to start a game. Do not send 0 to reset
  while (Serial.available() > 0) 
  {
    char c = Serial.read();
    if (c == '\n' || c == '\r') 
    {
      if (serialBuffer.length() > 0) 
      {
        processSerialCommand(serialBuffer);// this is where we process the number
        serialBuffer = "";
      }
    } 
    else 
    {
      serialBuffer += c;
    }
  }
}  // end of loop 

//==============================================================================

void loadData() 
{  //actively load incoming results and sensor values, sets dataReceived = false
  //airtime = myResults.t; 
  //jumpState = myResults.js; 
  //stepDelay is 500ms by default
  //stepDelay = myResults.sd; // update stepDelay-if we wanted to alter this.
  //leftToe = myResults.dA;
  //leftHeel = myResults.dB;
  //rightToe = myResults.eA;
  //rightHeel = myResults.eB;
  //gameSuccess = myResults.gB; 
  dataReceived = false;


  //Temp calling audio if recieving message from Tiles,
  //Shouldn't need this.
  struct_message_all audioMessage;
  switch(gameSuccess)
  {
    case 0:
    case 1:
    case 2:
      audioMessage.id = 6;
      audioMessage.dA = gameSuccess + 1 ; // plays success, partial or fail sound effect
      audioMessage.eB = 70; // Sound effect volume
      sendMessage(ybAddress,audioMessage);
    break;
    case 3:
      
      audioMessage.id = 6;
      audioMessage.dA = 5 ; // plays success, partial or fail sound effect
      audioMessage.eB = 70; // Sound effect volume
      //When music is active use bit bellow instead.
      //audioMessage.dB = // number of celebration music.
      //audioMessage.eA = 30; // background volume
      sendMessage(ybAddress,audioMessage);
    break;

  }
}



void processSerialCommand(String command) 
{
  //Input command that sends either to Tile ESP / Audio ESP or both depending on the input
  command.trim();

  if (command.length() == 0) return;

  // Check if it's a number
  bool isNumber = true;
  for (unsigned int i = 0; i < command.length(); i++) 
  {
    if (!isDigit(command.charAt(i))) 
    {
      isNumber = false;
      break;
    }
  }

  if(isNumber)
  {
    int numInput = command.toInt();
    if(numInput >= 90 && numInput <= 98)
    {
      Serial.println("Sending to Tile");
      struct_message_all gameMessage;
      gameMessage.id = 6; //Game ESP id, i don't know if this is correct
      gameMessage.b = numInput; //The game it's calling upon.
      sendMessage(tileAddress, gameMessage);
      Serial.println("Sending to tile END");
      if(numInput != 90 && numInput != 98 ) //If it's not the reset / celibration
      {
        struct_message_all audioMessage;
        audioMessage.id = 6;
        audioMessage.dA = 7; // Excerise start sound effect
        audioMessage.eB = 70; // Sound effect volume
        sendMessage(ybAddress,audioMessage);
      }
    }
    //For testing
    else if (numInput >= 0 && numInput <= 10)
    {
      Serial.println("Start of sending.");
      //Send Sound effect
      struct_message_all audioMessage;
        audioMessage.id = 6;
        audioMessage.dA = numInput; // Excerise start sound effect
        audioMessage.eB = 70; // Sound effect volume
        sendMessage(ybAddress,audioMessage);
        Serial.println("End of sending.");
    }
  }
}

// ================================ Message Sending

bool sendMessage(const uint8_t *macAddress, struct_message_all &message)
{
  esp_err_t result = esp_now_send(macAddress, (uint8_t *)&message, sizeof(message));
  if(result == ESP_OK)
  {
    Serial.println("Message Sent Successfully");
    return true;
  }
  else
  {
    Serial.print("Error sending message: ");
    Serial.println(result);
    return false;
  }
}

bool addPeer(const uint8_t *macAddress)
{
  esp_now_peer_info_t peerInfo;
    memset(&peerInfo, 0, sizeof(peerInfo));
    peerInfo.channel = 0;
    peerInfo.encrypt = false;
    memcpy(peerInfo.peer_addr, macAddress, 6);

    if(esp_now_add_peer(&peerInfo) != ESP_OK)
    {
        Serial.println("Failed to add peer");
        return false;
    }
    Serial.println("Peer added successfully");
    return true;
}


