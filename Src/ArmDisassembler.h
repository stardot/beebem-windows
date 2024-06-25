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
// ArmDisassembler.h: declarations for the CArmDisassembler class.
// Part of Tarmac
// By David Sharp
// http://www.davidsharp.com
//////////////////////////////////////////////////////////////////////

#include "TarmacGlobals.h"

char *Arm_disassemble(uint32 address, uint32 instruction, char *buff);
const char *decodeRegisterList(uint32 instruction);
char *decodeSingleDataSwap(uint32 address, uint32 instruction, char *buff);
char *decodeMultiply(uint32 address, uint32 instruction, char *buff);
const char *decodeConditionCode(uint32 instruction);
char *decodeSoftwareInterrupt(uint32 address, uint32 instruction, char *buff);
char *decodeCoProRegTransferOrDataOperation(uint32 address, uint32 instruction, char *buff);
char *decodeCoProDTPostIndex(uint32 address, uint32 instruction, char *buff);
char *decodeCoProDTPreIndex(uint32 address, uint32 instruction, char *buff);
char *decodeBranchWithLink(uint32 address, uint32 instruction, char *buff);
char *decodeBranch(uint32 address, uint32 instruction, char *buff);
char *decodeBlockDTPreIndex(uint32 address, uint32 instruction, char *buff);
char *decodeBlockDTPostIndex(uint32 address, uint32 instruction, char *buff);
char *decodeSingleDTRegOffsetPreIndex(uint32 address, uint32 instruction, char *buff);
char *decodeSingleDTRegOffsetPostIndex(uint32 address, uint32 instruction, char *buff);
char *decodeSingleDTImmOffsetPreIndex(uint32 address, uint32 instruction, char *buff);
char *decodeSingleDTImmOffsetPostIndex(uint32 address, uint32 instruction, char *buff);
char *decodeDataProcessing(uint32 address, uint32 instruction, char *buff);
char *decodeSingleDataSwapOrDataProcessing(uint32 address, uint32 instruction, char *buff);
char *decodeMultiplyOrDataProcessing(uint32 address, uint32 instruction, char *buff);
