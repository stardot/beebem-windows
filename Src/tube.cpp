/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 1994  David Alan Gilbert
Copyright (C) 1997  Mike Wyatt
Copyright (C) 2001  Richard Gellman

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

/* 6502 core - 6502 emulator core - David Alan Gilbert 16/10/94 */
/* Mike Wyatt 7/6/97 - Added undocumented instructions */
/* Copied for 65C02 Tube core - 13/04/01 */

#include <iostream>
#include <fstream>
#include <stdio.h>
#include <stdlib.h>

#include "6502core.h"
#include "main.h"
#include "beebmem.h"
#include "tube.h"
#include "debug.h"
#include "uefstate.h"
#include "z80mem.h"
#include "z80.h"
#include "Arm.h"

#ifdef WIN32
#include <windows.h>
#define INLINE inline
#else
#define INLINE
#endif

// Some interrupt set macros
#define SETTUBEINT(a) TubeintStatus|=1<<a
#define RESETTUBEINT(a) TubeintStatus&=~(1<<a)

static int CurrentInstruction;
unsigned char TubeRam[65536];
extern int DumpAfterEach;
unsigned char TubeEnabled,AcornZ80,EnableTube;
#ifdef M512COPRO_ENABLED
unsigned char Tube186Enabled;
#endif
unsigned char TubeMachineType=3;

CycleCountT TotalTubeCycles=0;  

int TubeProgramCounter;
static int PreTPC; // Previous Tube Program Counter;
static int Accumulator,XReg,YReg;
static unsigned char StackReg,PSR;
static unsigned char IRQCycles;

unsigned char TubeintStatus=0; /* bit set (nums in IRQ_Nums) if interrupt being caused */
unsigned char TubeNMIStatus=0; /* bit set (nums in NMI_Nums) if NMI being caused */
static unsigned int NMILock=0; /* Well I think NMI's are maskable - to stop repeated NMI's - the lock is released when an RTI is done */

typedef int int16;



/* Note how GETCFLAG is special since being bit 0 we don't need to test it to get a clean 0/1 */
#define GETCFLAG ((PSR & FlagC))
#define GETZFLAG ((PSR & FlagZ)>0)
#define GETIFLAG ((PSR & FlagI)>0)
#define GETDFLAG ((PSR & FlagD)>0)
#define GETBFLAG ((PSR & FlagB)>0)
#define GETVFLAG ((PSR & FlagV)>0)
#define GETNFLAG ((PSR & FlagN)>0)

/* Types for internal function arrays */
typedef void (*InstrHandlerFuncType)(int16 Operand);
typedef int16 (*AddrModeHandlerFuncType)(int WantsAddr);

static int TubeCyclesTable[]={
  7,6,0,0,0,3,5,5,3,2,2,0,0,4,6,0, /* 0 */
  2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0, /* 1 */
  6,6,0,0,3,3,5,0,4,2,2,0,4,4,6,0, /* 2 */
  2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0, /* 3 */
  6,6,0,0,0,3,5,0,3,2,2,2,3,4,6,0, /* 4 */
  2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0, /* 5 */
  6,6,0,0,0,3,5,0,4,2,2,0,5,4,6,0, /* 6 */
  2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0, /* 7 */
  2,6,0,0,3,3,3,3,2,0,2,0,4,4,4,0, /* 8 */
  2,6,0,0,4,4,4,0,2,5,2,0,0,5,0,0, /* 9 */
  2,6,2,0,3,3,3,0,2,2,2,0,4,4,4,0, /* a */
  2,5,0,0,4,4,4,0,2,4,2,0,4,4,4,0, /* b */
  2,6,0,0,3,3,5,0,2,2,2,0,4,4,6,0, /* c */
  2,5,0,0,0,4,6,0,2,4,0,0,4,4,7,0, /* d */
  2,6,0,0,3,3,5,0,2,2,2,0,4,4,6,0, /* e */
  2,5,0,0,0,4,6,0,2,4,0,0,0,4,7,0  /* f */
}; /* TubeCyclesTable */

/* The number of TubeCycles to be used by the current instruction - exported to
   allow fernangling by memory subsystem */
unsigned int TubeCycles;

static unsigned char Branched;
// Branched - 1 if the instruction branched

/* A macro to speed up writes - uses a local variable called 'tmpaddr' */
#define TUBEREADMEM_FAST(a) ((a<0xfef8)?TubeRam[a]:TubeReadMem(a))
#define TUBEWRITEMEM_FAST(Address, Value) if (Address<0xfef8) TubeRam[Address]=Value; else TubeWriteMem(Address,Value);
#define TUBEWRITEMEM_DIRECT(Address, Value) TubeRam[Address]=Value;
#define TUBEFASTWRITE(addr,val) tmpaddr=addr; if (tmpaddr<0xfef8) TUBEWRITEMEM_DIRECT(tmpaddr,val) else TubeWriteMem(tmpaddr,val);

// Local fns
void Reset65C02(void);
void ResetTube(void);

// Staus bits
enum TubeFlags {
	TubeQ=1,         // Host IRQ from reg 4
	TubeI=2,         // Parasite IRQ from reg 1
	TubeJ=4,         // Parasite IRQ from reg 4
	TubeM=8,         // Parasite NMI from reg 3
	TubeV=16,        // Two byte op for reg 3
	TubeP=32,        // Parasite processor reset
	TubeT=64,        // Tube reset (write only)
	TubeNotFull=64,  // Reg not full (read only)
	TubeS=128,       // Set control flags mask (write only)
	TubeDataAv=128   // Data available (read only)
};

// Tube registers
unsigned char R1Status; // Q,I,J,M,V,P flags

unsigned char R1PHData[24];
unsigned char R1PHPtr;
unsigned char R1HStatus;
unsigned char R1HPData;
unsigned char R1PStatus;

unsigned char R2PHData;
unsigned char R2HStatus;
unsigned char R2HPData;
unsigned char R2PStatus;

unsigned char R3PHData[2];
unsigned char R3PHPtr;
unsigned char R3HStatus;
unsigned char R3HPData[2];
unsigned char R3HPPtr;
unsigned char R3PStatus;

unsigned char R4PHData;
unsigned char R4HStatus;
unsigned char R4HPData;
unsigned char R4PStatus;

/*-------------------------------------------------------------------*/
// Tube interupt functions
void UpdateR1Interrupt(void) {
	if ((R1Status & TubeI) && (R1PStatus & TubeDataAv))
		SETTUBEINT(R1);
	else
		RESETTUBEINT(R1);
}

void UpdateR4Interrupt(void) {
	if ((R1Status & TubeJ) && (R4PStatus & TubeDataAv))
		SETTUBEINT(R4);
	else
		RESETTUBEINT(R4);
}

void UpdateR3Interrupt(void) {
	if ((R1Status & TubeM) && !(R1Status & TubeV) &&
		( (R3HPPtr > 0) || (R3PHPtr == 0) ))
		TubeNMIStatus|=(1<<R3);
	else if ((R1Status & TubeM) && (R1Status & TubeV) &&
		( (R3HPPtr > 1) || (R3PHPtr == 0) ))
		TubeNMIStatus|=(1<<R3);
	else
		TubeNMIStatus&=~(1<<R3);
}

void UpdateHostR4Interrupt(void) {
	if ((R1Status & TubeQ) && (R4HStatus & TubeDataAv))
		intStatus|=(1<<tube);
	else
		intStatus&=~(1<<tube);
}


/*-------------------------------------------------------------------*/
// Torch tube memory/io handling functions

int TorchTubeActive = 0;

unsigned char ReadTorchTubeFromHostSide(unsigned char IOAddr) 
{
unsigned char TmpData;

	TmpData = 0xff;
	
	if (!TorchTubeActive) 
		return(MachineType==3 ? 0xff : 0xfe); // return ff for master else return fe

	switch (IOAddr) {
	case 0:
		TmpData=R1HStatus | R1Status;
		break;
	case 1:
		TmpData=R1PHData[0];
		R1HStatus&=~TubeDataAv;
		R1PStatus|=TubeNotFull;
		break;

// Data available in Z80 tube ?
// Bit #2 says data available to read in 0xfee1
// Bit #10 says room available to write to 0xfee1

	case 0x0d:
		TmpData = 0;
		
		if (R1HStatus & TubeDataAv) TmpData |= 0x02;
		if (R1HStatus & TubeNotFull) TmpData |= 0x10;
		
		break;

	case 0x10:

//		trace = 1;
		
		break;
		
	}
	
//	fprintf(tlog, "ReadTorchTubeFromHostSide - Addr = %02x, Value = %02x\n", (int) IOAddr, (int) TmpData);
	
	if (DebugEnabled) {
		char info[200];
		sprintf(info, "Tube: Read from host, addr %X value %02X", (int)IOAddr, (int)TmpData);
		DebugDisplayTrace(DEBUG_TUBE, true, info);
	}

	return TmpData;
}

void WriteTorchTubeFromHostSide(unsigned char IOAddr,unsigned char IOData) 
{

//	fprintf(tlog, "WriteTorchTubeFromHostSide - Addr = %02x, Value = %02x\n", (int) IOAddr, (int) IOData);

	if (DebugEnabled) {
		char info[200];
		sprintf(info, "Tube: Write from host, addr %X value %02X", (int)IOAddr, (int)IOData);
		DebugDisplayTrace(DEBUG_TUBE, true, info);
	}

	if ( (IOAddr == 0x02) && (IOData == 0xff) ) TorchTubeActive = 1;

	switch (IOAddr) {
	case 1:
		// S bit controls write of status flags
		if (IOData & TubeS)
			R1Status|=(IOData & 0x3f);
		else
			R1Status&=~IOData;

		// Reset required?
		if (R1Status & TubeP)
			Reset65C02();
		if (R1Status & TubeT)
			ResetTube();

		// Update interrupt flags
		UpdateR1Interrupt();
		UpdateR3Interrupt();
		UpdateR4Interrupt();
		UpdateHostR4Interrupt();
		break;
	case 0:
		R1HPData = IOData;
		R1PStatus|=TubeDataAv;
		R1HStatus&=~TubeNotFull;
		UpdateR1Interrupt();
		break;

// Echo back to tube ?

	case 0x08 :
//		WriteTorchTubeFromHostSide(1, IOData);
		break;
		
	case 0x0c :
		if (IOData == 0xaa)
		{
			init_z80();
			Enable_Z80 = 1;
		}
		break;
		
	case 0x0e:
		break;
	}
}

unsigned char ReadTorchTubeFromParasiteSide(unsigned char IOAddr) 
{
	unsigned char TmpData;

	switch (IOAddr) {
	case 0:
		TmpData=R1PStatus | R1Status;
		break;
	case 1:
		TmpData = R1HPData;
		R1PStatus&=~TubeDataAv;
		R1HStatus|=TubeNotFull;
		UpdateR1Interrupt();
		break;
	}

//	fprintf(tlog, "ReadTorchTubeFromParasiteSide - Addr = %02x, Value = %02x\n", (int) IOAddr, (int) TmpData);

	if (DebugEnabled) {
		char info[200];
		sprintf(info, "Tube: Read from para, addr %X value %02X", (int)IOAddr, (int)TmpData);
		DebugDisplayTrace(DEBUG_TUBE, false, info);
	}

	return TmpData;
}

void WriteTorchTubeFromParasiteSide(unsigned char IOAddr,unsigned char IOData) 
{

//	fprintf(tlog, "WriteTorchTubeFromParasiteSide - Addr = %02x, Value = %02x\n", (int) IOAddr, (int) IOData);

	if (DebugEnabled) {
		char info[200];
		sprintf(info, "Tube: Write from para, addr %X value %02X", (int)IOAddr, (int)IOData);
		DebugDisplayTrace(DEBUG_TUBE, false, info);
	}

	switch (IOAddr) {
	case 1:
		R1PHData[0]=IOData;
		R1HStatus|=TubeDataAv;
		R1PStatus&=~TubeNotFull;
		break;
	}
}

