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

#ifdef WIN32
#include <windows.h>
#endif

#include <iostream>
#include <fstream>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "6502core.h"
#include "beebmem.h"
#include "beebsound.h"
#include "music5000.h"
#include "disc8271.h"
#include "via.h"
#include "sysvia.h"
#include "uservia.h"
#include "video.h"
#include "atodconv.h"
#include "log.h"
#include "main.h"
#include "disc1770.h"
#include "serial.h"
#include "tube.h"
#include "debug.h"
#include "uefstate.h"
#include "z80mem.h"
#include "z80.h"
#include "econet.h"
#include "scsi.h"
#include "ide.h"
#include "debug.h"
#include "Arm.h"
#include "sprowcopro.h"
#include "Master512CoPro.h"

#ifdef WIN32
#define INLINE inline
#else
#define INLINE
#endif

using namespace std;

static int CurrentInstruction;

extern CArm *arm;
extern CSprowCoPro *sprow;

CycleCountT TotalCycles=0;

static bool trace = false;
int ProgramCounter;
int PrePC;
static int Accumulator,XReg,YReg;
static unsigned char StackReg,PSR;
static unsigned char IRQCycles;
int DisplayCycles=0;

unsigned char intStatus=0; /* bit set (nums in IRQ_Nums) if interrupt being caused */
unsigned char NMIStatus=0; /* bit set (nums in NMI_Nums) if NMI being caused */
bool NMILock = false; // Well I think NMI's are maskable - to stop repeated NMI's - the lock is released when an RTI is done
typedef int int16;

/* Note how GETCFLAG is special since being bit 0 we don't need to test it to get a clean 0/1 */
#define GETCFLAG ((PSR & FlagC))
#define GETZFLAG ((PSR & FlagZ)>0)
#define GETIFLAG ((PSR & FlagI)>0)
#define GETDFLAG ((PSR & FlagD)>0)
#define GETBFLAG ((PSR & FlagB)>0)
#define GETVFLAG ((PSR & FlagV)>0)
#define GETNFLAG ((PSR & FlagN)>0)

static const int CyclesTable6502[] = {
/*0 1 2 3 4 5 6 7 8 9 a b c d e f */
  7,6,1,8,3,3,5,5,3,2,2,2,4,4,6,6, /* 0 */
  2,5,1,8,4,4,6,6,2,4,2,7,4,4,7,7, /* 1 */
  6,6,1,8,3,3,5,5,4,2,2,2,4,4,6,6, /* 2 */
  2,5,1,8,4,4,6,6,2,4,2,7,4,4,7,7, /* 3 */
  6,6,1,8,3,3,5,5,3,2,2,2,3,4,6,6, /* 4 */
  2,5,1,8,4,4,6,6,2,4,2,7,4,4,7,7, /* 5 */
  6,6,1,8,3,3,5,5,4,2,2,2,5,4,6,6, /* 6 */
  2,5,1,8,4,4,6,6,2,4,2,7,4,4,7,7, /* 7 */
  2,6,2,6,3,3,3,3,2,2,2,2,4,4,4,4, /* 8 */
  2,6,1,6,4,4,4,4,2,5,2,5,5,5,5,5, /* 9 */
  2,6,2,6,3,3,3,3,2,2,2,2,4,4,4,4, /* a */
  2,5,1,5,4,4,4,4,2,4,2,4,4,4,4,4, /* b */
  2,6,2,8,3,3,5,5,2,2,2,2,4,4,6,6, /* c */
  2,5,1,8,4,4,6,6,2,4,2,7,4,4,7,7, /* d */
  2,6,2,8,3,3,5,5,2,2,2,2,4,4,6,6, /* e */
  2,5,1,8,4,4,6,6,2,4,2,7,4,4,7,7  /* f */
};

static const int CyclesTable65C02[] = {
/*0 1 2 3 4 5 6 7 8 9 a b c d e f */
  7,6,2,1,5,3,5,1,3,2,2,1,6,4,6,1, /* 0 */
  2,5,5,1,5,4,6,1,2,4,2,1,6,4,6,1, /* 1 */
  6,6,2,1,3,3,5,1,4,2,2,1,4,4,6,1, /* 2 */
  2,5,5,1,4,4,6,1,2,4,2,1,4,4,6,1, /* 3 */
  6,6,2,1,3,3,5,1,3,2,2,1,3,4,6,1, /* 4 */
  2,5,5,1,4,4,6,1,2,4,3,1,8,4,6,1, /* 5 */
  6,6,2,1,3,3,5,1,4,2,2,1,6,4,6,1, /* 6 */
  2,5,5,1,4,4,6,1,2,4,4,1,6,4,6,1, /* 7 */
  3,6,2,1,3,3,3,1,2,2,2,1,4,4,4,1, /* 8 */
  2,6,5,1,4,4,4,1,2,5,2,1,4,5,5,1, /* 9 */
  2,6,2,1,3,3,3,1,2,2,2,1,4,4,4,1, /* a */
  2,5,5,1,4,4,4,1,2,4,2,1,4,4,4,1, /* b */
  2,6,2,1,3,3,5,1,2,2,2,1,4,4,6,1, /* c */
  2,5,5,1,4,4,6,1,2,4,3,1,4,4,7,1, /* d */
  2,6,2,1,3,3,5,1,2,2,2,1,4,4,6,1, /* e */
  2,5,5,1,4,4,6,1,2,4,4,1,4,4,7,1  /* f */
};

const int *CyclesTable = CyclesTable6502;

// Number of cycles to start of memory read cycle
static const int CyclesToMemRead6502[] = {
/*0 1 2 3 4 5 6 7 8 9 a b c d e f */
  0,5,0,7,0,2,2,2,0,0,0,0,3,3,3,3, /* 0 */
  0,4,0,7,0,3,3,3,0,3,0,0,3,3,4,4, /* 1 */
  0,5,0,7,0,2,2,2,0,0,0,0,3,3,3,3, /* 2 */
  0,4,0,7,0,3,3,3,0,3,0,0,3,3,4,4, /* 3 */
  0,5,0,7,0,2,2,2,0,0,0,0,0,3,3,3, /* 4 */
  0,4,0,7,0,3,3,3,0,3,0,0,3,3,4,4, /* 5 */
  0,5,0,7,0,2,2,2,0,0,0,0,0,3,3,3, /* 6 */
  0,4,0,7,0,3,3,3,0,3,0,0,3,3,4,4, /* 7 */
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 8 */
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 9 */
  0,5,0,5,2,2,2,2,0,0,0,0,3,3,3,3, /* a */
  0,4,0,4,3,3,3,3,0,3,0,0,3,3,4,4, /* b */
  0,5,0,7,2,2,2,2,0,0,0,0,3,3,3,3, /* c */
  0,4,0,7,0,3,3,3,0,3,0,0,3,3,4,4, /* d */
  0,5,0,7,2,2,2,2,0,0,0,0,3,3,3,3, /* e */
  0,4,0,7,0,3,3,3,0,3,0,0,3,3,4,4  /* f */
};

static const int CyclesToMemRead65C02[] = {
/*0 1 2 3 4 5 6 7 8 9 a b c d e f */
  0,5,0,0,0,2,2,0,0,0,0,0,0,3,3,0, /* 0 */
  0,4,0,0,0,3,3,0,0,3,0,0,0,3,4,0, /* 1 */
  0,5,0,0,0,2,2,0,0,0,0,0,3,3,3,0, /* 2 */
  0,4,0,0,0,3,3,0,0,3,0,0,0,3,4,0, /* 3 */
  0,5,0,0,0,2,2,0,0,0,0,0,0,3,3,0, /* 4 */
  0,4,0,0,0,3,3,0,0,3,0,0,0,3,4,0, /* 5 */
  0,5,0,0,0,2,2,0,0,0,0,0,0,3,3,0, /* 6 */
  0,4,0,0,0,3,3,0,0,3,0,0,0,3,4,0, /* 7 */
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 8 */
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* 9 */
  0,5,0,0,2,2,2,0,0,0,0,0,3,3,3,0, /* a */
  0,4,0,0,3,3,3,0,0,3,0,0,3,3,4,0, /* b */
  0,5,0,0,2,2,2,0,0,0,0,0,3,3,3,0, /* c */
  0,4,0,0,0,3,3,0,0,3,0,0,0,3,4,0, /* d */
  0,5,0,0,2,2,2,0,0,0,0,0,3,3,3,0, /* e */
  0,4,0,0,0,3,3,0,0,3,0,0,0,3,4,0  /* f */
};

const int *CyclesToMemRead = CyclesToMemRead6502;

// Number of cycles to start of memory write cycle
static const int CyclesToMemWrite6502[] = {
/*0 1 2 3 4 5 6 7 8 9 a b c d e f */
  0,0,0,0,0,0,2,2,0,0,0,0,0,0,2,2, /* 0 */
  0,0,0,0,0,0,2,2,0,0,0,0,0,0,2,2, /* 1 */
  0,0,0,0,0,0,2,2,0,0,0,0,0,0,2,2, /* 2 */
  0,0,0,0,0,0,2,2,0,0,0,0,0,0,2,2, /* 3 */
  0,0,0,0,0,0,2,2,0,0,0,0,0,0,2,2, /* 4 */
  0,0,0,0,0,0,2,2,0,0,0,0,0,0,2,2, /* 5 */
  0,0,0,0,0,0,2,2,0,0,0,0,0,0,2,2, /* 6 */
  0,0,0,0,0,0,2,2,0,0,0,0,0,0,2,2, /* 7 */
  0,5,0,5,2,2,2,2,0,0,0,0,3,3,3,3, /* 8 */
  0,5,0,5,3,3,3,3,0,4,0,0,0,0,0,4, /* 9 */
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* a */
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* b */
  0,0,0,0,0,0,2,2,0,0,0,0,0,0,2,2, /* c */
  0,0,0,0,0,0,2,2,0,0,0,0,0,0,2,2, /* d */
  0,0,0,0,0,0,2,2,0,0,0,0,0,0,2,2, /* e */
  0,0,0,0,0,0,2,2,0,0,0,0,0,0,2,2  /* f */
};

static const int CyclesToMemWrite65C02[] = {
/*0 1 2 3 4 5 6 7 8 9 a b c d e f */
  0,0,0,0,0,0,2,0,0,0,0,0,0,0,2,0, /* 0 */
  0,0,0,0,0,0,2,0,0,0,0,0,0,0,2,0, /* 1 */
  0,0,0,0,0,0,2,0,0,0,0,0,0,0,2,0, /* 2 */
  0,0,0,0,0,0,2,0,0,0,0,0,0,0,2,0, /* 3 */
  0,0,0,0,0,0,2,0,0,0,0,0,0,0,2,0, /* 4 */
  0,0,0,0,0,0,2,0,0,0,0,0,0,0,2,0, /* 5 */
  0,0,0,0,0,0,2,0,0,0,0,0,0,0,2,0, /* 6 */
  0,0,0,0,0,0,2,0,0,0,0,0,0,0,2,0, /* 7 */
  0,5,0,0,2,2,2,0,0,0,0,0,3,3,3,0, /* 8 */
  0,5,0,0,3,3,3,0,0,4,0,0,0,0,0,0, /* 9 */
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* a */
  0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, /* b */
  0,0,0,0,0,0,2,0,0,0,0,0,0,0,2,0, /* c */
  0,0,0,0,0,0,2,0,0,0,0,0,0,0,2,0, /* d */
  0,0,0,0,0,0,2,0,0,0,0,0,0,0,2,0, /* e */
  0,0,0,0,0,0,2,0,0,0,0,0,0,0,2,0  /* f */
};

