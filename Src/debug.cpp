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
// Mike Wyatt - Nov 2004
// Econet added Rob O'Donnell 2004-12-28.

#include <windows.h>
#include <ctype.h>
#include <string.h>
#include <stdarg.h>
#include "main.h"
#include "beebmem.h"
#include "tube.h"
#include "beebemrc.h"
#include "6502core.h"
#include "tube.h"
#include "debug.h"
#include "z80mem.h"
#include "z80.h"

#define MAX_LINES 4096          // Max lines in info window
#define LINES_IN_INFO 28        // Visible lines in info window
#define MAX_COMMAND_LEN 200     // Max debug command length
#define MAX_BPS 50				// Max num of breakpoints/watches
#define MAX_LABELS 500			// Max num labels.
#define MAX_HISTORY 20			// Number of commands in the command history.
// Where control goes
#define NORM 1
#define JUMP 2
#define FORK 4
#define STOP 8
#define CTLMASK (NORM|JUMP|FORK|STOP)

// Instruction format
#define IMM  0x20
#define ABS  0x40
#define ACC  0x80
#define IMP  0x100
#define INX  0x200
#define INY  0x400
#define ZPX  0x800
#define ABX  0x1000
#define ABY  0x2000
#define REL  0x4000
#define IND  0x8000
#define ZPY  0x10000
#define ZPG  0x20000
#define ILL  0x40000

#define STR(x) #x

#define ADRMASK (IMM|ABS|ACC|IMP|INX|INY|ZPX|ABX|ABY|REL|IND|ZPY|ZPG|ILL)

#define MAX_BUFFER 65536

int DebugEnabled = false;        // Debug dialog visible
static int DebugSource = DEBUG_NONE; // Debugging active?
static int LinesDisplayed = 0;  // Lines in info window
static int InstCount = 0;       // Instructions to execute before breaking
static int DumpAddress = 0;     // Next address for memory dump command
static int DisAddress = 0;      // Next address for disassemble command
static int BPCount = 0;         // Num of breakpoints
static int WCount = 0;          // Num of watches
static int LastBreakAddr = 0;	// Address of last break
static int DebugInfoWidth = 0;	// Width of debug info window
static bool BPSOn = true;
static bool BRKOn = false;
static bool DebugOS = false;
static bool LastAddrInOS = false;
static bool LastAddrInBIOS = false;
static bool DebugROM = false;
static bool LastAddrInROM = false;
static bool DebugHost = true;
static bool DebugParasite = false;
static bool WatchDecimal = false;
static bool WatchRefresh = false;
static bool WatchBigEndian = false;
HWND hwndDebug;
static HWND hwndInfo;
static HWND hwndBP;
static HWND hwndW;
static HACCEL haccelDebug;
static Label Labels[MAX_LABELS];
static int LabelCount = 0;
static Breakpoint Breakpoints[MAX_BPS];
static Watch Watches[MAX_BPS];
static unsigned char buffer[MAX_BUFFER];
static MemoryMap MemoryMaps[17];
BOOL CALLBACK DebugDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam);
extern HWND hCurrentDialog;
extern HACCEL hCurrentAccelTable;



char debugHistory[MAX_HISTORY][300];
int debugHistoryIndex = 0;

// Debugger commands go here. Format is COMMAND, HANDLER, ARGSPEC, HELPSTRING
// Aliases are supported, put these below the command they reference and leave argspec/help
// empty.
DebugCmd DebugCmdTable[] = {
	{ "bp",		DebugCmdToggleBreak, "start[-end] [name]", "Sets/Clears a breakpoint or break range." },
	{ "b",		DebugCmdToggleBreak, "", ""}, // Alias of "bp"
	{ "breakpoint", DebugCmdToggleBreak, "", ""}, // Alias of "bp"
	{ "labels",	DebugCmdLabels, "[filename]", "Loads labels from VICE file, or display known labels." },
	{ "l",		DebugCmdLabels,"",""}, // Alias of "labels"
	{ "help",	DebugCmdHelp, "[command/addr]", "Displays help for the specified command or address." },
	{ "?",		DebugCmdHelp,"",""}, // Alias of "help"
	{ "q",		DebugCmdHelp,"",""}, // Alias of "help"
	{ "break",	DebugCmdBreakContinue, "", "Break/Continue." },
	{ ".",		DebugCmdBreakContinue,"",""}, // Alias of "break"
	{ "set",	DebugCmdSet, "host/parasite/rom/os/endian/breakpoint/decimal/brk on/off", "Turns various UI checkboxes on or off." },
	{ "next",	DebugCmdNext, "[count]", "Execute the specified number instructions, default 1." },
	{ "n",		DebugCmdNext,"",""}, // Alias of "next"
	{ "peek",	DebugCmdPeek, "[p] [start] [count]", "Dumps memory to console." },
	{ "m",		DebugCmdPeek,"",""}, // Alias of "peek"
	{ "code",	DebugCmdCode, "[p] [start] [count]", "Dissassembles specified range." },
	{ "d",		DebugCmdCode,"",""}, // Alias of "code"
	{ "watch",	DebugCmdWatch, "[p] addr [b/w/d] [name]", "Sets/Clears a byte/word/dword watch at addr." },
	{ "e",		DebugCmdWatch,"",""}, // Alias of "watch"
	{ "state",	DebugCmdState, "v/u/s/t/m/r", "Displays state of Video/UserVIA/SysVIA/Tube/Memory/Roms." },
	{ "s",		DebugCmdState,"",""}, // Alias of "state"
	{ "save",	DebugCmdSave, "[count] file", "Writes console lines to file." },
	{ "w",		DebugCmdSave,"",""}, // Alias of "save"
	{ "poke",	DebugCmdPoke, "[p] start byte [byte...]", "Write bytes to memory." },
	{ "c",		DebugCmdPoke,"",""}, // Alias of "poke"
	{ "goto",	DebugCmdGoto, "[p] addr", "Jump to address." },
	{ "g",		DebugCmdGoto,"",""}, // Alias of "goto"
	{ "file",	DebugCmdFile, "r/w addr [count] filename", "Read/Write memory at address from/to file." },
	{ "f",		DebugCmdFile,"",""}, // Alias of "file"
	{ "echo",	DebugCmdEcho, "string", "Write string to console." },
	{ "!",	DebugCmdEcho, "","" }, // Alias of "echo"
	{ "script",	DebugCmdScript, "[filename]", "Executes a debugger script." }
};

