/*
WD1770 FDC Disc Support for BeebEm

Written by Richard Gellman - Feb 2001

You may not distribute this entire file separate from the whole BeebEm distribution.

You may use sections or all of this code for your own uses, provided that:

1) It is not a separate file in itself.
2) This copyright message is included
3) Acknowledgement is made to the author, and any aubsequent authors of additional code.

The code may be modified as required, and freely distributed as such authors see fit,
provided that:

1) Acknowledgement is made to the author, and any subsequent authors of additional code
2) Indication is made of modification.

Absolutely no warranties/guarantees are made by using this code. You use and/or run this code
at your own risk. The code is classed by the author as "unlikely to cause problems as far as
can be determined under normal use".

-- end of legal crap - Richard Gellman :) */

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include "disc1770.h"
#include "6502core.h"
#include "main.h"

// Control/Status Register, Track, Sector, and Data Registers
unsigned char Status=0;
unsigned char Data;
unsigned char Track;
unsigned char Sector;
unsigned char HeadDir=1; // Head Movement direction - 1 to step in
unsigned char FDCommand=0;
unsigned char UpdateTrack=0;
unsigned char MultiSect=0;
int ByteCount=0;
long DataPos;
char errstr[250];


FILE *Disc0; // File handlers for the disc drives 0 and 1
FILE *Disc1;
FILE *CurrentDisc; // Current Disc Handle

unsigned char Disc0Open=0;
unsigned char Disc1Open=0; // Disc open status markers

unsigned char ExtControl; // FDC External Control Register
unsigned char CurrentDrive=0; // FDC Control drive setting
long HeadPos[1]; // Position of Head on each drive for swapping
unsigned char CurrentHead[2]; // Current Head on any drive
int DiscStep[2]; // Single/Double sided disc step
int DiscStrt[2]; // Single/Double sided disc start

void SetStatus(unsigned char bit) {
	Status|=1<<bit;
}

void ResetStatus(unsigned char bit) {
	Status&=~(1<<bit);
}

unsigned char Read1770Register(unsigned char Register) {
	// Read 1770 Register. Note - NOT the FDC Control register @ &FE24
	if (Register==0) {
//		sprintf(errstr,"Read attempt of FDC Register 0 of %02x",Status);
//		MessageBox(GETHWND,errstr,"BBC Emulator",MB_OKCANCEL|MB_ICONERROR);
		NMIStatus &= ~(1<<nmi_floppy);
		return(Status);
	}
	if (Register==1) return(Track); 
	if (Register==2) return(Sector);
	if (Register==3) {
//		sprintf(errstr,"Read attempt of FDC Data Register of %02x",Data);
//		MessageBox(GETHWND,errstr,"BBC Emulator",MB_OKCANCEL|MB_ICONERROR);
		if (FDCommand>5) 
		{
			ResetStatus(1); NMIStatus &= ~(1<<nmi_floppy); 
			//fputc(Data,Disc1);
		}
		return(Data);
	}
}

void Write1770Register(unsigned char Register, unsigned char Value) {
	unsigned char ComBits,HComBits;
	// Write 1770 Register - NOT the FDC Control register @ &FE24
	if (Register==0 && ((Disc0Open==1 && CurrentDrive==0) || (Disc1Open==1 && CurrentDrive==1))) {
//		sprintf(errstr,"Write attempt to FDC Register 0 of %02x",Value);
//		MessageBox(GETHWND,errstr,"BBC Emulator",MB_OKCANCEL|MB_ICONERROR);

		// Control Register - can only write if current drive is open
		ComBits=Value & 0xf0;
		HComBits=Value & 0xe0;
		if (HComBits<0x80) {
			// Type 1 Command
			SetStatus(0);
			ResetStatus(3);
			ResetStatus(4);
			if (HComBits==0x40) { FDCommand=4; HeadDir=1; UpdateTrack=(Value & 16)>>4; }
			if (HComBits==0x60) { FDCommand=5; HeadDir=0; UpdateTrack=(Value & 16)>>4; }
			if (HComBits==0x20) { FDCommand=3; UpdateTrack=(Value & 16)>>4; }
			if (ComBits==0x10) { FDCommand=2; }
			if (ComBits==0) { FDCommand=1; }
			if (FDCommand<6) { SetStatus(5); }
			SetStatus(7);
		}
		if (HComBits==0x80) { 
			FDCommand=6; MultiSect=(Value & 16)>>4; 
			SetStatus(0);
			ResetStatus(1);
			ResetStatus(5); ResetStatus(6);
			ByteCount=257; DataPos=ftell(CurrentDisc); HeadPos[CurrentDrive]=DataPos;
			fseek(CurrentDisc,Sector*256,SEEK_CUR);
		}
		
	}
	if (Register==1) {
		Track=Value; }
	if (Register==2) {
		Sector=Value; }
	if (Register==3) {
		Data=Value;
		if (FDCommand>5) ResetStatus(1);
	}
	
}

