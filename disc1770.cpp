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
#include "beebemrc.h"

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
unsigned char CDiscOpen=0; // Current Disc Open

unsigned char ExtControl; // FDC External Control Register
unsigned char CurrentDrive=0; // FDC Control drive setting
long HeadPos[1]; // Position of Head on each drive for swapping
unsigned char CurrentHead[2]; // Current Head on any drive
int DiscStep[2]; // Single/Double sided disc step
int DiscStrt[2]; // Single/Double sided disc start
unsigned char MaxSects[2]; // Maximum sectors per track
unsigned int DefStart[2]; // Starting point for head 1
unsigned int TrkLen[2]; // Track Length in bytes
unsigned char DWriteable[2]={0,0}; // Write Protect

static void SetStatus(unsigned char bit) {
	Status|=1<<bit;
}

static void ResetStatus(unsigned char bit) {
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
	if (Register==0) {
//		sprintf(errstr,"Write attempt to FDC Register 0 of %02x",Value);
//		MessageBox(GETHWND,errstr,"BBC Emulator",MB_OKCANCEL|MB_ICONERROR);

		// Control Register - can only write if current drive is open
		// Changed, now command returns errors if no disc inserted
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
			if (FDCommand<6) { SetStatus(5); SetStatus(0); }
			SetStatus(7);
		}
		if (HComBits==0x80) {
			SetStatus(0);
			FDCommand=8; MultiSect=(Value & 16)>>4; 
		}
		if (HComBits==0xa0) { 
			SetStatus(0);
			FDCommand=9; MultiSect=(Value & 16)>>4; 
		}
	}
	if (Register==1) {
		Track=Value; }
	if (Register==2) {
		Sector=Value; }
	if (Register==3) {
		Data=Value;
		if (FDCommand>5) { ResetStatus(1); NMIStatus &= ~(1<<nmi_floppy); }
	}
}