InstInfo optable[256] =
{
	/* 00 */	{ "BRK",  1, IMP|STOP, 0, },
	/* 01 */	{ "ORA",  2, INX|NORM, 0, },
	/* 00 */	{ "?02",  1, ILL|NORM, 0, },
	/* 00 */	{ "?03",  1, ILL|NORM, 0, },
	/* 04 */	{ "TSB",  2, ZPG|NORM, 1, },
	/* 05 */	{ "ORA",  2, ZPG|NORM, 0, },
	/* 06 */	{ "ASL",  2, ZPG|NORM, 0, },
	/* 00 */	{ "?07",  1, ILL|NORM, 0, },
	/* 08 */	{ "PHP",  1, IMP|NORM, 0, },
	/* 09 */	{ "ORA",  2, IMM|NORM, 0, },
	/* 0a */	{ "ASL",  1, ACC|NORM, 0, },
	/* 00 */	{ "?0B",  1, ILL|NORM, 0, },
	/* 0c */	{ "TSB",  3, ABS|NORM, 1, },
	/* 0d */	{ "ORA",  3, ABS|NORM, 0, },
	/* 0e */	{ "ASL",  3, ABS|NORM, 0, },
	/* 00 */	{ "?0F",  1, ILL|NORM, 0, },
	/* 10 */	{ "BPL",  2, REL|FORK, 0, },
	/* 11 */	{ "ORA",  2, INY|NORM, 0, },
	/* 12 */	{ "ORA",  2, IND|NORM, 1, },
	/* 00 */	{ "?13",  1, ILL|NORM, 0, },
	/* 14 */	{ "TRB",  2, ZPG|NORM, 1, },
	/* 15 */	{ "ORA",  2, ZPX|NORM, 0, },
	/* 16 */	{ "ASL",  2, ZPX|NORM, 0, },
	/* 00 */	{ "?17",  1, ILL|NORM, 0, },
	/* 18 */	{ "CLC",  1, IMP|NORM, 0, },
	/* 19 */	{ "ORA",  3, ABY|NORM, 0, },
	/* 1a */	{ "INC",  1, ACC|NORM, 1, },
	/* 00 */	{ "?1B",  1, ILL|NORM, 0, },
	/* 1c */	{ "TRB",  3, ABS|NORM, 1, },
	/* 1d */	{ "ORA",  3, ABX|NORM, 0, },
	/* 1e */	{ "ASL",  3, ABX|NORM, 0, },
	/* 00 */	{ "?1F",  1, ILL|NORM, 0, },
	/* 20 */	{ "JSR",  3, ABS|FORK, 0, },
	/* 21 */	{ "AND",  2, INX|NORM, 0, },
	/* 00 */	{ "?22",  1, ILL|NORM, 0, },
	/* 00 */	{ "?23",  1, ILL|NORM, 0, },
	/* 24 */	{ "BIT",  2, ZPG|NORM, 0, },
	/* 25 */	{ "AND",  2, ZPG|NORM, 0, },
	/* 26 */	{ "ROL",  2, ZPG|NORM, 0, },
	/* 00 */	{ "?27",  1, ILL|NORM, 0, },
	/* 28 */	{ "PLP",  1, IMP|NORM, 0, },
	/* 29 */	{ "AND",  2, IMM|NORM, 0, },
	/* 2a */	{ "ROL",  1, ACC|NORM, 0, },
	/* 00 */	{ "?2B",  1, ILL|NORM, 0, },
	/* 2c */	{ "BIT",  3, ABS|NORM, 0, },
	/* 2d */	{ "AND",  3, ABS|NORM, 0, },
	/* 2e */	{ "ROL",  3, ABS|NORM, 0, },
	/* 00 */	{ "?2F",  1, ILL|NORM, 0, },
	/* 30 */	{ "BMI",  2, REL|FORK, 0, },
	/* 31 */	{ "AND",  2, INY|NORM, 0, },
	/* 32 */	{ "AND",  2, IND|NORM, 1, },
	/* 00 */	{ "?33",  1, ILL|NORM, 0, },
	/* 34 */	{ "BIT",  2, ZPX|NORM, 1, },
	/* 35 */	{ "AND",  2, ZPX|NORM, 0, },
	/* 36 */	{ "ROL",  2, ZPX|NORM, 0, },
	/* 00 */	{ "?37",  1, ILL|NORM, 0, },
	/* 38 */	{ "SEC",  1, IMP|NORM, 0, },
	/* 39 */	{ "AND",  3, ABY|NORM, 0, },
	/* 3a */	{ "DEC",  1, ACC|NORM, 1, },
	/* 00 */	{ "?3B",  1, ILL|NORM, 0, },
	/* 3c */	{ "BIT",  3, ABX|NORM, 1, },
	/* 3d */	{ "AND",  3, ABX|NORM, 0, },
	/* 3e */	{ "ROL",  3, ABX|NORM, 0, },
	/* 00 */	{ "?3F",  1, ILL|NORM, 0, },
	/* 40 */	{ "RTI",  1, IMP|STOP, 0, },
	/* 41 */	{ "EOR",  2, INX|NORM, 0, },
	/* 00 */	{ "?42",  1, ILL|NORM, 0, },
	/* 00 */	{ "?43",  1, ILL|NORM, 0, },
	/* 00 */	{ "?44",  1, ILL|NORM, 0, },
	/* 45 */	{ "EOR",  2, ZPG|NORM, 0, },
	/* 46 */	{ "LSR",  2, ZPG|NORM, 0, },
	/* 00 */	{ "?47",  1, ILL|NORM, 0, },
	/* 48 */	{ "PHA",  1, IMP|NORM, 0, },
	/* 49 */	{ "EOR",  2, IMM|NORM, 0, },
	/* 4a */	{ "LSR",  1, ACC|NORM, 0, },
	/* 00 */	{ "?4B",  1, ILL|NORM, 0, },
	/* 4c */	{ "JMP",  3, ABS|JUMP, 0, },
	/* 4d */	{ "EOR",  3, ABS|NORM, 0, },
	/* 4e */	{ "LSR",  3, ABS|NORM, 0, },
	/* 00 */	{ "?4F",  1, ILL|NORM, 0, },
	/* 50 */	{ "BVC",  2, REL|FORK, 0, },
	/* 51 */	{ "EOR",  2, INY|NORM, 0, },
	/* 52 */	{ "EOR",  2, IND|NORM, 1, },
	/* 00 */	{ "?53",  1, ILL|NORM, 0, },
	/* 00 */	{ "?54",  1, ILL|NORM, 0, },
	/* 55 */	{ "EOR",  2, ZPX|NORM, 0, },
	/* 56 */	{ "LSR",  2, ZPX|NORM, 0, },
	/* 00 */	{ "?57",  1, ILL|NORM, 0, },
	/* 58 */	{ "CLI",  1, IMP|NORM, 0, },
	/* 59 */	{ "EOR",  3, ABY|NORM, 0, },
	/* 5a */	{ "PHY",  1, IMP|NORM, 1, },
	/* 00 */	{ "?5B",  1, ILL|NORM, 0, },
	/* 00 */	{ "?5C",  1, ILL|NORM, 0, },
	/* 5d */	{ "EOR",  3, ABX|NORM, 0, },
	/* 5e */	{ "LSR",  3, ABX|NORM, 0, },
	/* 00 */	{ "?5D",  1, ILL|NORM, 0, },
	/* 60 */	{ "RTS",  1, IMP|STOP, 0, },
	/* 61 */	{ "ADC",  2, INX|NORM, 0, },
	/* 00 */	{ "?62",  1, ILL|NORM, 0, },
	/* 00 */	{ "?63",  1, ILL|NORM, 0, },
	/* 64 */	{ "STZ",  2, ZPG|NORM, 1, },
	/* 65 */	{ "ADC",  2, ZPG|NORM, 0, },
	/* 66 */	{ "ROR",  2, ZPG|NORM, 0, },
	/* 00 */	{ "?67",  1, ILL|NORM, 0, },
	/* 68 */	{ "PLA",  1, IMP|NORM, 0, },
	/* 69 */	{ "ADC",  2, IMM|NORM, 0, },
	/* 6a */	{ "ROR",  1, ACC|NORM, 0, },
	/* 00 */	{ "?6B",  1, ILL|NORM, 0, },
	/* 6c */	{ "JMP",  3, IND|STOP, 0, },
	/* 6d */	{ "ADC",  3, ABS|NORM, 0, },
	/* 6e */	{ "ROR",  3, ABS|NORM, 0, },
	/* 00 */	{ "?6F",  1, ILL|NORM, 0, },
	/* 70 */	{ "BVS",  2, REL|FORK, 0, },
	/* 71 */	{ "ADC",  2, INY|NORM, 0, },
	/* 72 */	{ "ADC",  2, IND|NORM, 1, },
	/* 00 */	{ "?73",  1, ILL|NORM, 0, },
	/* 74 */	{ "STZ",  2, ZPX|NORM, 1, },
	/* 75 */	{ "ADC",  2, ZPX|NORM, 0, },
	/* 76 */	{ "ROR",  2, ZPX|NORM, 0, },
	/* 00 */	{ "?77",  1, ILL|NORM, 0, },
	/* 78 */	{ "SEI",  1, IMP|NORM, 0, },
	/* 79 */	{ "ADC",  3, ABY|NORM, 0, },
	/* 7a */	{ "PLY",  1, IMP|NORM, 1, },
	/* 00 */	{ "?7B",  1, ILL|NORM, 0, },
	/* 7c */	{ "JMP",  3, INX|NORM, 1, },
	/* 7d */	{ "ADC",  3, ABX|NORM, 0, },
	/* 7e */	{ "ROR",  3, ABX|NORM, 0, },
	/* 00 */	{ "?7F",  1, ILL|NORM, 0, },
	/* 80 */	{ "BRA",  2, REL|FORK, 1, },
	/* 81 */	{ "STA",  2, INX|NORM, 0, },
	/* 00 */	{ "?82",  1, ILL|NORM, 0, },
	/* 00 */	{ "?83",  1, ILL|NORM, 0, },
	/* 84 */	{ "STY",  2, ZPG|NORM, 0, },
	/* 85 */	{ "STA",  2, ZPG|NORM, 0, },
	/* 86 */	{ "STX",  2, ZPG|NORM, 0, },
	/* 00 */	{ "?87",  1, ILL|NORM, 0, },
	/* 88 */	{ "DEY",  1, IMP|NORM, 0, },
	/* 00 */	{ "?89",  1, ILL|NORM, 0, },
	/* 8a */	{ "TXA",  1, IMP|NORM, 0, },
	/* 00 */	{ "?8B",  1, ILL|NORM, 0, },
	/* 8c */	{ "STY",  3, ABS|NORM, 0, },
	/* 8d */	{ "STA",  3, ABS|NORM, 0, },
	/* 8e */	{ "STX",  3, ABS|NORM, 0, },
	/* 00 */	{ "?8F",  1, ILL|NORM, 0, },
	/* 90 */	{ "BCC",  2, REL|FORK, 0, },
	/* 91 */	{ "STA",  2, INY|NORM, 0, },
	/* 92 */	{ "STA",  2, IND|NORM, 1, },
	/* 00 */	{ "?93",  1, ILL|NORM, 0, },
	/* 94 */	{ "STY",  2, ZPX|NORM, 0, },
	/* 95 */	{ "STA",  2, ZPX|NORM, 0, },
	/* 96 */	{ "STX",  2, ZPY|NORM, 0, },
	/* 00 */	{ "?97",  1, ILL|NORM, 0, },
	/* 98 */	{ "TYA",  1, IMP|NORM, 0, },
	/* 99 */	{ "STA",  3, ABY|NORM, 0, },
	/* 9a */	{ "TXS",  1, IMP|NORM, 0, },
	/* 00 */	{ "?9B",  1, ILL|NORM, 0, },
	/* 9c */	{ "STZ",  3, ABS|NORM, 1, },
	/* 9d */	{ "STA",  3, ABX|NORM, 0, },
	/* 9e */	{ "STZ",  3, ABX|NORM, 1, },
	/* 00 */	{ "?9F",  1, ILL|NORM, 0, },
	/* a0 */	{ "LDY",  2, IMM|NORM, 0, },
	/* a1 */	{ "LDA",  2, INX|NORM, 0, },
	/* a2 */	{ "LDX",  2, IMM|NORM, 0, },
	/* 00 */	{ "?A3",  1, ILL|NORM, 0, },
	/* a4 */	{ "LDY",  2, ZPG|NORM, 0, },
	/* a5 */	{ "LDA",  2, ZPG|NORM, 0, },
	/* a6 */	{ "LDX",  2, ZPG|NORM, 0, },
	/* 00 */	{ "?A7",  1, ILL|NORM, 0, },
	/* a8 */	{ "TAY",  1, IMP|NORM, 0, },
	/* a9 */	{ "LDA",  2, IMM|NORM, 0, },
	/* aa */	{ "TAX",  1, IMP|NORM, 0, },
	/* 00 */	{ "?AB",  1, ILL|NORM, 0, },
	/* ac */	{ "LDY",  3, ABS|NORM, 0, },
	/* ad */	{ "LDA",  3, ABS|NORM, 0, },
	/* ae */	{ "LDX",  3, ABS|NORM, 0, },
	/* 00 */	{ "?AF",  1, ILL|NORM, 0, },
	/* b0 */	{ "BCS",  2, REL|FORK, 0, },
	/* b1 */	{ "LDA",  2, INY|NORM, 0, },
	/* b2 */	{ "LDA",  2, IND|NORM, 1, },
	/* 00 */	{ "?B3",  1, ILL|NORM, 0, },
	/* b4 */	{ "LDY",  2, ZPX|NORM, 0, },
	/* b5 */	{ "LDA",  2, ZPX|NORM, 0, },
	/* b6 */	{ "LDX",  2, ZPY|NORM, 0, },
	/* 00 */	{ "?B7",  1, ILL|NORM, 0, },
	/* b8 */	{ "CLV",  1, IMP|NORM, 0, },
	/* b9 */	{ "LDA",  3, ABY|NORM, 0, },
	/* ba */	{ "TSX",  1, IMP|NORM, 0, },
	/* 00 */	{ "?BB",  1, ILL|NORM, 0, },
	/* bc */	{ "LDY",  3, ABX|NORM, 0, },
	/* bd */	{ "LDA",  3, ABX|NORM, 0, },
	/* be */	{ "LDX",  3, ABY|NORM, 0, },
	/* 00 */	{ "?BF",  1, ILL|NORM, 0, },
	/* c0 */	{ "CPY",  2, IMM|NORM, 0, },
	/* c1 */	{ "CMP",  2, INX|NORM, 0, },
	/* 00 */	{ "?C2",  1, ILL|NORM, 0, },
	/* 00 */	{ "?C3",  1, ILL|NORM, 0, },
	/* c4 */	{ "CPY",  2, ZPG|NORM, 0, },
	/* c5 */	{ "CMP",  2, ZPG|NORM, 0, },
	/* c6 */	{ "DEC",  2, ZPG|NORM, 0, },
	/* 00 */	{ "?C7",  1, ILL|NORM, 0, },
	/* c8 */	{ "INY",  1, IMP|NORM, 0, },
	/* c9 */	{ "CMP",  2, IMM|NORM, 0, },
	/* ca */	{ "DEX",  1, IMP|NORM, 0, },
	/* 00 */	{ "?CB",  1, ILL|NORM, 0, },
	/* cc */	{ "CPY",  3, ABS|NORM, 0, },
	/* cd */	{ "CMP",  3, ABS|NORM, 0, },
	/* ce */	{ "DEC",  3, ABS|NORM, 0, },
	/* 00 */	{ "?CF",  1, ILL|NORM, 0, },
	/* d0 */	{ "BNE",  2, REL|FORK, 0, },
	/* d1 */	{ "CMP",  2, INY|NORM, 0, },
	/* d2 */	{ "CMP",  2, IND|NORM, 1, },
	/* 00 */	{ "?D3",  1, ILL|NORM, 0, },
	/* 00 */	{ "?D4",  1, ILL|NORM, 0, },
	/* d5 */	{ "CMP",  2, ZPX|NORM, 0, },
	/* d6 */	{ "DEC",  2, ZPX|NORM, 0, },
	/* 00 */	{ "?D7",  1, ILL|NORM, 0, },
	/* d8 */	{ "CLD",  1, IMP|NORM, 0, },
	/* d9 */	{ "CMP",  3, ABY|NORM, 0, },
	/* da */	{ "PHX",  1, IMP|NORM, 1, },
	/* 00 */	{ "?DB",  1, ILL|NORM, 0, },
	/* 00 */	{ "?DC",  1, ILL|NORM, 0, },
	/* dd */	{ "CMP",  3, ABX|NORM, 0, },
	/* de */	{ "DEC",  3, ABX|NORM, 0, },
	/* 00 */	{ "?DF",  1, ILL|NORM, 0, },
	/* e0 */	{ "CPX",  2, IMM|NORM, 0, },
	/* e1 */	{ "SBC",  2, INX|NORM, 0, },
	/* 00 */	{ "?E2",  1, ILL|NORM, 0, },
	/* 00 */	{ "?E3",  1, ILL|NORM, 0, },
	/* e4 */	{ "CPX",  2, ZPG|NORM, 0, },
	/* e5 */	{ "SBC",  2, ZPG|NORM, 0, },
	/* e6 */	{ "INC",  2, ZPG|NORM, 0, },
	/* 00 */	{ "?E7",  1, ILL|NORM, 0, },
	/* e8 */	{ "INX",  1, IMP|NORM, 0, },
	/* e9 */	{ "SBC",  2, IMM|NORM, 0, },
	/* ea */	{ "NOP",  1, IMP|NORM, 0, },
	/* 00 */	{ "?EB",  1, ILL|NORM, 0, },
	/* ec */	{ "CPX",  3, ABS|NORM, 0, },
	/* ed */	{ "SBC",  3, ABS|NORM, 0, },
	/* ee */	{ "INC",  3, ABS|NORM, 0, },
	/* 00 */	{ "?EF",  1, ILL|NORM, 0, },
	/* f0 */	{ "BEQ",  2, REL|FORK, 0, },
	/* f1 */	{ "SBC",  2, INY|NORM, 0, },
	/* f2 */	{ "SBC",  2, IND|NORM, 1, },
	/* 00 */	{ "?F3",  1, ILL|NORM, 0, },
	/* 00 */	{ "?F4",  1, ILL|NORM, 0, },
	/* f5 */	{ "SBC",  2, ZPX|NORM, 0, },
	/* f6 */	{ "INC",  2, ZPX|NORM, 0, },
	/* 00 */	{ "?F7",  1, ILL|NORM, 0, },
	/* f8 */	{ "SED",  1, IMP|NORM, 0, },
	/* f9 */	{ "SBC",  3, ABY|NORM, 0, },
	/* fa */	{ "PLX",  1, IMP|NORM, 1, },
	/* 00 */	{ "?FB",  1, ILL|NORM, 0, },
	/* 00 */	{ "?FC",  1, ILL|NORM, 0, },
	/* fd */	{ "SBC",  3, ABX|NORM, 0, },
	/* fe */	{ "INC",  3, ABX|NORM, 0, },
	/* 00 */	{ "?FF",  1, ILL|NORM, 0, }
};

