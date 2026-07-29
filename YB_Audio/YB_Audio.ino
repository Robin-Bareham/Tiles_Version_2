// yb CODE SENDING AND RECEIVING
#include <Arduino.h>
#include <SD.h>
#include <FS.h>
#include <SPI.h>

#include "Structs.h"
#include "audio_data.h"
//For the next libraies to work, if AudioTools by pschatzmann isn't available in the library
//Will have to manually install them via the termial doing 
// 'git install https://github.com/pschatzmann/arduino-audio-tools' into the Arduino Library Folder.
#include "AudioTools.h"
#include "AudioTools/AudioCodecs/CodecWAV.h"
#include "AudioTools/CoreAudio/ResampleStream.h"

// SD Card pins for YB-ESP32-S3-AMP
#define SD_CS 10
#define SD_MOSI 11
#define SD_MISO 13
#define SD_SCK 12

// I2S pins for YB-ESP32-S3-AMP (connected to MAX98357A amplifiers)
#define I2S_BCLK 5  // Bit clock
#define I2S_LRC 6   // Frame clock (LRCLK)
#define I2S_DOUT 7  // Digital audio signal (DIN)

// Status LED
#define STATUS_LED 47

#define AUDIO_BUFFER_SIZE 1024

// ----- Audio Object Definitions ------

// ---- Audio Objects
I2SStream i2s;
InputMixer<int16_t> mixer; //Allows for two sounds at once.
StreamCopy copier(i2s,mixer,AUDIO_BUFFER_SIZE); //Where the dual audio gets coppied into.
AudioInfo info(44100,2,16); //Rate of the WAV Files

// ---- Background Music ----
File bgFile; //SD Path
WAVDecoder bgDecoder;
EncodedAudioStream bgStream(&bgFile,&bgDecoder);
VolumeStream bgVolume(bgStream); //Controls Audio's volume

int bgIndex = -1; //Index location of background track in mixer
float bgVol = 0.50; //Base volume of background, Game ESP overwrites this
String currentPath; //Path of the audio currently playing for looping purposes

// ---- Sound effects ----
SfxVoice sfx_voices[2]; //Amount of Sound effcts that could play at once.
int totalSfxVoices = 2;
int currentSfx = 0; //Acts as a pointer
bool isSfxPlaying = false;
float sfxVolValue = 1.0; //Base volume of sound effects, Game ESP overwrites this.

//Sound Effect start and end times (Milliseconds).
static uint32_t sfxStartTime = 0;
uint32_t sfxDurationMs = 1450; 
//Automatically set to 1.45 seconds as that's the longest sfx currently.
// in audio_data.h in the 'sfxList', the last number is the duration of the song in milliseconds 
//(do it slightly shorter than the actual duration of the song to avoid a clack noise)


AudioFile audioFiles[100]; //List of all WAV files from SD card
int totalAudioFiles = 0;
bool isPlaying = false;
String serialBuffer = "";
AudioFile musicFiles[15]; //List of all WAV Background Music files from SD card
int totalMusicFiles = 0;

//Serial Monitor Debugging
String menu = "main"; //"main", "bg", "sfx"


#include <esp_now.h>  //===============================================ESP
#include <WiFi.h>

// 1. THE MAC ADDRESS OF THE 1Tile LED ESP BOARD (=receiver). we send to this address.
uint8_t receiverAddress1[] = { 0xEC, 0xDA, 0x3B, 0x95, 0xC5, 0xC8 }; //Tile 1 Reciever (ec:da:3b:95:c5:c8)
uint8_t receiverAddress2[] = { 0xEC, 0xDA, 0x3B, 0x96, 0xEA, 0xB0 }; //Game ESP reciever (ec:da:3b:96:ea:b0)

struct_message_all myResults;  // Message that hold all the data received.

