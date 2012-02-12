/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2001  Richard Gellman
Copyright (C) 2005  Greg Cook

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public 
License along with this program; if not, write to the Free 
Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
Boston, MA  02110-1301, USA.
****************************************************************/

/*
WD1770 FDC Disc Support for BeebEm

Written by Richard Gellman - Feb 2001
*/

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include "disc1770.h"
#include "6502core.h"
#include "main.h"
#include "beebemrc.h"
#include "uefstate.h"
#include "z80mem.h"
#include "z80.h"
#include "beebsound.h"

extern FILE *tlog;
extern int trace;

// Control/Status Register, Track, Sector, and Data Registers
unsigned char FormatBuffer[2048];
unsigned char *FormatPtr;
unsigned char FormatState=0;
unsigned int FormatCount=0;
unsigned int FormatSize=0;
unsigned char Status=0;
unsigned char Data=0;
unsigned char Track=0,ATrack=0;
unsigned char Sector;
unsigned char HeadDir=1; // Head Movement direction - 1 to step in
unsigned char FDCommand=0,NFDCommand=0; // NFD is used as "Next command" during spin up/settling periods
int LoadingCycles=0; // Spin up/settle counter in CPU Cycles
int SpinDown[2]={0,0}; // Spin down delay per drive
// The following are control bits
unsigned char UpdateTrack=0;
unsigned char MultiSect=0;
const unsigned char StepRate[4]={6,12,20,30};
unsigned char CStepRate=StepRate[0];
unsigned char ESpinUp=0;
unsigned char EVerify=0;
bool LightsOn[2]={FALSE,FALSE};
bool HeadLoaded[2]={FALSE,FALSE};
// End of control bits
int ByteCount=0;
long DataPos;
char errstr[250];
unsigned char Disc1770Enabled=1;

/* File names of loaded disc images */
static char DscFileNames[2][256];

FILE *Disc0; // File handlers for the disc drives 0 and 1
FILE *Disc1;
FILE *CurrentDisc; // Current Disc Handle

FILE *fdclog;

unsigned char Disc0Open=0;
unsigned char Disc1Open=0; // Disc open status markers
unsigned char *CDiscOpen=&Disc0Open; // Current Disc Open

unsigned char ExtControl; // FDC External Control Register
unsigned char CurrentDrive=0; // FDC Control drive setting
long HeadPos[2]; // Position of Head on each drive for swapping
unsigned char CurrentHead[2]; // Current Head on any drive
int DiscStep[2]; // Single/Double sided disc step
int DiscStrt[2]; // Single/Double sided disc start
unsigned int SecSize[2]; 
unsigned char DiscType[2];
unsigned char MaxSects[2]; // Maximum sectors per track
unsigned int DefStart[2]; // Starting point for head 1
unsigned int TrkLen[2]; // Track Length in bytes
unsigned char DWriteable[2]={0,0}; // Write Protect
char DiskDensity[2];
char SelectedDensity;
unsigned char RotSect=0; // Sector counter to fool Opus DDOS on read address
bool InvertTR00; // Needed because the bloody stupid watford board inverts the input.

// A few defines here
#define DENSITY_MISMATCH DiskDensity[CurrentDrive]!=SelectedDensity

#define SPIN_DOWN_TIME 4000000 // 2secs
#define SETTLE_TIME 30000 // 30 Milliseconds
#define ONE_REV_TIME 500000 // 1 sixth of a second - used for density mismatch
#define SPIN_UP_TIME (ONE_REV_TIME*3) // Two Seconds
#define VERIFY_TIME (ONE_REV_TIME/MaxSects[CurrentDrive])
#define BYTE_TIME (VERIFY_TIME/256)

// Density selects on the disk image, and the actual chip

FILE *sectlog;

static void SetStatus(unsigned char bit) {
	Status|=1<<bit;
}

static void ResetStatus(unsigned char bit) {
	Status&=~(1<<bit);
}

unsigned char Read1770Register(unsigned char Register) {
	if (!Disc1770Enabled)
		return 0xFF;
	// ResetStatus(5);
	// fprintf(fdclog,"Read of Register %d - Status is %02X\n",Register,Status);
	// Read 1770 Register. Note - NOT the FDC Control register @ &FE24
	if ((FDCommand<6) && (FDCommand!=0)) Status^=2; // Fool anything reading the
	// Index pulse signal by alternating it on each read.
	if (Register==0) {
		NMIStatus &= ~(1<<nmi_floppy);
		return(Status);
	}
	if (Register==1) return(ATrack); 
	if (Register==2)
    {
      if ( (DiscType[CurrentDrive] == 3) || (DiscType[CurrentDrive] == 4) )
         return(Sector + 1);
      else
         return(Sector);
    }

	if (Register==3) {
		if (FDCommand>5) 
		{
			ResetStatus(1); NMIStatus &= ~(1<<nmi_floppy); 
		}
		return(Data);
	}
	return(0);
}

void SetMotor(char Drive,bool State) {
	if (Drive==0) LEDs.Disc0=State; else LEDs.Disc1=State;
	if (State) SetStatus(7);
	if (State) {
		if (DiscDriveSoundEnabled && !HeadLoaded[Drive]) {
			PlaySoundSample(SAMPLE_DRIVE_MOTOR, true);
			PlaySoundSample(SAMPLE_HEAD_LOAD, false);
			HeadLoaded[Drive] = TRUE;
		}
	}
	else {
		StopSoundSample(SAMPLE_DRIVE_MOTOR);
		StopSoundSample(SAMPLE_HEAD_SEEK);
		if (DiscDriveSoundEnabled && HeadLoaded[Drive]) {
			PlaySoundSample(SAMPLE_HEAD_UNLOAD, false);
			HeadLoaded[Drive] = FALSE;
		}
	}
}