void Poll1770(void) {
	// This procedure is called from the 6502core to enable the chip to do stuff in the background
  if (Status & 1) {
	if (FDCommand<6) {
		// Seek type commands
		if (FDCommand==1) { fseek(CurrentDisc,DiscStrt[CurrentDrive],SEEK_SET); ResetStatus(0); Track=0; } // Restore
		if (FDCommand==2) { fseek(CurrentDisc,DiscStrt[CurrentDrive]+(DiscStep[CurrentDrive]*Data),SEEK_SET); Track=Data; ResetStatus(0); } // Seek
		if (FDCommand==4) { HeadDir=1; fseek(CurrentDisc,DiscStep[CurrentDrive],SEEK_CUR); Track++; ResetStatus(0); } // Step In
		if (FDCommand==5) { HeadDir=0; fseek(CurrentDisc,-DiscStep[CurrentDrive],SEEK_CUR); Track--; ResetStatus(0); } // Step Out
		if (FDCommand==3) { fseek(CurrentDisc,(HeadDir)?DiscStep[CurrentDrive]:-DiscStep[CurrentDrive],SEEK_CUR); Track=(HeadDir)?Track+1:Track-1; ResetStatus(0); } // Step
		NMIStatus|=1<<nmi_floppy; FDCommand=0;
	}
	if (FDCommand==6) {
		if ((Status & 2)>>1==0) { 
			if (!feof(CurrentDisc)) { Data=fgetc(CurrentDisc); SetStatus(1); NMIStatus|=1<<nmi_floppy; /* fputc(Data,Disc1); */  } // DRQ
			ByteCount--;
			if (ByteCount==0 && MultiSect==0) { ResetStatus(0); NMIStatus|=1<<nmi_floppy; fseek(CurrentDisc,HeadPos[CurrentDrive],SEEK_SET); FDCommand=0; }
			if (ByteCount==0 && MultiSect) { ByteCount=257; Sector++; 
				if (Sector==9) MultiSect=0; Sector=0; 
			}
		}
	}
  }
}		
void Load1770DiscImage(char *DscFileName,int DscDrive,unsigned char DscType) {
	if (DscDrive==0) {
		if (Disc0Open==1) fclose(Disc0);
		Disc0=fopen(DscFileName,"rb");
		if (CurrentDrive==0) CurrentDisc=Disc0;
		Disc0Open=1;
	}
	if (DscDrive==1) {
		if (Disc1Open==1) fclose(Disc1);
		Disc1=fopen(DscFileName,"rb");
		if (CurrentDrive==1) CurrentDisc=Disc1;
		Disc1Open=1;
	}
//	if (DscType=0) CurrentHead[DscDrive]=0;
	DiscStep[DscDrive]=(DscType==1)?5120:2560;
	DiscStrt[DscDrive]=(CurrentHead[DscDrive]==1)?2560:0;
}

void WriteFDCControlReg(unsigned char Value) {
	// This function writes the control register @ &FE24
	ExtControl=Value;
	if ((ExtControl & 1)==1) { CurrentDisc=Disc0; CurrentDrive=0; }
	if ((ExtControl & 2)==2) { CurrentDisc=Disc1; CurrentDrive=1; }
	if ((ExtControl & 16)==16 && CurrentHead[CurrentDrive]==0) { CurrentHead[CurrentDrive]=1; fseek(CurrentDisc,2560,SEEK_CUR); DiscStrt[CurrentDrive]=2560; }
	if ((ExtControl & 16)!=16 && CurrentHead[CurrentDrive]==1) { CurrentHead[CurrentDrive]=0; fseek(CurrentDisc,-2560,SEEK_CUR); DiscStrt[CurrentDrive]=0; }
}

unsigned char ReadFDCControlReg(void) {
	return(ExtControl);
}

void Reset1770(void) {
	CurrentDisc=Disc0;
	CurrentDrive=0;
	CurrentHead[0]=CurrentHead[1]=0;
}

void Kill1770(void) {
	fclose(Disc0);
	fclose(Disc1);
}