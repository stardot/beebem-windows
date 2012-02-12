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
#include <stdio.h>
#include <stdlib.h>

#include "6502core.h"
#include "beebmem.h"
#include "beebsound.h"
#include "disc8271.h"
#include "sysvia.h"
#include "uservia.h"
#include "video.h"
#include "atodconv.h"
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

#ifdef WIN32
#define INLINE inline
#else
#define INLINE
#endif

using namespace std;

int CPUDebug=0;
int BeginDump=0;
FILE *InstrLog;
FILE *osclilog; //=fopen("/oscli.log","wt");

static unsigned int InstrCount;
int IgnoreIllegalInstructions = 1;
static int CurrentInstruction;

extern int DumpAfterEach;
extern CArm *arm;

CycleCountT TotalCycles=0;

int trace = 0;
int ProgramCounter;
int PrePC;
static int Accumulator,XReg,YReg;
static unsigned char StackReg,PSR;
static unsigned char IRQCycles;
int DisplayCycles=0;
int SwitchOnCycles=2000000; // Reset delay

unsigned char intStatus=0; /* bit set (nums in IRQ_Nums) if interrupt being caused */
unsigned char NMIStatus=0; /* bit set (nums in NMI_Nums) if NMI being caused */
unsigned int NMILock=0; /* Well I think NMI's are maskable - to stop repeated NMI's - the lock is released when an RTI is done */
typedef int int16;
INLINE static void SBCInstrHandler(int16 operand);

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

static int CyclesTable[]={
/*0 1 2 3 4 5 6 7 8 9 a b c d e f */
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
}; /* CyclesTable */

