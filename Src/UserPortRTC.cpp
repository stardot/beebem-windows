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

#include <windows.h>

#include <stdio.h>
#include <time.h>

#include "UserPortRTC.h"
#include "Bcd.h"
#include "DebugTrace.h"

/*--------------------------------------------------------------------------*/

// Acorn Econet Level 3 File Server Real Time Clock (SAF3019P clock/timer)
//
//
// From https://stardot.org.uk/forums/viewtopic.php?p=351865#p351865
//
// "The even numbered registers (0,2,4,6) are the current time registers months,
// days, hours, minutes. These are called "counters" in the datasheet.
//
// The odd numbered registers (1,3,5,7) are called "time registers" in the
// datasheet and I think are officially used for an alarm function, but the L3
// server uses these registers for other purposes. Register 1 is where the year
// is stored. Although it's the month "time register", it looks to have 5 data
// bits according to the datasheet, but testing just now seems to show that it
// can only be loaded with values 0-19. In theory that means the L3 server
// should support dates up to 2000 (the year stored in the RTC having an offset
// of 1981 added to give the actual year) but I don't think it does."

bool UserPortRTCEnabled = false;

static int RTC_bit = 0;
static int RTC_cmd = 0;
static int RTC_data = 0;              // Mon   Year  Day         Hour        Min
unsigned char UserPortRTCRegisters[8] = {0x12, 0x00, 0x05, 0x00, 0x05, 0x00, 0x07, 0x00};

static unsigned char LastValue = 0xff;

/*--------------------------------------------------------------------------*/

void UserPortRTCWrite(unsigned char Value)
{
	if (((LastValue & 0x02) != 0) && ((Value & 0x02) == 0)) // falling clock edge
	{
		if ((Value & 0x04) != 0)
		{
			RTC_cmd = (RTC_cmd >> 1) | ((Value & 0x01) << 15);
			RTC_bit++;

			DebugTrace("UserPortRTC Shift cmd : 0x%03x, bit : %d\n", RTC_cmd, RTC_bit);
		}
		else
		{
			if (RTC_bit == 11) // Write data
			{
				RTC_cmd >>= 5;

				DebugTrace("UserPortRTC Write cmd : 0x%03x, reg : 0x%02x, data = 0x%02x\n", RTC_cmd, (RTC_cmd & 0x0f) >> 1, RTC_cmd >> 4);

				UserPortRTCRegisters[(RTC_cmd & 0x0f) >> 1] = (unsigned char)(RTC_cmd >> 4);
			}
			else
			{
				RTC_cmd >>= 12;

				SYSTEMTIME Time;
				GetLocalTime(&Time);

				switch ((RTC_cmd & 0x0f) >> 1)
				{
					case 0: // Month counter (1-12)
						RTC_data = BCD((unsigned char)(Time.wMonth));
						break;

					case 1: // Month register
						RTC_data = UserPortRTCRegisters[1];
						break;

					case 2: // Date counter (1-31)
						RTC_data = BCD((unsigned char)Time.wDay);
						break;

					case 3: // Date register
						RTC_data = UserPortRTCRegisters[3];
						break;

					case 4: // Hour counter (0-23)
						RTC_data = BCD((unsigned char)Time.wHour);
						break;

					case 5: // Hour register
						RTC_data = UserPortRTCRegisters[5];
						break;

					case 6: // Minute counter (0-59)
						RTC_data = BCD((unsigned char)Time.wMinute);
						break;

					case 7: // Minute register
						RTC_data = UserPortRTCRegisters[7];
						break;
				}

				DebugTrace("UserPortRTC Read cmd : 0x%03x, reg : 0x%02x, data : 0x%02x\n", RTC_cmd, (RTC_cmd & 0x0f) >> 1, RTC_data);
			}
		}
	}

	LastValue = Value;
}

/*--------------------------------------------------------------------------*/

int UserPortRTCReadBit()
{
	int Value = RTC_data & 0x01;

	RTC_data >>= 1;

	DebugTrace("UserPortRTC read bit : %d\n", Value);

	return Value;
}

/*--------------------------------------------------------------------------*/

void UserPortRTCResetWrite()
{
	RTC_bit = 0;

	DebugTrace("UserPortRTC bit : %d\n", RTC_bit);
}

/*--------------------------------------------------------------------------*/