void Write1770Register(unsigned char Register, unsigned char Value) {
	unsigned char ComBits,HComBits;
    int SectorCycles=0; // Number of cycles to wait for sector to come round

	if (!Disc1770Enabled)
		return;
    //fprintf(fdclog,"Write of %02X to Register %d\n",Value, Register);
	// Write 1770 Register - NOT the FDC Control register @ &FE24
	if (Register==0) {
		NMIStatus &= ~(1<<nmi_floppy); // reset INTRQ
		// Control Register - can only write if current drive is open
		// Changed, now command returns errors if no disc inserted
		ComBits=Value & 0xf0;
		HComBits=Value & 0xe0;
		if (HComBits<0x80) {
			// Type 1 Command
			SetStatus(0);
			ResetStatus(3);
			ResetStatus(4);
			if (HComBits==0x40) { FDCommand=4; HeadDir=1; UpdateTrack=(Value & 16)>>4; } // Step In
			if (HComBits==0x60) { FDCommand=5; HeadDir=0; UpdateTrack=(Value & 16)>>4; } // Step Out
			if (HComBits==0x20) { FDCommand=3; UpdateTrack=(Value & 16)>>4; } // Step
			if (ComBits==0x10) { FDCommand=2; } // Seek
			if (ComBits==0) { FDCommand=1; } // Restore (Seek to Track 00)
			if (FDCommand<6) { 
				ResetStatus(5); SetStatus(0); 
				// Now set some control bits for Type 1 Commands
				ESpinUp=(Value & 8);
				EVerify=(Value & 4);
				CStepRate=StepRate[(Value & 3)]; // Make sure the step rate time is added to the delay time.
				if (!(Status & 128)) {
					NFDCommand=FDCommand; FDCommand=11; /* Spin-Up delay */ LoadingCycles=SPIN_UP_TIME; 
					//if (!ESpinUp) LoadingCycles=ONE_REV_TIME; 
					SetMotor(CurrentDrive,TRUE);
					SetStatus(7);
				} else { LoadingCycles=ONE_REV_TIME; }
				if (DENSITY_MISMATCH) {
					FDCommand=13; // "Confusion spin"
					SetStatus(7); SetMotor(CurrentDrive,TRUE);
					ResetStatus(5); ResetStatus(4); ResetStatus(3); SetStatus(0);
					LoadingCycles=ONE_REV_TIME; // Make it about 4 milliseconds
				}

			}
		}
		SectorCycles=0;
		if (*CDiscOpen && Sector>(RotSect+1))
			SectorCycles=((ONE_REV_TIME)/MaxSects[CurrentDrive])*((RotSect+1)-Sector);
		if (HComBits==0x80) { // Read Sector
			RotSect=Sector;
			SetStatus(0);
			FDCommand=8; MultiSect=(Value & 16)>>4; 
			ResetStatus(1);
		}
		if (HComBits==0xa0) { // Write Sector
			RotSect=Sector;
			SetStatus(0);
			FDCommand=9; MultiSect=(Value & 16)>>4; 
		}

//		if (ComBits==0xe0) { // Read Track		- not implemented yet
//			Sector = 0;
//			Track = Data;
//			RotSect=Sector;
//			FDCommand=20; MultiSect = 1; 
//			ResetStatus(1);
//		}

		if (ComBits==0xf0) { // Write Track
			Sector = 0;
			RotSect=Sector;
			SetStatus(0);
			FDCommand=21; 
		}

        if (ComBits==0xD0) {
			// Force Interrupt - Type 4 Command
			if (FDCommand!=0) {
				ResetStatus(0);
			} else {
				ResetStatus(0); ResetStatus(1); ResetStatus(3); ResetStatus(4); ResetStatus(5);
			}
			FDCommand=12;
			LoadingCycles=4000000;
			NFDCommand=0; // just in case
			Data=0; // If this isn't done, the stupid Opus DDOS tries to use the last 
			// byte of the last sector read in as a Track number for a seek command.
			if ((Value & 0xf)) NMIStatus|=1<<nmi_floppy;
		}
		if (ComBits==0xc0) {
			// Read Address - Type 3 Command
			FDCommand=14;
			SetStatus(0);
			ByteCount=6;
			if (!(Status & 128)) {
				NFDCommand=FDCommand; FDCommand=11; /* Spin-Up delay */ LoadingCycles=SPIN_UP_TIME; 
				//if (!ESpinUp) LoadingCycles=ONE_REV_TIME; // Make it two seconds instead of one
				SetMotor(CurrentDrive,TRUE);
				SetStatus(7);
			} else { LoadingCycles=SectorCycles; }
		}
		if ((FDCommand==8) || (FDCommand==9)) {
			ResetStatus(1);
			// Now set some control bits for Type 2 Commands
			ESpinUp=(Value & 8);
			EVerify=(Value & 4);
			if (!(Status & 128)) {
				NFDCommand=FDCommand; FDCommand=11; /* Spin-Up delay */ LoadingCycles=SPIN_UP_TIME; 
				//if (ESpinUp) LoadingCycles=ONE_REV_TIME; 
				SetMotor(CurrentDrive,TRUE);
				SetStatus(7);
			} else { LoadingCycles=SectorCycles; }
			LoadingCycles+=BYTE_TIME;
			if (DENSITY_MISMATCH) {
				FDCommand=13; // "Confusion spin"
				SetStatus(7); SetMotor(CurrentDrive,TRUE);
				ResetStatus(5); ResetStatus(4); ResetStatus(3); SetStatus(0);
				LoadingCycles=ONE_REV_TIME; 
			}
		}
	}
	if (Register==1) {
		Track=Value; 
		ATrack=Value;
	}
	if (Register==2)
    {
        if ( (DiscType[CurrentDrive] == 3) || (DiscType[CurrentDrive] == 4) )
		   Sector=Value - 1;
		else
           Sector=Value;
    }
	if (Register==3) {
		Data=Value;
		if (FDCommand>5) { ResetStatus(1); NMIStatus &= ~(1<<nmi_floppy); }
	}
}

