// Watford DDFS Board Drive Controller Chip DLL
// (C) September 2001 - Richard Gellman

#include <windows.h>
#include "Watford.h"

char *WatfordName="Watford DDFS Extension board for BBC Model B";

// The dll must assume the 1770 system accepts a Master 128 type control reg thus:
// Bit 0 Drive Select 0
// Bit 1 Drive Select 1
// Bit 2 Reset controller
// Bit 3 Drive Select 2
// Bit 4 Side Select
// Bit 5 Density Select = 0 - double ; 1 - single
// Bits 6 & 7 not used

EXPORT unsigned char SetDriveControl(unsigned char value) {
	unsigned char temp;
	// from native to master 128
	if (value & 4) temp=2; else temp=1; // drive select
	temp|=(value & 2)<<3; // side select
	temp|=(value & 1)<<5; // density select
	return(temp);
}

EXPORT unsigned char GetDriveControl(unsigned char value) {
	unsigned char temp;
	// from master 128 to native
	if (value & 1) temp=0; // Drive select 0
	if (value & 2) temp=4; // Drive select 1
	temp|=(value & 16)>>3; // Side select
	temp|=(value & 32)>>5;
	return(temp);
}

EXPORT void GetBoardProperties(struct DriveControlBlock *FDBoard) {
	FDBoard->FDCAddress=0xfe84;
	FDBoard->DCAddress=0xfe80;
	FDBoard->BoardName=WatfordName;
	FDBoard->TR00_ActiveHigh=TRUE;
}