//variables incoming and outgoing
bool dataReceived = false;
//Audio Inputs 0 means nothing happnens.
int recvSfx = 0; //What sound effect will be played
int recvBg = 0; //What background effect will be played
int recvBgVol = bgVol; //What volume the background music will be at
int recvSfxVol = sfxVolValue; //What volume the sound effects will be at
int recvBgLooping = 0; //If the background won't loop, 1=it plays once.


// // --- CALLBACK: DATA SENT --- (Shouldn't need this)
// void OnDataSent(const esp_now_send_info_t *info, esp_now_send_status_t status) {
//   Serial.print("\r\nLast Packet Send Status:\t");
//   Serial.println(status == ESP_NOW_SEND_SUCCESS ? "Delivery Success" : "Delivery Fail");
// }

// --- CALLBACK: DATA RECEIVED ---
void OnDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  //const uint8_t *mac_addr = info->src_addr;// Extract MAC address of sender
  memcpy(&myResults, incomingData, sizeof(myResults));
  //Will need to put in responses for background mus, sfx opt, bg + sfx vol, bg looping
  //Need to allocate which ones which.
  recvSfx = myResults.dA;
  recvBg = myResults.dB;
  recvSfxVol = myResults.eA;
  recvBgVol = myResults.eB;
  recvBgLooping = myResults.gB;
  Serial.println("Package Recieved.");
  dataReceived = true;
}

//===================================================== Setup Functions

// --- Initate Sound Effects Structure

void initVoices(SfxVoice &v)
{
  //Pointers are used as they make changing the sound effect playing easier
  //As the sound effect part of the mixer isn't always full (has something playing)
  //Without pointers it either doesn't work or sounds buggy.
  v.decoder = new WAVDecoder();
  v.memory = new MemoryStream(nullptr,0,false);
  v.stream = new EncodedAudioStream(v.memory,v.decoder);

  //Placed last as it then allows the sfx to have their volume changed
  v.volume = new VolumeStream(*v.stream);
  v.volume->setAudioInfo(info);
  v.volume->begin();

  v.stream->begin();
  v.decoder->begin();

  v.mixerIndex = mixer.add(*v.volume, 0);
  v.active = false;
  v.startTime = 0;
  v.endTime = 1450;
}

// --- Initiate Audio 
void audioSetup() 
{
  // Configure I2S Settings
  auto i2s_config = i2s.defaultConfig(TX_MODE);
  i2s_config.pin_bck = I2S_BCLK;
  i2s_config.pin_ws = I2S_LRC;
  i2s_config.pin_data = I2S_DOUT;
  i2s_config.sample_rate = 44100;    
  i2s_config.bits_per_sample = 16;
  i2s_config.channels = 2;
  i2s_config.buffer_size = AUDIO_BUFFER_SIZE;
  i2s_config.buffer_count = 10; 
  i2s.begin(i2s_config);

  mixer.begin(info);
  //Loading SFX Sounds based on STRUCT calling from Flash Memory
  for(int i = 0; i < totalSfxVoices; i++)
  {
    initVoices(sfx_voices[i]);
  } 
}

// ---- Scan Audio files

