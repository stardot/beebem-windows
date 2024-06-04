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
static int RTC_data = 0;               // Mon   Year  Day         Hour        Min
unsigned char UserPortRTCRegisters[8] = { 0x12, 0x00, 0x05, 0x00, 0x05, 0x00, 0x07, 0x00 };

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

				const int Register = (RTC_cmd & 0x0f) >> 1;
				unsigned char Data = (unsigned char)(RTC_cmd >> 4);

				switch (Register)
				{
					case 0: // Month counter (1-12)
						if (Data == 0)
						{
							// Write is ignored
						}
						else if (Data >= 1 && Data < 20)
						{
							UserPortRTCRegisters[Register] = Data;
						}
						else if (Data >= 20 && Data < 60)
						{
							UserPortRTCRegisters[Register] = 19;
						}
						else if (Data >= 60 && Data < 80)
						{
							UserPortRTCRegisters[Register] = (unsigned char)(Data - 60);
						}
						else if (Data >= 80 && Data < 100)
						{
							UserPortRTCRegisters[Register] = (unsigned char)(Data - 80);
						}
						else if (Data >= 100 && Data < 120)
						{
							UserPortRTCRegisters[Register] = (unsigned char)(Data - 100);
						}
						else if (Data >= 120 && Data < 160)
						{
							UserPortRTCRegisters[Register] = 19;
						}
						else if (Data >= 160 && Data < 180)
						{
							UserPortRTCRegisters[Register] = (unsigned char)(Data - 160);
						}
						else if (Data >= 180 && Data < 200)
						{
							UserPortRTCRegisters[Register] = (unsigned char)(Data - 180);
						}
						else if (Data >= 200 && Data < 220)
						{
							UserPortRTCRegisters[Register] = (unsigned char)(Data - 200);
						}
						else if (Data >= 220)
						{
							UserPortRTCRegisters[Register] = 19;
						}
						break;

					case 1: // Month register (alarm)
						UserPortRTCRegisters[Register] = (unsigned char)(Data % 20);
						break;

					case 2: // Date counter (1-31)
					case 3: // Date register (alarm)
					case 4: // Hour counter (0-23)
					case 5: // Hour register (alarm)
						if (Data < 40)
						{
							UserPortRTCRegisters[Register] = Data;
						}
						else if (Data >= 40 && Data < 80)
						{
							UserPortRTCRegisters[Register] = (unsigned char)(Data - 40);
						}
						else if (Data >= 80 && Data < 100)
						{
							UserPortRTCRegisters[Register] = (unsigned char)(Data - 80);
						}
						else if (Data >= 100 && Data < 140)
						{
							UserPortRTCRegisters[Register] = (unsigned char)(Data - 100);
						}
						else if (Data >= 140 && Data < 180)
						{
							UserPortRTCRegisters[Register] = (unsigned char)(Data - 140);
						}
						else if (Data >= 180 && Data < 200)
						{
							UserPortRTCRegisters[Register] = (unsigned char)(Data - 180);
						}
						else if (Data >= 200 && Data < 240)
						{
							UserPortRTCRegisters[Register] = (unsigned char)(Data - 200);
						}
						else
						{
							UserPortRTCRegisters[Register] = (unsigned char)(Data - 240);
						}
						break;

					case 6: // Minute counter (0-59)
					case 7: // Minute register (alarm)
						if (Data < 80)
						{
							UserPortRTCRegisters[Register] = Data;
						}
						else if (Data >= 80 && Data < 100)
						{
							UserPortRTCRegisters[Register] = (unsigned char)(Data - 80);
						}
						else if (Data >= 100 && Data < 180)
						{
							UserPortRTCRegisters[Register] = (unsigned char)(Data - 100);
						}
						else if (Data >= 180 && Data < 200)
						{
							UserPortRTCRegisters[Register] = (unsigned char)(Data - 180);
						}
						else if (Data >= 200)
						{
							UserPortRTCRegisters[Register] = (unsigned char)(Data - 200);
						}
						break;
				}

				DebugTrace("UserPortRTC Write cmd : 0x%03x, reg : 0x%02x, data = 0x%02x, wrapped = 0x%02x\n", RTC_cmd, Register, (unsigned char)(RTC_cmd >> 4), Data);

				UserPortRTCRegisters[Register] = Data;
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

					case 1: // Month register (alarm)
						RTC_data = UserPortRTCRegisters[1];
						break;

					case 2: // Date counter (1-31)
						RTC_data = BCD((unsigned char)Time.wDay);
						break;

					case 3: // Date register (alarm)
						RTC_data = UserPortRTCRegisters[3];
						break;

					case 4: // Hour counter (0-23)
						RTC_data = BCD((unsigned char)Time.wHour);
						break;

					case 5: // Hour register (alarm)
						RTC_data = UserPortRTCRegisters[5];
						break;

					case 6: // Minute counter (0-59)
						RTC_data = BCD((unsigned char)Time.wMinute);
						break;

					case 7: // Minute register (alarm)
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
