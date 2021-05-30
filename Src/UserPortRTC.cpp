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

#ifdef WIN32
#include <windows.h>
#include "main.h"
#endif

#include <time.h>
#include "rtc.h"
#include "debug.h"
#include "log.h"

/* Real Time Clock */
int userPortRTC_bit = 0;
int RTC_cmd = 0;
int userPortRTC_data = 0; // Mon    Yr   Day         Hour        Min
unsigned char RTC_ram[8] = { 0x12, 0x01, 0x05, 0x00, 0x05, 0x00, 0x07, 0x00 };
bool userPortRTC_Enabled = false;
signed char userPortRTC_YearCorrection;

/*--------------------------------------------------------------------------*/
void UserPortRTCWrite(int Value, int lastValue)
{
	if (((lastValue & 0x02) == 0x02) && ((Value & 0x02) == 0x00))		// falling clock edge
	{
		if ((Value & 0x04) == 0x04)
		{
			RTC_cmd = (RTC_cmd >> 1) | ((Value & 0x01) << 15);
			userPortRTC_bit++;

			WriteLog("RTC Shift cmd : 0x%03x, bit : %d\n", RTC_cmd, userPortRTC_bit);
		}
		else
		{
			if (userPortRTC_bit == 11) // Write data
			{
				RTC_cmd >>= 5;

				WriteLog("RTC Write cmd : 0x%03x, reg : 0x%02x, data = 0x%02x\n", RTC_cmd, (RTC_cmd & 0x0f) >> 1, RTC_cmd >> 4);

				RTC_ram[(RTC_cmd & 0x0f) >> 1] = (unsigned char)(RTC_cmd >> 4);
			}
			else
			{
				RTC_cmd >>= 12;

				time_t SysTime;
				time(&SysTime);

				struct tm* CurTime = localtime(&SysTime);

				switch ((RTC_cmd & 0x0f) >> 1)
				{
				case 0:
					userPortRTC_data = BCD((unsigned char)(CurTime->tm_mon + 1));
					break;
				case 1:
					// userPortRTC_data = BCD((CurTime->tm_year % 100) - 1); 
					// value returned was 20 years behind for L3 FileServer v1.40
					// implemented this as a value in the preferences file as different programs use different 
					// calculations
					userPortRTC_data = BCD(((CurTime->tm_year % 100) + userPortRTC_YearCorrection));
					break;
				case 2:
					userPortRTC_data = BCD((unsigned char)CurTime->tm_mday);
					break;
				case 3:
					userPortRTC_data = RTC_ram[3];
					break;
				case 4:
					userPortRTC_data = BCD((unsigned char)CurTime->tm_hour);
					break;
				case 5:
					userPortRTC_data = RTC_ram[5];
					break;
				case 6:
					userPortRTC_data = BCD((unsigned char)CurTime->tm_min);
					break;
				case 7:
					userPortRTC_data = RTC_ram[7];
					break;
				}

				WriteLog("RTC Write cmd : 0x%03x, reg : 0x%02x, data : 0x%02x\n", RTC_cmd, (RTC_cmd & 0x0f) >> 1, userPortRTC_data);
			}
		}
	}
}
