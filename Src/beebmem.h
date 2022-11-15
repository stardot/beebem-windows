/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 1994  David Alan Gilbert
Copyright (C) 1997  Mike Wyatt
Copyright (C) 2001  Richard Gellman
Copyright (C) 2004  Ken Lowe
Copyright (C) 2004  Rob O'Donnell

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

/* Beebemulator - memory subsystem - David Alan Gilbert 16/10/94 */

#ifndef BEEBMEM_HEADER
#define BEEBMEM_HEADER

#include <stdio.h>
#include <stdlib.h>

typedef char ROMConfigFile[4][17][_MAX_PATH];
static const char *BANK_EMPTY = "EMPTY";
static const char *BANK_RAM = "RAM";
static const char *ROM_WRITABLE = ":RAM";

enum class BankType
{
	Rom,
	Ram,
	Empty
};

extern bool RomWritable[16]; // Allow writing to banks on an individual basis
extern BankType RomBankType[16]; // Identifies what is in each bank

extern unsigned char WholeRam[65536];
extern unsigned char Roms[16][16384];
extern unsigned char ROMSEL;
extern unsigned char PagedRomReg;

/* Master 128 Specific Stuff */
extern unsigned char FSRam[8192]; // 8K Filing System RAM
extern unsigned char PrivateRAM[4096]; // 4K Private RAM (VDU Use mainly)
extern unsigned char CMOSRAM[64]; // 50 Bytes CMOS RAM
extern unsigned char ShadowRAM[32768]; // 20K Shadow RAM
extern unsigned char ACCCON; // ACCess CONtrol register
extern bool MemSel, PrvEn, ShEn, Prvs1, Prvs4, Prvs8;

struct CMOSType {
	bool Enabled;
	// unsigned char ChipSelect;
	unsigned char Address;
	// unsigned char StrobedData;
	bool DataStrobe;
	bool Op;
};

enum RomFlags
{
	RomService=128,
	RomLanguage=64,
	RomRelocate=32,
	RomSoftKey=16, // Electron only
	RomFlag3=8,
	RomFlag2=4,
	RomFlag1=2,
	RomFlag0=1,
};

constexpr int MAX_ROMINFO_LENGTH = 255;

struct RomInfo {
	int Slot;
	int LanguageAddr;
	int ServiceAddr;
	int WorkspaceAddr;
	RomFlags Flags;
	int Version;
	char Title[MAX_ROMINFO_LENGTH + 1];
	char VersionStr[MAX_ROMINFO_LENGTH + 1];
	char Copyright[MAX_ROMINFO_LENGTH + 1];
	int RelocationAddr;
};

extern struct CMOSType CMOS;
extern bool Sh_Display;
/* End of Master 128 Specific Stuff, note initilised anyway regardless of Model Type in use */

extern ROMConfigFile RomConfig;
extern char RomPath[_MAX_PATH];
extern char RomFile[_MAX_PATH];

extern char HardDrivePath[_MAX_PATH]; // JGH

unsigned char BeebReadMem(int Address);
void BeebWriteMem(int Address, unsigned char Value);
#define BEEBWRITEMEM_DIRECT(Address, Value) WholeRam[Address] = Value
const unsigned char *BeebMemPtrWithWrap(int Address, int Length);
const unsigned char *BeebMemPtrWithWrapMode7(int Address, int Length);
bool ReadROMFile(const char *filename, ROMConfigFile RomConfig);
void BeebReadRoms(void);
void BeebMemInit(bool LoadRoms, bool SkipIntegraBConfig);
void RTCReset(void);

/* used by debugger */
bool ReadRomInfo(int bank, RomInfo* info);
/* Used to show the Rom titles from the options menu */
char *ReadRomTitle(int bank, char *Title, int BufSize);

void SaveMemUEF(FILE *SUEF);
extern int EFDCAddr; // 1770 FDC location
extern int EDCAddr; // Drive control location
extern bool NativeFDC; // see beebmem.cpp for description
void LoadRomRegsUEF(FILE *SUEF);
void LoadMainMemUEF(FILE *SUEF);
void LoadShadMemUEF(FILE *SUEF);
void LoadPrivMemUEF(FILE *SUEF);
void LoadFileMemUEF(FILE *SUEF);
void LoadSWRamMemUEF(FILE *SUEF);
void LoadSWRomMemUEF(FILE *SUEF);
void LoadIntegraBHiddenMemUEF(FILE *SUEF);
//void LoadJIMPageRegUEF(FILE *SUEF);
void DebugMemoryState();

#endif
