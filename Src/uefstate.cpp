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

// UEF Game state code.

#include <stdio.h>
#include "UEFState.h"
#include "6502core.h"
#include "beebmem.h"
#include "video.h"
#include "via.h"
#include "beebwin.h"
#include "Main.h"
#include "beebsound.h"
#include "music5000.h"
#include "disc8271.h"
#include "disc1770.h"
#include "tube.h"
#include "Serial.h"
#include "atodconv.h"

void fput32(unsigned int word32, FILE *fileptr)
{
	fputc(word32 & 255, fileptr);
	fputc((word32 >> 8) & 255, fileptr);
	fputc((word32 >> 16) & 255, fileptr);
	fputc((word32 >> 24) & 255, fileptr);
}

void fput16(unsigned int word16, FILE *fileptr)
{
	fputc(word16 & 255, fileptr);
	fputc((word16 >> 8) & 255, fileptr);
}

unsigned int fget32(FILE *fileptr)
{
	unsigned int Value = fgetc(fileptr);
	Value |= fgetc(fileptr) << 8;
	Value |= fgetc(fileptr) << 16;
	Value |= fgetc(fileptr) << 24;
	return Value;
}

unsigned int fget16(FILE *fileptr)
{
	unsigned int Value = fgetc(fileptr);
	Value |= fgetc(fileptr) << 8;
	return Value;
}

unsigned char fget8(FILE *fileptr)
{
	return (unsigned char)fgetc(fileptr);
}

bool fgetbool(FILE *fileptr)
{
	return fget8(fileptr) != 0;
}

UEFStateResult SaveUEFState(const char *FileName)
{
	FILE *UEFState = fopen(FileName, "wb");

	if (UEFState != nullptr)
	{
		fprintf(UEFState,"UEF File!");
		fputc(0,UEFState); // UEF Header

		const unsigned char UEFMinorVersion = 13;
		const unsigned char UEFMajorVersion = 0;

		fputc(UEFMinorVersion, UEFState);
		fputc(UEFMajorVersion, UEFState);

		mainWin->SaveEmuUEF(UEFState);
		Save6502UEF(UEFState);
		SaveMemUEF(UEFState);
		SaveVideoUEF(UEFState);
		SaveVIAUEF(UEFState);
		SaveSoundUEF(UEFState);
		if (MachineType != Model::Master128 && NativeFDC)
			Save8271UEF(UEFState);
		else
			Save1770UEF(UEFState);
		if (TubeType == Tube::Acorn65C02) {
			SaveTubeUEF(UEFState); // TODO: Save Tube state for all co-pros?
			Save65C02UEF(UEFState);
			Save65C02MemUEF(UEFState);
		}
		SaveSerialUEF(UEFState);
		SaveAtoDUEF(UEFState);
		SaveMusic5000UEF(UEFState);
		fclose(UEFState);

		return UEFStateResult::Success;
	}
	else
	{
		return UEFStateResult::WriteFailed;
	}
}

UEFStateResult LoadUEFState(const char *FileName)
{
	FILE *UEFState = fopen(FileName, "rb");

	if (UEFState != nullptr)
	{
		fseek(UEFState, 0, SEEK_END);
		long FLength = ftell(UEFState);
		fseek(UEFState, 0, SEEK_SET); // Get File length for eof comparison.

		char UEFId[10];
		UEFId[0] = '\0';

		fread(UEFId, sizeof(UEFId), 1, UEFState);

		if (strcmp(UEFId,"UEF File!") != 0)
		{
			fclose(UEFState);
			return UEFStateResult::InvalidUEFFile;
		}

		const int Version = fget16(UEFState);

		if (Version > 13)
		{
			fclose(UEFState);
			return UEFStateResult::InvalidUEFVersion;
		}

		while (ftell(UEFState) < FLength)
		{
			unsigned int Block = fget16(UEFState);
			unsigned int Length = fget32(UEFState);
			long CPos = ftell(UEFState);

			switch (Block)
			{
				case 0x046A:
					mainWin->LoadEmuUEF(UEFState,Version);
					break;

				case 0x0460:
					Load6502UEF(UEFState);
					break;

				case 0x0461:
					LoadRomRegsUEF(UEFState);
					break;

				case 0x0462:
					LoadMainMemUEF(UEFState);
					break;

				case 0x0463:
					LoadShadMemUEF(UEFState);
					break;

				case 0x0464:
					LoadPrivMemUEF(UEFState);
					break;

				case 0x0465:
					LoadFileMemUEF(UEFState);
					break;

				case 0x0466:
					LoadSWRamMemUEF(UEFState);
					break;

				case 0x0467:
					LoadViaUEF(UEFState);
					break;

				case 0x0468:
					LoadVideoUEF(UEFState, Version);
					break;

				case 0x046B:
					LoadSoundUEF(UEFState);
					break;

				case 0x046D:
					LoadIntegraBHiddenMemUEF(UEFState);
					break;

				case 0x046E:
					Load8271UEF(UEFState);
					break;

				case 0x046F:
					Load1770UEF(UEFState,Version);
					break;

				case 0x0470:
					LoadTubeUEF(UEFState);
					break;

				case 0x0471:
					Load65C02UEF(UEFState);
					break;

				case 0x0472:
					Load65C02MemUEF(UEFState);
					break;

				case 0x0473:
					LoadSerialUEF(UEFState);
					break;

				case 0x0474:
					LoadAtoDUEF(UEFState);
					break;

				case 0x0475:
					LoadSWRomMemUEF(UEFState);
					break;

				case 0x0476:
					LoadMusic5000JIMPageRegUEF(UEFState);
					break;

				case 0x0477:
					LoadMusic5000UEF(UEFState, Version);
					break;

				default:
					break;
			}

			fseek(UEFState, CPos + Length, SEEK_SET); // Skip unrecognised blocks (and over any gaps)
		}

		fclose(UEFState);

		return UEFStateResult::Success;
	}
	else
	{
		return UEFStateResult::OpenFailed;
	}
}

bool IsUEFSaveState(const char* FileName)
{
	bool IsSaveState = false;

	FILE *file = fopen(FileName, "rb");

	if (file != nullptr)
	{
		char buf[14];
		fread(buf, sizeof(buf), 1, file);

		if (strcmp(buf, "UEF File!") == 0 && buf[12] == 0x6c && buf[13] == 0x04)
		{
			IsSaveState = true;
		}

		fclose(file);
	}

	return IsSaveState;
}