/*-------------------------------------------------------------------*/
// Tube memory/io handling functions

unsigned char ReadTubeFromHostSide(unsigned char IOAddr) {
	unsigned char TmpData,TmpCntr;

	if (! (EnableTube ||
#ifdef M512COPRO_ENABLED
		   Tube186Enabled ||
#endif
		   AcornZ80 || ArmTube) ) 
		return(MachineType==3 ? 0xff : 0xfe); // return ff for master else return fe

	switch (IOAddr) {
	case 0:
		TmpData=R1HStatus | R1Status;
		break;
	case 1:
		TmpData=R1PHData[0];
		if (R1PHPtr>0) {
			for (TmpCntr=1;TmpCntr<24;TmpCntr++)
				R1PHData[TmpCntr-1]=R1PHData[TmpCntr]; // Shift FIFO Buffer
			R1PHPtr--; // Shift FIFO Pointer
			if (R1PHPtr == 0)
				R1HStatus&=~TubeDataAv;
			R1PStatus|=TubeNotFull;
		}
		break;
	case 2:
		TmpData=R2HStatus;
		break;
	case 3:
		TmpData=R2PHData;
		if (R2HStatus & TubeDataAv) {
			R2HStatus&=~TubeDataAv;
			R2PStatus|=TubeNotFull;
		}
		break;
	case 4:
		TmpData=R3HStatus;
		break;
	case 5:
		TmpData=R3PHData[0];
		if (R3PHPtr>0) {
			R3PHData[0]=R3PHData[1]; // Shift FIFO Buffer
			R3PHPtr--; // Shift FIFO Pointer
			if (R3PHPtr == 0) {
				R3HStatus&=~TubeDataAv;
				R3PStatus|=TubeNotFull;
			}
		}
		UpdateR3Interrupt();
		break;
	case 6:
		TmpData=R4HStatus;
		break;
	case 7:
		TmpData=R4PHData;
		if (R4HStatus & TubeDataAv) {
			R4HStatus&=~TubeDataAv;
			R4PStatus|=TubeNotFull;
		}
		UpdateHostR4Interrupt();
		break;
	}

	if (DebugEnabled) {
		char info[200];
		sprintf(info, "Tube: Read from host, addr %X value %02X", (int)IOAddr, (int)TmpData);
		DebugDisplayTrace(DEBUG_TUBE, true, info);
	}

	return TmpData;
}

void WriteTubeFromHostSide(unsigned char IOAddr,unsigned char IOData) {
	if (! (EnableTube ||
#ifdef M512COPRO_ENABLED
		   Tube186Enabled ||
#endif
		   AcornZ80 || ArmTube) ) 
		return;

	if (DebugEnabled) {
		char info[200];
		sprintf(info, "Tube: Write from host, addr %X value %02X", (int)IOAddr, (int)IOData);
		DebugDisplayTrace(DEBUG_TUBE, true, info);
	}

	switch (IOAddr) {
	case 0:
		// S bit controls write of status flags
		if (IOData & TubeS)
			R1Status|=(IOData & 0x3f);
		else
			R1Status&=~IOData;

		// Reset required?
		if (R1Status & TubeP)
			Reset65C02();
		if (R1Status & TubeT)
			ResetTube();

		// Update interrupt flags
		UpdateR1Interrupt();
		UpdateR3Interrupt();
		UpdateR4Interrupt();
		UpdateHostR4Interrupt();
		break;
	case 1:
		R1HPData=IOData;
		R1PStatus|=TubeDataAv;
		R1HStatus&=~TubeNotFull;
		UpdateR1Interrupt();
		break;
	case 3:
		R2HPData=IOData;
		R2PStatus|=TubeDataAv;
		R2HStatus&=~TubeNotFull;
		break;
	case 5:
		if (R1Status & TubeV) {
			if (R3HPPtr < 2)
				R3HPData[R3HPPtr++]=IOData;
			if (R3HPPtr == 2) {
				R3PStatus|=TubeDataAv;
				R3HStatus&=~TubeNotFull;
			}
		}
		else {
			R3HPPtr=0;
			R3HPData[R3HPPtr++]=IOData;
			R3PStatus|=TubeDataAv;
			R3HStatus&=~TubeNotFull;
		}
		UpdateR3Interrupt();
		break;
	case 7:
		R4HPData=IOData;
		R4PStatus|=TubeDataAv;
		R4HStatus&=~TubeNotFull;
		UpdateR4Interrupt();
		break;
	}
}

unsigned char ReadTubeFromParasiteSide(unsigned char IOAddr) {
	unsigned char TmpData;

	if (TorchTube) 
		return ReadTorchTubeFromHostSide(IOAddr);

	switch (IOAddr) {
	case 0:
		TmpData=R1PStatus | R1Status;
		break;
	case 1:
		TmpData=R1HPData;
		if (R1PStatus & TubeDataAv) {
			R1PStatus&=~TubeDataAv;
			R1HStatus|=TubeNotFull;
		}
		UpdateR1Interrupt();
		break;
	case 2:
		TmpData=R2PStatus;
		break;
	case 3:
		TmpData=R2HPData;
		if (R2PStatus & TubeDataAv) {
			R2PStatus&=~TubeDataAv;
			R2HStatus|=TubeNotFull;
		}
		break;
	case 4:
		TmpData=R3PStatus;
		// Tube Spec says top bit in R3PStatus has value 'N', which looks like it is
		// the same as the PNMI status (i.e. H->P data available OR P->H not full).
		if (R3PHPtr == 0)
			TmpData |= 128;
		break;
	case 5:
		TmpData=R3HPData[0];
		if (R3HPPtr>0) {
			R3HPData[0]=R3HPData[1]; // Shift FIFO Buffer
			R3HPPtr--; // Shift FIFO Pointer
			if (R3HPPtr == 0) {
				R3PStatus&=~TubeDataAv;
				R3HStatus|=TubeNotFull;
			}
		}
		UpdateR3Interrupt();
		break;
	case 6:
		TmpData=R4PStatus;
		break;
	case 7:
		TmpData=R4HPData;
		if (R4PStatus & TubeDataAv) {
			R4PStatus&=~TubeDataAv;
			R4HStatus|=TubeNotFull;
		}
		UpdateR4Interrupt();
		break;
	}

	if (DebugEnabled) {
		char info[200];
		sprintf(info, "Tube: Read from para, addr %X value %02X", (int)IOAddr, (int)TmpData);
		DebugDisplayTrace(DEBUG_TUBE, false, info);
	}

	return TmpData;
}

void WriteTubeFromParasiteSide(unsigned char IOAddr,unsigned char IOData)
{
	if (TorchTube) 
	{
		WriteTorchTubeFromParasiteSide(IOAddr, IOData);
		return;
	}

	if (DebugEnabled) {
		char info[200];
		sprintf(info, "Tube: Write from para, addr %X value %02X", (int)IOAddr, (int)IOData);
		DebugDisplayTrace(DEBUG_TUBE, false, info);
	}

	switch (IOAddr) {
	case 0:
		// Cannot write status flags from parasite
		break;
	case 1:
		if (R1PHPtr<24) {
			R1PHData[R1PHPtr++]=IOData;
			R1HStatus|=TubeDataAv;
			if (R1PHPtr==24)
				R1PStatus&=~TubeNotFull;
		}
		break;
	case 3:
		R2PHData=IOData;
		R2HStatus|=TubeDataAv;
		R2PStatus&=~TubeNotFull;
		break;
	case 5:
		if (R1Status & TubeV) {
			if (R3PHPtr < 2)
				R3PHData[R3PHPtr++]=IOData;
			if (R3PHPtr == 2) {
				R3HStatus|=TubeDataAv;
				R3PStatus&=~TubeNotFull;
			}
		}
		else {
			R3PHPtr=0;
			R3PHData[R3PHPtr++]=IOData;
			R3HStatus|=TubeDataAv;
			R3PStatus&=~TubeNotFull;
		}
		UpdateR3Interrupt();
		break;
	case 7:
		R4PHData=IOData;
		R4HStatus|=TubeDataAv;
		R4PStatus&=~TubeNotFull;
		UpdateHostR4Interrupt();
		break;
	}
}

/*----------------------------------------------------------------------------*/
void TubeWriteMem(unsigned int IOAddr,unsigned char IOData) {
	if (IOAddr>=0xff00 || IOAddr<0xfef8)
		TubeRam[IOAddr]=IOData;
	else
		WriteTubeFromParasiteSide(IOAddr-0xfef8,IOData);
}

unsigned char TubeReadMem(unsigned int IOAddr) {
	if (IOAddr>=0xff00 || IOAddr<0xfef8)
		return(TubeRam[IOAddr]);
	else
		return(ReadTubeFromParasiteSide(IOAddr-0xfef8));
}

/* Get a two byte address from the program counter, and then post inc the program counter */
#define GETTWOBYTEFROMPC(var) \
  var=TubeRam[TubeProgramCounter]; \
  var|=(TubeRam[TubeProgramCounter+1]<<8); \
  TubeProgramCounter+=2;

/*----------------------------------------------------------------------------*/
INLINE void Carried() {
	// Correct cycle count for indirection across page boundary
	if (((CurrentInstruction & 0xf)==0x1 ||
		 (CurrentInstruction & 0xf)==0x9 ||
		 (CurrentInstruction & 0xf)==0xd) &&
		(CurrentInstruction & 0xf0)!=0x90)
	{
		TubeCycles++;
	}
	else if (CurrentInstruction==0xBC ||
			 CurrentInstruction==0xBE)
	{
		TubeCycles++;
	}
}

/*----------------------------------------------------------------------------*/
INLINE int SignExtendByte(signed char in) {
  /*if (in & 0x80) return(in | 0xffffff00); else return(in); */
  /* I think this should sign extend by virtue of the casts - gcc does anyway - the code
  above will definitly do the trick */
  return((int)in);
} /* SignExtendByte */

/*----------------------------------------------------------------------------*/
/* Set the Z flag if 'in' is 0, and N if bit 7 is set - leave all other bits  */
/* untouched.                                                                 */
INLINE static void SetPSRZN(const unsigned char in) {
  PSR&=~(FlagZ | FlagN);
  PSR|=((in==0)<<1) | (in & 128);
}; /* SetPSRZN */

/*----------------------------------------------------------------------------*/
/* Note: n is 128 for true - not 1                                            */
INLINE static void SetPSR(int mask,int c,int z,int i,int d,int b, int v, int n) {
  PSR&=~mask;
  PSR|=c | (z<<1) | (i<<2) | (d<<3) | (b<<4) | (v<<6) | n;
} /* SetPSR */

/*----------------------------------------------------------------------------*/
/* NOTE!!!!! n is 128 or 0 - not 1 or 0                                       */
INLINE static void SetPSRCZN(int c,int z, int n) {
  PSR&=~(FlagC | FlagZ | FlagN);
  PSR|=c | (z<<1) | n;
} /* SetPSRCZN */

/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
INLINE static void Push(unsigned char ToPush) {
  TUBEWRITEMEM_DIRECT(0x100+StackReg,ToPush);
  StackReg--;
} /* Push */

/*----------------------------------------------------------------------------*/
INLINE static unsigned char Pop(void) {
  StackReg++;
  return(TubeRam[0x100+StackReg]);
} /* Pop */

/*----------------------------------------------------------------------------*/
INLINE static void PushWord(int16 topush) {
  Push((topush>>8) & 255);
  Push(topush & 255);
} /* PushWord */

/*----------------------------------------------------------------------------*/
INLINE static int16 PopWord() {
  int16 RetValue;

  RetValue=Pop();
  RetValue|=(Pop()<<8);
  return(RetValue);
} /* PopWord */

