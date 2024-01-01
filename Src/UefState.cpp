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

#include <functional>

#include "UEFState.h"
#include "6502core.h"
#include "Arm.h"
#include "AtoDConv.h"
#include "BeebMem.h"
#include "BeebWin.h"
#include "Disc1770.h"
#include "Disc8271.h"
#include "Main.h"
#include "Master512CoPro.h"
#include "Music5000.h"
#include "Serial.h"
#include "Sound.h"
#include "SprowCoPro.h"
#include "Tube.h"
#include "Via.h"
#include "Video.h"
#include "Z80mem.h"
#include "Z80.h"

/*-------------------------------------------------------------------------*/

void fput64(uint64_t Value, FILE *pFile)
{
	fputc(Value & 0xFF, pFile);
	fputc((Value >> 8) & 0xFF, pFile);
	fputc((Value >> 16) & 0xFF, pFile);
	fputc((Value >> 24) & 0xFF, pFile);
	fputc((Value >> 32) & 0xFF, pFile);
	fputc((Value >> 40) & 0xFF, pFile);
	fputc((Value >> 48) & 0xFF, pFile);
	fputc((Value >> 56) & 0xFF, pFile);
}

void fput32(unsigned int Value, FILE *pFile)
{
	fputc(Value & 0xFF, pFile);
	fputc((Value >> 8) & 0xFF, pFile);
	fputc((Value >> 16) & 0xFF, pFile);
	fputc((Value >> 24) & 0xFF, pFile);
}

void fput16(unsigned int Value, FILE *pFile)
{
	fputc(Value & 0xFF, pFile);
	fputc((Value >> 8) & 0xFF, pFile);
}

void fputstring(const char* String, FILE *pFile)
{
	unsigned int Length = (unsigned int)strlen(String);

	fput32(Length, pFile);

	if (Length > 0)
	{
		fwrite(String, 1, Length, pFile);
	}
}

/*-------------------------------------------------------------------------*/

uint64_t fget64(FILE *pFile)
{
	uint64_t Result = 0;
	uint64_t Value;

	Value = fgetc(pFile); Result |= Value;
	Value = fgetc(pFile); Result |= Value << 8;
	Value = fgetc(pFile); Result |= Value << 16;
	Value = fgetc(pFile); Result |= Value << 24;
	Value = fgetc(pFile); Result |= Value << 32;
	Value = fgetc(pFile); Result |= Value << 40;
	Value = fgetc(pFile); Result |= Value << 48;
	Value = fgetc(pFile); Result |= Value << 56;

	return Result;
}

uint32_t fget32(FILE *pFile)
{
	uint32_t Value = fgetc(pFile);
	Value |= fgetc(pFile) << 8;
	Value |= fgetc(pFile) << 16;
	Value |= fgetc(pFile) << 24;

	return Value;
}

uint16_t fget16(FILE *pFile)
{
	uint16_t Value = fgetc(pFile);
	Value |= fgetc(pFile) << 8;

	return Value;
}

uint8_t fget8(FILE *pFile)
{
	return (uint8_t)fgetc(pFile);
}

bool fgetbool(FILE *pFile)
{
	return fget8(pFile) != 0;
}

void fgetstring(char* String, unsigned int BufSize, FILE *pFile)
{
	unsigned int Length = fget32(pFile);

	unsigned int i;

	for (i = 0; i < Length && i < BufSize - 1; i++)
	{
		String[i] = fget8(pFile);
	}

	String[i] = '\0';

	// Discard remaining characters, if stored string is longer than
	// the given buffer.

	for (; i < Length; i++)
	{
		fget8(pFile);
	}
}

/*-------------------------------------------------------------------------*/

// Writes a chunk to the UEF file and updates the chunk length

