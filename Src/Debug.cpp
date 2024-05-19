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
#include <stdarg.h>
#include <string.h>

#include <algorithm>
#include <deque>
#include <fstream>
#include <string>
#include <vector>

#include "Debug.h"
#include "6502core.h"
#include "BeebMem.h"
#include "DebugTrace.h"
#include "FileDialog.h"
#include "Main.h"
#include "Resource.h"
#include "Serial.h"
#include "StringUtils.h"
#include "Tube.h"
#include "Z80mem.h"
#include "Z80.h"

constexpr int MAX_LINES = 4096;          // Max lines in info window
constexpr int LINES_IN_INFO = 28;        // Visible lines in info window
constexpr int MAX_COMMAND_LEN = 200;     // Max debug command length
constexpr int MAX_BPS = 50;              // Max num of breakpoints/watches
constexpr int MAX_HISTORY = 20;          // Number of commands in the command history.

// Instruction format
constexpr int IMM = 0x20;
constexpr int ABS = 0x40;
constexpr int ACC = 0x80;
constexpr int IMP = 0x100;
constexpr int INX = 0x200;
constexpr int INY = 0x400;
constexpr int ZPX = 0x800;
constexpr int ABX = 0x1000;
constexpr int ABY = 0x2000;
constexpr int REL = 0x4000;
constexpr int IND = 0x8000;
constexpr int ZPY = 0x10000;
constexpr int ZPG = 0x20000;
constexpr int ZPR = 0x40000;
constexpr int ILL = 0x80000;

constexpr int ADRMASK = IMM | ABS | ACC | IMP | INX | INY | ZPX | ABX | ABY | REL | IND | ZPY | ZPG | ZPR | ILL;

constexpr int MAX_BUFFER = 65536;

bool DebugEnabled = false; // Debug dialog visible
static DebugType DebugSource = DebugType::None; // Debugging active?
static int LinesDisplayed = 0;  // Lines in info window
static int InstCount = 0;       // Instructions to execute before breaking
static int DumpAddress = 0;     // Next address for memory dump command
static int DisAddress = 0;      // Next address for disassemble command
static int LastBreakAddr = 0;   // Address of last break
static int DebugInfoWidth = 0;  // Width of debug info window
static bool BPSOn = true;
static bool BRKOn = false;
static bool StepOver = false;
static int ReturnAddress = 0;
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
static HWND hwndInvisibleOwner;
static HWND hwndInfo;
static HWND hwndBP;
static HWND hwndW;
static HACCEL haccelDebug;

static std::vector<Label> Labels;
static std::vector<Breakpoint> Breakpoints;
static std::vector<Watch> Watches;

typedef std::vector<AddrInfo> MemoryMap;

static MemoryMap MemoryMaps[17];

INT_PTR CALLBACK DebugDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam);

std::deque<std::string> DebugHistory;
int DebugHistoryIndex = 0;

static void DebugParseCommand(char *command);
static void DebugWriteMem(int addr, bool host, unsigned char data);
static int DebugDisassembleCommand(int addr, int count, bool host);
static void DebugMemoryDump(int addr, int count, bool host);
static void DebugExecuteCommand();
static void DebugToggleRun();
static void DebugUpdateWatches(bool all);
static bool DebugLookupAddress(int addr, AddrInfo* addrInfo);
static void DebugHistoryMove(int delta);
static void DebugHistoryAdd(char* command);
static void DebugSetCommandString(const char* str);
static void DebugChompString(char* str);

// Command handlers
static bool DebugCmdBreakContinue(char* args);
static bool DebugCmdToggleBreak(char* args);
static bool DebugCmdLabels(char* args);
static bool DebugCmdHelp(char* args);
static bool DebugCmdSet(char* args);
static bool DebugCmdNext(char* args);
static bool DebugCmdOver(char* args);
static bool DebugCmdPeek(char* args);
static bool DebugCmdCode(char* args);
static bool DebugCmdWatch(char* args);
static bool DebugCmdState(char* args);
static bool DebugCmdSave(char* args);
static bool DebugCmdPoke(char* args);
static bool DebugCmdGoto(char* args);
static bool DebugCmdFile(char* args);
static bool DebugCmdEcho(char* args);
static bool DebugCmdScript(char *args);
static bool DebugCmdClear(char *args);

// Debugger commands go here. Format is COMMAND, HANDLER, ARGSPEC, HELPSTRING
// Aliases are supported, put these below the command they reference and leave argspec/help
// empty.
static const DebugCmd DebugCmdTable[] = {
	{ "bp",         DebugCmdToggleBreak,   "start[-end] [name]", "Sets/Clears a breakpoint or break range." },
	{ "b",          DebugCmdToggleBreak,   "", ""}, // Alias of "bp"
	{ "breakpoint", DebugCmdToggleBreak,   "", ""}, // Alias of "bp"
	{ "labels",     DebugCmdLabels,        "load/show [filename]", "Loads labels from VICE file, or display known labels." },
	{ "l",          DebugCmdLabels,        "", ""}, // Alias of "labels"
	{ "help",       DebugCmdHelp,          "[command/addr]", "Displays help for the specified command or address." },
	{ "?",          DebugCmdHelp,          "", ""}, // Alias of "help"
	{ "q",          DebugCmdHelp,          "", ""}, // Alias of "help"
	{ "break",      DebugCmdBreakContinue, "", "Break/Continue." },
	{ ".",          DebugCmdBreakContinue, "",""}, // Alias of "break"
	{ "set",        DebugCmdSet,           "host/parasite/rom/os/endian/breakpoints/decimal/brk on/off", "Turns various UI checkboxes on or off." },
	{ "next",       DebugCmdNext,          "[count]", "Execute the specified number instructions, default 1." },
	{ "n",          DebugCmdNext,          "", ""}, // Alias of "next"
	{ "over",       DebugCmdOver,          "", "Step over JSR (host only)." },
	{ "o",          DebugCmdOver,          "", ""}, // Alias of "over"
	{ "peek",       DebugCmdPeek,          "[p] [start] [count]", "Dumps memory to console." },
	{ "m",          DebugCmdPeek,          "", ""}, // Alias of "peek"
	{ "code",       DebugCmdCode,          "[p] [start] [count]", "Disassembles specified range." },
	{ "d",          DebugCmdCode,          "", ""}, // Alias of "code"
	{ "watch",      DebugCmdWatch,         "[p] addr [b/w/d] [name]", "Sets/Clears a byte/word/dword watch at addr." },
	{ "e",          DebugCmdWatch,         "", ""}, // Alias of "watch"
	{ "state",      DebugCmdState,         "v/u/s/e/t/m/r", "Displays state of Video/UserVIA/SysVIA/Serial/Tube/Memory/Roms." },
	{ "s",          DebugCmdState,         "", ""}, // Alias of "state"
	{ "save",       DebugCmdSave,          "[count] [file]", "Writes console lines to file." },
	{ "w",          DebugCmdSave,          "", ""}, // Alias of "save"
	{ "poke",       DebugCmdPoke,          "[p] start byte [byte...]", "Write bytes to memory." },
	{ "c",          DebugCmdPoke,          "", ""}, // Alias of "poke"
	{ "goto",       DebugCmdGoto,          "[p] addr", "Jump to address." },
	{ "g",          DebugCmdGoto,          "", ""}, // Alias of "goto"
	{ "file",       DebugCmdFile,          "r/w addr [count] [filename]", "Read/Write memory at address from/to file." },
	{ "f",          DebugCmdFile,          "", ""}, // Alias of "file"
	{ "echo",       DebugCmdEcho,          "string", "Write string to console." },
	{ "!",          DebugCmdEcho,          "", "" }, // Alias of "echo"
	{ "script",     DebugCmdScript,        "[filename]", "Executes a debugger script." },
	{ "clear",      DebugCmdClear,         "", "Clears the console." }
};