void DebugOpenDialog(HINSTANCE hinst, HWND hwndMain)
{
	DebugEnabled = TRUE;
	if (!IsWindow(hwndDebug)) 
	{ 
		haccelDebug = LoadAccelerators(hinst, MAKEINTRESOURCE(IDR_ACCELERATORS));
		hwndDebug = CreateDialog(hinst, MAKEINTRESOURCE(IDD_DEBUG),
								 NULL, (DLGPROC)DebugDlgProc);
		memset(debugHistory,'\0',sizeof(debugHistory));
		hCurrentDialog = hwndDebug;
		hCurrentAccelTable = haccelDebug;
		ShowWindow(hwndDebug, SW_SHOW);

		hwndInfo = GetDlgItem(hwndDebug, IDC_DEBUGINFO);
		SendMessage(hwndInfo, WM_SETFONT, (WPARAM)GetStockObject(ANSI_FIXED_FONT),
					(LPARAM)MAKELPARAM(FALSE,0));

		hwndBP = GetDlgItem(hwndDebug, IDC_DEBUGBREAKPOINTS);
		SendMessage(hwndBP, WM_SETFONT, (WPARAM)GetStockObject(ANSI_FIXED_FONT),
					(LPARAM)MAKELPARAM(FALSE,0));

		hwndW = GetDlgItem(hwndDebug, IDC_DEBUGWATCHES);
		SendMessage(hwndW, WM_SETFONT, (WPARAM)GetStockObject(ANSI_FIXED_FONT),
					(LPARAM)MAKELPARAM(FALSE,0));

		SendDlgItemMessage(hwndDebug, IDC_DEBUGBPS, BM_SETCHECK, BST_CHECKED, 0);
		SendDlgItemMessage(hwndDebug, IDC_DEBUGHOST, BM_SETCHECK, BST_CHECKED, 0);
	}
}

void DebugCloseDialog()
{
	DestroyWindow(hwndDebug);
	hwndDebug = NULL;
	hwndInfo = NULL;
	DebugEnabled = FALSE;
	hCurrentDialog = NULL;
	hCurrentAccelTable = NULL;
	DebugSource = DEBUG_NONE;
	LinesDisplayed = 0;
	InstCount = 0;
	DumpAddress = 0;
	DisAddress = 0;
	BPCount = 0;
	BPSOn = true;
	DebugOS = false;
	LastAddrInOS = false;
	LastAddrInBIOS = false;
	DebugROM = false;
	LastAddrInROM = false;
	DebugHost = true;
	DebugParasite = false;
	DebugInfoWidth = 0;
	memset(Breakpoints, 0, MAX_BPS * sizeof(Breakpoint));
	memset(Watches, 0, MAX_BPS * sizeof(Watch));
}

//*******************************************************************
void DebugDisplayInfoF(const char *format, ...)
{
    va_list args;
    int len;
    char *buffer;

    va_start(args, format);
    
    len = _vscprintf( format, args ) // _vscprintf doesn't count
                                + 1; // terminating '\0'
    
    buffer = (char*)malloc( len * sizeof(char) );

    vsprintf_s(buffer, len * sizeof(char), format, args );

	DebugDisplayInfo(buffer);
    free( buffer );
}

void DebugDisplayInfo(const char *info)
{
	HDC pDC = GetDC(hwndInfo);
	SIZE size;
	HGDIOBJ oldFont = SelectObject(pDC, (HFONT)SendMessage(hwndInfo, WM_GETFONT, 0, 0));
	GetTextExtentPoint(pDC, info, (int)strlen(info), &size);
	size.cx += 3;
	SelectObject(pDC, oldFont);
	ReleaseDC(hwndInfo, pDC);

	SendMessage(hwndInfo, LB_ADDSTRING, 0, (LPARAM)info);
	if((int)size.cx > DebugInfoWidth)
	{
		DebugInfoWidth = (int)size.cx;
		SendMessage(hwndInfo, LB_SETHORIZONTALEXTENT, DebugInfoWidth, 0);
	}

	LinesDisplayed++;
	if (LinesDisplayed > MAX_LINES)
	{
		SendMessage(hwndInfo, LB_DELETESTRING, 0, 0);
		LinesDisplayed = MAX_LINES;
	}
	if (LinesDisplayed > LINES_IN_INFO)
		SendMessage(hwndInfo, LB_SETTOPINDEX, LinesDisplayed - LINES_IN_INFO, 0);
}

BOOL CALLBACK DebugDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	switch (message)
	{
		case WM_INITDIALOG:
			SendDlgItemMessage(hwndDlg, IDC_DEBUGCOMMAND, EM_SETLIMITTEXT, MAX_COMMAND_LEN, 0);
			return TRUE;

		case WM_ACTIVATE:
			if (LOWORD(wParam) == WA_INACTIVE)
			{
				hCurrentDialog = NULL;
				hCurrentAccelTable = NULL;
			}
			else
			{
				hCurrentDialog = hwndDebug;
				hCurrentAccelTable = haccelDebug;
			}

			return FALSE;

		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case ID_ACCELUP:
					if(GetFocus() == GetDlgItem(hwndDebug, IDC_DEBUGCOMMAND))
						DebugHistoryMove(-1);
					return TRUE;
				case ID_ACCELDOWN:
					if(GetFocus() == GetDlgItem(hwndDebug, IDC_DEBUGCOMMAND))
						DebugHistoryMove(1);
					return TRUE;
				case IDC_DEBUGBREAK:
					DebugToggleRun();
					return TRUE;

				case IDC_DEBUGEXECUTE:
					DebugExecuteCommand();
					SetFocus(GetDlgItem(hwndDebug, IDC_DEBUGCOMMAND));
					return TRUE;

				case IDC_DEBUGBPS:
					BPSOn = (SendDlgItemMessage(hwndDebug, IDC_DEBUGBPS, BM_GETCHECK, 0, 0) == BST_CHECKED);
					break;

				case IDC_DEBUGBRK:
					BRKOn = (SendDlgItemMessage(hwndDebug, IDC_DEBUGBRK, BM_GETCHECK, 0, 0) == BST_CHECKED);
					break;

				case IDC_DEBUGOS:
					DebugOS = (SendDlgItemMessage(hwndDebug, IDC_DEBUGOS, BM_GETCHECK, 0, 0) == BST_CHECKED);
					break;

				case IDC_DEBUGROM:
					DebugROM = (SendDlgItemMessage(hwndDebug, IDC_DEBUGROM, BM_GETCHECK, 0, 0) == BST_CHECKED);
					break;

				case IDC_DEBUGHOST:
					DebugHost = (SendDlgItemMessage(hwndDebug, IDC_DEBUGHOST, BM_GETCHECK, 0, 0) == BST_CHECKED);
					break;

				case IDC_DEBUGPARASITE:
					DebugParasite = (SendDlgItemMessage(hwndDebug, IDC_DEBUGPARASITE, BM_GETCHECK, 0, 0) == BST_CHECKED);
					break;

				case IDC_WATCHDECIMAL:
				case IDC_WATCHENDIAN:
					WatchDecimal = (SendDlgItemMessage(hwndDebug, IDC_WATCHDECIMAL, BM_GETCHECK, 0, 0) == BST_CHECKED);
					WatchBigEndian = (SendDlgItemMessage(hwndDebug, IDC_WATCHENDIAN, BM_GETCHECK, 0, 0) == BST_CHECKED);
					DebugUpdateWatches(true);
					break;

				case IDCANCEL:
					DebugCloseDialog();
					return TRUE;
			}
	}
	return FALSE;
}

