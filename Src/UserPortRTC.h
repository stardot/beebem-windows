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

/* User Port RTC Dongle - not to be confused with the RTC chip in 
the Master128 and FileStores.

Code moved here from User VIA */

#ifndef USER_PORT_RTC_HEADER
#define USER_PORT_RTC_HEADER

extern bool userPortRTC_Enabled;
extern int userPortRTC_bit;
extern int userPortRTC_data;
extern signed char userPortRTC_YearCorrection;

void UserPortRTCWrite(int Value, int lastValue);

#endif