void Poll1770(int NCycles) {
  for (int d=0;d<2;d++) {
	  if (LightsOn[d]) {
		  SpinDown[d]-=NCycles;
		  if (SpinDown[d]<=0) {
			  SetMotor(d,FALSE);
			  LightsOn[d]=FALSE;
			  if ((!LightsOn[0]) && (!LightsOn[1])) ResetStatus(7);
		  }
	  }
  }

  // This procedure is called from the 6502core to enable the chip to do stuff in the background
  if ((Status & 1) && (NMILock==0)) {
    if (FDCommand<6 && *CDiscOpen && (DiscType[CurrentDrive] == 0 && CurrentHead[CurrentDrive] == 1)) {
      // Single sided disk, disc not ready
      ResetStatus(0);
      SetStatus(4);
      NMIStatus|=1<<nmi_floppy; FDCommand=12;
      return;
    }
	if (FDCommand<6 && *CDiscOpen) {
		int TracksPassed=0; // Holds the number of tracks passed over by the head during seek
		unsigned char OldTrack=(Track >= 80 ? 0 : Track);
		// Seek type commands
		ResetStatus(4); ResetStatus(3);
		if (FDCommand==1) { fseek(CurrentDisc,DiscStrt[CurrentDrive],SEEK_SET); Track=0; } // Restore
		if (FDCommand==2) { fseek(CurrentDisc,DiscStrt[CurrentDrive]+(DiscStep[CurrentDrive]*Data),SEEK_SET); Track=Data;  } // Seek
		if (FDCommand==4) { HeadDir=1; fseek(CurrentDisc,DiscStep[CurrentDrive],SEEK_CUR); Track++;  } // Step In
		if (FDCommand==5) { HeadDir=0; fseek(CurrentDisc,-DiscStep[CurrentDrive],SEEK_CUR); Track--;  } // Step Out
		if (FDCommand==3) { fseek(CurrentDisc,(HeadDir)?DiscStep[CurrentDrive]:-DiscStep[CurrentDrive],SEEK_CUR); Track=(HeadDir)?Track+1:Track-1;  } // Step
		if ((UpdateTrack) || (FDCommand<3)) ATrack=Track;
		FDCommand=15; NFDCommand=0;
		UpdateTrack=0; // This following bit calculates stepping time
		if (Track>=OldTrack) TracksPassed=Track-OldTrack; else TracksPassed=OldTrack-Track;
		OldTrack=Track;
		RotSect=0;
		// Add track * (steprate * 1000) to LoadingCycles
		LoadingCycles=(TracksPassed*(CStepRate*1000));
		LoadingCycles+=((EVerify)?VERIFY_TIME:0);
		if (DiscDriveSoundEnabled) {
			if (TracksPassed > 1) {
				PlaySoundSample(SAMPLE_HEAD_SEEK, true);
				LoadingCycles=TracksPassed*SAMPLE_HEAD_SEEK_CYCLES_PER_TRACK;
			}
			else if (TracksPassed == 1) {
				PlaySoundSample(SAMPLE_HEAD_STEP, false);
				LoadingCycles = SAMPLE_HEAD_STEP_CYCLES;
			}
		}
		return;
	}
	if (FDCommand==15) {
		LoadingCycles-=NCycles;
		if (LoadingCycles<=0) {
			StopSoundSample(SAMPLE_HEAD_SEEK);
			LoadingCycles=SPIN_DOWN_TIME;
			FDCommand=12;
			NMIStatus|=1<<nmi_floppy; 
			if (InvertTR00) { if (Track!=0) ResetStatus(2); else SetStatus(2); }
			else { if (Track==0) ResetStatus(2); else SetStatus(2); }
			ResetStatus(5); ResetStatus(0);
		}
		return;
	}
	if (FDCommand<6 && !*CDiscOpen) {
		// Disc not ready, return seek error;
		ResetStatus(0);
		SetStatus(4);
		NMIStatus|=1<<nmi_floppy; FDCommand=12;
		return;
	}
	if (FDCommand==6) { // Read
		LoadingCycles-=NCycles; if (LoadingCycles>0) return;
		if ((Status & 2)==0) { 
			NFDCommand=0;
			ResetStatus(4); ResetStatus(5); ResetStatus(3); ResetStatus(2);
			ByteCount--;
			// If reading a single sector, and ByteCount== :-
			// 256..1: read + DRQ (256x)
			//      0: INTRQ + rotate disc
			// If reading multiple sectors, and ByteCount== :-
			// 256..2: read + DRQ (255x)
			//      1: read + DRQ + rotate disc + go back to 256
			if (ByteCount>0 && !feof(CurrentDisc)) { Data=fgetc(CurrentDisc); SetStatus(1); NMIStatus|=1<<nmi_floppy; } // DRQ
			if (ByteCount==0 || ((ByteCount==1) && (MultiSect))) RotSect++; if (RotSect>MaxSects[CurrentDrive]) RotSect=0;
			if ((ByteCount==0) && (!MultiSect)) { ResetStatus(0); NMIStatus|=1<<nmi_floppy; fseek(CurrentDisc,HeadPos[CurrentDrive],SEEK_SET); FDCommand=10; ResetStatus(1); } // End of sector
			if ((ByteCount==1) && (MultiSect)) { ByteCount=SecSize[CurrentDrive]+1; Sector++; 
				if (Sector==MaxSects[CurrentDrive]) { MultiSect=0; /* Sector=0; */ }
			}
			LoadingCycles=BYTE_TIME; // Slow down the read a bit :)
		}
		return;
	}
	if ((FDCommand==7) && (DWriteable[CurrentDrive]==1)) { // Write
		LoadingCycles-=NCycles; if (LoadingCycles>0) return;
		if ((Status & 2)==0) { 
			// DRQ already issued and answered
			NFDCommand=0;
			ResetStatus(4); ResetStatus(5); ResetStatus(3); ResetStatus(2); 
			ByteCount--;
			// If writing a single sector, and ByteCount== :-
			// 256..2: write + next DRQ (255x)
			//      1: write + INTRQ + rotate disc
			// If writing multiple sectors, and ByteCount== :-
			// 256..2: write + next DRQ (255x)
			//      1: write + next DRQ + rotate disc + go back to 256
			fputc(Data,CurrentDisc);
			if ((ByteCount>1) || (MultiSect)) { SetStatus(1); NMIStatus|=1<<nmi_floppy; } // DRQ
			if (ByteCount<=1) RotSect++; if (RotSect>MaxSects[CurrentDrive]) RotSect=0;
			if ((ByteCount<=1) && (!MultiSect)) { ResetStatus(0); NMIStatus|=1<<nmi_floppy; fseek(CurrentDisc,HeadPos[CurrentDrive],SEEK_SET); FDCommand=10; ResetStatus(1); }
			if ((ByteCount<=1) && (MultiSect)) { ByteCount=SecSize[CurrentDrive]+1; Sector++; 
				if (Sector==MaxSects[CurrentDrive]) { MultiSect=0; /* Sector=0; */ }
			}
			LoadingCycles=BYTE_TIME; // Bit longer for a write
		} 
		return;
	}
	if ((FDCommand==7) && (DWriteable[CurrentDrive]==0)) {
 //		ResetStatus(0);
		SetStatus(6);
		NMIStatus|=1<<nmi_floppy; 
        FDCommand=0;
	}
	if ((FDCommand>=8) && (*CDiscOpen==1) && (FDCommand<=10)) { // Read/Write Prepare
		SetStatus(0);
		ResetStatus(5); ResetStatus(6); ResetStatus(2);
		ByteCount=SecSize[CurrentDrive]+1; DataPos=ftell(CurrentDisc); HeadPos[CurrentDrive]=DataPos;
		LoadingCycles=45;
		fseek(CurrentDisc,DiscStrt[CurrentDrive]+(DiscStep[CurrentDrive]*Track)+(Sector*SecSize[CurrentDrive]),SEEK_SET);
	}
	if ((FDCommand>=8) && (*CDiscOpen==0) && (FDCommand<=9)) {
		ResetStatus(0);
		SetStatus(4);
		NMIStatus|=1<<nmi_floppy; FDCommand=0;
	}
	if ((FDCommand==8) && (*CDiscOpen==1)) FDCommand=6;
	if ((FDCommand==9) && (*CDiscOpen==1)) { FDCommand=7; SetStatus(1); NMIStatus|=1<<nmi_floppy; }
  }
  
// Not implemented Read Track yet, perhaps don't really need it

//	if (FDCommand==22) { // Read Track
//		LoadingCycles-=NCycles; if (LoadingCycles>0) return;
//		if ((dStatus & 2)==0) { 
//			NFDCommand=0;
//			ResetStatus(4); ResetStatus(5); ResetStatus(3); ResetStatus(2);
//			if (!feof(CurrentDisc)) { Data=fgetc(CurrentDisc); SetStatus(1); NMIStatus|=1<<nmi_floppy; } // DRQ
//			dByteCount--;
//			if (dByteCount==0) RotSect++; if (RotSect>MaxSects[CurrentDrive]) RotSect=0;
//			if ((dByteCount==0) && (!MultiSect)) { ResetStatus(0); NMIStatus|=1<<nmi_floppy; fseek(CurrentDisc,HeadPos[CurrentDrive],SEEK_SET); FDCommand=10; } // End of sector
//			if ((dByteCount==0) && (MultiSect)) { dByteCount=257; Sector++; 
//				if (Sector==MaxSects[CurrentDrive]) { MultiSect=0; /* Sector=0; */ }
//			}
//			LoadingCycles=BYTE_TIME; // Slow down the read a bit :)
//		}
//		return;
//	}

	if ((FDCommand==23) && (DWriteable[CurrentDrive]==1)) { // Write Track
		LoadingCycles-=NCycles; if (LoadingCycles>0) return;
		if ((Status & 2)==0) { 

//            if (ByteCount == 0)
//                fprintf(tlog, "Formatting Track %d, Sector %d\n", Track, Sector);

            NFDCommand=0;
			ResetStatus(4); ResetStatus(5); ResetStatus(3); ResetStatus(2); 

            SetStatus(1);
            NMIStatus|=1<<nmi_floppy; // DRQ

            switch (FormatState)
            {
            case 0x00 :
                FormatPtr = FormatBuffer;
                *FormatPtr++ = Data;
                FormatState++;
                break;
            case 0x01 :
                *FormatPtr++ = Data;
                if (Data == 0xfb)       // Data Marker
                {
                    FormatState++;
                    FormatCount = 0;
                    FormatSize = 256;       // Assume default
                    unsigned char *ptr = FormatBuffer;
                    while (*ptr != 0xfe) ptr++;
                    ptr++;

                    switch (ptr[3])
                    {
                    case 0 :
                        FormatSize = 128;
                        break;
                    case 1 :
                        FormatSize = 256;
                        break;
                    case 2 :
                        FormatSize = 512;
                        MaxSects[CurrentDrive] = 8;
                		SecSize[CurrentDrive]=512; 
		                DiskDensity[CurrentDrive]=0;
                		DiscStep[CurrentDrive]=512 * 9 * 2; 
                		DiscStrt[CurrentDrive]=ptr[1] * 512 * 9;       // Head number 0 or 1
                		DefStart[CurrentDrive]=512 * 9;
                		TrkLen[CurrentDrive]=512 * 9; 
                        DiscType[CurrentDrive] = 4;
                        break;
                    case 3 :
                        FormatSize = 1024;
                        MaxSects[CurrentDrive] = 4;
                		SecSize[CurrentDrive]=1024; 
		                DiskDensity[CurrentDrive]=0;
                		DiscStep[CurrentDrive]=1024 * 5 * 2; 
                		DiscStrt[CurrentDrive]=ptr[1] * 1024 * 5;       // Head number 0 or 1
                		DefStart[CurrentDrive]=1024 * 5;
                		TrkLen[CurrentDrive]=1024 * 5; 
                        DiscType[CurrentDrive] = 3;
                        break;
                    }
                }
                break;
            case 0x02 :                 // Sector contents
                if (FormatCount < FormatSize)
                {
                    fputc(Data,CurrentDisc);
                    FormatCount++;
                }
                else
                {
                    FormatState++;
                    FormatCount = 0;
                    Sector++;

    				if (Sector>MaxSects[CurrentDrive]) 
                    {
                        ResetStatus(0);
                        NMIStatus|=1<<nmi_floppy;
                        fseek(CurrentDisc,HeadPos[CurrentDrive],SEEK_SET);
                        FDCommand=10; 
                        ResetStatus(1);
                    }
                }

                break;

            case 0x03 :                 // 0xF7 - write CRC
                FormatState = 0;
                break;
            }

            LoadingCycles=BYTE_TIME; // Bit longer for a write
		} 
		return;
	}
            
	if ((FDCommand==23) && (DWriteable[CurrentDrive]==0)) 
    {
//        fprintf(tlog, "Disc Write Protected\n");
		SetStatus(6);
		NMIStatus|=1<<nmi_floppy; 
        FDCommand=0;
	}

	if ((FDCommand>=20) && (*CDiscOpen==1) && (FDCommand<=21)) { // Read/Write Track Prepare
		SetStatus(0);
		ResetStatus(5); ResetStatus(6); ResetStatus(2);
		LoadingCycles=45;
		fseek(CurrentDisc,DiscStrt[CurrentDrive]+(DiscStep[CurrentDrive]*Track),SEEK_SET);
        Sector = 0;
        ByteCount=0; DataPos=ftell(CurrentDisc); HeadPos[CurrentDrive]=DataPos;
//        fprintf(tlog, "Read/Write Track Prepare - Disc = %d, Track = %d\n", CurrentDrive, Track);
    }
	if ((FDCommand>=20) && (*CDiscOpen==0) && (FDCommand<=21)) {

//        fprintf(tlog, "ResetStatus(0) Here 8\n");

        ResetStatus(0);
		SetStatus(4);
		NMIStatus|=1<<nmi_floppy; FDCommand=0;
	}
	if ((FDCommand==20) && (*CDiscOpen==1)) FDCommand=22;
	if ((FDCommand==21) && (*CDiscOpen==1)) { FDCommand=23; FormatState = 0; SetStatus(1); NMIStatus|=1<<nmi_floppy; }
  
  if (FDCommand==10) {
	ResetStatus(0);
	ResetStatus(4);
	ResetStatus(5);
	ResetStatus(6);
	if (NFDCommand==255) {
		// Error during access
		SetStatus(4);
		ResetStatus(3);
		if ((Track==0) && (InvertTR00)) SetStatus(2); else ResetStatus(2);
		if ((Track==0) && (!InvertTR00)) ResetStatus(2); else SetStatus(2);
	}
	NMIStatus|=1<<nmi_floppy; FDCommand=12; LoadingCycles=SPIN_DOWN_TIME; // Spin-down delay
	return;
  }
  if (FDCommand==11) {
  	// Spin-up Delay
	LoadingCycles-=NCycles;
	if (LoadingCycles<=0) {
		FDCommand=NFDCommand;
		if (NFDCommand<6) SetStatus(5); else ResetStatus(5);
	}
	return;
  }
  if (FDCommand==12) { // Spin Down delay
	  if (EVerify) LoadingCycles+=SETTLE_TIME;
	  LightsOn[CurrentDrive]=TRUE;
	  SpinDown[CurrentDrive]=SPIN_DOWN_TIME;
	  RotSect=0; FDCommand=0;
	  ResetStatus(0);
	  return;
  }
  if (FDCommand==13) { // Confusion spin
	  LoadingCycles-=NCycles;
	  if ((LoadingCycles<2000) && (!(Status & 32))) SetStatus(5); // Compelte spin-up, but continuing whirring
	  if (LoadingCycles<=0) FDCommand=10; NFDCommand=255; // Go to spin down, but report error.
	  SpinDown[CurrentDrive]=SPIN_DOWN_TIME;
	  LightsOn[CurrentDrive]=TRUE;
	  return;
  }
  if (FDCommand==14) { // Read Address - just 6 bytes
	  LoadingCycles-=NCycles; if (LoadingCycles>0) return;
      if ((Status & 2)==0) { 

      NFDCommand=0;
      ResetStatus(4); ResetStatus(5); ResetStatus(3); ResetStatus(2);

      if (ByteCount==6) Data=Track;
      if (ByteCount==5) Data=CurrentHead[CurrentDrive];
      if (ByteCount==4) Data=RotSect+1;
      if (ByteCount==3) {
          if (DiscType[CurrentDrive] == 0) Data = 1;        // 256
          if (DiscType[CurrentDrive] == 1) Data = 1;        // 256
          if (DiscType[CurrentDrive] == 2) Data = 1;        // 256
          if (DiscType[CurrentDrive] == 3) Data = 3;        // 1024
          if (DiscType[CurrentDrive] == 4) Data = 2;        // 512
      }
      if (ByteCount==2) Data=0;
      if (ByteCount==1) Data=0;

	  if (ByteCount==0) { 

          FDCommand=0; 
          ResetStatus(0); 
          RotSect++; 
		  if (RotSect==(MaxSects[CurrentDrive]+1)) RotSect=0; 
		  FDCommand=10;
		  return;
      }
        
      SetStatus(1); 
	  ByteCount--;
	  NMIStatus|=1<<nmi_floppy;
      LoadingCycles=BYTE_TIME; // Slow down the read a bit :)
    }
	return;
    
  }
}		