//*******************************************************************

void DebugToggleRun()
{
	if(DebugSource != DEBUG_NONE)
	{
		// Resume execution
		DebugBreakExecution(DEBUG_NONE);
	}
	else
	{
		// Cause manual break
		DebugBreakExecution(DEBUG_MANUAL);
	}
}

void DebugBreakExecution(DebugType type)
{
	DebugSource = type;
	if(type == DEBUG_NONE)
	{
		InstCount = 0;
		LastBreakAddr = 0;
		SetDlgItemText(hwndDebug, IDC_DEBUGBREAK, "Break");
	}
	else
	{
		InstCount = 1;
		SetDlgItemText(hwndDebug, IDC_DEBUGBREAK, "Cancel");
		LastAddrInBIOS = LastAddrInOS = LastAddrInROM = false;
	}
}

void DebugAssertBreak(int addr, int prevAddr, bool host)
{
	AddrInfo addrInfo;
	char* source = "Unknown";

	DebugUpdateWatches(false);
	SetDlgItemText(hwndDebug, IDC_DEBUGBREAK, "Continue");

	if(LastBreakAddr == 0)
		LastBreakAddr = addr;
	else
		return;
	
	switch(DebugSource)
	{
	case DEBUG_NONE:
		break;
	case DEBUG_VIDEO:
		source = "Video";
		break;
	case DEBUG_SERIAL:
		source = "Serial";
		break;
	case DEBUG_ECONET:
		source = "Econet";
		break;
	case DEBUG_TUBE:
		source = "Tube";
		break;
	case DEBUG_SYSVIA:
		source = "System VIA";
		break;
	case DEBUG_USERVIA:
		source = "User VIA";
		break;
	case DEBUG_MANUAL:
		source = "Manual";
		break;
	case DEBUG_BREAKPOINT:
		source = "Breakpoint";
		break;
	case DEBUG_BRK:
		source = "BRK instruction";
		break;
	case DEBUG_REMSER:
		source = "Remote server";
		break;
	}

	if(DebugSource == DEBUG_BREAKPOINT)
	{
		for(int i = 0; i < BPCount; i++)
		{
			if(Breakpoints[i].start == addr)
			{
				DebugDisplayInfoF("%s break at 0x%04X (Breakpoint '%s' / %s)",(host ? "Host" : "Parasite"), addr, Breakpoints[i].name, (DebugLookupAddress(addr, &addrInfo) ? addrInfo.desc : "Unknown"));
				if(prevAddr > 0)
					DebugDisplayInfoF("  Previous PC 0x%04X (%s)",prevAddr,DebugLookupAddress(prevAddr, &addrInfo) ? addrInfo.desc : "Unknown");
				return;
			}
		}
	}

	DebugDisplayInfoF("%s break at 0x%04X %s / %s)",(host ? "Host" : "Parasite"), addr, source, DebugLookupAddress(addr, &addrInfo) ? addrInfo.desc : "Unknown");
	if(prevAddr > 0)
		DebugDisplayInfoF("  Previous PC 0x%04X (%s)",prevAddr,DebugLookupAddress(prevAddr, &addrInfo) ? addrInfo.desc : "Unknown");
}

void DebugDisplayTrace(DebugType type, bool host, const char *info)
{
	if (DebugEnabled && ( (DebugHost && host) || (DebugParasite && !host) ))
	{
		switch (type)
		{
		case DEBUG_VIDEO:
			if (SendDlgItemMessage(hwndDebug, IDC_DEBUGVIDEO, BM_GETCHECK, 0, 0) == BST_CHECKED)
				DebugDisplayInfo(info);
			if (SendDlgItemMessage(hwndDebug, IDC_DEBUGVIDEOBRK, BM_GETCHECK, 0, 0) == BST_CHECKED)
				DebugBreakExecution(type);
			break;
		case DEBUG_USERVIA:
			if (SendDlgItemMessage(hwndDebug, IDC_DEBUGUSERVIA, BM_GETCHECK, 0, 0) == BST_CHECKED)
				DebugDisplayInfo(info);
			if (SendDlgItemMessage(hwndDebug, IDC_DEBUGUSERVIABRK, BM_GETCHECK, 0, 0) == BST_CHECKED)
				DebugBreakExecution(type);
			break;
		case DEBUG_SYSVIA:
			if (SendDlgItemMessage(hwndDebug, IDC_DEBUGSYSVIA, BM_GETCHECK, 0, 0) == BST_CHECKED)
				DebugDisplayInfo(info);
			if (SendDlgItemMessage(hwndDebug, IDC_DEBUGSYSVIABRK, BM_GETCHECK, 0, 0) == BST_CHECKED)
				DebugBreakExecution(type);
			break;
		case DEBUG_TUBE:
			if ((DebugHost && host) || (DebugParasite && !host))
			{
				if (SendDlgItemMessage(hwndDebug, IDC_DEBUGTUBE, BM_GETCHECK, 0, 0) == BST_CHECKED)
					DebugDisplayInfo(info);
				if (SendDlgItemMessage(hwndDebug, IDC_DEBUGTUBEBRK, BM_GETCHECK, 0, 0) == BST_CHECKED)
					DebugBreakExecution(type);
			}
			break;
		case DEBUG_SERIAL:
			if (SendDlgItemMessage(hwndDebug, IDC_DEBUGSERIAL, BM_GETCHECK, 0, 0) == BST_CHECKED)
				DebugDisplayInfo(info);
			if (SendDlgItemMessage(hwndDebug, IDC_DEBUGSERIALBRK, BM_GETCHECK, 0, 0) == BST_CHECKED)
				DebugBreakExecution(type);
			break;
		case DEBUG_REMSER:
			if (SendDlgItemMessage(hwndDebug, IDC_DEBUGREMSER, BM_GETCHECK, 0, 0) == BST_CHECKED)
				DebugDisplayInfo(info);
			if (SendDlgItemMessage(hwndDebug, IDC_DEBUGREMSERBRK, BM_GETCHECK, 0, 0) == BST_CHECKED)
				DebugBreakExecution(type);
			break;
		case DEBUG_ECONET:
			if (SendDlgItemMessage(hwndDebug, IDC_DEBUGECONET, BM_GETCHECK, 0, 0) == BST_CHECKED)
				DebugDisplayInfo(info);
			if (SendDlgItemMessage(hwndDebug, IDC_DEBUGECONETBRK, BM_GETCHECK, 0, 0) == BST_CHECKED)
				DebugBreakExecution(type);
			break;
		}
	}
}

void DebugUpdateWatches(bool all)
{
	int i, value;
	char str[200];
	if(WCount > 0)
	{
		for(i = 0; i < WCount; ++i)
		{
			switch(Watches[i].type)
			{
			case 'b':
				value = DebugReadMem(Watches[i].start, Watches[i].host);
				break;
			case 'w':
				if(WatchBigEndian)
					value = DebugReadMem(Watches[i].start, Watches[i].host) * 256
						+ DebugReadMem(Watches[i].start + 1, Watches[i].host);
				else
					value = DebugReadMem(Watches[i].start + 1, Watches[i].host) * 256
						+ DebugReadMem(Watches[i].start, Watches[i].host);
				break;
			case 'd':
				if(WatchBigEndian)
					value =	DebugReadMem(Watches[i].start, Watches[i].host) * 256 * 256 * 256 + 
						DebugReadMem(Watches[i].start + 1, Watches[i].host) * 256 * 256 + 
						DebugReadMem(Watches[i].start + 2, Watches[i].host) * 256 + 
						DebugReadMem(Watches[i].start + 3, Watches[i].host);
				else
					value =	DebugReadMem(Watches[i].start + 3, Watches[i].host) * 256 * 256 * 256 + 
						DebugReadMem(Watches[i].start + 2, Watches[i].host) * 256 * 256 + 
						DebugReadMem(Watches[i].start + 1, Watches[i].host) * 256 + 
						DebugReadMem(Watches[i].start, Watches[i].host);
				break;
			}
			if(all || value != Watches[i].value)
			{
				Watches[i].value = value;
				SendMessage(hwndW, LB_DELETESTRING, i, 0);
				if(WatchDecimal)
					sprintf(str,"%s%04X %s=%d (%c)", (Watches[i].host ? "" : "p"), Watches[i].start, Watches[i].name, Watches[i].value, Watches[i].type);
				else
				{
					switch(Watches[i].type)
					{
					case 'b':
						sprintf(str,"%s%04X %s=$%02X", (Watches[i].host ? "" : "p"), Watches[i].start, Watches[i].name, Watches[i].value);
						break;
					case 'w':
						sprintf(str,"%s%04X %s=$%04X", (Watches[i].host ? "" : "p"), Watches[i].start, Watches[i].name, Watches[i].value);
						break;
					case 'd':
						sprintf(str,"%s%04X %s=$%08X", (Watches[i].host ? "" : "p"), Watches[i].start, Watches[i].name, Watches[i].value);
						break;
					}
				}
				SendMessage(hwndW, LB_INSERTSTRING, i, (LPARAM)str);
			}
		}
	}	
}

