/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2001  David Sharp

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

//////////////////////////////////////////////////////////////////////
// ArmDisassembler.cpp: implementation of the CArmDisassembler class.
// Part of Tarmac
// By David Sharp
// http://www.davidsharp.com
//////////////////////////////////////////////////////////////////////

#include <stdio.h>
#include <string.h>

#include "ArmDisassembler.h"

static const char* decodeRegisterList(uint32 instruction);
static char* decodeSingleDataSwap(uint32 address, uint32 instruction, char* buff);
static char* decodeMultiply(uint32 address, uint32 instruction, char* buff);
static const char* decodeConditionCode(uint32 instruction);
static char* decodeSoftwareInterrupt(uint32 address, uint32 instruction, char* buff);
static char* decodeCoProRegTransferOrDataOperation(uint32 address, uint32 instruction, char* buff);
static char* decodeCoProDTPostIndex(uint32 address, uint32 instruction, char* buff);
static char* decodeCoProDTPreIndex(uint32 address, uint32 instruction, char* buff);
static char* decodeBranchWithLink(uint32 address, uint32 instruction, char* buff);
static char* decodeBranch(uint32 address, uint32 instruction, char* buff);
static char* decodeBlockDTPreIndex(uint32 address, uint32 instruction, char* buff);
static char* decodeBlockDTPostIndex(uint32 address, uint32 instruction, char* buff);
static char* decodeSingleDTRegOffsetPreIndex(uint32 address, uint32 instruction, char* buff);
static char* decodeSingleDTRegOffsetPostIndex(uint32 address, uint32 instruction, char* buff);
static char* decodeSingleDTImmOffsetPreIndex(uint32 address, uint32 instruction, char* buff);
static char* decodeSingleDTImmOffsetPostIndex(uint32 address, uint32 instruction, char* buff);
static char* decodeDataProcessing(uint32 address, uint32 instruction, char* buff);
static char* decodeSingleDataSwapOrDataProcessing(uint32 address, uint32 instruction, char* buff);
static char* decodeMultiplyOrDataProcessing(uint32 address, uint32 instruction, char* buff);

char *Arm_disassemble(uint32 address, uint32 instruction, char *buff)
{
	// decode based on bits 24 - 27 of instruction
	switch( getField(instruction, 24, 27) )
	{
		case 0	:	return decodeMultiplyOrDataProcessing(address, instruction, buff);
		case 1	:	return decodeSingleDataSwapOrDataProcessing(address, instruction, buff);
		case 2	:	return decodeDataProcessing(address, instruction, buff);
		case 3	:	return decodeDataProcessing(address, instruction, buff);
		case 4	:	return decodeSingleDTImmOffsetPostIndex(address, instruction, buff);
		case 5	:	return decodeSingleDTImmOffsetPreIndex(address, instruction, buff);
		case 6	:	return decodeSingleDTRegOffsetPostIndex(address, instruction, buff);
		case 7	:	return decodeSingleDTRegOffsetPreIndex(address, instruction, buff);
		case 8	:	return decodeBlockDTPostIndex(address, instruction, buff);
		case 9	:	return decodeBlockDTPreIndex(address, instruction, buff);
		case 10	:	return decodeBranch(address, instruction, buff);
		case 11	:	return decodeBranchWithLink(address, instruction, buff);
		case 12 :	return decodeCoProDTPreIndex(address, instruction, buff);
		case 13 :	return decodeCoProDTPostIndex(address, instruction, buff);
		case 14	:	return decodeCoProRegTransferOrDataOperation(address, instruction, buff);
		case 15	:	return decodeSoftwareInterrupt(address, instruction, buff);
	}

	strcpy(buff, "ERROR DISASSEMBLING IN Disassemble()");
	return buff;
}

static char *decodeMultiplyOrDataProcessing(uint32 address, uint32 instruction, char *buff)
{
	// check for bit pattern 1001 in bits 4-7
	if(	getField(instruction, 4,7) == 0x09 )
	{
		// multiply
		return decodeMultiply(address, instruction, buff);
	}
	else
	{
		// data processing
		return decodeDataProcessing(address, instruction, buff);
	}
}

