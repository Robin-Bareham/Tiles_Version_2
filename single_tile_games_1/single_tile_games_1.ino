#include <Adafruit_NeoPixel.h>  //---------------------------------------------------------------------------NEOPIXEL
#define PIN 1                   //define pin on esp32-s3 the neopixes is connected to
#define NUMPIXELS 60            // NeoPixel strip size
Adafruit_NeoPixel pixels(NUMPIXELS, PIN, NEO_GRB + NEO_KHZ800);
#define DELAYVAL 300  // Time (in milliseconds) to pause between pixels

Adafruit_NeoPixel stripArray[] = { pixels };  // array with 1 element
Adafruit_NeoPixel lightToCheck;               // use this object to address a light tile by its number in stripArray[]
Adafruit_NeoPixel tileOff;                    // to address a lit tile with strip[].clear, strip[].show

// ======== TILE COLOURS =========

uint32_t levColor = pixels.Color(255, 255, 255);  //white as example, later redefined
uint32_t lime = pixels.Color(105, 255, 10);       //not used for level
uint32_t cyan = pixels.Color(0, 255, 255);        //used for level 1
uint32_t blue = pixels.Color(0, 0, 255);          //used for  level 2
uint32_t purple = pixels.Color(85, 0, 255);       //used for level 3
uint32_t magenta = pixels.Color(255, 0, 255);     // used for level 4
uint32_t orange = pixels.Color(255, 60, 0);       // used for level 5
uint32_t left = pixels.Color(0, 255, 255);        //colour used for LEFT FOOT cyan
uint32_t right = pixels.Color(255, 0, 255);       // colour used for RIGHT FOOT magenta
uint32_t green = pixels.Color(0, 150, 0);         //used for goodJump
uint32_t yellow = pixels.Color(155, 150, 0);      //used for partialJump
uint32_t red = pixels.Color(255, 0, 0);           // used for badJump
uint32_t white = pixels.Color(200, 200, 200);     // used for endJump
uint32_t black = pixels.Color(0, 0, 0);           // used to switch off??

// Define the typical strucure of a LED Segment (applies then to all tiles)
struct Segment {
  int start;  //start LED number is...
  int count;  //how many LEDs from start LED
};
// Define the tile LED segments globally
Segment righthalftile[] = {
  //used in calib4 for the LEFT foot
  { 10, 10 },  // Segment 1
  { 50, 10 },  // Segment 2
  { 40, 11 }   // Segment 3
};
Segment lefthalftile[] = {
  //used in calib4 for the RIGHT foot
  { 20, 20 },  // Segment 1
  { 50, 10 }   // Segment 2
};
// Q1  Q2      layout of quartals per tile
// Q3  Q4
Segment Q1[] = {
  { 20, 10 },  // Segment 1; 10 or 11?
  { 50, 5 },   // Segment 2
  { 0, 5 }     // Segment 3
};
Segment Q2[] = {
  { 10, 10 },  // Segment 1
  { 50, 5 },   // Segment 2
  { 5, 5 }     // Segment 3
};
Segment Q3[] = {
  { 0, 5 },    // Segment 1
  { 30, 10 },  // Segment 2
  { 50, 5 }    // Segment 3
};
Segment Q4[] = {
  { 5, 5 },    // Segment 1
  { 40, 10 },  // Segment 2
  { 55, 5 }    // Segment 3
};
Segment cross[] = {
  { 0, 10 },   // Segment 1
  { 50, 10 },  // Segment 2
};
Segment outline[] = {
  { 10, 40 },  // Segment 1
};
// Create an array of pointers to the 8 segment arrays defined above
Segment *segmentName[] = {
  righthalftile,
  lefthalftile,
  Q1,
  Q2,
  Q3,
  Q4,
  cross,
  outline
};
// Array to store the number of segments within each of the 8 segment arrays
int segmentCounts[] = {
  sizeof(righthalftile) / sizeof(righthalftile[0]),
  sizeof(lefthalftile) / sizeof(lefthalftile[0]),
  sizeof(Q1) / sizeof(Q1[0]),
  sizeof(Q2) / sizeof(Q2[0]),
  sizeof(Q3) / sizeof(Q3[0]),
  sizeof(Q4) / sizeof(Q4[0]),
  sizeof(cross) / sizeof(cross[0]),
  sizeof(outline) / sizeof(outline[0])
};
class LightController {  // to determine which tile(s) and strip(s) to set the color on
public:
  void lightTile(uint32_t color, int index) {
    //Serial.print("Filling light at position ");
    //Serial.print(index);
    //Serial.print(" colour ");
    //Serial.println(color);
    // Determine which strip to set the color on
    int stripIndex = index / NUMPIXELS;  // Assuming NUMPIXELS is the number of LEDs per strip
    int pixelIndex = index % NUMPIXELS;  // Get the pixel index within the strip
    if (stripIndex < 1) {                // Ensure we don't go out of bounds, we only have 1 element at pos 0
      stripArray[stripIndex].setPixelColor(pixelIndex, color);
    }
  }
};
//int pixelIndex = 0; already defined in LightController
LightController lightToLoad;
int tileSegment;  // this allows us to choose one of the 8 predefined tile segments

#include <esp_now.h>  //now the ESP-NOW stuff................................................................ESP NOW
#include <WiFi.h>
uint8_t receiverAddress1[] = {  0x24, 0x58, 0x7C, 0x65, 0x76, 0xF8 };  //we send to yellobyte esp for sounds
uint8_t receiverAddress2[] = { 0xEC, 0xDA, 0x3B, 0x96, 0xEA, 0xB0 }; //Game ESP reciever (ec:da:3b:96:ea:b0)
uint8_t gameAddress[6];
uint8_t ybAddress[6];



//variables incoming and outgoing data
bool dataReceived = false;
int airtime = 550;    //default if no data coming in
int buttonInput = 5;  //default; incoming from yellobyte ESP = gamelevel 0,1,2,3,4,5, 91, 92, 93, 94, 95, 96, 97, 98
int jumpState = 5;    //dummy state, nothing happens
int leftToe;
int leftHeel;
int rightToe;
int rightHeel;

//Calibration exercises variables........................................................................VARIABLES
int stepDelay = 500;                   // smallest timing unit. 666 would be equivalent to taking 3 steps in 2 seconds at 90bpm
int stepDelaymob = (stepDelay * 1.5);  // adjust smallest timing unit for mobility game = 750 ms
int balanceDelay = (stepDelay * 20);   // 10s
int sideliftDelay = (stepDelay * 4);   // 2s time for one side leg lift
int lungeDelay = (stepDelay * 4);      // 2s time to do to one lunge
int squat1Delay = (stepDelay * 4);     // 2s time to do one squat
//int squat2Delay = (stepDelay * 8);     // 4s time do to one squat with 2 pulses or slow squat
int jumpDelay = (stepDelay * 4);  // 2s time to do 1 jump

int exCounter = 0;         // we are counting up exercise parts completed, start at 0
int balanceScore = 0;      // time spent in static balance for L, R, toes, heels as % calib2
int balChecker = 0;        // adds ms in balance, calib2
int balanceAchieved = 0;   // additive balance held (as % of total time) calib2
int balanceScoreSide = 0;  // for cailb3 as %
//int maxcount = 0;                    // overall score for calib4, max is 32 x 100, sent to data ESP
//int mobScore = 0;                    // mobility score from calib4 CLOCKFACE exercise, tapping tiles in time: max value is 8x4 = 32, send as %
int weightOn = 200;  // threshold for weight on a tile (estimate)
//int noise = 50;                      // threshold for noise on a tile (estimate) unused
//int tap = 0;            // to register any taps on tiles during lightOn (multi triggers are ok) calib4
//int tapcounter = 0;     // now single-trigger: was tile ever tapped or not (0 or 100) during last lightOn calib4
int balanceScoreDyn = 0;  // quality of dynamic balance lunges (calib 5) as %
//int strengthScore = 0;  // counting 6 squats as 100 pts, max score is 600, sent as % calib6
//int squatCounter = 0;   // to register squats on tiles (multi triggers are ok) calib6
//int squatScore = 0;     // score of 100 for each squat calib6
int jumpCount;
int liftOff = 0;
int landing = 0;
int gameSuccess = 5; // can be 0=success, 1=partial, 2= fail. 5 = safety

// uniform structure for all data messages
typedef struct struct_message_all {  // sender/receiver must match structure
  int id;                            // unique sender ID: yellobyte ESP = 1, game ESP = 2
  int t;                             // can be used for airtime
  int b;                             // can be used for buttonInput
  int jc;                            // can be used for jumpCount
  int js;                            // can be used for jumpState
  int sd;                            // can be used for stepDelay
  int dA;                            // left toe sensor
  int dB;                            // left heel sensor
  int eA;                            // right toe sensor
  int eB;                            // right heel sensor
  int fA;                            // Calib 2 Balance Score
  int fB;                            // Calib 3 Leg Lift Score
  int gA;                            // Calib 5 Lunge Score
  int gB;                            // Game Success
} struct_message_all;

struct_message_all myResults;  // Create a struct_message called myResults to be sent to game ESP
struct_message_all myGame;     // Create an incoming struct_message from yellobyte ESP called myGame
struct_message_all audioMessage; //What gets sent to YB.

esp_now_peer_info_t peerInfo;  // store info about peer

// Callback when data is sent from here (but we do nothing)
// The first argument is a pointer to an *info struct, no longer the MAC address
void OnDataSent(const esp_now_send_info_t *info, esp_now_send_status_t status) {
  //Serial.print("\r\nLast Packet Send Status:\t");
  //Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
}

// callback function, executed when data is received here, sets dataReceived = true
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  //char macStr[18];
  //Serial.print("Packet received from: ");
  //snprintf(macStr, sizeof(macStr), "%02x:%02x:%02x:%02x:%02x:%02x",
  //         mac_addr[0], mac_addr[1], mac_addr[2], mac_addr[3], mac_addr[4], mac_addr[5]);
  //print the MAC address of the sending board:
  //Serial.println(macStr);
  //We now specify that the incomingData from the sending board goes into a myGame structure:
  memcpy(&myGame, incomingData, sizeof(myGame));
  //With the myGame structure ready for the values (sent by the yellobyte) we identify (again) which board sent the packet by printing the board ID:
  //Serial.printf("Board ID %u: %u bytes\n", myGame.id, len);
  //Fill the myGame structure with the data.
  //airtime = myGame.t;
  buttonInput = myGame.b;
  //stepDelay = myGame.sd;
  dataReceived = true;
}

//------------------------------------------------------------------------------------------------------STATE MACHINES
enum resetState {  // state machine for reset
  RESET,
  RESETDONE
};
resetState currentResetState = RESET;  // declare variable for switching and start state reset

enum marchState {  //state machine for calib1
  MARCHPREP,
  MARCHSTART,
  MARCH1END,
  MARCH2END,
  MARCHDONE
};
marchState currentMarchState = MARCHPREP;  // declare variable for switching and start state calib3

enum balState {  //state machine for calib2
  BAL1PREP,
  BAL1START,
  BAL1END,
  BAL2START,
  BAL2END,
  BAL3START,
  BAL3END,
  BAL4START,
  BAL4END,
  BALDONE
};
balState currentBalState = BAL1PREP;  // declare variable for switching and start state calib2

enum sideState {  //state machine for calib3
  SIDEPREP,
  SIDE1START,
  SIDE1END,
  SIDE2START,
  SIDE2END,
  SIDEDONE
};
sideState currentSideState = SIDEPREP;  // declare variable for switching and start state calib3