bool DebugDisassembler(int addr, int prevAddr, int Accumulator, int XReg, int YReg, int PSR, int StackReg, bool host)
{
	char str[150];
	AddrInfo addrInfo;
	RomInfo romInfo;
	int i;

	// If this is the host and we're debugging that and have no further
	// instructions to execute, halt.
	if (host && DebugHost && DebugSource != DEBUG_NONE && InstCount == 0)
	{
		return(FALSE);
	}

	// Don't process further if we're not debugging the parasite either
	if (!host && !DebugParasite)
		return(TRUE);

	if (BRKOn && DebugReadMem(addr, host) == 0)
	{
		DebugBreakExecution(DEBUG_BRK);
		ProgramCounter++;
	}

	// Check breakpoints
	if (BPSOn)
	{
		for (i = 0; i < BPCount && DebugSource != DEBUG_BREAKPOINT; ++i)
		{
			if (Breakpoints[i].end == -1)
			{
				if (addr == Breakpoints[i].start)
				{
					DebugBreakExecution(DEBUG_BREAKPOINT);
				}
			}
			else
			{
				if (addr >= Breakpoints[i].start && addr <= Breakpoints[i].end)
				{
					DebugBreakExecution(DEBUG_BREAKPOINT);
				}
			}
		}
	}

	if (DebugSource == DEBUG_NONE)
		return(TRUE);

	if ( (TorchTube || AcornZ80) && !host)
	{
		if (DebugOS == false && addr >= 0xf800 && addr <= 0xffff)
		{
			if (!LastAddrInBIOS)
			{
				DebugDisplayInfoF("Entered BIOS (0xF800-0xFFFF) at 0x%04X (%s)",addr,DebugLookupAddress(addr,&addrInfo) ? addrInfo.desc : "Unknown");
				LastAddrInBIOS = true;
				LastAddrInOS = LastAddrInROM = false;
			}
			return(TRUE);
		}
		LastAddrInBIOS = false;
	}
	else
	{
		if (DebugOS == false && addr >= 0xc000 && addr <= 0xfbff)
		{
			if (!LastAddrInOS)
			{
				DebugDisplayInfoF("Entered OS (0xC000-0xFBFF) at 0x%04X (%s)",addr,DebugLookupAddress(addr,&addrInfo) ? addrInfo.desc : "Unknown");
				LastAddrInOS = true;
				LastAddrInBIOS = LastAddrInROM = false;
			}
			return(TRUE);
		}
		LastAddrInOS = false;

		if (DebugROM == false && addr >= 0x8000 && addr <= 0xbfff)
		{
			if (!LastAddrInROM)
			{
				if(ReadRomInfo(PagedRomReg, &romInfo))
					DebugDisplayInfoF("Entered ROM \"%s\" (0x8000-0xBFFF) at 0x%04X", romInfo.Title, addr);
				else
					DebugDisplayInfoF("Entered unknown ROM (0x8000-0xBFFF) at 0x%04X", addr);
				LastAddrInROM = true;
				LastAddrInOS = LastAddrInBIOS = false;
			}
			return(TRUE);
		}
		LastAddrInROM = false;
	}

	if (host && InstCount == 0)
	{
		return(FALSE);
	}

	
	DebugAssertBreak(addr, prevAddr, host);
	// Parasite instructions:
	if ( (TorchTube || AcornZ80) && !host)
	{
		char buff[128];
		Z80_Disassemble(addr, buff);

		Disp_RegSet1(str);
		sprintf(str + strlen(str), " %s", buff);
				
		DebugDisplayInfo(str);
		Disp_RegSet2(str);

	}
	else
	{
		
		DebugDisassembleInstruction(addr, host, str);

		sprintf(str + strlen(str), "A=%02X X=%02X Y=%02X S=%02X ", Accumulator, XReg, YReg, StackReg);

		sprintf(str + strlen(str), (PSR & FlagC) ? "C" : ".");
		sprintf(str + strlen(str), (PSR & FlagZ) ? "Z" : ".");
		sprintf(str + strlen(str), (PSR & FlagI) ? "I" : ".");
		sprintf(str + strlen(str), (PSR & FlagD) ? "D" : ".");
		sprintf(str + strlen(str), (PSR & FlagB) ? "B" : ".");
		sprintf(str + strlen(str), (PSR & FlagV) ? "V" : ".");
		sprintf(str + strlen(str), (PSR & FlagN) ? "N" : ".");

		if (!host)
			sprintf(str + strlen(str), "  Parasite");
	}

	DebugDisplayInfo(str);

	// If host debug is enable then only count host instructions
	// and display all parasite inst (otherwise we lose them).
	if ((DebugHost && host) || !DebugHost)
		if (InstCount > 0)
			InstCount--;

	return(TRUE);
}

bool DebugLookupAddress(int addr, AddrInfo* addrInfo)
{
	int i;
	RomInfo rom;

	if(MemoryMaps[ROMSEL].count > 0)
	{
		// Try current ROM's map
		for(i = 0; i < MemoryMaps[ROMSEL].count; i++)
		{
			if(addr >= MemoryMaps[ROMSEL].entries[i].start && addr <= MemoryMaps[ROMSEL].entries[i].end)
			{
				addrInfo->start = MemoryMaps[ROMSEL].entries[i].start;
				addrInfo->end = MemoryMaps[ROMSEL].entries[i].end;
				sprintf(addrInfo->desc,"%s: %s", ReadRomInfo(ROMSEL, &rom) ? "ROM" : rom.Title);
				return true;
			}
		}
	}
	else if(addr >= 0x8000 && addr <= 0xBFFF && ReadRomInfo(ROMSEL, &rom))
	{
		addrInfo->start = 0x8000;
		addrInfo->end = 0xBFFF;
		// Try ROM info:
		if(ReadRomInfo(ROMSEL, &rom))
		{
			sprintf(addrInfo->desc,"Paged ROM bank %d: %s",ROMSEL,rom.Title);
			return true;
		}
		else if(RomWritable[ROMSEL])
		{
			sprintf(addrInfo->desc,"Sideways RAM bank %d",ROMSEL);
			return true;
		}
	}
	else
	{
		// Some custom machine related stuff
		if(MachineType == 3)
		{
			// Master cartridge (not implemented in BeebEm yet)
			if((ACCCON & 0x20) && addr >= 0xFC00 && addr <= 0xFDFF)
			{
				addrInfo->start = 0xFC00;
				addrInfo->end = 0xFDFF;
				sprintf(addrInfo->desc,"Cartridge (ACCCON bit 5 set)",ROMSEL);
				return true;
			}
			// Master private and shadow RAM.
			if((ACCCON & 0x08) && addr >= 0xC000 && addr <= 0xDFFF)
			{
				addrInfo->start = 0xC000;
				addrInfo->end = 0xDFFF;
				sprintf(addrInfo->desc,"8K Private RAM (ACCCON bit 3 set)");
				return true;
			}
			if((ACCCON & 0x04) && addr >= 0x3000 && addr <= 0x7FFF)
			{
				addrInfo->start = 0x3000;
				addrInfo->end = 0x7FFF;
				sprintf(addrInfo->desc,"Shadow RAM (ACCCON bit 2 set)");
				return true;
			}
			if((ACCCON & 0x02) && PrePC >= 0xC000 && PrePC <= 0xDFFF && addr >= 0x3000 && addr <= 0x7FFF)
			{
				addrInfo->start = 0x3000;
				addrInfo->end = 0x7FFF;
				sprintf(addrInfo->desc,"Shadow RAM (ACCCON bit 1 set and PC in VDU driver)");
				return true;
			}
		}
		else if(MachineType == 2)
		{
			// BBC B+
			if(addr >= 0x3000 && addr <= 0x7FFF)
			{
				addrInfo->start = 0x3000;
				addrInfo->end = 0x7FFF;
				if(Sh_Display && PrePC>=0xC000 && PrePC<=0xDFFF)
				{
					sprintf(addrInfo->desc,"Shadow RAM (PC in VDU driver)");
					return true;
				}
				else if(Sh_Display && MemSel && PrePC>=0xA000 && PrePC <=0xAFFF)
				{
					addrInfo->start = 0x3000;
					addrInfo->end = 0x7FFF;
					sprintf(addrInfo->desc,"Shadow RAM (PC in upper 4K of ROM and shadow selected)");
					return true;
				}
			}
			else if(addr >= 0x8000 && addr <= 0xAFFF && MemSel)
			{
				addrInfo->start = 0x8000;
				addrInfo->end = 0xAFFF;
				sprintf(addrInfo->desc,"Paged RAM");
				return true;
			}
		}
		else if(MachineType == 1)
		{
			// Integra B
			if(ShEn==1 && MemSel==0 && addr >= 0x3000 && addr <= 0x7FFF)
			{
				addrInfo->start = 0x3000;
				addrInfo->end = 0x7FFF;
				sprintf(addrInfo->desc,"Shadow RAM");
				return true;
			}
			if(PrvEn)
			{
				if(Prvs8 && addr >= 0x8000 && addr <= 0x83FF)
				{
					addrInfo->start = 0x8000;
					addrInfo->end = 0x83FF;
					sprintf(addrInfo->desc,"1K private area");
					return true;
				}
				else if(Prvs4 && addr >= 0x8000 && addr <= 0x8FFF)
				{
					addrInfo->start = 0x8400;
					addrInfo->end = 0x8FFF;
					sprintf(addrInfo->desc,"4K private area");
					return true;
				}
				else if(Prvs1 && addr >= 0x9000 && addr <= 0xAFFF)
				{
					addrInfo->start = 0x9000;
					addrInfo->end = 0xAFFF;
					sprintf(addrInfo->desc,"8K private area");
					return true;
				}
			}
		}

		// Try OS map:
		if(MemoryMaps[16].count > 0)
		{
			for(i = 0; i < MemoryMaps[16].count; i++)
			{
				if(addr >= MemoryMaps[16].entries[i].start && addr <= MemoryMaps[16].entries[i].end)
				{
					memcpy(addrInfo, &MemoryMaps[16].entries[i], sizeof(AddrInfo));
					return true;
				}
			}
		}
	}
	return false;
}

void DebugExecuteCommand()
{
	char command[MAX_COMMAND_LEN + 1];
	GetDlgItemText(hwndDebug, IDC_DEBUGCOMMAND, command, MAX_COMMAND_LEN);
	DebugParseCommand(command);
}

void DebugInitMemoryMaps()
{
	for(int i = 0; i < _countof(MemoryMaps); i++)
	{
		MemoryMaps[i].count = 0;
	}
}