void Load1770DiscImage(char *DscFileName,int DscDrive,unsigned char DscType,HMENU dmenu) {
	long int TotalSectors;
	long HeadStore;
	bool openFailure=false;
	if (DscDrive==0) {
		if (Disc0Open==1) fclose(Disc0);
		Disc0Open=0;
		Disc0=fopen(DscFileName,"rb+");
		if (Disc0!=NULL) {
			EnableMenuItem(dmenu, IDM_WPDISC0, MF_ENABLED );
		}
		else {
			Disc0=fopen(DscFileName,"rb");
			if (Disc0!=NULL)
				EnableMenuItem(dmenu, IDM_WPDISC0, MF_GRAYED );
		}
		if (Disc0) {
			if (CurrentDrive==0) CurrentDisc=Disc0;
			Disc0Open=1;
		}
		else {
			openFailure=true;
		}
	}
	if (DscDrive==1) {
		if (Disc1Open==1) fclose(Disc1);
		Disc1Open=0;
		Disc1=fopen(DscFileName,"rb+");
		if (Disc1!=NULL) {
			EnableMenuItem(dmenu, IDM_WPDISC1, MF_ENABLED );
		}
		else {
			Disc1=fopen(DscFileName,"rb");
			if (Disc1!=NULL)
				EnableMenuItem(dmenu, IDM_WPDISC1, MF_GRAYED );
		}
		if (Disc1) {
			if (CurrentDrive==1) CurrentDisc=Disc1;
			Disc1Open=1;
		}
		else {
			openFailure=true;
		}
	}

	if (openFailure)
	{
		char errstr[200];
		sprintf(errstr, "Could not open disc file:\n  %s", DscFileName);
		MessageBox(GETHWND,errstr,WindowTitle,MB_OK|MB_ICONERROR);
		return;
	}

	strcpy(DscFileNames[DscDrive], DscFileName);

//	if (DscType=0) CurrentHead[DscDrive]=0;
//  Feb 14th 2001 - Valentines Day - Bah Humbug - ADFS Support added here
    if (DscType==0) { 
		SecSize[DscDrive]=256; 
		DiskDensity[DscDrive]=1;
		DiscStep[DscDrive]=2560; 
		DiscStrt[DscDrive]=0; 
		DefStart[DscDrive]=80*10*256; // 0; 
		TrkLen[DscDrive]=2560; 
	}
    if (DscType==1) { 
		SecSize[DscDrive]=256; 
		DiskDensity[DscDrive]=1;
		DiscStep[DscDrive]=5120; 
		DiscStrt[DscDrive]=CurrentHead[DscDrive]*2560; 
		DefStart[DscDrive]=2560; 
		TrkLen[DscDrive]=2560; 
	}
    if (DscType==3) { 
		SecSize[DscDrive]=1024; 
		DiskDensity[DscDrive]=0;
		DiscStep[DscDrive]=1024 * 5 * 2; 
		DiscStrt[DscDrive]=CurrentHead[DscDrive]*1024 * 5; 
		DefStart[DscDrive]=1024 * 5;
		TrkLen[DscDrive]=1024 * 5; 
	}
    if (DscType==4) { 
		SecSize[DscDrive]=512; 
		DiskDensity[DscDrive]=0;
		DiscStep[DscDrive]=512 * 9 * 2; 
		DiscStrt[DscDrive]=CurrentHead[DscDrive]*512 * 9; 
		DefStart[DscDrive]=512 * 9;
		TrkLen[DscDrive]=512 * 9; 
	}
    if (DscType==2) { 
		SecSize[DscDrive]=256; 
		DiskDensity[DscDrive]=0;
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
		fseek(CurrentDisc,0xfc,SEEK_SET);
		TotalSectors=fgetc(CurrentDisc);
		TotalSectors|=fgetc(CurrentDisc)<<8;
		TotalSectors|=fgetc(CurrentDisc)<<16;
		fseek(CurrentDisc,HeadStore,SEEK_SET);
		if ( (TotalSectors == 0x500) || (TotalSectors == 0x280) ) {		// Just so 1024 sector mixed mode ADFS/NET discs can be recognised as dbl sided
			DiscStep[DscDrive]=4096;
			DiscStrt[DscDrive]=0;
			DefStart[DscDrive]=0;
			TrkLen[DscDrive]=4096;
		}
	}
	DiscType[DscDrive]=DscType;
	MaxSects[DscDrive]=(DscType<2)?9:15;
	if (DscType == 3) MaxSects[DscDrive] = 4;
	if (DscType == 4) MaxSects[DscDrive] = 8;
	mainWin->SetImageName(DscFileName,DscDrive,DscType);
}