/*-------------------------------------------------------------------------*/
/* Relative addressing mode handler                                        */
INLINE static int16 RelAddrModeHandler_Data(void) {
  int EffectiveAddress;

  /* For branches - is this correct - i.e. is the program counter incremented
     at the correct time? */
  EffectiveAddress=SignExtendByte((signed char)TubeRam[TubeProgramCounter++]);
  EffectiveAddress+=TubeProgramCounter;

  return(EffectiveAddress);
} /* RelAddrModeHandler */

/*----------------------------------------------------------------------------*/
INLINE static void ADCInstrHandler(int16 operand) {
  /* NOTE! Not sure about C and V flags */
  int TmpResultV,TmpResultC;
  if (!GETDFLAG) {
    TmpResultC=Accumulator+operand+GETCFLAG;
    TmpResultV=(signed char)Accumulator+(signed char)operand+GETCFLAG;
    Accumulator=TmpResultC & 255;
    SetPSR(FlagC | FlagZ | FlagV | FlagN, (TmpResultC & 256)>0,Accumulator==0,0,0,0,((Accumulator & 128)>0) ^ (TmpResultV<0),(Accumulator & 128));
  } else {
    int ZFlag=0,NFlag=0,CFlag=0,VFlag=0;
    int TmpResult,TmpCarry=0;
    int ln,hn;

    /* Z flag determined from 2's compl result, not BCD result! */
    TmpResult=Accumulator+operand+GETCFLAG;
    ZFlag=((TmpResult & 0xff)==0);

    ln=(Accumulator & 0xf)+(operand & 0xf)+GETCFLAG;
    if (ln>9) {
      ln += 6;
      ln &= 0xf;
      TmpCarry=0x10;
    }
    hn=(Accumulator & 0xf0)+(operand & 0xf0)+TmpCarry;
    /* N and V flags are determined before high nibble is adjusted.
       NOTE: V is not always correct */
    NFlag=hn & 128;
    VFlag=(hn ^ Accumulator) & 128 && !((Accumulator ^ operand) & 128);
    if (hn>0x90) {
      hn += 0x60;
      hn &= 0xf0;
      CFlag=1;
    }
    Accumulator=hn|ln;
	ZFlag=(Accumulator==0);
	NFlag=(Accumulator&128);
    SetPSR(FlagC | FlagZ | FlagV | FlagN,CFlag,ZFlag,0,0,0,VFlag,NFlag);
  }
} /* ADCInstrHandler */

/*----------------------------------------------------------------------------*/
INLINE static void ANDInstrHandler(int16 operand) {
  Accumulator=Accumulator & operand;
  PSR&=~(FlagZ | FlagN);
  PSR|=((Accumulator==0)<<1) | (Accumulator & 128);
} /* ANDInstrHandler */

INLINE static void ASLInstrHandler(int16 address) {
  unsigned char oldVal,newVal;
  oldVal=TUBEREADMEM_FAST(address);
  newVal=(((unsigned int)oldVal)<<1) & 254;
  TUBEWRITEMEM_FAST(address,newVal);
  SetPSRCZN((oldVal & 128)>0, newVal==0,newVal & 128);
} /* ASLInstrHandler */

INLINE static void TRBInstrHandler(int16 address) {
	unsigned char oldVal,newVal;
	oldVal=TUBEREADMEM_FAST(address);
	newVal=(Accumulator ^ 255) & oldVal;
    TUBEWRITEMEM_FAST(address,newVal);
    PSR&=253;
	PSR|=((Accumulator & oldVal)==0) ? 2 : 0;
} // TRBInstrHandler

INLINE static void TSBInstrHandler(int16 address) {
	unsigned char oldVal,newVal;
	oldVal=TUBEREADMEM_FAST(address);
	newVal=Accumulator | oldVal;
    TUBEWRITEMEM_FAST(address,newVal);
    PSR&=253;
	PSR|=((Accumulator & oldVal)==0) ? 2 : 0;
} // TSBInstrHandler

INLINE static void ASLInstrHandler_Acc(void) {
  unsigned char oldVal,newVal;
  /* Accumulator */
  oldVal=Accumulator;
  Accumulator=newVal=(((unsigned int)Accumulator)<<1) & 254;
  SetPSRCZN((oldVal & 128)>0, newVal==0,newVal & 128);
} /* ASLInstrHandler_Acc */

INLINE static void BCCInstrHandler(void) {
  if (!GETCFLAG) {
    TubeProgramCounter=RelAddrModeHandler_Data();
    Branched=1;
  } else TubeProgramCounter++;
} /* BCCInstrHandler */

INLINE static void BCSInstrHandler(void) {
  if (GETCFLAG) {
    TubeProgramCounter=RelAddrModeHandler_Data();
    Branched=1;
  } else TubeProgramCounter++;
} /* BCSInstrHandler */

INLINE static void BEQInstrHandler(void) {
  if (GETZFLAG) {
    TubeProgramCounter=RelAddrModeHandler_Data();
    Branched=1;
  } else TubeProgramCounter++;
} /* BEQInstrHandler */

INLINE static void BITInstrHandler(int16 operand) {
  PSR&=~(FlagZ | FlagN | FlagV);
  /* z if result 0, and NV to top bits of operand */
  PSR|=(((Accumulator & operand)==0)<<1) | (operand & 192);
} /* BITInstrHandler */

INLINE static void BMIInstrHandler(void) {
  if (GETNFLAG) {
    TubeProgramCounter=RelAddrModeHandler_Data();
    Branched=1;
  } else TubeProgramCounter++;
} /* BMIInstrHandler */

INLINE static void BNEInstrHandler(void) {
  if (!GETZFLAG) {
    TubeProgramCounter=RelAddrModeHandler_Data();
    Branched=1;
  } else TubeProgramCounter++;
} /* BNEInstrHandler */

INLINE static void BPLInstrHandler(void) {
  if (!GETNFLAG) {
    TubeProgramCounter=RelAddrModeHandler_Data();
    Branched=1;
  } else TubeProgramCounter++;
}; /* BPLInstrHandler */

INLINE static void BRKInstrHandler(void) {
  PushWord(TubeProgramCounter+1);
  SetPSR(FlagB,0,0,0,0,1,0,0); /* Set B before pushing */
  Push(PSR);
  SetPSR(FlagI,0,0,1,0,0,0,0); /* Set I after pushing - see Birnbaum */
  TubeProgramCounter=TubeReadMem(0xfffe) | (TubeReadMem(0xffff)<<8);
} /* BRKInstrHandler */

INLINE static void BVCInstrHandler(void) {
  if (!GETVFLAG) {
    TubeProgramCounter=RelAddrModeHandler_Data();
    Branched=1;
  } else TubeProgramCounter++;
} /* BVCInstrHandler */

INLINE static void BVSInstrHandler(void) {
  if (GETVFLAG) {
    TubeProgramCounter=RelAddrModeHandler_Data();
    Branched=1;
  } else TubeProgramCounter++;
} /* BVSInstrHandler */

INLINE static void BRAInstrHandler(void) {
    TubeProgramCounter=RelAddrModeHandler_Data();
    Branched=1;
} /* BRAnstrHandler */

INLINE static void CMPInstrHandler(int16 operand) {
  /* NOTE! Should we consult D flag ? */
  unsigned char result=Accumulator-operand;
  unsigned char CFlag;
  CFlag=0; if (Accumulator>=operand) CFlag=FlagC;
  SetPSRCZN(CFlag,Accumulator==operand,result & 128);
} /* CMPInstrHandler */

INLINE static void CPXInstrHandler(int16 operand) {
  unsigned char result=(XReg-operand);
  SetPSRCZN(XReg>=operand,XReg==operand,result & 128);
} /* CPXInstrHandler */

INLINE static void CPYInstrHandler(int16 operand) {
  unsigned char result=(YReg-operand);
  SetPSRCZN(YReg>=operand,YReg==operand,result & 128);
} /* CPYInstrHandler */

INLINE static void DECInstrHandler(int16 address) {
  unsigned char val;

  val=TUBEREADMEM_FAST(address);

  val=(val-1);

  TUBEWRITEMEM_FAST(address,val);
  SetPSRZN(val);
} /* DECInstrHandler */

INLINE static void DEXInstrHandler(void) {
  XReg=(XReg-1) & 255;
  SetPSRZN(XReg);
} /* DEXInstrHandler */

INLINE static void DEAInstrHandler(void) {
  Accumulator=(Accumulator-1) & 255;
  SetPSRZN(Accumulator);
} /* DEAInstrHandler */

INLINE static void EORInstrHandler(int16 operand) {
  Accumulator^=operand;
  SetPSRZN(Accumulator);
} /* EORInstrHandler */

INLINE static void INCInstrHandler(int16 address) {
  unsigned char val;

  val=TUBEREADMEM_FAST(address);

  val=(val+1) & 255;

  TUBEWRITEMEM_FAST(address,val);
  SetPSRZN(val);
} /* INCInstrHandler */

INLINE static void INXInstrHandler(void) {
  XReg+=1;
  XReg&=255;
  SetPSRZN(XReg);
} /* INXInstrHandler */

INLINE static void INAInstrHandler(void) {
  Accumulator+=1;
  Accumulator&=255;
  SetPSRZN(Accumulator);
} /* INAInstrHandler */

INLINE static void JSRInstrHandler(int16 address) {
  PushWord(TubeProgramCounter-1);
  TubeProgramCounter=address;
} /* JSRInstrHandler */

INLINE static void LDAInstrHandler(int16 operand) {
  Accumulator=operand;
  SetPSRZN(Accumulator);
} /* LDAInstrHandler */

INLINE static void LDXInstrHandler(int16 operand) {
  XReg=operand;
  SetPSRZN(XReg);
} /* LDXInstrHandler */

INLINE static void LDYInstrHandler(int16 operand) {
  YReg=operand;
  SetPSRZN(YReg);
} /* LDYInstrHandler */

INLINE static void LSRInstrHandler(int16 address) {
  unsigned char oldVal,newVal;
  oldVal=TUBEREADMEM_FAST(address);
  newVal=(((unsigned int)oldVal)>>1) & 127;
  TUBEWRITEMEM_FAST(address,newVal);
  SetPSRCZN((oldVal & 1)>0, newVal==0,0);
} /* LSRInstrHandler */

INLINE static void LSRInstrHandler_Acc(void) {
  unsigned char oldVal,newVal;
  /* Accumulator */
  oldVal=Accumulator;
  Accumulator=newVal=(((unsigned int)Accumulator)>>1) & 127;
  SetPSRCZN((oldVal & 1)>0, newVal==0,0);
} /* LSRInstrHandler_Acc */

INLINE static void ORAInstrHandler(int16 operand) {
  Accumulator=Accumulator | operand;
  SetPSRZN(Accumulator);
} /* ORAInstrHandler */

INLINE static void ROLInstrHandler(int16 address) {
  unsigned char oldVal,newVal;

  oldVal=TUBEREADMEM_FAST(address);
  newVal=((unsigned int)oldVal<<1) & 254;
  newVal+=GETCFLAG;
  TUBEWRITEMEM_FAST(address,newVal);
  SetPSRCZN((oldVal & 128)>0,newVal==0,newVal & 128);
} /* ROLInstrHandler */

INLINE static void ROLInstrHandler_Acc(void) {
  unsigned char oldVal,newVal;

  oldVal=Accumulator;
  newVal=((unsigned int)oldVal<<1) & 254;
  newVal+=GETCFLAG;
  Accumulator=newVal;
  SetPSRCZN((oldVal & 128)>0,newVal==0,newVal & 128);
} /* ROLInstrHandler_Acc */

INLINE static void RORInstrHandler(int16 address) {
  unsigned char oldVal,newVal;

  oldVal=TUBEREADMEM_FAST(address);
  newVal=((unsigned int)oldVal>>1) & 127;
  newVal+=GETCFLAG*128;
  TUBEWRITEMEM_FAST(address,newVal);
  SetPSRCZN(oldVal & 1,newVal==0,newVal & 128);
} /* RORInstrHandler */

