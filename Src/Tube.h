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

/* 6502Core - header - David Alan Gilbert 16/10/94 */
/* Copied for 65C02 Tube by Richard Gellman 13/04/01 */

#ifndef TUBE6502_HEADER
#define TUBE6502_HEADER

enum class TubeDevice
{
	None,
	Acorn65C02,
	Master512CoPro,
	AcornZ80,
	TorchZ80,
	AcornArm,
	SprowArm
};

extern TubeDevice TubeType;

extern unsigned char R1Status;
void ResetTube(void);

extern int TubeProgramCounter;
const int TubeBufferLength = 24;
extern unsigned char TubeintStatus; /* bit set (nums in IRQ_Nums) if interrupt being caused */
extern unsigned char TubeNMIStatus; /* bit set (nums in NMI_Nums) if NMI being caused */

enum TubeIRQ {
	R1,
	R4
};

enum TubeNMI {
	R3
};

/*-------------------------------------------------------------------------*/

// Initialise 6502core

void Init65C02Core();

// Execute one 6502 instruction, move program counter on.
void Exec65C02Instruction();

void WrapTubeCycles(void);
void SyncTubeProcessor(void);
unsigned char ReadTubeFromHostSide(int IOAddr);
unsigned char ReadTubeFromParasiteSide(int IOAddr);
void WriteTubeFromHostSide(int IOAddr, unsigned char IOData);
void WriteTubeFromParasiteSide(int IOAddr, unsigned char IOData);

unsigned char ReadTorchTubeFromHostSide(int IOAddr);
unsigned char ReadTorchTubeFromParasiteSide(int IOAddr);
void WriteTorchTubeFromHostSide(int IOAddr, unsigned char IOData);
void WriteTorchTubeFromParasiteSide(int IOAddr, unsigned char IOData);

unsigned char TubeReadMem(int IOAddr);
void TubeWriteMem(int IOAddr,unsigned char IOData);
void DebugTubeState(void);
void SaveTubeUEF(FILE *SUEF);
void Save65C02UEF(FILE *SUEF);
void Save65C02MemUEF(FILE *SUEF);
void LoadTubeUEF(FILE *SUEF);
void Load65C02UEF(FILE *SUEF);
void Load65C02MemUEF(FILE *SUEF);

#endif
