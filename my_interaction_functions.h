#pragma once

extern "C" {
#include <interface.h>
}

int getBitValue(uInt8 value, uInt8 bit_n);
void setBitValue(uInt8* variable, int n_bit, int new_value_bit);
void switch1ON();
void switch1OFF();
void switch2ON();
void switch2OFF();


// X axis related functions
void moveXLeft();
void moveXRight();
void stopX();
int  getXPos();
void gotoX(int x);

// Y axis related functions
void moveYInside();
void moveYOutside();
void stopY();
int  getYPos();
void gotoY(int y);

// Z axis related functions
void moveZUp();
void moveZDown();
void stopZ();
int  getZPos();
void gotoZ(int z);

//
void gotoZUp();
void gotoZDown();
void putPartInCell();
void takePartInCell();

bool check_drug_cage();

void emergency_stop();

void restaure_bits(uInt8 p22);

// Put here the other function headers!!!
