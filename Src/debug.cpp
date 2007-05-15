//
// BeebEm debugger
//
// Mike Wyatt - Nov 2004
// Econet added Rob O'Donnell 2004-12-28.

#include <windows.h>
#include <ctype.h>
#include <string.h>
#include "main.h"
#include "beebmem.h"
#include "tube.h"
#include "beebemrc.h"
#include "6502core.h"
#include "debug.h"
#include "z80mem.h"
#include "z80.h"

int DebugEnabled = false;        // Debug dialog visible

static int DebugOn = 0;         // Debugging active?
static int LinesDisplayed = 0;  // Lines in info window
static int InstCount = 0;       // Instructions to execute before breaking
static int DumpAddress = 0;     // Next address for memory dump command
static int DisAddress = 0;      // Next address for disassemble command
static int BPCount = 0;         // Num of breakpoints
static bool BPSOn = true;
static bool BreakpointHit = false;
static bool DebugOS = false;
static bool LastAddrInOS = false;
static bool LastAddrInBIOS = false;
static bool DebugROM = false;
static bool LastAddrInROM = false;
static bool DebugHost = true;
static bool DebugParasite = false;
static HWND hwndDebug;
static HWND hwndInfo;
static HWND hwndBP;

extern HWND hCurrentDialog;

//*******************************************************************
// Data structs

enum PSRFlags {
  FlagC=1,
  FlagZ=2,
  FlagI=4,
  FlagD=8,
  FlagB=16,
  FlagV=64,
  FlagN=128
};

#define MAX_LINES 4096          // Max lines in info window
#define LINES_IN_INFO 43        // Visible lines in info window
#define MAX_COMMAND_LEN 200     // Max debug command length
#define MAX_BPS 50				// Max num of breakpoints

// Breakpoints
struct Breakpoint
{
	int start;
	int end;
};

Breakpoint Breakpoints[MAX_BPS];

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

#define ADRMASK (IMM|ABS|ACC|IMP|INX|INY|ZPX|ABX|ABY|REL|IND|ZPY|ZPG|ILL)

// Instruction data
struct InstInfo
{
	char opn[4];
	int  nb;
	int  flag;
	int  c6502;
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

//*******************************************************************

BOOL CALLBACK DebugDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam);
int DebugDisassembleInstruction(int addr, bool host, char *opstr);
int DebugDisassembleCommand(int addr, int count, bool host);
void DebugMemoryDump(int addr, int count, bool host);
void DebugExecuteCommand();

//*******************************************************************

void DebugOpenDialog(HINSTANCE hinst, HWND hwndMain)
{
	DebugEnabled = TRUE;

	if (!IsWindow(hwndDebug)) 
	{ 
		hwndDebug = CreateDialog(hinst, MAKEINTRESOURCE(IDD_DEBUG),
								NULL, (DLGPROC)DebugDlgProc);
		hCurrentDialog = hwndDebug;
		ShowWindow(hwndDebug, SW_SHOW);

		hwndInfo = GetDlgItem(hwndDebug, IDC_DEBUGINFO);
		SendMessage(hwndInfo, WM_SETFONT, (WPARAM)GetStockObject(ANSI_FIXED_FONT),
					(LPARAM)MAKELPARAM(FALSE,0));

		hwndBP = GetDlgItem(hwndDebug, IDC_DEBUGBREAKPOINTS);
		SendMessage(hwndBP, WM_SETFONT, (WPARAM)GetStockObject(ANSI_FIXED_FONT),
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
	DebugOn = 0;
	LinesDisplayed = 0;
	InstCount = 0;
	DumpAddress = 0;
	DisAddress = 0;
	BPCount = 0;
	BreakpointHit = false;
	BPSOn = true;
	DebugOS = false;
	LastAddrInOS = false;
	LastAddrInBIOS = false;
	DebugROM = false;
	LastAddrInROM = false;
	DebugHost = true;
	DebugParasite = false;
	memset(Breakpoints, 0, MAX_BPS * sizeof(Breakpoint));
}

//*******************************************************************

void DebugDisplayInfo(const char *info)
{
	SendMessage(hwndInfo, LB_ADDSTRING, 0, (LPARAM)info);
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
				hCurrentDialog = NULL;
			else
				hCurrentDialog = hwndDebug;
			return FALSE;

		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case IDC_DEBUGBREAK:
					DebugOn = TRUE;
					InstCount = 1;
					BreakpointHit = false;
					DebugDisplayInfo("");
					DebugDisplayInfo("- EXECUTION BREAK -");
					DebugDisplayInfo("");
					return TRUE;

				case IDC_DEBUGRESTART:
					if (DebugOn == TRUE)
					{
						DebugOn = FALSE;
						InstCount = 0;
						BreakpointHit = false;
						DebugDisplayInfo("");
						DebugDisplayInfo("- EXECUTION RESTARTED -");
						DebugDisplayInfo("");
						SetDlgItemText(hwndDebug, IDC_DEBUGCOMMAND, "");
					}
					return TRUE;

				case IDC_DEBUGEXECUTE:
					if (DebugOn)
					{
						DebugExecuteCommand();
					}
					return TRUE;

				case IDC_DEBUGBPS:
					BPSOn = (SendDlgItemMessage(hwndDebug, IDC_DEBUGBPS, BM_GETCHECK, 0, 0) == BST_CHECKED);
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

				case IDCANCEL:
					DebugCloseDialog();
					return TRUE;
			}
	}
	return FALSE;
}

