
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
/* System VIA support file for the beeb emulator- includes things like the
keyboard emulation - David Alan Gilbert 30/10/94 */

#ifndef SYSVIA_HEADER
#define SYSVIA_HEADER

#include "via.h"

extern VIAState SysVIAState;

void SysVIAWrite(int Address, int Value);
int SysVIARead(int Address);
void SysVIAReset(void);

void SysVIA_poll_real(void);

#define SysVIA_poll(ncycles) \
  SysVIAState.timer1c-=ncycles; \
  SysVIAState.timer2c-=ncycles; \
  if ((SysVIAState.timer1c<0) || (SysVIAState.timer2c<0)) SysVIA_poll_real();

void BeebKeyUp(int row, int col);
void BeebKeyDown(int row, int col);

void SysVIATriggerCA1Int(int value);
extern unsigned char IC32State;

void SaveSysVIAState(unsigned char *StateData);
void RestoreSysVIAState(unsigned char *StateData);

void sysvia_dumpstate(void);

extern int JoystickButton;

#endif