static char *decodeSingleDataSwapOrDataProcessing(uint32 address, uint32 instruction, char *buff)
{
	// check for bit pattern 0000 1001 in bits 4-11
	if( getField(instruction, 4,11) == 0x09)
	{
		// single data swap
		return decodeSingleDataSwap(address, instruction, buff);
	}
	else
	{
		// data processing
		return decodeDataProcessing(address, instruction, buff);
	}
}

static char *decodeDataProcessing(uint32 /* address */, uint32 instruction, char *buff)
{
	// table of opcode names
	static const char* const opcodeNames[16] =
	{
		"and", "eor", "sub", "rsb",
		"add", "adc", "sbc", "rsc",
		"tst", "teq", "cmp", "cmn",
		"orr", "mov", "bic", "mvn"
	};

	static const bool useRd[16] =
	{
		true,  true,  true,  true,
		true,  true,  true,  true,
		false, false, false, false,
		true,  true,  true,  true
	};

	static const bool useRn[16] =
	{
		true,  true,  true,  true,
		true,  true,  true,  true,
		true,  true,  true,  true,
		true,  false, true,  false
	};

	// deduce opcode for specific data processing instruction
	// based on bits 21-24
	uint8 opcode = (uint8)( getField(instruction, 21,24) );
	strcpy(buff, opcodeNames[ opcode ]);

	// add condition code information
	strcat(buff, decodeConditionCode(instruction));

	// table of which flags don't need explicit S adding
	// so that S is not added for tst, teq, cmp and cmn
	static const bool opcodeSetsFlagsExplicitly[16] =
	{
		true,  true,  true,  true,
		true,  true,  true,  true,
		false, false, false, false,
		true,  true,  true,  true,
	};

	// test S flag (set PSR flags), based on bit 20 and whether this is implicit in the opcode
	if( getBit(instruction, 20) && opcodeSetsFlagsExplicitly[opcode] )
	{
		strcat(buff, "S");
	}

	// append spaces as necessary, so mnemonic with suffixes is 8 characters long
	while( strlen(buff) < 8 )
	{
			strcat(buff, " ");
	}

	char registerNumber[10];

	if(useRd[getField(instruction, 21,24)] )
	{
		// add destination register
		sprintf(registerNumber, "r%d", (instruction >> 12) & 0x0F );
		strcat(buff, registerNumber);
	}

	// the 'mov' and 'mvn' instructions dosn't use the first operand so don't output it
	if( useRn[getField(instruction, 21,24)] )
	{

		// add comma if needed
		if( useRd[getField(instruction, 21, 24)] )
			strcat(buff, ",");

		// add first operand register
		sprintf(registerNumber, "r%d", getField(instruction, 16, 19) );
		strcat(buff, registerNumber);
	}

	// add comma
	strcat(buff, ",");

	// decide type of second operand from bit 25
	if( getBit(instruction, 25) )
	{
		// 2nd operand is immediate

		// get immediate value from bits 0-7
		uint32 immediate = getField(instruction, 0,7);

		// get rotate (right) applied to immediate from bits 8-11
		uint32 rotate = getField(instruction, 8,11);
		rotate *= 2;

		// rotate immediate right
		uint32 actualImmediate = (immediate >> rotate) | (immediate << (32 - rotate));

		// add immediate value
		char immediateValue[12];
		sprintf(immediateValue, "#0x%x", actualImmediate);
		strcat(buff, immediateValue);
	}
	else
	{
		// 2nd operand is register

		// add second operand register (to which the shift is applied) from bits 0-3
		sprintf(registerNumber, "r%d", instruction & 0x0F );
		strcat(buff, registerNumber);

		// if there's any shift at all
		if( getField(instruction, 4, 11) )
		{
			strcat(buff, ",");

			// table of shift mnemonics
			static const char* const shiftMnemonic[4] =
			{
				"lsl", "lsr", "asr", "ror"
			};

			// ??? cope with RRX

			// ??? cope with LSR #0 encoding being LSR #32

			// add shift mnemonic from bits 5-6
			strcat(buff, shiftMnemonic[ getField(instruction, 5,6) ]);
			strcat(buff, " ");

			// decide type of shift amount based on bit 4
			if( getBit(instruction, 4) )
			{
				// shifted by amount in register

				// add register to shift by from bits 8-11
				sprintf(registerNumber, "r%d", getField(instruction, 8,11) );
				strcat(buff, registerNumber);
			}
			else
			{
				// shifted by amount in immediate

				// add immediate value to shift by from bits 7-11
				char immediateValue[12];
				sprintf(immediateValue, "#%d", getField(instruction, 7,11) );
				strcat(buff, immediateValue);
			}
		} // end of if any shift
	}

	return buff;
}