static const InstInfo optable_6502[256] =
{
	{ "BRK",  1, IMP }, // 00
	{ "ORA",  2, INX }, // 01
	{ "KIL",  1, ILL }, // 02
	{ "SLO",  2, INX }, // 03
	{ "NOP",  2, ZPG }, // 04
	{ "ORA",  2, ZPG }, // 05
	{ "ASL",  2, ZPG }, // 06
	{ "SLO",  2, ZPG }, // 07
	{ "PHP",  1, IMP }, // 08
	{ "ORA",  2, IMM }, // 09
	{ "ASL",  1, ACC }, // 0a
	{ "ANC",  2, IMM }, // 0b
	{ "NOP",  3, ABS }, // 0c
	{ "ORA",  3, ABS }, // 0d
	{ "ASL",  3, ABS }, // 0e
	{ "SLO",  3, ABS }, // 0f
	{ "BPL",  2, REL }, // 10
	{ "ORA",  2, INY }, // 11
	{ "KIL",  1, ILL }, // 12
	{ "SLO",  2, INY }, // 13
	{ "NOP",  2, ZPX }, // 14
	{ "ORA",  2, ZPX }, // 15
	{ "ASL",  2, ZPX }, // 16
	{ "SLO",  2, ZPX }, // 17
	{ "CLC",  1, IMP }, // 18
	{ "ORA",  3, ABY }, // 19
	{ "NOP",  1, IMP }, // 1a
	{ "SLO",  3, ABY }, // 1b
	{ "NOP",  3, ABX }, // 1c
	{ "ORA",  3, ABX }, // 1d
	{ "ASL",  3, ABX }, // 1e
	{ "SLO",  3, ABX }, // 1f
	{ "JSR",  3, ABS }, // 20
	{ "AND",  2, INX }, // 21
	{ "KIL",  1, ILL }, // 22
	{ "RLA",  2, INX }, // 23
	{ "BIT",  2, ZPG }, // 24
	{ "AND",  2, ZPG }, // 25
	{ "ROL",  2, ZPG }, // 26
	{ "RLA",  2, ZPG }, // 27
	{ "PLP",  1, IMP }, // 28
	{ "AND",  2, IMM }, // 29
	{ "ROL",  1, ACC }, // 2a
	{ "ANC",  2, IMM }, // 2b
	{ "BIT",  3, ABS }, // 2c
	{ "AND",  3, ABS }, // 2d
	{ "ROL",  3, ABS }, // 2e
	{ "RLA",  3, ABS }, // 2f
	{ "BMI",  2, REL }, // 30
	{ "AND",  2, INY }, // 31
	{ "KIL",  1, ILL }, // 32
	{ "RLA",  2, INY }, // 33
	{ "NOP",  2, ZPX }, // 34
	{ "AND",  2, ZPX }, // 35
	{ "ROL",  2, ZPX }, // 36
	{ "RLA",  2, ZPX }, // 37
	{ "SEC",  1, IMP }, // 38
	{ "AND",  3, ABY }, // 39
	{ "NOP",  1, IMP }, // 3a
	{ "RLA",  3, ABY }, // 3b
	{ "NOP",  3, ABX }, // 3c
	{ "AND",  3, ABX }, // 3d
	{ "ROL",  3, ABX }, // 3e
	{ "RLA",  3, ABX }, // 3f
	{ "RTI",  1, IMP }, // 40
	{ "EOR",  2, INX }, // 41
	{ "KIL",  1, ILL }, // 42
	{ "SRE",  2, INX }, // 43
	{ "NOP",  2, ZPG }, // 44
	{ "EOR",  2, ZPG }, // 45
	{ "LSR",  2, ZPG }, // 46
	{ "SRE",  2, ZPG }, // 47
	{ "PHA",  1, IMP }, // 48
	{ "EOR",  2, IMM }, // 49
	{ "LSR",  1, ACC }, // 4a
	{ "ALR",  2, IMM }, // 4b
	{ "JMP",  3, ABS }, // 4c
	{ "EOR",  3, ABS }, // 4d
	{ "LSR",  3, ABS }, // 4e
	{ "SRE",  3, ABS }, // 4f
	{ "BVC",  2, REL }, // 50
	{ "EOR",  2, INY }, // 51
	{ "KIL",  1, ILL }, // 52
	{ "SRE",  2, INY }, // 53
	{ "NOP",  2, ZPX }, // 54
	{ "EOR",  2, ZPX }, // 55
	{ "LSR",  2, ZPX }, // 56
	{ "SRE",  2, ZPX }, // 57
	{ "CLI",  1, IMP }, // 58
	{ "EOR",  3, ABY }, // 59
	{ "NOP",  1, IMP }, // 5a
	{ "SRE",  3, ABY }, // 5b
	{ "NOP",  3, ABX }, // 5c
	{ "EOR",  3, ABX }, // 5d
	{ "LSR",  3, ABX }, // 5e
	{ "SRE",  3, ABX }, // 5f
	{ "RTS",  1, IMP }, // 60
	{ "ADC",  2, INX }, // 61
	{ "KIL",  1, ILL }, // 62
	{ "RRA",  2, INX }, // 63
	{ "NOP",  2, ZPG }, // 64
	{ "ADC",  2, ZPG }, // 65
	{ "ROR",  2, ZPG }, // 66
	{ "RRA",  2, ZPG }, // 67
	{ "PLA",  1, IMP }, // 68
	{ "ADC",  2, IMM }, // 69
	{ "ROR",  1, ACC }, // 6a
	{ "ARR",  2, IMM }, // 6b
	{ "JMP",  3, IND }, // 6c
	{ "ADC",  3, ABS }, // 6d
	{ "ROR",  3, ABS }, // 6e
	{ "RRA",  3, ABS }, // 6f
	{ "BVS",  2, REL }, // 70
	{ "ADC",  2, INY }, // 71
	{ "KIL",  1, ILL }, // 72
	{ "RRA",  2, INY }, // 73
	{ "NOP",  2, ZPX }, // 74
	{ "ADC",  2, ZPX }, // 75
	{ "ROR",  2, ZPX }, // 76
	{ "RRA",  2, ZPX }, // 77
	{ "SEI",  1, IMP }, // 78
	{ "ADC",  3, ABY }, // 79
	{ "NOP",  1, IMP }, // 7a
	{ "RRA",  3, ABY }, // 7b
	{ "NOP",  3, ABX }, // 7c
	{ "ADC",  3, ABX }, // 7d
	{ "ROR",  3, ABX }, // 7e
	{ "RRA",  3, ABX }, // 7f
	{ "NOP",  2, IMM }, // 80
	{ "STA",  2, INX }, // 81
	{ "NOP",  2, IMM }, // 82
	{ "SAX",  2, INX }, // 83
	{ "STY",  2, ZPG }, // 84
	{ "STA",  2, ZPG }, // 85
	{ "STX",  2, ZPG }, // 86
	{ "SAX",  2, ZPG }, // 87
	{ "DEY",  1, IMP }, // 88
	{ "NOP",  2, IMM }, // 89
	{ "TXA",  1, IMP }, // 8a
	{ "XAA",  2, IMM }, // 8b
	{ "STY",  3, ABS }, // 8c
	{ "STA",  3, ABS }, // 8d
	{ "STX",  3, ABS }, // 8e
	{ "SAX",  3, ABS }, // 8f
	{ "BCC",  2, REL }, // 90
	{ "STA",  2, INY }, // 91
	{ "KIL",  1, ILL }, // 92
	{ "AHX",  2, INY }, // 93
	{ "STY",  2, ZPX }, // 94
	{ "STA",  2, ZPX }, // 95
	{ "STX",  2, ZPY }, // 96
	{ "SAX",  2, ZPY }, // 97
	{ "TYA",  1, IMP }, // 98
	{ "STA",  3, ABY }, // 99
	{ "TXS",  1, IMP }, // 9a
	{ "TAS",  3, ABY }, // 9b
	{ "SHY",  3, ABX }, // 9c
	{ "STA",  3, ABX }, // 9d
	{ "SHX",  3, ABY }, // 9e
	{ "AHX",  3, ABY }, // 9f
	{ "LDY",  2, IMM }, // a0
	{ "LDA",  2, INX }, // a1
	{ "LDX",  2, IMM }, // a2
	{ "LAX",  2, INX }, // a3
	{ "LDY",  2, ZPG }, // a4
	{ "LDA",  2, ZPG }, // a5
	{ "LDX",  2, ZPG }, // a6
	{ "LAX",  2, ZPG }, // a7
	{ "TAY",  1, IMP }, // a8
	{ "LDA",  2, IMM }, // a9
	{ "TAX",  1, IMP }, // aa
	{ "LAX",  2, IMM }, // ab
	{ "LDY",  3, ABS }, // ac
	{ "LDA",  3, ABS }, // ad
	{ "LDX",  3, ABS }, // ae
	{ "LAX",  3, ABS }, // af
	{ "BCS",  2, REL }, // b0
	{ "LDA",  2, INY }, // b1
	{ "KIL",  1, ILL }, // b2
	{ "LAX",  2, INY }, // b3
	{ "LDY",  2, ZPX }, // b4
	{ "LDA",  2, ZPX }, // b5
	{ "LDX",  2, ZPY }, // b6
	{ "LAX",  2, ZPY }, // b7
	{ "CLV",  1, IMP }, // b8
	{ "LDA",  3, ABY }, // b9
	{ "TSX",  1, IMP }, // ba
	{ "LAS",  3, ABY }, // bb
	{ "LDY",  3, ABX }, // bc
	{ "LDA",  3, ABX }, // bd
	{ "LDX",  3, ABY }, // be
	{ "LAX",  3, ABY }, // bf
	{ "CPY",  2, IMM }, // c0
	{ "CMP",  2, INX }, // c1
	{ "NOP",  2, IMM }, // c2
	{ "DCP",  2, INX }, // c3
	{ "CPY",  2, ZPG }, // c4
	{ "CMP",  2, ZPG }, // c5
	{ "DEC",  2, ZPG }, // c6
	{ "DCP",  2, ZPG }, // c7
	{ "INY",  1, IMP }, // c8
	{ "CMP",  2, IMM }, // c9
	{ "DEX",  1, IMP }, // ca
	{ "AXS",  2, IMM }, // cb
	{ "CPY",  3, ABS }, // cc
	{ "CMP",  3, ABS }, // cd
	{ "DEC",  3, ABS }, // ce
	{ "DCP",  3, ABS }, // cf
	{ "BNE",  2, REL }, // d0
	{ "CMP",  2, INY }, // d1
	{ "KIL",  1, ILL }, // d2
	{ "DCP",  2, INY }, // d3
	{ "NOP",  2, ZPX }, // d4
	{ "CMP",  2, ZPX }, // d5
	{ "DEC",  2, ZPX }, // d6
	{ "DCP",  2, ZPX }, // d7
	{ "CLD",  1, IMP }, // d8
	{ "CMP",  3, ABY }, // d9
	{ "NOP",  1, IMP }, // da
	{ "DCP",  3, ABY }, // db
	{ "NOP",  3, ABX }, // dc
	{ "CMP",  3, ABX }, // dd
	{ "DEC",  3, ABX }, // de
	{ "DCP",  3, ABX }, // df
	{ "CPX",  2, IMM }, // e0
	{ "SBC",  2, INX }, // e1
	{ "NOP",  2, IMM }, // e2
	{ "ISC",  2, INX }, // e3
	{ "CPX",  2, ZPG }, // e4
	{ "SBC",  2, ZPG }, // e5
	{ "INC",  2, ZPG }, // e6
	{ "ISC",  2, ZPG }, // e7
	{ "INX",  1, IMP }, // e8
	{ "SBC",  2, IMM }, // e9
	{ "NOP",  1, IMP }, // ea
	{ "SBC",  2, IMM }, // eb
	{ "CPX",  3, ABS }, // ec
	{ "SBC",  3, ABS }, // ed
	{ "INC",  3, ABS }, // ee
	{ "ISC",  3, ABS }, // ef
	{ "BEQ",  2, REL }, // f0
	{ "SBC",  2, INY }, // f1
	{ "KIL",  1, ILL }, // f2
	{ "ISC",  2, INY }, // f3
	{ "NOP",  2, ZPX }, // f4
	{ "SBC",  2, ZPX }, // f5
	{ "INC",  2, ZPX }, // f6
	{ "ISC",  2, ZPX }, // f7
	{ "SED",  1, IMP }, // f8
	{ "SBC",  3, ABY }, // f9
	{ "NOP",  1, IMP }, // fa
	{ "ISC",  3, ABY }, // fb
	{ "NOP",  3, ABX }, // fc
	{ "SBC",  3, ABX }, // fd
	{ "INC",  3, ABX }, // fe
	{ "ISC",  3, ABX }  // ff
};