INLINE static void RORInstrHandler_Acc(void) {
  unsigned char oldVal,newVal;

  oldVal=Accumulator;
  newVal=((unsigned int)oldVal>>1) & 127;
  newVal+=GETCFLAG*128;
  Accumulator=newVal;
  SetPSRCZN(oldVal & 1,newVal==0,newVal & 128);
} /* RORInstrHandler_Acc */

INLINE static void SBCInstrHandler(int16 operand) {
  /* NOTE! Not sure about C and V flags */
  int TmpResultV,TmpResultC;
  unsigned char nhn,nln;
  if (!GETDFLAG) {
    TmpResultV=(signed char)Accumulator-(signed char)operand-(1-GETCFLAG);
    TmpResultC=Accumulator-operand-(1-GETCFLAG);
    Accumulator=TmpResultC & 255;
    SetPSR(FlagC | FlagZ | FlagV | FlagN, TmpResultC>=0,Accumulator==0,0,0,0,
      ((Accumulator & 128)>0) ^ ((TmpResultV & 256)!=0),(Accumulator & 128));
  } else {
    int ZFlag=0,NFlag=0,CFlag=1,VFlag=0;
    int TmpResult,TmpCarry=0;
    int ln,hn,oln,ohn;
	nhn=(Accumulator>>4)&15; nln=Accumulator & 15;

    /* Z flag determined from 2's compl result, not BCD result! */
    TmpResult=Accumulator-operand-(1-GETCFLAG);
    ZFlag=((TmpResult & 0xff)==0);

	ohn=operand & 0xf0; oln = operand & 0xf;
	if ((oln>9) && ((Accumulator&15)<10)) { oln-=10; ohn+=0x10; } 
	// promote the lower nibble to the next ten, and increase the higher nibble
    ln=(Accumulator & 0xf)-oln-(1-GETCFLAG);
    if (ln<0) {
	  if ((Accumulator & 15)<10) ln-=6;
      ln&=0xf;
      TmpCarry=0x10;
    }
    hn=(Accumulator & 0xf0)-ohn-TmpCarry;
    /* N and V flags are determined before high nibble is adjusted.
       NOTE: V is not always correct */
    NFlag=hn & 128;
	TmpResultV=(signed char)Accumulator-(signed char)operand-(1-GETCFLAG);
	if ((TmpResultV<-128)||(TmpResultV>127)) VFlag=1; else VFlag=0;
    if (hn<0) {
      hn-=0x60;
      hn&=0xf0;
      CFlag=0;
    }
    Accumulator=hn|ln;
	if (Accumulator==0) ZFlag=1;
	NFlag=(hn &128);
	CFlag=(TmpResult&256)==0;
    SetPSR(FlagC | FlagZ | FlagV | FlagN,CFlag,ZFlag,0,0,0,VFlag,NFlag);
  }
} /* SBCInstrHandler */

INLINE static void STXInstrHandler(int16 address) {
  TUBEWRITEMEM_FAST(address,XReg);
} /* STXInstrHandler */

INLINE static void STYInstrHandler(int16 address) {
  TUBEWRITEMEM_FAST(address,YReg);
} /* STYInstrHandler */

INLINE static void BadInstrHandler(int opcode) {
	if (!IgnoreIllegalInstructions)
	{
#ifdef WIN32
		char errstr[250];
		sprintf(errstr,"Unsupported 65C02 instruction 0x%02X at 0x%04X\n"
			"  OK - instruction will be skipped\n"
			"  Cancel - dump memory and exit",opcode,TubeProgramCounter-1);
		if (MessageBox(GETHWND,errstr,WindowTitle,MB_OKCANCEL|MB_ICONERROR) == IDCANCEL)
		{
			exit(0);
		}
#else
		fprintf(stderr,"Bad instruction handler called:\n");
		fprintf(stderr,"Dumping main memory\n");
		beebmem_dumpstate();
		// abort();
#endif
	}

	/* Do not know what the instruction does but can guess if it is 1,2 or 3 bytes */
	switch (opcode & 0xf)
	{
	/* One byte instructions */
	case 0xa:
		break;

	/* Two byte instructions */
	case 0x0:
	case 0x2:  /* Inst 0xf2 causes the 6502 to hang! Try it on your BBC Micro */
	case 0x3:
	case 0x4:
	case 0x7:
	case 0x9:
	case 0xb:
		TubeProgramCounter++;
		break;

	/* Three byte instructions */
	case 0xc:
	case 0xe:
	case 0xf:
		TubeProgramCounter+=2;
		break;
	}
} /* BadInstrHandler */

/*-------------------------------------------------------------------------*/
/* Absolute  addressing mode handler                                       */
INLINE static int16 AbsAddrModeHandler_Data(void) {
  int FullAddress;

  /* Get the address from after the instruction */
  
  GETTWOBYTEFROMPC(FullAddress)

  /* And then read it */
  return(TUBEREADMEM_FAST(FullAddress));
} /* AbsAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Absolute  addressing mode handler                                       */
INLINE static int16 AbsAddrModeHandler_Address(void) {
  int FullAddress;

  /* Get the address from after the instruction */
  GETTWOBYTEFROMPC(FullAddress)

  /* And then read it */
  return(FullAddress);
} /* AbsAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Zero page addressing mode handler                                       */
INLINE static int16 ZeroPgAddrModeHandler_Address(void) {
  return(TubeRam[TubeProgramCounter++]);
} /* ZeroPgAddrModeHandler_Address */

/*-------------------------------------------------------------------------*/
/* Indexed with X preinc addressing mode handler                           */
INLINE static int16 IndXAddrModeHandler_Data(void) {
  unsigned char ZeroPageAddress;
  int EffectiveAddress;

  ZeroPageAddress=(TubeRam[TubeProgramCounter++]+XReg) & 255;

  EffectiveAddress=TubeRam[ZeroPageAddress] | (TubeRam[ZeroPageAddress+1]<<8);
  return(TUBEREADMEM_FAST(EffectiveAddress));
} /* IndXAddrModeHandler_Data */

/*-------------------------------------------------------------------------*/
/* Indexed with X preinc addressing mode handler                           */
INLINE static int16 IndXAddrModeHandler_Address(void) {
  unsigned char ZeroPageAddress;
  int EffectiveAddress;

  ZeroPageAddress=(TubeRam[TubeProgramCounter++]+XReg) & 255;

  EffectiveAddress=TubeRam[ZeroPageAddress] | (TubeRam[ZeroPageAddress+1]<<8);
  return(EffectiveAddress);
} /* IndXAddrModeHandler_Address */

/*-------------------------------------------------------------------------*/
/* Indexed with Y postinc addressing mode handler                          */
INLINE static int16 IndYAddrModeHandler_Data(void) {
  int EffectiveAddress;
  unsigned char ZPAddr=TubeRam[TubeProgramCounter++];
  EffectiveAddress=TubeRam[ZPAddr]+YReg;
  if (EffectiveAddress>0xff) Carried();
  EffectiveAddress+=(TubeRam[ZPAddr+1]<<8);

  return(TUBEREADMEM_FAST(EffectiveAddress));
} /* IndYAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Indexed with Y postinc addressing mode handler                          */
INLINE static int16 IndYAddrModeHandler_Address(void) {
  int EffectiveAddress;
  unsigned char ZPAddr=TubeRam[TubeProgramCounter++];
  EffectiveAddress=TubeRam[ZPAddr]+YReg;
  if (EffectiveAddress>0xff) Carried();
  EffectiveAddress+=(TubeRam[ZPAddr+1]<<8);

  return(EffectiveAddress);
} /* IndYAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Zero page wih X offset addressing mode handler                          */
INLINE static int16 ZeroPgXAddrModeHandler_Data(void) {
  int EffectiveAddress;
  EffectiveAddress=(TubeRam[TubeProgramCounter++]+XReg) & 255;
  return(TubeRam[EffectiveAddress]);
} /* ZeroPgXAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Zero page wih X offset addressing mode handler                          */
INLINE static int16 ZeroPgXAddrModeHandler_Address(void) {
  int EffectiveAddress;
  EffectiveAddress=(TubeRam[TubeProgramCounter++]+XReg) & 255;
  return(EffectiveAddress);
} /* ZeroPgXAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Absolute with X offset addressing mode handler                          */
INLINE static int16 AbsXAddrModeHandler_Data(void) {
  int EffectiveAddress;
  GETTWOBYTEFROMPC(EffectiveAddress);
  if ((EffectiveAddress & 0xff00)!=((EffectiveAddress+XReg) & 0xff00)) Carried();
  EffectiveAddress+=XReg;
  EffectiveAddress&=0xffff;

  return(TUBEREADMEM_FAST(EffectiveAddress));
} /* AbsXAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Absolute with X offset addressing mode handler                          */
INLINE static int16 AbsXAddrModeHandler_Address(void) {
  int EffectiveAddress;
  GETTWOBYTEFROMPC(EffectiveAddress)
  if ((EffectiveAddress & 0xff00)!=((EffectiveAddress+XReg) & 0xff00)) Carried();
  EffectiveAddress+=XReg;
  EffectiveAddress&=0xffff;

  return(EffectiveAddress);
} /* AbsXAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Absolute with Y offset addressing mode handler                          */
INLINE static int16 AbsYAddrModeHandler_Data(void) {
  int EffectiveAddress;
  GETTWOBYTEFROMPC(EffectiveAddress);
  if ((EffectiveAddress & 0xff00)!=((EffectiveAddress+YReg) & 0xff00)) Carried();
  EffectiveAddress+=YReg;
  EffectiveAddress&=0xffff;

  return(TUBEREADMEM_FAST(EffectiveAddress));
} /* AbsYAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Absolute with Y offset addressing mode handler                          */
INLINE static int16 AbsYAddrModeHandler_Address(void) {
  int EffectiveAddress;
  GETTWOBYTEFROMPC(EffectiveAddress)
  if ((EffectiveAddress & 0xff00)!=((EffectiveAddress+YReg) & 0xff00)) Carried();
  EffectiveAddress+=YReg;
  EffectiveAddress&=0xffff;

  return(EffectiveAddress);
} /* AbsYAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Indirect addressing mode handler                                        */
INLINE static int16 IndAddrModeHandler_Address(void) {
  /* For jump indirect only */
  int VectorLocation;
  int EffectiveAddress;

  GETTWOBYTEFROMPC(VectorLocation)

  /* Ok kiddies, deliberate bug time.
  According to my BBC Master Reference Manual Part 2
  the 6502 has a bug concerning this addressing mode and VectorLocation==xxFF
  so, we're going to emulate that bug -- Richard Gellman */
  if ((VectorLocation & 0xff)!=0xff || TubeMachineType==3) {
   EffectiveAddress=TUBEREADMEM_FAST(VectorLocation);
   EffectiveAddress|=TUBEREADMEM_FAST(VectorLocation+1) << 8; }
  else {
   EffectiveAddress=TUBEREADMEM_FAST(VectorLocation);
   EffectiveAddress|=TUBEREADMEM_FAST(VectorLocation-255) << 8;
  }
  return(EffectiveAddress);
} /* IndAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Zero page Indirect addressing mode handler                                        */
INLINE static int16 ZPIndAddrModeHandler_Address(void) {
  int VectorLocation;
  int EffectiveAddress;

  VectorLocation=TubeRam[TubeProgramCounter++];
  EffectiveAddress=TubeRam[VectorLocation]+(TubeRam[VectorLocation+1]<<8);

   // EffectiveAddress|=TUBEREADMEM_FAST(VectorLocation+1) << 8; }
  return(EffectiveAddress);
} /* ZPIndAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Zero page Indirect addressing mode handler                                        */
INLINE static int16 ZPIndAddrModeHandler_Data(void) {
  int VectorLocation;
  int EffectiveAddress;

  VectorLocation=TubeRam[TubeProgramCounter++];
  EffectiveAddress=TubeRam[VectorLocation]+(TubeRam[VectorLocation+1]<<8);

   // EffectiveAddress|=TUBEREADMEM_FAST(VectorLocation+1) << 8; }
  return(TubeRam[EffectiveAddress]);
} /* ZPIndAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Pre-indexed absolute Indirect addressing mode handler                                        */
INLINE static int16 IndAddrXModeHandler_Address(void) {
  /* For jump indirect only */
  int VectorLocation;
  int EffectiveAddress;

  GETTWOBYTEFROMPC(VectorLocation)
  EffectiveAddress=TUBEREADMEM_FAST(VectorLocation+XReg);
  EffectiveAddress|=TUBEREADMEM_FAST(VectorLocation+1+XReg) << 8; 

   // EffectiveAddress|=TUBEREADMEM_FAST(VectorLocation+1) << 8; }
  return(EffectiveAddress);
} /* ZPIndAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Zero page with Y offset addressing mode handler                         */
INLINE static int16 ZeroPgYAddrModeHandler_Data(void) {
  int EffectiveAddress;
  EffectiveAddress=(TubeRam[TubeProgramCounter++]+YReg) & 255;
  return(TubeRam[EffectiveAddress]);
} /* ZeroPgYAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Zero page with Y offset addressing mode handler                         */
INLINE static int16 ZeroPgYAddrModeHandler_Address(void) {
  int EffectiveAddress;
  EffectiveAddress=(TubeRam[TubeProgramCounter++]+YReg) & 255;
  return(EffectiveAddress);
} /* ZeroPgYAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Reset processor */
void Reset65C02(void) {
  FILE *TubeRom;
  char TRN[256];
  char *TubeRomName=TRN;
  Accumulator=XReg=YReg=0; /* For consistancy of execution */
  StackReg=0xff; /* Initial value ? */
  PSR=FlagI; /* Interrupts off for starters */

  TubeintStatus=0;
  TubeNMIStatus=0;
  NMILock=0;

  //The fun part, the tube OS is copied from ROM to tube RAM before the processor starts processing
  //This makes the OS "ROM" writable in effect, but must be restored on each reset.
  strcpy(TubeRomName,RomPath);
  strcat(TubeRomName,"beebfile/6502Tube.rom");
  TubeRom=fopen(TubeRomName,"rb");
  if (TubeRom!=NULL) {
	  fread(TubeRam+0xf800,1,2048,TubeRom);
	  fclose(TubeRom);
  }

  TubeProgramCounter=TubeReadMem(0xfffc) | (TubeReadMem(0xfffd)<<8);
  TotalTubeCycles=TotalCycles/2*3;
}