/*enum CalibrationState {  //to manage multi-stage process, one by one, in calib4
  STEP1_PREP,
  STEP1_WAIT_PREP_OFF_LIGHTS_ON,
  STEP1_LIGHTS_ON_WAIT_TAP,
  STEP1_STEPBACK_LIGHTS2_ON,
  STEP2_LIGHTS_ON_WAIT_TAP,
  STEP2_STEPBACK_LIGHTS3_ON,
  STEP3_LIGHTS_ON_WAIT_TAP,
  STEP3_STEPBACK_LIGHTS4_ON,
  STEP4_LIGHTS_ON_WAIT_TAP,
  STEP4_STEPBACK_LIGHTS5_ON,
  STEP5_LIGHTS_ON_WAIT_TAP,
  STEP5_STEPBACK_LIGHTS6_ON,
  STEP6_LIGHTS_ON_WAIT_TAP,
  STEP6_STEPBACK_LIGHTS7_ON,
  STEP7_LIGHTS_ON_WAIT_TAP,
  STEP7_STEPBACK_LIGHTS8_ON,
  STEP8_LIGHTS_ON_WAIT_TAP,
  STEP9_LIGHTS_ON_WAIT_TAP,
  STEP9_STEPBACK_LIGHTS10_ON,
  STEP10_LIGHTS_ON_WAIT_TAP,
  STEP10_STEPBACK_LIGHTS11_ON,
  STEP11_LIGHTS_ON_WAIT_TAP,
  STEP11_STEPBACK_LIGHTS12_ON,
  STEP12_LIGHTS_ON_WAIT_TAP,
  STEP12_STEPBACK_LIGHTS13_ON,
  STEP13_LIGHTS_ON_WAIT_TAP,
  STEP13_STEPBACK_LIGHTS14_ON,
  STEP14_LIGHTS_ON_WAIT_TAP,
  STEP14_STEPBACK_LIGHTS15_ON,
  STEP15_LIGHTS_ON_WAIT_TAP,
  STEP15_STEPBACK_LIGHTS16_ON,
  STEP16_LIGHTS_ON_WAIT_TAP,
  STEP16_DONE
};
CalibrationState currentState = STEP1_PREP;  // declare variable for switching and start state calib4*/

enum lungeState {  //state machine for calib5
  LUNGEPREP,
  LUNGE1START,
  LUNGE1END,
  LUNGE2START,
  LUNGE2END,
  LUNGEDONE
};
lungeState currentLungeState = LUNGEPREP;  // declare variable for switching and start state calib2

enum squatState {  //state machine for calib6
  SQUATPREP,
  SQUAT1START,
  SQUAT1END,
  /*SQUAT2START,
  SQUAT2END,
  SQUAT3START,
  SQUAT3END,
  SQUAT4PREP,
  SQUAT4START,
  SQUAT4END,
  SQUAT5START,
  SQUAT5END,
  SQUAT6START,
  SQUAT6END*/
  SQUATEND,
  SQUATDONE
};
squatState currentSquatState = SQUATPREP;  // declare variable for switching and start state calib6*/

enum jumpingState {  // state machine for calib7
  JUMPPREP,
  JUMPBENCHMARK,
  JUMPSTART,
  JUMPOFF,
  JUMPLANDPREP,
  JUMPLAND,
  JUMPEND,
  JUMPDONE
};
jumpingState currentJumpingState = JUMPPREP;  // declare variable for switching and start state reset

enum finalState {  // state machine for calibend
  FINAL,
  FINALDONE
};
finalState currentFinalState = FINAL;  // declare variable for switching and start state final

enum resultState {  // state machine for results
  RESULT,
  RESULTDONE
};
resultState currentResultState = RESULT;  // declare variable for switching and start state result