template<typename SaveStateFunctionType>
void SaveState(SaveStateFunctionType SaveStateFunction, int ChunkID, FILE *SUEF)
{
	fput16(ChunkID, SUEF); // UEF Chunk ID
	fput32(0, SUEF); // Chunk length (updated after writing data)
	long StartPos = ftell(SUEF);

	SaveStateFunction(SUEF);

	long EndPos = ftell(SUEF);
	long Length = EndPos - StartPos;
	fseek(SUEF, StartPos - 4, SEEK_SET);
	fput32(Length, SUEF); // Size
	fseek(SUEF, EndPos, SEEK_SET);
}

/*-------------------------------------------------------------------------*/

UEFStateResult SaveUEFState(const char *FileName)
{
	using std::placeholders::_1;

	FILE *UEFState = fopen(FileName, "wb");

	if (UEFState != nullptr)
	{
		fprintf(UEFState,"UEF File!");
		fputc(0,UEFState); // UEF Header

		const unsigned char UEFMinorVersion = 14;
		const unsigned char UEFMajorVersion = 0;

		fputc(UEFMinorVersion, UEFState);
		fputc(UEFMajorVersion, UEFState);

		SaveState(std::bind(&BeebWin::SaveBeebEmID, mainWin, _1), 0x046C, UEFState);
		SaveState(std::bind(&BeebWin::SaveEmuUEF, mainWin, _1), 0x046A, UEFState);
		SaveState(Save6502UEF, 0x0460, UEFState);
		SaveMemUEF(UEFState);
		SaveState(SaveVideoUEF, 0x0468, UEFState);
		SaveVIAUEF(UEFState);
		SaveState(SaveSoundUEF, 0x046B, UEFState);

		if (MachineType != Model::Master128 && NativeFDC)
		{
			SaveState(Save8271UEF, 0x046E, UEFState);
		}
		else
		{
			SaveState(Save1770UEF, 0x046F, UEFState);
		}

		if (TubeType != Tube::None)
		{
			SaveState(SaveTubeUEF, 0x0470, UEFState);
		}

		switch (TubeType)
		{
			case Tube::Acorn65C02:
				SaveState(Save65C02UEF, 0x0471, UEFState);
				SaveState(Save65C02MemUEF, 0x0472, UEFState);
				break;

			case Tube::Master512CoPro:
				SaveState(std::bind(&Master512CoPro::SaveState, &master512CoPro, _1), 0x047B, UEFState);
				break;

			case Tube::AcornZ80:
				SaveState(SaveZ80UEF, 0x0478, UEFState);
				break;

			case Tube::TorchZ80:
				SaveState(SaveZ80UEF, 0x0478, UEFState);
				break;

			case Tube::AcornArm:
				SaveState(std::bind(&CArm::SaveState, arm, _1), 0x0479, UEFState);
				break;

			case Tube::SprowArm:
				SaveState(std::bind(&CSprowCoPro::SaveState, sprow, _1), 0x047A, UEFState);
				break;
		}

		if (SerialGetTapeState() != SerialTapeState::NoTape)
		{
			SaveState(SaveSerialUEF, 0x0473, UEFState);
		}

		SaveState(SaveAtoDUEF, 0x0474, UEFState);

		if (Music5000Enabled)
		{
			SaveState(SaveMusic5000UEF, 0x0477, UEFState);
		}

		fclose(UEFState);

		return UEFStateResult::Success;
	}
	else
	{
		return UEFStateResult::WriteFailed;
	}
}

/*-------------------------------------------------------------------------*/

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

		if (Version > 14)
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
					mainWin->LoadEmuUEF(UEFState, Version);
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
					Load8271UEF(UEFState, Version);
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
					LoadSerialUEF(UEFState, Version);
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

				case 0x0478:
					LoadZ80UEF(UEFState);
					break;

				case 0x0479:
					arm->LoadState(UEFState);
					break;

				case 0x047A:
					sprow->LoadState(UEFState);
					break;

				case 0x047B:
					master512CoPro.LoadState(UEFState);
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

/*-------------------------------------------------------------------------*/

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