const int *CyclesToMemWrite = CyclesToMemWrite6502;

/* The number of cycles to be used by the current instruction - exported to
   allow fernangling by memory subsystem */
static unsigned int Cycles;

/* Number of cycles VIAs advanced for mem read and writes */
static unsigned int ViaCycles;

/* Number of additional cycles for IO read / writes */
int IOCycles=0;

/* Flag indicating if an interrupt is due */
bool IntDue=false;

/* When a timer interrupt is due this is the number of cycles
   to it (usually -ve) */
int CyclesToInt = NO_TIMER_INT_DUE;

static bool Branched; // true if the instruction branched
bool BasicHardwareOnly = false; // false = all hardware, true = basic hardware only
// 1 if first cycle happened

// Get a two byte address from the program counter, and then post inc
// the program counter
#define GETTWOBYTEFROMPC(var) \
	var = ReadPaged(ProgramCounter++); \
	var |= (ReadPaged(ProgramCounter++) << 8);

#define WritePaged(addr,val) BeebWriteMem(addr,val)
#define ReadPaged(Address) BeebReadMem(Address)

static void PollVIAs(unsigned int nCycles);
static void PollHardware(unsigned int nCycles);

static unsigned int InstructionCount[256];

/*----------------------------------------------------------------------------*/

// Correct cycle count for indirection across page boundary

static INLINE void Carried()
{
	if (((CurrentInstruction & 0xf) == 0x1 ||
	     (CurrentInstruction & 0xf) == 0x9 ||
	     (CurrentInstruction & 0xf) == 0xd) &&
	    (CurrentInstruction & 0xf0) != 0x90)
	{
		Cycles++;
	}
	else if (CurrentInstruction == 0x1c ||
	         CurrentInstruction == 0x3c ||
	         CurrentInstruction == 0x5c ||
	         CurrentInstruction == 0x7c ||
	         CurrentInstruction == 0xbc ||
	         CurrentInstruction == 0xbe ||
	         CurrentInstruction == 0xbf ||
	         CurrentInstruction == 0xdc ||
	         CurrentInstruction == 0xfc)
	{
		Cycles++;
	}
}

/*----------------------------------------------------------------------------*/
void DoIntCheck(void)
{
	if (!IntDue)
	{
		IntDue = (intStatus != 0);
		if (!IntDue)
		{
			CyclesToInt = NO_TIMER_INT_DUE;
		}
		else if (CyclesToInt == NO_TIMER_INT_DUE)
		{
			// Non-timer interrupt has occurred
			CyclesToInt = 0;
		}
	}
}
/*----------------------------------------------------------------------------*/
// IO read + write take extra cycle & require sync with 1MHz clock (taken
// from Model-b - have not seen this documented anywhere)
void SyncIO(void)
{
	if ((TotalCycles+Cycles) & 1)
	{
		Cycles++;
		IOCycles = 1;
		PollVIAs(1);
	}
	else
	{
		IOCycles = 0;
	}
}
void AdjustForIORead(void)
{
	Cycles++;
	IOCycles += 1;
	PollVIAs(1);
}
void AdjustForIOWrite(void)
{
	Cycles++;
	IOCycles += 1;
	PollVIAs(1);
	DoIntCheck();
}

/*----------------------------------------------------------------------------*/

static void AdvanceCyclesForMemRead()
{
	// Advance VIAs to point where mem read happens
	Cycles += CyclesToMemRead[CurrentInstruction];
	PollVIAs(CyclesToMemRead[CurrentInstruction]);

	// Check if interrupt should be taken if instruction does
	// a read but not a write (write instructions checked below).
	if (CyclesToMemRead[CurrentInstruction] != 0 &&
		CyclesToMemWrite[CurrentInstruction] == 0)
	{
		DoIntCheck();
	}
}

static void AdvanceCyclesForMemWrite()
{
	// Advance VIAs to point where mem write happens
	Cycles += CyclesToMemWrite[CurrentInstruction];
	PollVIAs(CyclesToMemWrite[CurrentInstruction]);

	DoIntCheck();
}

/*----------------------------------------------------------------------------*/
/* Set the Z flag if 'in' is 0, and N if bit 7 is set - leave all other bits  */
/* untouched.                                                                 */
INLINE static void SetPSRZN(const unsigned char in) {
  PSR&=~(FlagZ | FlagN);
  PSR|=((in==0)<<1) | (in & 128);
}

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
void DumpRegs(void) {
  static const char FlagNames[]="CZIDB-VNczidb-vn";

  fprintf(stderr,"  PC=0x%x A=0x%x X=0x%x Y=0x%x S=0x%x PSR=0x%x=",
    ProgramCounter,Accumulator,XReg,YReg,StackReg,PSR);
  for (int FlagNum = 0; FlagNum < 8; FlagNum++)
    fputc(FlagNames[FlagNum+8*((PSR & (1<<FlagNum))==0)],stderr);
  fputc('\n',stderr);
} /* DumpRegs */

/*----------------------------------------------------------------------------*/
INLINE static void Push(unsigned char ToPush) {
  BEEBWRITEMEM_DIRECT(0x100+StackReg,ToPush);
  StackReg--;
} /* Push */