static char *decodeSingleDTImmOffsetPostIndex(uint32 /* address */, uint32 instruction, char *buff)
{
	// decode whether load or store from bit 20
	if( getBit(instruction, 20) )
	{
		// load from memory
		strcpy(buff, "ldr");
	}
	else
	{
		// store to memory
		strcpy(buff, "str");
	}

	// add condition code information
	strcat(buff, decodeConditionCode(instruction));

	// decode whether we're dealing in byte quantities or not from bit 22
	if( getBit(instruction, 22) )
	{
		// byte quantity
		strcat(buff, "b");
	}

	// check for trans bit
	if( getBit(instruction, 21) )
		strcat(buff, "t");

	// append spaces as necessary, so mnemonic with suffixes is 8 characters long
	while( strlen(buff) < 8 )
	{
		strcat(buff, " ");
	}

	char registerNumber[12];

	// add source/destination register from bits 12-15
	sprintf(registerNumber, "r%d,[", getField(instruction, 12,15) );
	strcat(buff, registerNumber);

	// add base register from bits 16-19
	sprintf(registerNumber, "r%d],#", getField(instruction, 16,19) );
	strcat(buff, registerNumber);

	// add up/down sign from bit 23
	if( !getBit(instruction, 23) )
	{
		// if down then add subtract symbol
		strcat(buff, "-");
	}

	// add unsigned 12 bit immediate offset from bits 0-11
	char immediateOffset[12];
	sprintf(immediateOffset, "%d", getField(instruction, 0,11) );
	strcat(buff, immediateOffset);

	return buff;
}

static char *decodeSingleDTImmOffsetPreIndex(uint32 /* address */, uint32 instruction, char *buff)
{
	// decode whether load or store from bit 20
	if( getBit(instruction, 20) )
	{
		// load from memory
		strcpy(buff, "ldr");
	}
	else
	{
		// store to memory
		strcpy(buff, "str");
	}

	// add condition code information
	strcat(buff, decodeConditionCode(instruction));

	// decode whether we're dealing in byte quantities or not from bit 22
	if( getBit(instruction, 22) )
	{
		// byte quantity
		strcat(buff, "b");
	}

	// append spaces as necessary, so mnemonic with suffixes is 8 characters long
	while( strlen(buff) < 8 )
	{
		strcat(buff, " ");
	}

	char registerNumber[12];

	// add source/destination register from bits 12-15
	sprintf(registerNumber, "r%d,[", getField(instruction, 12,15) );
	strcat(buff, registerNumber);

	// add base register from bits 16-19
	sprintf(registerNumber, "r%d,#", getField(instruction, 16,19) );
	strcat(buff, registerNumber);

	// add up/down sign from bit 23
	if( !getBit(instruction, 23) )
	{
		// if down then add subtract symbol
		strcat(buff, "-");
	}

	// add unsigned 12 bit immediate offset from bits 0-11
	char immediateOffset[12];
	sprintf(immediateOffset, "%d]", getField(instruction, 0,11) );
	strcat(buff, immediateOffset);

	// decode write-back from bit 21
	if( getBit(instruction, 21) )
	{
		// write-back present
		strcat(buff, "!");
	}

	return buff;
}

