/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2006  Jon Welch
Copyright (C) 2009  Rob O'Donnell

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

#include <stdio.h>

#include "TouchScreen.h"
#include "AtoDConv.h"
#include "DebugTrace.h"
#include "RingBuffer.h"
#include "UserVia.h"

/*--------------------------------------------------------------------------*/

constexpr int TS_DELAY = 8192; // Cycles to wait for data to be TX'd or RX'd
static int TouchScreenMode = 0;
static int TouchScreenDelay;

// Note: Touchscreen routines use InputBuffer for data into the
// touchscreen and OutputBuffer as data from the touchscreen.

static RingBuffer InputBuffer(1024);
static RingBuffer OutputBuffer(1024);

/*--------------------------------------------------------------------------*/

void TouchScreenOpen()
{
	InputBuffer.Reset();
	OutputBuffer.Reset();
	TouchScreenDelay = 0;
}

/*--------------------------------------------------------------------------*/

bool TouchScreenPoll()
{
	if (TouchScreenDelay > 0)
	{
		TouchScreenDelay--;
		return false;
	}

	// Process data waiting to be received by BeebEm straight away
	if (OutputBuffer.HasData())
	{
		return true;
	}

	if (InputBuffer.HasData())
	{
		unsigned char data = InputBuffer.GetData();

		switch (data)
		{
			case 'M':
				TouchScreenMode = 0;
				break;

			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				TouchScreenMode = TouchScreenMode * 10 + data - '0';
				break;

			case '.':
				// Mode 1 seems to be polled, sends a '?' and we reply with data
				// Mode 129 seems to be send current values all time
				DebugTrace("Setting touch screen mode to %d\n", TouchScreenMode);
				break;

			case '?':
				if (TouchScreenMode == 1)
				{
					// Polled mode
					TouchScreenReadScreen(false);
				}
				break;
		}
	}

	if (TouchScreenMode == 129)
	{
		// Real time mode
		TouchScreenReadScreen(true);
	}
	else if (TouchScreenMode == 130)
	{
		// Area mode - seems to be pressing with two fingers which we can't really do ??
		TouchScreenReadScreen(true);
	}

	return false;
}

/*--------------------------------------------------------------------------*/

void TouchScreenWrite(unsigned char Data)
{
	// DebugTrace("TouchScreenWrite 0x%02x\n", Data);

	if (InputBuffer.PutData(Data))
	{
		TouchScreenDelay = TS_DELAY;
	}
	else
	{
		DebugTrace("TouchScreenWrite input buffer full\n");
	}
}

/*--------------------------------------------------------------------------*/

unsigned char TouchScreenRead()
{
	unsigned char Data = 0;

	if (OutputBuffer.HasData())
	{
		Data = OutputBuffer.GetData();
		TouchScreenDelay = TS_DELAY;
	}
	else
	{
		DebugTrace("TouchScreenRead output buffer empty\n");
	}

	// DebugTrace("TouchScreenRead 0x%02x\n", Data);

	return Data;
}

/*--------------------------------------------------------------------------*/

void TouchScreenClose()
{
}

/*--------------------------------------------------------------------------*/

void TouchScreenStore(unsigned char Data)
{
	if (!OutputBuffer.PutData(Data))
	{
		DebugTrace("TouchScreenStore output buffer full\n");
	}
}

/*--------------------------------------------------------------------------*/

void TouchScreenReadScreen(bool Check)
{
	static int last_x = -1, last_y = -1, last_m = -1;

	int x = (65535 - JoystickX) / (65536 / 120) + 1;
	int y = JoystickY / (65536 / 90) + 1;

	if (last_x != x || last_y != y || last_m != AMXButtons || !Check)
	{
		// DebugTrace("JoystickX = %d, JoystickY = %d, last_x = %d, last_y = %d\n", JoystickX, JoystickY, last_x, last_y);

		if (AMXButtons & AMX_LEFT_BUTTON)
		{
			TouchScreenStore(64 + ((x & 0xf0) >> 4));
			TouchScreenStore(64 + (x & 0x0f));
			TouchScreenStore(64 + ((y & 0xf0) >> 4));
			TouchScreenStore(64 + (y & 0x0f));
			TouchScreenStore('.');
			// DebugTrace("Sending X = %d, Y = %d\n", x, y);
		}
		else
		{
			TouchScreenStore(64 + 0x0f);
			TouchScreenStore(64 + 0x0f);
			TouchScreenStore(64 + 0x0f);
			TouchScreenStore(64 + 0x0f);
			TouchScreenStore('.');
			// DebugTrace("Screen not touched\n");
		}

		last_x = x;
		last_y = y;
		last_m = AMXButtons;
	}
}
