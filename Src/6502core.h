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

#ifndef CORE6502_HEADER
#define CORE6502_HEADER

#include <stdio.h>

#include "Port.h"

enum IRQ_Nums {
  sysVia,
  userVia,
  serial,
  tube,
  teletext,
  hdc,
};

enum NMI_Nums{
	nmi_floppy,
	nmi_econet,
};

enum PSR_Flags
{
  FlagC=1,
  FlagZ=2,
  FlagI=4,
  FlagD=8,
  FlagB=16,
  FlagV=64,
  FlagN=128
};

extern unsigned char intStatus;
extern unsigned char NMIStatus;

constexpr int CPU_CYCLES_PER_SECOND = 2000000;

extern int ProgramCounter;
extern int PrePC;
extern CycleCountT TotalCycles;
extern bool NMILock;
extern int DisplayCycles;

extern int CyclesToInt;
#define NO_TIMER_INT_DUE -1000000

#define SetTrigger(after, var) var = TotalCycles + (after)
#define IncTrigger(after, var) var += (after)

#define ClearTrigger(var) var=CycleCountTMax;

#define AdjustTrigger(var) if (var!=CycleCountTMax) var-=CycleCountWrap;

/*-------------------------------------------------------------------------*/
/* Initialise 6502core                                                     */
void Init6502core(void);

/*-------------------------------------------------------------------------*/
/* Execute one 6502 instruction, move program counter on                   */
void Exec6502Instruction(void);

void DoNMI(void);
void DoInterrupt(void);
void Save6502UEF(FILE *SUEF);
void Load6502UEF(FILE *SUEF);
void SyncIO(void);
void AdjustForIORead(void);
void AdjustForIOWrite(void);

extern int OpCodes;
extern bool BasicHardwareOnly;

void WriteInstructionCounts(const char *FileName);

int MillisecondsToCycles(int Time);
int MicrosecondsToCycles(int Time);


/* Instruction Set Defines */