static char *decodeSingleDTRegOffsetPostIndex(uint32 /* address */, uint32 instruction, char *buff)
{
	// decode whether load or store from bit 20
	if( getBit(instruction, 20) )
	{
		// load from memory
		strcpy(buff, "ldr");
	}
	else
	{
		// store to memory
		strcpy(buff, "str");
	}

	// add condition code information
	strcat(buff, decodeConditionCode(instruction));

	// decode whether we're dealing in byte quantities or not from bit 22
	if( getBit(instruction, 22) )
	{
		// byte quantity
		strcat(buff, "b");
	}

	// check for trans bit
	if( getBit(instruction, 21) )
		strcat(buff, "t");

	// append spaces as necessary, so mnemonic with suffixes is 8 characters long
	while( strlen(buff) < 8 )
	{
		strcat(buff, " ");
	}

	char registerNumber[12];

	// add source/destination register from bits 12-15
	sprintf(registerNumber, "r%d,[", getField(instruction, 12,15) );
	strcat(buff, registerNumber);

	// add base register from bits 16-19
	sprintf(registerNumber, "r%d],", getField(instruction, 16,19) );
	strcat(buff, registerNumber);

	// ??? it would seem perverse to have an up/down bit apply to a signed register
	// however this needs to be confirmed.

	// add offset register from bits 0-3
	sprintf(registerNumber, "r%d", getField(instruction, 0,3) );
	strcat(buff, registerNumber);

	uint32 imm = getField(instruction, 7,11);

	// table of shift mnemonics
	static const char* const shiftMnemonic[4] =
	{
		"lsl", "lsr", "asr", "ror"
	};

	const char *shiftType = shiftMnemonic[ getField(instruction, 5,6) ];

	// if not LSL #0
	if( !( (strcmp(shiftType, "lsl") == 0) && imm ==0) )
	{
		strcat(buff, ",");

		// add shift mnemonic from bits 5-6
		strcat(buff, shiftType);
		strcat(buff, " ");

		// note, single data stores can only be shifted by an immediate amount

		// add immediate value to shift by from bits 7-11
		strcat(buff, "#");
		char immediateValue[12];
		sprintf(immediateValue, "%d", imm );
		strcat(buff, immediateValue);
	}

	return buff;
}

static char *decodeSingleDTRegOffsetPreIndex(uint32 /* address */, uint32 instruction, char *buff)
{
	// decode whether load or store from bit 20
	if( getBit(instruction, 20) )
	{
		// load from memory
		strcpy(buff, "ldr");
	}
	else
	{
		// store to memory
		strcpy(buff, "str");
	}

	// add condition code information
	strcat(buff, decodeConditionCode(instruction));

	// decode whether we're dealing in byte quantities or not from bit 22
	if( getBit(instruction, 22) )
	{
		// byte quantity
		strcat(buff, "b");
	}

	// append spaces as necessary, so mnemonic with suffixes is 8 characters long
	while( strlen(buff) < 8 )
	{
		strcat(buff, " ");
	}

	char registerNumber[12];

	// add source/destination register from bits 12-15
	sprintf(registerNumber, "r%d,[", getField(instruction, 12,15) );
	strcat(buff, registerNumber);

	// add base register from bits 16-19
	sprintf(registerNumber, "r%d,", getField(instruction, 16,19) );
	strcat(buff, registerNumber);

	// ??? it would seem perverse to have an up/down bit apply to a signed register
	// however this needs to be confirmed.

	// add offset register from bits 0-3
	sprintf(registerNumber, "r%d", getField(instruction, 0,3) );
	strcat(buff, registerNumber);

	uint32 imm = getField(instruction, 7,11);

	// table of shift mnemonics
	static const char* const shiftMnemonic[4] =
	{
		"lsl", "lsr", "asr", "ror"
	};

	const char *shiftType = shiftMnemonic[ getField(instruction, 5,6) ];

	// if not LSL #0
	if( !( (strcmp(shiftType, "lsl") == 0) && imm == 0) )
	{
		strcat(buff, ",");

		// add shift mnemonic from bits 5-6
		strcat(buff, shiftType);
		strcat(buff, " ");

		// note, single data stores can only be shifted by an immediate amount

		// add immediate value to shift by from bits 7-11
		strcat(buff, "#");
		char immediateValue[12];
		sprintf(immediateValue, "%d", imm );
		strcat(buff, immediateValue);
	}

	strcat(buff, "]");

	// check for write-back from bit 21
	if( getBit(instruction, 21) )
	{
		// write-back
		strcat(buff, "!");
	}

	return buff;
}