//*******************************************************************

void DebugDisplayTrace(DebugType type, bool host, const char *info)
{
	if (DebugEnabled)
	{
		switch (type)
		{
		case DEBUG_VIDEO:
			if (SendDlgItemMessage(hwndDebug, IDC_DEBUGVIDEO, BM_GETCHECK, 0, 0) == BST_CHECKED)
				DebugDisplayInfo(info);
			if (SendDlgItemMessage(hwndDebug, IDC_DEBUGVIDEOBRK, BM_GETCHECK, 0, 0) == BST_CHECKED)
			{
				DebugOn = TRUE;
				InstCount = 1;
				BreakpointHit = false;
				DebugDisplayInfo("- VIDEO BREAK -");
			}
			break;
		case DEBUG_USERVIA:
			if (SendDlgItemMessage(hwndDebug, IDC_DEBUGUSERVIA, BM_GETCHECK, 0, 0) == BST_CHECKED)
				DebugDisplayInfo(info);
			if (SendDlgItemMessage(hwndDebug, IDC_DEBUGUSERVIABRK, BM_GETCHECK, 0, 0) == BST_CHECKED)
			{
				DebugOn = TRUE;
				InstCount = 1;
				BreakpointHit = false;
				DebugDisplayInfo("- USER VIA BREAK -");
			}
			break;
		case DEBUG_SYSVIA:
			if (SendDlgItemMessage(hwndDebug, IDC_DEBUGSYSVIA, BM_GETCHECK, 0, 0) == BST_CHECKED)
				DebugDisplayInfo(info);
			if (SendDlgItemMessage(hwndDebug, IDC_DEBUGSYSVIABRK, BM_GETCHECK, 0, 0) == BST_CHECKED)
			{
				DebugOn = TRUE;
				InstCount = 1;
				BreakpointHit = false;
				DebugDisplayInfo("- SYS VIA BREAK -");
			}
			break;
		case DEBUG_TUBE:
			if ((DebugHost && host) || (DebugParasite && !host))
			{
				if (SendDlgItemMessage(hwndDebug, IDC_DEBUGTUBE, BM_GETCHECK, 0, 0) == BST_CHECKED)
					DebugDisplayInfo(info);
				if (SendDlgItemMessage(hwndDebug, IDC_DEBUGTUBEBRK, BM_GETCHECK, 0, 0) == BST_CHECKED)
				{
					DebugOn = TRUE;
					InstCount = 1;
					BreakpointHit = false;
					DebugDisplayInfo("- TUBE BREAK -");
				}
			}
			break;
		case DEBUG_SERIAL:
			if (SendDlgItemMessage(hwndDebug, IDC_DEBUGSERIAL, BM_GETCHECK, 0, 0) == BST_CHECKED)
				DebugDisplayInfo(info);
			if (SendDlgItemMessage(hwndDebug, IDC_DEBUGSERIALBRK, BM_GETCHECK, 0, 0) == BST_CHECKED)
			{
				DebugOn = TRUE;
				InstCount = 1;
				BreakpointHit = false;
				DebugDisplayInfo("- SERIAL BREAK -");
			}
			break;
		case DEBUG_ECONET:
			if (SendDlgItemMessage(hwndDebug, IDC_DEBUGECONET, BM_GETCHECK, 0, 0) == BST_CHECKED)
				DebugDisplayInfo(info);
			if (SendDlgItemMessage(hwndDebug, IDC_DEBUGECONETBRK, BM_GETCHECK, 0, 0) == BST_CHECKED)
			{
				DebugOn = TRUE;
				InstCount = 1;
				BreakpointHit = false;
				DebugDisplayInfo("- ECONET BREAK -");
			}
			break;
		}
	}
}

