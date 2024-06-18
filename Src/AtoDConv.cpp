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

#include <windows.h>

#include <stdio.h>

#include "6502core.h"
#include "AtoDConv.h"
#include "SysVia.h"
#include "UefState.h"

bool JoystickEnabled = false;

// X and Y positions for joystick 1
int JoystickX;
int JoystickY;

/* A to D state */
struct AtoDState
{
	unsigned char DataLatch;
	unsigned char Status;
	unsigned char High;
	unsigned char Low;
};

static AtoDState AtoD;

int AtoDTrigger; // For next A to D conversion completion

/*--------------------------------------------------------------------------*/

// Address is in the range 0-f - with the fec0 stripped out

void AtoDWrite(int Address, unsigned char Value)
{
	if (Address == 0)
	{
		AtoD.DataLatch = Value;

		const int TimeToConvert = (AtoD.DataLatch & 8) ?
		                          20000 : // 10 bit conversion, 10 ms
		                          8000;   // 8 bit conversion, 4 ms

		SetTrigger(TimeToConvert, AtoDTrigger);

		AtoD.Status = (AtoD.DataLatch & 0xf) | 0x80; // busy, not complete
	}
}

/*--------------------------------------------------------------------------*/

// Address is in the range 0-f - with the fec0 stripped out

unsigned char AtoDRead(int Address)
{
	unsigned char Value = 0xff;

	switch (Address)
	{
	case 0:
		Value = AtoD.Status;
		break;

	case 1:
		Value = AtoD.High;
		break;

	case 2:
		Value = AtoD.Low;
		break;
	}

	return Value;
}

/*--------------------------------------------------------------------------*/

void AtoDPollReal()
{
	ClearTrigger(AtoDTrigger);

	AtoD.Status &= 0xf;
	AtoD.Status |= 0x40; // Not busy

	PulseSysViaCB1();

	int Value;

	switch (AtoD.Status & 3)
	{
	case 0:
		Value = JoystickX;
		break;
	case 1:
		Value = JoystickY;
		break;
	default:
		Value = 0;
		break;
	}

	AtoD.Status |= (Value & 0xc000) >> 10;
	AtoD.High = (unsigned char)(Value >> 8);
	AtoD.Low = Value & 0xf0;
}

/*--------------------------------------------------------------------------*/

void AtoDInit()
{
	AtoD.DataLatch = 0;
	AtoD.High = 0;
	AtoD.Low = 0;
	ClearTrigger(AtoDTrigger);

	// Move joystick to middle
	JoystickX = 32767;
	JoystickY = 32767;

	// Not busy, conversion complete (OS1.2 will then request another conversion)
	AtoD.Status = 0x40;
	PulseSysViaCB1();
}

/*--------------------------------------------------------------------------*/

void AtoDEnable()
{
	JoystickEnabled = true;
	AtoDInit();
}

/*--------------------------------------------------------------------------*/

void AtoDDisable()
{
	JoystickEnabled = false;
	AtoD.DataLatch = 0;
	AtoD.Status = 0x80; // Busy, conversion not complete
	AtoD.High = 0;
	AtoD.Low = 0;
	ClearTrigger(AtoDTrigger);

	// Move joystick to middle (Super Pool looks at joystick even when
	// not selected)
	JoystickX = 32767;
	JoystickY = 32767;
}

/*--------------------------------------------------------------------------*/

void SaveAtoDUEF(FILE *SUEF)
{
	UEFWrite8(AtoD.DataLatch, SUEF);
	UEFWrite8(AtoD.Status, SUEF);
	UEFWrite8(AtoD.High, SUEF);
	UEFWrite8(AtoD.Low, SUEF);
	if (AtoDTrigger == CycleCountTMax)
		UEFWrite32(AtoDTrigger, SUEF);
	else
		UEFWrite32(AtoDTrigger - TotalCycles, SUEF);
}

void LoadAtoDUEF(FILE *SUEF)
{
	AtoD.DataLatch = UEFRead8(SUEF);
	AtoD.Status = UEFRead8(SUEF);
	AtoD.High = UEFRead8(SUEF);
	AtoD.Low = UEFRead8(SUEF);
	AtoDTrigger = UEFRead32(SUEF);
	if (AtoDTrigger != CycleCountTMax)
		AtoDTrigger += TotalCycles;
}
