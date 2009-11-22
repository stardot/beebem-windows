/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 1994  David Alan Gilbert
Copyright (C) 1997  Mike Wyatt
Copyright (C) 2001  Richard Gellman
Copyright (C) 2004  Ken Lowe

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

/* System VIA support file for the beeb emulator- includes things like the
keyboard emulation - David Alan Gilbert 30/10/94 */

#ifndef SYSVIA_HEADER
#define SYSVIA_HEADER

#include "via.h"
#include "viastate.h"

extern VIAState SysVIAState;
extern unsigned char IC32State;
extern unsigned char RTCY2KAdjust;

void SysVIAWrite(int Address, int Value);
int SysVIARead(int Address);
void SysVIAReset(void);

void SysVIA_poll_real(void);

void SysVIA_poll(unsigned int ncycles);

void BeebKeyUp(int row, int col);
void BeebKeyDown(int row, int col);
void BeebReleaseAllKeys(void);

void SysVIATriggerCA1Int(int value);
extern unsigned char IC32State;

void RTCInit(void);
void CMOSWrite(unsigned char CMOSAddr,unsigned char CMOSData);
unsigned char CMOSRead(unsigned char CMOSAddr);

void sysvia_dumpstate(void);
void PulseSysViaCB1(void);

unsigned char BCD(unsigned char nonBCD);

extern int JoystickButton;

#endif