/*----------------------------------------------------------------------------*/
INLINE static unsigned char Pop(void) {
  StackReg++;
  return(WholeRam[0x100+StackReg]);
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

// Relative addressing mode handler

INLINE static int16 RelAddrModeHandler_Data() {
	// For branches - is this correct - i.e. is the program counter incremented
	// at the correct time?
	int EffectiveAddress = (signed char)ReadPaged(ProgramCounter++);
	EffectiveAddress += ProgramCounter;

	return EffectiveAddress;
}

/*----------------------------------------------------------------------------*/
INLINE static void ADCInstrHandler(int16 operand) {
  /* NOTE! Not sure about C and V flags */
  if (!GETDFLAG) {
    int TmpResultC = Accumulator + operand + GETCFLAG;
    int TmpResultV = (signed char)Accumulator + (signed char)operand + GETCFLAG;
    Accumulator = TmpResultC & 255;
    SetPSR(FlagC | FlagZ | FlagV | FlagN, (TmpResultC & 256) > 0,
      Accumulator == 0, 0, 0, 0, ((Accumulator & 128) > 0) ^ (TmpResultV < 0),
      Accumulator & 128);
  } else {
    /* Z flag determined from 2's compl result, not BCD result! */
    int TmpResult = Accumulator + operand + GETCFLAG;
    int ZFlag = (TmpResult & 0xff) == 0;

    int ln = (Accumulator & 0xf) + (operand & 0xf) + GETCFLAG;

    int TmpCarry = 0;

    if (ln > 9) {
      ln += 6;
      ln &= 0xf;
      TmpCarry = 0x10;
    }

    int hn = (Accumulator & 0xf0) + (operand & 0xf0) + TmpCarry;
    /* N and V flags are determined before high nibble is adjusted.
       NOTE: V is not always correct */
    int NFlag = hn & 128;
    int VFlag = (hn ^ Accumulator) & 128 && !((Accumulator ^ operand) & 128);

    int CFlag = 0;

    if (hn > 0x90) {
      hn += 0x60;
      hn &= 0xf0;
      CFlag = 1;
    }

    Accumulator = hn | ln;

    if (MachineType == Model::Master128) {
      ZFlag = Accumulator == 0;
      NFlag = Accumulator & 128;
    }

    SetPSR(FlagC | FlagZ | FlagV | FlagN, CFlag, ZFlag, 0, 0, 0, VFlag, NFlag);
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
  oldVal=ReadPaged(address);
  Cycles+=1;
  PollVIAs(1);
  WritePaged(address,oldVal);
  newVal=(((unsigned int)oldVal)<<1) & 254;
  Cycles+=CyclesToMemWrite[CurrentInstruction] - 1;
  PollVIAs(CyclesToMemWrite[CurrentInstruction] - 1);
  WritePaged(address,newVal);
  SetPSRCZN((oldVal & 128)>0, newVal==0,newVal & 128);
} /* ASLInstrHandler */

INLINE static void TRBInstrHandler(int16 address) {
	unsigned char oldVal,newVal;
	oldVal=ReadPaged(address);
	newVal=(Accumulator ^ 255) & oldVal;
    WritePaged(address,newVal);
    PSR&=253;
	PSR|=((Accumulator & oldVal)==0) ? 2 : 0;
} // TRBInstrHandler

INLINE static void TSBInstrHandler(int16 address) {
	unsigned char oldVal,newVal;
	oldVal=ReadPaged(address);
	newVal=Accumulator | oldVal;
    WritePaged(address,newVal);
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
    ProgramCounter=RelAddrModeHandler_Data();
    Branched = true;
  } else ProgramCounter++;
} /* BCCInstrHandler */

INLINE static void BCSInstrHandler(void) {
  if (GETCFLAG) {
    ProgramCounter=RelAddrModeHandler_Data();
    Branched = true;
  } else ProgramCounter++;
} /* BCSInstrHandler */

INLINE static void BEQInstrHandler(void) {
  if (GETZFLAG) {
    ProgramCounter=RelAddrModeHandler_Data();
    Branched = true;
  } else ProgramCounter++;
} /* BEQInstrHandler */

INLINE static void BITInstrHandler(int16 operand) {
  PSR&=~(FlagZ | FlagN | FlagV);
  /* z if result 0, and NV to top bits of operand */
  PSR|=(((Accumulator & operand)==0)<<1) | (operand & 192);
} /* BITInstrHandler */

INLINE static void BITImmedInstrHandler(int16 operand) {
  PSR&=~FlagZ;
  /* z if result 0, and NV to top bits of operand */
  PSR|=(((Accumulator & operand)==0)<<1);
} /* BITImmedInstrHandler */

INLINE static void BMIInstrHandler(void) {
  if (GETNFLAG) {
    ProgramCounter=RelAddrModeHandler_Data();
    Branched = true;
  } else ProgramCounter++;
} /* BMIInstrHandler */

INLINE static void BNEInstrHandler(void) {
  if (!GETZFLAG) {
    ProgramCounter=RelAddrModeHandler_Data();
    Branched = true;
  } else ProgramCounter++;
} /* BNEInstrHandler */

INLINE static void BPLInstrHandler(void) {
  if (!GETNFLAG) {
    ProgramCounter=RelAddrModeHandler_Data();
    Branched = true;
  } else ProgramCounter++;
}

INLINE static void BRKInstrHandler(void) {
  PushWord(ProgramCounter+1);
  SetPSR(FlagB,0,0,0,0,1,0,0); /* Set B before pushing */
  Push(PSR);
  SetPSR(FlagI,0,0,1,0,0,0,0); /* Set I after pushing - see Birnbaum */
  ProgramCounter=BeebReadMem(0xfffe) | (BeebReadMem(0xffff)<<8);
} /* BRKInstrHandler */

INLINE static void BVCInstrHandler(void) {
  if (!GETVFLAG) {
    ProgramCounter=RelAddrModeHandler_Data();
    Branched = true;
  } else ProgramCounter++;
} /* BVCInstrHandler */

INLINE static void BVSInstrHandler(void) {
  if (GETVFLAG) {
    ProgramCounter=RelAddrModeHandler_Data();
    Branched = true;
  } else ProgramCounter++;
} /* BVSInstrHandler */

INLINE static void BRAInstrHandler(void) {
    ProgramCounter=RelAddrModeHandler_Data();
    Branched = true;
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
  val=ReadPaged(address);
  Cycles+=1;
  PollVIAs(1);
  WritePaged(address,val);
  val=(val-1);
  Cycles+=CyclesToMemWrite[CurrentInstruction] - 1;
  PollVIAs(CyclesToMemWrite[CurrentInstruction] - 1);
  WritePaged(address,val);
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
  val=ReadPaged(address);
  Cycles+=1;
  PollVIAs(1);
  WritePaged(address,val);
  val=(val+1) & 255;
  Cycles+=CyclesToMemWrite[CurrentInstruction] - 1;
  PollVIAs(CyclesToMemWrite[CurrentInstruction] - 1);
  WritePaged(address,val);
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
  PushWord(ProgramCounter-1);
  ProgramCounter=address;
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
  oldVal=ReadPaged(address);
  Cycles+=1;
  PollVIAs(1);
  WritePaged(address,oldVal);
  newVal=(((unsigned int)oldVal)>>1) & 127;
  Cycles+=CyclesToMemWrite[CurrentInstruction] - 1;
  PollVIAs(CyclesToMemWrite[CurrentInstruction] - 1);
  WritePaged(address,newVal);
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
  oldVal=ReadPaged(address);
  Cycles+=1;
  PollVIAs(1);
  WritePaged(address,oldVal);
  newVal=((unsigned int)oldVal<<1) & 254;
  newVal+=GETCFLAG;
  Cycles+=CyclesToMemWrite[CurrentInstruction] - 1;
  PollVIAs(CyclesToMemWrite[CurrentInstruction] - 1);
  WritePaged(address,newVal);
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
  oldVal=ReadPaged(address);
  Cycles+=1;
  PollVIAs(1);
  WritePaged(address,oldVal);
  newVal=((unsigned int)oldVal>>1) & 127;
  newVal+=GETCFLAG*128;
  Cycles+=CyclesToMemWrite[CurrentInstruction] - 1;
  PollVIAs(CyclesToMemWrite[CurrentInstruction] - 1);
  WritePaged(address,newVal);
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
  if (!GETDFLAG) {
    int TmpResultV = (signed char)Accumulator - (signed char)operand - (1 - GETCFLAG);
    int TmpResultC = Accumulator - operand - (1 - GETCFLAG);
    Accumulator = TmpResultC & 255;
    SetPSR(FlagC | FlagZ | FlagV | FlagN, TmpResultC >= 0,
      Accumulator == 0, 0, 0, 0,
      ((Accumulator & 128) > 0) ^ ((TmpResultV & 256) != 0),
      Accumulator & 128);
  } else {
    if (MachineType == Model::Master128) {
      int ohn = operand & 0xf0;
      int oln = operand & 0x0f;

      int ln = (Accumulator & 0xf) - oln - (1 - GETCFLAG);
      int TmpResult = Accumulator - operand - (1 - GETCFLAG);

      int TmpResultV = (signed char)Accumulator - (signed char)operand - (1 - GETCFLAG);
      int VFlag = ((TmpResultV < -128) || (TmpResultV > 127));

      int CFlag = (TmpResult & 256) == 0;

      if (TmpResult < 0) {
        TmpResult -= 0x60;
      }

      if (ln < 0) {
        TmpResult -= 0x06;
      }

      int NFlag = TmpResult & 128;
      Accumulator = TmpResult & 0xFF;
      int ZFlag = (Accumulator == 0);

      SetPSR(FlagC | FlagZ | FlagV | FlagN, CFlag, ZFlag, 0, 0, 0, VFlag, NFlag);
    } else {
      /* Z flag determined from 2's compl result, not BCD result! */
      int TmpResult = Accumulator - operand - (1 - GETCFLAG);
      int ZFlag = ((TmpResult & 0xff) == 0);

      int ohn = operand & 0xf0;
      int oln = operand & 0xf;

      int ln = (Accumulator & 0xf) - oln - (1 - GETCFLAG);
      if (ln & 0x10) {
        ln -= 6;
      }

      int TmpCarry = 0;

      if (ln & 0x20) {
        TmpCarry = 0x10;
      }

      ln &= 0xf;
      int hn = (Accumulator & 0xf0) - ohn - TmpCarry;
      /* N and V flags are determined before high nibble is adjusted.
         NOTE: V is not always correct */
      int NFlag = hn & 128;

      int TmpResultV = (signed char)Accumulator - (signed char)operand - (1 - GETCFLAG);
      int VFlag = ((TmpResultV < -128) || (TmpResultV > 127));

      int CFlag = 1;

      if (hn & 0x100) {
        hn -= 0x60;
        hn &= 0xf0;
        CFlag = 0;
      }

      Accumulator = hn | ln;

      SetPSR(FlagC | FlagZ | FlagV | FlagN, CFlag, ZFlag, 0, 0, 0, VFlag, NFlag);
    }
  }
} /* SBCInstrHandler */

INLINE static void STXInstrHandler(int16 address) {
  WritePaged(address,XReg);
} /* STXInstrHandler */

INLINE static void STYInstrHandler(int16 address) {
  WritePaged(address,YReg);
} /* STYInstrHandler */

// KIL (Halt) instruction handler.

INLINE static void KILInstrHandler() {
	// Just repeat the instruction indefinitely.
	ProgramCounter--;
}

/*-------------------------------------------------------------------------*/
/* Absolute  addressing mode handler                                       */
INLINE static int16 AbsAddrModeHandler_Data(void) {
  int FullAddress;

  /* Get the address from after the instruction */

  GETTWOBYTEFROMPC(FullAddress)

  /* And then read it */
  return(ReadPaged(FullAddress));
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
  return(ReadPaged(ProgramCounter++));
} /* ZeroPgAddrModeHandler_Address */

/*-------------------------------------------------------------------------*/
/* Indexed with X preinc addressing mode handler                           */
INLINE static int16 IndXAddrModeHandler_Data(void) {
  unsigned char ZeroPageAddress;
  int EffectiveAddress;

  ZeroPageAddress=(ReadPaged(ProgramCounter++)+XReg) & 255;

  EffectiveAddress=WholeRam[ZeroPageAddress] | (WholeRam[ZeroPageAddress+1]<<8);
  return(ReadPaged(EffectiveAddress));
} /* IndXAddrModeHandler_Data */

/*-------------------------------------------------------------------------*/
/* Indexed with X preinc addressing mode handler                           */
INLINE static int16 IndXAddrModeHandler_Address() {
  unsigned char ZeroPageAddress = (ReadPaged(ProgramCounter++) + XReg) & 0xff;
  int EffectiveAddress = WholeRam[ZeroPageAddress] | (WholeRam[ZeroPageAddress + 1] << 8);

  return EffectiveAddress;
}

/*-------------------------------------------------------------------------*/
/* Indexed with Y postinc addressing mode handler                          */
INLINE static int16 IndYAddrModeHandler_Data(void) {
  uint8_t ZPAddr=ReadPaged(ProgramCounter++);
  uint16_t EffectiveAddress=WholeRam[ZPAddr]+YReg;
  if (EffectiveAddress>0xff) Carried();
  EffectiveAddress+=(WholeRam[(uint8_t)(ZPAddr+1)]<<8);

  return(ReadPaged(EffectiveAddress));
} /* IndYAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Indexed with Y postinc addressing mode handler                          */
INLINE static int16 IndYAddrModeHandler_Address(void) {
  uint8_t ZPAddr=ReadPaged(ProgramCounter++);
  uint16_t EffectiveAddress=WholeRam[ZPAddr]+YReg;
  if (EffectiveAddress>0xff) Carried();
  EffectiveAddress+=(WholeRam[(uint8_t)(ZPAddr+1)]<<8);

  return(EffectiveAddress);
} /* IndYAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Zero page wih X offset addressing mode handler                          */
INLINE static int16 ZeroPgXAddrModeHandler_Data(void) {
  int EffectiveAddress;
  EffectiveAddress=(ReadPaged(ProgramCounter++)+XReg) & 255;
  return(WholeRam[EffectiveAddress]);
} /* ZeroPgXAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Zero page wih X offset addressing mode handler                          */
INLINE static int16 ZeroPgXAddrModeHandler_Address(void) {
  int EffectiveAddress;
  EffectiveAddress=(ReadPaged(ProgramCounter++)+XReg) & 255;
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

  return(ReadPaged(EffectiveAddress));
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

  return(ReadPaged(EffectiveAddress));
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
  if ((VectorLocation & 0xff) != 0xff || MachineType == Model::Master128) {
   EffectiveAddress=ReadPaged(VectorLocation);
   EffectiveAddress|=ReadPaged(VectorLocation+1) << 8;
  }
  else {
   EffectiveAddress=ReadPaged(VectorLocation);
   EffectiveAddress|=ReadPaged(VectorLocation-255) << 8;
  }
  return(EffectiveAddress);
} /* IndAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Zero page Indirect addressing mode handler                                        */
INLINE static int16 ZPIndAddrModeHandler_Address(void) {
  int VectorLocation;
  int EffectiveAddress;

  VectorLocation=ReadPaged(ProgramCounter++);
  EffectiveAddress=ReadPaged(VectorLocation)+(ReadPaged(VectorLocation+1)<<8);

   // EffectiveAddress|=ReadPaged(VectorLocation+1) << 8; }
  return(EffectiveAddress);
} /* ZPIndAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Zero page Indirect addressing mode handler                                        */
INLINE static int16 ZPIndAddrModeHandler_Data(void) {
  int VectorLocation;
  int EffectiveAddress;

  VectorLocation=ReadPaged(ProgramCounter++);
  EffectiveAddress=ReadPaged(VectorLocation)+(ReadPaged(VectorLocation+1)<<8);

   // EffectiveAddress|=ReadPaged(VectorLocation+1) << 8; }
  return(ReadPaged(EffectiveAddress));
} /* ZPIndAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Pre-indexed absolute Indirect addressing mode handler                                        */
INLINE static int16 IndAddrXModeHandler_Address(void) {
  /* For jump indirect only */
  int VectorLocation;
  int EffectiveAddress;

  GETTWOBYTEFROMPC(VectorLocation)

  EffectiveAddress=ReadPaged(VectorLocation+XReg);
  EffectiveAddress|=ReadPaged(VectorLocation+1+XReg) << 8;
  EffectiveAddress&=0xffff;
   // EffectiveAddress|=ReadPaged(VectorLocation+1) << 8; }
  return(EffectiveAddress);
} /* ZPIndAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Zero page with Y offset addressing mode handler                         */
INLINE static int16 ZeroPgYAddrModeHandler_Data(void) {
  int EffectiveAddress;
  EffectiveAddress=(ReadPaged(ProgramCounter++)+YReg) & 255;
  return(WholeRam[EffectiveAddress]);
} /* ZeroPgYAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Zero page with Y offset addressing mode handler                         */
INLINE static int16 ZeroPgYAddrModeHandler_Address(void) {
  int EffectiveAddress;
  EffectiveAddress=(ReadPaged(ProgramCounter++)+YReg) & 255;
  return(EffectiveAddress);
} /* ZeroPgYAddrModeHandler */

/*-------------------------------------------------------------------------*/

// Initialise 6502core

void Init6502core()
{
	if (MachineType == Model::Master128) {
		CyclesTable = CyclesTable65C02;
		CyclesToMemRead = CyclesToMemRead65C02;
		CyclesToMemWrite = CyclesToMemWrite65C02;
	}
	else {
		CyclesTable = CyclesTable6502;
		CyclesToMemRead = CyclesToMemRead6502;
		CyclesToMemWrite = CyclesToMemWrite6502;
	}

	ProgramCounter = BeebReadMem(0xfffc) | (BeebReadMem(0xfffd) << 8);

	// For consistancy of execution
	Accumulator = 0;
	XReg = 0;
	YReg = 0;
	StackReg = 0xff; // Initial value?
	PSR = FlagI; // Interrupts off for starters

	intStatus = 0;
	NMIStatus = 0;
	NMILock = false;
}

/*-------------------------------------------------------------------------*/
void DoInterrupt(void) {
  PushWord(ProgramCounter);
  Push(PSR & ~FlagB);
  ProgramCounter=BeebReadMem(0xfffe) | (BeebReadMem(0xffff)<<8);
  SetPSR(FlagI,0,0,1,0,0,0,0);
  IRQCycles=7;
} /* DoInterrupt */

/*-------------------------------------------------------------------------*/
void DoNMI(void) {
  /*cerr << "Doing NMI\n"; */
  NMILock = true;
  PushWord(ProgramCounter);
  Push(PSR);
  ProgramCounter=BeebReadMem(0xfffa) | (BeebReadMem(0xfffb)<<8);
  SetPSR(FlagI,0,0,1,0,0,0,0); /* Normal interrupts should be disabled during NMI ? */
  IRQCycles=7;
} /* DoNMI */

static void Dis6502()
{
	char str[256];

	int Length = DebugDisassembleInstructionWithCPUStatus(
		ProgramCounter, true, Accumulator, XReg, YReg, StackReg, PSR, str
	);

	str[Length] = '\n';
	str[Length + 1] = '\0';

	WriteLog(str);
}

void MemoryDump6502(int addr, int count)
{
	char info[80];

	int s = addr & 0xffff0;
	int e = (addr + count - 1) | 0xf;

	if (e > 0xfffff)
		e = 0xfffff;

	for (int a = s; a < e; a += 16)
	{
		sprintf(info, "%04X  ", a);

		for (int b = 0; b < 16; ++b)
		{
			sprintf(info+strlen(info), "%02X ", DebugReadMem(a+b, true));
		}

		for (int b = 0; b < 16; ++b)
		{
			unsigned char v = DebugReadMem(a+b, true);
			if (v < 32 || v > 127)
				v = '.';
			sprintf(info+strlen(info), "%c", v);
		}

		WriteLog("%s\n", info);
	}
}

/*-------------------------------------------------------------------------*/

// The routine indirected through the REMV vector is used by the operating
// system to remove a character from a buffer, or to examine the buffer only.
//
// On entry, X=buffer number. (0 is the keyboard buffer)
//
// The overflow flag is set if only an examination is needed.
//
// If the buffer is only examined, the next character to be withdrawn from the
// buffer is returned, but not removed, hence no buffer empty event can be
// caused.
//
// If the last character is removed, a buffer empty event will be caused.
//
// On exit,
//   A is the next character to be removed, for the examine option,
//   undefined otherwise.
//   X is preserved.
//   Y is the character removed for the remove option.
//   C is set if the buffer was empty on entry.

static void ClipboardREMVHandler()
{
	if (GETVFLAG) {
		// Examine buffer state

		if (mainWin->m_ClipboardIndex < mainWin->m_ClipboardLength) {
			Accumulator = mainWin->m_ClipboardBuffer[mainWin->m_ClipboardIndex];
			PSR &= ~FlagC;
		}
		else {
			PSR |= FlagC;
		}
	}
	else {
		// Remove character from buffer
		if (mainWin->m_ClipboardIndex < mainWin->m_ClipboardLength) {
			unsigned char c = mainWin->m_ClipboardBuffer[mainWin->m_ClipboardIndex++];

			if (c == 0xa3) {
				// Convert pound sign
				c = 0x60;
			}
			else if (mainWin->m_translateCRLF) {
				if (c == 0x0a) {
					// Convert LF to CR
					c = 0x0d;
				}
				else if (c == 0x0d && mainWin->m_ClipboardBuffer[mainWin->m_ClipboardIndex] == 0x0a) {
					// Drop LF after CR
					mainWin->m_ClipboardIndex++;
				}
			}

			Accumulator = c;
			YReg = c;
			PSR &= ~FlagC;
		}
		else {
			// We've reached the end of the clipboard contents
			mainWin->ClearClipboardBuffer();

			PSR |= FlagC;
		}
	}

	CurrentInstruction = 0x60; // RTS
}

// The routine indirected through the CNPV vector is used by the operating
// system to count the entries in a buffer or to purge the contents of a buffer.
//
// On entry,
//   X=buffer number (0 is the keyboard buffer)
//
// The overflow flag is set if the buffer is to be purged.
//
// The overflow flag is clear if the buffer is to be counted.
//
// For a count operation, if the carry flag is set, the amount of space left
// in the buffer is returned, otherwise the number of entries in the buffer
// is returned.
//
// On exit,
//   For purge: X and Y are preserved
//   For count: X=low byte of result, Y=high byte of result
//   A is undefined
//   V,C are preserved

static void ClipboardCNPVHandler()
{
	if (GETVFLAG) {
		mainWin->ClearClipboardBuffer();
	}
	else {
		if (GETCFLAG) {
			XReg = 0;
			YReg = 0;
		}
		else {
			int Length = mainWin->m_ClipboardLength - mainWin->m_ClipboardIndex;
			XReg = Length > 0;
			YReg = 0;
		}
	}

	CurrentInstruction = 0x60; // RTS
}

/*-------------------------------------------------------------------------*/
/* Execute one 6502 instruction, move program counter on                   */
void Exec6502Instruction(void) {
	static unsigned char OldNMIStatus;
	int OldPC;
	bool iFlagJustCleared;
	bool iFlagJustSet;

	const int Count = DebugEnabled ? 1 : 1024; // Makes debug window more responsive

	for (int i = 0; i < Count; i++) {
		// Output debug info
		if (DebugEnabled && !DebugDisassembler(ProgramCounter, PrePC, Accumulator, XReg, YReg, PSR, StackReg, true))
		{
			Sleep(10);  // Ease up on CPU when halted
			continue;
		}

		if (trace)
		{
			Dis6502();
		}

		Branched = false;
		iFlagJustCleared = false;
		iFlagJustSet = false;
		Cycles = 0;
		IOCycles = 0;
		IntDue = false;
		CurrentInstruction = -1;

		OldPC = ProgramCounter;
		PrePC = ProgramCounter;

		// Check for WRCHV, send char to speech output
		if (mainWin->m_TextToSpeechEnabled &&
			ProgramCounter == (WholeRam[0x20e] | (WholeRam[0x20f] << 8))) {
			mainWin->SpeakChar(Accumulator);
		}
		else if (mainWin->m_ClipboardBuffer[0] != '\0') {
			// Check for REMV (Remove from buffer vector) and CNPV (Count/purge buffer
			// vector). X register contains the buffer number (0 indicates the keyboard
			// buffer). See AUG p.263/264 and p.138

			if (ProgramCounter == (WholeRam[0x22c] | (WholeRam[0x22d] << 8)) && XReg == 0) {
				ClipboardREMVHandler();
			}
			else if (ProgramCounter == (WholeRam[0x22e] | (WholeRam[0x22f] << 8)) && XReg == 0) {
				ClipboardCNPVHandler();
			}
		}

		if (CurrentInstruction == -1) {
			// Read an instruction and post inc program counter
			CurrentInstruction = ReadPaged(ProgramCounter++);
		}

		InstructionCount[CurrentInstruction]++;

		// Advance VIAs to point where mem read happens
		ViaCycles=0;
		AdvanceCyclesForMemRead();

		switch (CurrentInstruction) {
			case 0x00:
				// BRK
				BRKInstrHandler();
				break;
			case 0x01:
				// ORA (zp,X)
				ORAInstrHandler(IndXAddrModeHandler_Data());
				break;
			case 0x02:
			case 0x22:
			case 0x42:
			case 0x62:
				if (MachineType == Model::Master128) {
					// NOP imm
					ReadPaged(ProgramCounter++);
				}
				else {
					// Undocumented instruction: KIL
					KILInstrHandler();
				}
				break;
			case 0x03:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: SLO (zp,X)
					int16 zpaddr = IndXAddrModeHandler_Address();
					ASLInstrHandler(zpaddr);
					ORAInstrHandler(WholeRam[zpaddr]);
				}
				break;
			case 0x04:
				if (MachineType == Model::Master128) {
					// TSB zp
					TSBInstrHandler(ZeroPgAddrModeHandler_Address());
				}
				else {
					// Undocumented instruction: NOP zp
					ReadPaged(ZeroPgAddrModeHandler_Address());
				}
				break;
			case 0x05:
				// ORA zp
				ORAInstrHandler(WholeRam[ZeroPgAddrModeHandler_Address()]);
				break;
			case 0x06:
				// ASL zp
				ASLInstrHandler(ZeroPgAddrModeHandler_Address());
				break;
			case 0x07:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: SLO zp
					int16 zpaddr = ZeroPgAddrModeHandler_Address();
					ASLInstrHandler(zpaddr);
					ORAInstrHandler(WholeRam[zpaddr]);
				}
				break;
			case 0x08:
				// PHP
				Push(PSR | 48);
				break;
			case 0x09:
				// ORA imm
				ORAInstrHandler(ReadPaged(ProgramCounter++));
				break;
			case 0x0a:
				// ASL A
				ASLInstrHandler_Acc();
				break;
			case 0x0b:
			case 0x2b:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: ANC imm
					ANDInstrHandler(WholeRam[ProgramCounter++]);
					PSR |= ((Accumulator & 128) >> 7);
				}
				break;
			case 0x0c:
				if (MachineType == Model::Master128) {
					// TSB abs
					TSBInstrHandler(AbsAddrModeHandler_Address());
				}
				else {
					// Undocumented instruction: NOP abs
					AbsAddrModeHandler_Address();
				}
				break;
			case 0x0d:
				// ORA abs
				ORAInstrHandler(AbsAddrModeHandler_Data());
				break;
			case 0x0e:
				// ASL abs
				ASLInstrHandler(AbsAddrModeHandler_Address());
				break;
			case 0x0f:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: SLO abs
					int16 zpaddr = AbsAddrModeHandler_Address();
					ASLInstrHandler(zpaddr);
					ORAInstrHandler(WholeRam[zpaddr]);
				}
				break;
			case 0x10:
				// BPL rel
				BPLInstrHandler();
				break;
			case 0x11:
				// ORA (zp),Y
				ORAInstrHandler(IndYAddrModeHandler_Data());
				break;
			case 0x12:
				if (MachineType == Model::Master128) {
					// ORA (zp)
					ORAInstrHandler(ZPIndAddrModeHandler_Data());
				}
				else {
					// Undocumented instruction: KIL
					KILInstrHandler();
				}
				break;
			case 0x13:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: SLO (zp),Y
					int16 zpaddr = IndYAddrModeHandler_Address();
					ASLInstrHandler(zpaddr);
					ORAInstrHandler(WholeRam[zpaddr]);
				}
				break;
			case 0x14:
				if (MachineType == Model::Master128) {
					// TRB zp
					TRBInstrHandler(ZeroPgAddrModeHandler_Address());
				}
				else {
					// Undocumented instruction: NOP zp,X
					ZeroPgXAddrModeHandler_Address();
				}
				break;
			case 0x15:
				// ORA zp,X
				ORAInstrHandler(ZeroPgXAddrModeHandler_Data());
				break;
			case 0x16:
				// ASL zp,X
				ASLInstrHandler(ZeroPgXAddrModeHandler_Address());
				break;
			case 0x17:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: SLO zp,X
					int16 zpaddr = ZeroPgXAddrModeHandler_Address();
					ASLInstrHandler(zpaddr);
					ORAInstrHandler(WholeRam[zpaddr]);
				}
				break;
			case 0x18:
				// CLC
				PSR &= 255 - FlagC;
				break;
			case 0x19:
				// ORA abs,Y
				ORAInstrHandler(AbsYAddrModeHandler_Data());
				break;
			case 0x1a:
				if (MachineType == Model::Master128) {
					// INC A
					INAInstrHandler();
				}
				else {
					// Undocumented instruction: NOP
				}
				break;
			case 0x1b:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: SLO abs,Y
					int16 zpaddr = AbsYAddrModeHandler_Address();
					ASLInstrHandler(zpaddr);
					ORAInstrHandler(WholeRam[zpaddr]);
				}
				break;
			case 0x1c:
				if (MachineType == Model::Master128) {
					// TRB abs
					TRBInstrHandler(AbsAddrModeHandler_Address());
				}
				else {
					// Undocumented instruction: NOP abs,X
					AbsXAddrModeHandler_Data();
				}
				break;
			case 0x1d:
				// ORA abs,X
				ORAInstrHandler(AbsXAddrModeHandler_Data());
				break;
			case 0x1e:
				// ASL abs,X
				ASLInstrHandler(AbsXAddrModeHandler_Address());
				break;
			case 0x1f:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: SLO abs,X
					int16 zpaddr = AbsXAddrModeHandler_Address();
					ASLInstrHandler(zpaddr);
					ORAInstrHandler(WholeRam[zpaddr]);
				}
				break;
			case 0x20:
				// JSR abs
				JSRInstrHandler(AbsAddrModeHandler_Address());
				break;
			case 0x21:
				// AND (zp,X)
				ANDInstrHandler(IndXAddrModeHandler_Data());
				break;
			case 0x23:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: RLA (zp,X)
					int16 zpaddr = IndXAddrModeHandler_Address();
					ROLInstrHandler(zpaddr);
					ANDInstrHandler(WholeRam[zpaddr]);
				}
				break;
			case 0x24:
				// BIT zp
				BITInstrHandler(WholeRam[ReadPaged(ProgramCounter++)]);
				break;
			case 0x25:
				// AND zp
				ANDInstrHandler(WholeRam[ReadPaged(ProgramCounter++)]);
				break;
			case 0x26:
				// ROL zp
				ROLInstrHandler(ZeroPgAddrModeHandler_Address());
				break;
			case 0x27:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: RLA zp
					int16 zpaddr = ZeroPgAddrModeHandler_Address();
					ROLInstrHandler(zpaddr);
					ANDInstrHandler(WholeRam[zpaddr]);
				}
				break;
			case 0x28: {
					// PLP
					unsigned char oldPSR = PSR;
					PSR = Pop();

					if ((oldPSR ^ PSR) & FlagI) {
						if (PSR & FlagI) {
							iFlagJustSet = true;
						}
						else {
							iFlagJustCleared = true;
						}
					}
				}
				break;
			case 0x29:
				// AND imm
				ANDInstrHandler(ReadPaged(ProgramCounter++));
				break;
			case 0x2a:
				// ROL A
				ROLInstrHandler_Acc();
				break;
			case 0x2c:
				// BIT abs
				BITInstrHandler(AbsAddrModeHandler_Data());
				break;
			case 0x2d:
				// AND abs
				ANDInstrHandler(AbsAddrModeHandler_Data());
				break;
			case 0x2e:
				// ROL abs
				ROLInstrHandler(AbsAddrModeHandler_Address());
				break;
			case 0x2f:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: RLA abs
					int16 zpaddr = AbsAddrModeHandler_Address();
					ROLInstrHandler(zpaddr);
					ANDInstrHandler(WholeRam[zpaddr]);
				}
				break;
			case 0x30:
				// BMI rel
				BMIInstrHandler();
				break;
			case 0x31:
				// AND (zp),Y
				ANDInstrHandler(IndYAddrModeHandler_Data());
				break;
			case 0x32:
				if (MachineType == Model::Master128) {
					// AND (zp)
					ANDInstrHandler(ZPIndAddrModeHandler_Data());
				}
				else {
					// Undocumented instruction: KIL
					KILInstrHandler();
				}
				break;
			case 0x33:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: RLA (zp),Y
					int16 zpaddr = IndYAddrModeHandler_Address();
					ROLInstrHandler(zpaddr);
					ANDInstrHandler(WholeRam[zpaddr]);
				}
				break;
			case 0x34:
				if (MachineType == Model::Master128) {
					// BIT abs,X
					BITInstrHandler(ZeroPgXAddrModeHandler_Data());
				}
				else {
					// Undocumented instruction: NOP zp,X
					ZeroPgXAddrModeHandler_Address();
				}
				break;
			case 0x35:
				// AND zp,X
				ANDInstrHandler(ZeroPgXAddrModeHandler_Data());
				break;
			case 0x36:
				// ROL zp,X
				ROLInstrHandler(ZeroPgXAddrModeHandler_Address());
				break;
			case 0x37:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: RLA zp,X
					int16 zpaddr = ZeroPgXAddrModeHandler_Address();
					ROLInstrHandler(zpaddr);
					ANDInstrHandler(WholeRam[zpaddr]);
				}
				break;
			case 0x38:
				// SEC
				PSR |= FlagC;
				break;
			case 0x39:
				// AND abs,Y
				ANDInstrHandler(AbsYAddrModeHandler_Data());
				break;
			case 0x3a:
				if (MachineType == Model::Master128) {
					// DEC A
					DEAInstrHandler();
				}
				else {
					// Undocumented instruction: NOP
				}
				break;
			case 0x3b:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: RLA abs.Y
					int16 zpaddr = AbsYAddrModeHandler_Address();
					ROLInstrHandler(zpaddr);
					ANDInstrHandler(WholeRam[zpaddr]);
				}
				break;
			case 0x3c:
				if (MachineType == Model::Master128) {
					// BIT abs,X
					BITInstrHandler(AbsXAddrModeHandler_Data());
				}
				else {
					// Undocumented instruction: NOP abs,x
					AbsXAddrModeHandler_Data();
				}
				break;
			case 0x3d:
				// AND abs,X
				ANDInstrHandler(AbsXAddrModeHandler_Data());
				break;
			case 0x3e:
				// ROL abs,X
				ROLInstrHandler(AbsXAddrModeHandler_Address());
				break;
			case 0x3f:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: RLA abs.X
					int16 zpaddr = AbsXAddrModeHandler_Address();
					ROLInstrHandler(zpaddr);
					ANDInstrHandler(WholeRam[zpaddr]);
				}
				break;
			case 0x40:
				// RTI
				PSR = Pop();
				ProgramCounter = PopWord();
				NMILock = false;
				break;
			case 0x41:
				// EOR (zp,X)
				EORInstrHandler(IndXAddrModeHandler_Data());
				break;
			case 0x43:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: SRE (zp,X)
					int16 zpaddr = IndXAddrModeHandler_Address();
					LSRInstrHandler(zpaddr);
					EORInstrHandler(WholeRam[zpaddr]);
				}
				break;
			case 0x44:
				// NOP zp
				ReadPaged(ZeroPgAddrModeHandler_Address());
				break;
			case 0x45:
				// EOR zp
				EORInstrHandler(WholeRam[ReadPaged(ProgramCounter++)]);
				break;
			case 0x46:
				// LSR zp
				LSRInstrHandler(ZeroPgAddrModeHandler_Address());
				break;
			case 0x47:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: SRE zp
					int16 zpaddr = ZeroPgAddrModeHandler_Address();
					LSRInstrHandler(zpaddr);
					EORInstrHandler(WholeRam[zpaddr]);
				}
				break;
			case 0x48:
				// PHA
				Push(Accumulator);
				break;
			case 0x49:
				// EOR imm
				EORInstrHandler(ReadPaged(ProgramCounter++));
				break;
			case 0x4a:
				// LSR A
				LSRInstrHandler_Acc();
				break;
			case 0x4b:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: ALR imm
					ANDInstrHandler(WholeRam[ProgramCounter++]);
					LSRInstrHandler_Acc();
				}
				break;
			case 0x4c:
				// JMP abs
				ProgramCounter = AbsAddrModeHandler_Address();
				break;
			case 0x4d:
				// EOR abs
				EORInstrHandler(AbsAddrModeHandler_Data());
				break;
			case 0x4e:
				// LSR abs
				LSRInstrHandler(AbsAddrModeHandler_Address());
				break;
			case 0x4f:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: SRE abs
					int16 zpaddr = AbsAddrModeHandler_Address();
					LSRInstrHandler(zpaddr);
					EORInstrHandler(WholeRam[zpaddr]);
				}
				break;
			case 0x50:
				// BVC rel
				BVCInstrHandler();
				break;
			case 0x51:
				// EOR (zp),Y
				EORInstrHandler(IndYAddrModeHandler_Data());
				break;
			case 0x52:
				if (MachineType == Model::Master128) {
					// EOR (zp)
					EORInstrHandler(ZPIndAddrModeHandler_Data());
				}
				else {
					// Undocumented instruction: KIL
					KILInstrHandler();
				}
				break;
			case 0x53:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: SRE (zp),Y
					int16 zpaddr = IndYAddrModeHandler_Address();
					LSRInstrHandler(zpaddr);
					EORInstrHandler(WholeRam[zpaddr]);
				}
				break;
			case 0x54:
			case 0xd4:
			case 0xf4:
				// Undocumented instruction: NOP zp,X
				ZeroPgXAddrModeHandler_Address();
				break;
			case 0x55:
				// EOR zp,X
				EORInstrHandler(ZeroPgXAddrModeHandler_Data());
				break;
			case 0x56:
				// LSR zp,X
				LSRInstrHandler(ZeroPgXAddrModeHandler_Address());
				break;
			case 0x57:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: SRE zp,X
					int16 zpaddr = ZeroPgXAddrModeHandler_Address();
					LSRInstrHandler(zpaddr);
					EORInstrHandler(WholeRam[zpaddr]);
				}
				break;
			case 0x58:
				// CLI
				if (PSR & FlagI) {
					iFlagJustCleared = true;
				}
				PSR &= 255 - FlagI;
				break;
			case 0x59:
				// EOR abs,Y
				EORInstrHandler(AbsYAddrModeHandler_Data());
				break;
			case 0x5a:
				if (MachineType == Model::Master128) {
					// PHY
					Push(YReg);
				}
				else {
					// Undocumented instruction: NOP
				}
				break;
			case 0x5b:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: SRE abs,Y
					int16 zpaddr = AbsYAddrModeHandler_Address();
					LSRInstrHandler(zpaddr);
					EORInstrHandler(WholeRam[zpaddr]);
				}
				break;
			case 0x5c:
				if (MachineType == Model::Master128) {
					// NOP abs
					AbsAddrModeHandler_Address();
				}
				else {
					// Undocumented instruction: NOP abs,x
					AbsXAddrModeHandler_Data();
				}
				break;
			case 0x5d:
				// EOR abs,X
				EORInstrHandler(AbsXAddrModeHandler_Data());
				break;
			case 0x5e:
				// LSR abs,X
				LSRInstrHandler(AbsXAddrModeHandler_Address());
				break;
			case 0x5f:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: SRE abs,X
					int16 zpaddr = AbsXAddrModeHandler_Address();
					LSRInstrHandler(zpaddr);
					EORInstrHandler(WholeRam[zpaddr]);
				}
				break;
			case 0x60:
				// RTS
				ProgramCounter = PopWord() + 1;
				break;
			case 0x61:
				// ADC (zp,X)
				ADCInstrHandler(IndXAddrModeHandler_Data());
				break;
			case 0x63:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: RRA (zp,X)
					int16 zpaddr = IndXAddrModeHandler_Address();
					RORInstrHandler(zpaddr);
					ADCInstrHandler(WholeRam[zpaddr]);
				}
				break;
			case 0x64:
				if (MachineType == Model::Master128) {
					// STZ zp
					BEEBWRITEMEM_DIRECT(ZeroPgAddrModeHandler_Address(), 0);
				}
				else {
					// Undocumented instruction: NOP zp
					ReadPaged(ZeroPgAddrModeHandler_Address());
				}
				break;
			case 0x65:
				// ADC zp
				ADCInstrHandler(WholeRam[ReadPaged(ProgramCounter++)]);
				break;
			case 0x66:
				// ROR zp
				RORInstrHandler(ZeroPgAddrModeHandler_Address());
				break;
			case 0x67:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: RRA zp
					int16 zpaddr = ZeroPgAddrModeHandler_Address();
					RORInstrHandler(zpaddr);
					ADCInstrHandler(WholeRam[zpaddr]);
				}
				break;
			case 0x68:
				// PLA
				Accumulator = Pop();
				SetPSRZN(Accumulator);
				break;
			case 0x69:
				// ADC imm
				ADCInstrHandler(ReadPaged(ProgramCounter++));
				break;
			case 0x6a:
				// ROR A
				RORInstrHandler_Acc();
				break;
			case 0x6b:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: ARR imm
					ANDInstrHandler(WholeRam[ProgramCounter++]);
					RORInstrHandler_Acc();
				}
				break;
			case 0x6c:
				// JMP (abs)
				ProgramCounter = IndAddrModeHandler_Address();
				break;
			case 0x6d:
				// ADC abs
				ADCInstrHandler(AbsAddrModeHandler_Data());
				break;
			case 0x6e:
				// ROR abs
				RORInstrHandler(AbsAddrModeHandler_Address());
				break;
			case 0x6f:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: RRA abs
					int16 zpaddr = AbsAddrModeHandler_Address();
					RORInstrHandler(zpaddr);
					ADCInstrHandler(WholeRam[zpaddr]);
				}
				break;
			case 0x70:
				// BVS rel
				BVSInstrHandler();
				break;
			case 0x71:
				// ADC (zp),Y
				ADCInstrHandler(IndYAddrModeHandler_Data());
				break;
			case 0x72:
				if (MachineType == Model::Master128) {
					// ADC (zp)
					ADCInstrHandler(ZPIndAddrModeHandler_Data());
				}
				else {
					// Undocumented instruction: KIL
					KILInstrHandler();
				}
				break;
			case 0x73:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: RRA (zp),Y
					int16 zpaddr = IndYAddrModeHandler_Address();
					RORInstrHandler(zpaddr);
					ADCInstrHandler(WholeRam[zpaddr]);
				}
				break;
			case 0x74:
				if (MachineType == Model::Master128) {
					// STZ zp,X
					BEEBWRITEMEM_DIRECT(ZeroPgXAddrModeHandler_Address(), 0);
				}
				else {
					// Undocumented instruction: NOP zp,x
					ZeroPgXAddrModeHandler_Address();
				}
				break;
			case 0x75:
				// ADC zp,X
				ADCInstrHandler(ZeroPgXAddrModeHandler_Data());
				break;
			case 0x76:
				// ROR zp,X
				RORInstrHandler(ZeroPgXAddrModeHandler_Address());
				break;
			case 0x77:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: RRA zp,X
					int16 zpaddr = ZeroPgXAddrModeHandler_Address();
					RORInstrHandler(zpaddr);
					ADCInstrHandler(WholeRam[zpaddr]);
				}
				break;
			case 0x78:
				// SEI
				if (!(PSR & FlagI)) {
					iFlagJustSet = true;
				}
				PSR |= FlagI;
				break;
			case 0x79:
				// ADC abs,Y
				ADCInstrHandler(AbsYAddrModeHandler_Data());
				break;
			case 0x7a:
				if (MachineType == Model::Master128) {
					// PLY
					YReg = Pop();
					SetPSRZN(YReg);
				}
				else {
					// Undocumented instruction: NOP
				}
				break;
			case 0x7b:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: RRA abs,Y
					int16 zpaddr = AbsYAddrModeHandler_Address();
					RORInstrHandler(zpaddr);
					ADCInstrHandler(WholeRam[zpaddr]);
				}
				break;
			case 0x7c:
				if (MachineType == Model::Master128) {
					// JMP abs,X
					ProgramCounter = IndAddrXModeHandler_Address();
				}
				else {
					// Undocumented instruction: NOP abs,X
					AbsXAddrModeHandler_Data();
				}
				break;
			case 0x7d:
				// ADC abs,X
				ADCInstrHandler(AbsXAddrModeHandler_Data());
				break;
			case 0x7e:
				// ROR abs,X
				RORInstrHandler(AbsXAddrModeHandler_Address());
				break;
			case 0x7f:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: RRA abs,X
					int16 zpaddr = AbsXAddrModeHandler_Address();
					RORInstrHandler(zpaddr);
					ADCInstrHandler(WholeRam[zpaddr]);
				}
				break;
			case 0x80:
				if (MachineType == Model::Master128) {
					// BRA rel
					BRAInstrHandler();
				}
				else {
					// Undocumented instruction: NOP imm
					ReadPaged(ProgramCounter++);
				}
				break;
			case 0x81:
				// STA (zp,X)
				AdvanceCyclesForMemWrite();
				WritePaged(IndXAddrModeHandler_Address(), Accumulator);
				break;
			case 0x82:
			case 0xc2:
			case 0xe2:
				// Undocumented instruction: NOP imm
				ReadPaged(ProgramCounter++);
				break;
			case 0x83:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: SAX (zp,X)
					WholeRam[IndXAddrModeHandler_Address()] = Accumulator & XReg;
				}
				break;
			case 0x84:
				// STY zp
				AdvanceCyclesForMemWrite();
				BEEBWRITEMEM_DIRECT(ZeroPgAddrModeHandler_Address(), YReg);
				break;
			case 0x85:
				// STA zp
				AdvanceCyclesForMemWrite();
				BEEBWRITEMEM_DIRECT(ZeroPgAddrModeHandler_Address(), Accumulator);
				break;
			case 0x86:
				// STX zp
				AdvanceCyclesForMemWrite();
				BEEBWRITEMEM_DIRECT(ZeroPgAddrModeHandler_Address(), XReg);
				break;
			case 0x87:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: SAX zp
					// This one does not seem to change the processor flags
					AdvanceCyclesForMemWrite();
					WholeRam[ZeroPgAddrModeHandler_Address()] = Accumulator & XReg;
				}
				break;
			case 0x88:
				// DEY
				YReg = (YReg - 1) & 255;
				SetPSRZN(YReg);
				break;
			case 0x89:
				if (MachineType == Model::Master128) {
					// BIT imm
					BITImmedInstrHandler(ReadPaged(ProgramCounter++));
				}
				else {
					// Undocumented instruction: NOP imm
					ReadPaged(ProgramCounter++);
				}
				break;
			case 0x8a:
				// TXA
				Accumulator = XReg;
				SetPSRZN(Accumulator);
				break;
			case 0x8b:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: XAA imm
					// See http://visual6502.org/wiki/index.php?title=6502_Opcode_8B_(XAA,_ANE)_explained
					Accumulator &= XReg & ReadPaged(ProgramCounter++);
					SetPSRZN(Accumulator);
				}
				break;
			case 0x8c:
				// STY abs
				AdvanceCyclesForMemWrite();
				STYInstrHandler(AbsAddrModeHandler_Address());
				break;
			case 0x8d:
				// STA abs
				AdvanceCyclesForMemWrite();
				WritePaged(AbsAddrModeHandler_Address(), Accumulator);
				break;
			case 0x8e:
				// STX abs
				AdvanceCyclesForMemWrite();
				STXInstrHandler(AbsAddrModeHandler_Address());
				break;
			case 0x8f:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: SAX abs
					WholeRam[AbsAddrModeHandler_Address()] = Accumulator & XReg;
				}
				break;
			case 0x90:
				// BCC rel
				BCCInstrHandler();
				break;
			case 0x91:
				// STA (zp),Y
				AdvanceCyclesForMemWrite();
				WritePaged(IndYAddrModeHandler_Address(), Accumulator);
				break;
			case 0x92:
				if (MachineType == Model::Master128) {
					// STA (zp)
					AdvanceCyclesForMemWrite();
					WritePaged(ZPIndAddrModeHandler_Address(), Accumulator);
				}
				else {
					// Undocumented instruction: KIL
					KILInstrHandler();
				}
				break;
			case 0x93:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: AHX (zp),Y
					AdvanceCyclesForMemWrite();
					int Address = IndYAddrModeHandler_Address();
					WholeRam[Address] = Accumulator & XReg & ((Address >> 8) + 1);
				}
				break;
			case 0x94:
				// STY zp,X
				AdvanceCyclesForMemWrite();
				STYInstrHandler(ZeroPgXAddrModeHandler_Address());
				break;
			case 0x95:
				// STA zp,X
				AdvanceCyclesForMemWrite();
				WritePaged(ZeroPgXAddrModeHandler_Address(), Accumulator);
				break;
			case 0x96:
				// STX zp,X
				AdvanceCyclesForMemWrite();
				STXInstrHandler(ZeroPgYAddrModeHandler_Address());
				break;
			case 0x97:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: SAX zp,Y
					AdvanceCyclesForMemWrite();
					WholeRam[ZeroPgYAddrModeHandler_Address()] = Accumulator & XReg;
				}
				break;
			case 0x98:
				// TYA
				Accumulator = YReg;
				SetPSRZN(Accumulator);
				break;
			case 0x99:
				// STA abs,Y
				AdvanceCyclesForMemWrite();
				WritePaged(AbsYAddrModeHandler_Address(), Accumulator);
				break;
			case 0x9a:
				// TXS
				StackReg = XReg;
				break;
			case 0x9b:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: TAS abs,Y
					WholeRam[AbsYAddrModeHandler_Address()] = Accumulator & XReg;
				}
				break;
			case 0x9c:
				if (MachineType == Model::Master128) {
					// STZ abs
					WritePaged(AbsAddrModeHandler_Address(), 0);
				}
				else {
					// Undocumented instruction: SHY abs,X
					int Address = AbsXAddrModeHandler_Address();
					WritePaged(Address, YReg & (unsigned char)((Address >> 8) + 1));
				}
				break;
			case 0x9d:
				// STA abs,X
				AdvanceCyclesForMemWrite();
				WritePaged(AbsXAddrModeHandler_Address(), Accumulator);
				break;
			case 0x9e:
				if (MachineType == Model::Master128) {
					// STZ abs,x
					AdvanceCyclesForMemWrite();
					WritePaged(AbsXAddrModeHandler_Address(), 0);
				}
				else {
					// Undocumented instruction: SHX abs,Y
					AdvanceCyclesForMemWrite();
					WritePaged(AbsXAddrModeHandler_Address(), Accumulator & XReg);
				}
				break;
			case 0x9f:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: AHX abs,Y
					AdvanceCyclesForMemWrite();
					int Address = AbsYAddrModeHandler_Address();
					WritePaged(Address, Accumulator & XReg & ((Address >> 8) + 1));
				}
				break;
			case 0xa0:
				// LDY imm
				LDYInstrHandler(ReadPaged(ProgramCounter++));
				break;
			case 0xa1:
				// LDA (zp,X)
				LDAInstrHandler(IndXAddrModeHandler_Data());
				break;
			case 0xa2:
				// LDX imm
				LDXInstrHandler(ReadPaged(ProgramCounter++));
				break;
			case 0xa3:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: LAX (zp,X)
					LDAInstrHandler(IndXAddrModeHandler_Data());
					XReg = Accumulator;
				}
				break;
			case 0xa4:
				// LDY zp
				LDYInstrHandler(WholeRam[ReadPaged(ProgramCounter++)]);
				break;
			case 0xa5:
				// LDA zp
				LDAInstrHandler(WholeRam[ReadPaged(ProgramCounter++)]);
				break;
			case 0xa6:
				// LDX zp
				LDXInstrHandler(WholeRam[ReadPaged(ProgramCounter++)]);
				break;
			case 0xa7:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: LAX zp
					LDAInstrHandler(WholeRam[WholeRam[ProgramCounter++]]);
					XReg = Accumulator;
				}
				break;
			case 0xa8:
				// TAY
				YReg = Accumulator;
				SetPSRZN(Accumulator);
				break;
			case 0xa9:
				// LDA imm
				LDAInstrHandler(ReadPaged(ProgramCounter++));
				break;
			case 0xaa:
				// TXA
				XReg = Accumulator;
				SetPSRZN(Accumulator);
				break;
			case 0xab:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: LAX imm
					LDAInstrHandler(Accumulator & ReadPaged(ProgramCounter++));
					XReg = Accumulator;
				}
				break;
			case 0xac:
				// LDY abs
				LDYInstrHandler(AbsAddrModeHandler_Data());
				break;
			case 0xad:
				// LDA abs
				LDAInstrHandler(AbsAddrModeHandler_Data());
				break;
			case 0xae:
				// LDX abs
				LDXInstrHandler(AbsAddrModeHandler_Data());
				break;
			case 0xaf:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: LAX abs
					LDAInstrHandler(AbsAddrModeHandler_Data());
					XReg = Accumulator;
				}
				break;
			case 0xb0:
				// BCS rel
				BCSInstrHandler();
				break;
			case 0xb1:
				// LDA (zp),Y
				LDAInstrHandler(IndYAddrModeHandler_Data());
				break;
			case 0xb2:
				if (MachineType == Model::Master128) {
					// LDA (zp)
					LDAInstrHandler(ZPIndAddrModeHandler_Data());
				}
				else {
					// Undocumented instruction: KIL
					KILInstrHandler();
				}
				break;
			case 0xb3:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: LAX (zp),Y
					LDAInstrHandler(IndYAddrModeHandler_Data());
					XReg = Accumulator;
				}
				break;
			case 0xb4:
				// LDY zp,X
				LDYInstrHandler(ZeroPgXAddrModeHandler_Data());
				break;
			case 0xb5:
				// LDA zp,X
				LDAInstrHandler(ZeroPgXAddrModeHandler_Data());
				break;
			case 0xb6:
				// LDX zp,Y
				LDXInstrHandler(ZeroPgYAddrModeHandler_Data());
				break;
			case 0xb7:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: LAX zp,Y
					LDXInstrHandler(ZeroPgYAddrModeHandler_Data());
					Accumulator = XReg;
				}
				break;
			case 0xb8:
				// CLV
				PSR &= 255 - FlagV;
				break;
			case 0xb9:
				// LDA abs,Y
				LDAInstrHandler(AbsYAddrModeHandler_Data());
				break;
			case 0xba:
				// TSX
				XReg = StackReg;
				SetPSRZN(XReg);
				break;
			case 0xbb:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: LAS abs,Y
					LDAInstrHandler(StackReg & AbsYAddrModeHandler_Data());
					XReg = Accumulator;
					StackReg = Accumulator;
				}
				break;
			case 0xbc:
				// LDY abs,X
				LDYInstrHandler(AbsXAddrModeHandler_Data());
				break;
			case 0xbd:
				// LDA abs,X
				LDAInstrHandler(AbsXAddrModeHandler_Data());
				break;
			case 0xbe:
				// LDX abs,Y
				LDXInstrHandler(AbsYAddrModeHandler_Data());
				break;
			case 0xbf:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: LAX abs,Y
					LDAInstrHandler(AbsYAddrModeHandler_Data());
					XReg = Accumulator;
				}
				break;
			case 0xc0:
				// CPY imm
				CPYInstrHandler(ReadPaged(ProgramCounter++));
				break;
			case 0xc1:
				// CMP (zp,X)
				CMPInstrHandler(IndXAddrModeHandler_Data());
				break;
			case 0xc3:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocument instruction: DCP (zp,X)
					int16 zpaddr = IndXAddrModeHandler_Address();
					DECInstrHandler(zpaddr);
					CMPInstrHandler(WholeRam[zpaddr]);
				}
				break;
			case 0xc4:
				// CPY zp
				CPYInstrHandler(WholeRam[ReadPaged(ProgramCounter++)]);
				break;
			case 0xc5:
				// CMP zp
				CMPInstrHandler(WholeRam[ReadPaged(ProgramCounter++)]);
				break;
			case 0xc6:
				// DEC zp
				DECInstrHandler(ZeroPgAddrModeHandler_Address());
				break;
			case 0xc7:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: DCP zp
					int16 zpaddr = ZeroPgAddrModeHandler_Address();
					DECInstrHandler(zpaddr);
					CMPInstrHandler(WholeRam[zpaddr]);
				}
				break;
			case 0xc8:
				// INY
				YReg += 1;
				YReg &= 255;
				SetPSRZN(YReg);
				break;
			case 0xc9:
				// CMP imm
				CMPInstrHandler(ReadPaged(ProgramCounter++));
				break;
			case 0xca:
				// DEX
				DEXInstrHandler();
				break;
			case 0xcb:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: ASX imm
					// I dont know if this uses the carry or not, i'm assuming it's
					// Subtract #n from X with carry.
					unsigned char TmpAcc = Accumulator;
					Accumulator = XReg;
					SBCInstrHandler(WholeRam[ProgramCounter++]);
					XReg = Accumulator;
					Accumulator = TmpAcc; // Fudge so that I dont have to do the whole SBC code again
				}
				break;
			case 0xcc:
				// CPY abs
				CPYInstrHandler(AbsAddrModeHandler_Data());
				break;
			case 0xcd:
				// CMP abs
				CMPInstrHandler(AbsAddrModeHandler_Data());
				break;
			case 0xce:
				// DEC abs
				DECInstrHandler(AbsAddrModeHandler_Address());
				break;
			case 0xcf:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: DCP abs
					int16 zpaddr = AbsAddrModeHandler_Address();
					DECInstrHandler(zpaddr);
					CMPInstrHandler(WholeRam[zpaddr]);
				}
				break;
			case 0xd0:
				// BNE rel
				BNEInstrHandler();
				break;
			case 0xd1:
				// CMP (zp),Y
				CMPInstrHandler(IndYAddrModeHandler_Data());
				break;
			case 0xd2:
				if (MachineType == Model::Master128) {
					// CMP (zp)
					CMPInstrHandler(ZPIndAddrModeHandler_Data());
				}
				else {
					// Undocumented instruction: KIL
					KILInstrHandler();
				}
				break;
			case 0xd3:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: DCP (zp),Y
					int16 zpaddr = IndYAddrModeHandler_Address();
					DECInstrHandler(zpaddr);
					CMPInstrHandler(WholeRam[zpaddr]);
				}
				break;
			case 0xd5:
				// CMP zp,X
				CMPInstrHandler(ZeroPgXAddrModeHandler_Data());
				break;
			case 0xd6:
				// DEC zp,X
				DECInstrHandler(ZeroPgXAddrModeHandler_Address());
				break;
			case 0xd7:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: DCP zp,X
					int16 zpaddr = ZeroPgXAddrModeHandler_Address();
					DECInstrHandler(zpaddr);
					CMPInstrHandler(WholeRam[zpaddr]);
				}
				break;
			case 0xd8:
				// CLD
				PSR &= 255 - FlagD;
				break;
			case 0xd9:
				// CMP abs,Y
				CMPInstrHandler(AbsYAddrModeHandler_Data());
				break;
			case 0xda:
				if (MachineType == Model::Master128) {
					// PHX
					Push(XReg);
				}
				else {
					// Undocumented instruction: NOP
				}
				break;
			case 0xdb:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: DCP abs,Y
					int16 zpaddr = AbsYAddrModeHandler_Address();
					DECInstrHandler(zpaddr);
					CMPInstrHandler(WholeRam[zpaddr]);
				}
				break;
			case 0xdc:
			case 0xfc:
				if (MachineType == Model::Master128) {
					// NOP abs
					AbsAddrModeHandler_Address();
				}
				else {
					// Undocumented instruction: NOP abs,X
					AbsXAddrModeHandler_Data();
				}
				break;
			case 0xdd:
				// CMP abs,X
				CMPInstrHandler(AbsXAddrModeHandler_Data());
				break;
			case 0xde:
				// DEC abs,X
				DECInstrHandler(AbsXAddrModeHandler_Address());
				break;
			case 0xdf:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: DCP abs,X
					int16 zpaddr = AbsXAddrModeHandler_Address();
					DECInstrHandler(zpaddr);
					CMPInstrHandler(WholeRam[zpaddr]);
				}
				break;
			case 0xe0:
				// CPX imm
				CPXInstrHandler(ReadPaged(ProgramCounter++));
				break;
			case 0xe1:
				// SBC (zp,X)
				SBCInstrHandler(IndXAddrModeHandler_Data());
				break;
			case 0xe3:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: ISC (zp,X)
					int16 zpaddr = IndXAddrModeHandler_Address();
					INCInstrHandler(zpaddr);
					SBCInstrHandler(WholeRam[zpaddr]);
				}
				break;
			case 0xe4:
				// CPX zp
				CPXInstrHandler(WholeRam[ReadPaged(ProgramCounter++)]);
				break;
			case 0xe5:
				// SBC zp
				SBCInstrHandler(WholeRam[ReadPaged(ProgramCounter++)]);
				break;
			case 0xe6:
				// INC zp
				INCInstrHandler(ZeroPgAddrModeHandler_Address());
				break;
			case 0xe7:
				if (MachineType != Model::Master128) {
					// Undocumented instruction: ISC zp
					int16 zpaddr = ZeroPgAddrModeHandler_Address();
					INCInstrHandler(zpaddr);
					SBCInstrHandler(WholeRam[zpaddr]);
				}
				break;
			case 0xe8:
				// INX
				INXInstrHandler();
				break;
			case 0xe9:
				// SBC imm
				SBCInstrHandler(ReadPaged(ProgramCounter++));
				break;
			case 0xea:
				// NOP
				break;
			case 0xeb:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// TODO: SBC imm
				}
				break;
			case 0xec:
				// CPX abs
				CPXInstrHandler(AbsAddrModeHandler_Data());
				break;
			case 0xed:
				// SBC abs
				SBCInstrHandler(AbsAddrModeHandler_Data());
				break;
			case 0xee:
				// INC abs
				INCInstrHandler(AbsAddrModeHandler_Address());
				break;
			case 0xef:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: ISC abs
					int16 zpaddr = AbsAddrModeHandler_Address();
					INCInstrHandler(zpaddr);
					SBCInstrHandler(WholeRam[zpaddr]);
				}
				break;
			case 0xf0:
				// BEQ rel
				BEQInstrHandler();
				break;
			case 0xf1:
				// SBC (zp),Y
				SBCInstrHandler(IndYAddrModeHandler_Data());
				break;
			case 0xf2:
				if (MachineType == Model::Master128) {
					// SBC (zp)
					SBCInstrHandler(ZPIndAddrModeHandler_Data());
				}
				else {
					// Undocumented instruction: KIL
					KILInstrHandler();
				}
				break;
			case 0xf3:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: ISC (zp),Y
					int16 zpaddr = IndYAddrModeHandler_Address();
					INCInstrHandler(zpaddr);
					SBCInstrHandler(WholeRam[zpaddr]);
				}
				break;
			case 0xf5:
				// SBC zp,X
				SBCInstrHandler(ZeroPgXAddrModeHandler_Data());
				break;
			case 0xf6:
				// INC zp,X
				INCInstrHandler(ZeroPgXAddrModeHandler_Address());
				break;
			case 0xf7:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: ISC zp,X
					int16 zpaddr = ZeroPgXAddrModeHandler_Address();
					INCInstrHandler(zpaddr);
					SBCInstrHandler(WholeRam[zpaddr]);
				}
				break;
			case 0xf8:
				// SED
				PSR |= FlagD;
				break;
			case 0xf9:
				// SBC abs,Y
				SBCInstrHandler(AbsYAddrModeHandler_Data());
				break;
			case 0xfa:
				if (MachineType == Model::Master128) {
					// PLX
					XReg = Pop();
					SetPSRZN(XReg);
				}
				else {
					// Undocumented instruction: NOP
				}
				break;
			case 0xfb:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: ISC abs,Y
					int16 zpaddr = AbsYAddrModeHandler_Address();
					INCInstrHandler(zpaddr);
					SBCInstrHandler(WholeRam[zpaddr]);
				}
				break;
			case 0xfd:
				// SBC abs,X
				SBCInstrHandler(AbsXAddrModeHandler_Data());
				break;
			case 0xfe:
				// INC abs,X
				INCInstrHandler(AbsXAddrModeHandler_Address());
				break;
			case 0xff:
				if (MachineType == Model::Master128) {
					// NOP
				}
				else {
					// Undocumented instruction: ISC abs,X
					int16 zpaddr = AbsXAddrModeHandler_Address();
					INCInstrHandler(zpaddr);
					SBCInstrHandler(WholeRam[zpaddr]);
				}
				break;
		}

		// This block corrects the cycle count for the branch instructions
		if ((CurrentInstruction == 0x10) ||
		    (CurrentInstruction == 0x30) ||
		    (CurrentInstruction == 0x50) ||
		    (CurrentInstruction == 0x70) ||
		    (CurrentInstruction == 0x80 && MachineType == Model::Master128) ||
		    (CurrentInstruction == 0x90) ||
		    (CurrentInstruction == 0xb0) ||
		    (CurrentInstruction == 0xd0) ||
		    (CurrentInstruction == 0xf0))
		{
			if (Branched)
			{
				Cycles++;
				if ((ProgramCounter & 0xff00) != ((OldPC+2) & 0xff00)) {
					Cycles++;
				}
			}
		}

		Cycles += CyclesTable[CurrentInstruction] -
			CyclesToMemRead[CurrentInstruction] - CyclesToMemWrite[CurrentInstruction];

		PollVIAs(Cycles - ViaCycles);
		PollHardware(Cycles);

		// Check for IRQ
		DoIntCheck();
		if (IntDue && (!GETIFLAG || iFlagJustSet) &&
			(CyclesToInt <= (-2-IOCycles) && !iFlagJustCleared))
		{
			// Int noticed 2 cycles before end of instruction - interrupt now
			CyclesToInt = NO_TIMER_INT_DUE;
			DoInterrupt();
			PollHardware(IRQCycles);
			PollVIAs(IRQCycles);
			IRQCycles=0;
		}

		// Check for NMI
		if ((NMIStatus && !OldNMIStatus) || (NMIStatus & 1<<nmi_econet))
		{
			NMIStatus &= ~(1<<nmi_econet);
			DoNMI();
			PollHardware(IRQCycles);
			PollVIAs(IRQCycles);
			IRQCycles=0;
		}
		OldNMIStatus=NMIStatus;

		switch (TubeType) {
			case Tube::Acorn65C02: // 3MHz
				SyncTubeProcessor();
				break;

			case Tube::AcornZ80: // TODO: 6MHz
			case Tube::TorchZ80: // TODO: 4MHz
				z80_execute();
				break;

			case Tube::AcornArm: // TODO: 8MHz
				arm->exec(4);
				break;

			case Tube::SprowArm: // 64MHz
				#if _DEBUG
				sprow->Execute(2);
				#else
				sprow->Execute(32 * Cycles);
				#endif
				break;

			case Tube::Master512CoPro: // 8MHz
				master512CoPro.Execute(4 * Cycles);
				break;
		}
	}
}