static const InstInfo optable_65c02[256] =
{
	{ "BRK",  1, IMP }, // 00
	{ "ORA",  2, INX }, // 01
	{ "NOP",  2, IMM }, // 02
	{ "NOP",  1, IMP }, // 03
	{ "TSB",  2, ZPG }, // 04
	{ "ORA",  2, ZPG }, // 05
	{ "ASL",  2, ZPG }, // 06
	{ "RMB0", 2, ZPG }, // 07
	{ "PHP",  1, IMP }, // 08
	{ "ORA",  2, IMM }, // 09
	{ "ASL",  1, ACC }, // 0a
	{ "NOP",  1, IMP }, // 0b
	{ "TSB",  3, ABS }, // 0c
	{ "ORA",  3, ABS }, // 0d
	{ "ASL",  3, ABS }, // 0e
	{ "BBR0", 3, ZPR }, // 0f
	{ "BPL",  2, REL }, // 10
	{ "ORA",  2, INY }, // 11
	{ "ORA",  2, IND }, // 12
	{ "NOP",  1, IMP }, // 13
	{ "TRB",  2, ZPG }, // 14
	{ "ORA",  2, ZPX }, // 15
	{ "ASL",  2, ZPX }, // 16
	{ "RMB1", 2, ZPG }, // 17
	{ "CLC",  1, IMP }, // 18
	{ "ORA",  3, ABY }, // 19
	{ "INC",  1, ACC }, // 1a
	{ "NOP",  1, IMP }, // 1b
	{ "TRB",  3, ABS }, // 1c
	{ "ORA",  3, ABX }, // 1d
	{ "ASL",  3, ABX }, // 1e
	{ "BBR1", 3, ZPR }, // 1f
	{ "JSR",  3, ABS }, // 20
	{ "AND",  2, INX }, // 21
	{ "NOP",  2, IMM }, // 22
	{ "NOP",  1, IMP }, // 23
	{ "BIT",  2, ZPG }, // 24
	{ "AND",  2, ZPG }, // 25
	{ "ROL",  2, ZPG }, // 26
	{ "RMB2", 2, ZPG }, // 27
	{ "PLP",  1, IMP }, // 28
	{ "AND",  2, IMM }, // 29
	{ "ROL",  1, ACC }, // 2a
	{ "NOP",  1, IMP }, // 2b
	{ "BIT",  3, ABS }, // 2c
	{ "AND",  3, ABS }, // 2d
	{ "ROL",  3, ABS }, // 2e
	{ "BBR2", 3, ZPR }, // 2f
	{ "BMI",  2, REL }, // 30
	{ "AND",  2, INY }, // 31
	{ "AND",  2, IND }, // 32
	{ "NOP",  1, IMP }, // 33
	{ "BIT",  2, ZPX }, // 34
	{ "AND",  2, ZPX }, // 35
	{ "ROL",  2, ZPX }, // 36
	{ "RMB3", 2, ZPG }, // 37
	{ "SEC",  1, IMP }, // 38
	{ "AND",  3, ABY }, // 39
	{ "DEC",  1, ACC }, // 3a
	{ "NOP",  1, IMP }, // 3b
	{ "BIT",  3, ABX }, // 3c
	{ "AND",  3, ABX }, // 3d
	{ "ROL",  3, ABX }, // 3e
	{ "BBR3", 3, ZPR }, // 3f
	{ "RTI",  1, IMP }, // 40
	{ "EOR",  2, INX }, // 41
	{ "NOP",  2, IMM }, // 42
	{ "NOP",  1, IMP }, // 43
	{ "NOP",  2, ZPG }, // 44
	{ "EOR",  2, ZPG }, // 45
	{ "LSR",  2, ZPG }, // 46
	{ "RMB4", 2, ZPG }, // 47
	{ "PHA",  1, IMP }, // 48
	{ "EOR",  2, IMM }, // 49
	{ "LSR",  1, ACC }, // 4a
	{ "NOP",  1, IMP }, // 4b
	{ "JMP",  3, ABS }, // 4c
	{ "EOR",  3, ABS }, // 4d
	{ "LSR",  3, ABS }, // 4e
	{ "BBR4", 3, ZPR }, // 4f
	{ "BVC",  2, REL }, // 50
	{ "EOR",  2, INY }, // 51
	{ "EOR",  2, IND }, // 52
	{ "NOP",  1, IMP }, // 53
	{ "NOP",  2, ZPX }, // 54
	{ "EOR",  2, ZPX }, // 55
	{ "LSR",  2, ZPX }, // 56
	{ "RMB5", 2, ZPG }, // 57
	{ "CLI",  1, IMP }, // 58
	{ "EOR",  3, ABY }, // 59
	{ "PHY",  1, IMP }, // 5a
	{ "NOP",  1, IMP }, // 5b
	{ "NOP",  3, ABS }, // 5c
	{ "EOR",  3, ABX }, // 5d
	{ "LSR",  3, ABX }, // 5e
	{ "BBR5", 3, ZPR }, // 5f
	{ "RTS",  1, IMP }, // 60
	{ "ADC",  2, INX }, // 61
	{ "NOP",  2, IMM }, // 62
	{ "NOP",  1, IMP }, // 63
	{ "STZ",  2, ZPG }, // 64
	{ "ADC",  2, ZPG }, // 65
	{ "ROR",  2, ZPG }, // 66
	{ "RMB6", 2, ZPG }, // 67
	{ "PLA",  1, IMP }, // 68
	{ "ADC",  2, IMM }, // 69
	{ "ROR",  1, ACC }, // 6a
	{ "NOP",  1, IMP }, // 6b
	{ "JMP",  3, IND }, // 6c
	{ "ADC",  3, ABS }, // 6d
	{ "ROR",  3, ABS }, // 6e
	{ "BBR6", 3, ZPR }, // 6f
	{ "BVS",  2, REL }, // 70
	{ "ADC",  2, INY }, // 71
	{ "ADC",  2, IND }, // 72
	{ "NOP",  1, IMP }, // 73
	{ "STZ",  2, ZPX }, // 74
	{ "ADC",  2, ZPX }, // 75
	{ "ROR",  2, ZPX }, // 76
	{ "RMB7", 2, ZPG }, // 77
	{ "SEI",  1, IMP }, // 78
	{ "ADC",  3, ABY }, // 79
	{ "PLY",  1, IMP }, // 7a
	{ "NOP",  1, IMP }, // 7b
	{ "JMP",  3, INX }, // 7c
	{ "ADC",  3, ABX }, // 7d
	{ "ROR",  3, ABX }, // 7e
	{ "BBR7", 3, ZPR }, // 7f
	{ "BRA",  2, REL }, // 80
	{ "STA",  2, INX }, // 81
	{ "NOP",  2, IMM }, // 82
	{ "NOP",  1, IMP }, // 83
	{ "STY",  2, ZPG }, // 84
	{ "STA",  2, ZPG }, // 85
	{ "STX",  2, ZPG }, // 86
	{ "SMB0", 2, ZPG }, // 87
	{ "DEY",  1, IMP }, // 88
	{ "BIT",  2, IMM }, // 89
	{ "TXA",  1, IMP }, // 8a
	{ "NOP",  1, IMP }, // 8b
	{ "STY",  3, ABS }, // 8c
	{ "STA",  3, ABS }, // 8d
	{ "STX",  3, ABS }, // 8e
	{ "BBS0", 3, ZPR }, // 8f
	{ "BCC",  2, REL }, // 90
	{ "STA",  2, INY }, // 91
	{ "STA",  2, IND }, // 92
	{ "NOP",  1, IMP }, // 93
	{ "STY",  2, ZPX }, // 94
	{ "STA",  2, ZPX }, // 95
	{ "STX",  2, ZPY }, // 96
	{ "SMB1", 2, ZPG }, // 97
	{ "TYA",  1, IMP }, // 98
	{ "STA",  3, ABY }, // 99
	{ "TXS",  1, IMP }, // 9a
	{ "NOP",  1, IMP }, // 9b
	{ "STZ",  3, ABS }, // 9c
	{ "STA",  3, ABX }, // 9d
	{ "STZ",  3, ABX }, // 9e
	{ "BBS1", 3, ZPR }, // 9f
	{ "LDY",  2, IMM }, // a0
	{ "LDA",  2, INX }, // a1
	{ "LDX",  2, IMM }, // a2
	{ "NOP",  1, IMP }, // a3
	{ "LDY",  2, ZPG }, // a4
	{ "LDA",  2, ZPG }, // a5
	{ "LDX",  2, ZPG }, // a6
	{ "SMB2", 2, ZPG }, // a7
	{ "TAY",  1, IMP }, // a8
	{ "LDA",  2, IMM }, // a9
	{ "TAX",  1, IMP }, // aa
	{ "NOP",  1, IMP }, // ab
	{ "LDY",  3, ABS }, // ac
	{ "LDA",  3, ABS }, // ad
	{ "LDX",  3, ABS }, // ae
	{ "BBS2", 3, ZPR }, // af
	{ "BCS",  2, REL }, // b0
	{ "LDA",  2, INY }, // b1
	{ "LDA",  2, IND }, // b2
	{ "NOP",  1, IMP }, // b3
	{ "LDY",  2, ZPX }, // b4
	{ "LDA",  2, ZPX }, // b5
	{ "LDX",  2, ZPY }, // b6
	{ "SMB3", 2, ZPG }, // b7
	{ "CLV",  1, IMP }, // b8
	{ "LDA",  3, ABY }, // b9
	{ "TSX",  1, IMP }, // ba
	{ "NOP",  1, IMP }, // bb
	{ "LDY",  3, ABX }, // bc
	{ "LDA",  3, ABX }, // bd
	{ "LDX",  3, ABY }, // be
	{ "BBS3", 3, ZPR }, // bf
	{ "CPY",  2, IMM }, // c0
	{ "CMP",  2, INX }, // c1
	{ "NOP",  2, IMM }, // c2
	{ "NOP",  1, IMP }, // c3
	{ "CPY",  2, ZPG }, // c4
	{ "CMP",  2, ZPG }, // c5
	{ "DEC",  2, ZPG }, // c6
	{ "SMB4", 2, ZPG }, // c7
	{ "INY",  1, IMP }, // c8
	{ "CMP",  2, IMM }, // c9
	{ "DEX",  1, IMP }, // ca
	{ "NOP",  1, IMP }, // cb
	{ "CPY",  3, ABS }, // cc
	{ "CMP",  3, ABS }, // cd
	{ "DEC",  3, ABS }, // ce
	{ "BBS4", 3, ZPR }, // cf
	{ "BNE",  2, REL }, // d0
	{ "CMP",  2, INY }, // d1
	{ "CMP",  2, IND }, // d2
	{ "NOP",  1, IMP }, // d3
	{ "NOP",  2, ZPX }, // d4
	{ "CMP",  2, ZPX }, // d5
	{ "DEC",  2, ZPX }, // d6
	{ "SMB5", 2, ZPG }, // d7
	{ "CLD",  1, IMP }, // d8
	{ "CMP",  3, ABY }, // d9
	{ "PHX",  1, IMP }, // da
	{ "NOP",  1, IMP }, // db
	{ "NOP",  3, ABS }, // dc
	{ "CMP",  3, ABX }, // dd
	{ "DEC",  3, ABX }, // de
	{ "BBS5", 3, ZPR }, // df
	{ "CPX",  2, IMM }, // e0
	{ "SBC",  2, INX }, // e1
	{ "NOP",  2, IMM }, // e2
	{ "NOP",  1, IMP }, // e3
	{ "CPX",  2, ZPG }, // e4
	{ "SBC",  2, ZPG }, // e5
	{ "INC",  2, ZPG }, // e6
	{ "SMB6", 2, ZPG }, // e7
	{ "INX",  1, IMP }, // e8
	{ "SBC",  2, IMM }, // e9
	{ "NOP",  1, IMP }, // ea
	{ "NOP",  1, IMP }, // eb
	{ "CPX",  3, ABS }, // ec
	{ "SBC",  3, ABS }, // ed
	{ "INC",  3, ABS }, // ee
	{ "BBS6", 3, ZPR }, // ef
	{ "BEQ",  2, REL }, // f0
	{ "SBC",  2, INY }, // f1
	{ "SBC",  2, IND }, // f2
	{ "NOP",  1, IMP }, // f3
	{ "NOP",  2, ZPX }, // f4
	{ "SBC",  2, ZPX }, // f5
	{ "INC",  2, ZPX }, // f6
	{ "SMB7", 2, ZPG }, // f7
	{ "SED",  1, IMP }, // f8
	{ "SBC",  3, ABY }, // f9
	{ "PLX",  1, IMP }, // fa
	{ "NOP",  1, IMP }, // fb
	{ "NOP",  3, ABS }, // fc
	{ "SBC",  3, ABX }, // fd
	{ "INC",  3, ABX }, // fe
	{ "BBS7", 3, ZPR }  // ff
};

// Same as 65c02 but without BBRx and RMBx instructions