void scanAudioFiles() {
  File root = SD.open("/");
  if (!root) {
    Serial.println("Failed to open root directory");
    return;
  }

  totalAudioFiles = 0;
  //Allows the first position in the music files to be empty
  //So when nothing gets called for music it won't change anything.
  musicFiles[0].name = "Empty";
  musicFiles[0].hasWav = false;
  musicFiles[0].wavPath = "";
  totalMusicFiles = 1;

  File file = root.openNextFile();

  while (file) {
    String fileName = String(file.name());
    //Checks if the audio will be used for background music
    //If adding new music to the SD card make sure it starts with lowercase m, or it won't be included.
    bool currentMus = false;
    if(fileName.charAt(0) == 'm')
    {
      Serial.println("Music Found");
      currentMus = true;
    }
    //Adds audio if it's a WAV file. Only WAV files will work with the mixer.
    if (!file.isDirectory() && (fileName.endsWith(".wav") || fileName.endsWith(".WAV"))) 
    {

      // Get base name without extension
      String baseName = fileName;
      int lastDot = baseName.lastIndexOf('.');
      if (lastDot > 0) {
        baseName = baseName.substring(0, lastDot);
      }

      String fullPath = "/" + fileName;

      // Create new entry
      if (totalAudioFiles < 100) 
      {
        //Adds it to list of all WAV files in SD card
        audioFiles[totalAudioFiles].name = baseName;
        audioFiles[totalAudioFiles].hasWav = true;
        audioFiles[totalAudioFiles].wavPath = fullPath;
        totalAudioFiles++;
        //If it's background music, adds it to the music list.
        if(currentMus)
        {
          musicFiles[totalMusicFiles].name = baseName;
          musicFiles[totalMusicFiles].hasWav = true;
          musicFiles[totalMusicFiles].wavPath = fullPath;
          totalMusicFiles++;
        }
      }
    }
    file.close();
    file = root.openNextFile();
  }
  root.close();
}

//===================================================== Setup
void setup() {
  Serial.begin(115200);
  WiFi.mode(WIFI_STA);
  delay(1000);

  // Initialize ESP-NOW
  if (esp_now_init() != ESP_OK) {
    Serial.println("Error initializing ESP-NOW");
    return;
  }

  // Register Callbacks using explicit casting to prevent v3.0 errors
  //esp_now_register_send_cb((esp_now_send_cb_t)OnDataSent);
  esp_now_register_recv_cb((esp_now_recv_cb_t)OnDataRecv);

  // Register Peers that YB receives information from.
  esp_now_peer_info_t peerInfo = {};
  memcpy(peerInfo.peer_addr, receiverAddress1, 6);
  memcpy(peerInfo.peer_addr, receiverAddress2, 6);
  peerInfo.channel = 0;
  peerInfo.encrypt = false;
  // Add peer to network
  if (esp_now_add_peer(&peerInfo) != ESP_OK) {
    Serial.println("Failed to add peer");
    return;
  }

  // Setup status LED for YB board --------------- start Audio
  pinMode(STATUS_LED, OUTPUT);
  digitalWrite(STATUS_LED, LOW);
  Serial.println("YB-ESP32-S3-AMP Rev2 Audio Player");

  // Initialize SD card using SPI
  SPI.begin(SD_SCK, SD_MISO, SD_MOSI);
  if (!SD.begin(SD_CS)) {
    Serial.println("SD Card initialization failed!");
    return;
  }
  Serial.println("SD Card initialized");

  // Initialize audio and sound effects
  audioSetup();
  // Scan SD card for audio files
  scanAudioFiles();

  if (totalAudioFiles == 0) {
    Serial.println("No audio files found on SD card!");
    return;
  }

  // Display all background music and sound effects.
  listAllFiles();

  // Blink LED to indicate ready - why are we checking 0,1,2?
  for (int i = 0; i < 3; i++) {
    digitalWrite(STATUS_LED, HIGH);
    delay(200);
    digitalWrite(STATUS_LED, LOW);
    //delay(200);
  }

  //Setup Background Music
  bgFile = SD.open("/m_silence.wav"); //The first track that'll be heard
  currentPath = "/m_silence.wav"; //The track it'll loop as upon loading.
  if(!bgFile)
  {
    Serial.println("Background music missing.");
    return;   
  }
  //Starts Background music stream
  bgStream.begin();
  //Starts background music volume, so it can be changed throughout
  bgVolume.setAudioInfo(info);
  bgVolume.begin();
  bgVolume.setVolume(bgVol);
  bgIndex = mixer.add(bgVolume, 100); //Weight of mixer 0 = off, 100 = on.
  Serial.println("Background Music Start!");
}

//===================================================== Loop