void WriteFDCControlReg(unsigned char Value) {
	// This function writes the control register @ &FE24
	//fprintf(fdclog,"CTRL REG write of %02X\n",Value);
	ExtControl=Value;
	if ((ExtControl & 1)==1) { CurrentDisc=Disc0; CurrentDrive=0; CDiscOpen=&Disc0Open; }
	if ((ExtControl & 2)==2) { CurrentDisc=Disc1; CurrentDrive=1; CDiscOpen=&Disc1Open; }
	if ((ExtControl & 16)==16 && CurrentHead[CurrentDrive]==0) { 
		CurrentHead[CurrentDrive]=1; 
		if (*CDiscOpen) fseek(CurrentDisc,TrkLen[CurrentDrive],SEEK_CUR); 
		DiscStrt[CurrentDrive]=DefStart[CurrentDrive]; 
	}
	if ((ExtControl & 16)!=16 && CurrentHead[CurrentDrive]==1) { 
		CurrentHead[CurrentDrive]=0; 
 		if (*CDiscOpen) fseek(CurrentDisc,0-TrkLen[CurrentDrive],SEEK_CUR); 
		DiscStrt[CurrentDrive]=0; 
	}
	SelectedDensity=(Value & 32)>>5; // Density Select - 0 = Double 1 = Single
//	SelectedDensity=1;
}

