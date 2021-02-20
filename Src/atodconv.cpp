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

#ifdef WIN32
#include <windows.h>
#endif

#include <stdio.h>

#include "6502core.h"
#include "atodconv.h"
#include "sysvia.h"
#include "uefstate.h"

bool JoystickEnabled[2] = { false, false };

/* X and Y positions for joystick 1 and 2 */
int JoystickX[2];
int JoystickY[2];

/* A to D state */
typedef struct AtoDStateT{
	unsigned char datalatch;
	unsigned char status;
	unsigned char high;
	unsigned char low;
} AtoDStateT;

AtoDStateT AtoDState;

int AtoDTrigger;  /* For next A to D conversion completion */

/*--------------------------------------------------------------------------*/
/* Address is in the range 0-f - with the fec0 stripped out */
void AtoDWrite(int Address, int Value)
{
	if (Address == 0)
	{
		int timetoconvert;
		AtoDState.datalatch = Value & 0xff;
		if (AtoDState.datalatch & 8)
			timetoconvert = 20000; /* 10 bit conversion, 10 ms */
		else
			timetoconvert = 8000; /* 8 bit conversion, 4 ms */
		SetTrigger(timetoconvert,AtoDTrigger);

		AtoDState.status = (AtoDState.datalatch & 0xf) | 0x80; /* busy, not complete */
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
		Value = AtoDState.status;
		break;

	case 1:
		Value = AtoDState.high;
		break;

	case 2:
		Value = AtoDState.low;
		break;
	}

	return Value;
}

/*--------------------------------------------------------------------------*/
void AtoD_poll_real(void)
{
	int value;

	ClearTrigger(AtoDTrigger);
	AtoDState.status &= 0xf;
	AtoDState.status |= 0x40; /* not busy */
	PulseSysViaCB1();

	switch (AtoDState.status & 3)
	{
	case 0:
		value = JoystickX[0];
		break;
	case 1:
		value = JoystickY[0];
		break;
	case 2:
		value = JoystickX[1];
		break;
	case 3:
		value = JoystickY[1];
		break;
	default:
		value = 0;
	}

	AtoDState.status |= (value & 0xc000)>>10;
	AtoDState.high = value>>8;
	AtoDState.low = value & 0xf0;
}

/*--------------------------------------------------------------------------*/
void AtoDInit(int index)
{
	AtoDState.datalatch = 0;
	AtoDState.high = 0;
	AtoDState.low = 0;
	ClearTrigger(AtoDTrigger);

	/* Move joystick to middle */
	JoystickX[index] = 32767;
	JoystickY[index] = 32767;

	/* Not busy, conversion complete (OS1.2 will then request another conversion) */
	AtoDState.status = 0x40;
	PulseSysViaCB1();
}

/*--------------------------------------------------------------------------*/
void AtoDEnable(int index)
{
	JoystickEnabled[index] = true;
	AtoDInit(index);
}

/*--------------------------------------------------------------------------*/
void AtoDDisable(int index)
{
	JoystickEnabled[index] = false;
	if (!JoystickEnabled[0] && !JoystickEnabled[1])
	{
		AtoDState.datalatch = 0;
		AtoDState.status = 0x80; /* busy, conversion not complete */
		AtoDState.high = 0;
		AtoDState.low = 0;
		ClearTrigger(AtoDTrigger);
	}

	/* Move joystick to middle (superpool looks at joystick even when not selected) */
	JoystickX[index] = 32767;
	JoystickY[index] = 32767;
}

/*--------------------------------------------------------------------------*/
void SaveAtoDUEF(FILE *SUEF) {
	fput16(0x0474,SUEF);
	fput32(8,SUEF);
	fputc(AtoDState.datalatch,SUEF);
	fputc(AtoDState.status,SUEF);
	fputc(AtoDState.high,SUEF);
	fputc(AtoDState.low,SUEF);
	if (AtoDTrigger == CycleCountTMax)
		fput32(AtoDTrigger,SUEF);
	else
		fput32(AtoDTrigger - TotalCycles,SUEF);
}
void LoadAtoDUEF(FILE *SUEF) {
	AtoDState.datalatch = fget8(SUEF);
	AtoDState.status = fget8(SUEF);
	AtoDState.high = fget8(SUEF);
	AtoDState.low = fget8(SUEF);
	AtoDTrigger = fget32(SUEF);
	if (AtoDTrigger != CycleCountTMax)
		AtoDTrigger+=TotalCycles;
}