static char *decodeBlockDTPostIndex(uint32 /* address */, uint32 instruction, char *buff)
{
	// decode whether to load from or store to memory from bit 20
	if( getBit(instruction, 20) )
	{
		// load from memory
		strcpy(buff, "ldm");
	}
	else
	{
		// store to memory
		strcpy(buff, "stm");
	}

	// add condition code information
	strcat(buff, decodeConditionCode(instruction));

	// decode whether to increment or decrement base from bit 23
	if( getBit(instruction, 23) )
	{
		// increment
		strcat(buff, "i");
	}
	else
	{
		// decrement
		strcat(buff, "d");
	}

	// we already decoded from the initial lookup that it's post indexed
	strcat(buff, "a");

	// append spaces as necessary, so mnemonic with suffixes is 8 characters long
	while( strlen(buff) < 8 )
	{
		strcat(buff, " ");
	}

	char registerNumber[12];

	// decode base register from bits 16-19
	sprintf(registerNumber, "r%d", getField(instruction, 16,19) );
	strcat(buff, registerNumber);

	// decode write-back from bit 21
	if( getBit(instruction, 21) )
	{
		strcat(buff, "!");
	}

	strcat(buff, ",{");

	// decode register list from bits 0-15
	strcat(buff, decodeRegisterList(instruction));

	// decode whether to load PSR and force user mode or not from bit 22
	if( getBit(instruction, 22) )
	{
		strcat(buff, "^");
	}

	return buff;
}

static char *decodeBlockDTPreIndex(uint32 /* address */, uint32 instruction, char *buff)
{
	// decode whether to load from or store to memory from bit 20
	if( getBit(instruction, 20) )
	{
		// load from memory
		strcpy(buff, "ldm");
	}
	else
	{
		// store to memory
		strcpy(buff, "stm");
	}

	// add condition code information
	strcat(buff, decodeConditionCode(instruction));

	// decode whether to increment or decrement base from bit 23
	if( getBit(instruction, 23) )
	{
		// increment
		strcat(buff, "i");
	}
	else
	{
		// decrement
		strcat(buff, "d");
	}

	// we already decoded from the initial lookup that it's pre indexed
	strcat(buff, "b");

	// append spaces as necessary, so mnemonic with suffixes is 8 characters long
	while( strlen(buff) < 8 )
	{
		strcat(buff, " ");
	}

	char registerNumber[12];

	// decode base register from bits 16-19
	sprintf(registerNumber, "r%d", getField(instruction, 16,19) );
	strcat(buff, registerNumber);

	// decode write-back from bit 21
	if( getBit(instruction, 21) )
	{
		strcat(buff, "!");
	}

	strcat(buff, ",{");

	// decode register list from bits 0-15
	strcat(buff, decodeRegisterList(instruction));

	// decode whether to load PSR and force user mode or not from bit 22
	if( getBit(instruction, 22) )
	{
		strcat(buff, "^");
	}

	return buff;
}

static char registerList[128];