void loop() {
  //Keeps audio running
  copier.copy();

  //check for messages from game esp / tile esp
  if (dataReceived == true) {
    activateData();  // load gameSuccess data from game esp, sets dataReceived to false again
  }

  //Detect End of Background Music
  if(!bgFile.available() || bgFile.available() < 2048)
  {
    //Serial.println("BG music ended");
    playBg(-1,1,0); //Not passing in new audio (-1), confirmed its looping instead (1)
  }

  //Deactivating any sound effects that have ended
  for(int i = 0; i < totalSfxVoices; i++)
  {
    SfxVoice &v = sfx_voices[i];
    //Has an end time which is slightly shorter than the file's actual duration.
    //This is done to avoid the click that appears when closing a file.
    if(v.active && millis() - v.startTime > v.endTime)
    {
      v.active = false;
      mixer.setWeight(v.mixerIndex,0);
    }
  }

  // Read serial input
  while (Serial.available() > 0) {
    char c = Serial.read();
    if (c == '\n' || c == '\r') {
      if (serialBuffer.length() > 0) {
        processSerialCommand(serialBuffer);
        Serial.println("Process Inputted Succesfully");
        serialBuffer = "";
      }
    } else {
      serialBuffer += c;
    }
  }
} 

//===================================================== Loop functions

// --- Activating Data upon receiving Data.

void activateData() {  //actively load incoming results and sensor values, sets dataReceived = false
  //Debugging
  /*
    Serial.printf("Sfx Data received: ");
    Serial.println(recvSfx);
    Serial.printf("Bg Data received: ");
    Serial.println(recvBg);
    Serial.printf("Bg Vol Data received: ");
    Serial.println(recvBgVol);
    Serial.printf("Sfx Vol Data received: ");
    Serial.println(recvSfxVol);
  */
  //0 Means Nothing, Don't use that, it's a null state meaning empty
  // ---- Background Volume Changing
  if(recvBgVol >= 1 && recvBgVol <= 101)
  {
    Serial.print("Background Volume Change:  ");
    Serial.println(recvBgVol);
    changeBgVol(recvBgVol-1);
  }
  // ---- Sound Effect Volume Change
  if(recvSfxVol >= 1 && recvSfxVol <= 101)
  {
    Serial.print("Sound Effect Volume Change.");
    Serial.println(recvSfxVol);
    changeSfxVol(recvSfxVol-1);
  }
  // ----- Play sound effects
  if(recvSfx >= 1 && recvSfx < 16)
  {
    Serial.print("Sound Effect Playing.");
    Serial.println(recvSfx);
    playSfx(recvSfx);
  }
  // ---- Play Background Music
  if(recvBg >= 1 && recvBg < totalMusicFiles)
  {
    Serial.print("Background Change.");
    Serial.println(recvBg);
    //recvBgLooping is if the new background music shall loop (0) or not (1)
    playBg(recvBg,0,recvBgLooping);
  }
  Serial.println("");
  dataReceived = false;
}  

void listAllFiles() 
{
  Serial.println("\n===== MUSIC FILES ON SD CARD =====");
  Serial.printf("Total files: %d\n\n", totalMusicFiles);
  //Lists Music Files on SD Card
  for (int i = 0; i < totalMusicFiles; i++) { //Change totalMusicFiles to totalAudioFiles to display all files on the SD card
    Serial.printf("[%d] %s", i, musicFiles[i].name.c_str()); //Change musicFiles to audioFiles to display all files on the SD card
    Serial.print(" (WAV)");
    Serial.println();
  }
  Serial.println("\n===== SOUND EFFECTS ON FLASH MEMORY =====");
  Serial.printf("Total files: %d\n\n", sfxAmount);
  //Lists Sound Effects within Flash Memory
  for (int j = 0; j < sfxAmount; j++)
  {
    Serial.printf("[%d] %s",j,sfxList[j].name);
    Serial.println();
  }
  Serial.println("\n===================================");
  Serial.println("Type a number to play a SFX and a name to play Music:");
  Serial.println("Type 'list' to show all files again\n");
}