/* Reset Tube */
void ResetTube(void) {
  memset(R1PHData,0,sizeof(R1PHData));
  R1PHPtr=0;
  R1HStatus=TubeNotFull;
  R1HPData=0;
  R1PStatus=TubeNotFull;

  R2PHData=0;
  R2HStatus=TubeNotFull;
  R2HPData=0;
  R2PStatus=TubeNotFull;

  R3PHData[0]=0;
  R3PHData[1]=0;
  R3PHPtr=1; // To prevent NMI on reset
  R3HStatus=TubeDataAv | TubeNotFull;
  R3HPData[0]=0;
  R3HPData[1]=0;
  R3HPPtr=0;
  R3PStatus=TubeNotFull;

  R4PHData=0;
  R4HStatus=TubeNotFull;
  R4HPData=0;
  R4PStatus=TubeNotFull;

  TubeintStatus=0;
  TubeNMIStatus=0;
}

/* Initialise 6502core */
void Init65C02core(void) {
  Reset65C02();
  R1Status=0;
  ResetTube();
}

#include "via.h"

/*-------------------------------------------------------------------------*/
void DoTubeInterrupt(void) {
  PushWord(TubeProgramCounter);
  Push(PSR & ~FlagB);
  TubeProgramCounter=TubeReadMem(0xfffe) | (TubeReadMem(0xffff)<<8);
  SetPSR(FlagI,0,0,1,0,0,0,0);
  IRQCycles=7;
} /* DoInterrupt */

/*-------------------------------------------------------------------------*/
void DoTubeNMI(void) {
  /*cerr << "Doing NMI\n"; */
  NMILock=1;
  PushWord(TubeProgramCounter);
  Push(PSR);
  TubeProgramCounter=TubeReadMem(0xfffa) | (TubeReadMem(0xfffb)<<8);
  SetPSR(FlagI,0,0,1,0,0,0,0); /* Normal interrupts should be disabled during NMI ? */
  IRQCycles=7;
} /* DoNMI */

