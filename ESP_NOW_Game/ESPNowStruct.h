#pragma once

// Structure template to hold data received, must match sender structure
typedef struct struct_message_all {
  int id; // unique sender ID: LO = 1, LI = 2, RI = 3, RO = 4, button ESP = 5, game ESP = 6
  int t;  // airtime
  int b;  // buttonInput
  int jc; // jumpCount 
  int js; // jumpState
  int sd; // stepDelay
  int dA; // toe sensor (A) of the first tile
  int dB;
  int eA;
  int eB;
  int fA;
  int fB;
  int gA;
  int gB;
} struct_message_all;