static const char *decodeRegisterList(uint32 instruction)
{
	uint8 run = 0;
	char registerNumber[12];

	bool commaNeeded = false;	// comma not needed
	strcpy(registerList, "");

	// for each of the 16 registers, r0 to r15
	for(uint8 regNumber=0; regNumber<16; regNumber++)
	{
		if(run)
		{
			if(getBit(instruction, regNumber) )
			{
				// bit set
				if( regNumber<15 && getBit(instruction, regNumber+1) )
				{
					// if we're second element in the run add -
					if(run == 1)
						strcat(registerList, "-");
					run++;
				}
				else
				{
					// if run was only 1 reg then need to add a comma
					if(run == 1)
						strcat(registerList, ",");

					// next bit not set or dealing with r15
					// we're at the end of a list
					sprintf(registerNumber, "r%d",regNumber);
					strcat(registerList, registerNumber);

					commaNeeded = true;
					run = 0;
				}
			}
			else
			{
				// last reg so end list
				sprintf(registerNumber, "r%d",regNumber);
				strcat(registerList, registerNumber);
				run = 0;
				commaNeeded = true;
			}
		}
		else
		{
			// not in run
			if( getBit(instruction, regNumber) )
			{
				if( regNumber<15 && getBit(instruction, regNumber+1) )
				{
					// if next bit set, then we're first of run

					// place comma if needed
					if(commaNeeded)
						strcat(registerList, ",");

					sprintf(registerNumber, "r%d",regNumber);
					strcat(registerList, registerNumber);

					run++;
				}
				else
				{
					if(commaNeeded)
						strcat(registerList, ",");

					// next bit not set or we're looking at r15 (end list)
					sprintf(registerNumber, "r%d",regNumber);
					strcat(registerList, registerNumber);

					run = 0;
					commaNeeded = true;
				}
			}
		}
	}

	strcat(registerList, "}");

	return registerList;
}

// ??? absolute address generated is wrong
static char *decodeBranch(uint32 address, uint32 instruction, char *buff)
{
	strcpy(buff, "b");

	strcat(buff, decodeConditionCode(instruction));

	// append spaces as necessary, so mnemonic with suffixes is 8 characters long
	while( strlen(buff) < 8 )
	{
		strcat(buff, " ");
	}

	// get offset from bits 0-23
	// note how shifted left 2 as instructions are aligned to 32 bit word boundaries.
	uint32 offset = getField(instruction, 0,23);
	offset <<= 2;

	// add offset to instruction's address and add 8 for the pipelining effect
	// i.e. PC is 8 bytes ahead of the currently executing instruction
	uint32 branchAddress = address + offset + 8;
	// limit size to 26 bit address
	branchAddress &= (1<<26)-1;

	// output branch address in hex
	char branchAddressHex[12];
	sprintf(branchAddressHex, "0x%08x", branchAddress);
	strcat(buff, branchAddressHex);

	return buff;
}

static char *decodeBranchWithLink(uint32 address, uint32 instruction, char *buff)
{
	strcpy(buff, "bl");

	strcat(buff, decodeConditionCode(instruction));

	// append spaces as necessary, so mnemonic with suffixes is 8 characters long
	while( strlen(buff) < 8 )
	{
		strcat(buff, " ");
	}

	// get offset from bits 0-23
	// note how shifted left 2 as instructions are aligned to 32 bit word boundaries.
	uint32 offset = getField(instruction, 0,23);
	offset <<= 2;

	// add offset to instruction's address and add 8 for the pipelining effect
	// i.e. PC is 8 bytes ahead of the currently executing instruction
	uint32 branchAddress = address + offset + 8;
	// limit size to 26 bit address
	branchAddress &= (1<<26)-1;

	// output branch address in hex
	char branchAddressHex[12];
	sprintf(branchAddressHex, "0x%08x", branchAddress);
	strcat(buff, branchAddressHex);

	return buff;
}

static char *decodeCoProDTPreIndex(uint32 /* address */, uint32 /* instruction */, char * buff)
{
	strcpy(buff, "CO PRO DATA TRANSFER PRE INDEX");
	return buff;
}

static char *decodeCoProDTPostIndex(uint32 /* address */, uint32 /* instruction */, char * buff)
{
	strcpy(buff, "CO PRO DATA TRANSFER POST INDEX");
	return buff;
}

static char *decodeCoProRegTransferOrDataOperation(uint32 /* address */, uint32 /* instruction */, char * buff)
{
	strcpy(buff, "CO PRO REG TRANSFER OR DATA OP");
	return buff;
}