/*-------------------------------------------------------------------------*/
/* Execute one 6502 instruction, move program counter on                   */
void Exec65C02Instruction(void) {
  static int tmpaddr;
  static int OldTubeNMIStatus;
  int OldPC;

  // Output debug info
  if (DebugEnabled)
    DebugDisassembler(TubeProgramCounter,PreTPC,Accumulator,XReg,YReg,PSR,StackReg,false);

  // For the Master, check Shadow Ram Presence
  // Note, this has to be done BEFORE reading an instruction due to Bit E and the PC
  /* Read an instruction and post inc program couter */
  OldPC=TubeProgramCounter;
  PreTPC=TubeProgramCounter;
  CurrentInstruction=TubeRam[TubeProgramCounter++];
  // cout << "Fetch at " << hex << (TubeProgramCounter-1) << " giving 0x" << CurrentInstruction << dec << "\n"; 
  TubeCycles=TubeCyclesTable[CurrentInstruction]; 
  /*Stats[CurrentInstruction]++; */
  Branched=0;
  switch (CurrentInstruction) {
    case 0x00:
      BRKInstrHandler();
      break;
    case 0x01:
      ORAInstrHandler(IndXAddrModeHandler_Data());
      break;
	case 0x04:
	  if (TubeMachineType==3) TSBInstrHandler(ZeroPgAddrModeHandler_Address()); else TubeProgramCounter+=1;
	  break;
    case 0x05:
      ORAInstrHandler(TubeRam[TubeRam[TubeProgramCounter++]]/*zp */);
      break;
    case 0x06:
      ASLInstrHandler(ZeroPgAddrModeHandler_Address());
      break;
    case 0x08:
      Push(PSR|48); /* PHP */
      break;
    case 0x09:
      ORAInstrHandler(TubeRam[TubeProgramCounter++]); /* immediate */
      break;
    case 0x0a:
      ASLInstrHandler_Acc();
      break;
	case 0x0c:
	  if (TubeMachineType==3) TSBInstrHandler(AbsAddrModeHandler_Address()); else TubeProgramCounter+=2;
	  break;
    case 0x0d:
      ORAInstrHandler(AbsAddrModeHandler_Data());
      break;
    case 0x0e:
      ASLInstrHandler(AbsAddrModeHandler_Address());
      break;
    case 0x10:
      BPLInstrHandler();
      break;
    case 0x30:
      BMIInstrHandler();
      break;
    case 0x50:
      BVCInstrHandler();
      break;
    case 0x70:
      BVSInstrHandler();
      break;
    case 0x80:
      BRAInstrHandler();
      break;
    case 0x90:
      BCCInstrHandler();
      break;
    case 0xb0:
      BCSInstrHandler();
      break;
    case 0xd0:
      BNEInstrHandler();
      break;
    case 0xf0:
      BEQInstrHandler();
      break;
    case 0x11:
      ORAInstrHandler(IndYAddrModeHandler_Data());
      break;
    case 0x12:
      if (TubeMachineType==3) ORAInstrHandler(ZPIndAddrModeHandler_Data());
      break;
	case 0x14:
	  if (TubeMachineType==3) TRBInstrHandler(ZeroPgAddrModeHandler_Address()); else TubeProgramCounter+=1;
	  break;
    case 0x15:
      ORAInstrHandler(ZeroPgXAddrModeHandler_Data());
      break;
    case 0x16:
      ASLInstrHandler(ZeroPgXAddrModeHandler_Address());
      break;
    case 0x18:
      PSR&=255-FlagC; /* CLC */
      break;
    case 0x19:
      ORAInstrHandler(AbsYAddrModeHandler_Data());
      break;
    case 0x1a:
      if (TubeMachineType==3) INAInstrHandler();
      break;
	case 0x1c:
	  if (TubeMachineType==3) TRBInstrHandler(AbsAddrModeHandler_Address()); else TubeProgramCounter+=2;
	  break;
    case 0x1d:
      ORAInstrHandler(AbsXAddrModeHandler_Data());
      break;
    case 0x1e:
      ASLInstrHandler(AbsXAddrModeHandler_Address());
      break;
    case 0x20:
      JSRInstrHandler(AbsAddrModeHandler_Address());
      break;
    case 0x21:
      ANDInstrHandler(IndXAddrModeHandler_Data());
      break;
    case 0x24:
      BITInstrHandler(TubeRam[TubeRam[TubeProgramCounter++]]/*zp */);
      break;
    case 0x25:
      ANDInstrHandler(TubeRam[TubeRam[TubeProgramCounter++]]/*zp */);
      break;
    case 0x26:
      ROLInstrHandler(ZeroPgAddrModeHandler_Address());
      break;
    case 0x28:
      PSR=Pop(); /* PLP */
      break;
    case 0x29:
      ANDInstrHandler(TubeRam[TubeProgramCounter++]); /* immediate */
      break;
    case 0x2a:
      ROLInstrHandler_Acc();
      break;
    case 0x2c:
      BITInstrHandler(AbsAddrModeHandler_Data());
      break;
    case 0x2d:
      ANDInstrHandler(AbsAddrModeHandler_Data());
      break;
    case 0x2e:
      ROLInstrHandler(AbsAddrModeHandler_Address());
      break;
    case 0x31:
      ANDInstrHandler(IndYAddrModeHandler_Data());
      break;
    case 0x32:
      if (TubeMachineType==3) ANDInstrHandler(ZPIndAddrModeHandler_Data());
      break;
    case 0x34: /* BIT Absolute,X */
      if (TubeMachineType==3) BITInstrHandler(ZeroPgXAddrModeHandler_Data()); else TubeProgramCounter+=1;
      break;
    case 0x35:
      ANDInstrHandler(ZeroPgXAddrModeHandler_Data());
      break;
    case 0x36:
      ROLInstrHandler(ZeroPgXAddrModeHandler_Address());
      break;
    case 0x38:
      PSR|=FlagC; /* SEC */
      break;
    case 0x39:
      ANDInstrHandler(AbsYAddrModeHandler_Data());
      break;
    case 0x3a:
      if (TubeMachineType==3) DEAInstrHandler();
      break;
    case 0x3c: /* BIT Absolute,X */
      if (TubeMachineType==3) BITInstrHandler(AbsXAddrModeHandler_Data()); else TubeProgramCounter+=2;
      break;
    case 0x3d:
      ANDInstrHandler(AbsXAddrModeHandler_Data());
      break;
    case 0x3e:
      ROLInstrHandler(AbsXAddrModeHandler_Address());
      break;
    case 0x40:
      PSR=Pop(); /* RTI */
      TubeProgramCounter=PopWord();
      NMILock=0;
      break;
    case 0x41:
      EORInstrHandler(IndXAddrModeHandler_Data());
      break;
    case 0x45:
      EORInstrHandler(TubeRam[TubeRam[TubeProgramCounter++]]/*zp */);
      break;
    case 0x46:
      LSRInstrHandler(ZeroPgAddrModeHandler_Address());
      break;
    case 0x48:
      Push(Accumulator); /* PHA */
      break;
    case 0x49:
      EORInstrHandler(TubeRam[TubeProgramCounter++]); /* immediate */
      break;
    case 0x4a:
      LSRInstrHandler_Acc();
      break;
    case 0x4c:
      TubeProgramCounter=AbsAddrModeHandler_Address(); /* JMP */
      break;
    case 0x4d:
      EORInstrHandler(AbsAddrModeHandler_Data());
      break;
    case 0x4e:
      LSRInstrHandler(AbsAddrModeHandler_Address());
      break;
    case 0x51:
      EORInstrHandler(IndYAddrModeHandler_Data());
      break;
    case 0x52:
      if (TubeMachineType==3) EORInstrHandler(ZPIndAddrModeHandler_Data());
      break;
    case 0x55:
      EORInstrHandler(ZeroPgXAddrModeHandler_Data());
      break;
    case 0x56:
      LSRInstrHandler(ZeroPgXAddrModeHandler_Address());
      break;
    case 0x58:
      PSR&=255-FlagI; /* CLI */
      break;
    case 0x59:
      EORInstrHandler(AbsYAddrModeHandler_Data());
      break;
    case 0x5a:
      if (TubeMachineType==3) Push(YReg); /* PHY */
      break;
    case 0x5d:
      EORInstrHandler(AbsXAddrModeHandler_Data());
      break;
    case 0x5e:
      LSRInstrHandler(AbsXAddrModeHandler_Address());
      break;
    case 0x60:
      TubeProgramCounter=PopWord()+1; /* RTS */
      break;
    case 0x61:
      ADCInstrHandler(IndXAddrModeHandler_Data());
      break;
    case 0x64:
      if (TubeMachineType==3) TUBEWRITEMEM_DIRECT(ZeroPgAddrModeHandler_Address(),0); /* STZ Zero Page */
      break;
    case 0x65:
      ADCInstrHandler(TubeRam[TubeRam[TubeProgramCounter++]]/*zp */);
      break;
    case 0x66:
      RORInstrHandler(ZeroPgAddrModeHandler_Address());
      break;
    case 0x68:
      Accumulator=Pop(); /* PLA */
      PSR&=~(FlagZ | FlagN);
      PSR|=((Accumulator==0)<<1) | (Accumulator & 128);
      break;
    case 0x69:
      ADCInstrHandler(TubeRam[TubeProgramCounter++]); /* immediate */
      break;
    case 0x6a:
      RORInstrHandler_Acc();
      break;
    case 0x6c:
      TubeProgramCounter=IndAddrModeHandler_Address(); /* JMP */
      break;
    case 0x6d:
      ADCInstrHandler(AbsAddrModeHandler_Data());
      break;
    case 0x6e:
      RORInstrHandler(AbsAddrModeHandler_Address());
      break;
    case 0x71:
      ADCInstrHandler(IndYAddrModeHandler_Data());
      break;
    case 0x72:
      if (TubeMachineType==3) ADCInstrHandler(ZPIndAddrModeHandler_Data());
      break;
    case 0x74:
	  if (TubeMachineType==3) { TUBEFASTWRITE(ZeroPgXAddrModeHandler_Address(),0); } else TubeProgramCounter+=1; /* STZ Zpg,X */
      break;
    case 0x75:
      ADCInstrHandler(ZeroPgXAddrModeHandler_Data());
      break;
    case 0x76:
      RORInstrHandler(ZeroPgXAddrModeHandler_Address());
      break;
    case 0x78:
      PSR|=FlagI; /* SEI */
      break;
    case 0x79:
      ADCInstrHandler(AbsYAddrModeHandler_Data());
      break;
    case 0x7a:
		if (TubeMachineType==3) {
			YReg=Pop(); /* PLY */
			PSR&=~(FlagZ | FlagN);
			PSR|=((YReg==0)<<1) | (YReg & 128);
		}
	  break;
    case 0x7c:
      if (TubeMachineType==3) TubeProgramCounter=IndAddrXModeHandler_Address(); /* JMP abs,X*/ else TubeProgramCounter+=2;
      break;
    case 0x7d:
      ADCInstrHandler(AbsXAddrModeHandler_Data());
      break;
    case 0x7e:
      RORInstrHandler(AbsXAddrModeHandler_Address());
      break;
    case 0x81:
      TUBEFASTWRITE(IndXAddrModeHandler_Address(),Accumulator); /* STA */
      break;
    case 0x84:
      TUBEWRITEMEM_DIRECT(ZeroPgAddrModeHandler_Address(),YReg);
      break;
    case 0x85:
      TUBEWRITEMEM_DIRECT(ZeroPgAddrModeHandler_Address(),Accumulator); /* STA */
      break;
    case 0x86:
      TUBEWRITEMEM_DIRECT(ZeroPgAddrModeHandler_Address(),XReg);
      break;
    case 0x88:
      YReg=(YReg-1) & 255; /* DEY */
      PSR&=~(FlagZ | FlagN);
      PSR|=((YReg==0)<<1) | (YReg & 128);
      break;
    case 0x89: /* BIT Immediate */
      if (TubeMachineType==3) BITInstrHandler(TubeRam[TubeProgramCounter++]);
      break;
    case 0x8a:
      Accumulator=XReg; /* TXA */
      PSR&=~(FlagZ | FlagN);
      PSR|=((Accumulator==0)<<1) | (Accumulator & 128);
      break;
    case 0x8c:
      STYInstrHandler(AbsAddrModeHandler_Address());
      break;
    case 0x8d:
      TUBEFASTWRITE(AbsAddrModeHandler_Address(),Accumulator); /* STA */
      break;
    case 0x8e:
      STXInstrHandler(AbsAddrModeHandler_Address());
      break;
    case 0x91:
      TUBEFASTWRITE(IndYAddrModeHandler_Address(),Accumulator); /* STA */
      break;
    case 0x92:
      if (TubeMachineType==3) TUBEFASTWRITE(ZPIndAddrModeHandler_Address(),Accumulator); /* STA */
      break;
    case 0x94:
      STYInstrHandler(ZeroPgXAddrModeHandler_Address());
      break;
    case 0x95:
      TUBEFASTWRITE(ZeroPgXAddrModeHandler_Address(),Accumulator); /* STA */
      break;
    case 0x96:
      STXInstrHandler(ZeroPgYAddrModeHandler_Address());
      break;
    case 0x98:
      Accumulator=YReg; /* TYA */
      PSR&=~(FlagZ | FlagN);
      PSR|=((Accumulator==0)<<1) | (Accumulator & 128);
      break;
    case 0x99:
      TUBEFASTWRITE(AbsYAddrModeHandler_Address(),Accumulator); /* STA */
      break;
    case 0x9a:
      StackReg=XReg; /* TXS */
      break;
    case 0x9c:
      TUBEFASTWRITE(AbsAddrModeHandler_Address(),0); /* STZ Absolute */
	  /* here's a curiosity, STZ Absolute IS on the 6502 UNOFFICIALLY
	  and on the 65C12 OFFICIALLY. Something we should know? - Richard Gellman */
      break;
    case 0x9d:
      TUBEFASTWRITE(AbsXAddrModeHandler_Address(),Accumulator); /* STA */
      break;
    case 0x9e:
		if (TubeMachineType==3) { TUBEFASTWRITE(AbsXAddrModeHandler_Address(),0); } /* STZ Abs,X */ 
		else TubeRam[AbsXAddrModeHandler_Address()] = Accumulator & XReg;
      break;
    case 0xa0:
      LDYInstrHandler(TubeRam[TubeProgramCounter++]); /* immediate */
      break;
    case 0xa1:
      LDAInstrHandler(IndXAddrModeHandler_Data());
      break;
    case 0xa2:
      LDXInstrHandler(TubeRam[TubeProgramCounter++]); /* immediate */
      break;
    case 0xa4:
      LDYInstrHandler(TubeRam[TubeRam[TubeProgramCounter++]]/*zp */);
      break;
    case 0xa5:
      LDAInstrHandler(TubeRam[TubeRam[TubeProgramCounter++]]/*zp */);
      break;
    case 0xa6:
      LDXInstrHandler(TubeRam[TubeRam[TubeProgramCounter++]]/*zp */);
      break;
    case 0xa8:
      YReg=Accumulator; /* TAY */
      PSR&=~(FlagZ | FlagN);
      PSR|=((Accumulator==0)<<1) | (Accumulator & 128);
      break;
    case 0xa9:
      LDAInstrHandler(TubeRam[TubeProgramCounter++]); /* immediate */
      break;
    case 0xaa:
      XReg=Accumulator; /* TXA */
      PSR&=~(FlagZ | FlagN);
      PSR|=((Accumulator==0)<<1) | (Accumulator & 128);
      break;
    case 0xac:
      LDYInstrHandler(AbsAddrModeHandler_Data());
      break;
    case 0xad:
      LDAInstrHandler(AbsAddrModeHandler_Data());
      break;
    case 0xae:
      LDXInstrHandler(AbsAddrModeHandler_Data());
      break;
    case 0xb1:
      LDAInstrHandler(IndYAddrModeHandler_Data());
      break;
    case 0xb2:
      if (TubeMachineType==3) LDAInstrHandler(ZPIndAddrModeHandler_Data());
      break;
    case 0xb4:
      LDYInstrHandler(ZeroPgXAddrModeHandler_Data());
      break;
    case 0xb5:
      LDAInstrHandler(ZeroPgXAddrModeHandler_Data());
      break;
    case 0xb6:
      LDXInstrHandler(ZeroPgYAddrModeHandler_Data());
      break;
    case 0xb8:
      PSR&=255-FlagV; /* CLV */
      break;
    case 0xb9:
      LDAInstrHandler(AbsYAddrModeHandler_Data());
      break;
    case 0xba:
      XReg=StackReg; /* TSX */
      PSR&=~(FlagZ | FlagN);
      PSR|=((XReg==0)<<1) | (XReg & 128);
      break;
    case 0xbc:
      LDYInstrHandler(AbsXAddrModeHandler_Data());
      break;
    case 0xbd:
      LDAInstrHandler(AbsXAddrModeHandler_Data());
      break;
    case 0xbe:
      LDXInstrHandler(AbsYAddrModeHandler_Data());
      break;
    case 0xc0:
      CPYInstrHandler(TubeRam[TubeProgramCounter++]); /* immediate */
      break;
    case 0xc1:
      CMPInstrHandler(IndXAddrModeHandler_Data());
      break;
    case 0xc4:
      CPYInstrHandler(TubeRam[TubeRam[TubeProgramCounter++]]/*zp */);
      break;
    case 0xc5:
      CMPInstrHandler(TubeRam[TubeRam[TubeProgramCounter++]]/*zp */);
      break;
    case 0xc6:
      DECInstrHandler(ZeroPgAddrModeHandler_Address());
      break;
    case 0xc8:
      YReg+=1; /* INY */
      YReg&=255;
      PSR&=~(FlagZ | FlagN);
      PSR|=((YReg==0)<<1) | (YReg & 128);
      break;
    case 0xc9:
      CMPInstrHandler(TubeRam[TubeProgramCounter++]); /* immediate */
      break;
    case 0xca:
      DEXInstrHandler();
      break;
    case 0xcc:
      CPYInstrHandler(AbsAddrModeHandler_Data());
      break;
    case 0xcd:
      CMPInstrHandler(AbsAddrModeHandler_Data());
      break;
    case 0xce:
      DECInstrHandler(AbsAddrModeHandler_Address());
      break;
    case 0xd1:
      CMPInstrHandler(IndYAddrModeHandler_Data());
      break;
    case 0xd2:
      if (TubeMachineType==3) CMPInstrHandler(ZPIndAddrModeHandler_Data());
      break;
    case 0xd5:
      CMPInstrHandler(ZeroPgXAddrModeHandler_Data());
      break;
    case 0xd6:
      DECInstrHandler(ZeroPgXAddrModeHandler_Address());
      break;
    case 0xd8:
      PSR&=255-FlagD; /* CLD */
      break;
    case 0xd9:
      CMPInstrHandler(AbsYAddrModeHandler_Data());
      break;
    case 0xda:
      if (TubeMachineType==3) Push(XReg); /* PHX */
      break;
    case 0xdd:
      CMPInstrHandler(AbsXAddrModeHandler_Data());
      break;
    case 0xde:
      DECInstrHandler(AbsXAddrModeHandler_Address());
      break;
    case 0xe0:
      CPXInstrHandler(TubeRam[TubeProgramCounter++]); /* immediate */
      break;
    case 0xe1:
      SBCInstrHandler(IndXAddrModeHandler_Data());
      break;
    case 0xe4:
      CPXInstrHandler(TubeRam[TubeRam[TubeProgramCounter++]]/*zp */);
      break;
    case 0xe5:
      SBCInstrHandler(TubeRam[TubeRam[TubeProgramCounter++]]/*zp */);
      break;
    case 0xe6:
      INCInstrHandler(ZeroPgAddrModeHandler_Address());
      break;
    case 0xe8:
      INXInstrHandler();
      break;
    case 0xe9:
      SBCInstrHandler(TubeRam[TubeProgramCounter++]); /* immediate */
      break;
    case 0xea:
      /* NOP */
      break;
    case 0xec:
      CPXInstrHandler(AbsAddrModeHandler_Data());
      break;
    case 0xed:
      SBCInstrHandler(AbsAddrModeHandler_Data());
      break;
    case 0xee:
      INCInstrHandler(AbsAddrModeHandler_Address());
      break;
    case 0xf1:
      SBCInstrHandler(IndYAddrModeHandler_Data());
      break;
    case 0xf2:
      if (TubeMachineType==3) SBCInstrHandler(ZPIndAddrModeHandler_Data());
      break;
    case 0xf5:
      SBCInstrHandler(ZeroPgXAddrModeHandler_Data());
      break;
    case 0xf6:
      INCInstrHandler(ZeroPgXAddrModeHandler_Address());
      break;
    case 0xf8:
      PSR|=FlagD; /* SED */
      break;
    case 0xf9:
      SBCInstrHandler(AbsYAddrModeHandler_Data());
      break;
    case 0xfa:
		if (TubeMachineType==3) {
	  XReg=Pop(); /* PLX */
      PSR&=~(FlagZ | FlagN);
      PSR|=((XReg==0)<<1) | (XReg & 128);
		}
		break;
    case 0xfd:
      SBCInstrHandler(AbsXAddrModeHandler_Data());
      break;
    case 0xfe:
      INCInstrHandler(AbsXAddrModeHandler_Address());
      break;
    case 0x07: /* Undocumented Instruction: ASL zp and ORA zp */
      {
        int16 zpaddr = ZeroPgAddrModeHandler_Address();
        ASLInstrHandler(zpaddr);
        ORAInstrHandler(TubeRam[zpaddr]);
      }
      break;
    case 0x03: /* Undocumented Instruction: ASL-ORA (zp,X) */
      {
        int16 zpaddr = IndXAddrModeHandler_Address();
        ASLInstrHandler(zpaddr);
        ORAInstrHandler(TubeRam[zpaddr]);
      }
      break;
    case 0x13: /* Undocumented Instruction: ASL-ORA (zp),Y */
      {
        int16 zpaddr = IndYAddrModeHandler_Address();
        ASLInstrHandler(zpaddr);
        ORAInstrHandler(TubeRam[zpaddr]);
      }
      break;
    case 0x0f: /* Undocumented Instruction: ASL-ORA abs */
      {
        int16 zpaddr = AbsAddrModeHandler_Address();
        ASLInstrHandler(zpaddr);
        ORAInstrHandler(TubeRam[zpaddr]);
      }
      break;
    case 0x17: /* Undocumented Instruction: ASL-ORA zp,X */
      {
        int16 zpaddr = ZeroPgXAddrModeHandler_Address();
        ASLInstrHandler(zpaddr);
        ORAInstrHandler(TubeRam[zpaddr]);
      }
      break;
    case 0x1b: /* Undocumented Instruction: ASL-ORA abs,Y */
      {
        int16 zpaddr = AbsYAddrModeHandler_Address();
        ASLInstrHandler(zpaddr);
        ORAInstrHandler(TubeRam[zpaddr]);
      }
      break;
    case 0x1f: /* Undocumented Instruction: ASL-ORA abs,X */
      {
        int16 zpaddr = AbsXAddrModeHandler_Address();
        ASLInstrHandler(zpaddr);
        ORAInstrHandler(TubeRam[zpaddr]);
      }
      break;
	case 0x23: /* Undocumented Instruction: ROL-AND (zp,X) */
		{
		int16 zpaddr=IndXAddrModeHandler_Address();
		ROLInstrHandler(zpaddr);
		ANDInstrHandler(TubeRam[zpaddr]);
		}
		break;
	case 0x27: /* Undocumented Instruction: ROL-AND zp */
		{
		int16 zpaddr=ZeroPgAddrModeHandler_Address();
		ROLInstrHandler(zpaddr);
		ANDInstrHandler(TubeRam[zpaddr]);
		}
		break;
	case 0x2f: /* Undocumented Instruction: ROL-AND abs */
		{
		int16 zpaddr=AbsAddrModeHandler_Address();
		ROLInstrHandler(zpaddr);
		ANDInstrHandler(TubeRam[zpaddr]);
		}
		break;
	case 0x33: /* Undocumented Instruction: ROL-AND (zp),Y */
		{
		int16 zpaddr=IndYAddrModeHandler_Address();
		ROLInstrHandler(zpaddr);
		ANDInstrHandler(TubeRam[zpaddr]);
		}
		break;
	case 0x37: /* Undocumented Instruction: ROL-AND zp,X */
		{
		int16 zpaddr=ZeroPgXAddrModeHandler_Address();
		ROLInstrHandler(zpaddr);
		ANDInstrHandler(TubeRam[zpaddr]);
		}
		break;
	case 0x3b: /* Undocumented Instruction: ROL-AND abs.Y */
		{
		int16 zpaddr=AbsYAddrModeHandler_Address();
		ROLInstrHandler(zpaddr);
		ANDInstrHandler(TubeRam[zpaddr]);
		}
		break;
	case 0x3f: /* Undocumented Instruction: ROL-AND abs.X */
		{
		int16 zpaddr=AbsXAddrModeHandler_Address();
		ROLInstrHandler(zpaddr);
		ANDInstrHandler(TubeRam[zpaddr]);
		}
		break;
	case 0x43: /* Undocumented Instruction: LSR-EOR (zp,X) */
		{
		int16 zpaddr=IndXAddrModeHandler_Address();
		LSRInstrHandler(zpaddr);
		EORInstrHandler(TubeRam[zpaddr]);
		}
		break;
	case 0x47: /* Undocumented Instruction: LSR-EOR zp */
		{
		int16 zpaddr=ZeroPgAddrModeHandler_Address();
		LSRInstrHandler(zpaddr);
		EORInstrHandler(TubeRam[zpaddr]);
		}
		break;
	case 0x4f: /* Undocumented Instruction: LSR-EOR abs */
		{
		int16 zpaddr=AbsAddrModeHandler_Address();
		LSRInstrHandler(zpaddr);
		EORInstrHandler(TubeRam[zpaddr]);
		}
		break;
	case 0x53: /* Undocumented Instruction: LSR-EOR (zp),Y */
		{
		int16 zpaddr=IndYAddrModeHandler_Address();
		LSRInstrHandler(zpaddr);
		EORInstrHandler(TubeRam[zpaddr]);
		}
		break;
	case 0x57: /* Undocumented Instruction: LSR-EOR zp,X */
		{
		int16 zpaddr=ZeroPgXAddrModeHandler_Address();
		LSRInstrHandler(zpaddr);
		EORInstrHandler(TubeRam[zpaddr]);
		}
		break;
	case 0x5b: /* Undocumented Instruction: LSR-EOR abs,Y */
		{
		int16 zpaddr=AbsYAddrModeHandler_Address();
		LSRInstrHandler(zpaddr);
		EORInstrHandler(TubeRam[zpaddr]);
		}
		break;
	case 0x5f: /* Undocumented Instruction: LSR-EOR abs,X */
		{
		int16 zpaddr=AbsXAddrModeHandler_Address();
		LSRInstrHandler(zpaddr);
		EORInstrHandler(TubeRam[zpaddr]);
		}
		break;
	case 0x44:
	case 0x54:
		TubeProgramCounter+=1;
		break;
	case 0x5c:
		TubeProgramCounter+=2;
		break;
	case 0x63: /* Undocumented Instruction: ROR-ADC (zp,X) */
		{
		int16 zpaddr=IndXAddrModeHandler_Address();
		RORInstrHandler(zpaddr);
		ADCInstrHandler(TubeRam[zpaddr]);
		}
		break;
	case 0x67: /* Undocumented Instruction: ROR-ADC zp */
		{
		int16 zpaddr=ZeroPgAddrModeHandler_Address();
		RORInstrHandler(zpaddr);
		ADCInstrHandler(TubeRam[zpaddr]);
		}
		break;
	case 0x6f: /* Undocumented Instruction: ROR-ADC abs */
		{
		int16 zpaddr=AbsAddrModeHandler_Address();
		RORInstrHandler(zpaddr);
		ADCInstrHandler(TubeRam[zpaddr]);
		}
		break;
	case 0x73: /* Undocumented Instruction: ROR-ADC (zp),Y */
		{
		int16 zpaddr=IndYAddrModeHandler_Address();
		RORInstrHandler(zpaddr);
		ADCInstrHandler(TubeRam[zpaddr]);
		}
		break;
	case 0x77: /* Undocumented Instruction: ROR-ADC zp,X */
		{
		int16 zpaddr=ZeroPgXAddrModeHandler_Address();
		RORInstrHandler(zpaddr);
		ADCInstrHandler(TubeRam[zpaddr]);
		}
		break;
	case 0x7b: /* Undocumented Instruction: ROR-ADC abs,Y */
		{
		int16 zpaddr=AbsYAddrModeHandler_Address();
		RORInstrHandler(zpaddr);
		ADCInstrHandler(TubeRam[zpaddr]);
		}
		break;
	case 0x7f: /* Undocumented Instruction: ROR-ADC abs,X */
		{
		int16 zpaddr=AbsXAddrModeHandler_Address();
		RORInstrHandler(zpaddr);
		ADCInstrHandler(TubeRam[zpaddr]);
		}
		break;
    case 0x0b:
	case 0x2b:
      ANDInstrHandler(TubeRam[TubeProgramCounter++]); /* AND-MVC #n,b7 */
	  PSR|=((Accumulator & 128)>>7);
      break;
    case 0x4b: /* Undocumented Instruction: AND imm and LSR A */
      ANDInstrHandler(TubeRam[TubeProgramCounter++]);
      LSRInstrHandler_Acc();
      break;
    case 0x87: /* Undocumented Instruction: SAX zp (i.e. (zp) = A & X) */
      /* This one does not seem to change the processor flags */
      TubeRam[ZeroPgAddrModeHandler_Address()] = Accumulator & XReg;
      break;
    case 0x83: /* Undocumented Instruction: SAX (zp,X) */
      TubeRam[IndXAddrModeHandler_Address()] = Accumulator & XReg;
      break;
    case 0x8f: /* Undocumented Instruction: SAX abs */
      TubeRam[AbsAddrModeHandler_Address()] = Accumulator & XReg;
      break;
    case 0x93: /* Undocumented Instruction: SAX (zp),Y */
      TubeRam[IndYAddrModeHandler_Address()] = Accumulator & XReg;
      break;
    case 0x97: /* Undocumented Instruction: SAX zp,Y */
      TubeRam[ZeroPgYAddrModeHandler_Address()] = Accumulator & XReg;
      break;
    case 0x9b: /* Undocumented Instruction: SAX abs,Y */
      TubeRam[AbsYAddrModeHandler_Address()] = Accumulator & XReg;
      break;
    case 0x9f: /* Undocumented Instruction: SAX abs,X */
      TubeRam[AbsXAddrModeHandler_Address()] = Accumulator & XReg;
      break;
    case 0xab: /* Undocumented Instruction: LAX #n */
      LDAInstrHandler(TubeRam[TubeProgramCounter++]);
      XReg = Accumulator;
      break;
    case 0xa3: /* Undocumented Instruction: LAX (zp,X) */
      LDAInstrHandler(IndXAddrModeHandler_Data());
      XReg = Accumulator;
      break;
    case 0xa7: /* Undocumented Instruction: LAX zp */
      LDAInstrHandler(TubeRam[TubeRam[TubeProgramCounter++]]);
      XReg = Accumulator;
      break;
    case 0xaf: /* Undocumented Instruction: LAX abs */
      LDAInstrHandler(AbsAddrModeHandler_Data());
      XReg = Accumulator;
      break;
    case 0xb3: /* Undocumented Instruction: LAX (zp),Y */
      LDAInstrHandler(IndYAddrModeHandler_Data());
      XReg = Accumulator;
      break;
    case 0xb7: /* Undocumented Instruction: LAX zp,Y */
      LDXInstrHandler(ZeroPgYAddrModeHandler_Data());
      Accumulator = XReg;
      break;
	case 0xbb:
    case 0xbf: /* Undocumented Instruction: LAX abs,Y */
      LDAInstrHandler(AbsYAddrModeHandler_Data());
      XReg = Accumulator;
      break;
	// Undocumented DEC-CMP and INC-SBC Instructions
	case 0xc3: // DEC-CMP (zp,X)
		{
		int16 zpaddr=IndXAddrModeHandler_Address();
		DECInstrHandler(zpaddr);
		CMPInstrHandler(TubeRam[zpaddr]);
		}
		break;
	case 0xc7: // DEC-CMP zp
		{
		int16 zpaddr=ZeroPgAddrModeHandler_Address();
		DECInstrHandler(zpaddr);
		CMPInstrHandler(TubeRam[zpaddr]);
		}
		break;
	case 0xcf: // DEC-CMP abs
		{
		int16 zpaddr=AbsAddrModeHandler_Address();
		DECInstrHandler(zpaddr);
		CMPInstrHandler(TubeRam[zpaddr]);
		}
		break;
	case 0xd3: // DEC-CMP (zp),Y
		{
		int16 zpaddr=IndYAddrModeHandler_Address();
		DECInstrHandler(zpaddr);
		CMPInstrHandler(TubeRam[zpaddr]);
		}
		break;
	case 0xd7: // DEC-CMP zp,X
		{
		int16 zpaddr=ZeroPgXAddrModeHandler_Address();
		DECInstrHandler(zpaddr);
		CMPInstrHandler(TubeRam[zpaddr]);
		}
		break;
	case 0xdb: // DEC-CMP abs,Y
		{
		int16 zpaddr=AbsYAddrModeHandler_Address();
		DECInstrHandler(zpaddr);
		CMPInstrHandler(TubeRam[zpaddr]);
		}
		break;
	case 0xdf: // DEC-CMP abs,X
		{
		int16 zpaddr=AbsXAddrModeHandler_Address();
		DECInstrHandler(zpaddr);
		CMPInstrHandler(TubeRam[zpaddr]);
		}
		break;
	case 0xd4:
	case 0xf4:
		TubeProgramCounter+=1;
		break;
	case 0xdc:
	case 0xfc:
		TubeProgramCounter+=2;
		break;
	case 0xe3: // INC-SBC (zp,X)
		{
		int16 zpaddr=IndXAddrModeHandler_Address();
		INCInstrHandler(zpaddr);
		SBCInstrHandler(TubeRam[zpaddr]);
		}
		break;
	case 0xe7: // INC-SBC zp
		{
		int16 zpaddr=ZeroPgAddrModeHandler_Address();
		INCInstrHandler(zpaddr);
		SBCInstrHandler(TubeRam[zpaddr]);
		}
		break;
	case 0xef: // INC-SBC abs
		{
		int16 zpaddr=AbsAddrModeHandler_Address();
		INCInstrHandler(zpaddr);
		SBCInstrHandler(TubeRam[zpaddr]);
		}
		break;
	case 0xf3: // INC-SBC (zp).Y
		{
		int16 zpaddr=IndYAddrModeHandler_Address();
		INCInstrHandler(zpaddr);
		SBCInstrHandler(TubeRam[zpaddr]);
		}
		break;
	case 0xf7: // INC-SBC zp,X
		{
		int16 zpaddr=ZeroPgXAddrModeHandler_Address();
		INCInstrHandler(zpaddr);
		SBCInstrHandler(TubeRam[zpaddr]);
		}
		break;
	case 0xfb: // INC-SBC abs,Y
		{
		int16 zpaddr=AbsYAddrModeHandler_Address();
		INCInstrHandler(zpaddr);
		SBCInstrHandler(TubeRam[zpaddr]);
		}
		break;
	case 0xff: // INC-SBC abs,X
		{
		int16 zpaddr=AbsXAddrModeHandler_Address();
		INCInstrHandler(zpaddr);
		SBCInstrHandler(TubeRam[zpaddr]);
		}
		break;
	// REALLY Undocumented instructions 6B, 8B and CB
    case 0x6b:
		ANDInstrHandler(TubeRam[TubeProgramCounter++]);
		RORInstrHandler_Acc();
		break;
	case 0x8b:
		Accumulator=XReg; /* TXA */
		PSR&=~(FlagZ | FlagN);
		PSR|=((Accumulator==0)<<1) | (Accumulator & 128);
		ANDInstrHandler(TubeRam[TubeProgramCounter++]);
		break;
	case 0xcb:
		// SBX #n - I dont know if this uses the carry or not, i'm assuming its
		// Subtract #n from X with carry.
		{
			unsigned char TmpAcc=Accumulator;
			Accumulator=XReg;
			SBCInstrHandler(TubeRam[TubeProgramCounter++]);
			XReg=Accumulator;
			Accumulator=TmpAcc; // Fudge so that I dont have to do the whole SBC code again
		}
		break;
    default:
      BadInstrHandler(CurrentInstruction);
      break;
	break;

  }; /* OpCode switch */

  // This block corrects the cycle count for the branch instructions
  if ((CurrentInstruction==0x10) ||
      (CurrentInstruction==0x30) ||
      (CurrentInstruction==0x50) ||
      (CurrentInstruction==0x70) ||
      (CurrentInstruction==0x80) ||
      (CurrentInstruction==0x90) ||
      (CurrentInstruction==0xb0) ||
      (CurrentInstruction==0xd0) ||
      (CurrentInstruction==0xf0))
  {
    if (Branched)
    {
      TubeCycles++;
      if ((TubeProgramCounter & 0xff00) != ((OldPC+2) & 0xff00))
        TubeCycles+=1;
    }
  }

  TubeCycles+=IRQCycles;
  IRQCycles=0; // IRQ Timing
  // End of cycle correction

  if ((TubeintStatus) && (!GETIFLAG)) DoTubeInterrupt();
  
  TotalTubeCycles+=TubeCycles;

  if ((TubeNMIStatus) && (!OldTubeNMIStatus)) DoTubeNMI();
  OldTubeNMIStatus=TubeNMIStatus;
} /* Exec6502Instruction */

