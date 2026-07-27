#pragma once
#include "AudioTools.h"

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
  int fA;
  int fB;
  int gA;
  int gB;
} struct_message_all;

//Audio Files for SD card loading

typedef struct AudioFile {
  String name;     // Base name without extension
  String wavPath;  // Full path to WAV file (if exists)
  bool hasWav;
} AudioFile;

// ---- Sound Effects  ----

//Structure for how the sound effect runs in the mixer.
typedef struct SfxVoice 
{
  MemoryStream* memory;
  WAVDecoder* decoder;
  EncodedAudioStream* stream;
  VolumeStream* volume; 
  int mixerIndex;
  bool active;
  uint32_t startTime;
  uint32_t endTime;
} SfxVoice;

//For creating sound effects stored in PSRAM 
typedef struct SoundEffect
{
  uint8_t *data;
  size_t length;
} SoundEffect;