static char *decodeSoftwareInterrupt(uint32 /* address */, uint32 instruction, char *buff)
{
	strcpy(buff, "swi");
	strcat(buff, decodeConditionCode(instruction));

	static const char* const swiList[] = {
		"WriteC", "WriteS", "Write0", "NewLine", "ReadC", "CLI", "Byte",
		"Word", "File", "Args", "BGet", "BPut", "Multiple", "Open",
		"ReadLine", "Control", "GetEnv", "Exit", "SetEnv",
		"IntOn", "IntOff", "CallBack", "EnterOS", "BreakPT",
		"BreakCT", "UnUsed", "SetMEMC", "SetCallB"
	};

	// append spaces as necessary, so mnemonic with suffixes is 8 characters long
	while( strlen(buff) < 8 )
	{
		strcat(buff, " ");
	}

	char swiNumber[12];

	int ins = getField(instruction, 0,23) & 0xffff;

	if (ins <= 27)
	{
		strcpy(swiNumber, swiList[ins]);
	} else if (ins >= 256) {
		ins &= 255;
		if ( (ins >= 32) && (ins <= 126) ) {
			sprintf(swiNumber, "WriteI+%c%c%c", 34, ins, 34);
		} else {
			sprintf(swiNumber, "WriteI+%d", ins);
		}
	} else {
		sprintf(swiNumber, "0x%02x", ins);
	}

	strcat(buff, swiNumber);

	return buff;
}

static char *decodeMultiply(uint32 /* address */, uint32 instruction, char *buff)
{
	bool accumulate = false;

	// decide whether additional accumulate function from bit 21
	if( getBit(instruction, 21) )
	{
		// multiply and accumulate
		strcpy(buff, "mla");
		accumulate = true;
	}
	else
	{
		// standard multiply
		strcpy(buff, "mul");
	}

	strcat(buff, decodeConditionCode(instruction));

	// test S flag (set PSR flags), based on bit 20
	if( getBit(instruction, 20) )
	{
		strcat(buff, "S");
	}

	// append spaces as necessary, so mnemonic with suffixes is 8 characters long
	while( strlen(buff) < 8 )
	{
		strcat(buff, " ");
	}

	char registerNumber[12];

	// add destination register from bits 16-19
	sprintf(registerNumber, "r%d,", getField(instruction, 16,19) );
	strcat(buff, registerNumber);

	// add 1st operand register from bits 0-3
	sprintf(registerNumber, "r%d,", getField(instruction, 0,3) );
	strcat(buff, registerNumber);

	// add 2nd operand register from bits 8-11
	sprintf(registerNumber, "r%d", getField(instruction, 8,11) );
	strcat(buff, registerNumber);

	if(accumulate)
	{
		strcat(buff, ",");

		// add 3rd operand register from bits 12-15
		sprintf(registerNumber, "r%d", getField(instruction, 12,15) );
		strcat(buff, registerNumber);
	}

	return buff;
}

static char *decodeSingleDataSwap(uint32 /* address */, uint32 instruction, char *buff)
{
	// word or byte quantity, byte if bit 22 set
	if(getBit(instruction,22))
	{
		strcpy(buff, "swpb");
	}
	else
	{
		strcpy(buff, "swp");
	}

	strcat(buff, decodeConditionCode(instruction));

	while( strlen(buff) < 8 )
	{
		strcat(buff, " ");
	}

	char registerNumber[12];

	// add destination register from bits 12-15
	sprintf(registerNumber, "r%d,", getField(instruction, 12,15) );
	strcat(buff, registerNumber);

	// add rm from bits 0-3
	sprintf(registerNumber, "r%d,[", getField(instruction, 0, 3) );
	strcat(buff, registerNumber);

	// add rn from bits 16-19
	sprintf(registerNumber, "r%d]", getField(instruction, 16, 19) );
	strcat(buff, registerNumber);

	return buff;
}

static const char *decodeConditionCode(uint32 instruction)
{
	// table of condition code meanings, note that as convention dictates, AL is blank
	static const char* const conditionCodes[16] =
	{
		"eq", "ne", "cs", "cc",
		"mi", "pl", "vs", "vc",
		"hi", "ls", "ge", "lt",
		"gt", "le", "", "nv"
	};

	return conditionCodes[ getField(instruction, 28,31) ];
}