/*-------------------------------------------------------------------------*/
void WrapTubeCycles(void) {
	TotalTubeCycles -= CycleCountWrap/2*3;
}

void SyncTubeProcessor(void) {
	// This proc syncronises the two processors on a cycle based timing.
	// Second pro runs at 3MHz
	while (TotalTubeCycles<(TotalCycles/2*3)) {
		Exec65C02Instruction();
	}
}

/*-------------------------------------------------------------------------*/
void DebugTubeState(void)
{
	DebugDisplayInfo("");

	DebugDisplayInfoF("HostTube: R1=%02X R2=%02X R3=%02X R4=%02X R1n=%02X R3n=%02X",
		(int)R1HStatus | R1Status,
		(int)R2HStatus,
		(int)R3HStatus,
		(int)R4HStatus,
		(int)R1PHPtr,
		(int)R3PHPtr);

	DebugDisplayInfoF("ParaTube: R1=%02X R2=%02X R3=%02X R4=%02X R3n=%02X",
		(int)R1PStatus | R1Status,
		(int)R2PStatus,
		(int)R3PStatus,
		(int)R4PStatus,
		(int)R3HPPtr);
}

/*-------------------------------------------------------------------------*/
void SaveTubeUEF(FILE *SUEF) {
	fput16(0x0470,SUEF);
	fput32(45,SUEF);
	fputc(R1Status,SUEF);
	fwrite(R1PHData,1,24,SUEF);
	fputc(R1PHPtr,SUEF);
	fputc(R1HStatus,SUEF);
	fputc(R1HPData,SUEF);
	fputc(R1PStatus,SUEF);
	fputc(R2PHData,SUEF);
	fputc(R2HStatus,SUEF);
	fputc(R2HPData,SUEF);
	fputc(R2PStatus,SUEF);
	fputc(R3PHData[0],SUEF);
	fputc(R3PHData[1],SUEF);
	fputc(R3PHPtr,SUEF);
	fputc(R3HStatus,SUEF);
	fputc(R3HPData[0],SUEF);
	fputc(R3HPData[1],SUEF);
	fputc(R3HPPtr,SUEF);
	fputc(R3PStatus,SUEF);
	fputc(R4PHData,SUEF);
	fputc(R4HStatus,SUEF);
	fputc(R4HPData,SUEF);
	fputc(R4PStatus,SUEF);
}