bool DebugDisassembler(int addr, int Accumulator, int XReg, int YReg, int PSR, int StackReg, bool host)
{
	char str[150];
	int i;

	if (host && !DebugHost)
	{
		// Just check if we should halt
		if (DebugOn && InstCount == 0)
			return(FALSE);
		return(TRUE);
	}

	if (!host && !DebugParasite)
		return(TRUE);

	if (BreakpointHit)
		return(FALSE);

	// Check breakpoints
	if (BPSOn)
	{
		for (i = 0; i < BPCount && BreakpointHit == false; ++i)
		{
			if (Breakpoints[i].end == -1)
			{
				if (addr == Breakpoints[i].start)
					BreakpointHit = true;
			}
			else
			{
				if (addr >= Breakpoints[i].start && addr <= Breakpoints[i].end)
					BreakpointHit = true;
			}
		}
		if (BreakpointHit)
		{
			DebugOn = TRUE;
			InstCount = 1;
			DebugDisplayInfo("- BREAKPOINT HIT -");
		}
	}

	if (!DebugOn)
		return(TRUE);

	if ( (TorchTube || AcornZ80) && !host)
	{
		if (DebugOS == false && addr >= 0xf800 && addr <= 0xffff)
		{
			if (!LastAddrInBIOS)
			{
				DebugDisplayInfo("- ENTERING BIOS -");
				LastAddrInBIOS = true;
			}
			return(TRUE);
		}
		LastAddrInBIOS = false;
	}
	else
	{
		if (DebugOS == false && addr >= 0xc000 && addr <= 0xffff)
		{
			if (!LastAddrInOS)
			{
				DebugDisplayInfo("- ENTERING OS -");
				LastAddrInOS = true;
			}
			return(TRUE);
		}
		LastAddrInOS = false;

		if (DebugROM == false && addr >= 0x8000 && addr <= 0xbfff)
		{
			if (!LastAddrInROM)
			{
				DebugDisplayInfo("- ENTERING ROM -");
				LastAddrInROM = true;
			}
			return(TRUE);
		}
		LastAddrInROM = false;
	}

	// Display all parasite instructions (otherwise we loose them).
	if (host && InstCount == 0)
		return(FALSE);

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

		sprintf(str + strlen(str), "%02X %02X %02X %02X ", Accumulator, XReg, YReg, StackReg);

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
	// and display all parasite inst (otherwise we loose them).
	if ((DebugHost && host) || !DebugHost)
		if (InstCount > 0)
			InstCount--;

	return(TRUE);
}

//*******************************************************************

