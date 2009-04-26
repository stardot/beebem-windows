/****************************************************************************/
/*              Beebem - (c) David Alan Gilbert 1994                        */
/*              ------------------------------------                        */
/* This program may be distributed freely within the following restrictions:*/
/*                                                                          */
/* 1) You may not charge for this program or for any part of it.            */
/* 2) This copyright message must be distributed with all copies.           */
/* 3) This program must be distributed complete with source code.  Binary   */
/*    only distribution is not permitted.                                   */
/* 4) The author offers no warrenties, or guarentees etc. - you use it at   */
/*    your own risk.  If it messes something up or destroys your computer   */
/*    thats YOUR problem.                                                   */
/* 5) You may use small sections of code from this program in your own      */
/*    applications - but you must acknowledge its use.  If you plan to use  */
/*    large sections then please ask the author.                            */
/*                                                                          */
/* If you do not agree with any of the above then please do not use this    */
/* program.                                                                 */
/* Please report any problems to the author at beebem@treblig.org           */
/****************************************************************************/
/* 6502Core - header - David Alan Gilbert 16/10/94 */
#ifndef CORE6502_HEADER
#define CORE6502_HEADER

#include "port.h"
#include "stdio.h"

void DumpRegs(void);

typedef enum IRQ_Nums {
  sysVia,
  userVia,
  serial,
  tube,
  teletext,
  hdc,
} IRQ_Nums;

typedef enum NMI_Nums{
	nmi_floppy,
	nmi_econet,
} NMI_Nums;

typedef enum PSR_Flags
{
  FlagC=1,
  FlagZ=2,
  FlagI=4,
  FlagD=8,
  FlagB=16,
  FlagV=64,
  FlagN=128
} PSR_Flags;

extern int trace;
extern int trace_186;
extern int IgnoreIllegalInstructions;
extern int BeginDump;

extern unsigned char intStatus;
extern unsigned char NMIStatus;
extern unsigned int Cycles;
extern int ProgramCounter;
extern int PrePC;
extern CycleCountT TotalCycles;
extern unsigned int NMILock;
extern int DisplayCycles;

extern int CyclesToInt;
#define NO_TIMER_INT_DUE -1000000

#define SetTrigger(after,var) var=TotalCycles+after;
#define IncTrigger(after,var) var+=(after);

#define ClearTrigger(var) var=CycleCountTMax;

#define AdjustTrigger(var) if (var!=CycleCountTMax) var-=CycleCountWrap;

/*-------------------------------------------------------------------------*/
/* Initialise 6502core                                                     */
void Init6502core(void);

/*-------------------------------------------------------------------------*/
/* Execute one 6502 instruction, move program counter on                   */
void Exec6502Instruction(void);

void DoNMI(void);
void core_dumpstate(void);
void DoInterrupt(void);
void Save6502UEF(FILE *SUEF);
void Load6502UEF(FILE *SUEF);
void SyncIO(void);
void AdjustForIORead(void);
void AdjustForIOWrite(void);

int i186_execute(int num_cycles);

extern int SwitchOnCycles; // Reset delay
extern int OpCodes;
extern int BHardware;
#endif