bool DebugLoadMemoryMap(char* filename, int bank)
{
	char line[1024];
	char errstr[200];
	AddrInfo* entry;
	MemoryMap* map;
	FILE *infile;
	char *buf;

	if(bank < 0 || bank > 16)
		return false;

	map = &MemoryMaps[bank];
	infile = fopen(filename, "r");
	if (infile == NULL)
	{
		return false;
	}
	else
	{
		map->count = 0;
		map->entries = NULL;
		while(fgets(line, _countof(line), infile) != NULL)
		{
			DebugChompString(line);
			buf = line;
			while(buf[0] == ' ' || buf[0] == '\t' || buf[0] == '\r' || buf[0] == '\n')
				buf++;
			if(buf[0] == ';' || buf[0] == '\0')	// Skip comments and empty lines
				continue;
			if(map->count % 256 == 0)
			{
				AddrInfo* newAddrInfo;
				newAddrInfo = (AddrInfo*)realloc(map->entries, (map->count + 256) * sizeof(AddrInfo));
				if(newAddrInfo == NULL)
				{
					fclose(infile);
					sprintf(errstr, "Allocation failure reading memory map!");
					MessageBox(GETHWND,errstr,WindowTitle,MB_OK|MB_ICONERROR);
					if(map->entries != NULL)
					{
						free(map->entries);
						map->entries = NULL;
						map->count = 0;
					}
					return false;
				}
				map->entries = newAddrInfo;
			}

			entry = &map->entries[map->count];

			memset(entry->desc, 0, _countof(entry->desc));
			if(sscanf(buf, " %x %x %99c", &entry->start, &entry->end, &entry->desc) != 3)
			{
				sprintf(errstr, "Invalid memory map format!");
				MessageBox(GETHWND,errstr,WindowTitle,MB_OK|MB_ICONERROR);
				free(map->entries);
				map->entries = NULL;
				map->count = 0;
				fclose(infile);
				return false;
			}
			map->count++;
		}
		fclose(infile);
	}
	return true;
}

void DebugLoadLabels(char *filename)
{
	char buf[1024];
	FILE *infile;
	infile = fopen(filename, "r");
	if (infile == NULL)
	{
		DebugDisplayInfoF("Error: Failed to open labels from %s", filename);
	}
	else
	{
		LabelCount = 0;
		while(fgets(buf, _countof(buf), infile) != NULL && LabelCount < MAX_LABELS)
		{
			DebugChompString(buf);
			if(sscanf(buf,"%*s %x .%64s", &Labels[LabelCount].addr, Labels[LabelCount].name) != 2)
			{
				DebugDisplayInfoF("Error: Invalid labels format: %s", filename);
				fclose(infile);
				LabelCount = 0;
				return;
			}
			LabelCount++;
		}
		DebugDisplayInfoF("Loaded %d labels from %s", LabelCount, filename);
		fclose(infile);
	}
}

void DebugRunScript(char *filename)
{
	char buf[1024];
	FILE *infile;

	infile=fopen(filename,"r");
	if (infile == NULL)
	{
		DebugDisplayInfoF("Failed to read script file:\n  %s", filename);
	}
	else
	{
		DebugDisplayInfoF("Running script %s",filename);
		while(fgets(buf, _countof(buf), infile) != NULL)
		{
			DebugChompString(buf);
			if(strlen(buf) > 0)
				DebugParseCommand(buf);
		}
		fclose(infile);
	}
}

void DebugChompString(char *str)
{
	int end = strlen(str) - 1;
	while(end > 0 && (str[end] == '\r' || str[end] == '\n' || str[end] == ' ' || str[end] == '\t'))
	{
		str[end] = '\0';
		end--;
	}
}

int DebugParseLabel(char *label)
{
	//int addr;
	// Check it's not simply a hex address
	//if(sscanf(label, "%x", &addr) == 1)
	//	return addr;

	for(int i = 0; i < LabelCount; i++)
	{
		if(_stricmp(label, Labels[i].name) == 0)
			return Labels[i].addr;
	}
	return -1;
}

void DebugHistoryAdd(char *command)
{
	int i;
	// Do nothing if this is the same as the last
	// command
	if(_stricmp(debugHistory[0], command) != 0)
	{
		// Otherwise insert command string at index 0.
		for(i = MAX_HISTORY - 2; i >= 0; i--)
			memcpy(debugHistory[i + 1],debugHistory[i],300);
		strncpy(debugHistory[0], command, 300);
	}
	debugHistoryIndex = -1;
}

void DebugHistoryMove(int delta)
{
	int newIndex = debugHistoryIndex - delta;
	if(newIndex < 0)
	{
		debugHistoryIndex = -1;
		SetDlgItemText(hwndDebug, IDC_DEBUGCOMMAND, "");
	}
	if(newIndex >= MAX_HISTORY)
		newIndex = MAX_HISTORY - 1;
	if(strlen(debugHistory[newIndex]) == 0)
		return;
	else
	{
		debugHistoryIndex = newIndex;
		DebugSetCommandString(debugHistory[debugHistoryIndex]);
	}
}

void DebugSetCommandString(char* string)
{
	if(debugHistoryIndex == -1 && _stricmp(debugHistory[0], string) == 0)
	{
		// The string we're about to set is the same as the top history one,
		// so use history to set it. This is just a nicety to make the up
		// key work as expected when commands such as 'next' and 'peek'
		// have automatically filled in the command box.
		DebugHistoryMove(-1);
	}
	else
	{
		SetDlgItemText(hwndDebug, IDC_DEBUGCOMMAND, string);
		SendDlgItemMessage(hwndDebug, IDC_DEBUGCOMMAND, EM_SETSEL, strlen(string),strlen(string));
	}
}

void DebugParseCommand(char *command)
{
	char label[65], addrStr[6];
	char info[MAX_PATH + 100];
	int end, i, addr;
	char* args;

	while(command[0] == '\n' || command[0] == '\r' || command[0] == '\t' || command[0] == ' ')
		command++;

	if (strlen(command) == 0 || command[0] == '/' || command[0] == ';' || command[0] == '#')
		return;

	DebugHistoryAdd(command);

	info[0] = '\0';
	args = strchr(command, ' ');
	if(args == NULL)
	{
		args = "";
	}
	else
	{
		char* commandEnd = args;
		// Resolve labels:
		while(args[0] != '\0')
		{
			if(args[0] == ' ' && args[1] == '.')
			{
				if(sscanf(&args[2], "%64s", label) == 1)
				{
					// Try to resolve label:
					addr = DebugParseLabel(label);
					if(addr == -1)
					{
						DebugDisplayInfoF("Error: Label %s not found", label);
						return;
					}
					sprintf(addrStr, " %04X", addr);
					strncat(info, addrStr, _countof(addrStr));
					args += strnlen(label,_countof(label)) + 1;
				}
			}
			else
			{
				end = (int)strnlen(info, _countof(info));
				info[end] = args[0];
				info[end+1] = '\0';
			}
			args++;
		}

		args = info;
		while(args[0] == ' ')
			args++;

		commandEnd[0] = '\0';
	}

	SetDlgItemText(hwndDebug, IDC_DEBUGCOMMAND, "");

	for(i = 0; i < _countof(DebugCmdTable); i++)
	{
		if(_stricmp(DebugCmdTable[i].name, command) == 0)
		{
			if(!DebugCmdTable[i].handler(args))
				DebugCmdHelp(command);
			return;
		}
	}
	DebugDisplayInfoF("Invalid command %s - try 'help'",command);

	return;
}

/**************************************************************
 * Start of debugger command handlers                         *
 **************************************************************/

bool DebugCmdEcho(char* args)
{
	DebugDisplayInfo(args);
	return true;
}

bool DebugCmdGoto(char* args)
{
	bool host = true;
	int addr = 0;

	if (tolower(args[0]) == 'p') // Parasite
	{
		host = false;
		args++;
	}

	if(sscanf(args, "%x", &addr) == 1)
	{
		addr = addr & 0xffff;
		if (host)
			ProgramCounter = addr;
		else
			TubeProgramCounter = addr;

		DebugDisplayInfoF("Next %s instruction address 0x%04X", host ? "host" : "parasite", addr);
		return true;
	}
	return false;
}

bool DebugCmdFile(char* args)
{
	char mode;
	int i = 0;
	int addr = 0;
	int count = MAX_BUFFER;
	char filename[MAX_PATH];
	memset(filename, 0, MAX_PATH);
	if(sscanf(args,"%c %x %u %259c", &mode, &addr, &count, filename) == 4 ||
		sscanf(args,"%c %x %259c", &mode, &addr, filename) == 3)
	{
		addr &= 0xFFFF;
		if(tolower(mode) == 'r')
		{
			FILE *fd = fopen(filename, "rb");
			if (fd)
			{
				if(count > MAX_BUFFER)
					count = MAX_BUFFER;
				count = (int)fread(buffer, 1, count, fd);
				fclose(fd);

				for (i = 0; i < count; ++i)
					BeebWriteMem((addr + i) & 0xffff, buffer[i] & 0xff);

				DebugDisplayInfoF("Read %d bytes from %s to address 0x%04X", count,filename, addr);
			}
			else
			{
				DebugDisplayInfoF("Failed to open file: %s", filename);
			}
			return true;
		}
		else if(tolower(mode) == 'w')
		{
			FILE *fd = fopen(filename, "wb");
			if (fd)
			{
				if(count + addr > 0xFFFF)
					count = 0xFFFF - addr;
				for (i = 0; i < count; ++i)
					buffer[i] = DebugReadMem((addr + i) & 0xffff, true);

				count = (int)fwrite(buffer, 1, count, fd);
				fclose(fd);
				DebugDisplayInfoF("Wrote %d bytes from address 0x%04X to %s", count, addr,filename);
			}
			else
			{
				DebugDisplayInfoF("Failed to open file: %s", filename);
			}
			return true;
		}
	}
	return false;
}

bool DebugCmdPoke(char* args)
{
	int addr, data;
	int start;
	int i = 0;
	bool host = true;
	if (tolower(args[0]) == 'p') // Parasite
	{
		host = false;
		args++;
		while(args[0] == ' ')
			args++;
	}
	if (sscanf(args, "%x", &addr) == 1)
	{
		args = strchr(args, ' ');
		start = addr = addr & 0xFFFF;
		if(args == NULL)
			return false;
		while (args[0] != '\0')
		{
			while (args[0] == ' ')
				args++;
			if (sscanf(args, "%x", &data) == 1)
			{
				DebugWriteMem(addr, host, (unsigned char)(data & 0xff));
				i++;
				addr++;
			}
			// Spool past last found addr.
			while(args[0] != ' ' && args[0] != '\0')
				args++;
		}

		if(i == 0)
			return false;
		else
		{
			DebugDisplayInfoF("Changed %d bytes starting at 0x%04X", i, addr);
			return true;
		}
	}
	else
		return false;
}