static void PollVIAs(unsigned int nCycles)
{
	if (nCycles != 0)
	{
		if (CyclesToInt != NO_TIMER_INT_DUE)
			CyclesToInt -= nCycles;

		SysVIA_poll(nCycles);
		UserVIA_poll(nCycles);

		ViaCycles += nCycles;
	}
}

static void PollHardware(unsigned int nCycles)
{
	TotalCycles+=nCycles;

	if (TotalCycles > CycleCountWrap)
	{
		TotalCycles -= CycleCountWrap;
		AdjustTrigger(AtoDTrigger);
		AdjustTrigger(SoundTrigger);
		AdjustTrigger(Disc8271Trigger);
		AdjustTrigger(AMXTrigger);
		AdjustTrigger(PrinterTrigger);
		AdjustTrigger(VideoTriggerCount);
		AdjustTrigger(TapeTrigger);
		AdjustTrigger(EconetTrigger);
		AdjustTrigger(EconetFlagFillTimeoutTrigger);
		AdjustTrigger(IP232RxTrigger);
		if (TubeType == Tube::Acorn65C02)
			WrapTubeCycles();
	}

	VideoPoll(nCycles);
	if (!BasicHardwareOnly) {
		AtoD_poll(nCycles);
		Serial_Poll();
	}
	Disc8271Poll();
	Music5000Poll(nCycles);
	SoundPoll();
	if (DisplayCycles>0) DisplayCycles-=nCycles; // Countdown time till end of display of info.
	if (MachineType == Model::Master128 || !NativeFDC) Poll1770(nCycles); // Do 1770 Background stuff

	if (EconetEnabled && EconetPoll()) {
		if (EconetNMIenabled) {
			NMIStatus|=1<<nmi_econet;
			if (DebugEnabled)
				DebugDisplayTrace(DebugType::Econet, true, "Econet: NMI asserted");
		}
		else {
			if (DebugEnabled)
				DebugDisplayTrace(DebugType::Econet, true, "Econet: NMI requested but supressed");
		}
	}
}