unsigned char ReadFDCControlReg(void) {
	return(ExtControl);
}

void Reset1770(void) {
	//fdclog=fopen("/fd.log","wb");
	CurrentDisc=Disc0;
	CurrentDrive=0;
	CurrentHead[0]=CurrentHead[1]=0;
	DiscStrt[0]=0; 
	DiscStrt[1]=0; 
	if (Disc0) fseek(Disc0,0,SEEK_SET);
	if (Disc1) fseek(Disc1,0,SEEK_SET);
	SetMotor(0,FALSE);
	SetMotor(1,FALSE);
	Status=0;
	ExtControl=1; // Drive 0 selected, single density, side 0
	MaxSects[0]=(DiscType[0]<2)?9:15;
	MaxSects[1]=(DiscType[1]<2)?9:15;
	if (DiscType[0] == 3) MaxSects[0] = 4;
	if (DiscType[1] == 3) MaxSects[1] = 4;
	if (DiscType[0] == 4) MaxSects[0] = 8;
	if (DiscType[1] == 4) MaxSects[1] = 8;
}

void Close1770Disc(char Drive) {
	if ((Drive==0) && (Disc0Open)) {
		fclose(Disc0);
		Disc0=NULL;
		Disc0Open=0;
		DscFileNames[0][0] = 0;
	}
	if ((Drive==1) && (Disc1Open)) {
		fclose(Disc1);
		Disc1=NULL;
		Disc1Open=0;
		DscFileNames[1][0] = 0;
	}
}

