/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2001  Richard Gellman
Copyright (C) 2009  Mike Wyatt

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

#ifndef UEFSTATE_HEADER
#define UEFSTATE_HEADER

#include <stdint.h>
#include <stdio.h>

enum class UEFStateResult
{
	Success,
	OpenFailed,
	ReadFailed,
	WriteFailed,
	InvalidUEFFile,
	InvalidUEFVersion
};

void UEFWrite64(uint64_t Value, FILE *pFile);
void UEFWrite32(unsigned int Value, FILE *pFile);
void UEFWrite16(unsigned int Value, FILE *pFile);
void UEFWrite8(unsigned int Value, FILE *pFile);
void UEFWriteBuf(const void* pData, size_t Size, FILE *pFile);
void UEFWriteString(const char* String, FILE *pFile);

uint64_t UEFRead64(FILE *pFile);
uint32_t UEFRead32(FILE *pFile);
uint16_t UEFRead16(FILE *pFile);
uint8_t UEFRead8(FILE *pFile);
bool UEFReadBool(FILE *pFile);
void UEFReadBuf(void* pData, size_t Size, FILE *pFile);
void UEFReadString(char* String, unsigned int BufSize, FILE *pFile);

UEFStateResult SaveUEFState(const char *FileName);
UEFStateResult LoadUEFState(const char *FileName);

bool IsUEFSaveState(const char* FileName);

#endif