bool DebugCmdSave(char* args)
{
	int count = 0;
	int i;
	char filename[MAX_PATH];
	char* info = NULL;
	int infoSize = 0;
	int len = 0;
	memset(filename, 0, MAX_PATH);

	if (sscanf(args, "%u %259c", &count, filename) == 2 ||
		sscanf(args, "%259c", filename) == 1)
	{
		if (count <= 0 || count > LinesDisplayed)
			count = LinesDisplayed;

		FILE *fd = fopen(filename, "w");
		if (fd)
		{
			for (i = LinesDisplayed - count; i < LinesDisplayed; ++i)
			{
				len = (int)(SendMessage(hwndInfo, LB_GETTEXTLEN, i, NULL) + 1) * sizeof(TCHAR);
				if(len > infoSize)
				{
					infoSize = len;
					info = (char*)realloc(info, len);
				}
				if(info != NULL)
				{
					SendMessage(hwndInfo, LB_GETTEXT, i, (LPARAM)info);
					fprintf(fd, "%s\n", info);
				}
				else
				{
					DebugDisplayInfoF("Allocation failure while writing to %s", filename);
					fclose(fd);
					return true;
				}
			}
			fclose(fd);
			free(info);
			DebugDisplayInfoF("Wrote %d lines to: %s", count, filename);
		}
		else
		{
			DebugDisplayInfoF("Failed open for write: %s", filename);
		}
		return true;
	}
	return false;
}

bool DebugCmdState(char* args)
{
	int i;
	RomInfo rom;
	char flags[50] = "";
	switch (tolower(args[0]))
	{
		case 'v': // Video state
			DebugVideoState();
			break;
		case 'u': // User via state
			DebugUserViaState();
			break;
		case 's': // Sys via state
			DebugSysViaState();
			break;
		case 't': // Tube state
			DebugTubeState();
			break;
		case 'm': // Memory state
			DebugMemoryState();
			break;
		case 'r': // ROM state
			DebugDisplayInfo("ROMs by priority:");
			for(i = 15; i >= 0; i--)
			{
				flags[0] = '\0';
				if(ReadRomInfo(i, &rom))
				{
					if(RomWritable[i])
						strcat(flags, "Writable, ");
					if(rom.Flags & RomLanguage)
						strcat(flags, "Language, ");
					if(rom.Flags & RomService)
						strcat(flags, "Service, ");
					if(rom.Flags & RomRelocate)
						strcat(flags, "Relocate, ");
					if(rom.Flags & RomSoftKey)
						strcat(flags, "SoftKey, ");
					flags[strlen(flags) - 2] = '\0';
					DebugDisplayInfoF("Bank %d: %s %s",i,rom.Title,(PagedRomReg == i ? "(Paged in)" : ""));
					if(strlen(rom.VersionStr) > 0)
						DebugDisplayInfoF("         Version: 0x%02X (%s)",rom.Version, rom.VersionStr);
					else
						DebugDisplayInfoF("         Version: 0x%02X",rom.Version);
					DebugDisplayInfoF("       Copyright: %s",rom.Copyright);
					DebugDisplayInfoF("           Flags: %s",flags);
					DebugDisplayInfoF("   Language Addr: 0x%04X",rom.LanguageAddr);
					DebugDisplayInfoF("    Service Addr: 0x%04X",rom.ServiceAddr);
					DebugDisplayInfoF("  Workspace Addr: 0x%04X",rom.WorkspaceAddr);
					DebugDisplayInfoF(" Relocation Addr: 0x%08X",rom.RelocationAddr);
					DebugDisplayInfo("");
				}
			}
			break;
		default:
			return false;
	}
	return true;
}

bool DebugCmdCode(char* args)
{
	bool host = true;
	int count = LINES_IN_INFO;

	if (tolower(args[0]) == 'p') // Parasite
	{
		host = false;
		args++;
	}

	sscanf(args, "%x %u", &DisAddress, &count);
	DisAddress &= 0xffff;
	DisAddress += DebugDisassembleCommand(DisAddress, count, host);
	if (DisAddress > 0xffff)
		DisAddress = 0;
	DebugSetCommandString(host ? "code" : "code p");
	return true;
}

bool DebugCmdPeek(char* args)
{
	int start = DumpAddress;
	int count = 256;
	bool host = true;

	if (tolower(args[0]) == 'p') // Parasite
	{
		host = false;
		args++;
	}
	sscanf(args, "%x %u", &DumpAddress, &count);
	DumpAddress &= 0xffff;
	DebugMemoryDump(DumpAddress, count, host);
	DumpAddress += count;
	if (DumpAddress > 0xffff)
		DumpAddress = 0;
	DebugSetCommandString(host ? "peek" : "peek p");
	return true;
}

bool DebugCmdNext(char* args)
{
	int count = 1;
	if(args[0] != '\0' && sscanf(args, "%u", &count) == 0)
		return false;
	if (count > MAX_LINES)
	count = MAX_LINES;
	InstCount = count;
	DebugSetCommandString("next");
	return true;
}

bool DebugCmdSet(char* args)
{
	char name[20];
	char state[4];
	int checkState = BST_UNCHECKED;
	int dlgItem = 0;

	if(sscanf(args,"%s %s",name,state) == 2)
	{
		//host/parasite/rom/os/bigendian/breakpoint/decimal/brk
		if(_stricmp(state, "on") == 0)
			checkState = BST_CHECKED;

		if(_stricmp(name, "host") == 0)
		{
			dlgItem = IDC_DEBUGHOST;
			DebugHost = (checkState == BST_CHECKED);
		}
		else if(_stricmp(name, "parasite") == 0)
		{
			dlgItem = IDC_DEBUGPARASITE;
			DebugParasite = (checkState == BST_CHECKED);
		}
		else if(_stricmp(name, "rom") == 0)
		{
			dlgItem = IDC_DEBUGROM;
			DebugROM = (checkState == BST_CHECKED);
		}
		else if(_stricmp(name, "os") == 0)
		{
			dlgItem = IDC_DEBUGOS;
			DebugOS = (checkState == BST_CHECKED);
		}
		else if(_stricmp(name, "endian") == 0)
		{
			dlgItem = IDC_WATCHENDIAN;
			WatchBigEndian = (checkState == BST_CHECKED);
			DebugUpdateWatches(true);
		}
		else if(_stricmp(name, "breakpoints") == 0)
		{
			dlgItem = IDC_DEBUGBPS;
			BPSOn = (checkState == BST_CHECKED);
		}
		else if(_stricmp(name, "decimal") == 0)
		{
			dlgItem = IDC_WATCHDECIMAL;
			WatchDecimal = (checkState == BST_CHECKED);
		}
		else if(_stricmp(name, "brk") == 0)
		{
			dlgItem = IDC_DEBUGBRK;
			BRKOn = (checkState == BST_CHECKED);
		}
		else
			return false;
		SendDlgItemMessage(hwndDebug, dlgItem, BM_SETCHECK, checkState, 0);
		return true;
	}
	else
		return false;
}

bool DebugCmdBreakContinue(char* args)
{
	DebugToggleRun();
	DebugSetCommandString(".");
	return true;
}

bool DebugCmdHelp(char* args)
{
	int i, addr;
	int li = 0;
	AddrInfo addrInfo;
	char aliasInfo[300];
	aliasInfo[0] = 0;

	if(args[0] == '\0')
	{
		DebugDisplayInfo("- BeebEm debugger help -");
		DebugDisplayInfo("  Parameters in [] are optional. 'p' can be specified in some commands");
		DebugDisplayInfo("  to specify parasite processor. Words preceded with a . will be");
		DebugDisplayInfo("  interpreted as labels and may be used in place of addresses.");
		// Display help for basic commands:
		for(i = 0; i < _countof(DebugCmdTable); i++)
		{
			if(strlen(DebugCmdTable[i].help) > 0 && strlen(DebugCmdTable[i].argdesc) > 0)
			{
				DebugDisplayInfo("");
				DebugDisplayInfoF("  %s %s",DebugCmdTable[i].name,DebugCmdTable[i].argdesc);
				DebugDisplayInfoF("    %s",DebugCmdTable[i].help);
			}
		}

		DebugDisplayInfo("");
		DebugDisplayInfo("Command aliases:");
		// Display help for aliases
		for(i = 0; i < _countof(DebugCmdTable); i++)
		{
			if(strlen(DebugCmdTable[i].help) > 0 && strlen(DebugCmdTable[i].argdesc) > 0)
			{
				if(strlen(aliasInfo) > 0)
				{
					aliasInfo[strlen(aliasInfo) - 2] = 0;
					DebugDisplayInfoF("%8s: %s",DebugCmdTable[li].name, aliasInfo);
				}
				aliasInfo[0] = 0;
				li = i;
			}
			else if(strlen(DebugCmdTable[i].help) == 0 && strlen(DebugCmdTable[i].argdesc) == 0
					&& DebugCmdTable[li].handler == DebugCmdTable[i].handler)
			{
				strcat(aliasInfo, DebugCmdTable[i].name);
				strcat(aliasInfo, ", ");
			}
		}
		if(aliasInfo[0] != 0)
		{
			aliasInfo[strlen(aliasInfo) - 2] = 0;
			DebugDisplayInfoF("%8s: %s",DebugCmdTable[li].name, aliasInfo);
		}

		DebugDisplayInfo("");
	}
	else
	{
		// Display help for specific command/alias
		for(i = 0; i < _countof(DebugCmdTable); i++)
		{
			// Remember the last index with args and help so we can support aliases.
			if(strlen(DebugCmdTable[i].help) > 0 && strlen(DebugCmdTable[i].argdesc) > 0)
				li = i;
			if(_stricmp(args, DebugCmdTable[i].name) == 0)
			{
				if(strlen(DebugCmdTable[i].help) == 0 && strlen(DebugCmdTable[i].argdesc) == 0
					&& DebugCmdTable[li].handler == DebugCmdTable[i].handler)
				{
					// This is an alias:
					DebugDisplayInfoF("%s - alias of %s",DebugCmdTable[i].name,DebugCmdTable[li].name);
					DebugCmdHelp(DebugCmdTable[li].name);
				}
				else
				{
					DebugDisplayInfoF("%s - %s",DebugCmdTable[i].name,DebugCmdTable[i].help);
					DebugDisplayInfoF("  Usage: %s %s",DebugCmdTable[i].name,DebugCmdTable[i].argdesc);
				}
				return true;
			}
		}
		// Display help for address
		if(sscanf(args, "%x", &addr) == 1)
		{
			if(DebugLookupAddress(addr, &addrInfo))
				DebugDisplayInfoF("0x%04X: %s (0x%04X-0x%04X)", addr, addrInfo.desc, addrInfo.start, addrInfo.end);
			else
				DebugDisplayInfoF("0x%04X: No description", addr);
		}
		else
			DebugDisplayInfoF("Help: Command %s was not recognised.", args);
	}
	return true;
}