#define BPUT(a) fputc(a,NewImage); CheckSum=(CheckSum+a)&255

void CreateADFSImage(char *ImageName,unsigned char Drive,unsigned char Tracks, HMENU dmenu) {
	// This function creates a blank ADFS disc image, and then loads it.
	FILE *NewImage;
	int CheckSum;
	int ent;
	int sectors=(Tracks*16);
	NewImage=fopen(ImageName,"wb");
	if (NewImage!=NULL) {
		// Now write the data - this is complex
		// Free space map - T0S0 - start sectors
		CheckSum=0;
		BPUT(7); BPUT(0); BPUT(0);
		for (ent=0;ent<0xf9;ent++) BPUT(0);
		BPUT(sectors & 255); BPUT((sectors>>8)&255); BPUT(0); // Total sectors
		BPUT(CheckSum&255); // Checksum Byte
		CheckSum=0;
		// Free space map - T0S1 - lengths
		BPUT((sectors-7)&255); BPUT(((sectors-7)>>8)&255); BPUT(0); // Length of first free space
		for (ent=0;ent<0xfb;ent++) BPUT(0);
		BPUT(3); BPUT(CheckSum);
		// Root Catalogue - T0S2-T0S7
		BPUT(1);
		BPUT('H'); BPUT('u'); BPUT('g'); BPUT('o'); // Hugo
		for (ent=0; ent<47; ent++) {
			int bcount;
			// 47 catalogue entries
			for (bcount=5; bcount<0x1e; bcount++) BPUT(0);
			BPUT(ent);
		}
		BPUT(0);
		BPUT('$');		// Set root directory name
		BPUT(13);
		for (ent=0x0; ent<11; ent++) BPUT(0);
		BPUT('$');		// Set root title name
		BPUT(13);
		for (ent=0x0; ent<31; ent++) BPUT(0); 
		BPUT(1);
		BPUT('H'); BPUT('u'); BPUT('g'); BPUT('o'); // Hugo
		BPUT(0);
		fclose(NewImage);
		Load1770DiscImage(ImageName,Drive,2,dmenu);
	}
}

