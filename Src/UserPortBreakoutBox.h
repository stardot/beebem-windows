/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 1994  David Alan Gilbert
Copyright (C) 1997  Mike Wyatt
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

/* User VIA support file for the beeb emulator - David Alan Gilbert 11/12/94 */
/* Modified from the system via */

#ifndef USER_PORT_BREAKOUT_BOX_HEADER
#define USER_PORT_BREAKOUT_BOX_HEADER

#include <windows.h>

/* User Port Breakout Box */

bool BreakOutOpenDialog(HINSTANCE hinst, HWND hwndParent);
void BreakOutCloseDialog();

extern int BitKeys[8];
extern HWND hwndBreakOut;

void ShowInputs(unsigned char data);
void ShowOutputs(unsigned char data);

#endif