/*-------------------------------------------------------------------------*/
void Save6502UEF(FILE *SUEF) {
	fput16(0x0460,SUEF);
	fput32(16,SUEF);
	fput16(ProgramCounter,SUEF);
	fputc(Accumulator,SUEF);
	fputc(XReg,SUEF);
	fputc(YReg,SUEF);
	fputc(StackReg,SUEF);
	fputc(PSR,SUEF);
	fput32(TotalCycles,SUEF);
	fputc(intStatus,SUEF);
	fputc(NMIStatus,SUEF);
	fputc(NMILock,SUEF);
	fput16(0,SUEF);
}

void Load6502UEF(FILE *SUEF) {
	ProgramCounter = fget16(SUEF);
	Accumulator = fget8(SUEF);
	XReg = fget8(SUEF);
	YReg = fget8(SUEF);
	StackReg = fget8(SUEF);
	PSR = fget8(SUEF);
	// TotalCycles = fget32(SUEF);
	int Dlong = fget32(SUEF);
	intStatus = fget8(SUEF);
	NMIStatus = fget8(SUEF);
	NMILock = fgetbool(SUEF);
	// AtoDTrigger=Disc8271Trigger=AMXTrigger=PrinterTrigger=VideoTriggerCount=TotalCycles+100;
}

void WriteInstructionCounts(const char *FileName)
{
	FILE *file = fopen(FileName, "w");
	if (file) {
		for (int i = 0; i < 256; i++) {
			fprintf(file, "%02X\t%d\n", i, InstructionCount[i]);
		}
		fclose(file);
	}
}