void Poll1770(void) {
	// This procedure is called from the 6502core to enable the chip to do stuff in the background
  if (Status & 1) {
	if (FDCommand<6 && CDiscOpen) {
		// Seek type commands
		ResetStatus(4); ResetStatus(5);
		if (FDCommand==1) { fseek(CurrentDisc,DiscStrt[CurrentDrive],SEEK_SET); ResetStatus(0); Track=0; } // Restore
		if (FDCommand==2) { fseek(CurrentDisc,DiscStrt[CurrentDrive]+(DiscStep[CurrentDrive]*Data),SEEK_SET); Track=Data; ResetStatus(0); } // Seek
		if (FDCommand==4) { HeadDir=1; fseek(CurrentDisc,DiscStep[CurrentDrive],SEEK_CUR); Track++; ResetStatus(0); } // Step In
		if (FDCommand==5) { HeadDir=0; fseek(CurrentDisc,-DiscStep[CurrentDrive],SEEK_CUR); Track--; ResetStatus(0); } // Step Out
		if (FDCommand==3) { fseek(CurrentDisc,(HeadDir)?DiscStep[CurrentDrive]:-DiscStep[CurrentDrive],SEEK_CUR); Track=(HeadDir)?Track+1:Track-1; ResetStatus(0); } // Step
		NMIStatus|=1<<nmi_floppy; FDCommand=0;
	}
	if (FDCommand<6 && !CDiscOpen) {
		// Disc not ready, return seek error;
		ResetStatus(0);
		SetStatus(4);
		NMIStatus|=1<<nmi_floppy; FDCommand=0;
	}
	if (FDCommand==6) { // Read
		if ((Status & 2)>>1==0) { 
			ResetStatus(4); ResetStatus(5);
			if (!feof(CurrentDisc)) { Data=fgetc(CurrentDisc); SetStatus(1); NMIStatus|=1<<nmi_floppy; /* fputc(Data,Disc1); */  } // DRQ
			ByteCount--;
			if (ByteCount==0 && MultiSect==0) { ResetStatus(0); NMIStatus|=1<<nmi_floppy; fseek(CurrentDisc,HeadPos[CurrentDrive],SEEK_SET); FDCommand=0; }
			if (ByteCount==0 && MultiSect) { ByteCount=257; Sector++; 
				if (Sector==MaxSects[CurrentDrive]) { MultiSect=0; /* Sector=0; */ }
			}
		}
	}
	if (FDCommand==7 && DWriteable[CurrentDrive]==1) { // Write
		if ((Status & 2)==0) { 
			ResetStatus(4); ResetStatus(5);
			if (ByteCount<257) { fputc(Data,CurrentDisc); SetStatus(1); NMIStatus|=1<<nmi_floppy; } /* fputc(Data,Disc1); */   // DRQ
			// Ignore first byte.
			ByteCount--;
			if (ByteCount==0 && MultiSect==0) { ResetStatus(0); NMIStatus|=1<<nmi_floppy; fseek(CurrentDisc,HeadPos[CurrentDrive],SEEK_SET); FDCommand=0; }
			if (ByteCount==0 && MultiSect) { ByteCount=257; Sector++; 
				if (Sector==MaxSects[CurrentDrive]) { MultiSect=0; /* Sector=0; */ }
			}
		}
	}
	if (FDCommand==7 && DWriteable[CurrentDrive]==0) {
		ResetStatus(0);
		SetStatus(6);
		NMIStatus|=1<<nmi_floppy; FDCommand=0;
	}
	if (FDCommand>=8 && CDiscOpen==1) { // Read/Write Prepare
		SetStatus(0);
		ResetStatus(1);
		ResetStatus(5); ResetStatus(6);
		ByteCount=257; DataPos=ftell(CurrentDisc); HeadPos[CurrentDrive]=DataPos;
		fseek(CurrentDisc,Sector*256,SEEK_CUR);
	}
	if (FDCommand>=8 && CDiscOpen==0) {
		ResetStatus(0);
		SetStatus(4);
		NMIStatus|=1<<nmi_floppy; FDCommand=0;
	}
	if (FDCommand==8 && CDiscOpen==1) FDCommand=6;
	if (FDCommand==9 && CDiscOpen==1) { SetStatus(1); NMIStatus|=1<<nmi_floppy; FDCommand=7; }
	if (FDCommand==10) {
		ResetStatus(0);
		ResetStatus(4);
		ResetStatus(5);
		ResetStatus(6);
		NMIStatus|=1<<nmi_floppy; FDCommand=0;
	}
  }
}		