void processSerialCommand(String command) {
  command.trim();
  if (command.length() == 0) return;

  // Check if it's the list command
  if (command.equalsIgnoreCase("list")) {
    listAllFiles();
    return;
  }

  // Check if it's a number
  bool isNumber = true;
  for (unsigned int i = 0; i < command.length(); i++) {
    if (!isDigit(command.charAt(i))) {
      isNumber = false;
      break;
    }
  }

  int fileIndex = -1;  //variable to hold the data entered via serial monitor. cant use 0 as this is assigned

  // ----- This is a menu system for the Serial Monitor -----
  // "main" is where you can call sound effects (Number) or the background track (Name).
  // "bg" is where you adjust the background's volume, type "back" to return to main.
  // "sfx" is where you adjust the sound effect's volume, type "back" to return to main.
  // This can all be removed / simplified once the final ESP is ready as this has been for testing purposes
  if(menu == "main")
  {
    if(command.equalsIgnoreCase("bg"))
    {
      menu = "bg";
      Serial.println("In BG Vol");
      return;
    }
    if(command.equalsIgnoreCase("sfx"))
    {
      menu = "sfx";
      Serial.println("In SFX Vol");
      return;
    }
     //Typing in a number will get the sound effect, typing in the name gives the background music
    if (isNumber) 
    {
      // It's a number, see if that sound effect exists
      fileIndex = command.toInt();  // set the fileIndex to the int number identified?
      if(fileIndex > sfxAmount) //
        {
          Serial.println("No SFX fit that number");
          return; 
        }
      currentSfx = -1;
      //Checks if there's a channel free in the mixer for the sound effect to play.
      for(int i = 0; i < totalSfxVoices; i++)
      {
        SfxVoice &v = sfx_voices[i];
        if(!v.active)
        {
          currentSfx = i;
          break;
        }
      }
      if(currentSfx == -1)
      {
        Serial.println("No space for new sfx");
        return;
      }
      playSfx(command.toInt());
    } 
    else 
    {
      // It's a name, search for it
      for (int i = 0; i < totalAudioFiles; i++) {
        if (audioFiles[i].name.equalsIgnoreCase(command)) {
          fileIndex = i;
          break;
        }
      }

      if (fileIndex < 0) {
        Serial.printf("File '%s' not found. Type 'list' to see all files.\n", command.c_str());
        return;
      }
      //Finds the background music
        for (int i = 0; i < totalMusicFiles; i++)
        {
          if(musicFiles[i].name.equalsIgnoreCase(command))
          {
            fileIndex = i;
            break;
          }
        }
        if(fileIndex < 0)
        {
          Serial.printf("File '%s' not found. Type 'list' to see all files. \n", command.c_str());
          return;
        }
        playBg(fileIndex,0,0);
    }
  }
  else if (menu == "bg")
  {
    if(command.equalsIgnoreCase("back"))
    {
      menu = "main";
      Serial.println("In Main Menu");
      return;
    }
    if(isNumber)
    {
      changeBgVol(command.toInt());
    }
  }
  else if (menu == "sfx")
  {
    if(command.equalsIgnoreCase("back"))
    {
      menu = "main";
      Serial.println("In Main Menu");
      return;
    }
    if(isNumber)
    {
      changeSfxVol(command.toInt());
    }
  }
  // ----- End of menu system for the Serial Monitor -----

 
}//end of process Input



