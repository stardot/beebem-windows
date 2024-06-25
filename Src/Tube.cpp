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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <fstream>
#include <iostream>

#include "Tube.h"
#include "6502core.h"
#include "Arm.h"
#include "BeebMem.h"
#include "Debug.h"
#include "Log.h"
#include "Main.h"
#include "SprowCoPro.h"
#include "UefState.h"
#include "Z80mem.h"
#include "Z80.h"

#ifdef WIN32
#include <windows.h>
#define INLINE inline
#else
#define INLINE
#endif

// Some interrupt set macros
#define SETTUBEINT(a) TubeintStatus |= 1 << a
#define RESETTUBEINT(a) TubeintStatus &= ~(1 << a)

static int CurrentInstruction;
static unsigned char TubeRam[65536];
TubeDevice TubeType;

static CycleCountT TotalTubeCycles = 0;

static int old_readHIOAddr = 0;
static unsigned char old_readHTmpData = 0;

static int old_readPIOAddr = 0;
static unsigned char old_readPTmpData = 0;

int TubeProgramCounter;
static int TubePrePC; // Previous Tube Program Counter
static unsigned char Accumulator, XReg, YReg;
static unsigned char StackReg, PSR;
static unsigned char IRQCycles;

unsigned char TubeintStatus = 0; // bit set (nums in IRQ_Nums) if interrupt being caused
unsigned char TubeNMIStatus = 0; // bit set (nums in NMI_Nums) if NMI being caused
static bool TubeNMILock = false; // Well I think NMI's are maskable - to stop repeated NMI's - the lock is released when an RTI is done
static unsigned char OldTubeNMIStatus;

#define GETCFLAG ((PSR & FlagC) != 0)
#define GETZFLAG ((PSR & FlagZ) != 0)
#define GETIFLAG ((PSR & FlagI) != 0)
#define GETDFLAG ((PSR & FlagD) != 0)
#define GETBFLAG ((PSR & FlagB) != 0)
#define GETVFLAG ((PSR & FlagV) != 0)
#define GETNFLAG ((PSR & FlagN) != 0)

static const int TubeCyclesTable[] = {
/*0 1 2 3 4 5 6 7 8 9 a b c d e f */
  7,6,2,1,5,3,5,5,3,2,2,1,6,4,6,5, /* 0 */
  2,5,5,1,5,4,6,5,2,4,2,1,6,4,6,5, /* 1 */
  6,6,2,1,3,3,5,5,4,2,2,1,4,4,6,5, /* 2 */
  2,5,5,1,4,4,6,5,2,4,2,1,4,4,6,5, /* 3 */
  6,6,2,1,3,3,5,5,3,2,2,1,3,4,6,5, /* 4 */
  2,5,5,1,4,4,6,5,2,4,3,1,8,4,6,5, /* 5 */
  6,6,2,1,3,3,5,5,4,2,2,1,6,4,6,5, /* 6 */
  2,5,5,1,4,4,6,5,2,4,4,1,6,4,6,5, /* 7 */
  3,6,2,1,3,3,3,5,2,2,2,1,4,4,4,5, /* 8 */
  2,6,5,1,4,4,4,5,2,5,2,1,4,5,5,5, /* 9 */
  2,6,2,1,3,3,3,5,2,2,2,1,4,4,4,5, /* a */
  2,5,5,1,4,4,4,5,2,4,2,1,4,4,4,5, /* b */
  2,6,2,1,3,3,5,5,2,2,2,1,4,4,6,5, /* c */
  2,5,5,1,4,4,6,5,2,4,3,1,4,4,7,5, /* d */
  2,6,2,1,3,3,5,5,2,2,2,1,4,4,6,5, /* e */
  2,5,5,1,4,4,6,5,2,4,4,1,4,4,7,5  /* f */
};

/* The number of TubeCycles to be used by the current instruction - exported to
   allow fernangling by memory subsystem */
static unsigned int TubeCycles;

static bool Branched; // true if the instruction branched

/* A macro to speed up writes - uses a local variable called 'tmpaddr' */
#define TUBEREADMEM_FAST(a) ((a<0xfef8)?TubeRam[a]:TubeReadMem(a))
#define TUBEWRITEMEM_FAST(Address, Value) if (Address<0xfef8) TubeRam[Address]=Value; else TubeWriteMem(Address,Value);
#define TUBEWRITEMEM_DIRECT(Address, Value) TubeRam[Address]=Value;
#define TUBEFASTWRITE(addr,val) tmpaddr=addr; if (tmpaddr<0xfef8) TUBEWRITEMEM_DIRECT(tmpaddr,val) else TubeWriteMem(tmpaddr,val);

// Local fns
static void Reset65C02();

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

unsigned char R1PHData[TubeBufferLength * 2];
int R1PHPtr;
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

static void UpdateR1Interrupt()
{
	if ((R1Status & TubeI) && (R1PStatus & TubeDataAv))
		SETTUBEINT(R1);
	else
		RESETTUBEINT(R1);
}

static void UpdateR4Interrupt()
{
	if ((R1Status & TubeJ) && (R4PStatus & TubeDataAv))
		SETTUBEINT(R4);
	else
		RESETTUBEINT(R4);
}

static void UpdateR3Interrupt()
{
	if ((R1Status & TubeM) && !(R1Status & TubeV) &&
		( (R3HPPtr > 0) || (R3PHPtr == 0) ))
		TubeNMIStatus|=(1<<R3);
	else if ((R1Status & TubeM) && (R1Status & TubeV) &&
		( (R3HPPtr > 1) || (R3PHPtr == 0) ))
		TubeNMIStatus|=(1<<R3);
	else
		TubeNMIStatus&=~(1<<R3);
}

static void UpdateHostR4Interrupt()
{
	if ((R1Status & TubeQ) && (R4HStatus & TubeDataAv))
		intStatus|=(1<<tube);
	else
		intStatus&=~(1<<tube);
}

/*-------------------------------------------------------------------*/
// Torch tube memory/io handling functions

static bool TorchTubeActive = false;

void UpdateInterrupts()
{
	UpdateR1Interrupt();
	UpdateR3Interrupt();
	UpdateR4Interrupt();
	UpdateHostR4Interrupt();
}

unsigned char ReadTorchTubeFromHostSide(int IOAddr)
{
	unsigned char TmpData = 0xff;

	if (!TorchTubeActive)
		return MachineType == Model::Master128 ? 0xff : 0xfe;

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
		// trace = 1;
		break;
	}

	// WriteLog("ReadTorchTubeFromHostSide - Addr = %02x, Value = %02x\n", (int)IOAddr, (int)TmpData);

	if (DebugEnabled)
	{
		DebugDisplayTraceF(DebugType::Tube, true,
		                   "Tube: Read from host, addr %X value %02X",
		                   (int)IOAddr, (int)TmpData);
	}

	return TmpData;
}

void WriteTorchTubeFromHostSide(int IOAddr,unsigned char IOData)
{
	// WriteLog("WriteTorchTubeFromHostSide - Addr = %02x, Value = %02x\n", (int)IOAddr, (int)IOData);

	if (DebugEnabled)
	{
		DebugDisplayTraceF(DebugType::Tube, true,
		                   "Tube: Write from host, addr %X value %02X",
		                   IOAddr, (int)IOData);
	}

	if (IOAddr == 0x02 && IOData == 0xff)
	{
		TorchTubeActive = true;
	}

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
		// WriteTorchTubeFromHostSide(1, IOData);
		break;

	case 0x0c :
		if (IOData == 0xaa)
		{
			init_z80();
		}
		break;

	case 0x0e:
		break;
	}
}