void Load1770DiscImage(char *DscFileName,int DscDrive,unsigned char DscType,HMENU dmenu) {
	long int TotalSectors;
	long HeadStore;
	if (DscDrive==0) {
		if (Disc0Open==1) fclose(Disc0);
		Disc0=fopen(DscFileName,"rb+");
		if (Disc0!=NULL) EnableMenuItem(dmenu, IDM_WPDISC0, MF_ENABLED );
		else {
			Disc0=fopen(DscFileName,"rb");
			EnableMenuItem(dmenu, IDM_WPDISC0, MF_GRAYED );
		}
		DWriteable[0]=0;
		CheckMenuItem(dmenu,IDM_WPDISC0,MF_CHECKED);
		if (CurrentDrive==0) CurrentDisc=Disc0;
		Disc0Open=1;
	}
	if (DscDrive==1) {
		if (Disc1Open==1) fclose(Disc1);
		Disc1=fopen(DscFileName,"rb+");
		if (Disc1!=NULL) EnableMenuItem(dmenu, IDM_WPDISC1, MF_ENABLED );
		else {
			Disc1=fopen(DscFileName,"rb");
			EnableMenuItem(dmenu, IDM_WPDISC1, MF_GRAYED );
		}
		DWriteable[1]=0;
		CheckMenuItem(dmenu,IDM_WPDISC1,MF_CHECKED);
		if (CurrentDrive==1) CurrentDisc=Disc1;
		Disc1Open=1;
	}
//	if (DscType=0) CurrentHead[DscDrive]=0;
//  Feb 14th 2001 - Valentines Day - Bah Humbug - ADFS Support added here
    if (DscType==0) { 
		DiscStep[DscDrive]=2560; 
		DiscStrt[DscDrive]=0; 
		DefStart[DscDrive]=0; 
		TrkLen[DscDrive]=2560; 
	}
    if (DscType==1) { 
		DiscStep[DscDrive]=5120; 
		DiscStrt[DscDrive]=CurrentHead[DscDrive]*2560; 
		DefStart[DscDrive]=2560; 
		TrkLen[DscDrive]=2560; 
	}
    if (DscType==2) { 
		DiscStep[DscDrive]=8192; 
		DiscStrt[DscDrive]=CurrentHead[DscDrive]*4096; 
		DefStart[DscDrive]=4096; 
		TrkLen[DscDrive]=4096; 
		// This is a quick check to see what type of disc the ADFS disc is.
		// Bytes 0xfc - 0xfe is the total number of sectors.
		// In an ADFS L disc, this is 0xa00 (160 Tracks)
		// for and ADFS M disc, this is 0x500 (80 Tracks)
		// and for the dreaded ADFS S disc, this is 0x280
		HeadStore=ftell(CurrentDisc);
		fseek(CurrentDisc,0xfd,SEEK_SET);
		TotalSectors=((fgetc(CurrentDisc)<<8)+fgetc(CurrentDisc));
		fseek(CurrentDisc,HeadStore,SEEK_SET);
		if (TotalSectors<0xa00) {
			DiscStep[DscDrive]=4096;
			DiscStrt[DscDrive]=0;
			DefStart[DscDrive]=0;
			TrkLen[DscDrive]=4096;
		}
	}
	MaxSects[DscDrive]=(DscType<2)?9:15;
	CDiscOpen=0;
	if (CurrentDrive==0 && Disc0Open==1) CDiscOpen=1;
	if (CurrentDrive==1 && Disc1Open==1) CDiscOpen=1;
}

void WriteFDCControlReg(unsigned char Value) {
	// This function writes the control register @ &FE24
	ExtControl=Value;
	if ((ExtControl & 1)==1) { CurrentDisc=Disc0; CurrentDrive=0; }
	if ((ExtControl & 2)==2) { CurrentDisc=Disc1; CurrentDrive=1; }
	if ((ExtControl & 16)==16 && CurrentHead[CurrentDrive]==0) { 
		CurrentHead[CurrentDrive]=1; 
		fseek(CurrentDisc,TrkLen[CurrentDrive],SEEK_CUR); 
		DiscStrt[CurrentDrive]=DefStart[CurrentDrive]; 
	}
	if ((ExtControl & 16)!=16 && CurrentHead[CurrentDrive]==1) { 
		CurrentHead[CurrentDrive]=0; 
 		fseek(CurrentDisc,0-TrkLen[CurrentDrive],SEEK_CUR); 
		DiscStrt[CurrentDrive]=0; 
	}
	CDiscOpen=0;
	if (CurrentDrive==0 && Disc0Open==1) CDiscOpen=1;
	if (CurrentDrive==1 && Disc1Open==1) CDiscOpen=1;
}

unsigned char ReadFDCControlReg(void) {
	return(ExtControl);
}

void Reset1770(void) {
	if (Disc0Open) fclose(Disc0);
	if (Disc1Open) fclose(Disc1);
	CurrentDisc=Disc0;
	CurrentDrive=0;
	CurrentHead[0]=CurrentHead[1]=0;
}

void Kill1770(void) {
	if (Disc0Open) fclose(Disc0);
	if (Disc1Open) fclose(Disc1);
}