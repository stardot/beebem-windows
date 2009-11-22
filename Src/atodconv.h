/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 1997  Mike Wyatt

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

/* Analogue to digital converter support file for the beeb emulator -
   Mike Wyatt 7/6/97 */

#ifndef ATODCONV_HEADER
#define ATODCONV_HEADER

extern int JoystickEnabled;
extern int JoystickX;  /* 16 bit number, 0 = right */
extern int JoystickY;  /* 16 bit number, 0 = down */

void AtoDWrite(int Address, int Value);
int AtoDRead(int Address);
void AtoDInit(void);
void AtoDEnable(void);
void AtoDDisable(void);
void SaveAtoDUEF(FILE *SUEF);
void LoadAtoDUEF(FILE *SUEF);

extern int AtoDTrigger;  /* For next A to D conversion completion */

void AtoD_poll_real(void);
#define AtoD_poll(ncycles) if (AtoDTrigger<=TotalCycles) AtoD_poll_real();

#endif
