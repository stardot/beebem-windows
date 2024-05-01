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

constexpr int Inst_BRK = 0x0;
constexpr int Inst_ORA_zp_X_0x1 = 0x1;
constexpr int Inst_NOP_imm = 0x2;
constexpr int Inst_Undocumented_Inst_ruction__KIL_0x22 = 0x22;
constexpr int Inst_NOP_0x42 = 0x42;
constexpr int Inst_Undocumented_Inst_ruction__SLO_zp_X = 0x62;
constexpr int Inst_TSB_zp = 0x4;
constexpr int Inst_Undocumented_Inst_ruction__NOP_zp = 0x4;
constexpr int Inst_ORA_zp_0x05 = 0x5;
constexpr int Inst_ASL_zp = 0x6;
constexpr int Inst_NOP_0x7 = 0x7;
constexpr int Inst_Undocumented_Inst_ruction__SLO_zp = 0x7;
constexpr int Inst_PHP = 0x8;
constexpr int Inst_ORA_imm = 0x9;
constexpr int Inst_ASL_A = 0xa;
constexpr int Inst_NOP_0xb = 0xb;
constexpr int Inst_Undocumented_Inst_ruction__ANC_imm = 0x2b;
constexpr int Inst_TSB_abs = 0xc;
constexpr int Inst_Undocumented_Inst_ruction__NOP_abs = 0xc;
constexpr int Inst_ORA_abs = 0xd;
constexpr int Inst_ASL_abs = 0xe;
constexpr int Inst_NOP_0xf = 0xf;
constexpr int Inst_Undocumented_Inst_ruction__SLO_abs = 0xf;
constexpr int Inst_BPL_rel = 0x10;
constexpr int Inst_ORA_zp_Y = 0x11;
constexpr int Inst_ORA_zp = 0x12;
constexpr int Inst_Undocumented_Inst_ruction__KIL_0x12 = 0x12;
constexpr int Inst_NOP_0x13 = 0x13;
constexpr int Inst_Undocumented_Inst_ruction__SLO_zp_Y = 0x13;
constexpr int Inst_TRB_zp = 0x14;
constexpr int Inst_Undocumented_Inst_ruction__NOP_zp_X = 0x14;
constexpr int Inst_ORA_zp_X = 0x15;
constexpr int Inst_ASL_zp_X = 0x16;
constexpr int Inst_NOP_0x17 = 0x17;
constexpr int Inst_Undocumented_Inst_ruction__SLO_zp_X_0x17 = 0x17;
constexpr int Inst_CLC = 0x18;
constexpr int Inst_ORA_abs_Y = 0x19;
constexpr int Inst_INC_A = 0x1a;
constexpr int Inst_Undocumented_Inst_ruction__NOP_0x1a = 0x1a;
constexpr int Inst_NOP_0x1b = 0x1b;
constexpr int Inst_Undocumented_Inst_ruction__SLO_abs_Y = 0x1b;
constexpr int Inst_TRB_abs = 0x1c;
constexpr int Inst_Undocumented_Inst_ruction__NOP_abs_X = 0x1c;
constexpr int Inst_ORA_abs_X = 0x1d;
constexpr int Inst_ASL_abs_X = 0x1e;
constexpr int Inst_NOP_0x1f = 0x1f;
constexpr int Inst_Undocumented_Inst_ruction__SLO_abs_X = 0x1f;
constexpr int Inst_JSR_abs = 0x20;
constexpr int Inst_AND_zp_X_0x21 = 0x21;
constexpr int Inst_NOP_0x23 = 0x23;
constexpr int Inst_Undocumented_Inst_ruction__RLA_zp_X = 0x23;
constexpr int Inst_BIT_zp = 0x24;
constexpr int Inst_AND_zp_0x25 = 0x25;
constexpr int Inst_ROL_zp = 0x26;
constexpr int Inst_NOP_0x27 = 0x27;
constexpr int Inst_Undocumented_Inst_ruction__RLA_zp = 0x27;
constexpr int Inst_PLP = 0x28;
constexpr int Inst_AND_imm = 0x29;
constexpr int Inst_ROL_A = 0x2a;
constexpr int Inst_BIT_abs = 0x2c;
constexpr int Inst_AND_abs = 0x2d;
constexpr int Inst_ROL_abs = 0x2e;
constexpr int Inst_NOP_0x2f = 0x2f;
constexpr int Inst_Undocumented_Inst_ruction__RLA_abs = 0x2f;
constexpr int Inst_BMI_rel = 0x30;
constexpr int Inst_AND_zp_Y = 0x31;
constexpr int Inst_AND_zp = 0x32;
constexpr int Inst_Undocumented_Inst_ruction__KIL_0x32 = 0x32;
constexpr int Inst_NOP_0x33 = 0x33;
constexpr int Inst_Undocumented_Inst_ruction__RLA_zp_Y = 0x33;
constexpr int Inst_BIT_abs_X_0x34 = 0x34;
constexpr int Inst_Undocumented_Inst_ruction__NOP_zp_X_0x34 = 0x34;
constexpr int Inst_AND_zp_X = 0x35;
constexpr int Inst_ROL_zp_X = 0x36;
constexpr int Inst_NOP_0x37 = 0x37;
constexpr int Inst_Undocumented_Inst_ruction__RLA_zp_X_0x37 = 0x37;
constexpr int Inst_SEC = 0x38;
constexpr int Inst_AND_abs_Y = 0x39;
constexpr int Inst_DEC_A = 0x3a;
constexpr int Inst_Undocumented_Inst_ruction__NOP_0x3a = 0x3a;
constexpr int Inst_NOP_0x3b = 0x3b;
constexpr int Inst_Undocumented_Inst_ruction__RLA_abs_Y = 0x3b;
constexpr int Inst_BIT_abs_X = 0x3c;
constexpr int Inst_Undocumented_Inst_ruction__NOP_abs_x = 0x3c;
constexpr int Inst_AND_abs_X = 0x3d;
constexpr int Inst_ROL_abs_X = 0x3e;
constexpr int Inst_NOP_0x3f = 0x3f;
constexpr int Inst_Undocumented_Inst_ruction__RLA_abs_X = 0x3f;
constexpr int Inst_RTI = 0x40;
constexpr int Inst_EOR_zp_X_0x41 = 0x41;
constexpr int Inst_NOP_0x43 = 0x43;
constexpr int Inst_Undocumented_Inst_ruction__SRE_zp_X = 0x43;
constexpr int Inst_NOP_zp = 0x44;
constexpr int Inst_EOR_zp_0x45 = 0x45;
constexpr int Inst_LSR_zp = 0x46;
constexpr int Inst_NOP_0x47 = 0x47;
constexpr int Inst_Undocumented_Inst_ruction__SRE_zp = 0x47;
constexpr int Inst_PHA = 0x48;
constexpr int Inst_EOR_imm = 0x49;
constexpr int Inst_LSR_A = 0x4a;
constexpr int Inst_NOP_0x4b = 0x4b;
constexpr int Inst_Undocumented_Inst_ruction__ALR_imm = 0x4b;
constexpr int Inst_JMP_abs_0x4c = 0x4c;
constexpr int Inst_EOR_abs = 0x4d;
constexpr int Inst_LSR_abs = 0x4e;
constexpr int Inst_NOP_0x4f = 0x4f;
constexpr int Inst_Undocumented_Inst_ruction__SRE_abs = 0x4f;
constexpr int Inst_BVC_rel = 0x50;
constexpr int Inst_EOR_zp_Y = 0x51;
constexpr int Inst_EOR_zp = 0x52;
constexpr int Inst_Undocumented_Inst_ruction__KIL_0x52 = 0x52;
constexpr int Inst_NOP_0x53 = 0x53;
constexpr int Inst_Undocumented_Inst_ruction__SRE_zp_Y = 0x53;
constexpr int Inst_Undocumented_Inst_ruction__NOP_zp_X_1 = 0x54;
constexpr int Inst_Undocumented_Inst_ruction__NOP_zp_X_2 = 0xd4;
constexpr int Inst_Undocumented_Inst_ruction__NOP_zp_X_3 = 0xf4;
constexpr int Inst_EOR_zp_X = 0x55;
constexpr int Inst_LSR_zp_X = 0x56;
constexpr int Inst_NOP_0x57 = 0x57;
constexpr int Inst_Undocumented_Inst_ruction__SRE_zp_X_0x57 = 0x57;
constexpr int Inst_CLI = 0x58;
constexpr int Inst_EOR_abs_Y = 0x59;
constexpr int Inst_PHY = 0x5a;
constexpr int Inst_Undocumented_Inst_ruction__NOP_CommentOnly = 0x5a;
constexpr int Inst_NOP_0x5b = 0x5b;
constexpr int Inst_Undocumented_Inst_ruction__SRE_abs_Y = 0x5b;
constexpr int Inst_NOP_abs_0x5c = 0x5c;
constexpr int Inst_Undocumented_Inst_ruction__NOP_abs_x_0x5c = 0x5c;
constexpr int Inst_EOR_abs_X = 0x5d;
constexpr int Inst_LSR_abs_X = 0x5e;
constexpr int Inst_NOP_0x5f = 0x5f;
constexpr int Inst_Undocumented_Inst_ruction__SRE_abs_X = 0x5f;
constexpr int Inst_RTS = 0x60;
constexpr int Inst_ADC_zp_X_0x61 = 0x61;
constexpr int Inst_NOP_0x63 = 0x63;
constexpr int Inst_Undocumented_Inst_ruction__RRA_zp_X = 0x63;
constexpr int Inst_STZ_zp = 0x64;
constexpr int Inst_Undocumented_Inst_ruction__NOP_zp_0x64 = 0x64;
constexpr int Inst_ADC_zp_0x65 = 0x65;
constexpr int Inst_ROR_zp = 0x66;
constexpr int Inst_NOP_0x67 = 0x67;
constexpr int Inst_Undocumented_Inst_ruction__RRA_zp = 0x67;
constexpr int Inst_PLA = 0x68;
constexpr int Inst_ADC_imm = 0x69;
constexpr int Inst_ROR_A = 0x6a;
constexpr int Inst_NOP_0x6b = 0x6b;
constexpr int Inst_Undocumented_Inst_ruction__ARR_imm = 0x6b;
constexpr int Inst_JMP_abs = 0x6c;
constexpr int Inst_ADC_abs = 0x6d;
constexpr int Inst_ROR_abs = 0x6e;
constexpr int Inst_NOP_0x6f = 0x6f;
constexpr int Inst_Undocumented_Inst_ruction__RRA_abs = 0x6f;
constexpr int Inst_BVS_rel = 0x70;
constexpr int Inst_ADC_zp_Y = 0x71;
constexpr int Inst_ADC_zp = 0x72;
constexpr int Inst_Undocumented_Inst_ruction__KIL_0x72 = 0x72;
constexpr int Inst_NOP_0x73 = 0x73;
constexpr int Inst_Undocumented_Inst_ruction__RRA_zp_Y = 0x73;
constexpr int Inst_STZ_zp_X = 0x74;
constexpr int Inst_Undocumented_Inst_ruction__NOP_zp_x = 0x74;
constexpr int Inst_ADC_zp_X = 0x75;
constexpr int Inst_ROR_zp_X = 0x76;
constexpr int Inst_NOP_0x77 = 0x77;
constexpr int Inst_Undocumented_Inst_ruction__RRA_zp_X_0x77 = 0x77;
constexpr int Inst_SEI = 0x78;
constexpr int Inst_ADC_abs_Y = 0x79;
constexpr int Inst_PLY = 0x7a;
constexpr int Inst_Undocumented_Inst_ruction__NOP_0x7a = 0x7a;
constexpr int Inst_NOP_0x7b = 0x7b;
constexpr int Inst_Undocumented_Inst_ruction__RRA_abs_Y = 0x7b;
constexpr int Inst_JMP_abs_X = 0x7c;
constexpr int Inst_Undocumented_Inst_ruction__NOP_abs_X_0x7c = 0x7c;
constexpr int Inst_ADC_abs_X = 0x7d;
constexpr int Inst_ROR_abs_X = 0x7e;
constexpr int Inst_NOP_0x7f = 0x7f;
constexpr int Inst_Undocumented_Inst_ruction__RRA_abs_X = 0x7f;
constexpr int Inst_BRA_rel = 0x80;
constexpr int Inst_Undocumented_Inst_ruction__NOP_imm = 0x80;
constexpr int Inst_STA_zp_X_0x81 = 0x81;
constexpr int Inst_Undocumented_Inst_ruction__NOP_imm_0x82 = 0x82;
constexpr int Inst_Undocumented_Inst_ruction__NOP_imm_0xc2 = 0xc2;
constexpr int Inst_Undocumented_Inst_ruction__NOP_imm_0xe2 = 0xe2;
constexpr int Inst_NOP_0x83 = 0x83;
constexpr int Inst_Undocumented_Inst_ruction__SAX_zp_X = 0x83;
constexpr int Inst_STY_zp = 0x84;
constexpr int Inst_STA_zp_0x85 = 0x85;
constexpr int Inst_STX_zp = 0x86;
constexpr int Inst_NOP_0x87 = 0x87;
constexpr int Inst_Undocumented_Inst_ruction__SAX_zp_NO_FLAGS = 0x87;
constexpr int Inst_DEY = 0x88;
constexpr int Inst_BIT_imm = 0x89;
constexpr int Inst_Undocumented_Inst_ruction__NOP_imm_0x89 = 0x89;
constexpr int Inst_TXA_0x8a = 0x8a;
constexpr int Inst_NOP_0x8b = 0x8b;
constexpr int Inst_Undocumented_Inst_ruction__XAA_imm = 0x8b;
constexpr int Inst_STY_abs = 0x8c;
constexpr int Inst_STA_abs = 0x8d;
constexpr int Inst_STX_abs = 0x8e;
constexpr int Inst_NOP_0x8f = 0x8f;
constexpr int Inst_Undocumented_Inst_ruction__SAX_abs = 0x8f;
constexpr int Inst_BCC_rel = 0x90;
constexpr int Inst_STA_zp_Y = 0x91;
constexpr int Inst_STA_zp = 0x92;
constexpr int Inst_Undocumented_Inst_ruction__KIL_0x92 = 0x92;
constexpr int Inst_NOP_0x93 = 0x93;
constexpr int Inst_Undocumented_Inst_ruction__AHX_zp_Y = 0x93;
constexpr int Inst_STY_zp_X = 0x94;
constexpr int Inst_STA_zp_X = 0x95;
constexpr int Inst_STX_zp_X = 0x96;
constexpr int Inst_NOP_0x97 = 0x97;
constexpr int Inst_Undocumented_Inst_ruction__SAX_zp_Y = 0x97;
constexpr int Inst_TYA = 0x98;
constexpr int Inst_STA_abs_Y = 0x99;
constexpr int Inst_TXS = 0x9a;
constexpr int Inst_NOP_0x9b = 0x9b;
constexpr int Inst_Undocumented_Inst_ruction__TAS_abs_Y = 0x9b;
constexpr int Inst_STZ_abs = 0x9c;
constexpr int Inst_Undocumented_Inst_ruction__SHY_abs_X = 0x9c;
constexpr int Inst_STA_abs_X = 0x9d;
constexpr int Inst_STZ_abs_x = 0x9e;
constexpr int Inst_Undocumented_Inst_ruction__SHX_abs_Y = 0x9e;
constexpr int Inst_NOP_0x9f = 0x9f;
constexpr int Inst_Undocumented_Inst_ruction__AHX_abs_Y = 0x9f;
constexpr int Inst_LDY_imm = 0xa0;
constexpr int Inst_LDA_zp_X_0xa1 = 0xa1;
constexpr int Inst_LDX_imm = 0xa2;
constexpr int Inst_NOP_0xa3 = 0xa3;
constexpr int Inst_Undocumented_Inst_ruction__LAX_zp_X = 0xa3;
constexpr int Inst_LDY_zp = 0xa4;
constexpr int Inst_LDA_zp_0xa5 = 0xa5;
constexpr int Inst_LDX_zp = 0xa6;
constexpr int Inst_NOP_0xa7 = 0xa7;
constexpr int Inst_Undocumented_Inst_ruction__LAX_zp = 0xa7;
constexpr int Inst_TAY = 0xa8;
constexpr int Inst_LDA_imm = 0xa9;
constexpr int Inst_TXA = 0xaa;
constexpr int Inst_NOP_0xab = 0xab;
constexpr int Inst_Undocumented_Inst_ruction__LAX_imm = 0xab;
constexpr int Inst_LDY_abs = 0xac;
constexpr int Inst_LDA_abs = 0xad;
constexpr int Inst_LDX_abs = 0xae;
constexpr int Inst_NOP_0xaf = 0xaf;
constexpr int Inst_Undocumented_Inst_ruction__LAX_abs = 0xaf;
constexpr int Inst_BCS_rel = 0xb0;
constexpr int Inst_LDA_zp_Y = 0xb1;
constexpr int Inst_LDA_zp = 0xb2;
constexpr int Inst_Undocumented_Inst_ruction__KIL_0xb2 = 0xb2;
constexpr int Inst_NOP_0xb3 = 0xb3;
constexpr int Inst_Undocumented_Inst_ruction__LAX_zp_Y_0xb3 = 0xb3;
constexpr int Inst_LDY_zp_X = 0xb4;
constexpr int Inst_LDA_zp_X = 0xb5;
constexpr int Inst_LDX_zp_Y = 0xb6;
constexpr int Inst_NOP_0xb7 = 0xb7;
constexpr int Inst_Undocumented_Inst_ruction__LAX_zp_Y = 0xb7;
constexpr int Inst_CLV = 0xb8;
constexpr int Inst_LDA_abs_Y = 0xb9;
constexpr int Inst_TSX = 0xba;
constexpr int Inst_NOP_0xbb = 0xbb;
constexpr int Inst_Undocumented_Inst_ruction__LAS_abs_Y = 0xbb;
constexpr int Inst_LDY_abs_X = 0xbc;
constexpr int Inst_LDA_abs_X = 0xbd;
constexpr int Inst_LDX_abs_Y = 0xbe;
constexpr int Inst_NOP_0xbf = 0xbf;
constexpr int Inst_Undocumented_Inst_ruction__LAX_abs_Y = 0xbf;
constexpr int Inst_CPY_imm = 0xc0;
constexpr int Inst_CMP_zp_X_0x1c = 0xc1;
constexpr int Inst_NOP_0xc3 = 0xc3;
constexpr int Inst_Undocument_Inst_ruction__DCP_zp_X = 0xc3;
constexpr int Inst_CPY_zp = 0xc4;
constexpr int Inst_CMP_zp_0xc5 = 0xc5;
constexpr int Inst_DEC_zp = 0xc6;
constexpr int Inst_NOP_0xc7 = 0xc7;
constexpr int Inst_Undocumented_Inst_ruction__DCP_zp = 0xc7;
constexpr int Inst_INY = 0xc8;
constexpr int Inst_CMP_imm = 0xc9;
constexpr int Inst_DEX = 0xca;
constexpr int Inst_NOP_0xcb = 0xcb;
constexpr int Inst_Undocumented_Inst_ruction__ASX_imm = 0xcb;
constexpr int Inst_CPY_abs = 0xcc;
constexpr int Inst_CMP_abs = 0xcd;
constexpr int Inst_DEC_abs = 0xce;
constexpr int Inst_NOP_0xcf = 0xcf;
constexpr int Inst_Undocumented_Inst_ruction__DCP_abs = 0xcf;
constexpr int Inst_BNE_rel = 0xd0;
constexpr int Inst_CMP_zp_Y = 0xd1;
constexpr int Inst_CMP_zp = 0xd2;
constexpr int Inst_Undocumented_Inst_ruction__KIL_0xd2 = 0xd2;
constexpr int Inst_NOP_0xd3 = 0xd3;
constexpr int Inst_Undocumented_Inst_ruction__DCP_zp_Y = 0xd3;
constexpr int Inst_CMP_zp_X = 0xd5;
constexpr int Inst_DEC_zp_X = 0xd6;
constexpr int Inst_NOP_0xd7 = 0xd7;
constexpr int Inst_Undocumented_Inst_ruction__DCP_zp_X = 0xd7;
constexpr int Inst_CLD = 0xd8;
constexpr int Inst_CMP_abs_Y = 0xd9;
constexpr int Inst_PHX = 0xda;
constexpr int Inst_Undocumented_Inst_ruction__NOP_0xda = 0xda;
constexpr int Inst_NOP_0xdb = 0xdb;
constexpr int Inst_Undocumented_Inst_ruction__DCP_abs_Y = 0xdb;
constexpr int Inst_NOP_abs = 0xdc;
constexpr int Inst_Undocumented_Inst_ruction__NOP_abs_X_0xfc = 0xfc;
constexpr int Inst_CMP_abs_X = 0xdd;
constexpr int Inst_DEC_abs_X = 0xde;
constexpr int Inst_NOP_0xdf = 0xdf;
constexpr int Inst_Undocumented_Inst_ruction__DCP_abs_X = 0xdf;
constexpr int Inst_CPX_imm = 0xe0;
constexpr int Inst_SBC_zp_X_0xe1 = 0xe1;
constexpr int Inst_NOP_0xe3 = 0xe3;
constexpr int Inst_Undocumented_Inst_ruction__ISC_zp_X_0xe3 = 0xe3;
constexpr int Inst_CPX_zp = 0xe4;
constexpr int Inst_SBC_zp_0xe5 = 0xe5;
constexpr int Inst_INC_zp = 0xe6;
constexpr int Inst_Undocumented_Inst_ruction__ISC_zp = 0xe7;
constexpr int Inst_INX = 0xe8;
constexpr int Inst_SBC_imm_0xe9 = 0xe9;
constexpr int Inst_NOP_0xea = 0xea;
constexpr int Inst_NOP_0xeb = 0xeb;
constexpr int Inst_SBC_imm = 0xeb;
constexpr int Inst_CPX_abs = 0xec;
constexpr int Inst_SBC_abs = 0xed;
constexpr int Inst_INC_abs = 0xee;
constexpr int Inst_NOP_0xef = 0xef;
constexpr int Inst_Undocumented_Inst_ruction__ISC_abs = 0xef;
constexpr int Inst_BEQ_rel = 0xf0;
constexpr int Inst_SBC_zp_Y = 0xf1;
constexpr int Inst_SBC_zp = 0xf2;
constexpr int Inst_Undocumented_Inst_ruction__KIL_0xf2 = 0xf2;
constexpr int Inst_NOP_0xf3 = 0xf3;
constexpr int Inst_Undocumented_Inst_ruction__ISC_zp_Y = 0xf3;
constexpr int Inst_SBC_zp_X = 0xf5;
constexpr int Inst_INC_zp_X = 0xf6;
constexpr int Inst_NOP_0xf7 = 0xf7;
constexpr int Inst_Undocumented_Inst_ruction__ISC_zp_X = 0xf7;
constexpr int Inst_SED = 0xf8;
constexpr int Inst_SBC_abs_Y = 0xf9;
constexpr int Inst_PLX = 0xfa;
constexpr int Inst_Undocumented_Inst_ruction__NOP = 0xfa;
constexpr int Inst_NOP_0xfb = 0xfb;
constexpr int Inst_Undocumented_Inst_ruction__ISC_abs_Y = 0xfb;
constexpr int Inst_SBC_abs_X = 0xfd;
constexpr int Inst_INC_abs_X = 0xfe;
constexpr int Inst_NOP_0xff = 0xff;
constexpr int Inst_Undocumented_Inst_ruction__ISC_abs_X = 0xff;


#endif
