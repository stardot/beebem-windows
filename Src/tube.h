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

#include "port.h"

extern unsigned char R1Status;
void ResetTube(void);

extern unsigned char EnableTube,TubeEnabled,AcornZ80;
#ifdef M512COPRO_ENABLED
extern unsigned char Tube186Enabled;
#endif
extern int TorchTubeActive;
extern int TubeProgramCounter;

extern unsigned char TubeintStatus; /* bit set (nums in IRQ_Nums) if interrupt being caused */
extern unsigned char TubeNMIStatus; /* bit set (nums in NMI_Nums) if NMI being caused */

// EnableTube - Should the tube be enabled on next start - 1=yes
// TubeEnabled - Is the tube enabled by default - 1=yes

typedef enum TubeIRQ {
	R1,
	R4,
} TubeIRQ;

typedef enum TubeNMI {
	R3,
} TubeNMI;


/*-------------------------------------------------------------------------*/
/* Initialise 6502core                                                     */
void Init65C02core(void);

/*-------------------------------------------------------------------------*/
/* Execute one 6502 instruction, move program counter on                   */
void Exec65C02Instruction(void);

void DoTubeNMI(void);
void DoTubeInterrupt(void);
void WrapTubeCycles(void);
void SyncTubeProcessor(void);
unsigned char ReadTubeFromHostSide(unsigned char IOAddr);
unsigned char ReadTubeFromParasiteSide(unsigned char IOAddr);
void WriteTubeFromHostSide(unsigned char IOAddr,unsigned char IOData);
void WriteTubeFromParasiteSide(unsigned char IOAddr,unsigned char IOData);

unsigned char ReadTorchTubeFromHostSide(unsigned char IOAddr);
unsigned char ReadTorchTubeFromParasiteSide(unsigned char IOAddr);
void WriteTorchTubeFromHostSide(unsigned char IOAddr,unsigned char IOData);
void WriteTorchTubeFromParasiteSide(unsigned char IOAddr,unsigned char IOData);

unsigned char TubeReadMem(unsigned int IOAddr);
void TubeWriteMem(unsigned int IOAddr,unsigned char IOData);
void DebugTubeState(void);
void SaveTubeUEF(FILE *SUEF);
void Save65C02UEF(FILE *SUEF);
void Save65C02MemUEF(FILE *SUEF);
void LoadTubeUEF(FILE *SUEF);
void Load65C02UEF(FILE *SUEF);
void Load65C02MemUEF(FILE *SUEF);
#endif