// Number of cycles to start of memory read cycle
static int CyclesToMemRead[]={
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

// Number of cycles to start of memory write cycle
static int CyclesToMemWrite[]={
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


/* The number of cycles to be used by the current instruction - exported to
   allow fernangling by memory subsystem */
unsigned int Cycles;

/* Number of cycles VIAs advanced for mem read and writes */
unsigned int ViaCycles;

/* Number of additional cycles for IO read / writes */
int IOCycles=0;

/* Flag indicating if an interrupt is due */
bool IntDue=false;

/* When a timer interrupt is due this is the number of cycles 
   to it (usually -ve) */
int CyclesToInt = NO_TIMER_INT_DUE;

static unsigned char Branched;
// Branched - 1 if the instruction branched
int OpCodes=2; // 1 = documented only, 2 = commonoly used undocumenteds, 3 = full set
int BHardware=0; // 0 = all hardware, 1 = basic hardware only
// 1 if first cycle happened

/* Get a two byte address from the program counter, and then post inc the program counter */
#define GETTWOBYTEFROMPC(var) \
  var=ReadPaged(ProgramCounter); \
  var|=(ReadPaged(ProgramCounter+1)<<8); \
  ProgramCounter+=2;

#define WritePaged(addr,val) BeebWriteMem(addr,val)
#define ReadPaged(Address) BeebReadMem(Address)

void PollVIAs(unsigned int nCycles);
void PollHardware(unsigned int Cycles);

/*----------------------------------------------------------------------------*/
INLINE void Carried() {
	// Correct cycle count for indirection across page boundary
	if (((CurrentInstruction & 0xf)==0x1 ||
		 (CurrentInstruction & 0xf)==0x9 ||
		 (CurrentInstruction & 0xf)==0xd) &&
		(CurrentInstruction & 0xf0)!=0x90)
	{
		Cycles++;
	}
	else if (CurrentInstruction==0xBC ||
			 CurrentInstruction==0xBE)
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
void AdvanceCyclesForMemRead(void)
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
void AdvanceCyclesForMemWrite(void)
{
	// Advance VIAs to point where mem write happens
	Cycles += CyclesToMemWrite[CurrentInstruction];
	PollVIAs(CyclesToMemWrite[CurrentInstruction]);

	DoIntCheck();
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
void DumpRegs(void) {
  static char FlagNames[]="CZIDB-VNczidb-vn";
  int FlagNum;

  fprintf(stderr,"  PC=0x%x A=0x%x X=0x%x Y=0x%x S=0x%x PSR=0x%x=",
    ProgramCounter,Accumulator,XReg,YReg,StackReg,PSR);
  for(FlagNum=0;FlagNum<8;FlagNum++)
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
/* Relative addressing mode handler                                        */
INLINE static int16 RelAddrModeHandler_Data(void) {
  int EffectiveAddress;

  /* For branches - is this correct - i.e. is the program counter incremented
     at the correct time? */
  EffectiveAddress=SignExtendByte((signed char)ReadPaged(ProgramCounter++));
  EffectiveAddress+=ProgramCounter;

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
    Branched=1;
  } else ProgramCounter++;
} /* BCCInstrHandler */

INLINE static void BCSInstrHandler(void) {
  if (GETCFLAG) {
    ProgramCounter=RelAddrModeHandler_Data();
    Branched=1;
  } else ProgramCounter++;
} /* BCSInstrHandler */

INLINE static void BEQInstrHandler(void) {
  if (GETZFLAG) {
    ProgramCounter=RelAddrModeHandler_Data();
    Branched=1;
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
    Branched=1;
  } else ProgramCounter++;
} /* BMIInstrHandler */

INLINE static void BNEInstrHandler(void) {
  if (!GETZFLAG) {
    ProgramCounter=RelAddrModeHandler_Data();
    Branched=1;
  } else ProgramCounter++;
} /* BNEInstrHandler */

INLINE static void BPLInstrHandler(void) {
  if (!GETNFLAG) {
    ProgramCounter=RelAddrModeHandler_Data();
    Branched=1;
  } else ProgramCounter++;
}; /* BPLInstrHandler */

INLINE static void BRKInstrHandler(void) {
  char errstr[250];
  if (CPUDebug) {
  sprintf(errstr,"BRK Instruction at 0x%04x after %i instructions. ACCON: 0x%02x ROMSEL: 0x%02x",ProgramCounter,InstrCount,ACCCON,PagedRomReg);
  MessageBox(GETHWND,errstr,WindowTitle,MB_OKCANCEL|MB_ICONERROR);
  //fclose(InstrLog);
  exit(1); 
  }
  PushWord(ProgramCounter+1);
  SetPSR(FlagB,0,0,0,0,1,0,0); /* Set B before pushing */
  Push(PSR);
  SetPSR(FlagI,0,0,1,0,0,0,0); /* Set I after pushing - see Birnbaum */
  ProgramCounter=BeebReadMem(0xfffe) | (BeebReadMem(0xffff)<<8);
} /* BRKInstrHandler */

INLINE static void BVCInstrHandler(void) {
  if (!GETVFLAG) {
    ProgramCounter=RelAddrModeHandler_Data();
    Branched=1;
  } else ProgramCounter++;
} /* BVCInstrHandler */

INLINE static void BVSInstrHandler(void) {
  if (GETVFLAG) {
    ProgramCounter=RelAddrModeHandler_Data();
    Branched=1;
  } else ProgramCounter++;
} /* BVSInstrHandler */

INLINE static void BRAInstrHandler(void) {
    ProgramCounter=RelAddrModeHandler_Data();
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
/*  if (ProgramCounter==0xffdd) {
	  // OSCLI logging for elite debugging
	  unsigned char *bptr;
	  char pcbuf[256]; char *pcptr=pcbuf;
	  int blk=((YReg*256)+XReg);
	  bptr=WholeRam+((WholeRam[blk+1]*256)+WholeRam[blk]);
  	  while((*bptr != 13) && ((pcptr-pcbuf)<254)) {
		  *pcptr=*bptr; pcptr++;bptr++; 
	  } 
	  *pcptr=0;
	  fprintf(osclilog,"%s\n",pcbuf);
  }
  /*if (ProgramCounter==0xffdd) {
	  char errstr[250];
	  sprintf(errstr,"OSFILE called\n");
	  MessageBox(GETHWND,errstr,WindowTitle,MB_OKCANCEL|MB_ICONERROR);
  }*/

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
  WritePaged(address,XReg);
} /* STXInstrHandler */

INLINE static void STYInstrHandler(int16 address) {
  WritePaged(address,YReg);
} /* STYInstrHandler */

INLINE static void BadInstrHandler(int opcode) {
	if (!IgnoreIllegalInstructions)
	{
#ifdef WIN32
		char errstr[250];
		sprintf(errstr,"Unsupported 6502 instruction 0x%02X at 0x%04X\n"
			"  OK - instruction will be skipped\n"
			"  Cancel - dump memory and exit",opcode,ProgramCounter-1);
		if (MessageBox(GETHWND,errstr,WindowTitle,MB_OKCANCEL|MB_ICONERROR) == IDCANCEL)
		{
			beebmem_dumpstate();
			exit(0);
		}
#else
		fprintf(stderr,"Bad instruction handler called:\n");
		DumpRegs();
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
		ProgramCounter++;
		break;

	/* Three byte instructions */
	case 0xc:
	case 0xe:
	case 0xf:
		ProgramCounter+=2;
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
INLINE static int16 IndXAddrModeHandler_Address(void) {
  unsigned char ZeroPageAddress;
  int EffectiveAddress;

  ZeroPageAddress=(ReadPaged(ProgramCounter++)+XReg) & 255;

  EffectiveAddress=WholeRam[ZeroPageAddress] | (WholeRam[ZeroPageAddress+1]<<8);
  return(EffectiveAddress);
} /* IndXAddrModeHandler_Address */

/*-------------------------------------------------------------------------*/
/* Indexed with Y postinc addressing mode handler                          */
INLINE static int16 IndYAddrModeHandler_Data(void) {
  int EffectiveAddress;
  unsigned char ZPAddr=ReadPaged(ProgramCounter++);
  EffectiveAddress=WholeRam[ZPAddr]+YReg;
  if (EffectiveAddress>0xff) Carried();
  EffectiveAddress+=(WholeRam[ZPAddr+1]<<8);

  return(ReadPaged(EffectiveAddress));
} /* IndYAddrModeHandler */

/*-------------------------------------------------------------------------*/
/* Indexed with Y postinc addressing mode handler                          */
INLINE static int16 IndYAddrModeHandler_Address(void) {
  int EffectiveAddress;
  unsigned char ZPAddr=ReadPaged(ProgramCounter++);
  EffectiveAddress=WholeRam[ZPAddr]+YReg;
  if (EffectiveAddress>0xff) Carried();
  EffectiveAddress+=(WholeRam[ZPAddr+1]<<8);

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
  if ((VectorLocation & 0xff)!=0xff || MachineType==3) {
   EffectiveAddress=ReadPaged(VectorLocation);
   EffectiveAddress|=ReadPaged(VectorLocation+1) << 8; }
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
/* Initialise 6502core                                                     */
void Init6502core(void) {
  ProgramCounter=BeebReadMem(0xfffc) | (BeebReadMem(0xfffd)<<8);
  Accumulator=XReg=YReg=0; /* For consistancy of execution */
  StackReg=0xff; /* Initial value ? */
  PSR=FlagI; /* Interrupts off for starters */

  intStatus=0;
  NMIStatus=0;
  NMILock=0;
} /* Init6502core */

#include "via.h"

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
  NMILock=1;
  PushWord(ProgramCounter);
  Push(PSR);
  ProgramCounter=BeebReadMem(0xfffa) | (BeebReadMem(0xfffb)<<8);
  SetPSR(FlagI,0,0,1,0,0,0,0); /* Normal interrupts should be disabled during NMI ? */
  IRQCycles=7;
} /* DoNMI */

void Dis6502(void)
{
char str[256];

	DebugDisassembleInstruction(ProgramCounter, true, str);
	
	sprintf(str + strlen(str), "%02X %02X %02X ", Accumulator, XReg, YReg);
	
    sprintf(str + strlen(str), (PSR & FlagC) ? "C" : ".");
	sprintf(str + strlen(str), (PSR & FlagZ) ? "Z" : ".");
	sprintf(str + strlen(str), (PSR & FlagI) ? "I" : ".");
	sprintf(str + strlen(str), (PSR & FlagD) ? "D" : ".");
	sprintf(str + strlen(str), (PSR & FlagB) ? "B" : ".");
	sprintf(str + strlen(str), (PSR & FlagV) ? "V" : ".");
	sprintf(str + strlen(str), (PSR & FlagN) ? "N" : ".");

	WriteLog("%s\n", str);

}

void MemoryDump6502(int addr, int count)
{
	int a, b;
	int s, e;
	int v;
	char info[80];
	
	s = addr & 0xffff0;
	e = (addr + count - 1) | 0xf;
	if (e > 0xfffff)
		e = 0xfffff;
	for (a = s; a < e; a += 16)
	{
		sprintf(info, "%04X  ", a);
		
		for (b = 0; b < 16; ++b)
		{
			sprintf(info+strlen(info), "%02X ", DebugReadMem(a+b, true));
		}
		
		for (b = 0; b < 16; ++b)
		{
			v = DebugReadMem(a+b, true);
			if (v < 32 || v > 127)
				v = '.';
			sprintf(info+strlen(info), "%c", v);
		}
		
		WriteLog("%s\n", info);
	}
	
}

/*-------------------------------------------------------------------------*/
/* Execute one 6502 instruction, move program counter on                   */
void Exec6502Instruction(void) {
	static int OldNMIStatus;
	int BadCount=0;
	int OldPC;
	int loop,loopc;
	bool iFlagJustCleared;
	bool iFlagJustSet;

	loopc=(DebugEnabled ? 1 : 1024); // Makes debug window more responsive
	for(loop=0;loop<loopc;loop++) {
		/* Output debug info */
		if (DebugEnabled && !DebugDisassembler(ProgramCounter,PrePC,Accumulator,XReg,YReg,PSR,StackReg,true))
		{
			Sleep(10);  // Ease up on CPU when halted
			continue;
		}

		if (trace == 1)
		{
			Dis6502();
		}

		z80_execute();

		if (Enable_Arm)
		{
			arm->exec(4);
		}

#ifdef M512COPRO_ENABLED
		if (Tube186Enabled)
			i186_execute(12 * 4);
#endif

		Branched=0;
		iFlagJustCleared=false;
		iFlagJustSet=false;
		Cycles=0;
		IOCycles = 0;
		BadCount=0;
		IntDue = false;

		// Check for WRCHV, send char to speech output
		if (mainWin->m_TextToSpeechEnabled &&
			ProgramCounter == (WholeRam[0x20e] + (WholeRam[0x20f] << 8)))
			mainWin->SpeakChar(Accumulator);

		/* Read an instruction and post inc program couter */
		OldPC=ProgramCounter;
		PrePC=ProgramCounter;
		CurrentInstruction=ReadPaged(ProgramCounter++);
		// cout << "Fetch at " << hex << (ProgramCounter-1) << " giving 0x" << CurrentInstruction << dec << "\n"; 

		// Advance VIAs to point where mem read happens
		ViaCycles=0;
		AdvanceCyclesForMemRead();

		//  if ((ProgramCounter>=0x0100) && (ProgramCounter<=0x0300)) {
		//	  fprintf(InstrLog,"%04x %02x %02x %02x %02x\n",ProgramCounter-1,CurrentInstruction,ReadPaged(ProgramCounter),ReadPaged(ProgramCounter+1),YReg);
		//  }
		if (OpCodes>=1) { // Documented opcodes
			switch (CurrentInstruction) {
			case 0x00:
				BRKInstrHandler();
				break;
			case 0x01:
				ORAInstrHandler(IndXAddrModeHandler_Data());
				break;
			case 0x04:
				if (MachineType==3) TSBInstrHandler(ZeroPgAddrModeHandler_Address()); else ProgramCounter+=1;
				break;
			case 0x05:
				ORAInstrHandler(WholeRam[ReadPaged(ProgramCounter++)]/*zp */);
				break;
			case 0x06:
				ASLInstrHandler(ZeroPgAddrModeHandler_Address());
				break;
			case 0x08:
				Push(PSR|48); /* PHP */
				break;
			case 0x09:
				ORAInstrHandler(ReadPaged(ProgramCounter++)); /* immediate */
				break;
			case 0x0a:
				ASLInstrHandler_Acc();
				break;
			case 0x0c:
				if (MachineType==3) TSBInstrHandler(AbsAddrModeHandler_Address()); else ProgramCounter+=2;
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
				if (MachineType==3) ORAInstrHandler(ZPIndAddrModeHandler_Data());
				break;
			case 0x14:
				if (MachineType==3) TRBInstrHandler(ZeroPgAddrModeHandler_Address()); else ProgramCounter+=1;
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
				if (MachineType==3) INAInstrHandler();
				break;
			case 0x1c:
				if (MachineType==3) TRBInstrHandler(AbsAddrModeHandler_Address()); else ProgramCounter+=2;
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
				BITInstrHandler(WholeRam[ReadPaged(ProgramCounter++)]/*zp */);
				break;
			case 0x25:
				ANDInstrHandler(WholeRam[ReadPaged(ProgramCounter++)]/*zp */);
				break;
			case 0x26:
				ROLInstrHandler(ZeroPgAddrModeHandler_Address());
				break;
			case 0x28:
				{
					unsigned char oldPSR=PSR;
					PSR=Pop(); /* PLP */
					if ((oldPSR ^ PSR) & FlagI)
					{
						if (PSR & FlagI)
							iFlagJustSet=true;
						else
							iFlagJustCleared=true;
					}
				}
				break;
			case 0x29:
				ANDInstrHandler(ReadPaged(ProgramCounter++)); /* immediate */
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
				if (MachineType==3) ANDInstrHandler(ZPIndAddrModeHandler_Data());
				break;
			case 0x34: /* BIT Absolute,X */
				if (MachineType==3) BITInstrHandler(ZeroPgXAddrModeHandler_Data()); else ProgramCounter+=1;
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
				if (MachineType==3) DEAInstrHandler();
				break;
			case 0x3c: /* BIT Absolute,X */
				if (MachineType==3) BITInstrHandler(AbsXAddrModeHandler_Data()); else ProgramCounter+=2;
				break;
			case 0x3d:
				ANDInstrHandler(AbsXAddrModeHandler_Data());
				break;
			case 0x3e:
				ROLInstrHandler(AbsXAddrModeHandler_Address());
				break;
			case 0x40:
				PSR=Pop(); /* RTI */
				ProgramCounter=PopWord();
				NMILock=0;
				break;
			case 0x41:
				EORInstrHandler(IndXAddrModeHandler_Data());
				break;
			case 0x45:
				EORInstrHandler(WholeRam[ReadPaged(ProgramCounter++)]/*zp */);
				break;
			case 0x46:
				LSRInstrHandler(ZeroPgAddrModeHandler_Address());
				break;
			case 0x48:
				Push(Accumulator); /* PHA */
				break;
			case 0x49:
				EORInstrHandler(ReadPaged(ProgramCounter++)); /* immediate */
				break;
			case 0x4a:
				LSRInstrHandler_Acc();
				break;
			case 0x4c:
				ProgramCounter=AbsAddrModeHandler_Address(); /* JMP */
				/*    if (ProgramCounter==0xffdd) {
				// OSCLI logging for elite debugging
				unsigned char *bptr;
				char pcbuf[256]; char *pcptr=pcbuf;
				int blk=((YReg*256)+XReg);
				bptr=WholeRam+((WholeRam[blk+1]*256)+WholeRam[blk]);
				while((*bptr != 13) && ((pcptr-pcbuf)<254)) {
				*pcptr=*bptr; pcptr++;bptr++; 
				} 
				*pcptr=0;
				fprintf(osclilog,"%s\n",pcbuf);
				}
				/*if (ProgramCounter==0xffdd) {
				char errstr[250];
				sprintf(errstr,"OSFILE called\n");
				MessageBox(GETHWND,errstr,WindowTitle,MB_OKCANCEL|MB_ICONERROR);
				}*/
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
				if (MachineType==3) EORInstrHandler(ZPIndAddrModeHandler_Data());
				break;
			case 0x55:
				EORInstrHandler(ZeroPgXAddrModeHandler_Data());
				break;
			case 0x56:
				LSRInstrHandler(ZeroPgXAddrModeHandler_Address());
				break;
			case 0x58:
				if (PSR & FlagI)
					iFlagJustCleared=true;
				PSR&=255-FlagI; /* CLI */
				break;
			case 0x59:
				EORInstrHandler(AbsYAddrModeHandler_Data());
				break;
			case 0x5a:
				if (MachineType==3) Push(YReg); /* PHY */
				break;
			case 0x5d:
				EORInstrHandler(AbsXAddrModeHandler_Data());
				break;
			case 0x5e:
				LSRInstrHandler(AbsXAddrModeHandler_Address());
				break;
			case 0x60:
				ProgramCounter=PopWord()+1; /* RTS */
				break;
			case 0x61:
				ADCInstrHandler(IndXAddrModeHandler_Data());
				break;
			case 0x64:
				if (MachineType==3) BEEBWRITEMEM_DIRECT(ZeroPgAddrModeHandler_Address(),0); /* STZ Zero Page */
				break;
			case 0x65:
				ADCInstrHandler(WholeRam[ReadPaged(ProgramCounter++)]/*zp */);
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
				ADCInstrHandler(ReadPaged(ProgramCounter++)); /* immediate */
				break;
			case 0x6a:
				RORInstrHandler_Acc();
				break;
			case 0x6c:
				ProgramCounter=IndAddrModeHandler_Address(); /* JMP */
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
				if (MachineType==3) ADCInstrHandler(ZPIndAddrModeHandler_Data());
				break;
			case 0x74:
				if (MachineType==3) { BEEBWRITEMEM_DIRECT(ZeroPgXAddrModeHandler_Address(),0); } else ProgramCounter+=1; /* STZ Zpg,X */
				break;
			case 0x75:
				ADCInstrHandler(ZeroPgXAddrModeHandler_Data());
				break;
			case 0x76:
				RORInstrHandler(ZeroPgXAddrModeHandler_Address());
				break;
			case 0x78:
				if (!(PSR & FlagI))
					iFlagJustSet = true;
				PSR|=FlagI; /* SEI */
				break;
			case 0x79:
				ADCInstrHandler(AbsYAddrModeHandler_Data());
				break;
			case 0x7a:
				if (MachineType==3) {
					YReg=Pop(); /* PLY */
					PSR&=~(FlagZ | FlagN);
					PSR|=((YReg==0)<<1) | (YReg & 128);
				}
				break;
			case 0x7c:
				if (MachineType==3) ProgramCounter=IndAddrXModeHandler_Address(); /* JMP abs,X*/ else ProgramCounter+=2;
				break;
			case 0x7d:
				ADCInstrHandler(AbsXAddrModeHandler_Data());
				break;
			case 0x7e:
				RORInstrHandler(AbsXAddrModeHandler_Address());
				break;
			case 0x81:
				AdvanceCyclesForMemWrite();
				WritePaged(IndXAddrModeHandler_Address(),Accumulator); /* STA */
				break;
			case 0x84:
				AdvanceCyclesForMemWrite();
				BEEBWRITEMEM_DIRECT(ZeroPgAddrModeHandler_Address(),YReg);
				break;
			case 0x85:
				AdvanceCyclesForMemWrite();
				BEEBWRITEMEM_DIRECT(ZeroPgAddrModeHandler_Address(),Accumulator); /* STA */
				break;
			case 0x86:
				AdvanceCyclesForMemWrite();
				BEEBWRITEMEM_DIRECT(ZeroPgAddrModeHandler_Address(),XReg);
				break;
			case 0x88:
				YReg=(YReg-1) & 255; /* DEY */
				PSR&=~(FlagZ | FlagN);
				PSR|=((YReg==0)<<1) | (YReg & 128);
				break;
			case 0x89: /* BIT Immediate */
				if (MachineType==3) BITImmedInstrHandler(ReadPaged(ProgramCounter++));
				break;
			case 0x8a:
				Accumulator=XReg; /* TXA */
				PSR&=~(FlagZ | FlagN);
				PSR|=((Accumulator==0)<<1) | (Accumulator & 128);
				break;
			case 0x8c:
				AdvanceCyclesForMemWrite();
				STYInstrHandler(AbsAddrModeHandler_Address());
				break;
			case 0x8d:
				AdvanceCyclesForMemWrite();
				WritePaged(AbsAddrModeHandler_Address(),Accumulator); /* STA */
				break;
			case 0x8e:
				AdvanceCyclesForMemWrite();
				STXInstrHandler(AbsAddrModeHandler_Address());
				break;
			case 0x91:
				AdvanceCyclesForMemWrite();
				WritePaged(IndYAddrModeHandler_Address(),Accumulator); /* STA */
				break;
			case 0x92:
				AdvanceCyclesForMemWrite();
				if (MachineType==3) WritePaged(ZPIndAddrModeHandler_Address(),Accumulator); /* STA */
				break;
			case 0x94:
				AdvanceCyclesForMemWrite();
				STYInstrHandler(ZeroPgXAddrModeHandler_Address());
				break;
			case 0x95:
				AdvanceCyclesForMemWrite();
				WritePaged(ZeroPgXAddrModeHandler_Address(),Accumulator); /* STA */
				break;
			case 0x96:
				AdvanceCyclesForMemWrite();
				STXInstrHandler(ZeroPgYAddrModeHandler_Address());
				break;
			case 0x98:
				Accumulator=YReg; /* TYA */
				PSR&=~(FlagZ | FlagN);
				PSR|=((Accumulator==0)<<1) | (Accumulator & 128);
				break;
			case 0x99:
				AdvanceCyclesForMemWrite();
				WritePaged(AbsYAddrModeHandler_Address(),Accumulator); /* STA */
				break;
			case 0x9a:
				StackReg=XReg; /* TXS */
				break;
			case 0x9c:
				WritePaged(AbsAddrModeHandler_Address(),0); /* STZ Absolute */
				/* here's a curiosity, STZ Absolute IS on the 6502 UNOFFICIALLY
				   and on the 65C12 OFFICIALLY. Something we should know? - Richard Gellman */
				break;
			case 0x9d:
				AdvanceCyclesForMemWrite();
				WritePaged(AbsXAddrModeHandler_Address(),Accumulator); /* STA */
				break;
			case 0x9e:
				if (MachineType==3) { WritePaged(AbsXAddrModeHandler_Address(),0); } /* STZ Abs,X */ 
				else WritePaged(AbsXAddrModeHandler_Address(),Accumulator & XReg);
				break;
			case 0xa0:
				LDYInstrHandler(ReadPaged(ProgramCounter++)); /* immediate */
				break;
			case 0xa1:
				LDAInstrHandler(IndXAddrModeHandler_Data());
				break;
			case 0xa2:
				LDXInstrHandler(ReadPaged(ProgramCounter++)); /* immediate */
				break;
			case 0xa4:
				LDYInstrHandler(WholeRam[ReadPaged(ProgramCounter++)]/*zp */);
				break;
			case 0xa5:
				LDAInstrHandler(WholeRam[ReadPaged(ProgramCounter++)]/*zp */);
				break;
			case 0xa6:
				LDXInstrHandler(WholeRam[ReadPaged(ProgramCounter++)]/*zp */);
				break;
			case 0xa8:
				YReg=Accumulator; /* TAY */
				PSR&=~(FlagZ | FlagN);
				PSR|=((Accumulator==0)<<1) | (Accumulator & 128);
				break;
			case 0xa9:
				LDAInstrHandler(ReadPaged(ProgramCounter++)); /* immediate */
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
				if (MachineType==3) LDAInstrHandler(ZPIndAddrModeHandler_Data());
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
				CPYInstrHandler(ReadPaged(ProgramCounter++)); /* immediate */
				break;
			case 0xc1:
				CMPInstrHandler(IndXAddrModeHandler_Data());
				break;
			case 0xc4:
				CPYInstrHandler(WholeRam[ReadPaged(ProgramCounter++)]/*zp */);
				break;
			case 0xc5:
				CMPInstrHandler(WholeRam[ReadPaged(ProgramCounter++)]/*zp */);
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
				CMPInstrHandler(ReadPaged(ProgramCounter++)); /* immediate */
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
				if (MachineType==3) CMPInstrHandler(ZPIndAddrModeHandler_Data());
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
				if (MachineType==3) Push(XReg); /* PHX */
				break;
			case 0xdd:
				CMPInstrHandler(AbsXAddrModeHandler_Data());
				break;
			case 0xde:
				DECInstrHandler(AbsXAddrModeHandler_Address());
				break;
			case 0xe0:
				CPXInstrHandler(ReadPaged(ProgramCounter++)); /* immediate */
				break;
			case 0xe1:
				SBCInstrHandler(IndXAddrModeHandler_Data());
				break;
			case 0xe4:
				CPXInstrHandler(WholeRam[ReadPaged(ProgramCounter++)]/*zp */);
				break;
			case 0xe5:
				SBCInstrHandler(WholeRam[ReadPaged(ProgramCounter++)]/*zp */);
				break;
			case 0xe6:
				INCInstrHandler(ZeroPgAddrModeHandler_Address());
				break;
			case 0xe8:
				INXInstrHandler();
				break;
			case 0xe9:
				SBCInstrHandler(ReadPaged(ProgramCounter++)); /* immediate */
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
				if (MachineType==3) SBCInstrHandler(ZPIndAddrModeHandler_Data());
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
				if (MachineType==3) {
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
			default:
				BadCount++;
			}
		}
		if (OpCodes==3) {
			switch (CurrentInstruction) {
			case 0x07: /* Undocumented Instruction: ASL zp and ORA zp */
			{
				int16 zpaddr = ZeroPgAddrModeHandler_Address();
				ASLInstrHandler(zpaddr);
				ORAInstrHandler(WholeRam[zpaddr]);
			}
			break;
			case 0x03: /* Undocumented Instruction: ASL-ORA (zp,X) */
			{
				int16 zpaddr = IndXAddrModeHandler_Address();
				ASLInstrHandler(zpaddr);
				ORAInstrHandler(WholeRam[zpaddr]);
			}
			break;
			case 0x13: /* Undocumented Instruction: ASL-ORA (zp),Y */
			{
				int16 zpaddr = IndYAddrModeHandler_Address();
				ASLInstrHandler(zpaddr);
				ORAInstrHandler(WholeRam[zpaddr]);
			}
			break;
			case 0x0f: /* Undocumented Instruction: ASL-ORA abs */
			{
				int16 zpaddr = AbsAddrModeHandler_Address();
				ASLInstrHandler(zpaddr);
				ORAInstrHandler(WholeRam[zpaddr]);
			}
			break;
			case 0x17: /* Undocumented Instruction: ASL-ORA zp,X */
			{
				int16 zpaddr = ZeroPgXAddrModeHandler_Address();
				ASLInstrHandler(zpaddr);
				ORAInstrHandler(WholeRam[zpaddr]);
			}
			break;
			case 0x1b: /* Undocumented Instruction: ASL-ORA abs,Y */
			{
				int16 zpaddr = AbsYAddrModeHandler_Address();
				ASLInstrHandler(zpaddr);
				ORAInstrHandler(WholeRam[zpaddr]);
			}
			break;
			case 0x1f: /* Undocumented Instruction: ASL-ORA abs,X */
			{
				int16 zpaddr = AbsXAddrModeHandler_Address();
				ASLInstrHandler(zpaddr);
				ORAInstrHandler(WholeRam[zpaddr]);
			}
			break;
			case 0x23: /* Undocumented Instruction: ROL-AND (zp,X) */
			{
				int16 zpaddr=IndXAddrModeHandler_Address();
				ROLInstrHandler(zpaddr);
				ANDInstrHandler(WholeRam[zpaddr]);
			}
			break;
			case 0x27: /* Undocumented Instruction: ROL-AND zp */
			{
				int16 zpaddr=ZeroPgAddrModeHandler_Address();
				ROLInstrHandler(zpaddr);
				ANDInstrHandler(WholeRam[zpaddr]);
			}
			break;
			case 0x2f: /* Undocumented Instruction: ROL-AND abs */
			{
				int16 zpaddr=AbsAddrModeHandler_Address();
				ROLInstrHandler(zpaddr);
				ANDInstrHandler(WholeRam[zpaddr]);
			}
			break;
			case 0x33: /* Undocumented Instruction: ROL-AND (zp),Y */
			{
				int16 zpaddr=IndYAddrModeHandler_Address();
				ROLInstrHandler(zpaddr);
				ANDInstrHandler(WholeRam[zpaddr]);
			}
			break;
			case 0x37: /* Undocumented Instruction: ROL-AND zp,X */
			{
				int16 zpaddr=ZeroPgXAddrModeHandler_Address();
				ROLInstrHandler(zpaddr);
				ANDInstrHandler(WholeRam[zpaddr]);
			}
			break;
			case 0x3b: /* Undocumented Instruction: ROL-AND abs.Y */
			{
				int16 zpaddr=AbsYAddrModeHandler_Address();
				ROLInstrHandler(zpaddr);
				ANDInstrHandler(WholeRam[zpaddr]);
			}
			break;
			case 0x3f: /* Undocumented Instruction: ROL-AND abs.X */
			{
				int16 zpaddr=AbsXAddrModeHandler_Address();
				ROLInstrHandler(zpaddr);
				ANDInstrHandler(WholeRam[zpaddr]);
			}
			break;
			case 0x43: /* Undocumented Instruction: LSR-EOR (zp,X) */
			{
				int16 zpaddr=IndXAddrModeHandler_Address();
				LSRInstrHandler(zpaddr);
				EORInstrHandler(WholeRam[zpaddr]);
			}
			break;
			case 0x47: /* Undocumented Instruction: LSR-EOR zp */
			{
				int16 zpaddr=ZeroPgAddrModeHandler_Address();
				LSRInstrHandler(zpaddr);
				EORInstrHandler(WholeRam[zpaddr]);
			}
			break;
			case 0x4f: /* Undocumented Instruction: LSR-EOR abs */
			{
				int16 zpaddr=AbsAddrModeHandler_Address();
				LSRInstrHandler(zpaddr);
				EORInstrHandler(WholeRam[zpaddr]);
			}
			break;
			case 0x53: /* Undocumented Instruction: LSR-EOR (zp),Y */
			{
				int16 zpaddr=IndYAddrModeHandler_Address();
				LSRInstrHandler(zpaddr);
				EORInstrHandler(WholeRam[zpaddr]);
			}
			break;
			case 0x57: /* Undocumented Instruction: LSR-EOR zp,X */
			{
				int16 zpaddr=ZeroPgXAddrModeHandler_Address();
				LSRInstrHandler(zpaddr);
				EORInstrHandler(WholeRam[zpaddr]);
			}
			break;
			case 0x5b: /* Undocumented Instruction: LSR-EOR abs,Y */
			{
				int16 zpaddr=AbsYAddrModeHandler_Address();
				LSRInstrHandler(zpaddr);
				EORInstrHandler(WholeRam[zpaddr]);
			}
			break;
			case 0x5f: /* Undocumented Instruction: LSR-EOR abs,X */
			{
				int16 zpaddr=AbsXAddrModeHandler_Address();
				LSRInstrHandler(zpaddr);
				EORInstrHandler(WholeRam[zpaddr]);
			}
			break;
			case 0x44:
			case 0x54:
				ProgramCounter+=1;
				break;
			case 0x5c:
				ProgramCounter+=2;
				break;
			case 0x63: /* Undocumented Instruction: ROR-ADC (zp,X) */
			{
				int16 zpaddr=IndXAddrModeHandler_Address();
				RORInstrHandler(zpaddr);
				ADCInstrHandler(WholeRam[zpaddr]);
			}
			break;
			case 0x67: /* Undocumented Instruction: ROR-ADC zp */
			{
				int16 zpaddr=ZeroPgAddrModeHandler_Address();
				RORInstrHandler(zpaddr);
				ADCInstrHandler(WholeRam[zpaddr]);
			}
			break;
			case 0x6f: /* Undocumented Instruction: ROR-ADC abs */
			{
				int16 zpaddr=AbsAddrModeHandler_Address();
				RORInstrHandler(zpaddr);
				ADCInstrHandler(WholeRam[zpaddr]);
			}
			break;
			case 0x73: /* Undocumented Instruction: ROR-ADC (zp),Y */
			{
				int16 zpaddr=IndYAddrModeHandler_Address();
				RORInstrHandler(zpaddr);
				ADCInstrHandler(WholeRam[zpaddr]);
			}
			break;
			case 0x77: /* Undocumented Instruction: ROR-ADC zp,X */
			{
				int16 zpaddr=ZeroPgXAddrModeHandler_Address();
				RORInstrHandler(zpaddr);
				ADCInstrHandler(WholeRam[zpaddr]);
			}
			break;
			case 0x7b: /* Undocumented Instruction: ROR-ADC abs,Y */
			{
				int16 zpaddr=AbsYAddrModeHandler_Address();
				RORInstrHandler(zpaddr);
				ADCInstrHandler(WholeRam[zpaddr]);
			}
			break;
			case 0x7f: /* Undocumented Instruction: ROR-ADC abs,X */
			{
				int16 zpaddr=AbsXAddrModeHandler_Address();
				RORInstrHandler(zpaddr);
				ADCInstrHandler(WholeRam[zpaddr]);
			}
			break;
			// Undocumented DEC-CMP and INC-SBC Instructions
			case 0xc3: // DEC-CMP (zp,X)
			{
				int16 zpaddr=IndXAddrModeHandler_Address();
				DECInstrHandler(zpaddr);
				CMPInstrHandler(WholeRam[zpaddr]);
			}
			break;
			case 0xc7: // DEC-CMP zp
			{
				int16 zpaddr=ZeroPgAddrModeHandler_Address();
				DECInstrHandler(zpaddr);
				CMPInstrHandler(WholeRam[zpaddr]);
			}
			break;
			case 0xcf: // DEC-CMP abs
			{
				int16 zpaddr=AbsAddrModeHandler_Address();
				DECInstrHandler(zpaddr);
				CMPInstrHandler(WholeRam[zpaddr]);
			}
			break;
			case 0xd3: // DEC-CMP (zp),Y
			{
				int16 zpaddr=IndYAddrModeHandler_Address();
				DECInstrHandler(zpaddr);
				CMPInstrHandler(WholeRam[zpaddr]);
			}
			break;
			case 0xd7: // DEC-CMP zp,X
			{
				int16 zpaddr=ZeroPgXAddrModeHandler_Address();
				DECInstrHandler(zpaddr);
				CMPInstrHandler(WholeRam[zpaddr]);
			}
			break;
			case 0xdb: // DEC-CMP abs,Y
			{
				int16 zpaddr=AbsYAddrModeHandler_Address();
				DECInstrHandler(zpaddr);
				CMPInstrHandler(WholeRam[zpaddr]);
			}
			break;
			case 0xdf: // DEC-CMP abs,X
			{
				int16 zpaddr=AbsXAddrModeHandler_Address();
				DECInstrHandler(zpaddr);
				CMPInstrHandler(WholeRam[zpaddr]);
			}
			break;
			case 0xd4:
			case 0xf4:
				ProgramCounter+=1;
				break;
			case 0xdc:
			case 0xfc:
				ProgramCounter+=2;
				break;
			case 0xe3: // INC-SBC (zp,X)
			{
				int16 zpaddr=IndXAddrModeHandler_Address();
				INCInstrHandler(zpaddr);
				SBCInstrHandler(WholeRam[zpaddr]);
			}
			break;
			case 0xe7: // INC-SBC zp
			{
				int16 zpaddr=ZeroPgAddrModeHandler_Address();
				INCInstrHandler(zpaddr);
				SBCInstrHandler(WholeRam[zpaddr]);
			}
			break;
			case 0xef: // INC-SBC abs
			{
				int16 zpaddr=AbsAddrModeHandler_Address();
				INCInstrHandler(zpaddr);
				SBCInstrHandler(WholeRam[zpaddr]);
			}
			break;
			case 0xf3: // INC-SBC (zp).Y
			{
				int16 zpaddr=IndYAddrModeHandler_Address();
				INCInstrHandler(zpaddr);
				SBCInstrHandler(WholeRam[zpaddr]);
			}
			break;
			case 0xf7: // INC-SBC zp,X
			{
				int16 zpaddr=ZeroPgXAddrModeHandler_Address();
				INCInstrHandler(zpaddr);
				SBCInstrHandler(WholeRam[zpaddr]);
			}
			break;
			case 0xfb: // INC-SBC abs,Y
			{
				int16 zpaddr=AbsYAddrModeHandler_Address();
				INCInstrHandler(zpaddr);
				SBCInstrHandler(WholeRam[zpaddr]);
			}
			break;
			case 0xff: // INC-SBC abs,X
			{
				int16 zpaddr=AbsXAddrModeHandler_Address();
				INCInstrHandler(zpaddr);
				SBCInstrHandler(WholeRam[zpaddr]);
			}
			break;
			// REALLY Undocumented instructions 6B, 8B and CB
			case 0x6b:
				ANDInstrHandler(WholeRam[ProgramCounter++]);
				RORInstrHandler_Acc();
				break;
			case 0x8b:
				Accumulator=XReg; /* TXA */
				PSR&=~(FlagZ | FlagN);
				PSR|=((Accumulator==0)<<1) | (Accumulator & 128);
				ANDInstrHandler(WholeRam[ProgramCounter++]);
				break;
			case 0xcb:
				// SBX #n - I dont know if this uses the carry or not, i'm assuming its
				// Subtract #n from X with carry.
			{
				unsigned char TmpAcc=Accumulator;
				Accumulator=XReg;
				SBCInstrHandler(WholeRam[ProgramCounter++]);
				XReg=Accumulator;
				Accumulator=TmpAcc; // Fudge so that I dont have to do the whole SBC code again
			}
			break;
			default:
				BadCount++;
				break;
			}
		}
		if (OpCodes>=2) {
			switch (CurrentInstruction) {
			case 0x0b:
			case 0x2b:
				ANDInstrHandler(WholeRam[ProgramCounter++]); /* AND-MVC #n,b7 */
				PSR|=((Accumulator & 128)>>7);
				break;
			case 0x4b: /* Undocumented Instruction: AND imm and LSR A */
				ANDInstrHandler(WholeRam[ProgramCounter++]);
				LSRInstrHandler_Acc();
				break;
			case 0x87: /* Undocumented Instruction: SAX zp (i.e. (zp) = A & X) */
				/* This one does not seem to change the processor flags */
				WholeRam[ZeroPgAddrModeHandler_Address()] = Accumulator & XReg;
				break;
			case 0x83: /* Undocumented Instruction: SAX (zp,X) */
				WholeRam[IndXAddrModeHandler_Address()] = Accumulator & XReg;
				break;
			case 0x8f: /* Undocumented Instruction: SAX abs */
				WholeRam[AbsAddrModeHandler_Address()] = Accumulator & XReg;
				break;
			case 0x93: /* Undocumented Instruction: SAX (zp),Y */
				WholeRam[IndYAddrModeHandler_Address()] = Accumulator & XReg;
				break;
			case 0x97: /* Undocumented Instruction: SAX zp,Y */
				WholeRam[ZeroPgYAddrModeHandler_Address()] = Accumulator & XReg;
				break;
			case 0x9b: /* Undocumented Instruction: SAX abs,Y */
				WholeRam[AbsYAddrModeHandler_Address()] = Accumulator & XReg;
				break;
			case 0x9f: /* Undocumented Instruction: SAX abs,X */
				WholeRam[AbsXAddrModeHandler_Address()] = Accumulator & XReg;
				break;
			case 0xab: /* Undocumented Instruction: LAX #n */
				LDAInstrHandler(WholeRam[ProgramCounter++]);
				XReg = Accumulator;
				break;
			case 0xa3: /* Undocumented Instruction: LAX (zp,X) */
				LDAInstrHandler(IndXAddrModeHandler_Data());
				XReg = Accumulator;
				break;
			case 0xa7: /* Undocumented Instruction: LAX zp */
				LDAInstrHandler(WholeRam[WholeRam[ProgramCounter++]]);
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
			default:
				BadCount++;
			}
		}
		if (BadCount==OpCodes)
			BadInstrHandler(CurrentInstruction);

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
				Cycles++;
				if ((ProgramCounter & 0xff00) != ((OldPC+2) & 0xff00))
					Cycles+=1;
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

		if (EnableTube)
			SyncTubeProcessor();
	}
} /* Exec6502Instruction */


void PollVIAs(unsigned int nCycles)
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

void PollHardware(unsigned int nCycles)
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
		if (EnableTube)
			WrapTubeCycles();
	}

	VideoPoll(nCycles);
	if (!BHardware) {
		AtoD_poll(nCycles);
		Serial_Poll();
	}
	Disc8271_poll(nCycles);
	Sound_Trigger(nCycles);
	if (DisplayCycles>0) DisplayCycles-=nCycles; // Countdown time till end of display of info.
	if ((MachineType==3) || (!NativeFDC)) Poll1770(nCycles); // Do 1770 Background stuff

	if (EconetEnabled && EconetPoll()) {
		if (EconetNMIenabled ) { 
			NMIStatus|=1<<nmi_econet;
			if (DebugEnabled)
				DebugDisplayTrace(DEBUG_ECONET, true, "Econet: NMI asserted");
		}
		else {
			if (DebugEnabled)
				DebugDisplayTrace(DEBUG_ECONET, true, "Econet: NMI requested but supressed");
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
	int Dlong;
	ProgramCounter=fget16(SUEF);
	Accumulator=fgetc(SUEF);
	XReg=fgetc(SUEF);
	YReg=fgetc(SUEF);
	StackReg=fgetc(SUEF);
	PSR=fgetc(SUEF);
	//TotalCycles=fget32(SUEF);
	Dlong=fget32(SUEF);
	intStatus=fgetc(SUEF);
	NMIStatus=fgetc(SUEF);
	NMILock=fgetc(SUEF);
	//AtoDTrigger=Disc8271Trigger=AMXTrigger=PrinterTrigger=VideoTriggerCount=TotalCycles+100;
}

/*-------------------------------------------------------------------------*/
/* Dump state                                                              */
void core_dumpstate(void) {
  cerr << "core:\n";
  DumpRegs();
}; /* core_dumpstate */