void DebugExecuteCommand()
{
	char command[MAX_COMMAND_LEN + 1];
	char filename[100 + 1];
	char info[200];
	int start, end, count;
	int i;
	bool ok = false;
	bool host = true;

	GetDlgItemText(hwndDebug, IDC_DEBUGCOMMAND, command, MAX_COMMAND_LEN);
	if (strlen(command) == 0)
		return;

	switch (tolower(command[0]))
	{
	case 'n': // Next instruction, params: [count]
		ok = true;
		count = 1;
		sscanf(&command[1], "%x", &count);
		if (count > MAX_LINES)
			count = MAX_LINES;
		InstCount = count;
		BreakpointHit = false;
		break;

	case 'm': // Memory dump, params: [p] [start] [count]
		ok = true;
		i = 1;
		if (tolower(command[1]) == 'p') // Parasite
		{
			host = false;
			++i;
		}
		start = DumpAddress;
		count = 256;
		sscanf(&command[i], "%x %x", &start, &count);
		DumpAddress = start & 0xffff;
		if (count > MAX_LINES * 16)
			count = MAX_LINES * 16;
		DebugMemoryDump(DumpAddress, count, host);
		DumpAddress += count;
		if (DumpAddress > 0xffff)
			DumpAddress = 0;
		SetDlgItemText(hwndDebug, IDC_DEBUGCOMMAND, host ? "m" : "mp");
		break;

	case 'd': // Disassemble, params: [p] [start] [count]
		ok = true;
		i = 1;
		if (tolower(command[1]) == 'p') // Parasite
		{
			host = false;
			++i;
		}
		start = DisAddress;
		count = LINES_IN_INFO;
		sscanf(&command[i], "%x %x", &start, &count);
		DisAddress = start & 0xffff;
		if (count > MAX_LINES)
			count = MAX_LINES;
		DisAddress += DebugDisassembleCommand(DisAddress, count, host);
		if (DisAddress > 0xffff)
			DisAddress = 0;
		SetDlgItemText(hwndDebug, IDC_DEBUGCOMMAND, host ? "d" : "dp");
		break;

	case 'b': // Breakpoint set/reset, params: start [end]
		if (BPCount < MAX_BPS)
		{
			end = -1;
			if (sscanf(&command[1], "%x %x", &start, &end) >= 1 &&
				start >= 0 && start <= 0xffff)
			{
				ok = true;
				sprintf(info, "%04X", start);

				// Check if BP in list
				i = (int)SendMessage(hwndBP, LB_FINDSTRING, 0, (LPARAM)info);
				if (i != LB_ERR)
				{
					SendMessage(hwndBP, LB_DELETESTRING, i, 0);
					for (i = 0; i < BPCount; ++i)
					{
						if (Breakpoints[i].start == start)
						{
							if (i != BPCount - 1)
								memmove(&Breakpoints[i], &Breakpoints[i+1], sizeof(Breakpoint) * (BPCount - i - 1));
							BPCount--;
							i = BPCount;
						}
					}
				}
				else
				{
					if (end >= 0 && end <= 0xffff && start < end)
					{
						sprintf(info, "%04X-%04X", start, end);
						Breakpoints[BPCount].start = start;
						Breakpoints[BPCount].end = end;
						BPCount++;
					}
					else
					{
						Breakpoints[BPCount].start = start;
						Breakpoints[BPCount].end = -1;
						BPCount++;
					}
					SendMessage(hwndBP, LB_ADDSTRING, 0, (LPARAM)info);
				}

				SetDlgItemText(hwndDebug, IDC_DEBUGCOMMAND, "");
			}
		}
		break;

	case 's': // Show state
		switch (tolower(command[1]))
		{
		case 'v': // Video state
			ok = true;
			DebugVideoState();
			break;
		case 'u': // User via state
			ok = true;
			DebugUserViaState();
			break;
		case 's': // Sys via state
			ok = true;
			DebugSysViaState();
			break;
		case 't': // Tube state
			ok = true;
			DebugTubeState();
			break;
		}
		break;

	case 'w': // Write buffer to file, params: filename [count]
		count = 0;
		filename[0] = '\0';
		if (sscanf(&command[1], "%100s %x", filename, &count) >= 1 &&
			strlen(filename) != 0)
		{
			ok = true;

			if (count <= 0 || count > LinesDisplayed)
				count = LinesDisplayed;

			FILE *fd = fopen(filename, "w");
			if (fd)
			{
				for (i = LinesDisplayed - count; i < LinesDisplayed; ++i)
				{
					SendMessage(hwndInfo, LB_GETTEXT, i, (LPARAM)info);
					fprintf(fd, "%s\n", info);
				}
				fclose(fd);

				DebugDisplayInfo("");
				sprintf(info, "Wrote %x lines to: %s", count, filename);
				DebugDisplayInfo(info);

				SetDlgItemText(hwndDebug, IDC_DEBUGCOMMAND, "");
			}
			else
			{
				DebugDisplayInfo("");
				sprintf(info, "Failed open for write: %s", filename);
				DebugDisplayInfo(info);
			}
		}
		break;

	case 'c': // Change memory, params: start byte byte ...
	{
		char *param = command + 1;
		while (*param == ' ')
			++param;
		if (sscanf(param, "%x", &start) == 1 && start >= 0 && start <= 0xffff)
		{
			ok = true;
			i = 0;
			int addr = start;
			int data = 0;
			param = strchr(param, ' ');
			while (param != NULL)
			{
				while (*param == ' ')
					++param;
				if (sscanf(param, "%x", &data) == 1)
				{
					BeebWriteMem(addr, data & 0xff);
					++addr;
					++i;
				}
				param = strchr(param, ' ');
			}

			DebugDisplayInfo("");
			sprintf(info, "Changed %x bytes starting at %x", i, start);
			DebugDisplayInfo(info);

			SetDlgItemText(hwndDebug, IDC_DEBUGCOMMAND, "");
		}
		break;
	}
	}

	if (!ok)
	{
		DebugDisplayInfo("");
		sprintf(info, "Bad or unrecognised: %s", command);
		DebugDisplayInfo(info);
	}
}

int DebugReadMem(int addr, bool host)
{
	if (host)
		return BeebReadMem(addr);
	if ((TorchTube || AcornZ80))
		return ReadZ80Mem(addr);
	return TubeReadMem(addr);
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

	if (!DebugOn)
		return(0);

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

	if (!DebugOn)
		return;

	s = addr & 0xfff0;
	e = (addr + count - 1) | 0xf;
	if (e > 0xffff)
		e = 0xffff;
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
