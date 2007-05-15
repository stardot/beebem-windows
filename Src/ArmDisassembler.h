//////////////////////////////////////////////////////////////////////
// ArmDisassembler.h: declarations for the CArmDisassembler class.
// Part of Tarmac
// By David Sharp
// http://www.davidsharp.com
//////////////////////////////////////////////////////////////////////

#include "TarmacGlobals.h"

char *Arm_disassemble(uint32 address, uint32 instruction, char *buff);
char *decodeRegisterList(uint32 instruction);
char *decodeSingleDataSwap(uint32 address, uint32 instruction, char *buff);
char *decodeMultiply(uint32 address, uint32 instruction, char *buff);
char *decodeConditionCode(uint32 instruction);
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