constexpr int InstBRK = 0x0;
constexpr int InstORA_zp_X_0x1 = 0x1;
constexpr int InstNOP_imm = 0x2;
constexpr int InstUndocumented_instruction__KIL_0x22 = 0x22;
constexpr int InstNOP_0x42 = 0x42;
constexpr int InstUndocumented_instruction__SLO_zp_X = 0x62;
constexpr int InstTSB_zp = 0x4;
constexpr int InstUndocumented_instruction__NOP_zp = 0x4;
constexpr int InstORA_zp_0x5 = 0x5;
constexpr int InstASL_zp = 0x6;
constexpr int InstNOP_0x7 = 0x7;
constexpr int InstUndocumented_instruction__SLO_zp = 0x7;
constexpr int InstPHP = 0x8;
constexpr int InstORA_imm = 0x9;
constexpr int InstASL_A = 0xa;
constexpr int InstNOP_0xb = 0xb;
constexpr int InstUndocumented_instruction__ANC_imm = 0x2b;
constexpr int InstTSB_abs = 0xc;
constexpr int InstUndocumented_instruction__NOP_abs = 0xc;
constexpr int InstORA_abs = 0xd;
constexpr int InstASL_abs = 0xe;
constexpr int InstNOP_0xf = 0xf;
constexpr int InstUndocumented_instruction__SLO_abs = 0xf;
constexpr int InstBPL_rel = 0x10;
constexpr int InstORA_zp_Y = 0x11;
constexpr int InstORA_zp = 0x12;
constexpr int InstUndocumented_instruction__KIL_0x12 = 0x12;
constexpr int InstNOP_0x13 = 0x13;
constexpr int InstUndocumented_instruction__SLO_zp_Y = 0x13;
constexpr int InstTRB_zp = 0x14;
constexpr int InstUndocumented_instruction__NOP_zp_X = 0x14;
constexpr int InstORA_zp_X = 0x15;
constexpr int InstASL_zp_X = 0x16;
constexpr int InstNOP_0x17 = 0x17;
constexpr int InstUndocumented_instruction__SLO_zp_X_0x17 = 0x17;
constexpr int InstCLC = 0x18;
constexpr int InstORA_abs_Y = 0x19;
constexpr int InstINC_A = 0x1a;
constexpr int InstUndocumented_instruction__NOP_0x1a = 0x1a;
constexpr int InstNOP_0x1b = 0x1b;
constexpr int InstUndocumented_instruction__SLO_abs_Y = 0x1b;
constexpr int InstTRB_abs = 0x1c;
constexpr int InstUndocumented_instruction__NOP_abs_X = 0x1c;
constexpr int InstORA_abs_X = 0x1d;
constexpr int InstASL_abs_X = 0x1e;
constexpr int InstNOP_0x1f = 0x1f;
constexpr int InstUndocumented_instruction__SLO_abs_X = 0x1f;
constexpr int InstJSR_abs = 0x20;
constexpr int InstAND_zp_X_0x21 = 0x21;
constexpr int InstNOP_0x23 = 0x23;
constexpr int InstUndocumented_instruction__RLA_zp_X = 0x23;
constexpr int InstBIT_zp = 0x24;
constexpr int InstAND_zp_0x25 = 0x25;
constexpr int InstROL_zp = 0x26;
constexpr int InstNOP_0x27 = 0x27;
constexpr int InstUndocumented_instruction__RLA_zp = 0x27;
constexpr int InstPLP = 0x28;
constexpr int InstAND_imm = 0x29;
constexpr int InstROL_A = 0x2a;
constexpr int InstBIT_abs = 0x2c;
constexpr int InstAND_abs = 0x2d;
constexpr int InstROL_abs = 0x2e;
constexpr int InstNOP_0x2f = 0x2f;
constexpr int InstUndocumented_instruction__RLA_abs = 0x2f;
constexpr int InstBMI_rel = 0x30;
constexpr int InstAND_zp_Y = 0x31;
constexpr int InstAND_zp = 0x32;
constexpr int InstUndocumented_instruction__KIL_0x32 = 0x32;
constexpr int InstNOP_0x33 = 0x33;
constexpr int InstUndocumented_instruction__RLA_zp_Y = 0x33;
constexpr int InstBIT_abs_X_0x34 = 0x34;
constexpr int InstUndocumented_instruction__NOP_zp_X_0x34 = 0x34;
constexpr int InstAND_zp_X = 0x35;
constexpr int InstROL_zp_X = 0x36;
constexpr int InstNOP_0x37 = 0x37;
constexpr int InstUndocumented_instruction__RLA_zp_X_0x37 = 0x37;
constexpr int InstSEC = 0x38;
constexpr int InstAND_abs_Y = 0x39;
constexpr int InstDEC_A = 0x3a;
constexpr int InstUndocumented_instruction__NOP_0x3a = 0x3a;
constexpr int InstNOP_0x3b = 0x3b;
constexpr int InstUndocumented_instruction__RLA_abs_Y = 0x3b;
constexpr int InstBIT_abs_X = 0x3c;
constexpr int InstUndocumented_instruction__NOP_abs_x = 0x3c;
constexpr int InstAND_abs_X = 0x3d;
constexpr int InstROL_abs_X = 0x3e;
constexpr int InstNOP_0x3f = 0x3f;
constexpr int InstUndocumented_instruction__RLA_abs_X = 0x3f;
constexpr int InstRTI = 0x40;
constexpr int InstEOR_zp_X_0x41 = 0x41;
constexpr int InstNOP_0x43 = 0x43;
constexpr int InstUndocumented_instruction__SRE_zp_X = 0x43;
constexpr int InstNOP_zp = 0x44;
constexpr int InstEOR_zp_0x45 = 0x45;
constexpr int InstLSR_zp = 0x46;
constexpr int InstNOP_0x47 = 0x47;
constexpr int InstUndocumented_instruction__SRE_zp = 0x47;
constexpr int InstPHA = 0x48;
constexpr int InstEOR_imm = 0x49;
constexpr int InstLSR_A = 0x4a;
constexpr int InstNOP_0x4b = 0x4b;
constexpr int InstUndocumented_instruction__ALR_imm = 0x4b;
constexpr int InstJMP_abs_0x4c = 0x4c;
constexpr int InstEOR_abs = 0x4d;
constexpr int InstLSR_abs = 0x4e;
constexpr int InstNOP_0x4f = 0x4f;
constexpr int InstUndocumented_instruction__SRE_abs = 0x4f;
constexpr int InstBVC_rel = 0x50;
constexpr int InstEOR_zp_Y = 0x51;
constexpr int InstEOR_zp = 0x52;
constexpr int InstUndocumented_instruction__KIL_0x52 = 0x52;
constexpr int InstNOP_0x53 = 0x53;
constexpr int InstUndocumented_instruction__SRE_zp_Y = 0x53;
constexpr int InstUndocumented_instruction__NOP_zp_X_1 = 0x54;
constexpr int InstUndocumented_instruction__NOP_zp_X_2 = 0xd4;
constexpr int InstUndocumented_instruction__NOP_zp_X_3 = 0xf4;
constexpr int InstEOR_zp_X = 0x55;
constexpr int InstLSR_zp_X = 0x56;
constexpr int InstNOP_0x57 = 0x57;
constexpr int InstUndocumented_instruction__SRE_zp_X_0x57 = 0x57;
constexpr int InstCLI = 0x58;
constexpr int InstEOR_abs_Y = 0x59;
constexpr int InstPHY = 0x5a;
constexpr int InstUndocumented_instruction__NOP_CommentOnly = 0x5a;
constexpr int InstNOP_0x5b = 0x5b;
constexpr int InstUndocumented_instruction__SRE_abs_Y = 0x5b;
constexpr int InstNOP_abs_0x5c = 0x5c;
constexpr int InstUndocumented_instruction__NOP_abs_x_0x5c = 0x5c;
constexpr int InstEOR_abs_X = 0x5d;
constexpr int InstLSR_abs_X = 0x5e;
constexpr int InstNOP_0x5f = 0x5f;
constexpr int InstUndocumented_instruction__SRE_abs_X = 0x5f;
constexpr int InstRTS = 0x60;
constexpr int InstADC_zp_X_0x61 = 0x61;
constexpr int InstNOP_0x63 = 0x63;
constexpr int InstUndocumented_instruction__RRA_zp_X = 0x63;
constexpr int InstSTZ_zp = 0x64;
constexpr int InstUndocumented_instruction__NOP_zp_0x64 = 0x64;
constexpr int InstADC_zp_0x65 = 0x65;
constexpr int InstROR_zp = 0x66;
constexpr int InstNOP_0x67 = 0x67;
constexpr int InstUndocumented_instruction__RRA_zp = 0x67;
constexpr int InstPLA = 0x68;
constexpr int InstADC_imm = 0x69;
constexpr int InstROR_A = 0x6a;
constexpr int InstNOP_0x6b = 0x6b;
constexpr int InstUndocumented_instruction__ARR_imm = 0x6b;
constexpr int InstJMP_abs = 0x6c;
constexpr int InstADC_abs = 0x6d;
constexpr int InstROR_abs = 0x6e;
constexpr int InstNOP_0x6f = 0x6f;
constexpr int InstUndocumented_instruction__RRA_abs = 0x6f;
constexpr int InstBVS_rel = 0x70;
constexpr int InstADC_zp_Y = 0x71;
constexpr int InstADC_zp = 0x72;
constexpr int InstUndocumented_instruction__KIL_0x72 = 0x72;
constexpr int InstNOP_0x73 = 0x73;
constexpr int InstUndocumented_instruction__RRA_zp_Y = 0x73;
constexpr int InstSTZ_zp_X = 0x74;
constexpr int InstUndocumented_instruction__NOP_zp_x = 0x74;
constexpr int InstADC_zp_X = 0x75;
constexpr int InstROR_zp_X = 0x76;
constexpr int InstNOP_0x77 = 0x77;
constexpr int InstUndocumented_instruction__RRA_zp_X_0x77 = 0x77;
constexpr int InstSEI = 0x78;
constexpr int InstADC_abs_Y = 0x79;
constexpr int InstPLY = 0x7a;
constexpr int InstUndocumented_instruction__NOP_0x7a = 0x7a;
constexpr int InstNOP_0x7b = 0x7b;
constexpr int InstUndocumented_instruction__RRA_abs_Y = 0x7b;
constexpr int InstJMP_abs_X = 0x7c;
constexpr int InstUndocumented_instruction__NOP_abs_X_0x7c = 0x7c;
constexpr int InstADC_abs_X = 0x7d;
constexpr int InstROR_abs_X = 0x7e;
constexpr int InstNOP_0x7f = 0x7f;
constexpr int InstUndocumented_instruction__RRA_abs_X = 0x7f;
constexpr int InstBRA_rel = 0x80;
constexpr int InstUndocumented_instruction__NOP_imm = 0x80;
constexpr int InstSTA_zp_X_0x81 = 0x81;
constexpr int InstUndocumented_instruction__NOP_imm_0x82 = 0x82;
constexpr int InstUndocumented_instruction__NOP_imm_0xc2 = 0xc2;
constexpr int InstUndocumented_instruction__NOP_imm_0xe2 = 0xe2;
constexpr int InstNOP_0x83 = 0x83;
constexpr int InstUndocumented_instruction__SAX_zp_X = 0x83;
constexpr int InstSTY_zp = 0x84;
constexpr int InstSTA_zp_0x85 = 0x85;
constexpr int InstSTX_zp = 0x86;
constexpr int InstNOP_0x87 = 0x87;
constexpr int InstUndocumented_instruction__SAX_zp_NO_FLAGS = 0x87;
constexpr int InstDEY = 0x88;
constexpr int InstBIT_imm = 0x89;
constexpr int InstUndocumented_instruction__NOP_imm_0x89 = 0x89;
constexpr int InstTXA_0x8a = 0x8a;
constexpr int InstNOP_0x8b = 0x8b;
constexpr int InstUndocumented_instruction__XAA_imm = 0x8b;
constexpr int InstSTY_abs = 0x8c;
constexpr int InstSTA_abs = 0x8d;
constexpr int InstSTX_abs = 0x8e;
constexpr int InstNOP_0x8f = 0x8f;
constexpr int InstUndocumented_instruction__SAX_abs = 0x8f;
constexpr int InstBCC_rel = 0x90;
constexpr int InstSTA_zp_Y = 0x91;
constexpr int InstSTA_zp = 0x92;
constexpr int InstUndocumented_instruction__KIL_0x92 = 0x92;
constexpr int InstNOP_0x93 = 0x93;
constexpr int InstUndocumented_instruction__AHX_zp_Y = 0x93;
constexpr int InstSTY_zp_X = 0x94;
constexpr int InstSTA_zp_X = 0x95;
constexpr int InstSTX_zp_X = 0x96;
constexpr int InstNOP_0x97 = 0x97;
constexpr int InstUndocumented_instruction__SAX_zp_Y = 0x97;
constexpr int InstTYA = 0x98;
constexpr int InstSTA_abs_Y = 0x99;
constexpr int InstTXS = 0x9a;
constexpr int InstNOP_0x9b = 0x9b;
constexpr int InstUndocumented_instruction__TAS_abs_Y = 0x9b;
constexpr int InstSTZ_abs = 0x9c;
constexpr int InstUndocumented_instruction__SHY_abs_X = 0x9c;
constexpr int InstSTA_abs_X = 0x9d;
constexpr int InstSTZ_abs_x = 0x9e;
constexpr int InstUndocumented_instruction__SHX_abs_Y = 0x9e;
constexpr int InstNOP_0x9f = 0x9f;
constexpr int InstUndocumented_instruction__AHX_abs_Y = 0x9f;
constexpr int InstLDY_imm = 0xa0;
constexpr int InstLDA_zp_X_0xa1 = 0xa1;
constexpr int InstLDX_imm = 0xa2;
constexpr int InstNOP_0xa3 = 0xa3;
constexpr int InstUndocumented_instruction__LAX_zp_X = 0xa3;
constexpr int InstLDY_zp = 0xa4;
constexpr int InstLDA_zp_0xa5 = 0xa5;
constexpr int InstLDX_zp = 0xa6;
constexpr int InstNOP_0xa7 = 0xa7;
constexpr int InstUndocumented_instruction__LAX_zp = 0xa7;
constexpr int InstTAY = 0xa8;
constexpr int InstLDA_imm = 0xa9;
constexpr int InstTXA = 0xaa;
constexpr int InstNOP_0xab = 0xab;
constexpr int InstUndocumented_instruction__LAX_imm = 0xab;
constexpr int InstLDY_abs = 0xac;
constexpr int InstLDA_abs = 0xad;
constexpr int InstLDX_abs = 0xae;
constexpr int InstNOP_0xaf = 0xaf;
constexpr int InstUndocumented_instruction__LAX_abs = 0xaf;
constexpr int InstBCS_rel = 0xb0;
constexpr int InstLDA_zp_Y = 0xb1;
constexpr int InstLDA_zp = 0xb2;
constexpr int InstUndocumented_instruction__KIL_0xb2 = 0xb2;
constexpr int InstNOP_0xb3 = 0xb3;
constexpr int InstUndocumented_instruction__LAX_zp_Y_0xb3 = 0xb3;
constexpr int InstLDY_zp_X = 0xb4;
constexpr int InstLDA_zp_X = 0xb5;
constexpr int InstLDX_zp_Y = 0xb6;
constexpr int InstNOP_0xb7 = 0xb7;
constexpr int InstUndocumented_instruction__LAX_zp_Y = 0xb7;
constexpr int InstCLV = 0xb8;
constexpr int InstLDA_abs_Y = 0xb9;
constexpr int InstTSX = 0xba;
constexpr int InstNOP_0xbb = 0xbb;
constexpr int InstUndocumented_instruction__LAS_abs_Y = 0xbb;
constexpr int InstLDY_abs_X = 0xbc;
constexpr int InstLDA_abs_X = 0xbd;
constexpr int InstLDX_abs_Y = 0xbe;
constexpr int InstNOP_0xbf = 0xbf;
constexpr int InstUndocumented_instruction__LAX_abs_Y = 0xbf;
constexpr int InstCPY_imm = 0xc0;
constexpr int InstCMP_zp_X_0x1c = 0xc1;
constexpr int InstNOP_0xc3 = 0xc3;
constexpr int InstUndocument_instruction__DCP_zp_X = 0xc3;
constexpr int InstCPY_zp = 0xc4;
constexpr int InstCMP_zp_0xc5 = 0xc5;
constexpr int InstDEC_zp = 0xc6;
constexpr int InstNOP_0xc7 = 0xc7;
constexpr int InstUndocumented_instruction__DCP_zp = 0xc7;
constexpr int InstINY = 0xc8;
constexpr int InstCMP_imm = 0xc9;
constexpr int InstDEX = 0xca;
constexpr int InstNOP_0xcb = 0xcb;
constexpr int InstUndocumented_instruction__ASX_imm = 0xcb;
constexpr int InstCPY_abs = 0xcc;
constexpr int InstCMP_abs = 0xcd;
constexpr int InstDEC_abs = 0xce;
constexpr int InstNOP_0xcf = 0xcf;
constexpr int InstUndocumented_instruction__DCP_abs = 0xcf;
constexpr int InstBNE_rel = 0xd0;
constexpr int InstCMP_zp_Y = 0xd1;
constexpr int InstCMP_zp = 0xd2;
constexpr int InstUndocumented_instruction__KIL_0xd2 = 0xd2;
constexpr int InstNOP_0xd3 = 0xd3;
constexpr int InstUndocumented_instruction__DCP_zp_Y = 0xd3;
constexpr int InstCMP_zp_X = 0xd5;
constexpr int InstDEC_zp_X = 0xd6;
constexpr int InstNOP_0xd7 = 0xd7;
constexpr int InstUndocumented_instruction__DCP_zp_X = 0xd7;
constexpr int InstCLD = 0xd8;
constexpr int InstCMP_abs_Y = 0xd9;
constexpr int InstPHX = 0xda;
constexpr int InstUndocumented_instruction__NOP_0xda = 0xda;
constexpr int InstNOP_0xdb = 0xdb;
constexpr int InstUndocumented_instruction__DCP_abs_Y = 0xdb;
constexpr int InstNOP_abs = 0xdc;
constexpr int InstUndocumented_instruction__NOP_abs_X_0xfc = 0xfc;
constexpr int InstCMP_abs_X = 0xdd;
constexpr int InstDEC_abs_X = 0xde;
constexpr int InstNOP_0xdf = 0xdf;
constexpr int InstUndocumented_instruction__DCP_abs_X = 0xdf;
constexpr int InstCPX_imm = 0xe0;
constexpr int InstSBC_zp_X_0xe1 = 0xe1;
constexpr int InstNOP_0xe3 = 0xe3;
constexpr int InstUndocumented_instruction__ISC_zp_X_0xe3 = 0xe3;
constexpr int InstCPX_zp = 0xe4;
constexpr int InstSBC_zp_0xe5 = 0xe5;
constexpr int InstINC_zp = 0xe6;
constexpr int InstUndocumented_instruction__ISC_zp = 0xe7;
constexpr int InstINX = 0xe8;
constexpr int InstSBC_imm_0xe9 = 0xe9;
constexpr int InstNOP_0xea = 0xea;
constexpr int InstNOP_0xeb = 0xeb;
constexpr int InstSBC_imm = 0xeb;
constexpr int InstCPX_abs = 0xec;
constexpr int InstSBC_abs = 0xed;
constexpr int InstINC_abs = 0xee;
constexpr int InstNOP_0xef = 0xef;
constexpr int InstUndocumented_instruction__ISC_abs = 0xef;
constexpr int InstBEQ_rel = 0xf0;
constexpr int InstSBC_zp_Y = 0xf1;
constexpr int InstSBC_zp = 0xf2;
constexpr int InstUndocumented_instruction__KIL_0xf2 = 0xf2;
constexpr int InstNOP_0xf3 = 0xf3;
constexpr int InstUndocumented_instruction__ISC_zp_Y = 0xf3;
constexpr int InstSBC_zp_X = 0xf5;
constexpr int InstINC_zp_X = 0xf6;
constexpr int InstNOP_0xf7 = 0xf7;
constexpr int InstUndocumented_instruction__ISC_zp_X = 0xf7;
constexpr int InstSED = 0xf8;
constexpr int InstSBC_abs_Y = 0xf9;
constexpr int InstPLX = 0xfa;
constexpr int InstUndocumented_instruction__NOP = 0xfa;
constexpr int InstNOP_0xfb = 0xfb;
constexpr int InstUndocumented_instruction__ISC_abs_Y = 0xfb;
constexpr int InstSBC_abs_X = 0xfd;
constexpr int InstINC_abs_X = 0xfe;
constexpr int InstNOP_0xff = 0xff;
constexpr int InstUndocumented_instruction__ISC_abs_X = 0xff;


#endif
