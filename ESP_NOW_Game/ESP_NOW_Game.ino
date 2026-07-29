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


struct_message_all myResults;  // Message that hold the data sent to this ESP
struct_message_all audioMessage; //Message that gets sent to the YB ESP
struct_message_all tileMessage; //Message that gets sent to the Tile ESP

//Base volumes of background music and sound effects, 1 to 101. 1 being muted 101 being 100%
//because when it sends to yb it -1 on the value passed in.
int bg_volume = 60; 
int sfx_volume = 101;

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
int balanceScore = 0;
int legScore = 0;
int lungeScore = 0;

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
  // Load Data into variablea
  airtime = myResults.t; 
  jumpState = myResults.js; 
  //stepDelay is 500ms by default
  stepDelay = myResults.sd; // update stepDelay-if we wanted to alter this.
  leftToe = myResults.dA; // fill myResults struct with data
  leftHeel = myResults.dB;
  rightToe = myResults.eA;
  rightHeel = myResults.eB;
  gameSuccess = myResults.gB;
  //Scores
  balanceScore = myResults.fA; //Score from Calib 2 (Balance)
  legScore = myResults.fB; //Score from Calib 3 (Leg Lift)
  lungeScore = myResults.gA; //Score from Calib 5 (Lunge)
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
  
  //Adds Multiple Peers
  memcpy(tileAddress, receiverAddress1, 6);
  memcpy(ybAddress, receiverAddress2, 6);

  addPeer(receiverAddress1);
  addPeer(receiverAddress2);
  //Sends message to YB to start music (intro that'll lead to main menu)
  sendAudioMsg(0,1,0,bg_volume,0);
}  // end of void setup

//=====================================================================================
void loop() {
  //check for messages from game esp (not keyboard) - for game success sounds
  if (dataReceived == true) {
    loadData();  // load gameSuccess data from game esp, sets dataReceived to false again
    
  }
  //possible switch case for calling games

  //Serial Monitor Input
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
{  
  //This is where you do what you want with the data received
  
  //Prints out DATA information
  Serial.print("gameSuccess: ");
  Serial.print(gameSuccess);
  Serial.print(", balanceScore: ");
  Serial.print(balanceScore);
  Serial.print(", legScore: ");
  Serial.print(legScore);
  Serial.print(", lungeScore: ");
  Serial.println(lungeScore);
  Serial.print("LT: ");
  Serial.print(leftToe); 
  Serial.print(", LH: ");
  Serial.print(leftHeel);  
  Serial.print(", RT: ");
  Serial.print(rightToe);  
  Serial.print(", RH: ");
  Serial.println(rightHeel); 

  dataReceived = false;
}

void processSerialCommand(String command) 
{
  // 91,92,93,95,96,97,98 Start a game
  // 90 resets, 94 does nothing
  // 0 activates main menu (Currently music and nothing else, doesn't reset)
  // 1 to 10 play a sound effect from the YB <- can be removed if calling of Simon Says is implimented as well
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
    //Starting Exercises
    if(numInput >= 90 && numInput <= 98)
    {
      // Tell Tile to Start Exercise
      tileMessage.b = numInput; //Tells the tile ESP which game it's running.
      sendMessage(tileAddress, tileMessage);
      // Tell YB to start sfx and bg music
      audioMessage.eB = bg_volume; // BG volume
      audioMessage.eA = sfx_volume; // Sfx volume
      if(numInput != 90 && numInput != 98 && numInput != 94 ) //If it's not the reset / celibration / Agility
      {
        audioMessage.dA = 7; // Excerise start sound effect      
      }
      switch (numInput)
      {
        //reset
        case 90:
        case 94:
          audioMessage.dB = 2; //Silence
        break;
        //Marching
        case 91:
          audioMessage.dB = 6; //Background Music
        break;
        //Balance and Leg Lift
        case 92:
        case 93:
        case 97:
          audioMessage.dB = 7;
        break;
        //Lunge and Squats
        case 95:
        case 96:
          audioMessage.dB = 5;
        break;
        //Celibration
        case 98:
          audioMessage.dB = 9;
          audioMessage.gB = 1; //Doesn't loop
        break;
      }
      sendMessage(ybAddress,audioMessage);
    }
    //Main Menu, doesn't reset anything.
    else if(numInput == 0)
    {
      sendAudioMsg(numInput,4,0,bg_volume,0);
      tileMessage.b = numInput; //Tells it it's in the main menu.
      sendMessage(tileAddress, tileMessage);
    }
    //For testing, gets the yb to play a sound effect
    else if (numInput >= 1 && numInput <= 10)
    {
      sendAudioMsg(numInput,0,sfx_volume,0,0);
    }
  }
}

// ================================ Message Sending

//Quick way of sending audio messages
void sendAudioMsg(int p_sfx, int p_bg, int p_sfxVol, int p_bgVol, int p_bgLoop)
{
  audioMessage.dA = p_sfx;
  audioMessage.dB = p_bg;
  audioMessage.eA = p_sfxVol;
  audioMessage.eB = p_bgVol;
  audioMessage.gB = p_bgLoop;
  sendMessage(ybAddress, audioMessage);
}

//Function to send a message to a passed in address
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

//allows multiple peers to be added easily.
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


