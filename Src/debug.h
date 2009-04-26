//
// BeebEm debugger
//

#ifndef DEBUG_HEADER
#define DEBUG_HEADER

#include <windows.h>
#include "viastate.h"

extern int DebugEnabled;

enum DebugType {
	DEBUG_NONE,
	DEBUG_VIDEO,
	DEBUG_USERVIA,
	DEBUG_SYSVIA,
	DEBUG_TUBE,
	DEBUG_SERIAL,
	DEBUG_ECONET,
	DEBUG_REMSER,
	DEBUG_MANUAL,
	DEBUG_BREAKPOINT,
	DEBUG_BRK
};

//*******************************************************************
// Data structs


struct Label
{
	char name[65];
	int addr;
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
	int type;
	int value;
	bool host;
	char name[50 + 1];
};

struct InstInfo
{
	char opn[4];
	int  nb;
	int  flag;
	int  c6502;
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
	char *argdesc;
	char *help;
};

extern HWND hwndDebug;
int DebugDisassembleInstruction(int addr, bool host, char *opstr);
void DebugOpenDialog(HINSTANCE hinst, HWND hwndMain);
void DebugCloseDialog(void);
bool DebugDisassembler(int addr, int prevAddr, int Accumulator, int XReg, int YReg, int PSR, int StackReg, bool host);
void DebugDisplayTrace(DebugType type, bool host, const char *info);
void DebugDisplayInfo(const char *info);
void DebugDisplayInfoF(const char *format, ...);
void DebugVideoState(void);
void DebugUserViaState(void);
void DebugSysViaState(void);
void DebugViaState(const char *s, VIAState *v);
void DebugParseCommand(char *command);
void DebugRunScript(char *filename);
int DebugReadMem(int addr, bool host);
void DebugWriteMem(int addr, bool host, unsigned char data);
int DebugDisassembleInstruction(int addr, bool host, char *opstr);
int DebugDisassembleCommand(int addr, int count, bool host);
void DebugMemoryDump(int addr, int count, bool host);
void DebugExecuteCommand();
void DebugToggleRun();
void DebugBreakExecution(DebugType type);
void DebugUpdateWatches(bool all);
void DebugDisplayInfoF(const char *format, ...);
bool DebugLookupAddress(int addr, AddrInfo* addrInfo);
void DebugHistoryMove(int delta);
void DebugHistoryAdd(char* command);
void DebugInitMemoryMaps();
bool DebugLoadMemoryMap(char* filename, int bank);
void DebugSetCommandString(char* string);
#endif