unsigned long startMillis = 0;           // this is millis start time in timer.................................. TIMER
unsigned long currentMillis = millis();  //check time now in timer
// -------------------------------------------------------------------------------------------- END OF STATE MACHINES
//===============================================================
void setup() {
  pixels.begin();  // INITIALIZE NeoPixel strip object (REQUIRED)

  Serial.begin(115200);            // Initialize Serial Monitor
  WiFi.mode(WIFI_STA);             // Set device as a Wi-Fi Station
  if (esp_now_init() != ESP_OK) {  // Init ESP-NOW
    //Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Once ESPNow is successfully Init, we will register for Send CB to get the status of transmitted packet
  esp_now_register_send_cb((esp_now_send_cb_t)OnDataSent);

  // Register the peer(s)

  //Adds Multiple Peers it can send to + receive from
  memcpy(ybAddress, receiverAddress1, 6);
  memcpy(gameAddress, receiverAddress2, 6);

  addPeer(receiverAddress1);
  addPeer(receiverAddress2);

  // Once ESPNow is successfully Init, we will register for recv CB to get recv packer info
  esp_now_register_recv_cb(esp_now_recv_cb_t(OnDataRecv));
}  // end of void setup

//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
void loop() {
  loadSensorData();  //analog read sensors and send to yellobyte for monitoring
  if (dataReceived == true) {
    loadGameData();  // loads buttonInput, sets dataReceived to false again
  }
  /* Calibration and Gamestate based on buttonInput as follows:
   0 = Reset
   1 = 
   2 = 
   3 = 
   91 = Marching on the spot 
   92 = balance
   93 = sideleg lifts
   //94 = agility - not on the SINGLE TILE, needs something else like fast feet
   95 = lunges
   //96 = squats - can't measure, so pointless
   97 = jumps 
   98 = save data, victory display*/

  switch (buttonInput) {  //do NOT add cleanUps to these cases!
    case 0:
      //main menu, does nothing at the moment
      //game esp just plays the main menu music upon calling it.
    break;
    case 90:               //reset
      {
        switch (currentResetState) {
          case RESET:
            {
              cleanUp();      // wipes all tile LEDs
              exCounter = 0;  // resets the exercise counter, this is never sent
              //WIPE WHAT IS SENT BACK TO yellobyte ESP:
              //myResults.t is airtime for game, hard-coded. could be received, could be sent
              //myResults.b is buttonInput, is received but not sent
              jumpCount = 0;  //wipeclean used for calib7
              liftOff = 0;    // wipeclean used for calib7
              landing = 0;    // wipeclean used for calib7
              //myResults.jc is jumpCount, used for calib7?
              //myResults.js is jumpState, here used to express gameState & trigger sounds: 0=success, 1=partial, 2=fail
              //myResults.sd is stepDelay, hard-coded, not received, not sent
              balanceScore = 0;      //resets balance score static (calib 2)
              balanceScoreSide = 0;  // resets this (calib3)
              //mobScore = 0;          // resets the mobility score (calib4) NOT USED, NOT SENT
              //maxcount = 0;          // resets the max count (calib4)
              balanceScoreDyn = 0;  //resets balance score dynamic (calib 5)
              //strengthScore = 0;     //resets the strength score (calib6)
              gameSuccess = 5; //safety
              //ACTUAL RESETS SENT HERE:
              myResults.id = 2;  //sent from the game ESP to yellobyte ESP
              /*myResults.dA = leftToe; already sent
              myResults.dB = leftHeel;
              myResults.eA = rightToe;
              myResults.eB = rightHeel;*/
              myResults.fA = balanceScore;      //calib2 set to nil
              myResults.fB = balanceScoreSide;  //calib3 set to nil
              myResults.gA = balanceScoreDyn;   //calib5 set to nil
              myResults.gB = 5;                 //results score, for nil result, set to five
              //esp_err_t result1 = esp_now_send(receiverAddress, (uint8_t *)&myResults, sizeof(myResults));
              sendMessage(gameAddress,myResults);
              resetStates();
              currentResetState = RESETDONE;  //moves us into an idle state
            }
            break;
          case RESETDONE:
            {  //do nothing here
            }
            break;
        }  // end resetswitch
      }    // end of buttonInput case 0
      break;
    // In cases would need to send a message to YB esp to start the specific game Music 
    // message.dB = track | message.eA = track volume | message.gB = should it loop (0 = yes, 1 = no)
    // value of which sound is in which track depends how the audio files are ordered into the SD card.
    case 91:  //start calibration exercise 1 marching
      {
        calib1();
      }
      break;
    case 92:  //start calibration exercise 2 balance still
      {
        calib2();  // this sends off balanceScore at end
      }
      break;
    case 93:  //start calibration exercise 3 side leg lifts
      {
        calib3();  //provides balanceScoreSide
      }
      break;
    case 94:  //start calibration exercise 4 agility - do nothing here
      {
        //calib4();  //this sends off mobScore after 2 reps
      }
      break;
    case 95:  //start calibration exercise 5 lunges
      {
        calib5();  // lunges, sends off balanceScoreDyn
      }
      break;
    case 96:  //start calibration exercise 6 squats - do nothing here
      {
        calib6();  // after 3 reps this leads to 3 more reps, sends off strengthScore
      }
      break;
    case 97:  //start calibration exercise 7 jumps x3
      {
        calib7();  //time for jumping x 3
      }
      break;
    case 98:  //end collecting data in and send off?
      {
        calibend();  //final display, has cleanUp built in
      }
      break;
  }  //end of switch
}  //end of loop

//======================================================================================
void cleanUp() {
  pixels.clear();  // Set all pixel colors to 'off'
  pixels.show();
}
void loadGameData() {  // loads all sensor values, sets dataReceived = false
  //airtime = myGame.t;  // check the airtime received from yellobyte ESP
  //Serial.print("airtime: ");
  //Serial.print(airtime);
  buttonInput = myGame.b;  //check gamelevel received from yellobyte ESP
  Serial.print("buttonInput received: ");
  Serial.println(buttonInput);
  resetStates(); //Resets the switch cases inside the calibs, so they don't break if playing multiple exercises without fully reseting (90) between
  //stepDelay is 500ms by default
  //stepDelay = myGame.sd; // update stepDelay-if we wanted to alter this.
  dataReceived = false;
}  

void loadSensorData() {
  leftToe = analogRead(5);
  leftHeel = analogRead(4);
  rightToe = analogRead(9);
  rightHeel = analogRead(10);
  /*myResults.dA = leftToe;
  myResults.dB = leftHeel;
  myResults.eA = rightToe;
  myResults.eB = rightHeel;
  esp_now_send(receiverAddress, (uint8_t *)&myResults, sizeof(myResults));*/
  /*Serial.print("GPIO5: ");
  Serial.print(leftToe);  //print the number
  Serial.print(", GPIO4: ");
  Serial.print(leftHeel);  //print the number
  Serial.print(", GPIO9: ");
  Serial.print(rightToe);  //print the number
  Serial.print(", GPIO10: ");
  Serial.println(rightHeel);  //print the number*/
}

//=======================================================================================
//-------------------------------------START EXERCISES-----------------------------------
//=======================================================================================

//-- PREPARATION for all exercises
void leftStep() {                //lefthalftile in cyan
  for (int j = 1; j < 2; j++) {  // but here only one iteration (e.g. pos1 = lefthalftile)
    Segment *segments = segmentName[j];
    int count = segmentCounts[j];
    // Loop through each segment within the current array - gathers up bits of LED strips
    for (int i = 0; i < count; i++) {
      Segment seg = segments[i];
      for (int k = seg.start; k < seg.start + seg.count; k++) {
        lightToLoad.lightTile(left, k);  // we are prepping colour 'left' for chosen tile, for chosen bits of LED
        pixels.setPixelColor(k, left);   // Set the color of the pixels for relevant LEDS
      }
    }
  }
  pixels.show();  // Show the filled colors
}

void rightStep() {
  for (int j = 0; j < 1; j++) {  // but here only one iteration (e.g. pos0 = righthalftile)
    Segment *segments = segmentName[j];
    int count = segmentCounts[j];
    // Loop through each segment within the current array - gathers up bits of LED strips
    for (int i = 0; i < count; i++) {
      Segment seg = segments[i];
      for (int k = seg.start; k < seg.start + seg.count; k++) {
        lightToLoad.lightTile(right, k);  // we are prepping colour 'right' for chosen tile, for chosen bits of LED
        pixels.setPixelColor(k, right);   // Set the color of the pixels for relevant LEDS
      }
    }
  }
  pixels.show();  // Show the filled colors
}

// ======================= CALIB 1 - MARCHING ====================

void calib1() {                            // Calibration exercise 1: marching on the spot 60s total
  unsigned long currentMillis = millis();  // get the time now?
  switch (currentMarchState) {
    case MARCHPREP:
      currentResetState = RESET;  //make reset available
      cleanUp();                  //needed?
      endSet();                   // initial setup: lights on, no wait
      Serial.println("marchprep");
      startMillis = currentMillis;  // Record start time for the *next* stage wait
      exCounter = 0;
      currentMarchState = MARCHSTART;  // Move to the next stage
      break;
    case MARCHSTART:                                         // left step on
      if (currentMillis - startMillis >= (stepDelay * 4)) {  // once prep duration is over...
        cleanUp();                                           // lights off
        leftStep();                                          // Start the lights for left leg
        startMillis = currentMillis;                         // Reset timer for the *next* stage wait
        currentMarchState = MARCH1END;                       // Move to the next stage
      }
      break;
    case MARCH1END:  //weight on left leg (leftToe, leftHeel)?
      //check left sensors for weightOn, right for weightOff
      //if (sensorValue[18] > weightOn && sensorValue[19] > weightOn && sensorValue[20] < weightOn && sensorValue[21] < weightOn) {}
      if (currentMillis - startMillis >= stepDelay) {  // Check if timeout has occurred 500ms
        cleanUp();
        rightStep();                    // Start the lights for right leg
        startMillis = currentMillis;    // Reset timer for the *next* stage wait
        currentMarchState = MARCH2END;  // Move to the next stage and step
      }
      break;
    case MARCH2END:  //weight on right leg (rightToe, rightHeel)?
      //check left sensors for weightOff, right for weightOn
      //if (sensorValue[18] < weightOn && sensorValue[19] < weightOn && sensorValue[20] > weightOn && sensorValue[21] > weightOn) {}
      if (currentMillis - startMillis >= stepDelay) {  // Check if timeout has occurred 500ms
        cleanUp();
        leftStep();  // Start the lights for left leg
        exCounter++;
        if (exCounter == 12) {  //last one should be 32
          Serial.println("marchend");
          successResult();  // play success sound, green light on
          exCounter = 0;
          cleanUp();
          currentMarchState = MARCHDONE;  //move into idle state
        } else {                          //we have not reached full reps
          startMillis = currentMillis;    // Reset timer for the *next* stage wait
          currentMarchState = MARCH1END;  // Move to the next stage, T10 still on
        }
      }
      break;
    case MARCHDONE:  // idle, wait for button press
      break;
  }  //end of switch
}  //end of calib1

// ======================= CALIB 2 - BALANCE ====================

void calib2() {                            // exercise 2: balance on LEFT, RIGHT, TOES, HEELS 60s total
  unsigned long currentMillis = millis();  // get the time now?
  switch (currentBalState) {
    case BAL1PREP:
      currentResetState = RESET;  //make reset available
      cleanUp();
      endSet();                     // initial setup (lights on), no wait
      startMillis = currentMillis;  // Record start time for the *next* stage wait
      balanceScore = 0;
      balChecker = 0;
      balanceAchieved = 0;
      currentBalState = BAL1START;  // Move to the next stage
      break;
    case BAL1START:
      if (currentMillis - startMillis >= (stepDelay * 4)) {  // once prep duration is over...
        cleanUp();
        leftStep();                   // Start the leftside lights for balancing on left leg
        startMillis = currentMillis;  // Reset timer for the *next* stage wait
        currentBalState = BAL1END;    // Move to the next stage
      }
      break;
    case BAL1END:  //balance on left leg - additive
      //balChecker = 0;// don't use this, it is 0 to start, if active it would be a constant reset to 0
      //check leftToe, leftHeel sensor for weightOn, rightToe rightHeel for weightOff
      if (leftToe > weightOn && leftHeel > weightOn && rightToe < weightOn && rightHeel < weightOn) {
        balChecker++;                          // add up for every ms the condition is true, could be 0 - 20,000 - goes up
        balanceAchieved = (balChecker / 200);  // we convert ms in balance into a percentage for L balance 0 - 100 - goes up
      }
      if (currentMillis - startMillis >= (balanceDelay * 1)) {  // Check if timeout has occurred
        cleanUp();
        balanceScore = balanceScore + balanceAchieved;  //0 plus percentage achieved in L balance
        //Serial.print(", total score: ");
        //Serial.println(balanceScore);
        balChecker = 0;  // resetting balChecker
        balanceAchieved = 0;
        startMillis = currentMillis;  // Reset timer for the *next* stage wait
        currentBalState = BAL2START;  // Move to the next stage and step
      }
      break;
    case BAL2START:
      if (currentMillis - startMillis >= (stepDelay * 2)) {  // once prep duration is over...
        rightStep();                                         // Start the right tile lights for balancing on right leg
        startMillis = currentMillis;                         // Reset timer for the *next* stage wait
        currentBalState = BAL2END;                           // Move to the next stage
      }
      break;
    case BAL2END:  //balance on right leg additive
      // check left for weightOff, right for weightOn
      if (leftToe < weightOn && leftHeel < weightOn && rightToe > weightOn && rightHeel > weightOn) {
        balChecker++;                          // add up for every ms the condition is true, could be 0 - 20,000 - goes up
        balanceAchieved = (balChecker / 200);  // we convert ms in balance into a percentage for R balance 0 - 100 - goes up
      }
      if (currentMillis - startMillis >= (balanceDelay * 1)) {  // Check if timeout has occurred
        cleanUp();
        balanceScore = balanceScore + balanceAchieved;  //percentage achieved in L balance, plus R balance
        //Serial.print(", total score: ");
        //Serial.println(balanceScore);
        balChecker = 0;  // resetting balChecker
        balanceAchieved = 0;
        startMillis = currentMillis;  // Reset timer for the *next* stage wait
        currentBalState = BAL3START;  // Move to the next stage and step
      }
      break;
    case BAL3START:
      if (currentMillis - startMillis >= (stepDelay * 2)) {  // once prep duration is over...
        pixels.fill(right, 10, 10);                          //LEFT toe on
        //pixels.fill(right, 50, 5);
        pixels.fill(right, 5, 5);
        pixels.fill(left, 20, 10);  // RIGHT toe on
        //pixels.fill(left, 50, 5);
        pixels.fill(left, 0, 5);
        pixels.show();                // Start the toe tile lights for balancing on both toes
        startMillis = currentMillis;  // Reset timer for the *next* stage wait
        currentBalState = BAL3END;    // Move to the next stage
      }
      break;
    case BAL3END:  //balance on toes - additive
      // check toes for weightOn, heels for weightOff
      if (leftToe > weightOn && leftHeel < weightOn && rightToe > weightOn && rightHeel < weightOn) {
        balChecker++;                          // add up for every ms the condition is true, could be 0 - 10,000 - goes up
        balanceAchieved = (balChecker / 100);  // we convert ms in balance into a percentage for T balance 0 - 100 - goes up
      }
      if (currentMillis - startMillis >= (balanceDelay * 0.7)) {  // Check if timeout has occurred
        cleanUp();
        balanceScore = balanceScore + balanceAchieved;  //percentage achieved in L balance, R balance, plus T balance
        //Serial.print(", total score: ");
        //Serial.println(balanceScore);
        balChecker = 0;  // resetting balChecker
        balanceAchieved = 0;
        startMillis = currentMillis;  // Reset timer for the *next* stage wait
        currentBalState = BAL4START;  // Move to the next stage and step
      }
      break;
    case BAL4START:
      if (currentMillis - startMillis >= (stepDelay * 2)) {  // once prep duration is over...
        pixels.fill(left, 30, 10);                           //LEFT heel on
        //pixels.fill(left, 55, 5);
        pixels.fill(left, 0, 5);
        pixels.fill(right, 40, 10);  // RIGHT heel on
        //pixels.fill(right, 55, 5);
        pixels.fill(right, 5, 5);
        pixels.show();                // Start the heel tile lights for balancing on both heels
        startMillis = currentMillis;  // Reset timer for the *next* stage wait
        currentBalState = BAL4END;    // Move to the next stage
      }
      break;
    case BAL4END:  //balance on heels
      // check toes for weightOff, heels for weightOn
      if (leftToe < weightOn && leftHeel > weightOn && rightToe < weightOn && rightHeel > weightOn) {
        balChecker++;                          // add up for every ms the condition is true, could be 0 - 10,000 - goes up
        balanceAchieved = (balChecker / 100);  // we convert ms in balance into a percentage for H balance 0 - 100 - goes up
      }
      if (currentMillis - startMillis >= (balanceDelay *0.5)) {  // Check if timeout has occurred
        cleanUp();
        balanceScore = balanceScore + balanceAchieved;  //percentage achieved in L balance, R balance, T balance plus H balance
        balanceScore = balanceScore / 4;                // average the 4 added percentages before sending
        //Serial.print(", total score: ");
        //Serial.println(balanceScore);
        // send standing balanceScore to data ESP
        myResults.id = 2;  // sent from the game ESP
        //myResults.b = buttonInput; //which level 92
        //myResults.sd = stepDelay; //what stepDelay was used
        myResults.fA = balanceScore;
        // Send message1 via ESP-NOW to data ESP
        //esp_err_t result1 = esp_now_send(receiverAddress, (uint8_t *)&myResults, sizeof(myResults));
        sendMessage(gameAddress, myResults); //Sends the score to game ESP
        //if (result1 == ESP_OK) {
        //Serial.print(", myResults was sent with success");
        //} else {
        //Serial.println("Error sending myResults");
        //}
        balChecker = 0;  // resetting
        balanceAchieved = 0;
        balanceScore = 0;
        successResult();
        cleanUp();                  //wipe all the tiles
        currentBalState = BALDONE;  // Move to the idle state
      }
      break;
    case BALDONE:  //do nothing 
      break;
  }  //end of balState
}  //end of calib2

// ======================= CALIB 3 - LEG LIFT ====================

//===PREPARATIONS and CALIB3
void sideliftright() {
  pixels.fill(left, 20, 20);  // stand on left
  pixels.fill(left, 50, 10);
  pixels.show();
  delay (stepDelay * 0.5);// 250 ms
  pixels.fill(right, 19, 1);
  pixels.fill(right, 5, 1);
  pixels.fill(right, 40, 1);
  pixels.show();
  delay (stepDelay * 0.5);// 250 ms
  pixels.fill(right, 18, 1);
  pixels.fill(right, 6, 1);
  pixels.fill(right, 41, 1);
  pixels.show();
  delay (stepDelay * 0.5);
  pixels.fill(right, 17, 1);
  pixels.fill(right, 7, 1);
  pixels.fill(right, 42, 1);
  pixels.show();
  delay (stepDelay * 0.5);
  pixels.fill(right, 16, 1);
  pixels.fill(right, 8, 1);
  pixels.fill(right, 43, 1);
  pixels.show();
  delay (stepDelay * 0.5);
  pixels.fill(right, 15, 1);
  pixels.fill(right, 9, 1);
  pixels.fill(right, 44, 1);
  pixels.show();
}
void sideliftleft() {
  pixels.fill(right, 10, 10);  // stand on right
  pixels.fill(right, 50, 10);
  pixels.fill(right, 40, 10);
  pixels.show();
  delay (stepDelay * 0.5);// 250 ms
  pixels.fill(left, 20, 1);
  pixels.fill(left, 4, 1);
  pixels.fill(left, 39, 1);
  pixels.show();
  delay (stepDelay * 0.5);// 250 ms
  pixels.fill(left, 21, 1);
  pixels.fill(left, 3, 1); 
  pixels.fill(left, 38, 1);
  pixels.show();
  delay (stepDelay * 0.5);
  pixels.fill(left, 22, 1);
  pixels.fill(left, 2, 1);
  pixels.fill(left, 37, 1);
  pixels.show();
  delay (stepDelay * 0.5);
  pixels.fill(left, 23, 1);
  pixels.fill(left, 1, 1);
  pixels.fill(left, 36, 1);
  pixels.show();
  delay (stepDelay * 0.5);
  pixels.fill(left, 24, 1);
  pixels.fill(left, 0, 1);
  pixels.fill(left, 35, 1);
  pixels.show();
}
void calib3() {                            // exercise3: side lift 5xL and 5xR, 30s total
  unsigned long currentMillis = millis();  // get the time now?
  switch (currentSideState) {
    case SIDEPREP:
      currentResetState = RESET;    //make reset available
      cleanUp();                    //needed?
      endSet();                     // initial setup, no wait
      startMillis = currentMillis;  // Record start time for the *next* stage wait
      balanceScoreSide = 0;
      balChecker = 0;
      balanceAchieved = 0;
      exCounter = 0;
      currentSideState = SIDE1START;  // Move to the next stage
      break;
    case SIDE1START:                                         // left leg out to the side, right stays on
      if (currentMillis - startMillis >= (stepDelay * 4)) {  // once prep duration is over...
        cleanUp();
        sideliftleft();               // Start the lights for balancing on right leg
        startMillis = currentMillis;  // Reset timer for the *next* stage wait
        currentSideState = SIDE1END;  // Move to the next stage
      }
      break;
    case SIDE1END:  //balance on right leg - additive
      //check left sensors for weightOff, right for weightOn
      if (leftToe < weightOn && leftHeel < weightOn && rightToe > weightOn && rightHeel > weightOn) {
        balChecker++;                         // add up for every ms the condition is true, could be 0 - 2,000 - goes up
        balanceAchieved = (balChecker / 20);  // we convert ms in balance into a percentage for L balance 0 - 100 - goes up
      }
      if (currentMillis - startMillis >= sideliftDelay) {  // Check if timeout has occurred 2000ms
        cleanUp();
        //endSet();
        balanceScoreSide = balanceScoreSide + balanceAchieved;  //0 plus percentage achieved in R balance
        //Serial.print(", total score: ");
        //Serial.println(balanceScore);
        balChecker = 0;  // resetting balChecker
        balanceAchieved = 0;
        exCounter++;
        startMillis = currentMillis;    // Reset timer for the *next* stage wait
        currentSideState = SIDE2START;  // Move to the next stage and step
      }
      break;
    case SIDE2START:                                         // right leg out to the side, left stays on
      if (currentMillis - startMillis >= (stepDelay * 2)) {  // once prep duration is over...
        cleanUp();
        sideliftright();              // Start the lights for balancing on left leg
        startMillis = currentMillis;  // Reset timer for the *next* stage wait
        currentSideState = SIDE2END;  // Move to the next stage
      }
      break;
    case SIDE2END:  //balance on left leg - additive
      //check left sensors for weightOn, right for weightOff
      if (leftToe > weightOn && leftHeel > weightOn && rightToe < weightOn && rightHeel < weightOn) {
        balChecker++;                         // add up for every ms the condition is true, could be 0 - 2,000 - goes up
        balanceAchieved = (balChecker / 20);  // we convert ms in balance into a percentage for L balance 0 - 100 - goes up
      }
      if (currentMillis - startMillis >= sideliftDelay) {  // Check if timeout has occurred 2000ms
        cleanUp();
        //endSet();
        balanceScoreSide = balanceScoreSide + balanceAchieved;  //R plus percentage achieved in L balance
        //Serial.print(", total score: ");
        //Serial.println(balanceScore);
        balChecker = 0;  // resetting balChecker
        balanceAchieved = 0;
        exCounter++;
        if (exCounter == 6) {                       //last one
          balanceScoreSide = balanceScoreSide / 6;  //average after 10 side lifts
          myResults.id = 2;                          // sent from the game ESP
          //myResults.b = buttonInput; //which calib was it? 93
          //myResults.sd = stepDelay;  //what delay was used?
          myResults.fB = balanceScoreSide;  // percentage 0 - 100
          // Send message1 via ESP-NOW to data ESP
          //esp_err_t result1 = esp_now_send(receiverAddress, (uint8_t *)&myResults, sizeof(myResults));
          sendMessage(gameAddress,myResults);
          //if (result1 == ESP_OK) {
          //Serial.print(", myResults was sent with success");
          //} else {
          //Serial.println("Error sending myResults");
          //}
          balChecker = 0;  // resetting
          balanceAchieved = 0;
          exCounter = 0;
          balanceScoreSide = 0;
          successResult();
          cleanUp();
          currentSideState = SIDEDONE;  //move into idle state
        } else {                        //we have not reached 10 reps
          balChecker = 0;               // resetting balChecker
          balanceAchieved = 0;
          startMillis = currentMillis;    // Reset timer for the *next* stage wait
          currentSideState = SIDE1START;  // Move to the next stage and step T10 T11 still on
        }
      }
      break;
    case SIDEDONE:  // idle, wait for button press
      break;
  }  //end of switch
}  //end of calib3

// ======================= CALIB 4 - AGILITY ====================
/*
  /* // PREPARATIONS FOR CALIB 4
  void stepbackleft() {            // for tile 10
    for (int j = 0; j < 1; j++) {  // but here only one iteration (e.g. pos0 = righthalftile)
      Segment *segments = segmentName[j];
      int count = segmentCounts[j];
      // Loop through each segment within the current array - gathers up bits of LED strips
      for (int i = 0; i < count; i++) {
        Segment seg = segments[i];
        for (int k = seg.start; k < seg.start + seg.count; k++) {
          lightToLoad.lightTile(left, k);  // we are prepping colour 'left' for chosen tile, for chosen bits of LED
          strip10.setPixelColor(k, left);  // Set the color of the pixels for relevant LEDS
        }
      }
    }
    strip10.show();  // Show the filled colors
  }
  void stepbackright() {           // for tile 11
    for (int j = 1; j < 2; j++) {  // but here only one iteration (e.g. pos1 = lefthalftile)
      Segment *segments = segmentName[j];
      int count = segmentCounts[j];
      // Loop through each segment within the current array - gathers up bits of LED strips
      for (int i = 0; i < count; i++) {
        Segment seg = segments[i];
        for (int k = seg.start; k < seg.start + seg.count; k++) {
          lightToLoad.lightTile(right, k);  // we are prepping colour 'left' for chosen tile, for chosen bits of LED
          strip11.setPixelColor(k, right);  // Set the color of the pixels for relevant LEDS
        }
      }
    }
    strip11.show();  // Show the filled colors
  }
  void lightOn() {                                           //light up a section of a tile with left colour
    for (int j = tileSegment; j < (tileSegment + 1); j++) {  //normally runs through segmentArray (pos0 - pos5); here only 1 iteration over int tileSegment
      Segment *segments = segmentName[j];
      int count = segmentCounts[j];
      // Loop through each segment within the current array - gathers up bits of LED strips
      for (int i = 0; i < count; i++) {
        Segment seg = segments[i];
        for (int k = seg.start; k < seg.start + seg.count; k++) {
          lightToLoad.lightTile(left, k);       // we are prepping colour 'left' for chosen tile, for chosen bits of LED
          lightToCheck.setPixelColor(k, left);  // Set the color of the pixels for relevant LEDS
        }
      }
    }
    lightToCheck.show();  // Show us the filled color LEDs in the chosen tile, leave on
  }
  void lightOnR() {                                          //light up a section of a tile with right colour
    for (int j = tileSegment; j < (tileSegment + 1); j++) {  //normally runs through segmentArray (pos0 - pos5); here only 1 iteration over int tileSegment
      Segment *segments = segmentName[j];
      int count = segmentCounts[j];
      // Loop through each segment within the current array - gathers up bits of LED strips
      for (int i = 0; i < count; i++) {
        Segment seg = segments[i];
        for (int k = seg.start; k < seg.start + seg.count; k++) {
          lightToLoad.lightTile(right, k);       // we are prepping colour 'left' for chosen tile, for chosen bits of LED
          lightToCheck.setPixelColor(k, right);  // Set the color of the pixels for relevant LEDS
        }
      }
    }
    lightToCheck.show();  // Show us the filled color LEDs in the chosen tile, leave on
  }
  void prepcalib4() {              //T10 righthalf leftcolour and T11 lefthalf  rightcolour on
    for (int j = 0; j < 1; j++) {  // here only one iteration (e.g. pos0 = righthalftile)
      Segment *segments = segmentName[j];
      int count = segmentCounts[j];
      // Loop through each segment within the current array - gathers up bits of LED strips
      for (int i = 0; i < count; i++) {
        Segment seg = segments[i];
        for (int k = seg.start; k < seg.start + seg.count; k++) {
          lightToLoad.lightTile(left, k);  // we are prepping colour 'left' for chosen tile, for chosen bits of LED
          strip10.setPixelColor(k, left);  // Set the color of the pixels for relevant LEDS
        }
      }
    }
    strip10.show();                // Show the filled colors
    for (int j = 1; j < 2; j++) {  // here only one iteration (e.g. pos1 = lefthalftile)
      Segment *segments = segmentName[j];
      int count = segmentCounts[j];
      // Loop through each segment within the current array - gathers up bits of LED strips
      for (int i = 0; i < count; i++) {
        Segment seg = segments[i];
        for (int k = seg.start; k < seg.start + seg.count; k++) {
          lightToLoad.lightTile(right, k);  // we are prepping colour 'left' for chosen tile, for chosen bits of LED
          strip11.setPixelColor(k, right);  // Set the color of the pixels for relevant LEDS
        }
      }
    }
    strip11.show();  // Show the filled colors
  }*/

  /*void calib4() {   //-------------------------------------------START exercise 4:  CLOCKFACE, 4 times, total time 60s
    unsigned long currentMillis = millis();  // get the time now?
    switch (currentState) {
      case STEP1_PREP:
        currentResetState = RESET;                     //make reset available
        prepcalib4();                                  // Does initial setup (lights T10/T11), doesn't wait
        startMillis = currentMillis;                   // Record start time for the *next* stage wait
        currentState = STEP1_WAIT_PREP_OFF_LIGHTS_ON;  // Move to the next stage
        break;
      case STEP1_WAIT_PREP_OFF_LIGHTS_ON:
        if (currentMillis - startMillis >= stepDelaymob) {  // once prep duration is over...
          strip10.clear();
          strip10.show();
          lightToCheck = stripArray[5];             // prep strip6 lights
          tileSegment = 5;                          // pos 5 = Q4; used for struct Segment* segmentName[] in lightOn();
          lightOn();                                // Start the lights for tapping
          startMillis = currentMillis;              // Reset timer for the *next* stage wait
          currentState = STEP1_LIGHTS_ON_WAIT_TAP;  // Move to the next stage
        }
        break;
      case STEP1_LIGHTS_ON_WAIT_TAP:
        tap = 0;
        //sensorValue[11] = boardsStruct[1].eB;  //but T6B is already loaded
        indexToCheck = sensorValue[11];  // T6B sensor
        if (indexToCheck > weightOn) {   //continuously check if sensor is pressed at least once
          tap++;                         // switch from 0 to 1, we register multiple taps
          Serial.print("tap: ");
          Serial.println(tap);
          tapcounter = 100;  // we set tapcounter to a single score
        }
        if (currentMillis - startMillis >= stepDelaymob) {  // Check if timeout has occurred
          lightToCheck = stripArray[5];                     //clear strip6 lights
          lightToCheck.clear();
          lightToCheck.show();
          maxcount = maxcount + tapcounter;  //handle scoring logic here. we need to do something with this score!
          Serial.print(", total score: ");
          Serial.println(maxcount);
          mobScore++;
          tapcounter = 0;                            // resetting tapcounter
          stepbackleft();                            // Turn on the "step back" lights
          startMillis = currentMillis;               // Reset timer for the *next* stage wait
          currentState = STEP1_STEPBACK_LIGHTS2_ON;  // Move to the next stage and step
        }
        break;
      case STEP1_STEPBACK_LIGHTS2_ON:
        if (currentMillis - startMillis >= stepDelaymob) {
          strip10.clear();               //turn off left stepBack light (T10). right light (T11) stays on
          strip10.show();                //one off action
          lightToCheck = stripArray[5];  // prep strip6 lights
          tileSegment = 4;               // pos 4 = Q3; used for struct Segment* segmentName[] in lightOn();
          lightOn();
          startMillis = currentMillis;  // Record start time for the *next* stage wait
          currentState = STEP2_LIGHTS_ON_WAIT_TAP;
        }
        break;
      case STEP2_LIGHTS_ON_WAIT_TAP:
        tap = 0;
        //sensorValue[11] = boardsStruct[1].eB;  //but T6B is already loaded
        indexToCheck = sensorValue[11];  // T6B sensor
        if (indexToCheck > weightOn) {   //continuously check if sensor is pressed at least once
          tap++;                         // switch from 0 to 1, we register multiple taps
          Serial.print("tap: ");
          Serial.println(tap);
          tapcounter = 100;  // we set tapcounter to a single score
        }
        if (currentMillis - startMillis >= stepDelaymob) {  // Check if timeout has occurred
          lightToCheck = stripArray[5];                     //clear strip6 lights
          lightToCheck.clear();
          lightToCheck.show();
          maxcount = maxcount + tapcounter;  //handle scoring logic here. we need to do something with this score!
          Serial.print(", total score: ");
          Serial.println(maxcount);
          mobScore++;
          tapcounter = 0;                            // resetting tapcounter
          stepbackleft();                            // Turn on the "step back" lights
          startMillis = currentMillis;               // Reset timer for the *next* stage wait
          currentState = STEP2_STEPBACK_LIGHTS3_ON;  // Move to the next stage and step
        }
        break;
      case STEP2_STEPBACK_LIGHTS3_ON:
        if (currentMillis - startMillis >= stepDelaymob) {
          strip10.clear();               //turn off left stepBack light (T10). right light (T11) stays on
          strip10.show();                //one off action
          lightToCheck = stripArray[4];  // prep strip5 lights
          tileSegment = 5;               // pos 5 = Q4; used for struct Segment* segmentName[] in lightOn();
          lightOn();
          startMillis = currentMillis;  // Record start time for the *next* stage wait
          currentState = STEP3_LIGHTS_ON_WAIT_TAP;
        }
        break;
      case STEP3_LIGHTS_ON_WAIT_TAP:
        tap = 0;
        //sensorValue[11] = boardsStruct[1].eB;  //but T6B is already loaded
        indexToCheck = sensorValue[9];  // T5B sensor
        if (indexToCheck > weightOn) {  //continuously check if sensor is pressed at least once
          tap++;                        // switch from 0 to 1, we register multiple taps
          Serial.print("tap: ");
          Serial.println(tap);
          tapcounter = 100;  // we set tapcounter to a single score
        }
        if (currentMillis - startMillis >= stepDelaymob) {  // Check if timeout has occurred
          lightToCheck = stripArray[4];                     //clear strip5 lights
          lightToCheck.clear();
          lightToCheck.show();
          maxcount = maxcount + tapcounter;  //handle scoring logic here. we need to do something with this score!
          Serial.print(", total score: ");
          Serial.println(maxcount);
          mobScore++;
          tapcounter = 0;                            // resetting tapcounter
          stepbackleft();                            // Turn on the "step back" lights
          startMillis = currentMillis;               // Reset timer for the *next* stage wait
          currentState = STEP3_STEPBACK_LIGHTS4_ON;  // Move to the next stage and step
        }
        break;
      case STEP3_STEPBACK_LIGHTS4_ON:
        if (currentMillis - startMillis >= stepDelaymob) {
          strip10.clear();               //turn off left stepBack light (T10). right light (T11) stays on
          strip10.show();                //one off action
          lightToCheck = stripArray[8];  // prep strip9 lights for step 4
          tileSegment = 0;               // pos 0 = righthalftile; used for struct Segment* segmentName[] in lightOn();
          lightOn();
          startMillis = currentMillis;  // Record start time for the *next* stage wait
          currentState = STEP4_LIGHTS_ON_WAIT_TAP;
        }
        break;
      case STEP4_LIGHTS_ON_WAIT_TAP:
        tap = 0;
        //sensorValue[16] = boardsStruct[2].dA;  //T9A is already loaded
        //sensorValue[17] = boardsStruct[2].dB;  //T9B is already loaded
        indexToCheck = (sensorValue[16] + sensorValue[17]) / 2;  // average of T9A & T9B
        if (indexToCheck > weightOn) {                           //continuously check if sensor is pressed at least once
          tap++;                                                 // switch from 0 to 1, we register multiple taps
          Serial.print("tap: ");
          Serial.println(tap);
          tapcounter = 100;  // we set tapcounter to a single score
        }
        if (currentMillis - startMillis >= stepDelaymob) {  // Check if timeout has occurred
          lightToCheck = stripArray[8];                     //clear strip9 lights
          lightToCheck.clear();
          lightToCheck.show();
          maxcount = maxcount + tapcounter;  //handle scoring logic here. we need to do something with this score!
          Serial.print(", total score: ");
          Serial.println(maxcount);
          mobScore++;
          tapcounter = 0;                            // resetting tapcounter
          stepbackleft();                            // Turn on the "step back" lights
          startMillis = currentMillis;               // Reset timer for the *next* stage wait
          currentState = STEP4_STEPBACK_LIGHTS5_ON;  // Move to the next stage and step
        }
        break;
      case STEP4_STEPBACK_LIGHTS5_ON:
        if (currentMillis - startMillis >= stepDelaymob) {
          strip10.clear();                //turn off left stepBack light (T10). right light (T11) stays on
          strip10.show();                 //one off action
          lightToCheck = stripArray[12];  // prep strip13 lights
          tileSegment = 3;                // pos 3 = Q2; used for struct Segment* segmentName[] in lightOn();
          lightOn();
          startMillis = currentMillis;  // Record start time for the *next* stage wait
          currentState = STEP5_LIGHTS_ON_WAIT_TAP;
        }
        break;
      case STEP5_LIGHTS_ON_WAIT_TAP:
        tap = 0;
        //sensorValue[24] = boardsStruct[3].dA;  //T13A is already loaded
        indexToCheck = sensorValue[24];  // T13A sensor
        if (indexToCheck > weightOn) {   //continuously check if sensor is pressed at least once
          tap++;                         // switch from 0 to 1, we register multiple taps
          Serial.print("tap: ");
          Serial.println(tap);
          tapcounter = 100;  // we set tapcounter to a single score
        }
        if (currentMillis - startMillis >= stepDelaymob) {  // Check if timeout has occurred
          lightToCheck = stripArray[12];                    //clear strip13 lights
          lightToCheck.clear();
          lightToCheck.show();
          maxcount = maxcount + tapcounter;  //handle scoring logic here. we need to do something with this score!
          Serial.print(", total score: ");
          Serial.println(maxcount);
          mobScore++;
          tapcounter = 0;                            // resetting tapcounter
          stepbackleft();                            // Turn on the "step back" lights
          startMillis = currentMillis;               // Reset timer for the *next* stage wait
          currentState = STEP5_STEPBACK_LIGHTS6_ON;  // Move to the next stage and step
        }
        break;
      case STEP5_STEPBACK_LIGHTS6_ON:
        if (currentMillis - startMillis >= stepDelaymob) {
          strip10.clear();                //turn off left stepBack light (T10). right light (T11) stays on
          strip10.show();                 //one off action
          lightToCheck = stripArray[13];  // prep strip14 lights
          tileSegment = 1;                // pos 1 = lefthalftile; used for struct Segment* segmentName[] in lightOn();
          lightOn();
          startMillis = currentMillis;  // Record start time for the *next* stage wait
          currentState = STEP6_LIGHTS_ON_WAIT_TAP;
        }
        break;
      case STEP6_LIGHTS_ON_WAIT_TAP:
        tap = 0;
        //sensorValue[26] = boardsStruct[3].eA;  //T14A is already loaded
        //sensorValue[27] = boardsStruct[3].eB;  //T14B is already loaded
        indexToCheck = (sensorValue[26] + sensorValue[27]) / 2;  // average of T14A T14B sensor
        if (indexToCheck > weightOn) {                           //continuously check if sensor is pressed at least once
          tap++;                                                 // switch from 0 to 1, we register multiple taps
          Serial.print("tap: ");
          Serial.println(tap);
          tapcounter = 100;  // we set tapcounter to a single score
        }
        if (currentMillis - startMillis >= stepDelaymob) {  // Check if timeout has occurred
          lightToCheck = stripArray[13];                    //clear strip14 lights
          lightToCheck.clear();
          lightToCheck.show();
          maxcount = maxcount + tapcounter;  //handle scoring logic here. we need to do something with this score!
          Serial.print(", total score: ");
          Serial.println(maxcount);
          mobScore++;
          tapcounter = 0;                            // resetting tapcounter
          stepbackleft();                            // Turn on the "step back" lights
          startMillis = currentMillis;               // Reset timer for the *next* stage wait
          currentState = STEP6_STEPBACK_LIGHTS7_ON;  // Move to the next stage and step
        }
        break;
      case STEP6_STEPBACK_LIGHTS7_ON:
        if (currentMillis - startMillis >= stepDelaymob) {
          strip10.clear();               //turn off left stepBack light (T10). right light (T11) stays on
          strip10.show();                //one off action
          lightToCheck = stripArray[6];  // prep strip7 lights
          tileSegment = 1;               // pos 1 = lefthalftile; used for struct Segment* segmentName[] in lightOn();
          lightOn();
          startMillis = currentMillis;  // Record start time for the *next* stage wait
          currentState = STEP7_LIGHTS_ON_WAIT_TAP;
        }
        break;
      case STEP7_LIGHTS_ON_WAIT_TAP:
        tap = 0;
        //sensorValue[12] = boardsStruct[1].fA;  //T7A is already loaded
        //sensorValue[13] = boardsStruct[1].fB;  //T7B is already loaded
        indexToCheck = (sensorValue[12] + sensorValue[13]) / 2;  // average of T7A, T7B sensor
        if (indexToCheck > weightOn) {                           //continuously check if sensor is pressed at least once
          tap++;                                                 // switch from 0 to 1, we register multiple taps
          Serial.print("tap: ");
          Serial.println(tap);
          tapcounter = 100;  // we set tapcounter to a single score
        }
        if (currentMillis - startMillis >= stepDelaymob * 2) {  // Check if timeout has occurred
          lightToCheck = stripArray[6];                         //clear strip7 lights
          lightToCheck.clear();
          lightToCheck.show();
          maxcount = maxcount + tapcounter;  //handle scoring logic here. we need to do something with this score!
          Serial.print(", total score: ");
          Serial.println(maxcount);
          mobScore++;
          tapcounter = 0;                            // resetting tapcounter
          stepbackleft();                            // Turn on the "step back" lights
          startMillis = currentMillis;               // Reset timer for the *next* stage wait
          currentState = STEP7_STEPBACK_LIGHTS8_ON;  // Move to the next stage and step
        }
        break;
      case STEP7_STEPBACK_LIGHTS8_ON:
        if (currentMillis - startMillis >= stepDelaymob) {
          strip10.clear();               //turn off left stepBack light (T10). right light (T11) stays on
          strip10.show();                //one off action
          lightToCheck = stripArray[9];  // prep strip10 lights
          tileSegment = 0;               // pos 0 = righthalftile; used for struct Segment* segmentName[] in lightOn();
          lightOn();
          startMillis = currentMillis;  // Record start time for the *next* stage wait
          currentState = STEP8_LIGHTS_ON_WAIT_TAP;
        }
        break;
      case STEP8_LIGHTS_ON_WAIT_TAP:  //_____________________________________________________HALFWAY
        tap = 0;
        //sensorValue[18] = boardsStruct[2].eA;  //T10A is already loaded
        //sensorValue[19] = boardsStruct[2].eB;  //T10B is already loaded
        indexToCheck = (sensorValue[18] + sensorValue[19]) / 2;  // average of T10A, T10B sensor
        if (indexToCheck > weightOn) {                           //continuously check if sensor is pressed at least once
          tap++;                                                 // switch from 0 to 1, we register multiple taps
          Serial.print("tap: ");
          Serial.println(tap);
          tapcounter = 100;  // we set tapcounter to a single score
        }
        if (currentMillis - startMillis >= stepDelaymob * 2) {  // Check if timeout has occurred
          lightToCheck = stripArray[10];                        //clear strip11 (right) lights
          lightToCheck.clear();
          lightToCheck.show();
          maxcount = maxcount + tapcounter;  //handle scoring logic here. we need to do something with this score!
          Serial.print(", total score: ");
          Serial.println(maxcount);
          mobScore++;
          tapcounter = 0;                // resetting tapcounter
          lightToCheck = stripArray[6];  // prep strip7 lights
          tileSegment = 4;               // pos 4 = Q3; used for struct Segment* segmentName[] in lightOn();
          lightOnR();
          startMillis = currentMillis;              // Reset timer for the *next* stage wait
          currentState = STEP9_LIGHTS_ON_WAIT_TAP;  // Move to the next stage and step
        }
        break;
      case STEP9_LIGHTS_ON_WAIT_TAP:
        tap = 0;
        //sensorValue[13] = boardsStruct[1].fB;  //T7B is already loaded
        indexToCheck = sensorValue[13];  //  T7B sensor
        if (indexToCheck > weightOn) {   //continuously check if sensor is pressed at least once
          tap++;                         // switch from 0 to 1, we register multiple taps
          Serial.print("tap: ");
          Serial.println(tap);
          tapcounter = 100;  // we set tapcounter to a single score
        }
        if (currentMillis - startMillis >= stepDelaymob) {  // Check if timeout has occurred
          lightToCheck = stripArray[6];                     //clear strip7 lights
          lightToCheck.clear();
          lightToCheck.show();
          maxcount = maxcount + tapcounter;  //handle scoring logic here. we need to do something with this score!
          Serial.print(", total score: ");
          Serial.println(maxcount);
          mobScore++;
          tapcounter = 0;                             // resetting tapcounter
          stepbackright();                            // Turn on the "step back" lights
          startMillis = currentMillis;                // Reset timer for the *next* stage wait
          currentState = STEP9_STEPBACK_LIGHTS10_ON;  // Move to the next stage and step
        }
        break;
      case STEP9_STEPBACK_LIGHTS10_ON:
        if (currentMillis - startMillis >= stepDelaymob) {
          strip11.clear();               //turn off right stepBack light (T11). right light (T10) stays on
          strip11.show();                //one off action
          lightToCheck = stripArray[6];  // prep strip7 lights
          tileSegment = 5;               // pos 5 = Q4; used for struct Segment* segmentName[] in lightOn();
          lightOnR();
          startMillis = currentMillis;  // Record start time for the *next* stage wait
          currentState = STEP10_LIGHTS_ON_WAIT_TAP;
        }
        break;
      case STEP10_LIGHTS_ON_WAIT_TAP:
        tap = 0;
        //sensorValue[13] = boardsStruct[1].fB;  //T7B is already loaded
        indexToCheck = sensorValue[13];  //  T7B sensor
        if (indexToCheck > weightOn) {   //continuously check if sensor is pressed at least once
          tap++;                         // switch from 0 to 1, we register multiple taps
          Serial.print("tap: ");
          Serial.println(tap);
          tapcounter = 100;  // we set tapcounter to a single score
        }
        if (currentMillis - startMillis >= stepDelaymob) {  // Check if timeout has occurred
          lightToCheck = stripArray[6];                     //clear strip7 lights
          lightToCheck.clear();
          lightToCheck.show();
          maxcount = maxcount + tapcounter;  //handle scoring logic here. we need to do something with this score!
          Serial.print(", total score: ");
          Serial.println(maxcount);
          mobScore++;
          tapcounter = 0;                              // resetting tapcounter
          stepbackright();                             // Turn on the "step back" lights
          startMillis = currentMillis;                 // Reset timer for the *next* stage wait
          currentState = STEP10_STEPBACK_LIGHTS11_ON;  // Move to the next stage and step
        }
        break;
      case STEP10_STEPBACK_LIGHTS11_ON:
        if (currentMillis - startMillis >= stepDelaymob) {
          strip11.clear();               //turn off right stepBack light (T11). right light (T10) stays on
          strip11.show();                //one off action
          lightToCheck = stripArray[7];  // prep strip8 lights
          tileSegment = 4;               // pos 4 = Q3; used for struct Segment* segmentName[] in lightOn();
          lightOnR();
          startMillis = currentMillis;  // Record start time for the *next* stage wait
          currentState = STEP11_LIGHTS_ON_WAIT_TAP;
        }
        break;
      case STEP11_LIGHTS_ON_WAIT_TAP:
        tap = 0;
        //sensorValue[15] = boardsStruct[1].gB;  //T8B is already loaded
        indexToCheck = sensorValue[15];  //  T8B sensor
        if (indexToCheck > weightOn) {   //continuously check if sensor is pressed at least once
          tap++;                         // switch from 0 to 1, we register multiple taps
          Serial.print("tap: ");
          Serial.println(tap);
          tapcounter = 100;  // we set tapcounter to a single score
        }
        if (currentMillis - startMillis >= stepDelaymob) {  // Check if timeout has occurred
          lightToCheck = stripArray[7];                     //clear strip8 lights
          lightToCheck.clear();
          lightToCheck.show();
          maxcount = maxcount + tapcounter;  //handle scoring logic here. we need to do something with this score!
          Serial.print(", total score: ");
          Serial.println(maxcount);
          mobScore++;
          tapcounter = 0;                              // resetting tapcounter
          stepbackright();                             // Turn on the "step back" lights
          startMillis = currentMillis;                 // Reset timer for the *next* stage wait
          currentState = STEP11_STEPBACK_LIGHTS12_ON;  // Move to the next stage and step
        }
        break;
      case STEP11_STEPBACK_LIGHTS12_ON:
        if (currentMillis - startMillis >= stepDelaymob) {
          strip11.clear();                //turn off right stepBack light (T11). right light (T10) stays on
          strip11.show();                 //one off action
          lightToCheck = stripArray[11];  // prep strip12 lights
          tileSegment = 1;                // pos 1 = lefthalftile; used for struct Segment* segmentName[] in lightOn();
          lightOnR();
          startMillis = currentMillis;  // Record start time for the *next* stage wait
          currentState = STEP12_LIGHTS_ON_WAIT_TAP;
        }
        break;
      case STEP12_LIGHTS_ON_WAIT_TAP:
        tap = 0;
        //sensorValue[22] = boardsStruct[2].gA;  //T12A is already loaded
        //sensorValue[23] = boardsStruct[2].gB;  //T12B is already loaded
        indexToCheck = (sensorValue[22] + sensorValue[23]) / 2;  //av of T12A and T12B sensor
        if (indexToCheck > weightOn) {                           //continuously check if sensor is pressed at least once
          tap++;                                                 // switch from 0 to 1, we register multiple taps
          Serial.print("tap: ");
          Serial.println(tap);
          tapcounter = 100;  // we set tapcounter to a single score
        }
        if (currentMillis - startMillis >= stepDelaymob) {  // Check if timeout has occurred
          lightToCheck = stripArray[11];                    //clear strip12 lights
          lightToCheck.clear();
          lightToCheck.show();
          maxcount = maxcount + tapcounter;  //handle scoring logic here. we need to do something with this score!
          Serial.print(", total score: ");
          Serial.println(maxcount);
          mobScore++;
          tapcounter = 0;                              // resetting tapcounter
          stepbackright();                             // Turn on the "step back" lights
          startMillis = currentMillis;                 // Reset timer for the *next* stage wait
          currentState = STEP12_STEPBACK_LIGHTS13_ON;  // Move to the next stage and step
        }
        break;
      case STEP12_STEPBACK_LIGHTS13_ON:
        if (currentMillis - startMillis >= stepDelaymob) {
          strip11.clear();                //turn off right stepBack light (T11). right light (T10) stays on
          strip11.show();                 //one off action
          lightToCheck = stripArray[15];  // prep strip16 lights
          tileSegment = 2;                // pos 2 = Q1; used for struct Segment* segmentName[] in lightOn();
          lightOnR();
          startMillis = currentMillis;  // Record start time for the *next* stage wait
          currentState = STEP13_LIGHTS_ON_WAIT_TAP;
        }
        break;
      case STEP13_LIGHTS_ON_WAIT_TAP:
        tap = 0;
        //sensorValue[30] = boardsStruct[3].gA;  //T16A is already loaded
        indexToCheck = sensorValue[30];  //T16B sensor
        if (indexToCheck > weightOn) {   //continuously check if sensor is pressed at least once
          tap++;                         // switch from 0 to 1, we register multiple taps
          Serial.print("tap: ");
          Serial.println(tap);
          tapcounter = 100;  // we set tapcounter to a single score
        }
        if (currentMillis - startMillis >= stepDelaymob) {  // Check if timeout has occurred
          lightToCheck = stripArray[15];                    //clear strip16 lights
          lightToCheck.clear();
          lightToCheck.show();
          maxcount = maxcount + tapcounter;  //handle scoring logic here. we need to do something with this score!
          Serial.print(", total score: ");
          Serial.println(maxcount);
          mobScore++;
          tapcounter = 0;                              // resetting tapcounter
          stepbackright();                             // Turn on the "step back" lights
          startMillis = currentMillis;                 // Reset timer for the *next* stage wait
          currentState = STEP13_STEPBACK_LIGHTS14_ON;  // Move to the next stage and step
        }
        break;
      case STEP13_STEPBACK_LIGHTS14_ON:
        if (currentMillis - startMillis >= stepDelaymob) {
          strip11.clear();                //turn off right stepBack light (T10). left light (T10) stays on
          strip11.show();                 //one off action
          lightToCheck = stripArray[14];  // prep strip15 lights
          tileSegment = 0;                // pos 0 = righthalftile; used for struct Segment* segmentName[] in lightOn();
          lightOnR();
          startMillis = currentMillis;  // Record start time for the *next* stage wait
          currentState = STEP14_LIGHTS_ON_WAIT_TAP;
        }
        break;
      case STEP14_LIGHTS_ON_WAIT_TAP:
        tap = 0;
        //sensorValue[28] = boardsStruct[3].fA;  //T14A is already loaded
        //sensorValue[29] = boardsStruct[3].fB;  //T14B is already loaded
        indexToCheck = (sensorValue[28] + sensorValue[29]) / 2;  //av of T14A and T14B sensor
        if (indexToCheck > weightOn) {                           //continuously check if sensor is pressed at least once
          tap++;                                                 // switch from 0 to 1, we register multiple taps
          Serial.print("tap: ");
          Serial.println(tap);
          tapcounter = 100;  // we set tapcounter to a single score
        }
        if (currentMillis - startMillis >= stepDelaymob) {  // Check if timeout has occurred
          lightToCheck = stripArray[14];                    //clear strip5 lights
          lightToCheck.clear();
          lightToCheck.show();
          maxcount = maxcount + tapcounter;  //handle scoring logic here. we need to do something with this score!
          Serial.print(", total score: ");
          Serial.println(maxcount);
          mobScore++;
          tapcounter = 0;                              // resetting tapcounter
          stepbackright();                             // Turn on the "step back" lights
          startMillis = currentMillis;                 // Reset timer for the *next* stage wait
          currentState = STEP14_STEPBACK_LIGHTS15_ON;  // Move to the next stage and step
        }
        break;
      case STEP14_STEPBACK_LIGHTS15_ON:
        if (currentMillis - startMillis >= stepDelaymob) {
          strip11.clear();               //turn off right stepBack light (T11). left light (T10) stays on
          strip11.show();                //one off action
          lightToCheck = stripArray[5];  // prep strip6 lights
          tileSegment = 0;               // pos 0 = righthalftile; used for struct Segment* segmentName[] in lightOn();
          lightOnR();
          startMillis = currentMillis;  // Record start time for the *next* stage wait
          currentState = STEP15_LIGHTS_ON_WAIT_TAP;
        }
        break;
      case STEP15_LIGHTS_ON_WAIT_TAP:
        tap = 0;
        //sensorValue[10] = boardsStruct[1].eA;  //T6A is already loaded
        //sensorValue[11] = boardsStruct[1].eB;  //T6B is already loaded
        indexToCheck = (sensorValue[10] + sensorValue[11]) / 2;  //av of T14A and T14B sensor
        if (indexToCheck > weightOn) {                           //continuously check if sensor is pressed at least once
          tap++;                                                 // switch from 0 to 1, we register multiple taps
          Serial.print("tap: ");
          Serial.println(tap);
          tapcounter = 100;  // we set tapcounter to a single score
        }
        if (currentMillis - startMillis >= stepDelaymob * 2) {  // Check if timeout has occurred
          lightToCheck = stripArray[5];                         //clear strip6 lights
          lightToCheck.clear();
          lightToCheck.show();
          maxcount = maxcount + tapcounter;  //handle scoring logic here. we need to do something with this score!
          Serial.print(", total score: ");
          Serial.println(maxcount);
          mobScore++;
          tapcounter = 0;                              // resetting tapcounter
          stepbackright();                             // Turn on the "step back" lights
          startMillis = currentMillis;                 // Reset timer for the *next* stage wait
          currentState = STEP15_STEPBACK_LIGHTS16_ON;  // Move to the next stage and step
        }
        break;
      case STEP15_STEPBACK_LIGHTS16_ON:
        if (currentMillis - startMillis >= stepDelaymob * 2) {
          strip11.clear();                //turn off right stepBack light (T11). right light (T10) stays on
          strip11.show();                 //one off action
          lightToCheck = stripArray[10];  // prep strip11 lights
          tileSegment = 1;                // pos 1 = lefthalftile; used for struct Segment* segmentName[] in lightOn();
          lightOnR();
          startMillis = currentMillis;  // Record start time for the *next* stage wait
          currentState = STEP16_LIGHTS_ON_WAIT_TAP;
        }
        break;
      case STEP16_LIGHTS_ON_WAIT_TAP:
        tap = 0;
        //sensorValue[20] = boardsStruct[2].fA;  //T11A is already loaded
        //sensorValue[21] = boardsStruct[2].fB;  //T11B is already loaded
        indexToCheck = (sensorValue[20] + sensorValue[21]) / 2;  //av of T14A and T14B sensor
        if (indexToCheck > weightOn) {                           //continuously check if sensor is pressed at least once
          tap++;                                                 // switch from 0 to 1, we register multiple taps
          Serial.print("tap: ");
          Serial.println(tap);
          tapcounter = 100;  // we set tapcounter to a single score
        }
        if (currentMillis - startMillis >= stepDelaymob * 2) {  // Check if timeout has occurred, lights are still on!
          maxcount = maxcount + tapcounter;                     //handle scoring logic here. we need to do something with this score!
          Serial.print(", total score: ");
          Serial.println(maxcount);
          mobScore++;
          tapcounter = 0;                // resetting tapcounter
          exCounter++;                   //from 0 to 1 first round, set to 2 in second round
          if (exCounter == 2) {          // if last one: send mobScore off to data ESP
            mobScore = mobScore / 0.32;  //convert number from 0-32 to percentage
            myResults.id = 6;            // sent from the game ESP
            //myResults.b = buttonInput; //which calib was it? 94
            //myResults.sd = stepDelaymob;  //what delay was used? 750
            myResults.dA = mobScore;  // percentage 0 - 100
            myResults.eB = maxcount;  // number from 0 to 3200
            // Send message1 via ESP-NOW to data ESP
            esp_err_t result1 = esp_now_send(broadcastAddress1, (uint8_t *)&myResults, sizeof(myResults));
            //if (result1 == ESP_OK) {
            //Serial.print(", myResults was sent with success");
            //} else {
            //Serial.println("Error sending myResults");
            //}
            exCounter = 0;  //reset the exCounter
            maxcount = 0;
            mobScore = 0;
            cleanUp();                   //wipe all the tiles
            currentState = STEP16_DONE;  // Move into idle stage
          } else if (exCounter == 1) {
            startMillis = currentMillis;                   // Reset timer for the *next* stage wait
            currentState = STEP1_WAIT_PREP_OFF_LIGHTS_ON;  // Move to the first stage and step, lights are still on
          }
        }  //end  of timeout
        break;
      case STEP16_DONE:  // idle state
        break;
    }  // end switch
  }  // end of calib4 */


// ======================= CALIB 5 - LUNGE ====================

void calib5() {                            // exercise 5: alternating LUNGES, 5x left, 5x right, total time 30s
  unsigned long currentMillis = millis();  // get the time now?
  switch (currentLungeState) {
    case LUNGEPREP:
      currentResetState = RESET;    //make reset available
      cleanUp();                    //needed?
      endSet();                     // initial setup:lights on, no wait
      startMillis = currentMillis;  // Record start time for the *next* stage wait
      balanceScoreDyn = 0;
      balChecker = 0;
      balanceAchieved = 0;
      exCounter = 0;
      currentLungeState = LUNGE1START;  // Move to the next stage
      break;
    case LUNGE1START:                                        // left foot back, right stays on
      if (currentMillis - startMillis >= (stepDelay * 4)) {  // once prep duration is over...
        cleanUp();
        rightStep();                    // Start the lights for balancing on right leg
          for (int i = 25; i < 35; i++) {  // For each pixel...
          pixels.setPixelColor(i, left);     // colour one at a time
          pixels.show();                       // Send the updated pixel colors to the hardware
          delay(30);                           // one at a time
        }
        startMillis = currentMillis;    // Reset timer for the *next* stage wait
        currentLungeState = LUNGE1END;  // Move to the next stage
      }
      break;
    case LUNGE1END:  //balance on right foot - additive
      //check left sensors for weightOff, right sensors for weightOn
      if (leftToe < weightOn && leftHeel < weightOn && rightToe > weightOn && rightHeel > weightOn) {
        balChecker++;                         // add up for every ms the condition is true, could be 0 - 2,000 - goes up
        balanceAchieved = (balChecker / 20);  // we convert ms in balance into a percentage for L balance 0 - 100 - goes up
      }
      if (currentMillis - startMillis >= lungeDelay) {        // Check if timeout has occurred 2000ms
        cleanUp();                                            //lights off
        //endSet();                                             // lights on
        balanceScoreDyn = balanceScoreDyn + balanceAchieved;  //0 plus percentage achieved in R balance
        //Serial.print(", total score: ");
        //Serial.println(balanceScore);
        balChecker = 0;  // resetting balChecker
        balanceAchieved = 0;
        exCounter++;
        startMillis = currentMillis;      // Reset timer for the *next* stage wait
        currentLungeState = LUNGE2START;  // Move to the next stage and step
      }
      break;
    case LUNGE2START:                                        // right foot back, left stays on
      if (currentMillis - startMillis >= (stepDelay * 4)) {  // once prep duration is over...
        cleanUp();
        leftStep();                     // Start the lights for balancing on left leg
        pixels.fill(right, 14, 1);     // colour one at a time
        pixels.show();                       // Send the updated pixel colors to the hardware
        delay(30);   
        pixels.fill(right, 13, 1);     // colour one at a time
        pixels.show();                 // Send the updated pixel colors to the hardware
        delay(30);
        pixels.fill(right, 12, 1);     // colour one at a time
        pixels.show();                 // Send the updated pixel colors to the hardware
        delay(30);
        pixels.fill(right, 11, 1);     // colour one at a time
        pixels.show();                 // Send the updated pixel colors to the hardware
        delay(30);
        pixels.fill(right, 10, 1);     // colour one at a time
        pixels.show();                 // Send the updated pixel colors to the hardware
        delay(30);
        pixels.fill(right, 49, 1);     // colour one at a time
        pixels.show();                       // Send the updated pixel colors to the hardware
        delay(30);   
        pixels.fill(right, 48, 1);     // colour one at a time
        pixels.show();                 // Send the updated pixel colors to the hardware
        delay(30);
        pixels.fill(right, 47, 1);     // colour one at a time
        pixels.show();                 // Send the updated pixel colors to the hardware
        delay(30);
        pixels.fill(right, 46, 1);     // colour one at a time
        pixels.show();                 // Send the updated pixel colors to the hardware
        delay(30);
        pixels.fill(right, 45, 1);     // colour one at a time
        pixels.show();                 // Send the updated pixel colors to the hardware
        delay(30);
        startMillis = currentMillis;    // Reset timer for the *next* stage wait
        currentLungeState = LUNGE2END;  // Move to the next stage
      }
      break;
    case LUNGE2END:  //balance on left leg - additive
      //check left sensors for weightOn, right sensors for weightOff
      if (leftToe > weightOn && leftHeel > weightOn && rightToe < weightOn && rightHeel < weightOn) {
        balChecker++;                         // add up for every ms the condition is true, could be 0 - 2,000 - goes up
        balanceAchieved = (balChecker / 20);  // we convert ms in balance into a percentage for L balance 0 - 100 - goes up
      }
      if (currentMillis - startMillis >= lungeDelay) {  // Check if timeout has occurred 2000ms
        cleanUp();
        //endSet();
        balanceScoreDyn = balanceScoreDyn + balanceAchieved;  //R balance plus percentage achieved in L balance
        //Serial.print(", total score: ");
        //Serial.println(balanceScore);
        exCounter++;
        if (exCounter == 6) {                     //last one
          balanceScoreDyn = balanceScoreDyn / 6;  //average after 10 lunges
          myResults.id = 2;                        // sent from the game ESP
          //myResults.b = buttonInput; //which calib was it? 95
          //myResults.sd = stepDelay;  //what delay was used?
          myResults.gA = balanceScoreDyn;  // percentage 0 - 100
          // Send message1 via ESP-NOW to data ESP
          //esp_err_t result1 = esp_now_send(receiverAddress, (uint8_t *)&myResults, sizeof(myResults));
          sendMessage(gameAddress,myResults);
          //if (result1 == ESP_OK) {
          //Serial.print(", myResults was sent with success");
          //} else {
          //Serial.println("Error sending myResults");
          //}
          balChecker = 0;  // resetting balChecker
          balanceAchieved = 0;
          exCounter = 0;  //reset the exCounter
          balanceScoreDyn = 0;
          successResult();
          cleanUp();
          currentLungeState = LUNGEDONE;
        } else {           //we have not reached 10 lunge reps
          balChecker = 0;  // resetting balChecker
          balanceAchieved = 0;
          startMillis = currentMillis;      // Reset timer for the *next* stage wait
          currentLungeState = LUNGE1START;  // Move to the next stage and step
        }
      }
      break;
    case LUNGEDONE:  // idle state, wait for button press
      break;
  }  //end of switch
}  // end of calib5

// ======================= CALIB 6 - SQUATS ====================

// PREPARATIONS FOR CALIB 6
void squatlights() {
  pixels.fill(yellow, 10, 50);
  pixels.show();
  delay (stepDelay *0.5);
  pixels.fill(orange, 10, 50);
  pixels.show();
  delay (stepDelay *0.5);
  pixels.fill(red, 10, 50);
  pixels.show();
  delay (stepDelay *0.5);
  pixels.fill(magenta, 10, 50);
  pixels.show();
  delay (stepDelay *0.5);
  pixels.fill(purple, 10, 50);
  pixels.show();
  delay (stepDelay *0.5);
  pixels.fill(blue, 10, 50);
  pixels.show();
  delay (stepDelay *0.5);
  pixels.fill(cyan, 10, 50);
  pixels.show();
  delay (stepDelay *0.5);
  pixels.fill(green, 10, 50);
  pixels.show();
}
void calib6() {                            // exercises 6 a and b: narrow squat x 3 (10s), wide squat with pulses x 3 (20s), total time 30s
  unsigned long currentMillis = millis();  // get the time now?
  switch (currentSquatState) {
    case SQUATPREP:
      currentResetState = RESET;    //make reset available
      endSet();                 // initial setup (lights on), wait
      pixels.fill(white, 50, 10);
      startMillis = currentMillis;  // Record start time for the *next* stage wait
      //squatScore = 0;
      exCounter = 0;
      currentSquatState = SQUAT1START;  // Move to the next stage
      break;
    case SQUAT1START:
      if (currentMillis - startMillis >= (stepDelay * 4)) {  // once 2s prep duration is over...
        cleanUp();
        squatlights();                  // Start the white lights for squatting
        startMillis = currentMillis;    // Reset timer for the *next* stage wait
        currentSquatState = SQUAT1END;  // Move to the next stage
      }
      break;
    case SQUAT1END:
      //squatCounter = 0;
      // check T10A, T10B sensor, T11A, T11B sensor for weight
      /*if (sensorValue[18] > weightOn && sensorValue[19] > weightOn && sensorValue[20] > weightOn && sensorValue[21] > weightOn) {
        squatCounter++;    // switch from 0 to 1, can be multiples
        squatScore = 100;  // we set a single score for this squat
      }*/
      if (currentMillis - startMillis >= (squat1Delay *1.2)) {  // Check if timeout has occurred 2s
        cleanUp();
        //strengthScore = strengthScore + squatScore;  //handle scoring logic here. we need to do something with this score!
        //Serial.print(", total score: ");
        //Serial.println(strengthScore);
        exCounter++;
        //squatScore = 0;                   // resetting squat score
        startMillis = currentMillis;      // Reset timer for the *next* stage wait
        currentSquatState = SQUATEND;  // Move to the next stage and step
      }
      break;
    case SQUATEND:
      //squatCounter = 0;
      // check T10A, T10B sensor, T11A, T11B sensor for weight
      /*if (sensorValue[18] > weightOn && sensorValue[19] > weightOn && sensorValue[20] > weightOn && sensorValue[21] > weightOn) {
        squatCounter++;    // switch from 0 to 1, can be multiples
        squatScore = 100;  // we set a single score for this squat
      }*/
      if (currentMillis - startMillis >= (squat1Delay)) {  // once delay is over...
        if (exCounter == 4) {  //last jump
          successResult();
          cleanUp();
          exCounter = 0;
          currentSquatState = SQUATDONE;   // Move to the idle stage
        } 
        else {                            // not last squat
          startMillis = currentMillis;      // Reset timer for the *next* stage wait
          currentSquatState = SQUAT1START;  // Move to the next stage, no lights
        }
      }  //end of timercheck
      break;
    case SQUATDONE:  // idle state
      break;
  }  // end of switch
}  // end of calib6 

// ======================= CALIB 7 - JUMPS ====================

void calib7() {                            // exercise 7:  airtime x 3, total time for jumping 22.5s.
  unsigned long currentMillis = millis();  // get the time now?
  switch (currentJumpingState) {
    case JUMPPREP:
      currentResetState = RESET;  //make reset available
      cleanUp();                  //needed?
      pixels.fill(white, 10, 40);  // TiLE On outside
      pixels.fill(white, 50, 10);  // midline
      pixels.show();
      startMillis = currentMillis;  // Record start time for the *next* stage wait
      exCounter = 0;
      currentJumpingState = JUMPBENCHMARK;  // Move to the next stage
      break;
    case JUMPBENCHMARK:  // jumpwindow is 0
      /*if (leftToe > weightOn || leftHeel > weightOn || rightToe > weightOn || rightHeel > weightOn) {
        myResults.id = 2;    // sent from the game ESP to yb ESP
        myResults.gB = 111;  //DONT USE gB! number to indicate person is on tile, balancing
        esp_err_t result1 = esp_now_send(receiverAddress, (uint8_t *)&myResults, sizeof(myResults));
      }*/
      if (currentMillis - startMillis >= (stepDelay * 4)) {  //after 2s white, blue lights on
        //switch lights over for jumping
        cleanUp();
        pixels.fill(blue, 10, 40);  // TiLE On outside
        pixels.fill(blue, 50, 10);  // midline
        pixels.show();
        startMillis = currentMillis;      // Record start time for the *next* stage wait
        currentJumpingState = JUMPSTART;  // Move to the next stage
      }
      break;
    case JUMPSTART:  // jumpwindow is 0, lights are off
      /*if (sensorValue[26] > weightOn || sensorValue[27] > weightOn || sensorValue[28] > weightOn || sensorValue[29] > weightOn) {
        myResults.id = 6;    // sent from the game ESP to data ESP
        myResults.gB = 111;  //number to indicate person on tile, balancing
        esp_err_t result1 = esp_now_send(broadcastAddress1, (uint8_t *)&myResults, sizeof(myResults));
      }*/
      if (currentMillis - startMillis >= (stepDelay * 5)) {  //after 2.5s dip, lights off, indicate the jump, start jumpwindow
        //switch lights over for jumping;
        cleanUp();
        sendAudioMsg(9,0,101,0,0); //Fail sfx, Silence Mus, Volumes.
        pixels.fill(orange, 0, 60);  // LEFT TiLE
        //pixels.fill(orange, 50, 10);  // RIGHT Tile
        pixels.show();
        startMillis = currentMillis;    // Record start time for the *next* stage wait
        currentJumpingState = JUMPOFF;  // Move to the next stage
      }
      break;
    case JUMPOFF:
      /*if (leftToe < weightOn && leftHeel < weightOn && rightToe < weightOn && rightHeel < weightOn) {
        myResults.id = 2;    // sent from the game ESP to yb ESP
        myResults.gB = 333;  // number to indicate person in the air
        esp_err_t result1 = esp_now_send(receiverAddress, (uint8_t *)&myResults, sizeof(myResults));
      }*/
      // else no take off, we send nothing (but have not wiped the 222) and wait for time out
      if (currentMillis - startMillis >= airtime) {  //after airtime wait, prep landing
        cleanUp();                                         // wipe jumpoff colour
        startMillis = currentMillis;                       // Record start time for the *next* stage wait
        currentJumpingState = JUMPLANDPREP;                // Move to the next stage
      }
      break;
    case JUMPLANDPREP:
      if (leftToe < weightOn && leftHeel < weightOn && rightToe < weightOn && rightHeel < weightOn) {
        /*myResults.id = 2;    // sent from the game ESP to yb ESP
        myResults.gB = 333;  // number to indicate person in the air
        esp_err_t result1 = esp_now_send(receiverAddress, (uint8_t *)&myResults, sizeof(myResults));*/
      } else if (leftToe > weightOn || leftHeel > weightOn || rightToe > weightOn || rightHeel > weightOn) {
        //either landed or never took off
        /*myResults.id = 2;    // sent from the game ESP to data ESP
        myResults.gB = 444;  //number to indicate person on tile, balancing
        esp_err_t result2 = esp_now_send(receiverAddress, (uint8_t *)&myResults, sizeof(myResults));*/
      }
      if (currentMillis - startMillis >= (stepDelay / 2)) {  //after short break, indicate the landing
        pixels.fill(blue, 10, 40);                           //  TiLE is blue
        pixels.fill(blue, 50, 10);
        pixels.show();
        startMillis = currentMillis;     // Record start time for the *next* stage wait
        currentJumpingState = JUMPLAND;  // Move to the next stage
      }
      break;
    case JUMPLAND:  // lights are blue
      if (leftToe > weightOn || leftHeel > weightOn || rightToe > weightOn || rightHeel > weightOn) {
        //either landed or never took off
        /*myResults.id = 2;    // sent from the game ESP to data ESP
        myResults.gB = 444;  //number to indicate person on tile, balancing
        esp_err_t result1 = esp_now_send(receiverAddress, (uint8_t *)&myResults, sizeof(myResults));*/
      }
      if (currentMillis - startMillis >= (stepDelay * 4)) {  //after 2s, while person balances the landing tiles are blue
        cleanUp();
        /*myResults.id = 2;  // sent from the game ESP to the yb ESP
        myResults.gB = 0;  // reset jumpwindow
        esp_err_t result2 = esp_now_send(receiverAddress, (uint8_t *)&myResults, sizeof(myResults));*/
        startMillis = currentMillis;    // Record start time for the *next* stage wait
        currentJumpingState = JUMPEND;  // Move to the next stage
      }
      break;
    case JUMPEND:                                            // no lights
      if (currentMillis - startMillis >= (stepDelay * 2)) {  // once delay is over...
        exCounter++;
        if (exCounter == 1) {  //last jump
          successResult();
          cleanUp();
          exCounter = 0;
          currentJumpingState = JUMPDONE;   // Move to the idle stage
        } else {                            // not last jump
          startMillis = currentMillis;      // Reset timer for the *next* stage wait
          currentJumpingState = JUMPSTART;  // Move to the next stage, no lights
        }
      }  //end of timercheck
      break;
    case JUMPDONE:  //idle state
      break;
  }  // end of switch for calib7
}  //end of calib7

// ======================= CALIB 8 - CELEBRATION ====================

void calibend() {
  switch (currentFinalState) {
    case FINAL:
      {
      pixels.clear();                        // Set all pixel colors to 'off'
      delay(1500);                        
      for (int i = 0; i < NUMPIXELS; i++) {  // For each pixel...
        pixels.setPixelColor(i, purple);     // colour one at a time
        pixels.show();                       // Send the updated pixel colors to the hardware
        delay(45);                           // one at a time
      }
      gameSuccess = 5;  // safety
      Serial.println("fanfare");
        /*for (int i = 0; i < NUMPIXELS; i++) {  // Fill all strips with purple
          pixels.fill(purple, 0, 60);
        }
        for (int i = 0; i < NUMPIXELS; i++) {  // Show all strips at once
          pixels.show();
          delay(200);  //leave them on, does not show all of them?
        }*/
        delay(stepDelay * 4);  // Keep them lit for x seconds. delay does not work
        exCounter = 0;
        currentResetState = RESET;  //make reset available
        cleanUp();
        currentFinalState = FINALDONE;
      }
      break;
    case FINALDONE:
      break;
  }      // end of switch
}  //end of calibend


// =====================================================================================
//----------------------------------------DISPLAY RESULTS-------------------------------
// =====================================================================================


void successResult() {                     // we want to send back "success":
  unsigned long currentMillis = millis();  // get the time now?
  switch (currentResultState) {
    case RESULT:
      gameSuccess = 0; //to play sound 0 = success
      sendAudioMsg(1,2,101,0,0); //Fail sfx, Silence Mus, Volumes.
      pixels.clear();              // Set all pixel colors to 'off'
      pixels.fill(green, 10, 40);  //outside
      pixels.fill(green, 50, 10);
      pixels.fill(green, 0, 10);
      pixels.show();
      gameSuccess = 5;  // safety
      Serial.println("success");
      delay(stepDelay * 2);  // Keep them lit for x seconds.
      cleanUp();
      currentResultState = RESULTDONE;  // Move to the next stage
      break;
    case RESULTDONE:  // idle, wait for button press
      break;
  } //end of switch
}// end of successResult
void partialResult() {
  switch (currentResultState) {
    case RESULT:
      gameSuccess = 1; //to play sound 1 = partial
      sendAudioMsg(2,2,101,0,0); //Fail sfx, Silence Mus, Volumes.
      pixels.clear();              // Set all pixel colors to 'off'
      pixels.fill(yellow, 10, 40);  //outside
      pixels.fill(yellow, 50, 10);
      pixels.fill(yellow, 0, 10);
      pixels.show();
      gameSuccess = 5;  // safety
      Serial.println("success");
      delay(stepDelay * 2);  // Keep them lit for x seconds.
      cleanUp();
      currentResultState = RESULTDONE;  // Move to the next stage
      break;
    case RESULTDONE:  // idle, wait for button press
      break;
  } //end of switch
}
void failResult() {
  switch (currentResultState) {
    case RESULT:
      gameSuccess = 2; //to play sound 2 = fail
      sendAudioMsg(3,2,101,0,0); //Fail sfx, Silence Mus, Volumes.
      pixels.clear();              // Set all pixel colors to 'off'
      pixels.fill(red, 10, 40);  //outside
      pixels.fill(red, 50, 10);
      pixels.fill(red, 0, 10);
      pixels.show();
      gameSuccess = 5;  // safety
      Serial.println("success");
      delay(stepDelay * 2);  // Keep them lit for x seconds.
      cleanUp();
      currentResultState = RESULTDONE;  // Move to the next stage
      break;
    case RESULTDONE:  // idle, wait for button press
      break;
  } //end of switch
}

void endSet() {                //we dont send
  pixels.clear();              // Set all pixel colors to 'off'
  pixels.fill(white, 10, 40);  // TiLE On outside
  pixels.show();               // Send the updated pixel colors to the hardware
}


// ====================================    ESP SENDING MESSAGES AND ADDING PEERS

//allows for ease of sending audio messages.
void sendAudioMsg(int p_sfx, int p_bg, int p_sfxVol, int p_bgVol, int p_bgLoop)
{
  audioMessage.dA = p_sfx;
  audioMessage.dB = p_bg;
  audioMessage.eA = p_sfxVol;
  audioMessage.eB = p_bgVol;
  audioMessage.gB = p_bgLoop;
  sendMessage(ybAddress, audioMessage);
}

//Allows for ease of sending message
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

//Allows for adding multiple peers easier.
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

//Resets only the state of the exercise switch cases.
void resetStates()
{
  currentMarchState = MARCHPREP;  //reset calib1
  currentBalState = BAL1PREP;     //reset calib2
  currentSideState = SIDEPREP;    //reset calib3
  //currentState = STEP1_PREP;       //reset calib4
  currentLungeState = LUNGEPREP;  //reset calib5
  currentSquatState = SQUATPREP;   //reset calib6
  currentJumpingState = JUMPPREP;  //reset calib7
  currentFinalState = FINAL;       //reset calibend
  currentResultState = RESULT;     //reset results
  //currentRopeState = GAMEPREP;     //reset jumpRope game
}