bool DebugCmdScript(char *args)
{
	if(args[0] != '\0')
	{
		DebugRunScript(args);
	}
	return true;
}

bool DebugCmdLabels(char *args)
{
	int i;
	if(args[0] != '\0')
		DebugLoadLabels(args);
	if(LabelCount == 0)
		DebugDisplayInfo("No labels defined.");
	else
	{
		DebugDisplayInfoF("%d known labels:", LabelCount);
		for(i = 0; i < LabelCount; i++)
			DebugDisplayInfoF("%04X %s", Labels[i].addr, Labels[i].name);
	}
	return true;
}

bool DebugCmdWatch(char *args)
{
	Watch w;
	char info[64];
	int i;
	memset(w.name, 0, _countof(w.name));
	w.start = -1;
	w.host = true;
	w.type = 'w';

	if (WCount < _countof(Watches))
	{
		if (tolower(args[0]) == 'p') // Parasite
		{
			w.host = false;
			args++;
		}

		if (sscanf(args, "%x %c %50c", &w.start, &w.type, w.name) >= 2 ||
			sscanf(args, "%x", &w.start, w.name) >= 1)
		{
			// Check type is valid
			w.type = tolower(w.type);
			if(w.type != 'b' && w.type != 'w' && w.type != 'd')
				return false;

			sprintf(info, "%s%04X", (w.host ? "" : "p"), w.start);

			// Check if watch in list
			i = (int)SendMessage(hwndW, LB_FINDSTRING, 0, (LPARAM)info);
			if (i != LB_ERR)
			{
				SendMessage(hwndW, LB_DELETESTRING, i, 0);
				for (i = 0; i < WCount; ++i)
				{
					if (Watches[i].start == w.start)
					{
						if (i != WCount - 1)
							memmove(&Watches[i], &Watches[i+1], sizeof(Watch) * (WCount - i - 1));
						WCount--;
						break;
					}
				}
			}
			else
			{
				memcpy(&Watches[WCount], &w, sizeof(Watch));
				WCount++;
				SendMessage(hwndW, LB_ADDSTRING, 0, (LPARAM)info);
				DebugUpdateWatches(true);
			}
			SetDlgItemText(hwndDebug, IDC_DEBUGCOMMAND, "");
		}
		else
		{
			return false;
		}
	}
	else
	{
		DebugDisplayInfo("You have too many watches!");
	}
	return true;
}

bool DebugCmdToggleBreak(char *args)
{
	Breakpoint bp;
	char info[64];
	int i;

	memset(bp.name, 0, _countof(bp.name));
	bp.start = bp.end = -1;

	if (BPCount < _countof(Breakpoints))
	{
		if (sscanf(args, "%x-%x %50c", &bp.start, &bp.end, bp.name) >= 2 ||
			sscanf(args, "%x %50c", &bp.start, bp.name) >= 1)
		{
			sprintf(info, "%04X", bp.start);
			// Check if BP in list
			i = (int)SendMessage(hwndBP, LB_FINDSTRING, 0, (LPARAM)info);
			if (i != LB_ERR)
			{
				// Yes - delete
				SendMessage(hwndBP, LB_DELETESTRING, i, 0);
				for (i = 0; i < BPCount; i++)
				{
					if (Breakpoints[i].start == bp.start)
					{
						if (i != BPCount - 1)
							memmove(&Breakpoints[i], &Breakpoints[i+1], sizeof(Breakpoint) * (BPCount - i - 1));
						BPCount--;
						break;
					}
				}
			}
			else
			{
				if(bp.end >= 0 && bp.end < bp.start)
				{
					DebugDisplayInfo("Error: Invalid breakpoint range.");
					return false;
				}
				// No - add a new bp.
				memcpy(&Breakpoints[BPCount], &bp, sizeof(Breakpoint));
				if (Breakpoints[BPCount].end >= 0)
					sprintf(info, "%04X-%04X %s", Breakpoints[BPCount].start, Breakpoints[BPCount].end, Breakpoints[BPCount].name);
				else
					sprintf(info, "%04X %s", Breakpoints[BPCount].start, Breakpoints[BPCount].name);
				BPCount++;
				SendMessage(hwndBP, LB_ADDSTRING, 0, (LPARAM)info);
			}
			SetDlgItemText(hwndDebug, IDC_DEBUGCOMMAND, "");
		}
		else
		{
			return false;
		}
	}
	else
	{
		DebugDisplayInfo("You have too many breakpoints!");
	}
	return true;
}

/**************************************************************
 * End of debugger command handlers                           *
 **************************************************************/

int DebugReadMem(int addr, bool host)
{
	if (host)
		return BeebReadMem(addr);
	if ((TorchTube || AcornZ80))
		return ReadZ80Mem(addr);
	return TubeReadMem(addr);
}

void DebugWriteMem(int addr, bool host, unsigned char data)
{
	if (host)
		BeebWriteMem(addr, data);
	if ((TorchTube || AcornZ80))
		WriteZ80Mem(addr, data);
	TubeWriteMem(addr, data);
}


int DebugDisassembleInstruction(int addr, bool host, char *opstr)
{
	char *s;
	int opcode;
	InstInfo *ip; 
	int operand;
	int l;

	sprintf(opstr, "%04X ", addr);

	opcode = DebugReadMem(addr, host);
	ip = &optable[opcode];

	s=opstr+strlen(opstr);

	switch (ip->nb) {
		case 1:
			sprintf(s, "%02X        ", DebugReadMem(addr, host));
			break;
		case 2:
			sprintf(s, "%02X %02X     ", DebugReadMem(addr, host), DebugReadMem(addr+1, host));
			break;
		case 3:
			sprintf(s, "%02X %02X %02X  ", DebugReadMem(addr, host), DebugReadMem(addr+1, host), DebugReadMem(addr+2, host));
			break;
	}

	if (!host)
		sprintf(opstr + strlen(opstr), "            ");

	// Deal with 65C02 instructions
	if (!ip->c6502 || !host || MachineType==3)
	{
		sprintf(opstr + strlen(opstr), "%s ", ip->opn);
		addr++;

		switch(ip->nb)
		{
			case 1:
				l = 0;
				break;
			case 2:
				operand = DebugReadMem(addr, host);
				l = 2;
				break;
			case 3:
				operand = DebugReadMem(addr, host) + (DebugReadMem(addr+1, host) << 8);
				l = 4;
				break;
		}

		if (ip->flag & REL)
		{
			if (operand > 127) 
				operand = (~0xff | operand);
			operand = operand + ip->nb + addr - 1;
			l = 4;
		}

		s=opstr+strlen(opstr);

		switch (ip->flag & ADRMASK)
		{
		case IMM:
			sprintf(s, "#%0*X    ", l, operand);
			break;
		case REL:
		case ABS:
		case ZPG:
			sprintf(s, "%0*X     ", l, operand);
			break;
		case IND:
			sprintf(s, "(%0*X)   ", l, operand);
			break;
		case ABX:
		case ZPX:
			sprintf(s, "%0*X,X   ", l, operand);
			break;
		case ABY:
		case ZPY:
			sprintf(s, "%0*X,Y   ", l, operand);
			break;
		case INX:
			sprintf(s, "(%0*X,X) ", l, operand);
			break;
		case INY:
			sprintf(s, "(%0*X),Y ", l, operand);
			break;
		case ACC:
			sprintf(s, "A        ");
			break;
		case IMP:
		default:
			sprintf(s, "         ");
			break;
		}

		if (l == 2)
			sprintf(opstr + strlen(opstr), "  ");
	}
	else
	{
		sprintf(opstr + strlen(opstr), "???          ");
	}

	if (host)
		sprintf(opstr + strlen(opstr), "            ");

	return(ip->nb);
}

int DebugDisassembleCommand(int addr, int count, bool host)
{
	char opstr[80];
	int saddr = addr;

//	if (DebugSource == DEBUG_NONE)
//	{
//		DebugDisplayInfo("Cannot disassemble while code is executing."); // - why not?
//		return(0);
//	}

	if (count > MAX_LINES)
		count = MAX_LINES;

	while (count > 0 && addr <= 0xffff)
	{
		if ((TorchTube || AcornZ80) && !host)
		{
			int l;
			char *s;
			char buff[64];
			
			sprintf(opstr, "%04X ", addr);
			s = opstr + strlen(opstr);
			l = Z80_Disassemble(addr, buff);

			switch (l) {
				case 1:
					sprintf(s, "%02X           ", DebugReadMem(addr, host));
					break;
				case 2:
					sprintf(s, "%02X %02X        ", DebugReadMem(addr, host), DebugReadMem(addr+1, host));
					break;
				case 3:
					sprintf(s, "%02X %02X %02X     ", DebugReadMem(addr, host), DebugReadMem(addr+1, host), DebugReadMem(addr+2, host));
					break;
				case 4:
					sprintf(s, "%02X %02X %02X %02X  ", DebugReadMem(addr, host), DebugReadMem(addr+1, host), DebugReadMem(addr+2, host), DebugReadMem(addr+3, host));
					break;
			}
			
			strcat(opstr, buff);
			
			addr += l;
		}
		else
		{
			addr += DebugDisassembleInstruction(addr, host, opstr);
		}
		DebugDisplayInfo(opstr);
		count--;
	}

	return(addr - saddr);
}

void DebugMemoryDump(int addr, int count, bool host)
{
	int a, b;
	int s, e;
	int v;
	char info[80];

	if (count > MAX_LINES * 16)
		count = MAX_LINES * 16;

	s = addr & 0xfff0;
	e = (addr + count - 1) | 0xf;

	if (e > 0xffff)
		e = 0xffff;
	DebugDisplayInfo("       0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F 0123456789ABCDEF");
	for (a = s; a < e; a += 16)
	{
		sprintf(info, "%04X  ", a);

		if (host && a >= 0xfc00 && a < 0xff00)
		{
			sprintf(info+strlen(info), "IO space");
		}
		else
		{
			for (b = 0; b < 16; ++b)
			{
				if (!host && (a+b) >= 0xfef8 && (a+b) < 0xff00 && !(TorchTube || AcornZ80))
					sprintf(info+strlen(info), "IO ");
				else
					sprintf(info+strlen(info), "%02X ", DebugReadMem(a+b, host));
			}

			for (b = 0; b < 16; ++b)
			{
				if (host || (a+b) < 0xfef8 || (a+b) >= 0xff00)
				{
					v = DebugReadMem(a+b, host);
					if (v < 32 || v > 127)
						v = '.';
					sprintf(info+strlen(info), "%c", v);
				}
			}
		}

		DebugDisplayInfo(info);
	}

}