void Save1770UEF(FILE *SUEF)
{
	extern char FDCDLL[256];
	extern char CDiscName[2][256];
	char blank[256];
	memset(blank,0,256);

	fput16(0x046F,SUEF);
	fput32(857,SUEF);
	fputc(DiscType[0],SUEF);
	fputc(DiscType[1],SUEF);

	if (Disc0Open==0) {
		// No disc in drive 0
		fwrite(blank,1,256,SUEF);
	}
	else {
		fwrite(CDiscName[0],1,256,SUEF);
	}
	if (Disc1Open==0) {
		// No disc in drive 1
		fwrite(blank,1,256,SUEF);
	}
	else {
		fwrite(CDiscName[1],1,256,SUEF);
	}

	fputc(Status,SUEF);
	fputc(Data,SUEF);
	fputc(Track,SUEF);
	fputc(ATrack,SUEF);
	fputc(Sector,SUEF);
	fputc(HeadDir,SUEF);
	fputc(FDCommand,SUEF);
	fputc(NFDCommand,SUEF);
	fput32(LoadingCycles,SUEF);
	fput32(SpinDown[0],SUEF);
	fput32(SpinDown[1],SUEF);
	fputc(UpdateTrack,SUEF);
	fputc(MultiSect,SUEF);
	fputc(CStepRate,SUEF);
	fputc(ESpinUp,SUEF);
	fputc(EVerify,SUEF);
	fputc(LightsOn[0],SUEF);
	fputc(LightsOn[1],SUEF);
	fput32(ByteCount,SUEF);
	fput32(DataPos,SUEF);
	fputc(ExtControl,SUEF);
	fputc(CurrentDrive,SUEF);
	fput32(HeadPos[0],SUEF);
	fput32(HeadPos[1],SUEF);
	fputc(CurrentHead[0],SUEF);
	fputc(CurrentHead[1],SUEF);
	fput32(DiscStep[0],SUEF);
	fput32(DiscStep[1],SUEF);
	fput32(DiscStrt[0],SUEF);
	fput32(DiscStrt[1],SUEF);
	fputc(MaxSects[0],SUEF);
	fputc(MaxSects[1],SUEF);
	fput32(DefStart[0],SUEF);
	fput32(DefStart[1],SUEF);
	fput32(TrkLen[0],SUEF);
	fput32(TrkLen[1],SUEF);
	fputc(DWriteable[0],SUEF);
	fputc(DWriteable[1],SUEF);
	fputc(DiskDensity[0],SUEF);
	fputc(DiskDensity[1],SUEF);
	fputc(SelectedDensity,SUEF);
	fputc(RotSect,SUEF);
	fwrite(FDCDLL,1,256,SUEF);
}

void Load1770UEF(FILE *SUEF,int Version)
{
	extern char FDCDLL[256];
	extern bool DiscLoaded[2];
	extern bool NativeFDC;
	char FileName[256];
	int Loaded=0;
	int LoadFailed=0;

	// Close current images, don't want them corrupted if
	// saved state was in middle of writing to disc.
	Close1770Disc(0);
	Close1770Disc(1);
	DiscLoaded[0]=FALSE;
	DiscLoaded[1]=FALSE;

	DiscType[0]=fgetc(SUEF);
	DiscType[1]=fgetc(SUEF);

	fread(FileName,1,256,SUEF);
	if (FileName[0]) {
		// Load drive 0
		Loaded=1;
		Load1770DiscImage(FileName, 0, DiscType[0], mainWin->m_hMenu);
		if (!Disc0Open)
			LoadFailed=1;
	}

	fread(FileName,1,256,SUEF);
	if (FileName[0]) {
		// Load drive 1
		Loaded=1;
		Load1770DiscImage(FileName, 1, DiscType[1], mainWin->m_hMenu);
		if (!Disc1Open)
			LoadFailed=1;
	}

	if (Loaded && !LoadFailed)
	{
		Status=fgetc(SUEF);
		Data=fgetc(SUEF);
		Track=fgetc(SUEF);
		ATrack=fgetc(SUEF);
		Sector=fgetc(SUEF);
		HeadDir=fgetc(SUEF);
		FDCommand=fgetc(SUEF);
		NFDCommand=fgetc(SUEF);
		LoadingCycles=fget32(SUEF);
		SpinDown[0]=fget32(SUEF);
		SpinDown[1]=fget32(SUEF);
		UpdateTrack=fgetc(SUEF);
		MultiSect=fgetc(SUEF);
		CStepRate=fgetc(SUEF);
		ESpinUp=fgetc(SUEF);
		EVerify=fgetc(SUEF);
		LightsOn[0]=(fgetc(SUEF)!=0);
		LightsOn[1]=(fgetc(SUEF)!=0);
		ByteCount=fget32(SUEF);
		DataPos=fget32(SUEF);
		ExtControl=fgetc(SUEF);
		CurrentDrive=fgetc(SUEF);
		HeadPos[0]=fget32(SUEF);
		HeadPos[1]=fget32(SUEF);
		CurrentHead[0]=fgetc(SUEF);
		CurrentHead[1]=fgetc(SUEF);
		DiscStep[0]=fget32(SUEF);
		DiscStep[1]=fget32(SUEF);
		DiscStrt[0]=fget32(SUEF);
		DiscStrt[1]=fget32(SUEF);
		MaxSects[0]=fgetc(SUEF);
		MaxSects[1]=fgetc(SUEF);
		DefStart[0]=fget32(SUEF);
		DefStart[1]=fget32(SUEF);
		TrkLen[0]=fget32(SUEF);
		TrkLen[1]=fget32(SUEF);
		DWriteable[0]=fgetc(SUEF);
		DWriteable[1]=fgetc(SUEF);
		DiskDensity[0]=fgetc(SUEF);
		if (Version <= 9)
			DiskDensity[1]=DiskDensity[0];
		else
			DiskDensity[1]=fgetc(SUEF);
		SelectedDensity=fgetc(SUEF);
		RotSect=fgetc(SUEF);
		fread(FDCDLL,1,256,SUEF);

		if (CurrentDrive==1)
			CDiscOpen=&Disc1Open;
		else
			CDiscOpen=&Disc0Open;

		if (MachineType!=3) {
			mainWin->LoadFDC(FDCDLL, false);
		}
	}
}

/*--------------------------------------------------------------------------*/
void Get1770DiscInfo(int DscDrive, int *Type, char *pFileName)
{
	*Type = DiscType[DscDrive];
	strcpy(pFileName, DscFileNames[DscDrive]);
}