void playBg(int bg_playing, int looping, int cutting)
{
  String pathToPlay;
  // Looping music
  if(looping)
  {
    pathToPlay = currentPath;
  }
  // Changing Music
  else
  {
    //if the music's position doesn't exist
    if(bg_playing < 0 || bg_playing >= totalAudioFiles) {return;}
    
    //changing volumes
    bgVolume.setVolume(bgVol);
    //Adjusting volume if a quieter track is playing
    if(bg_playing == 3) //Whatever Value Simon Says Music is in the Music list
    {
      bgVolume.setVolume(bgVol * 1.8);
    }
    //If the file being played exists and has as WAV file
    if(musicFiles[bg_playing].hasWav)
    {
      //Changes the path to new music's location
      pathToPlay = musicFiles[bg_playing].wavPath;
      currentPath = pathToPlay;
    }
    else
    {
      Serial.println("Error: No Valid Audio File Found.");
      return; 
    }
    //What the music will loop round to afterwards.
    if(bg_playing == 1)
    {
      currentPath = "/m_main_a.wav";
    }
    if(cutting == 1)
    {
      currentPath = "/m_silence.wav"; 
      //Have to use a wav audio of silence for silence as the mixer
      //would break if both the sound effect and background 
      //locations in the mixer were both empty
    }
  }
  //Changing the track in the mixer.
  mixer.setWeight(bgIndex,0); //Have to mute to avoid clicks or distortion whilst changing
  //Opens file
  File newFile = SD.open(pathToPlay.c_str());
  if(!newFile){Serial.println("Missing Bg Music");}

  //Close file to change music file to avoid any issues
  bgFile.close();
  bgFile = newFile;
  //Have to begin and end stream / decoder to avoid distortion
  bgDecoder.begin();
  bgStream.end();
  bgStream = EncodedAudioStream(&bgFile, &bgDecoder);
  bgStream.begin();
  //Turn the mixer on again
  mixer.setWeight(bgIndex,100);
}

void playSfx(int sfx_playing)
{
  //Resets Variables for reasignment
  SfxVoice &v = sfx_voices[currentSfx];
  v.active = true;
  //Turn off mixer to avoid distortion or clicks whilst changing.
  mixer.setWeight(v.mixerIndex,0);

  //Changing Volume
  v.volume->setVolume(sfxVolValue);
  //Manual adjustment of volume for louder sound effects
  if(sfx_playing >= 4 && sfx_playing <= 6) // Selecting game nosises quite loud so this dampens it
  {
    v.volume->setVolume(sfxVolValue*0.46);
  }  

  // Empty variables to hold the new sound effect info
  const uint8_t* currentData = nullptr;
  size_t currentLen = 0;

  //get sound effect's information
  currentData = sfxList[sfx_playing].data;
  currentLen = sfxList[sfx_playing].len;
  v.endTime = sfxList[sfx_playing].ms;
  // ---- Deleting to avoid memory leaking / stacking up of the pointers
  //They have to be pointers as the version of Audio Tool that's being used
  //Doesn't have the functions to allow different methods of changing sound effects.
  delete v.memory;
  v.memory = new MemoryStream((uint8_t*)currentData,currentLen,true,FLASH_RAM);

  v.stream->begin();
  v.decoder -> begin();

  //Restarting sound effect timer and turning on mixer
  v.startTime = millis();
  mixer.setWeight(v.mixerIndex,100);
  isSfxPlaying = true;
}

void changeSfxVol(int p_Vol)
{
  //Change Sound Effect Volume
  if(p_Vol < 0 || p_Vol > 100)
  {
    Serial.println("Invalid Volume Number, has to be between 0 and 100.");
    return;
  }
  sfxVolValue = float(p_Vol) / 100; //Divide number by 100 for decimal.
  //Change volume for all sound effects in list
  for (int i = 0; i < totalSfxVoices; i++)
  {
    sfx_voices[i].volume->setVolume(sfxVolValue);
  }
}

void changeBgVol(int p_Vol)
{
  //Change Background Volume
  if(p_Vol < 0 || p_Vol > 100)
  {
    Serial.println("Invalid Volume Number, has to be between 0 and 100.");
    return;
  }
  bgVol = float(p_Vol) / 100; //Divide number by 100 for decimal.
  bgVolume.setVolume(bgVol);
}