void Save65C02UEF(FILE *SUEF) {
	fput16(0x0471,SUEF);
	fput32(16,SUEF);
	fput16(TubeProgramCounter,SUEF);
	fputc(Accumulator,SUEF);
	fputc(XReg,SUEF);
	fputc(YReg,SUEF);
	fputc(StackReg,SUEF);
	fputc(PSR,SUEF);
	fput32(TotalTubeCycles,SUEF);
	fputc(TubeintStatus,SUEF);
	fputc(TubeNMIStatus,SUEF);
	fputc(NMILock,SUEF);
	fput16(0,SUEF);
}

void Save65C02MemUEF(FILE *SUEF) {
	fput16(0x0472,SUEF);
	fput32(65536,SUEF);
	fwrite(TubeRam,1,65536,SUEF);
}

void LoadTubeUEF(FILE *SUEF) {
	R1Status=fgetc(SUEF);
	fread(R1PHData,1,24,SUEF);
	R1PHPtr=fgetc(SUEF);
	R1HStatus=fgetc(SUEF);
	R1HPData=fgetc(SUEF);
	R1PStatus=fgetc(SUEF);
	R2PHData=fgetc(SUEF);
	R2HStatus=fgetc(SUEF);
	R2HPData=fgetc(SUEF);
	R2PStatus=fgetc(SUEF);
	R3PHData[0]=fgetc(SUEF);
	R3PHData[1]=fgetc(SUEF);
	R3PHPtr=fgetc(SUEF);
	R3HStatus=fgetc(SUEF);
	R3HPData[0]=fgetc(SUEF);
	R3HPData[1]=fgetc(SUEF);
	R3HPPtr=fgetc(SUEF);
	R3PStatus=fgetc(SUEF);
	R4PHData=fgetc(SUEF);
	R4HStatus=fgetc(SUEF);
	R4HPData=fgetc(SUEF);
	R4PStatus=fgetc(SUEF);
}

void Load65C02UEF(FILE *SUEF) {
	int Dlong;
	TubeProgramCounter=fget16(SUEF);
	Accumulator=fgetc(SUEF);
	XReg=fgetc(SUEF);
	YReg=fgetc(SUEF);
	StackReg=fgetc(SUEF);
	PSR=fgetc(SUEF);
	//TotalTubeCycles=fget32(SUEF);
	Dlong=fget32(SUEF);
	TubeintStatus=fgetc(SUEF);
	TubeNMIStatus=fgetc(SUEF);
	NMILock=fgetc(SUEF);
}

void Load65C02MemUEF(FILE *SUEF) {
	fread(TubeRam,1,65536,SUEF);
}