static const InstInfo optable_65sc12[256] =
{
	{ "BRK",  1, IMP }, // 00
	{ "ORA",  2, INX }, // 01
	{ "NOP",  2, IMM }, // 02
	{ "NOP",  1, IMP }, // 03
	{ "TSB",  2, ZPG }, // 04
	{ "ORA",  2, ZPG }, // 05
	{ "ASL",  2, ZPG }, // 06
	{ "NOP",  1, IMP }, // 07
	{ "PHP",  1, IMP }, // 08
	{ "ORA",  2, IMM }, // 09
	{ "ASL",  1, ACC }, // 0a
	{ "NOP",  1, IMP }, // 0b
	{ "TSB",  3, ABS }, // 0c
	{ "ORA",  3, ABS }, // 0d
	{ "ASL",  3, ABS }, // 0e
	{ "NOP",  1, IMP }, // 0f
	{ "BPL",  2, REL }, // 10
	{ "ORA",  2, INY }, // 11
	{ "ORA",  2, IND }, // 12
	{ "NOP",  1, IMP }, // 13
	{ "TRB",  2, ZPG }, // 14
	{ "ORA",  2, ZPX }, // 15
	{ "ASL",  2, ZPX }, // 16
	{ "NOP",  1, IMP }, // 17
	{ "CLC",  1, IMP }, // 18
	{ "ORA",  3, ABY }, // 19
	{ "INC",  1, ACC }, // 1a
	{ "NOP",  1, IMP }, // 1b
	{ "TRB",  3, ABS }, // 1c
	{ "ORA",  3, ABX }, // 1d
	{ "ASL",  3, ABX }, // 1e
	{ "NOP",  1, IMP }, // 1f
	{ "JSR",  3, ABS }, // 20
	{ "AND",  2, INX }, // 21
	{ "NOP",  2, IMM }, // 22
	{ "NOP",  1, IMP }, // 23
	{ "BIT",  2, ZPG }, // 24
	{ "AND",  2, ZPG }, // 25
	{ "ROL",  2, ZPG }, // 26
	{ "NOP",  1, IMP }, // 27
	{ "PLP",  1, IMP }, // 28
	{ "AND",  2, IMM }, // 29
	{ "ROL",  1, ACC }, // 2a
	{ "NOP",  1, IMP }, // 2b
	{ "BIT",  3, ABS }, // 2c
	{ "AND",  3, ABS }, // 2d
	{ "ROL",  3, ABS }, // 2e
	{ "NOP",  1, IMP }, // 2f
	{ "BMI",  2, REL }, // 30
	{ "AND",  2, INY }, // 31
	{ "AND",  2, IND }, // 32
	{ "NOP",  1, IMP }, // 33
	{ "BIT",  2, ZPX }, // 34
	{ "AND",  2, ZPX }, // 35
	{ "ROL",  2, ZPX }, // 36
	{ "NOP",  1, IMP }, // 37
	{ "SEC",  1, IMP }, // 38
	{ "AND",  3, ABY }, // 39
	{ "DEC",  1, ACC }, // 3a
	{ "NOP",  1, IMP }, // 3b
	{ "BIT",  3, ABX }, // 3c
	{ "AND",  3, ABX }, // 3d
	{ "ROL",  3, ABX }, // 3e
	{ "NOP",  1, IMP }, // 3f
	{ "RTI",  1, IMP }, // 40
	{ "EOR",  2, INX }, // 41
	{ "NOP",  2, IMM }, // 42
	{ "NOP",  1, IMP }, // 43
	{ "NOP",  2, ZPG }, // 44
	{ "EOR",  2, ZPG }, // 45
	{ "LSR",  2, ZPG }, // 46
	{ "NOP",  1, IMP }, // 47
	{ "PHA",  1, IMP }, // 48
	{ "EOR",  2, IMM }, // 49
	{ "LSR",  1, ACC }, // 4a
	{ "NOP",  1, IMP }, // 4b
	{ "JMP",  3, ABS }, // 4c
	{ "EOR",  3, ABS }, // 4d
	{ "LSR",  3, ABS }, // 4e
	{ "NOP",  1, IMP }, // 4f
	{ "BVC",  2, REL }, // 50
	{ "EOR",  2, INY }, // 51
	{ "EOR",  2, IND }, // 52
	{ "NOP",  1, IMP }, // 53
	{ "NOP",  2, ZPX }, // 54
	{ "EOR",  2, ZPX }, // 55
	{ "LSR",  2, ZPX }, // 56
	{ "NOP",  1, IMP }, // 57
	{ "CLI",  1, IMP }, // 58
	{ "EOR",  3, ABY }, // 59
	{ "PHY",  1, IMP }, // 5a
	{ "NOP",  1, IMP }, // 5b
	{ "NOP",  3, ABS }, // 5c
	{ "EOR",  3, ABX }, // 5d
	{ "LSR",  3, ABX }, // 5e
	{ "NOP",  1, IMP }, // 5f
	{ "RTS",  1, IMP }, // 60
	{ "ADC",  2, INX }, // 61
	{ "NOP",  2, IMM }, // 62
	{ "NOP",  1, IMP }, // 63
	{ "STZ",  2, ZPG }, // 64
	{ "ADC",  2, ZPG }, // 65
	{ "ROR",  2, ZPG }, // 66
	{ "NOP",  1, IMP }, // 67
	{ "PLA",  1, IMP }, // 68
	{ "ADC",  2, IMM }, // 69
	{ "ROR",  1, ACC }, // 6a
	{ "NOP",  1, IMP }, // 6b
	{ "JMP",  3, IND }, // 6c
	{ "ADC",  3, ABS }, // 6d
	{ "ROR",  3, ABS }, // 6e
	{ "NOP",  1, IMP }, // 6f
	{ "BVS",  2, REL }, // 70
	{ "ADC",  2, INY }, // 71
	{ "ADC",  2, IND }, // 72
	{ "NOP",  1, IMP }, // 73
	{ "STZ",  2, ZPX }, // 74
	{ "ADC",  2, ZPX }, // 75
	{ "ROR",  2, ZPX }, // 76
	{ "NOP",  1, IMP }, // 77
	{ "SEI",  1, IMP }, // 78
	{ "ADC",  3, ABY }, // 79
	{ "PLY",  1, IMP }, // 7a
	{ "NOP",  1, IMP }, // 7b
	{ "JMP",  3, INX }, // 7c
	{ "ADC",  3, ABX }, // 7d
	{ "ROR",  3, ABX }, // 7e
	{ "NOP",  1, IMP }, // 7f
	{ "BRA",  2, REL }, // 80
	{ "STA",  2, INX }, // 81
	{ "NOP",  2, IMM }, // 82
	{ "NOP",  1, IMP }, // 83
	{ "STY",  2, ZPG }, // 84
	{ "STA",  2, ZPG }, // 85
	{ "STX",  2, ZPG }, // 86
	{ "NOP",  1, IMP }, // 87
	{ "DEY",  1, IMP }, // 88
	{ "BIT",  2, IMM }, // 89
	{ "TXA",  1, IMP }, // 8a
	{ "NOP",  1, IMP }, // 8b
	{ "STY",  3, ABS }, // 8c
	{ "STA",  3, ABS }, // 8d
	{ "STX",  3, ABS }, // 8e
	{ "NOP",  1, IMP }, // 8f
	{ "BCC",  2, REL }, // 90
	{ "STA",  2, INY }, // 91
	{ "STA",  2, IND }, // 92
	{ "NOP",  1, IMP }, // 93
	{ "STY",  2, ZPX }, // 94
	{ "STA",  2, ZPX }, // 95
	{ "STX",  2, ZPY }, // 96
	{ "NOP",  1, IMP }, // 97
	{ "TYA",  1, IMP }, // 98
	{ "STA",  3, ABY }, // 99
	{ "TXS",  1, IMP }, // 9a
	{ "NOP",  1, IMP }, // 9b
	{ "STZ",  3, ABS }, // 9c
	{ "STA",  3, ABX }, // 9d
	{ "STZ",  3, ABX }, // 9e
	{ "NOP",  1, IMP }, // 9f
	{ "LDY",  2, IMM }, // a0
	{ "LDA",  2, INX }, // a1
	{ "LDX",  2, IMM }, // a2
	{ "NOP",  1, IMP }, // a3
	{ "LDY",  2, ZPG }, // a4
	{ "LDA",  2, ZPG }, // a5
	{ "LDX",  2, ZPG }, // a6
	{ "NOP",  1, IMP }, // a7
	{ "TAY",  1, IMP }, // a8
	{ "LDA",  2, IMM }, // a9
	{ "TAX",  1, IMP }, // aa
	{ "NOP",  1, IMP }, // ab
	{ "LDY",  3, ABS }, // ac
	{ "LDA",  3, ABS }, // ad
	{ "LDX",  3, ABS }, // ae
	{ "NOP",  1, IMP }, // af
	{ "BCS",  2, REL }, // b0
	{ "LDA",  2, INY }, // b1
	{ "LDA",  2, IND }, // b2
	{ "NOP",  1, IMP }, // b3
	{ "LDY",  2, ZPX }, // b4
	{ "LDA",  2, ZPX }, // b5
	{ "LDX",  2, ZPY }, // b6
	{ "NOP",  1, IMP }, // b7
	{ "CLV",  1, IMP }, // b8
	{ "LDA",  3, ABY }, // b9
	{ "TSX",  1, IMP }, // ba
	{ "NOP",  1, IMP }, // bb
	{ "LDY",  3, ABX }, // bc
	{ "LDA",  3, ABX }, // bd
	{ "LDX",  3, ABY }, // be
	{ "NOP",  1, IMP }, // bf
	{ "CPY",  2, IMM }, // c0
	{ "CMP",  2, INX }, // c1
	{ "NOP",  2, IMM }, // c2
	{ "NOP",  1, IMP }, // c3
	{ "CPY",  2, ZPG }, // c4
	{ "CMP",  2, ZPG }, // c5
	{ "DEC",  2, ZPG }, // c6
	{ "NOP",  1, IMP }, // c7
	{ "INY",  1, IMP }, // c8
	{ "CMP",  2, IMM }, // c9
	{ "DEX",  1, IMP }, // ca
	{ "NOP",  1, IMP }, // cb
	{ "CPY",  3, ABS }, // cc
	{ "CMP",  3, ABS }, // cd
	{ "DEC",  3, ABS }, // ce
	{ "NOP",  1, IMP }, // cf
	{ "BNE",  2, REL }, // d0
	{ "CMP",  2, INY }, // d1
	{ "CMP",  2, IND }, // d2
	{ "NOP",  1, IMP }, // d3
	{ "NOP",  2, ZPX }, // d4
	{ "CMP",  2, ZPX }, // d5
	{ "DEC",  2, ZPX }, // d6
	{ "NOP",  1, IMP }, // d7
	{ "CLD",  1, IMP }, // d8
	{ "CMP",  3, ABY }, // d9
	{ "PHX",  1, IMP }, // da
	{ "NOP",  1, IMP }, // db
	{ "NOP",  3, ABS }, // dc
	{ "CMP",  3, ABX }, // dd
	{ "DEC",  3, ABX }, // de
	{ "NOP",  1, IMP }, // df
	{ "CPX",  2, IMM }, // e0
	{ "SBC",  2, INX }, // e1
	{ "NOP",  2, IMM }, // e2
	{ "NOP",  1, IMP }, // e3
	{ "CPX",  2, ZPG }, // e4
	{ "SBC",  2, ZPG }, // e5
	{ "INC",  2, ZPG }, // e6
	{ "NOP",  1, IMP }, // e7
	{ "INX",  1, IMP }, // e8
	{ "SBC",  2, IMM }, // e9
	{ "NOP",  1, IMP }, // ea
	{ "NOP",  1, IMP }, // eb
	{ "CPX",  3, ABS }, // ec
	{ "SBC",  3, ABS }, // ed
	{ "INC",  3, ABS }, // ee
	{ "NOP",  1, IMP }, // ef
	{ "BEQ",  2, REL }, // f0
	{ "SBC",  2, INY }, // f1
	{ "SBC",  2, IND }, // f2
	{ "NOP",  1, IMP }, // f3
	{ "NOP",  2, ZPX }, // f4
	{ "SBC",  2, ZPX }, // f5
	{ "INC",  2, ZPX }, // f6
	{ "NOP",  1, IMP }, // f7
	{ "SED",  1, IMP }, // f8
	{ "SBC",  3, ABY }, // f9
	{ "PLX",  1, IMP }, // fa
	{ "NOP",  1, IMP }, // fb
	{ "NOP",  3, ABS }, // fc
	{ "SBC",  3, ABX }, // fd
	{ "INC",  3, ABX }, // fe
	{ "NOP",  1, IMP }  // ff
};

static const InstInfo* GetOpcodeTable(bool host)
{
	if (host) {
		if (MachineType == Model::Master128 || MachineType == Model::MasterET) {
			return optable_65sc12;
		}
		else {
			return optable_6502;
		}
	}
	else {
		return optable_65c02;
	}
}