unsigned char ReadTorchTubeFromParasiteSide(int IOAddr)
{
	unsigned char TmpData = 0;

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

	// WriteLog("ReadTorchTubeFromParasiteSide - Addr = %02x, Value = %02x\n", (int)IOAddr, (int)TmpData);

	if (DebugEnabled)
	{
		DebugDisplayTraceF(DebugType::Tube, false,
		                   "Tube: Read from para, addr %X value %02X",
		                   IOAddr, (int)TmpData);
	}

	return TmpData;
}

void WriteTorchTubeFromParasiteSide(int IOAddr,unsigned char IOData)
{
	// WriteLog("WriteTorchTubeFromParasiteSide - Addr = %02x, Value = %02x\n", (int)IOAddr, (int)IOData);

	if (DebugEnabled)
	{
		DebugDisplayTraceF(DebugType::Tube, false,
		                   "Tube: Write from para, addr %X value %02X",
		                   IOAddr, (int)IOData);
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

unsigned char ReadTubeFromHostSide(int IOAddr) {
	unsigned char TmpData,TmpCntr;

	if (TubeType == TubeDevice::None)
		return (MachineType == Model::Master128 || MachineType == Model::MasterET) ? 0xff : 0xfe;

	switch (IOAddr) {
	case 0:
		TmpData=R1HStatus | R1Status;
		break;
	case 1:
		TmpData=R1PHData[0];
		if (R1PHPtr>0) {
			for (TmpCntr=1;TmpCntr<TubeBufferLength;TmpCntr++)
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

	default:
		return (MachineType == Model::Master128 || MachineType == Model::MasterET) ? 0xff : 0xfe;
	}

	if (DebugEnabled && (old_readHIOAddr != IOAddr || old_readHTmpData != TmpData))
	{
		DebugDisplayTraceF(DebugType::Tube, true,
		                   "Tube: Read from host, addr %X value %02X",
		                   IOAddr, (int)TmpData);
	}

	old_readHTmpData = TmpData;
	old_readHIOAddr = IOAddr;

	return TmpData;
}

void WriteTubeFromHostSide(int IOAddr, unsigned char IOData) {
	if (TubeType == TubeDevice::None)
		return;

	if (DebugEnabled)
	{
		DebugDisplayTraceF(DebugType::Tube, true,
		                   "Tube: Write from host, addr %X value %02X",
		                   IOAddr, (int)IOData);
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

	// UpdateInterrupts();
}

unsigned char ReadTubeFromParasiteSide(int IOAddr)
{
	if (TubeType == TubeDevice::TorchZ80)
		return ReadTorchTubeFromHostSide(IOAddr);

	unsigned char TmpData;

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

	default:
		return 0;
	}

	// UpdateInterrupts();

	if (DebugEnabled && (old_readPIOAddr != IOAddr || old_readPTmpData != TmpData))
	{
		DebugDisplayTraceF(DebugType::Tube, false,
		                   "Tube: Read from para, addr %X value %02X",
		                   (int)IOAddr, (int)TmpData);
	}

	old_readPTmpData = TmpData;
	old_readPIOAddr = IOAddr;

	// UpdateInterrupts();
	return TmpData;
}

void WriteTubeFromParasiteSide(int IOAddr, unsigned char IOData)
{
	if (TubeType == TubeDevice::TorchZ80)
	{
		WriteTorchTubeFromParasiteSide(IOAddr, IOData);
		return;
	}

	if (DebugEnabled)
	{
		DebugDisplayTraceF(DebugType::Tube, false,
		                   "Tube: Write from para, addr %X value %02X",
		                   (int)IOAddr, (int)IOData);
	}

	switch (IOAddr) {
	case 0:
		// Cannot write status flags from parasite
		break;
	case 1:
		if (R1PHPtr<TubeBufferLength) {
			R1PHData[R1PHPtr++]=IOData;
			R1HStatus|=TubeDataAv;
			if (R1PHPtr==TubeBufferLength)
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

	// UpdateInterrupts();
}

/*----------------------------------------------------------------------------*/
void TubeWriteMem(int IOAddr, unsigned char IOData) {
	if (IOAddr>=0xff00 || IOAddr<0xfef8)
		TubeRam[IOAddr]=IOData;
	else
		WriteTubeFromParasiteSide(IOAddr-0xfef8,IOData);
}

unsigned char TubeReadMem(int IOAddr) {
	if (IOAddr>=0xff00 || IOAddr<0xfef8)
		return(TubeRam[IOAddr]);
	else
		return(ReadTubeFromParasiteSide(IOAddr-0xfef8));
}

// Get a two byte address from the program counter, and then post inc
// the program counter
#define GETTWOBYTEFROMPC(var) \
	var = TubeRam[TubeProgramCounter++]; \
	var |= (TubeRam[TubeProgramCounter++] << 8)

/*----------------------------------------------------------------------------*/

INLINE void Carried()
{
	// Correct cycle count for indirection across page boundary
	if (((CurrentInstruction & 0xf) == 0x1 ||
	     (CurrentInstruction & 0xf) == 0x9 ||
	     (CurrentInstruction & 0xf) == 0xd) &&
	     (CurrentInstruction & 0xf0) != 0x90)
	{
		TubeCycles++;
	}
	else if (CurrentInstruction == 0xBC ||
	         CurrentInstruction == 0xBE)
	{
		TubeCycles++;
	}
}

/*----------------------------------------------------------------------------*/

// Set the Z flag if 'in' is 0, and N if bit 7 is set - leave all other bits
// untouched.

INLINE static void SetPSRZN(const unsigned char Value)
{
	PSR &= ~(FlagZ | FlagN);
	PSR |= ((Value == 0) << 1) | (Value & 0x80);
}

/*----------------------------------------------------------------------------*/

// Note: n is 128 for true - not 1

INLINE static void SetPSR(int Mask, int c, int z, int i, int d, int b, int v, int n)
{
	PSR &= ~Mask;
	PSR |= c | (z << 1) | (i << 2) | (d << 3) | (b << 4) | (v << 6) | n;
}

/*----------------------------------------------------------------------------*/

// NOTE!!!!! n is 128 or 0 - not 1 or 0

INLINE static void SetPSRCZN(int c, int z, int n)
{
	PSR &= ~(FlagC | FlagZ | FlagN);
	PSR |= c | (z << 1) | n;
}

/*----------------------------------------------------------------------------*/

INLINE static void Push(unsigned char Value)
{
	TUBEWRITEMEM_DIRECT(0x100 + StackReg, Value);
	StackReg--;
}

/*----------------------------------------------------------------------------*/

INLINE static unsigned char Pop()
{
	StackReg++;
	return TubeRam[0x100 + StackReg];
}

/*----------------------------------------------------------------------------*/

INLINE static void PushWord(int Value)
{
	Push((Value >> 8) & 0xff);
	Push(Value & 0xff);
}

/*----------------------------------------------------------------------------*/

INLINE static int PopWord()
{
	int Value = Pop();
	Value |= Pop() << 8;
	return Value;
}

/*-------------------------------------------------------------------------*/

// Relative addressing mode handler

INLINE static int RelAddrModeHandler_Data()
{
	// For branches - is this correct - i.e. is the program counter incremented
	// at the correct time?
	int EffectiveAddress = (signed char)TubeRam[TubeProgramCounter++];
	EffectiveAddress += TubeProgramCounter;

	return EffectiveAddress;
}

/*----------------------------------------------------------------------------*/

INLINE static void ADCInstrHandler(unsigned char Operand)
{
	// NOTE! Not sure about C and V flags
	if (!GETDFLAG)
	{
		int TmpResultC = Accumulator + Operand + GETCFLAG;
		int TmpResultV = (signed char)Accumulator + (signed char)Operand + GETCFLAG;
		Accumulator = TmpResultC & 255;
		SetPSR(FlagC | FlagZ | FlagV | FlagN, (TmpResultC & 256) != 0,
			Accumulator == 0, 0, 0, 0, ((Accumulator & 128) != 0) ^ (TmpResultV < 0),
			Accumulator & 128);
	}
	else
	{
		// Z flag determined from 2's compl result, not BCD result!
		int TmpResult = Accumulator + Operand + GETCFLAG;
		int ZFlag = (TmpResult & 0xff) == 0;

		int ln = (Accumulator & 0xf) + (Operand & 0xf) + GETCFLAG;

		int TmpCarry = 0;

		if (ln > 9)
		{
			ln += 6;
			ln &= 0xf;
			TmpCarry = 0x10;
		}

		int hn = (Accumulator & 0xf0) + (Operand & 0xf0) + TmpCarry;
		/* N and V flags are determined before high nibble is adjusted.
		NOTE: V is not always correct */
		int NFlag = hn & 128;
		int VFlag = (hn ^ Accumulator) & 128 && !((Accumulator ^ Operand) & 128);

		int CFlag = 0;

		if (hn > 0x90)
		{
			hn += 0x60;
			hn &= 0xf0;
			CFlag = 1;
		}

		Accumulator = (unsigned char)(hn | ln);

		ZFlag = Accumulator == 0;
		NFlag = Accumulator & 128;

		SetPSR(FlagC | FlagZ | FlagV | FlagN, CFlag, ZFlag, 0, 0, 0, VFlag, NFlag);
	}
}

/*----------------------------------------------------------------------------*/

INLINE static void ANDInstrHandler(unsigned char Operand)
{
	Accumulator &= Operand;
	PSR &= ~(FlagZ | FlagN);
	PSR |= ((Accumulator == 0) << 1) | (Accumulator & 128);
}

INLINE static void ASLInstrHandler(int Address)
{
	unsigned char OldValue = TUBEREADMEM_FAST(Address);
	unsigned char NewValue = OldValue << 1;
	TUBEWRITEMEM_FAST(Address, NewValue);
	SetPSRCZN((OldValue & 128) != 0, NewValue == 0, NewValue & 128);
}

INLINE static void TRBInstrHandler(int Address)
{
	unsigned char OldValue = TUBEREADMEM_FAST(Address);
	unsigned char NewValue = (Accumulator ^ 0xff) & OldValue;
	TUBEWRITEMEM_FAST(Address, NewValue);
	PSR &= ~FlagZ;
	PSR |= ((Accumulator & OldValue) == 0) ? FlagZ : 0;
}

INLINE static void TSBInstrHandler(int Address)
{
	unsigned char OldValue = TUBEREADMEM_FAST(Address);
	unsigned char NewValue = Accumulator | OldValue;
	TUBEWRITEMEM_FAST(Address, NewValue);
	PSR &= ~FlagZ;
	PSR |= ((Accumulator & OldValue) == 0) ? FlagZ : 0;
}

INLINE static void ASLInstrHandler_Acc()
{
	unsigned char OldValue = Accumulator;
	Accumulator <<= 1;
	SetPSRCZN((OldValue & 128) != 0, Accumulator == 0, Accumulator & 128);
}

INLINE static void BCCInstrHandler()
{
	if (!GETCFLAG)
	{
		TubeProgramCounter = RelAddrModeHandler_Data();
		Branched = true;
	}
	else
	{
		TubeProgramCounter++;
	}
}

INLINE static void BCSInstrHandler()
{
	if (GETCFLAG)
	{
		TubeProgramCounter = RelAddrModeHandler_Data();
		Branched = true;
	}
	else
	{
		TubeProgramCounter++;
	}
}

INLINE static void BEQInstrHandler()
{
	if (GETZFLAG)
	{
		TubeProgramCounter = RelAddrModeHandler_Data();
		Branched = true;
	}
	else
	{
		TubeProgramCounter++;
	}
}

INLINE static void BITInstrHandler(unsigned char Operand)
{
	PSR &= ~(FlagZ | FlagN | FlagV);
	// z if result 0, and NV to top bits of operand
	PSR |= (((Accumulator & Operand) == 0) << 1) | (Operand & 0xc0);
}

INLINE static void BITImmedInstrHandler(unsigned char Operand)
{
	PSR &= ~FlagZ;
	// Z if result 0, and NV to top bits of operand
	PSR |= ((Accumulator & Operand) == 0) << 1;
}

INLINE static void BMIInstrHandler()
{
	if (GETNFLAG)
	{
		TubeProgramCounter = RelAddrModeHandler_Data();
		Branched = true;
	}
	else
	{
		TubeProgramCounter++;
	}
}

INLINE static void BNEInstrHandler()
{
	if (!GETZFLAG)
	{
		TubeProgramCounter = RelAddrModeHandler_Data();
		Branched = true;
	}
	else
	{
		TubeProgramCounter++;
	}
}

INLINE static void BPLInstrHandler()
{
	if (!GETNFLAG)
	{
		TubeProgramCounter = RelAddrModeHandler_Data();
		Branched = true;
	}
	else
	{
		TubeProgramCounter++;
	}
}

INLINE static void BRKInstrHandler()
{
	PushWord(TubeProgramCounter + 1);
	PSR |= FlagB; // Set B before pushing
	Push(PSR);
	PSR |= FlagI; // Set I after pushing - see Birnbaum
	TubeProgramCounter = TubeReadMem(0xfffe) | (TubeReadMem(0xffff) << 8);
}

INLINE static void BVCInstrHandler()
{
	if (!GETVFLAG)
	{
		TubeProgramCounter = RelAddrModeHandler_Data();
		Branched = true;
	}
	else
	{
		TubeProgramCounter++;
	}
}

INLINE static void BVSInstrHandler()
{
	if (GETVFLAG)
	{
		TubeProgramCounter = RelAddrModeHandler_Data();
		Branched = true;
	}
	else
	{
		TubeProgramCounter++;
	}
}

INLINE static void BRAInstrHandler()
{
	TubeProgramCounter = RelAddrModeHandler_Data();
	Branched = true;
}

INLINE static void CMPInstrHandler(unsigned char Operand)
{
	unsigned char Result = Accumulator - Operand;
	unsigned char CFlag = (Accumulator >= Operand) ? FlagC : 0;
	SetPSRCZN(CFlag, Accumulator == Operand, Result & 0x80);
}

INLINE static void CPXInstrHandler(unsigned char Operand)
{
	unsigned char Result = XReg - Operand;
	SetPSRCZN(XReg >= Operand, XReg == Operand, Result & 0x80);
}

INLINE static void CPYInstrHandler(unsigned char Operand)
{
	unsigned char Result = YReg - Operand;
	SetPSRCZN(YReg >= Operand, YReg == Operand, Result & 0x80);
}

INLINE static void DECInstrHandler(int Address)
{
	unsigned char Value = TUBEREADMEM_FAST(Address);
	Value--;
	TUBEWRITEMEM_FAST(Address, Value);
	SetPSRZN(Value);
}

INLINE static void DEAInstrHandler()
{
	Accumulator--;
	SetPSRZN(Accumulator);
}

INLINE static void DEXInstrHandler()
{
	XReg--;
	SetPSRZN(XReg);
}

INLINE static void DEYInstrHandler()
{
	YReg--;
	SetPSRZN(YReg);
}

INLINE static void EORInstrHandler(unsigned char Operand)
{
	Accumulator ^= Operand;
	SetPSRZN(Accumulator);
}

INLINE static void INCInstrHandler(int Address)
{
	unsigned char Value = TUBEREADMEM_FAST(Address);
	Value++;
	TUBEWRITEMEM_FAST(Address, Value);
	SetPSRZN(Value);
}

INLINE static void INAInstrHandler()
{
	Accumulator++;
	SetPSRZN(Accumulator);
}

INLINE static void INXInstrHandler()
{
	XReg++;
	SetPSRZN(XReg);
}

INLINE static void INYInstrHandler()
{
	YReg++;
	SetPSRZN(YReg);
}

INLINE static void JSRInstrHandler(int Address)
{
	PushWord(TubeProgramCounter - 1);
	TubeProgramCounter = Address;
}

INLINE static void LDAInstrHandler(unsigned char Operand)
{
	Accumulator = Operand;
	SetPSRZN(Accumulator);
}

INLINE static void LDXInstrHandler(unsigned char Operand)
{
	XReg = Operand;
	SetPSRZN(XReg);
}

INLINE static void LDYInstrHandler(unsigned char Operand)
{
	YReg = Operand;
	SetPSRZN(YReg);
}

INLINE static void LSRInstrHandler(int Address)
{
	unsigned char OldValue = TUBEREADMEM_FAST(Address);
	unsigned char NewValue = OldValue >> 1;
	TUBEWRITEMEM_FAST(Address, NewValue);
	SetPSRCZN((OldValue & 1) != 0, NewValue == 0, 0);
}

INLINE static void LSRInstrHandler_Acc()
{
	unsigned char OldValue = Accumulator;
	Accumulator >>= 1;
	SetPSRCZN((OldValue & 1) != 0, Accumulator == 0, 0);
}

INLINE static void ORAInstrHandler(unsigned char Operand)
{
	Accumulator |= Operand;
	SetPSRZN(Accumulator);
}

INLINE static void ROLInstrHandler(int Address)
{
	unsigned char OldValue = TUBEREADMEM_FAST(Address);
	unsigned char NewValue = (OldValue << 1) + GETCFLAG;
	TUBEWRITEMEM_FAST(Address, NewValue);
	SetPSRCZN((OldValue & 0x80) != 0, NewValue == 0, NewValue & 0x80);
}

INLINE static void ROLInstrHandler_Acc()
{
	unsigned char OldValue = Accumulator;
	Accumulator = (OldValue << 1) + GETCFLAG;
	SetPSRCZN((OldValue & 0x80) != 0, Accumulator == 0, Accumulator & 0x80);
}

INLINE static void RORInstrHandler(int Address)
{
	unsigned char OldValue = TUBEREADMEM_FAST(Address);
	unsigned char NewValue = (OldValue >> 1) + (GETCFLAG << 7);
	TUBEWRITEMEM_FAST(Address, NewValue);
	SetPSRCZN(OldValue & 1, NewValue == 0, NewValue & 0x80);
}

INLINE static void RORInstrHandler_Acc()
{
	unsigned char OldValue = Accumulator;
	Accumulator = (OldValue >> 1) + (GETCFLAG << 7);
	SetPSRCZN(OldValue & 1, Accumulator == 0, Accumulator & 0x80);
}

INLINE static void SBCInstrHandler(unsigned char Operand)
{
	// NOTE! Not sure about C and V flags
	if (!GETDFLAG)
	{
		int TmpResultV = (signed char)Accumulator - (signed char)Operand -(1 - GETCFLAG);
		int TmpResultC = Accumulator - Operand - (1 - GETCFLAG);
		Accumulator = TmpResultC & 255;
		SetPSR(FlagC | FlagZ | FlagV | FlagN, TmpResultC >= 0,
			Accumulator == 0, 0, 0, 0,
			((Accumulator & 128) != 0) ^ ((TmpResultV & 256) != 0),
			Accumulator & 128);
	}
	else
	{
		// int ohn = Operand & 0xf0;
		int oln = Operand & 0x0f;

		int ln = (Accumulator & 0xf) - oln - (1 - GETCFLAG);
		int TmpResult = Accumulator - Operand - (1 - GETCFLAG);

		int TmpResultV = (signed char)Accumulator - (signed char)Operand - (1 - GETCFLAG);
		int VFlag = ((TmpResultV < -128) || (TmpResultV > 127));

		int CFlag = (TmpResult & 256) == 0;

		if (TmpResult < 0)
		{
			TmpResult -= 0x60;
		}

		if (ln < 0)
		{
			TmpResult -= 0x06;
		}

		int NFlag = TmpResult & 128;
		Accumulator = TmpResult & 0xFF;
		int ZFlag = (Accumulator == 0);

		SetPSR(FlagC | FlagZ | FlagV | FlagN, CFlag, ZFlag, 0, 0, 0, VFlag, NFlag);
	}
}

INLINE static void STXInstrHandler(int Address)
{
	TUBEWRITEMEM_FAST(Address, XReg);
}

INLINE static void STYInstrHandler(int Address)
{
	TUBEWRITEMEM_FAST(Address, YReg);
}

/*-------------------------------------------------------------------------*/

// The RMB, SMB, BBR, and BBS instructions are specific to the 65C02,
// used in the Acorn 6502 co-processor. They are not implemented in the
// 65SC02, used in the Master 128.

static void ResetMemoryBit(int bit)
{
	const int EffectiveAddress = TubeRam[TubeProgramCounter++];

	TUBEWRITEMEM_DIRECT(EffectiveAddress, TubeRam[EffectiveAddress] & ~(1 << bit));
}

static void SetMemoryBit(int bit)
{
	const int EffectiveAddress = TubeRam[TubeProgramCounter++];

	TUBEWRITEMEM_DIRECT(EffectiveAddress, TubeRam[EffectiveAddress] | (1 << bit));
}

static void BranchOnBitReset(int bit)
{
	const int EffectiveAddress = TubeRam[TubeProgramCounter++];
	const int Offset = TubeRam[TubeProgramCounter++];

	if ((TubeRam[EffectiveAddress] & (1 << bit)) == 0) {
		TubeProgramCounter += Offset;
	}
}

static void BranchOnBitSet(int bit)
{
	const int EffectiveAddress = TubeRam[TubeProgramCounter++];
	const int Offset = TubeRam[TubeProgramCounter++];

	if (TubeRam[EffectiveAddress] & (1 << bit)) {
		TubeProgramCounter += Offset;
	}
}

/*-------------------------------------------------------------------------*/

// Absolute addressing mode handler

INLINE static unsigned char AbsAddrModeHandler_Data()
{
	// Get the address from after the instruction.
	int FullAddress;
	GETTWOBYTEFROMPC(FullAddress);

	// And then read it.
	return TUBEREADMEM_FAST(FullAddress);
}

/*-------------------------------------------------------------------------*/

// Absolute addressing mode handler

INLINE static int AbsAddrModeHandler_Address()
{
	// Get the address from after the instruction.
	int FullAddress;
	GETTWOBYTEFROMPC(FullAddress);

	// And then read it.
	return FullAddress;
}

/*-------------------------------------------------------------------------*/

// Zero page addressing mode handler

INLINE static int ZeroPgAddrModeHandler_Address()
{
	return TubeRam[TubeProgramCounter++];
}

/*-------------------------------------------------------------------------*/

// Indexed with X preinc addressing mode handler

INLINE static unsigned char IndXAddrModeHandler_Data()
{
	unsigned char ZeroPageAddress = TubeRam[TubeProgramCounter++] + XReg;
	int EffectiveAddress = TubeRam[ZeroPageAddress] | (TubeRam[ZeroPageAddress + 1] << 8);
	return TUBEREADMEM_FAST(EffectiveAddress);
}

/*-------------------------------------------------------------------------*/

// Indexed with X preinc addressing mode handler

INLINE static int IndXAddrModeHandler_Address()
{
	unsigned char ZeroPageAddress = TubeRam[TubeProgramCounter++] + XReg;
	int EffectiveAddress = TubeRam[ZeroPageAddress] | (TubeRam[ZeroPageAddress + 1] << 8);
	return EffectiveAddress;
}

/*-------------------------------------------------------------------------*/

// Indexed with Y postinc addressing mode handler

INLINE static unsigned char IndYAddrModeHandler_Data()
{
	unsigned char ZeroPageAddress = TubeRam[TubeProgramCounter++];
	int EffectiveAddress = TubeRam[ZeroPageAddress] + YReg;

	if (EffectiveAddress > 0xff)
	{
		Carried();
	}

	EffectiveAddress += TubeRam[(unsigned char)(ZeroPageAddress + 1)] << 8;
	EffectiveAddress &= 0xffff;

	return TUBEREADMEM_FAST(EffectiveAddress);
}

/*-------------------------------------------------------------------------*/

// Indexed with Y postinc addressing mode handler

INLINE static int IndYAddrModeHandler_Address()
{
	unsigned char ZeroPageAddress = TubeRam[TubeProgramCounter++];
	uint16_t EffectiveAddress = TubeRam[ZeroPageAddress] + YReg;

	if (EffectiveAddress > 0xff)
	{
		Carried();
	}

	EffectiveAddress += TubeRam[(unsigned char)(ZeroPageAddress + 1)] << 8;

	return EffectiveAddress;
}

/*-------------------------------------------------------------------------*/

// Zero page wih X offset addressing mode handler

INLINE static unsigned char ZeroPgXAddrModeHandler_Data()
{
	unsigned char EffectiveAddress = TubeRam[TubeProgramCounter++] + XReg;
	return TubeRam[EffectiveAddress];
}

/*-------------------------------------------------------------------------*/

// Zero page wih X offset addressing mode handler

INLINE static int ZeroPgXAddrModeHandler_Address()
{
	return (TubeRam[TubeProgramCounter++] + XReg) & 0xff;
}

/*-------------------------------------------------------------------------*/

// Absolute with X offset addressing mode handler

INLINE static unsigned char AbsXAddrModeHandler_Data()
{
	int EffectiveAddress;
	GETTWOBYTEFROMPC(EffectiveAddress);

	if ((EffectiveAddress & 0xff00) != ((EffectiveAddress + XReg) & 0xff00))
	{
		Carried();
	}

	EffectiveAddress += XReg;
	EffectiveAddress &= 0xffff;

	return TUBEREADMEM_FAST(EffectiveAddress);
}

/*-------------------------------------------------------------------------*/

// Absolute with X offset addressing mode handler

INLINE static int AbsXAddrModeHandler_Address()
{
	int EffectiveAddress;
	GETTWOBYTEFROMPC(EffectiveAddress);

	if ((EffectiveAddress & 0xff00) != ((EffectiveAddress + XReg) & 0xff00))
	{
		Carried();
	}

	EffectiveAddress += XReg;
	EffectiveAddress &= 0xffff;

	return EffectiveAddress;
}

/*-------------------------------------------------------------------------*/

// Absolute with Y offset addressing mode handler

INLINE static unsigned char AbsYAddrModeHandler_Data()
{
	int EffectiveAddress;
	GETTWOBYTEFROMPC(EffectiveAddress);

	if ((EffectiveAddress & 0xff00) != ((EffectiveAddress + YReg) & 0xff00))
	{
		Carried();
	}

	EffectiveAddress += YReg;
	EffectiveAddress &= 0xffff;

	return TUBEREADMEM_FAST(EffectiveAddress);
}

/*-------------------------------------------------------------------------*/

// Absolute with Y offset addressing mode handler

INLINE static int AbsYAddrModeHandler_Address()
{
	int EffectiveAddress;
	GETTWOBYTEFROMPC(EffectiveAddress);

	if ((EffectiveAddress & 0xff00) != ((EffectiveAddress + YReg) & 0xff00))
	{
		Carried();
	}

	EffectiveAddress += YReg;
	EffectiveAddress &= 0xffff;

	return EffectiveAddress;
}

/*-------------------------------------------------------------------------*/

// Indirect addressing mode handler (for JMP indirect only)

INLINE static int IndAddrModeHandler_Address()
{
	int VectorLocation;
	GETTWOBYTEFROMPC(VectorLocation);

	// The 65C02 fixed the bug in the 6502 concerning this addressing mode
	// and VectorLocation == xxFF
	int EffectiveAddress = TUBEREADMEM_FAST(VectorLocation);
	EffectiveAddress |= TUBEREADMEM_FAST(VectorLocation + 1) << 8;

	return EffectiveAddress;
}

/*-------------------------------------------------------------------------*/

// Zero page indirect addressing mode handler

INLINE static int ZPIndAddrModeHandler_Address()
{
	int VectorLocation = TubeRam[TubeProgramCounter++];
	int EffectiveAddress = TubeRam[VectorLocation] + (TubeRam[VectorLocation+1] << 8);
	return EffectiveAddress;
}

/*-------------------------------------------------------------------------*/

// Zero page indirect addressing mode handler

INLINE static unsigned char ZPIndAddrModeHandler_Data()
{
	unsigned char VectorLocation = TubeRam[TubeProgramCounter++];
	int EffectiveAddress = TubeRam[VectorLocation] + (TubeRam[VectorLocation + 1] << 8);
	return TubeRam[EffectiveAddress];
}

/*-------------------------------------------------------------------------*/

// Pre-indexed absolute Indirect addressing mode handler
// (for jump indirect only)

INLINE static int IndAddrXModeHandler_Address()
{
	int VectorLocation;
	GETTWOBYTEFROMPC(VectorLocation);

	int EffectiveAddress = TUBEREADMEM_FAST(VectorLocation + XReg);
	EffectiveAddress |= TUBEREADMEM_FAST(VectorLocation + 1 + XReg) << 8;

	return EffectiveAddress;
}

/*-------------------------------------------------------------------------*/

// Zero page with Y offset addressing mode handler

INLINE static unsigned char ZeroPgYAddrModeHandler_Data()
{
	unsigned char EffectiveAddress = TubeRam[TubeProgramCounter++] + YReg;
	return TubeRam[EffectiveAddress];
}

/*-------------------------------------------------------------------------*/

// Zero page with Y offset addressing mode handler

INLINE static int ZeroPgYAddrModeHandler_Address()
{
	return (TubeRam[TubeProgramCounter++] + YReg) & 0xff;
}

/*-------------------------------------------------------------------------*/
/* Reset processor */
static void Reset65C02()
{
  Accumulator=XReg=YReg=0; /* For consistancy of execution */
  StackReg=0xff; /* Initial value ? */
  PSR=FlagI; /* Interrupts off for starters */

  TubeintStatus=0;
  TubeNMIStatus=0;
  TubeNMILock = false;

  //The fun part, the tube OS is copied from ROM to tube RAM before the processor starts processing
  //This makes the OS "ROM" writable in effect, but must be restored on each reset.
  char TubeRomName[MAX_PATH];
  strcpy(TubeRomName,RomPath);
  strcat(TubeRomName,"BeebFile/6502Tube.rom");
  FILE *TubeRom = fopen(TubeRomName,"rb");
  if (TubeRom != nullptr)
  {
    fread(TubeRam+0xf800,1,2048,TubeRom);
    fclose(TubeRom);
  }
  else
  {
    mainWin->Report(MessageType::Error,
                    "Cannot open ROM:\n %s", TubeRomName);
  }

  TubeProgramCounter = TubeReadMem(0xfffc);
  TubeProgramCounter |= TubeReadMem(0xfffd) << 8;
  TotalTubeCycles=TotalCycles/2*3;
}

/* Reset Tube */
void ResetTube(void)
{
  memset(R1PHData,0,TubeBufferLength * 2);
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

/*-------------------------------------------------------------------------*/

static void DoTubeInterrupt()
{
  PushWord(TubeProgramCounter);
  Push(PSR & ~FlagB);
  TubeProgramCounter=TubeReadMem(0xfffe) | (TubeReadMem(0xffff)<<8);
  SetPSR(FlagI,0,0,1,0,0,0,0);
  IRQCycles=7;
}

/*-------------------------------------------------------------------------*/

static void DoTubeNMI()
{
  TubeNMILock = true;
  PushWord(TubeProgramCounter);
  Push(PSR);
  TubeProgramCounter=TubeReadMem(0xfffa) | (TubeReadMem(0xfffb)<<8);
  SetPSR(FlagI,0,0,1,0,0,0,0); /* Normal interrupts should be disabled during NMI ? */
  IRQCycles=7;
}

/*-------------------------------------------------------------------------*/

// Execute one 6502 instruction, move program counter on

void Exec65C02Instruction() {
	static int tmpaddr;

	// Output debug info
	if (DebugEnabled) {
		DebugDisassembler(TubeProgramCounter, TubePrePC, Accumulator, XReg, YReg, PSR, StackReg, false);
	}

	// For the Master, check Shadow Ram Presence
	// Note, this has to be done BEFORE reading an instruction due to Bit E and the PC
	TubePrePC = TubeProgramCounter;

	// Read an instruction and post inc program counter
	CurrentInstruction = TubeRam[TubeProgramCounter++];
	// cout << "Fetch at " << hex << (TubeProgramCounter-1) << " giving 0x" << CurrentInstruction << dec << "\n";
	TubeCycles = TubeCyclesTable[CurrentInstruction];
	// Stats[CurrentInstruction]++;

	Branched = false;

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
		case 0x82:
		case 0xc2:
		case 0xe2:
			// NOP imm
			TubeProgramCounter++;
			break;
		case 0x03:
		case 0x13:
		case 0x23:
		case 0x33:
		case 0x43:
		case 0x53:
		case 0x63:
		case 0x73:
		case 0x83:
		case 0x93:
		case 0xa3:
		case 0xb3:
		case 0xc3:
		case 0xd3:
		case 0xe3:
		case 0xf3:
			// NOP
			break;
		case 0x04:
			// TSB zp
			TSBInstrHandler(ZeroPgAddrModeHandler_Address());
			break;
		case 0x05:
			// ORA zp
			ORAInstrHandler(TubeRam[TubeRam[TubeProgramCounter++]]);
			break;
		case 0x06:
			// ASL zp
			ASLInstrHandler(ZeroPgAddrModeHandler_Address());
			break;
		case 0x07:
			// RMB0
			ResetMemoryBit(0);
			break;
		case 0x08:
			// PHP
			Push(PSR | 48);
			break;
		case 0x09:
			// ORA imm
			ORAInstrHandler(TubeRam[TubeProgramCounter++]);
			break;
		case 0x0a:
			// ASL A
			ASLInstrHandler_Acc();
			break;
		case 0x0b:
		case 0x1b:
		case 0x2b:
		case 0x3b:
		case 0x4b:
		case 0x5b:
		case 0x6b:
		case 0x7b:
		case 0x8b:
		case 0x9b:
		case 0xab:
		case 0xbb:
		case 0xcb:
		case 0xdb:
		case 0xeb:
		case 0xfb:
			// NOP
			break;
		case 0x0c:
			// TSB abs
			TSBInstrHandler(AbsAddrModeHandler_Address());
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
			// BBR0
			BranchOnBitReset(0);
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
			// ORA (zp)
			ORAInstrHandler(ZPIndAddrModeHandler_Data());
			break;
		case 0x14:
			// TRB zp
			TRBInstrHandler(ZeroPgAddrModeHandler_Address());
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
			// RMB1
			ResetMemoryBit(1);
			break;
		case 0x18:
			// CLC
			PSR &= ~FlagC;
			break;
		case 0x19:
			// ORA abs,Y
			ORAInstrHandler(AbsYAddrModeHandler_Data());
			break;
		case 0x1a:
			// INC A
			INAInstrHandler();
			break;
		case 0x1c:
			// TRB abs
			TRBInstrHandler(AbsAddrModeHandler_Address());
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
			// BBR1
			BranchOnBitReset(1);
			break;
		case 0x20:
			// JSR abs
			JSRInstrHandler(AbsAddrModeHandler_Address());
			break;
		case 0x21:
			// AND (zp,X)
			ANDInstrHandler(IndXAddrModeHandler_Data());
			break;
		case 0x24:
			// BIT zp
			BITInstrHandler(TubeRam[TubeRam[TubeProgramCounter++]]);
			break;
		case 0x25:
			// AND zp
			ANDInstrHandler(TubeRam[TubeRam[TubeProgramCounter++]]);
			break;
		case 0x26:
			// ROL zp
			ROLInstrHandler(ZeroPgAddrModeHandler_Address());
			break;
		case 0x27:
			// RMB2
			ResetMemoryBit(2);
			break;
		case 0x28:
			// PLP
			PSR = Pop();
			break;
		case 0x29:
			// AND imm
			ANDInstrHandler(TubeRam[TubeProgramCounter++]);
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
			// BBR2
			BranchOnBitReset(2);
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
			// AND (zp)
			ANDInstrHandler(ZPIndAddrModeHandler_Data());
			break;
		case 0x34:
			// BIT abs,x
			BITInstrHandler(ZeroPgXAddrModeHandler_Data());
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
			// RMB3
			ResetMemoryBit(3);
			break;
		case 0x38:
			// SEC
			PSR |= FlagC;
			break;
		case 0x39:
			ANDInstrHandler(AbsYAddrModeHandler_Data());
			break;
		case 0x3a:
			// DEC A
			DEAInstrHandler();
			break;
		case 0x3c:
			// BIT abs,x
			BITInstrHandler(AbsXAddrModeHandler_Data());
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
			// BBR3
			BranchOnBitReset(3);
			break;
		case 0x40:
			// RTI
			PSR = Pop();
			TubeProgramCounter = PopWord();
			TubeNMILock = false;
			break;
		case 0x41:
			// EOR (zp,X)
			EORInstrHandler(IndXAddrModeHandler_Data());
			break;
		case 0x44:
			// NOP zp
			TubeProgramCounter += 1;
			break;
		case 0x45:
			// EOR zp
			EORInstrHandler(TubeRam[TubeRam[TubeProgramCounter++]]);
			break;
		case 0x46:
			// LSR zp
			LSRInstrHandler(ZeroPgAddrModeHandler_Address());
			break;
		case 0x47:
			// RMB4
			ResetMemoryBit(4);
			break;
		case 0x48:
			// PHA
			Push(Accumulator);
			break;
		case 0x49:
			// EOR imm
			EORInstrHandler(TubeRam[TubeProgramCounter++]);
			break;
		case 0x4a:
			// LSR A
			LSRInstrHandler_Acc();
			break;
		case 0x4c:
			// JMP
			TubeProgramCounter = AbsAddrModeHandler_Address();
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
			// BBR4
			BranchOnBitReset(4);
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
			// EOR (zp)
			EORInstrHandler(ZPIndAddrModeHandler_Data());
			break;
		case 0x54:
			// NOP zp,X
			TubeProgramCounter += 1;
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
			// RMB5
			ResetMemoryBit(5);
			break;
		case 0x58:
			// CLI
			PSR &= ~FlagI;
			break;
		case 0x59:
			// EOR abs,Y
			EORInstrHandler(AbsYAddrModeHandler_Data());
			break;
		case 0x5a:
			// PHY
			Push(YReg);
			break;
		case 0x5c:
			// NOP abs
			TubeProgramCounter += 2;
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
			// BBR5
			BranchOnBitReset(5);
			break;
		case 0x60:
			// RTS
			TubeProgramCounter = PopWord() + 1;
			break;
		case 0x61:
			// ADC (zp,X)
			ADCInstrHandler(IndXAddrModeHandler_Data());
			break;
		case 0x64:
			// STZ zp
			TUBEWRITEMEM_DIRECT(ZeroPgAddrModeHandler_Address(), 0);
			break;
		case 0x65:
			// ADC zp
			ADCInstrHandler(TubeRam[TubeRam[TubeProgramCounter++]]);
			break;
		case 0x66:
			// ROR zp
			RORInstrHandler(ZeroPgAddrModeHandler_Address());
			break;
		case 0x67:
			// RMB6
			ResetMemoryBit(6);
			break;
		case 0x68:
			// PLA
			Accumulator = Pop();
			SetPSRZN(Accumulator);
			break;
		case 0x69:
			// ADC imm
			ADCInstrHandler(TubeRam[TubeProgramCounter++]);
			break;
		case 0x6a:
			// ROR A
			RORInstrHandler_Acc();
			break;
		case 0x6c:
			// JMP (abs)
			TubeProgramCounter = IndAddrModeHandler_Address();
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
			// BBR6
			BranchOnBitReset(6);
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
			// ADC (zp)
			ADCInstrHandler(ZPIndAddrModeHandler_Data());
			break;
		case 0x74:
			// STZ zp,x
			TUBEFASTWRITE(ZeroPgXAddrModeHandler_Address(), 0);
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
			// RMB7
			ResetMemoryBit(7);
			break;
		case 0x78:
			// SEI
			PSR |= FlagI;
			break;
		case 0x79:
			// ADC abs,Y
			ADCInstrHandler(AbsYAddrModeHandler_Data());
			break;
		case 0x7a:
			// PLY
			YReg = Pop();
			SetPSRZN(YReg);
			break;
		case 0x7c:
			// JMP abs,X
			TubeProgramCounter = IndAddrXModeHandler_Address();
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
			// BBR7
			BranchOnBitReset(7);
			break;
		case 0x80:
			// BRA rel
			BRAInstrHandler();
			break;
		case 0x81:
			// STA (zp,X)
			TUBEFASTWRITE(IndXAddrModeHandler_Address(), Accumulator);
			break;
		case 0x84:
			// STY zp
			TUBEWRITEMEM_DIRECT(ZeroPgAddrModeHandler_Address(), YReg);
			break;
		case 0x85:
			// STA zp
			TUBEWRITEMEM_DIRECT(ZeroPgAddrModeHandler_Address(), Accumulator);
			break;
		case 0x86:
			// STX zp
			TUBEWRITEMEM_DIRECT(ZeroPgAddrModeHandler_Address(), XReg);
			break;
		case 0x87:
			// SMB0
			SetMemoryBit(0);
			break;
		case 0x88:
			// DEY
			DEYInstrHandler();
			break;
		case 0x89:
			// BIT imm
			BITImmedInstrHandler(TubeRam[TubeProgramCounter++]);
			break;
		case 0x8a:
			// TXA
			Accumulator = XReg;
			SetPSRZN(Accumulator);
			break;
		case 0x8c:
			// STY abs
			STYInstrHandler(AbsAddrModeHandler_Address());
			break;
		case 0x8d:
			// STA abs
			TUBEFASTWRITE(AbsAddrModeHandler_Address(), Accumulator);
			break;
		case 0x8e:
			// STX abs
			STXInstrHandler(AbsAddrModeHandler_Address());
			break;
		case 0x8f:
			// BBS0
			BranchOnBitSet(0);
			break;
		case 0x90:
			// BCC rel
			BCCInstrHandler();
			break;
		case 0x91:
			// STA (zp),Y
			TUBEFASTWRITE(IndYAddrModeHandler_Address(), Accumulator);
			break;
		case 0x92:
			// STA (zp)
			TUBEFASTWRITE(ZPIndAddrModeHandler_Address(), Accumulator);
			break;
		case 0x94:
			// STY zp,X
			STYInstrHandler(ZeroPgXAddrModeHandler_Address());
			break;
		case 0x95:
			// STA zp,X
			TUBEFASTWRITE(ZeroPgXAddrModeHandler_Address(), Accumulator);
			break;
		case 0x96:
			// STX zp,X
			STXInstrHandler(ZeroPgYAddrModeHandler_Address());
			break;
		case 0x97:
			// SMB1
			SetMemoryBit(1);
			break;
		case 0x98:
			// TYA
			Accumulator = YReg;
			SetPSRZN(Accumulator);
			break;
		case 0x99:
			// STA abs,Y
			TUBEFASTWRITE(AbsYAddrModeHandler_Address(), Accumulator);
			break;
		case 0x9a:
			// TXS
			StackReg = XReg;
			break;
		case 0x9c:
			// STZ abs
			TUBEFASTWRITE(AbsAddrModeHandler_Address(), 0);
			break;
		case 0x9d:
			// STA abs,X
			TUBEFASTWRITE(AbsXAddrModeHandler_Address(), Accumulator);
			break;
		case 0x9e:
			// STZ abs,x
			TUBEFASTWRITE(AbsXAddrModeHandler_Address(), 0);
			break;
		case 0x9f:
			// BBS1
			BranchOnBitSet(1);
			break;
		case 0xa0:
			// LDY imm
			LDYInstrHandler(TubeRam[TubeProgramCounter++]);
			break;
		case 0xa1:
			// LDA (zp,X)
			LDAInstrHandler(IndXAddrModeHandler_Data());
			break;
		case 0xa2:
			// LDX imm
			LDXInstrHandler(TubeRam[TubeProgramCounter++]);
			break;
		case 0xa4:
			// LDY zp
			LDYInstrHandler(TubeRam[TubeRam[TubeProgramCounter++]]);
			break;
		case 0xa5:
			// LDA zp
			LDAInstrHandler(TubeRam[TubeRam[TubeProgramCounter++]]);
			break;
		case 0xa6:
			// LDX zp
			LDXInstrHandler(TubeRam[TubeRam[TubeProgramCounter++]]);
			break;
		case 0xa7:
			// SMB2
			SetMemoryBit(2);
			break;
		case 0xa8:
			// TAY
			YReg = Accumulator;
			SetPSRZN(Accumulator);
			break;
		case 0xa9:
			// LDA imm
			LDAInstrHandler(TubeRam[TubeProgramCounter++]);
			break;
		case 0xaa:
			// TXA
			XReg = Accumulator;
			SetPSRZN(Accumulator);
			break;
		case 0xaf:
			// BBS2
			BranchOnBitSet(2);
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
		case 0xb0:
			// BCS rel
			BCSInstrHandler();
			break;
		case 0xb1:
			// LDA (zp),Y
			LDAInstrHandler(IndYAddrModeHandler_Data());
			break;
		case 0xb2:
			// LDA (zp)
			LDAInstrHandler(ZPIndAddrModeHandler_Data());
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
			// LDX zp,X
			LDXInstrHandler(ZeroPgYAddrModeHandler_Data());
			break;
		case 0xb7:
			// SMB3
			SetMemoryBit(3);
			break;
		case 0xb8:
			// CLV
			PSR &= ~FlagV;
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
		case 0xbc:
			// LDY abs,X
			LDYInstrHandler(AbsXAddrModeHandler_Data());
			break;
		case 0xbd:
			// LDA abs,X
			LDAInstrHandler(AbsXAddrModeHandler_Data());
			break;
		case 0xbe:
			// LDX abs,X
			LDXInstrHandler(AbsYAddrModeHandler_Data());
			break;
		case 0xbf:
			// BBS3
			BranchOnBitSet(3);
			break;
		case 0xc0:
			// CPY imm
			CPYInstrHandler(TubeRam[TubeProgramCounter++]);
			break;
		case 0xc1:
			// CMP (zp,X)
			CMPInstrHandler(IndXAddrModeHandler_Data());
			break;
		case 0xc4:
			// CPY zp
			CPYInstrHandler(TubeRam[TubeRam[TubeProgramCounter++]]);
			break;
		case 0xc5:
			// CMP zp
			CMPInstrHandler(TubeRam[TubeRam[TubeProgramCounter++]]);
			break;
		case 0xc6:
			// DEC zp
			DECInstrHandler(ZeroPgAddrModeHandler_Address());
			break;
		case 0xc7:
			// SMB4
			SetMemoryBit(4);
			break;
		case 0xc8:
			// INY
			INYInstrHandler();
			break;
		case 0xc9:
			// CMP imm
			CMPInstrHandler(TubeRam[TubeProgramCounter++]);
			break;
		case 0xca:
			// DEX
			DEXInstrHandler();
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
			// BBS4
			BranchOnBitSet(4);
			break;
		case 0xd0:
			// BNE rel
			BNEInstrHandler();
			break;
		case 0xd1:
			CMPInstrHandler(IndYAddrModeHandler_Data());
			break;
		case 0xd2:
			// CMP (zp)
			CMPInstrHandler(ZPIndAddrModeHandler_Data());
			break;
		case 0xd4:
			// NOP zp,X
			TubeProgramCounter += 1;
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
			// SMB5
			SetMemoryBit(5);
			break;
		case 0xd8:
			// CLD
			PSR &= ~FlagD;
			break;
		case 0xd9:
			// CMP abs,Y
			CMPInstrHandler(AbsYAddrModeHandler_Data());
			break;
		case 0xda:
			// PHX
			Push(XReg);
			break;
		case 0xdc:
			// NOP abs
			TubeProgramCounter += 2;
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
			// BBS5
			BranchOnBitSet(5);
			break;
		case 0xe0:
			// CPX imm
			CPXInstrHandler(TubeRam[TubeProgramCounter++]);
			break;
		case 0xe1:
			// SBC (zp,X)
			SBCInstrHandler(IndXAddrModeHandler_Data());
			break;
		case 0xe4:
			// CPX zp
			CPXInstrHandler(TubeRam[TubeRam[TubeProgramCounter++]]);
			break;
		case 0xe5:
			// SBC zp
			SBCInstrHandler(TubeRam[TubeRam[TubeProgramCounter++]]);
			break;
		case 0xe6:
			// INC zp
			INCInstrHandler(ZeroPgAddrModeHandler_Address());
			break;
		case 0xe7:
			// SMB6
			SetMemoryBit(6);
			break;
		case 0xe8:
			// INX
			INXInstrHandler();
			break;
		case 0xe9:
			// SBC imm
			SBCInstrHandler(TubeRam[TubeProgramCounter++]);
			break;
		case 0xea:
			// NOP
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
			// BBS6
			BranchOnBitSet(6);
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
			// SBC (zp)
			SBCInstrHandler(ZPIndAddrModeHandler_Data());
			break;
		case 0xf4:
			// NOP zp,X
			TubeProgramCounter += 1;
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
			// SMB7
			SetMemoryBit(7);
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
			// PLX
			XReg = Pop();
			SetPSRZN(XReg);
			break;
		case 0xfc:
			// NOP abs
			TubeProgramCounter += 2;
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
			// BBS7
			BranchOnBitSet(7);
			break;
	}

	// This block corrects the cycle count for the branch instructions
	if ((CurrentInstruction == 0x10) ||
	    (CurrentInstruction == 0x30) ||
	    (CurrentInstruction == 0x50) ||
	    (CurrentInstruction == 0x70) ||
	    (CurrentInstruction == 0x80) ||
	    (CurrentInstruction == 0x90) ||
	    (CurrentInstruction == 0xb0) ||
	    (CurrentInstruction == 0xd0) ||
	    (CurrentInstruction == 0xf0))
	{
		if (Branched)
		{
			TubeCycles++;
			if ((TubeProgramCounter & 0xff00) != ((TubePrePC + 2) & 0xff00)) {
				TubeCycles++;
			}
		}
	}

	TubeCycles += IRQCycles;
	IRQCycles = 0; // IRQ Timing
	// End of cycle correction

	if (TubeintStatus && !GETIFLAG) {
		DoTubeInterrupt();
	}

	TotalTubeCycles += TubeCycles;

	if (TubeNMIStatus && !OldTubeNMIStatus) {
		DoTubeNMI();
	}

	OldTubeNMIStatus = TubeNMIStatus;
}

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

void SaveTubeUEF(FILE *SUEF)
{
	UEFWrite8(R1Status, SUEF);
	UEFWriteBuf(R1PHData, TubeBufferLength, SUEF);
	UEFWrite8(R1PHPtr, SUEF);
	UEFWrite8(R1HStatus, SUEF);
	UEFWrite8(R1HPData, SUEF);
	UEFWrite8(R1PStatus, SUEF);
	UEFWrite8(R2PHData, SUEF);
	UEFWrite8(R2HStatus, SUEF);
	UEFWrite8(R2HPData, SUEF);
	UEFWrite8(R2PStatus, SUEF);
	UEFWrite8(R3PHData[ 0],SUEF);
	UEFWrite8(R3PHData[ 1],SUEF);
	UEFWrite8(R3PHPtr, SUEF);
	UEFWrite8(R3HStatus, SUEF);
	UEFWrite8(R3HPData[ 0],SUEF);
	UEFWrite8(R3HPData[ 1],SUEF);
	UEFWrite8(R3HPPtr, SUEF);
	UEFWrite8(R3PStatus, SUEF);
	UEFWrite8(R4PHData, SUEF);
	UEFWrite8(R4HStatus, SUEF);
	UEFWrite8(R4HPData, SUEF);
	UEFWrite8(R4PStatus, SUEF);
}

void Save65C02UEF(FILE *SUEF)
{
	UEFWrite16(TubeProgramCounter, SUEF);
	UEFWrite8(Accumulator, SUEF);
	UEFWrite8(XReg, SUEF);
	UEFWrite8(YReg, SUEF);
	UEFWrite8(StackReg, SUEF);
	UEFWrite8(PSR, SUEF);
	UEFWrite32(TotalTubeCycles, SUEF);
	UEFWrite8(TubeintStatus, SUEF);
	UEFWrite8(TubeNMIStatus, SUEF);
	UEFWrite8(TubeNMILock, SUEF);
	UEFWrite16(0, SUEF);
}

void Save65C02MemUEF(FILE *SUEF)
{
	UEFWriteBuf(TubeRam, 65536, SUEF);
}

void LoadTubeUEF(FILE *SUEF)
{
	R1Status = UEFRead8(SUEF);
	UEFReadBuf(R1PHData, TubeBufferLength, SUEF);
	R1PHPtr = UEFRead8(SUEF);
	R1HStatus = UEFRead8(SUEF);
	R1HPData = UEFRead8(SUEF);
	R1PStatus = UEFRead8(SUEF);
	R2PHData = UEFRead8(SUEF);
	R2HStatus = UEFRead8(SUEF);
	R2HPData = UEFRead8(SUEF);
	R2PStatus = UEFRead8(SUEF);
	R3PHData[0] = UEFRead8(SUEF);
	R3PHData[1] = UEFRead8(SUEF);
	R3PHPtr = UEFRead8(SUEF);
	R3HStatus = UEFRead8(SUEF);
	R3HPData[0] = UEFRead8(SUEF);
	R3HPData[1] = UEFRead8(SUEF);
	R3HPPtr = UEFRead8(SUEF);
	R3PStatus = UEFRead8(SUEF);
	R4PHData = UEFRead8(SUEF);
	R4HStatus = UEFRead8(SUEF);
	R4HPData = UEFRead8(SUEF);
	R4PStatus = UEFRead8(SUEF);
}

void Load65C02UEF(FILE *SUEF)
{
	TubeProgramCounter = UEFRead16(SUEF);
	Accumulator = UEFRead8(SUEF);
	XReg = UEFRead8(SUEF);
	YReg = UEFRead8(SUEF);
	StackReg = UEFRead8(SUEF);
	PSR = UEFRead8(SUEF);
	// TotalTubeCycles = UEFRead32(SUEF);
	UEFRead32(SUEF); // Unused, was: Dlong
	TubeintStatus = UEFRead8(SUEF);
	TubeNMIStatus = UEFRead8(SUEF);
	TubeNMILock = UEFReadBool(SUEF);
}

void Load65C02MemUEF(FILE *SUEF)
{
	UEFReadBuf(TubeRam, 65536, SUEF);
}
