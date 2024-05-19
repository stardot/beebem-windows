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

#include "Via.h"
#include "Model.h"

extern VIAState SysVIAState;
extern unsigned char IC32State;

extern bool JoystickButton[2];

extern unsigned char KeyboardLinks;

void SysVIAWrite(int Address, unsigned char Value);
unsigned char SysVIARead(int Address);
void SysVIAReset();

void SysVIA_poll_real(void);

void SysVIA_poll(unsigned int ncycles);

void BeebKeyUp(int Row, int Column);
void BeebKeyDown(int Row, int Column);
void BeebReleaseAllKeys();

void SysVIATriggerCA1Int(int value);

void PulseSysViaCB1(void);

#endif