static bool IsDlgItemChecked(HWND hDlg, int nIDDlgItem)
{
	return SendDlgItemMessage(hDlg, nIDDlgItem, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

static void SetDlgItemChecked(HWND hDlg, int nIDDlgItem, bool checked)
{
	SendDlgItemMessage(hDlg, nIDDlgItem, BM_SETCHECK, checked ? BST_CHECKED : BST_UNCHECKED, 0);
}

void DebugOpenDialog(HINSTANCE hinst, HWND /* hwndMain */)
{
	if (hwndInvisibleOwner == nullptr)
	{
		// Keep the debugger off the taskbar with an invisible owner window.
		// This persists until the process closes.
		hwndInvisibleOwner =
			CreateWindowEx(0, "STATIC", 0, 0, 0, 0, 0, 0, 0, 0, hinst, 0);
	}

	if (hwndDebug != nullptr)
	{
		DebugCloseDialog();
	}

	DebugEnabled = true;

	DebugHistory.clear();

	haccelDebug = LoadAccelerators(hinst, MAKEINTRESOURCE(IDR_ACCELERATORS));
	hwndDebug = CreateDialog(hinst, MAKEINTRESOURCE(IDD_DEBUG),
	                         hwndInvisibleOwner, DebugDlgProc);

	hCurrentDialog = hwndDebug;
	hCurrentAccelTable = haccelDebug;
	ShowWindow(hwndDebug, SW_SHOW);

	hwndInfo = GetDlgItem(hwndDebug, IDC_DEBUGINFO);
	SendMessage(hwndInfo, WM_SETFONT, (WPARAM)GetStockObject(ANSI_FIXED_FONT),
	            MAKELPARAM(FALSE, 0));

	hwndBP = GetDlgItem(hwndDebug, IDC_DEBUGBREAKPOINTS);
	SendMessage(hwndBP, WM_SETFONT, (WPARAM)GetStockObject(ANSI_FIXED_FONT),
	            MAKELPARAM(FALSE, 0));

	hwndW = GetDlgItem(hwndDebug, IDC_DEBUGWATCHES);
	SendMessage(hwndW, WM_SETFONT, (WPARAM)GetStockObject(ANSI_FIXED_FONT),
	            MAKELPARAM(FALSE, 0));

	SetDlgItemChecked(hwndDebug, IDC_DEBUGBPS, true);
	SetDlgItemChecked(hwndDebug, IDC_DEBUGHOST, true);
}

void DebugCloseDialog()
{
	DestroyWindow(hwndDebug);
	hwndDebug = nullptr;
	hwndInfo = nullptr;
	hCurrentDialog = nullptr;
	hCurrentAccelTable = nullptr;
	DebugEnabled = false;
	DebugSource = DebugType::None;
	LinesDisplayed = 0;
	InstCount = 0;
	DumpAddress = 0;
	DisAddress = 0;
	Breakpoints.clear();
	Watches.clear();
	BPSOn = true;
	StepOver = false;
	ReturnAddress = 0;
	DebugOS = false;
	LastAddrInOS = false;
	LastAddrInBIOS = false;
	DebugROM = false;
	LastAddrInROM = false;
	DebugHost = true;
	DebugParasite = false;
	DebugInfoWidth = 0;
}

//*******************************************************************
void DebugDisplayInfoF(const char *format, ...)
{
	va_list args;
	va_start(args, format);

	// _vscprintf doesn't count terminating '\0'
	int len = _vscprintf(format, args) + 1;

	char *buffer = (char*)malloc(len * sizeof(char));

	if (buffer != nullptr)
	{
		vsprintf_s(buffer, len * sizeof(char), format, args);

		DebugDisplayInfo(buffer);
		free(buffer);
	}
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

INT_PTR CALLBACK DebugDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM /* lParam */)
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
					BPSOn = IsDlgItemChecked(hwndDebug, IDC_DEBUGBPS);
					break;

				case IDC_DEBUGBRK:
					BRKOn = IsDlgItemChecked(hwndDebug, IDC_DEBUGBRK);
					break;

				case IDC_DEBUGOS:
					DebugOS = IsDlgItemChecked(hwndDebug, IDC_DEBUGOS);
					break;

				case IDC_DEBUGROM:
					DebugROM = IsDlgItemChecked(hwndDebug, IDC_DEBUGROM);
					break;

				case IDC_DEBUGHOST:
					DebugHost = IsDlgItemChecked(hwndDebug, IDC_DEBUGHOST);
					break;

				case IDC_DEBUGPARASITE:
					DebugParasite = IsDlgItemChecked(hwndDebug, IDC_DEBUGPARASITE);
					break;

				case IDC_WATCHDECIMAL:
				case IDC_WATCHENDIAN:
					WatchDecimal = IsDlgItemChecked(hwndDebug, IDC_WATCHDECIMAL);
					WatchBigEndian = IsDlgItemChecked(hwndDebug, IDC_WATCHENDIAN);
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

static void DebugToggleRun()
{
	if(DebugSource != DebugType::None)
	{
		// Resume execution
		DebugBreakExecution(DebugType::None);
	}
	else
	{
		// Cause manual break
		DebugBreakExecution(DebugType::Manual);
	}
}

void DebugBreakExecution(DebugType type)
{
	DebugSource = type;

	if (type == DebugType::None)
	{
		InstCount = 0;
		LastBreakAddr = 0;
		SetDlgItemText(hwndDebug, IDC_DEBUGBREAK, "Break");
	}
	else
	{
		InstCount = 1;
		SetDlgItemText(hwndDebug, IDC_DEBUGBREAK, "Continue");
		LastAddrInBIOS = LastAddrInOS = LastAddrInROM = false;

		DebugUpdateWatches(true);
	}
}

static const char* GetDebugSourceString()
{
	const char* source = "Unknown";

	switch(DebugSource)
	{
	case DebugType::None:
		break;

	case DebugType::Video:
		source = "Video";
		break;

	case DebugType::UserVIA:
		source = "User VIA";
		break;

	case DebugType::SysVIA:
		source = "System VIA";
		break;

	case DebugType::Tube:
		source = "Tube";
		break;

	case DebugType::Serial:
		source = "Serial";
		break;

	case DebugType::Econet:
		source = "Econet";
		break;

	case DebugType::Teletext:
		source = "Teletext";
		break;

	case DebugType::RemoteServer:
		source = "Remote server";
		break;

	case DebugType::Manual:
		source = "Manual";
		break;

	case DebugType::Breakpoint:
		source = "Breakpoint";
		break;

	case DebugType::CMOS:
		source = "CMOS";
		break;

	case DebugType::BRK:
		source = "BRK instruction";
		break;
	}

	return source;
}

static void DebugDisplayPreviousAddress(int prevAddr)
{
	if (prevAddr > 0)
	{
		AddrInfo addrInfo;

		if (DebugLookupAddress(prevAddr, &addrInfo))
		{
			DebugDisplayInfoF("  Previous PC 0x%04X (%s)", prevAddr, addrInfo.desc.c_str());
		}
		else
		{
			DebugDisplayInfoF("  Previous PC 0x%04X", prevAddr);
		}
	}
}

void DebugAssertBreak(int addr, int prevAddr, bool host)
{
	AddrInfo addrInfo;

	DebugUpdateWatches(false);
	SetDlgItemText(hwndDebug, IDC_DEBUGBREAK, "Continue");

	if (LastBreakAddr == 0)
	{
		LastBreakAddr = addr;
	}
	else
	{
		return;
	}

	if (DebugSource == DebugType::Breakpoint)
	{
		for (const Breakpoint& bp : Breakpoints)
		{
			if (bp.start == addr)
			{
				if (DebugLookupAddress(addr, &addrInfo))
				{
					DebugDisplayInfoF("%s break at 0x%04X (Breakpoint '%s' / %s)",
					                  host ? "Host" : "Parasite",
					                  addr,
					                  bp.name.c_str(),
					                  addrInfo.desc.c_str());
				}
				else
				{
					DebugDisplayInfoF("%s break at 0x%04X (Breakpoint '%s')",
					                  host ? "Host" : "Parasite",
					                  addr,
					                  bp.name.c_str());
				}

				DebugDisplayPreviousAddress(prevAddr);
				return;
			}
		}
	}

	if (DebugLookupAddress(addr, &addrInfo))
	{
		DebugDisplayInfoF("%s break at 0x%04X (%s / %s)",
		                  host ? "Host" : "Parasite",
		                  addr,
		                  GetDebugSourceString(),
		                  addrInfo.desc.c_str());
	}
	else
	{
		DebugDisplayInfoF("%s break at 0x%04X (%s)",
		                  host ? "Host" : "Parasite",
		                  addr,
		                  GetDebugSourceString());
	}

	DebugDisplayPreviousAddress(prevAddr);
}

void DebugDisplayTrace(DebugType type, bool host, const char *info)
{
	if (type == DebugType::Econet)
	{
		DebugTrace(info);
		DebugTrace("\n");
		return;
	}

	if (DebugEnabled && ((DebugHost && host) || (DebugParasite && !host)))
	{
		switch (type)
		{
		case DebugType::Video:
			if (IsDlgItemChecked(hwndDebug, IDC_DEBUG_VIDEO))
				DebugDisplayInfo(info);
			if (IsDlgItemChecked(hwndDebug, IDC_DEBUG_VIDEO_BRK))
				DebugBreakExecution(type);
			break;

		case DebugType::UserVIA:
			if (IsDlgItemChecked(hwndDebug, IDC_DEBUG_USERVIA))
				DebugDisplayInfo(info);
			if (IsDlgItemChecked(hwndDebug, IDC_DEBUG_USERVIA_BRK))
				DebugBreakExecution(type);
			break;

		case DebugType::SysVIA:
			if (IsDlgItemChecked(hwndDebug, IDC_DEBUG_SYSVIA))
				DebugDisplayInfo(info);
			if (IsDlgItemChecked(hwndDebug, IDC_DEBUG_SYSVIA_BRK))
				DebugBreakExecution(type);
			break;

		case DebugType::Tube:
			if ((DebugHost && host) || (DebugParasite && !host))
			{
				if (IsDlgItemChecked(hwndDebug, IDC_DEBUG_TUBE))
					DebugDisplayInfo(info);
				if (IsDlgItemChecked(hwndDebug, IDC_DEBUG_TUBE_BRK))
					DebugBreakExecution(type);
			}

			DebugTrace(info);
			break;

		case DebugType::Serial:
			if (IsDlgItemChecked(hwndDebug, IDC_DEBUG_SERIAL))
				DebugDisplayInfo(info);
			if (IsDlgItemChecked(hwndDebug, IDC_DEBUG_SERIAL_BRK))
				DebugBreakExecution(type);
			break;

		case DebugType::RemoteServer:
			if (IsDlgItemChecked(hwndDebug, IDC_DEBUG_REMSER))
				DebugDisplayInfo(info);
			if (IsDlgItemChecked(hwndDebug, IDC_DEBUG_REMSER_BRK))
				DebugBreakExecution(type);
			break;

		case DebugType::Econet:
			if (IsDlgItemChecked(hwndDebug, IDC_DEBUG_ECONET))
				DebugDisplayInfo(info);
			if (IsDlgItemChecked(hwndDebug, IDC_DEBUG_ECONET_BRK))
				DebugBreakExecution(type);
			break;

		case DebugType::Teletext:
			if (IsDlgItemChecked(hwndDebug, IDC_DEBUG_TELETEXT))
				DebugDisplayInfo(info);
			if (IsDlgItemChecked(hwndDebug, IDC_DEBUG_TELETEXT_BRK))
				DebugBreakExecution(type);
			break;

		case DebugType::CMOS:
			if (IsDlgItemChecked(hwndDebug, IDC_DEBUG_CMOS))
				DebugDisplayInfo(info);
			if (IsDlgItemChecked(hwndDebug, IDC_DEBUG_CMOS_BRK))
				DebugBreakExecution(type);
			break;
		}
	}
}

void DebugDisplayTraceF(DebugType type, bool host, const char *format, ...)
{
	va_list args;
	va_start(args, format);

	DebugDisplayTraceV(type, host, format, args);

	va_end(args);
}

void DebugDisplayTraceV(DebugType type, bool host, const char *format, va_list args)
{
	// _vscprintf doesn't count terminating '\0'
	int len = _vscprintf(format, args) + 1;

	char *buffer = (char*)malloc(len * sizeof(char));

	if (buffer != nullptr)
	{
		vsprintf_s(buffer, len * sizeof(char), format, args);

		DebugDisplayTrace(type, host, buffer);
		free(buffer);
	}
}

static void DebugUpdateWatches(bool all)
{
	int value = 0;
	char str[200];

	for (size_t i = 0; i < Watches.size(); ++i)
	{
		switch (Watches[i].type)
		{
			case 'b':
				value = DebugReadMem(Watches[i].start, Watches[i].host);
				break;

			case 'w':
				if (WatchBigEndian)
				{
					value = (DebugReadMem(Watches[i].start,     Watches[i].host) << 8) +
					         DebugReadMem(Watches[i].start + 1, Watches[i].host);
				}
				else
				{
					value = (DebugReadMem(Watches[i].start + 1, Watches[i].host) << 8) +
					         DebugReadMem(Watches[i].start,     Watches[i].host);
				}
				break;

			case 'd':
				if (WatchBigEndian)
				{
					value = (DebugReadMem(Watches[i].start,     Watches[i].host) << 24) +
					        (DebugReadMem(Watches[i].start + 1, Watches[i].host) << 16) +
					        (DebugReadMem(Watches[i].start + 2, Watches[i].host) << 8) +
					         DebugReadMem(Watches[i].start + 3, Watches[i].host);
				}
				else
				{
					value = (DebugReadMem(Watches[i].start + 3, Watches[i].host) << 24) +
					        (DebugReadMem(Watches[i].start + 2, Watches[i].host) << 16) +
					        (DebugReadMem(Watches[i].start + 1, Watches[i].host) << 8) +
					         DebugReadMem(Watches[i].start,     Watches[i].host);
				}
				break;
		}

		if (all || value != Watches[i].value)
		{
			Watches[i].value = value;

			SendMessage(hwndW, LB_DELETESTRING, i, 0);

			if (WatchDecimal)
			{
				sprintf(str, "%s%04X %s=%d (%c)", (Watches[i].host ? "" : "p"), Watches[i].start, Watches[i].name.c_str(), Watches[i].value, Watches[i].type);
			}
			else
			{
				switch (Watches[i].type)
				{
					case 'b':
						sprintf(str, "%s%04X %s=$%02X", Watches[i].host ? "" : "p", Watches[i].start, Watches[i].name.c_str(), Watches[i].value);
						break;

					case 'w':
						sprintf(str, "%s%04X %s=$%04X", Watches[i].host ? "" : "p", Watches[i].start, Watches[i].name.c_str(), Watches[i].value);
						break;

					case 'd':
						sprintf(str, "%s%04X %s=$%08X", Watches[i].host ? "" : "p", Watches[i].start, Watches[i].name.c_str(), Watches[i].value);
						break;
				}
			}

			SendMessage(hwndW, LB_INSERTSTRING, i, (LPARAM)str);
		}
	}
}

bool DebugDisassembler(int addr,
                       int prevAddr,
                       int Accumulator,
                       int XReg,
                       int YReg,
                       unsigned char PSR,
                       unsigned char StackReg,
                       bool host)
{
	// Update memory watches. Prevent emulator slowdown by limiting updates
	// to every 100ms, or on timer wrap-around.
	static DWORD LastTickCount = 0;
	const DWORD TickCount = GetTickCount();

	if (TickCount - LastTickCount > 100 || TickCount < LastTickCount)
	{
		LastTickCount = TickCount;
		DebugUpdateWatches(false);
	}

	// If this is the host and we're debugging that and have no further
	// instructions to execute, halt.
	if (host && DebugHost && DebugSource != DebugType::None && InstCount == 0)
	{
		return false;
	}

	// Don't process further if we're not debugging the parasite either
	if (!host && !DebugParasite)
	{
		return true;
	}

	if (BRKOn && DebugReadMem(addr, host) == 0)
	{
		DebugBreakExecution(DebugType::BRK);
		ProgramCounter++;
	}

	if (host && StepOver && addr == ReturnAddress)
	{
		StepOver = false;
		DebugBreakExecution(DebugType::Breakpoint);
	}

	// Check breakpoints
	if (BPSOn && DebugSource != DebugType::Breakpoint)
	{
		for (const Breakpoint& bp : Breakpoints)
		{
			if (bp.end == -1)
			{
				if (addr == bp.start)
				{
					DebugBreakExecution(DebugType::Breakpoint);
				}
			}
			else
			{
				if (addr >= bp.start && addr <= bp.end)
				{
					DebugBreakExecution(DebugType::Breakpoint);
				}
			}
		}
	}

	if (DebugSource == DebugType::None)
	{
		return true;
	}

	if ((TubeType == Tube::AcornZ80 || TubeType == Tube::TorchZ80) && !host)
	{
		if (!DebugOS && addr >= 0xf800 && addr <= 0xffff)
		{
			if (!LastAddrInBIOS)
			{
				AddrInfo addrInfo;

				if (DebugLookupAddress(addr, &addrInfo))
				{
					DebugDisplayInfoF("Entered BIOS (0xF800-0xFFFF) at 0x%04X (%s)",
					                  addr,
					                  addrInfo.desc.c_str());
				}
				else
				{
					DebugDisplayInfoF("Entered BIOS (0xF800-0xFFFF) at 0x%04X",
					                  addr);
				}

				LastAddrInBIOS = true;
				LastAddrInOS = LastAddrInROM = false;
			}
			return true;
		}

		LastAddrInBIOS = false;
	}
	else
	{
		if (!DebugOS && addr >= 0xc000 && addr <= 0xfbff)
		{
			if (!LastAddrInOS)
			{
				AddrInfo addrInfo;

				if (DebugLookupAddress(addr, &addrInfo))
				{
					DebugDisplayInfoF("Entered OS (0xC000-0xFBFF) at 0x%04X (%s)", addr, addrInfo.desc.c_str());
				}
				else
				{
					DebugDisplayInfoF("Entered OS (0xC000-0xFBFF) at 0x%04X", addr);
				}

				LastAddrInOS = true;
				LastAddrInBIOS = LastAddrInROM = false;
			}

			return true;
		}

		LastAddrInOS = false;

		if (!DebugROM && addr >= 0x8000 && addr <= 0xbfff)
		{
			if (!LastAddrInROM)
			{
				RomInfo romInfo;

				if (ReadRomInfo(ROMSEL, &romInfo))
				{
					DebugDisplayInfoF("Entered paged ROM bank %d \"%s\" (0x8000-0xBFFF) at 0x%04X", ROMSEL, romInfo.Title, addr);
				}
				else
				{
					DebugDisplayInfoF("Entered paged ROM bank %d (0x8000-0xBFFF) at 0x%04X", ROMSEL, addr);
				}

				LastAddrInROM = true;
				LastAddrInOS = LastAddrInBIOS = false;
			}

			return true;
		}

		LastAddrInROM = false;
	}

	if (host && InstCount == 0)
	{
		return false;
	}

	DebugAssertBreak(addr, prevAddr, host);

	char str[150];

	// Parasite instructions:
	if ((TubeType == Tube::AcornZ80 || TubeType == Tube::TorchZ80) && !host)
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
		int Length = DebugDisassembleInstructionWithCPUStatus(
			addr, host, Accumulator, XReg, YReg, StackReg, PSR, str
		);

		if (!host)
		{
			strcpy(&str[Length], "  Parasite");
		}
	}

	DebugDisplayInfo(str);

	// If host debug is enable then only count host instructions
	// and display all parasite inst (otherwise we lose them).
	if ((DebugHost && host) || !DebugHost)
	{
		if (InstCount > 0)
		{
			InstCount--;
		}
	}

	return true;
}

static void DebugLookupSWRAddress(AddrInfo* addrInfo)
{
	addrInfo->start = 0x8000;
	addrInfo->end   = 0xbfff;

	RomInfo rom;
	char desc[100];

	const char* ROMType = RomWritable[ROMSEL] ? "Paged ROM" : "Sideways RAM";

	// Try ROM info:
	if (ReadRomInfo(ROMSEL, &rom))
	{
		sprintf(desc, "%s bank %d: %.80s", ROMType, ROMSEL, rom.Title);
	}
	else
	{
		sprintf(desc, "%s bank %d", ROMType, ROMSEL);
	}

	addrInfo->desc = desc;
}

static bool DebugLookupAddress(int addr, AddrInfo* addrInfo)
{
	RomInfo rom;

	// Try current ROM's map
	if (!MemoryMaps[ROMSEL].empty())
	{
		for (size_t i = 0; i < MemoryMaps[ROMSEL].size(); i++)
		{
			if (addr >= MemoryMaps[ROMSEL][i].start && addr <= MemoryMaps[ROMSEL][i].end)
			{
				addrInfo->start = MemoryMaps[ROMSEL][i].start;
				addrInfo->end   = MemoryMaps[ROMSEL][i].end;

				char desc[100];
				sprintf(desc, "%.99s", ReadRomInfo(ROMSEL, &rom) ? rom.Title : "ROM");
				addrInfo->desc = desc;

				return true;
			}
		}
	}

	// Try OS map
	if (!MemoryMaps[16].empty())
	{
		for (size_t i = 0; i < MemoryMaps[16].size(); i++)
		{
			if (addr >= MemoryMaps[16][i].start && addr <= MemoryMaps[16][i].end)
			{
				*addrInfo = MemoryMaps[16][i];

				return true;
			}
		}
	}

	if (MachineType == Model::B)
	{
		if (addr >= 0x8000 && addr < 0xc000)
		{
			DebugLookupSWRAddress(addrInfo);
			return true;
		}
	}
	else if (MachineType == Model::IntegraB)
	{
		if (ShEn && !MemSel && addr >= 0x3000 && addr <= 0x7fff)
		{
			addrInfo->start = 0x3000;
			addrInfo->end   = 0x7fff;
			addrInfo->desc  = "Shadow RAM";
			return true;
		}

		if (PrvEn)
		{
			if (Prvs8 && addr >= 0x8000 && addr <= 0x83ff)
			{
				addrInfo->start = 0x8000;
				addrInfo->end   = 0x83ff;
				addrInfo->desc  = "1K private area";
				return true;
			}
			else if (Prvs4 && addr >= 0x8000 && addr <= 0x8fff)
			{
				addrInfo->start = 0x8400;
				addrInfo->end   = 0x8fff;
				addrInfo->desc  = "4K private area";
				return true;
			}
			else if (Prvs1 && addr >= 0x9000 && addr <= 0xafff)
			{
				addrInfo->start = 0x9000;
				addrInfo->end   = 0xafff;
				addrInfo->desc  = "8K private area";
				return true;
			}
		}

		if (addr >= 0x8000 && addr < 0xc000)
		{
			DebugLookupSWRAddress(addrInfo);
			return true;
		}
	}
	else if (MachineType == Model::BPlus)
	{
		if (addr >= 0x3000 && addr <= 0x7fff)
		{
			addrInfo->start = 0x3000;
			addrInfo->end   = 0x7fff;

			if (Sh_Display && PrePC >= 0xc000 && PrePC <= 0xdfff)
			{
				addrInfo->desc = "Shadow RAM (PC in VDU driver)";
				return true;
			}
			else if (Sh_Display && MemSel && PrePC >= 0xa000 && PrePC <= 0xafff)
			{
				addrInfo->start = 0x3000;
				addrInfo->end   = 0x7fff;
				addrInfo->desc  = "Shadow RAM (PC in upper 4K of ROM and shadow selected)";
				return true;
			}
		}
		else if (addr >= 0x8000 && addr <= 0xafff && MemSel)
		{
			addrInfo->start = 0x8000;
			addrInfo->end   = 0xafff;
			addrInfo->desc  = "Paged RAM";
			return true;
		}
		else if (addr >= 0x8000 && addr < 0xc000)
		{
			DebugLookupSWRAddress(addrInfo);
			return true;
		}
	}
	else if (MachineType == Model::Master128 || MachineType == Model::MasterET)
	{
		// Master cartridge (not implemented in BeebEm yet)
		if ((ACCCON & 0x20) && addr >= 0xfc00 && addr <= 0xfdff)
		{
			addrInfo->start = 0xfc00;
			addrInfo->end   = 0xfdff;
			addrInfo->desc  = "Cartridge (ACCCON bit 5 set)";
			return true;
		}

		// Master private and shadow RAM.
		if ((ACCCON & 0x08) && addr >= 0xc000 && addr <= 0xdfff)
		{
			addrInfo->start = 0xc000;
			addrInfo->end   = 0xdfff;
			addrInfo->desc  = "8K Private RAM (ACCCON bit 3 set)";
			return true;
		}

		if ((ACCCON & 0x04) && addr >= 0x3000 && addr <= 0x7fff)
		{
			addrInfo->start = 0x3000;
			addrInfo->end   = 0x7fff;
			addrInfo->desc  = "Shadow RAM (ACCCON bit 2 set)";
			return true;
		}

		if ((ACCCON & 0x02) && PrePC >= 0xC000 && PrePC <= 0xDFFF && addr >= 0x3000 && addr <= 0x7FFF)
		{
			addrInfo->start = 0x3000;
			addrInfo->end   = 0x7fff;
			addrInfo->desc  = "Shadow RAM (ACCCON bit 1 set and PC in VDU driver)";
			return true;
		}

		// Master private RAM.
		if ((PagedRomReg & 0x80) && addr >= 0x8000 && addr <= 0x8fff)
		{
			addrInfo->start = 0x8000;
			addrInfo->end   = 0x8fff;
			addrInfo->desc  = "4K Private RAM (ROMSEL bit 7 set)";
			return true;
		}

		if (addr >= 0x8000 && addr < 0xc000)
		{
			DebugLookupSWRAddress(addrInfo);
			return true;
		}
	}

	return false;
}

static void DebugExecuteCommand()
{
	char command[MAX_COMMAND_LEN + 1];
	GetDlgItemText(hwndDebug, IDC_DEBUGCOMMAND, command, MAX_COMMAND_LEN);
	DebugParseCommand(command);
}

void DebugInitMemoryMaps()
{
	for(int i = 0; i < _countof(MemoryMaps); i++)
	{
		MemoryMaps[i].clear();
	}
}

bool DebugLoadMemoryMap(const char* filename, int bank)
{
	if (bank < 0 || bank > 16)
		return false;

	MemoryMap* map = &MemoryMaps[bank];

	FILE *infile = fopen(filename, "r");

	if (infile != nullptr)
	{
		map->clear();

		char line[1024];

		while (fgets(line, _countof(line), infile) != nullptr)
		{
			DebugChompString(line);
			char *buf = line;

			while(buf[0] == ' ' || buf[0] == '\t' || buf[0] == '\r' || buf[0] == '\n')
				buf++;

			if (buf[0] == ';' || buf[0] == '\0') // Skip comments and empty lines
				continue;

			AddrInfo entry;

			char desc[256];
			memset(desc, 0, sizeof(desc));

			int result = sscanf(buf, "%x %x %99c", &entry.start, &entry.end, desc);

			if (result >= 2 && strlen(desc) > 0)
			{
				entry.desc = desc;
				map->push_back(entry);
			}
			else
			{
				mainWin->Report(MessageType::Error, "Invalid memory map format!");

				map->clear();

				fclose(infile);
				return false;
			}
		}

		fclose(infile);
	}
	else
	{
		return false;
	}

	return true;
}

void DebugLoadLabels(char *filename)
{
	FILE *infile = fopen(filename, "r");
	if (infile == NULL)
	{
		DebugDisplayInfoF("Error: Failed to open labels from %s", filename);
	}
	else
	{
		Labels.clear();

		char buf[1024];

		while(fgets(buf, _countof(buf), infile) != NULL)
		{
			DebugChompString(buf);

			int addr;
			char name[64];

			// Example: al FFEE .oswrch
			if (sscanf(buf, "%*s %x .%64s", &addr, name) != 2)
			{
				DebugDisplayInfoF("Error: Invalid labels format: %s", filename);
				fclose(infile);

				Labels.clear();
				return;
			}

			Labels.emplace_back(std::string(name), addr);
		}

		DebugDisplayInfoF("Loaded %u labels from %s", Labels.size(), filename);
		fclose(infile);
	}
}

void DebugRunScript(const char *filename)
{
	FILE *infile = fopen(filename,"r");
	if (infile == NULL)
	{
		DebugDisplayInfoF("Failed to read script file:\n  %s", filename);
	}
	else
	{
		DebugDisplayInfoF("Running script %s",filename);

		char buf[1024];

		while(fgets(buf, _countof(buf), infile) != NULL)
		{
			DebugChompString(buf);
			if(strlen(buf) > 0)
				DebugParseCommand(buf);
		}
		fclose(infile);
	}
}

// Loads Swift format labels, used by BeebAsm

bool DebugLoadSwiftLabels(const char* filename)
{
	std::ifstream input(filename);

	if (input)
	{
		bool valid = true;

		std::string line;

		while (std::getline(input, line))
		{
			trim(line);

			// Example: [{'SYMBOL':12345L,'SYMBOL2':12346L}]

			if (line.length() > 4 && line[0] == '[' && line[1] == '{')
			{
				std::size_t i = 2;

				while (line[i] == '\'')
				{
					std::size_t end = line.find('\'', i + 1);

					if (end == std::string::npos)
					{
						valid = false;
						break;
					}

					std::string symbol = line.substr(i + 1, end - (i + 1));
					i = end + 1;

					if (line[i] == ':')
					{
						end = line.find('L', i + 1);

						if (end == std::string::npos)
						{
							valid = false;
							break;
						}

						std::string address = line.substr(i + 1, end - (i + 1));
						i = end + 1;

						Label label;
						label.name = symbol;

						try
						{
							label.addr = std::stoi(address);
						}
						catch (const std::exception&)
						{
							valid = false;
							break;
						}

						Labels.push_back(label);
					}

					if (line[i] == ',')
					{
						i++;
					}
					else if (line[i] == '}')
					{
						break;
					}
					else
					{
						valid = false;
						break;
					}
				}
			}
		}

		if (!valid)
		{
			Labels.clear();
		}

		return valid;
	}
	else
	{
		DebugDisplayInfoF("Failed to load symbols file:\n  %s", filename);
		return false;
	}
}

static void DebugChompString(char *str)
{
	const size_t length = strlen(str);

	if (length > 0)
	{
		size_t end = length - 1;

		while (end > 0 && (str[end] == '\r' || str[end] == '\n' || str[end] == ' ' || str[end] == '\t'))
		{
			str[end] = '\0';
			end--;
		}
	}
}

int DebugParseLabel(char *label)
{
	auto it = std::find_if(Labels.begin(), Labels.end(), [=](const Label& Label) {
		return _stricmp(label, Label.name.c_str()) == 0;
	});

	return it != Labels.end() ? it->addr : -1;
}

static void DebugHistoryAdd(char *command)
{
	// Do nothing if this is the same as the last command

	if (DebugHistory.size() == 0 ||
	    (DebugHistory.size() > 0 && _stricmp(DebugHistory[0].c_str(), command) != 0))
	{
		// Otherwise insert command string at index 0.
		DebugHistory.push_front(command);

		if (DebugHistory.size() > MAX_HISTORY)
		{
			DebugHistory.pop_back();
		}
	}

	DebugHistoryIndex = -1;
}

static void DebugHistoryMove(int delta)
{
	int newIndex = DebugHistoryIndex - delta;

	if (newIndex < 0)
	{
		DebugHistoryIndex = -1;
		SetDlgItemText(hwndDebug, IDC_DEBUGCOMMAND, "");
		return;
	}

	const int HistorySize = (int)DebugHistory.size();

	if (newIndex >= HistorySize)
	{
		if (HistorySize > 0)
		{
			newIndex = HistorySize - 1;
		}
		else
		{
			return;
		}
	}

	DebugHistoryIndex = newIndex;
	DebugSetCommandString(DebugHistory[DebugHistoryIndex].c_str());
}

static void DebugSetCommandString(const char* str)
{
	if (DebugHistoryIndex == -1 &&
	    DebugHistory.size() > 0 &&
	    _stricmp(DebugHistory[0].c_str(), str) == 0)
	{
		// The string we're about to set is the same as the top history one,
		// so use history to set it. This is just a nicety to make the up
		// key work as expected when commands such as 'next' and 'peek'
		// have automatically filled in the command box.
		DebugHistoryMove(-1);
	}
	else
	{
		SetDlgItemText(hwndDebug, IDC_DEBUGCOMMAND, str);
		SendDlgItemMessage(hwndDebug, IDC_DEBUGCOMMAND, EM_SETSEL, strlen(str), strlen(str));
	}
}

static void DebugParseCommand(char *command)
{
	char label[65], addrStr[6];
	char info[MAX_PATH + 100];

	while(command[0] == '\n' || command[0] == '\r' || command[0] == '\t' || command[0] == ' ')
		command++;

	if (command[0] == '\0' || command[0] == '/' || command[0] == ';' || command[0] == '#')
		return;

	DebugHistoryAdd(command);

	info[0] = '\0';
	char *args = strchr(command, ' ');
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
					int addr = DebugParseLabel(label);
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
				size_t end = strnlen(info, _countof(info));
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

	for(int i = 0; i < _countof(DebugCmdTable); i++)
	{
		if(_stricmp(DebugCmdTable[i].name, command) == 0)
		{
			if(!DebugCmdTable[i].handler(args))
				DebugCmdHelp(command);
			return;
		}
	}

	DebugDisplayInfoF("Invalid command %s - try 'help'",command);
}

/**************************************************************
 * Start of debugger command handlers                         *
 **************************************************************/

static bool DebugCmdEcho(char* args)
{
	DebugDisplayInfo(args);
	return true;
}

static bool DebugCmdGoto(char* args)
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

static bool DebugCmdFile(char* args)
{
	char mode;
	int i = 0;
	int addr = 0;
	unsigned char buffer[MAX_BUFFER];
	int count = MAX_BUFFER;
	char filename[MAX_PATH];
	memset(filename, 0, MAX_PATH);

	int result = sscanf(args,"%c %x %u %259c", &mode, &addr, &count, filename);

	if (result < 3) {
		sscanf(args,"%c %x %259c", &mode, &addr, filename);
	}

	mode = static_cast<char>(tolower(mode));

	if (filename[0] == '\0')
	{
		const char* filter = "Memory Dump Files (*.dat)\0*.dat\0" "All Files (*.*)\0*.*\0";

		FileDialog Dialog(hwndDebug, filename, MAX_PATH, nullptr, filter);

		if (mode == 'w')
		{
			if (!Dialog.Save())
			{
				return false;
			}

			// Add a file extension if the user did not specify one
			if (strchr(filename, '.') == nullptr)
			{
				strcat(filename, ".dat");
			}
		}
		else
		{
			if (!Dialog.Open())
			{
				return false;
			}
		}
	}

	if (filename[0] != '\0')
	{
		addr &= 0xFFFF;

		if (mode == 'r')
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

				DebugUpdateWatches(true);
			}
			else
			{
				DebugDisplayInfoF("Failed to open file: %s", filename);
			}

			return true;
		}
		else if (mode == 'w')
		{
			FILE *fd = fopen(filename, "wb");

			if (fd)
			{
				if (count + addr > MAX_BUFFER)
				{
					count = MAX_BUFFER - addr;
				}

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

static bool DebugCmdPoke(char* args)
{
	int addr, data;
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
		int start = addr = addr & 0xFFFF;
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
			DebugUpdateWatches(true);
			DebugDisplayInfoF("Changed %d bytes starting at 0x%04X", i, start);
			return true;
		}
	}
	else
		return false;
}

static bool DebugCmdSave(char* args)
{
	int count = 0;
	char filename[MAX_PATH];
	char* info = NULL;
	int infoSize = 0;
	memset(filename, 0, MAX_PATH);

	int result = sscanf(args, "%u %259c", &count, filename);

	if (result < 1) {
		sscanf(args, "%259c", filename);
	}

	if (filename[0] == '\0')
	{
		const char* filter = "Text Files (*.txt)\0*.txt\0";

		FileDialog Dialog(hwndDebug, filename, MAX_PATH, nullptr, filter);

		if (!Dialog.Save())
		{
			return false;
		}

		// Add a file extension if the user did not specify one
		if (strchr(filename, '.') == nullptr)
		{
			strcat(filename, ".txt");
		}
	}

	if (filename[0] != '\0')
	{
		if (count <= 0 || count > LinesDisplayed)
			count = LinesDisplayed;

		FILE *fd = fopen(filename, "w");
		if (fd)
		{
			for (int i = LinesDisplayed - count; i < LinesDisplayed; ++i)
			{
				int len = (int)(SendMessage(hwndInfo, LB_GETTEXTLEN, i, NULL) + 1) * sizeof(TCHAR);

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

static bool DebugCmdState(char* args)
{
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

		case 'e': // Serial ACIA / ULA state
			DebugSerialState();
			break;

		case 't': // Tube state
			DebugTubeState();
			break;

		case 'm': // Memory state
			DebugMemoryState();
			break;

		case 'r': // ROM state
			DebugDisplayInfo("ROMs by priority:");
			for (int i = 15; i >= 0; i--)
			{
				RomInfo rom;

				char flags[50];
				flags[0] = '\0';

				if (ReadRomInfo(i, &rom))
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

static bool DebugCmdCode(char* args)
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

static bool DebugCmdPeek(char* args)
{
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

static bool DebugCmdNext(char* args)
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

// TODO: currently host only, enable for Tube debugging

static bool DebugCmdOver(char* args)
{
	// If current instruction is JSR, run to the following instruction,
	// otherwise do a regular 'Next'
	int Instruction = DebugReadMem(PrePC, true);

	if (Instruction == 0x20)
	{
		StepOver = true;
		InstCount = 1;

		const InstInfo* optable = GetOpcodeTable(true);

		ReturnAddress = PrePC + optable[Instruction].bytes;

		// Resume execution
		DebugBreakExecution(DebugType::None);
	}
	else
	{
		DebugCmdNext(args);
	}

	DebugSetCommandString("over");
	return true;
}

static bool DebugCmdSet(char* args)
{
	char name[20];
	char state[4];
	bool checked = false;
	int dlgItem = 0;

	if(sscanf(args,"%s %s",name,state) == 2)
	{
		//host/parasite/rom/os/bigendian/breakpoint/decimal/brk
		if(_stricmp(state, "on") == 0)
			checked = true;

		if(_stricmp(name, "host") == 0)
		{
			dlgItem = IDC_DEBUGHOST;
			DebugHost = checked;
		}
		else if(_stricmp(name, "parasite") == 0)
		{
			dlgItem = IDC_DEBUGPARASITE;
			DebugParasite = checked;
		}
		else if(_stricmp(name, "rom") == 0)
		{
			dlgItem = IDC_DEBUGROM;
			DebugROM = checked;
		}
		else if(_stricmp(name, "os") == 0)
		{
			dlgItem = IDC_DEBUGOS;
			DebugOS = checked;
		}
		else if(_stricmp(name, "endian") == 0)
		{
			dlgItem = IDC_WATCHENDIAN;
			WatchBigEndian = checked;
			DebugUpdateWatches(true);
		}
		else if(_stricmp(name, "breakpoints") == 0)
		{
			dlgItem = IDC_DEBUGBPS;
			BPSOn = checked;
		}
		else if(_stricmp(name, "decimal") == 0)
		{
			dlgItem = IDC_WATCHDECIMAL;
			WatchDecimal = checked;
		}
		else if(_stricmp(name, "brk") == 0)
		{
			dlgItem = IDC_DEBUGBRK;
			BRKOn = checked;
		}
		else
			return false;

		SetDlgItemChecked(hwndDebug, dlgItem, checked);
		return true;
	}
	else
		return false;
}

static bool DebugCmdBreakContinue(char* /* args */)
{
	DebugToggleRun();
	DebugSetCommandString(".");
	return true;
}

static bool DebugCmdHelp(char* args)
{
	int addr;
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
		for (int i = 0; i < _countof(DebugCmdTable); i++)
		{
			if (DebugCmdTable[i].help[0] != '\0')
			{
				DebugDisplayInfo("");
				DebugDisplayInfoF("  %s",DebugCmdTable[i].name);

				if (DebugCmdTable[i].argdesc[0] != '\0')
				{
					DebugDisplayInfoF("  %s",DebugCmdTable[i].argdesc);
				}

				DebugDisplayInfoF("    %s",DebugCmdTable[i].help);
			}
		}

		// Display help for aliases

		DebugDisplayInfo("");
		DebugDisplayInfo("Command aliases:");

		for (int i = 0; i < _countof(DebugCmdTable); i++)
		{
			if (strlen(DebugCmdTable[i].help) > 0)
			{
				if(strlen(aliasInfo) > 0)
				{
					aliasInfo[strlen(aliasInfo) - 2] = 0;
					DebugDisplayInfoF("%8s: %s",DebugCmdTable[li].name, aliasInfo);
				}
				aliasInfo[0] = 0;
				li = i;
			}
			else if (strlen(DebugCmdTable[i].help) == 0 && strlen(DebugCmdTable[i].argdesc) == 0 &&
			         DebugCmdTable[li].handler == DebugCmdTable[i].handler)
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
		for (int i = 0; i < _countof(DebugCmdTable); i++)
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
		if (sscanf(args, "%x", &addr) == 1)
		{
			if (DebugLookupAddress(addr, &addrInfo))
			{
				DebugDisplayInfoF("0x%04X: %s (0x%04X-0x%04X)", addr, addrInfo.desc.c_str(), addrInfo.start, addrInfo.end);
			}
			else
			{
				DebugDisplayInfoF("0x%04X: No description", addr);
			}
		}
		else
		{
			DebugDisplayInfoF("Help: Command %s was not recognised.", args);
		}
	}

	return true;
}

static bool DebugCmdScript(char *args)
{
	char filename[MAX_PATH];
	memset(filename, 0, sizeof(filename));

	strncpy(filename, args, sizeof(filename) - 1);

	if (filename[0] == '\0')
	{
		const char* filter = "Debugger Script Files (*.txt)\0*.txt\0" "All Files (*.*)\0*.*\0";

		FileDialog Dialog(hwndDebug, filename, MAX_PATH, nullptr, filter);

		if (!Dialog.Open())
		{
			return false;
		}
	}

	if (filename[0] != '\0')
	{
		DebugRunScript(filename);
	}

	return true;
}

static bool DebugCmdClear(char * /* args */)
{
	LinesDisplayed = 0;
	SendMessage(hwndInfo, LB_RESETCONTENT, 0, 0);
	return true;
}

static void DebugShowLabels()
{
	if (Labels.empty())
	{
		DebugDisplayInfo("No labels defined.");
	}
	else
	{
		DebugDisplayInfoF("%d known labels:", Labels.size());

		for (std::size_t i = 0; i < Labels.size(); i++)
		{
			DebugDisplayInfoF("%04X %s", Labels[i].addr, Labels[i].name.c_str());
		}
	}
}

static bool DebugCmdLabels(char *args)
{
	if (stricmp(args, "show") == 0)
	{
		DebugShowLabels();
		return true;
	}
	else if (_strnicmp(args, "load", 4) == 0)
	{
		char filename[MAX_PATH];
		memset(filename, 0, MAX_PATH);

		sscanf(args, "%*s %259c", filename);

		bool success = true;

		if (filename[0] == '\0')
		{
			const char* filter = "Label Files (*.txt)\0*.txt\0" "All Files (*.*)\0*.*\0";

			FileDialog Dialog(hwndDebug, filename, MAX_PATH, nullptr, filter);

			success = Dialog.Open();
		}

		if (success && filename[0] != '\0')
		{
			DebugLoadLabels(filename);
		}

		DebugShowLabels();
	}

	return true;
}

static bool DebugCmdWatch(char *args)
{
	Watch w;
	char info[64];
	int i;
	w.start = -1;
	w.host = true;
	w.type = 'w';

	if (Watches.size() < MAX_BPS)
	{
		if (tolower(args[0]) == 'p') // Parasite
		{
			w.host = false;
			args++;
		}

		char name[51];
		memset(name, 0, _countof(name));

		int result = sscanf(args, "%x %c %50c", &w.start, &w.type, name);

		if (result < 2) {
			result = sscanf(args, "%x %50c", &w.start, name);
		}

		if (result != EOF)
		{
			// Check type is valid
			w.type = (char)tolower(w.type);
			if (w.type != 'b' && w.type != 'w' && w.type != 'd')
				return false;

			w.name = name;

			sprintf(info, "%s%04X", (w.host ? "" : "p"), w.start);

			// Check if watch in list
			i = (int)SendMessage(hwndW, LB_FINDSTRING, 0, (LPARAM)info);

			if (i != LB_ERR)
			{
				SendMessage(hwndW, LB_DELETESTRING, i, 0);

				auto it = std::find_if(Watches.begin(), Watches.end(), [&w](const Watch& watch){
					return watch.start == w.start;
				});

				Watches.erase(it);
			}
			else
			{
				Watches.push_back(w);

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

static bool DebugCmdToggleBreak(char *args)
{
	Breakpoint bp;
	bp.start = bp.end = -1;

	if (Breakpoints.size() < MAX_BPS)
	{
		char name[51];
		memset(name, 0, _countof(name));

		if (sscanf(args, "%x-%x %50c", &bp.start, &bp.end, name) >= 2 ||
		    sscanf(args, "%x %50c", &bp.start, name) >= 1)
		{
			bp.name = name;

			// Check if BP in list

			char info[64];
			sprintf(info, "%04X", bp.start);

			int i = (int)SendMessage(hwndBP, LB_FINDSTRING, 0, (LPARAM)info);

			if (i != LB_ERR)
			{
				// Yes - delete
				SendMessage(hwndBP, LB_DELETESTRING, i, 0);

				auto it = std::find_if(Breakpoints.begin(), Breakpoints.end(), [&bp](const Breakpoint& b){
					return b.start == bp.start;
				});

				Breakpoints.erase(it);
			}
			else
			{
				if (bp.end >= 0 && bp.end < bp.start)
				{
					DebugDisplayInfo("Error: Invalid breakpoint range.");
					return false;
				}

				// No - add a new bp.
				Breakpoints.push_back(bp);

				if (bp.end >= 0)
				{
					sprintf(info, "%04X-%04X %s", bp.start, bp.end, bp.name.c_str());
				}
				else
				{
					sprintf(info, "%04X %s", bp.start, bp.name.c_str());
				}

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

unsigned char DebugReadMem(int addr, bool host)
{
	if (host)
		return BeebReadMem(addr);

	if (TubeType == Tube::AcornZ80 || TubeType == Tube::TorchZ80)
		return ReadZ80Mem(addr);

	return TubeReadMem(addr);
}

static void DebugWriteMem(int addr, bool host, unsigned char data)
{
	if (host)
		BeebWriteMem(addr, data);

	if (TubeType == Tube::AcornZ80 || TubeType == Tube::TorchZ80)
		WriteZ80Mem(addr, data);

	TubeWriteMem(addr, data);
}

int DebugDisassembleInstruction(int addr, bool host, char *opstr)
{
	int operand = 0;
	int zpaddr = 0;
	int l = 0;

	char *s = opstr;

	s += sprintf(s, "%04X ", addr);

	int opcode = DebugReadMem(addr, host);

	const InstInfo *optable = GetOpcodeTable(host);

	const InstInfo *ip = &optable[opcode];

	switch (ip->bytes) {
		case 1:
			s += sprintf(s, "%02X        ",
			             DebugReadMem(addr, host));
			break;
		case 2:
			s += sprintf(s, "%02X %02X     ",
			             DebugReadMem(addr, host),
			             DebugReadMem(addr + 1, host));
			break;
		case 3:
			s += sprintf(s, "%02X %02X %02X  ",
			             DebugReadMem(addr, host),
			             DebugReadMem(addr + 1, host),
			             DebugReadMem(addr + 2, host));
			break;
	}

	if (!host) {
		s += sprintf(s, "            ");
	}

	s += sprintf(s, "%s ", ip->opcode);
	addr++;

	switch (ip->bytes)
	{
		case 1:
			l = 0;
			break;
		case 2:
			operand = DebugReadMem(addr, host);
			l = 2;
			break;
		case 3:
			operand = DebugReadMem(addr, host) | (DebugReadMem(addr + 1, host) << 8);
			l = 4;
			break;
	}

	if (ip->flag & REL)
	{
		if (operand > 127)
		{
			operand = (~0xff | operand);
		}

		operand = operand + ip->bytes + addr - 1;
		l = 4;
	}
	else if (ip->flag & ZPR)
	{
		zpaddr = operand & 0xff;
		int Offset  = (operand & 0xff00) >> 8;

		if (Offset > 127)
		{
			Offset = (~0xff | Offset);
		}

		operand = addr + ip->bytes - 1 + Offset;
	}

	switch (ip->flag & ADRMASK)
	{
	case IMM:
		s += sprintf(s, "#%0*X    ", l, operand);
		break;
	case REL:
	case ABS:
	case ZPG:
		s += sprintf(s, "%0*X     ", l, operand);
		break;
	case IND:
		s += sprintf(s, "(%0*X)   ", l, operand);
		break;
	case ABX:
	case ZPX:
		s += sprintf(s, "%0*X,X   ", l, operand);
		break;
	case ABY:
	case ZPY:
		s += sprintf(s, "%0*X,Y   ", l, operand);
		break;
	case INX:
		s += sprintf(s, "(%0*X,X) ", l, operand);
		break;
	case INY:
		s += sprintf(s, "(%0*X),Y ", l, operand);
		break;
	case ACC:
		s += sprintf(s, "A        ");
		break;
	case ZPR:
		s += sprintf(s, "%02X,%04X ", zpaddr, operand);
		break;
	case IMP:
	default:
		s += sprintf(s, "         ");
		break;
	}

	if (l == 2) {
		s += sprintf(s, "  ");
	}

	if (host) {
		s += sprintf(s, "            ");
	}

	return ip->bytes;
}

int DebugDisassembleInstructionWithCPUStatus(int addr,
                                             bool host,
                                             int Accumulator,
                                             int XReg,
                                             int YReg,
                                             unsigned char StackReg,
                                             unsigned char PSR,
                                             char *opstr)
{
	DebugDisassembleInstruction(addr, host, opstr);

	char* p = opstr + strlen(opstr);

	p += sprintf(p, "A=%02X X=%02X Y=%02X S=%02X ", Accumulator, XReg, YReg, StackReg);

	*p++ = (PSR & FlagC) ? 'C' : '.';
	*p++ = (PSR & FlagZ) ? 'Z' : '.';
	*p++ = (PSR & FlagI) ? 'I' : '.';
	*p++ = (PSR & FlagD) ? 'D' : '.';
	*p++ = (PSR & FlagB) ? 'B' : '.';
	*p++ = (PSR & FlagV) ? 'V' : '.';
	*p++ = (PSR & FlagN) ? 'N' : '.';
	*p = '\0';

	return (int)(p - opstr);
}

static int DebugDisassembleCommand(int addr, int count, bool host)
{
	char opstr[80];
	char *s = opstr;

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
		if ((TubeType == Tube::AcornZ80 || TubeType == Tube::TorchZ80) && !host)
		{
			char buff[64];
			int Len = Z80_Disassemble(addr, buff);

			s += sprintf(s, "%04X ", addr);

			switch (Len)
			{
				case 1:
					s += sprintf(s, "%02X           ", DebugReadMem(addr, host));
					break;
				case 2:
					s += sprintf(s, "%02X %02X        ", DebugReadMem(addr, host), DebugReadMem(addr+1, host));
					break;
				case 3:
					s += sprintf(s, "%02X %02X %02X     ", DebugReadMem(addr, host), DebugReadMem(addr+1, host), DebugReadMem(addr+2, host));
					break;
				case 4:
					s += sprintf(s, "%02X %02X %02X %02X  ", DebugReadMem(addr, host), DebugReadMem(addr+1, host), DebugReadMem(addr+2, host), DebugReadMem(addr+3, host));
					break;
			}

			strcpy(s, buff);

			addr += Len;
		}
		else
		{
			addr += DebugDisassembleInstruction(addr, host, opstr);
		}

		DebugDisplayInfo(opstr);
		count--;
	}

	return addr - saddr;
}

static void DebugMemoryDump(int addr, int count, bool host)
{
	if (count > MAX_LINES * 16)
		count = MAX_LINES * 16;

	int s = addr & 0xfff0;
	int e = (addr + count - 1) | 0xf;

	if (e > 0xffff)
		e = 0xffff;

	DebugDisplayInfo("       0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F 0123456789ABCDEF");

	for (int a = s; a < e; a += 16)
	{
		char info[80];
		char* p = info;
		p += sprintf(p, "%04X  ", a);

		if (host && a >= 0xfc00 && a < 0xff00)
		{
			p += sprintf(p, "IO space");
		}
		else
		{
			for (int b = 0; b < 16; ++b)
			{
				if (!host && (a+b) >= 0xfef8 && (a+b) < 0xff00 && !(TubeType == Tube::AcornZ80 || TubeType == Tube::TorchZ80))
					p += sprintf(p, "IO ");
				else
					p += sprintf(p, "%02X ", DebugReadMem(a+b, host));
			}

			for (int b = 0; b < 16; ++b)
			{
				if (host || (a+b) < 0xfef8 || (a+b) >= 0xff00)
				{
					int v = DebugReadMem(a+b, host);
					if (v < 32 || v > 127)
						v = '.';
					*p++ = (char)v;
					*p = '\0';
				}
			}
		}

		DebugDisplayInfo(info);
	}
}
