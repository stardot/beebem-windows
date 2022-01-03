/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2004  Mike Wyatt
Copyright (C) 2004  Rob O'Donnell
Copyright (C) 2009  Steve Pick

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

//
// BeebEm debugger
//

#ifndef DEBUG_HEADER
#define DEBUG_HEADER

#include <windows.h>
#include <string>

#include "via.h"

extern bool DebugEnabled;

enum class DebugType {
	None,
	Video,
	UserVIA,
	SysVIA,
	Tube,
	Serial,
	Econet,
	Teletext,
	RemoteServer,
	Manual,
	Breakpoint,
	BRK,
	RTC
};

//*******************************************************************
// Data structs

struct Label
{
	std::string name;
	int addr;

	Label() : addr(0)
	{
	}

	Label(const std::string& n, int a) : name(n), addr(a)
	{
	}
};

struct Breakpoint
{
	int start;
	int end;
	char name[50 + 1];
};

struct Watch
{
	int start;
	char type;
	int value;
	bool host;
	char name[50 + 1];
};

struct InstInfo
{
	const char* opcode;
	int bytes;
	int flag;
};

struct AddrInfo
{
	int start;
	int end;
	char desc[100];
};

struct MemoryMap
{
	AddrInfo* entries;
	int count;
};

struct DebugCmd
{
	char *name;
	bool (*handler)(char* arguments);
	const char *argdesc;
	const char *help;
};

extern HWND hwndDebug;
bool DebugDisassembler(int addr,
                       int prevAddr,
                       int Accumulator,
                       int XReg,
                       int YReg,
                       int PSR,
                       int StackReg,
                       bool host);
int DebugDisassembleInstruction(int addr, bool host, char *opstr);
int DebugDisassembleInstructionWithCPUStatus(int addr,
                                             bool host,
                                             unsigned char Accumulator,
                                             unsigned char XReg,
                                             unsigned char YReg,
                                             unsigned char StackReg,
                                             unsigned char PSR,
                                             char *opstr);

void DebugOpenDialog(HINSTANCE hinst, HWND hwndMain);
void DebugCloseDialog(void);

void DebugDisplayTrace(DebugType type, bool host, const char *info);
void DebugDisplayInfo(const char *info);
void DebugDisplayInfoF(const char *format, ...);

void DebugVideoState(); // See video.cpp
void DebugUserViaState(); // See uservia.cpp
void DebugSysViaState(); // See sysvia.cpp
void DebugViaState(const char *s, VIAState *v); // See via.cpp

void DebugRunScript(const char *filename);
bool DebugLoadSwiftLabels(const char *filename);

unsigned char DebugReadMem(int addr, bool host);
void DebugBreakExecution(DebugType type);

void DebugInitMemoryMaps();
bool DebugLoadMemoryMap(char* filename, int bank);

#endif
