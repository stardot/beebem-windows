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

/* CMOS Ram finalised 06/01/2001 - Richard Gellman */

#include <windows.h>

#include <stdio.h>
#include <time.h>

#include "Rtc.h"
#include "Bcd.h"
#include "Debug.h"
#include "DebugTrace.h"
#include "Main.h"
#include "Model.h"

/*-------------------------------------------------------------------------*/

// Master 128 MC146818AP Real-Time Clock and RAM
static time_t RTCTimeOffset = 0;

struct CMOSType
{
	bool ChipEnable;
	unsigned char Address;
	// RTC registers + 50 bytes CMOS RAM for Master 128 and Master ET
	unsigned char Register[2][64];
};

static CMOSType CMOS;

// RTC Registers in memory
//
// 0x0: Seconds (0 to 59)
// 0x1: Alarm Seconds (0 to 59)
// 0x2: Minutes (0 to 59)
// 0x3: Alarm Minutes (0 to 59)
// 0x4: Hours (0 to 23)
// 0x5: Alarm Hours (0 to 23)
// 0x6: Day of week (1 = Sun, 7 = Sat)
// 0x7: Day of month (1 to 31)
// 0x8: Month (1 = Jan, 12 = Dec)
// 0x9: Year (last 2 digits)
// 0xA: Register A
// 0xB: Register B
// 0xC: Register C
// 0xD: Register D
// 0xE to 0x3F: User RAM (50 bytes)

const unsigned char RTC146818_REG_A = 0x0A;
const unsigned char RTC146818_REG_B = 0x0B;
const unsigned char RTC146818_REG_C = 0x0C;
const unsigned char RTC146818_REG_D = 0x0D;

const unsigned char RTC146818_REG_B_SET    = 0x80;
const unsigned char RTC146818_REG_B_BINARY = 0x04;

// Backup of Master 128 CMOS defaults from byte 14 (RAM bytes only)
const unsigned char CMOSDefault_Master128[50] =
{
	0x01, 0xfe, 0x00, 0xea, 0x00,
	0xc9, 0xff, 0xfe, 0x32, 0x00,
	0x07, 0xc1, 0x1e, 0x05, 0x00,
	0x59, 0xa2
};

// Backup of Master ET CMOS defaults from byte 14 (RAM bytes only)
const unsigned char CMOSDefault_MasterET[50] =
{
	0x01, 0xfe, 0x00, 0xea, 0x00,
	0xde, 0xff, 0xff, 0x32, 0x00,
	0x07, 0xc1, 0x1e, 0x05, 0x00,
	0x59, 0xa2
};

// Pointer used to switch between the CMOS RAM for the different machines
unsigned char* pCMOS = nullptr;

/*-------------------------------------------------------------------------*/

static time_t RTCConvertClock()
{
	struct tm Base;

	if (pCMOS[RTC146818_REG_B] & RTC146818_REG_B_BINARY)
	{
		Base.tm_sec   = pCMOS[0];
		Base.tm_min   = pCMOS[2];
		Base.tm_hour  = pCMOS[4];
		Base.tm_mday  = pCMOS[7];
		Base.tm_mon   = pCMOS[8] - 1;
		Base.tm_year  = pCMOS[9];
	}
	else
	{
		Base.tm_sec   = BCDToBin(pCMOS[0]);
		Base.tm_min   = BCDToBin(pCMOS[2]);
		Base.tm_hour  = BCDToBin(pCMOS[4]);
		Base.tm_mday  = BCDToBin(pCMOS[7]);
		Base.tm_mon   = BCDToBin(pCMOS[8]) - 1;
		Base.tm_year  = BCDToBin(pCMOS[9]);
	}

	Base.tm_wday  = -1;
	Base.tm_yday  = -1;
	Base.tm_isdst = -1;

	time_t SysTime;
	time(&SysTime);
	struct tm *CurTime = localtime(&SysTime);
	Base.tm_year += (CurTime->tm_year / 100) * 100;

	return mktime(&Base);
}

/*-------------------------------------------------------------------------*/

void RTCInit()
{
	// Select CMOS memory
	switch (MachineType)
	{
		case Model::Master128:
			pCMOS = CMOS.Register[0]; // Pointer to Master 128 CMOS
			break;

		case Model::MasterET:
			pCMOS = CMOS.Register[1]; // Pointer to Master ET CMOS
			break;

		default: // Machine doesn't use CMOS
			pCMOS = nullptr;
			if (DebugEnabled)
				DebugDisplayTrace(DebugType::CMOS, true, "RTC: Timer off");
			return;
	}

	time_t SysTime;
	time(&SysTime);
	struct tm *CurTime = localtime(&SysTime);

	if (pCMOS[RTC146818_REG_B] & RTC146818_REG_B_BINARY)
	{
		pCMOS[0] = static_cast<unsigned char>(CurTime->tm_sec);
		pCMOS[2] = static_cast<unsigned char>(CurTime->tm_min);
		pCMOS[4] = static_cast<unsigned char>(CurTime->tm_hour);
		pCMOS[6] = static_cast<unsigned char>(CurTime->tm_wday + 1);
		pCMOS[7] = static_cast<unsigned char>(CurTime->tm_mday);
		pCMOS[8] = static_cast<unsigned char>(CurTime->tm_mon + 1);
		pCMOS[9] = static_cast<unsigned char>(CurTime->tm_year % 100);
	}
	else
	{
		pCMOS[0] = BCD(static_cast<unsigned char>(CurTime->tm_sec));
		pCMOS[2] = BCD(static_cast<unsigned char>(CurTime->tm_min));
		pCMOS[4] = BCD(static_cast<unsigned char>(CurTime->tm_hour));
		pCMOS[6] = BCD(static_cast<unsigned char>(CurTime->tm_wday + 1));
		pCMOS[7] = BCD(static_cast<unsigned char>(CurTime->tm_mday));
		pCMOS[8] = BCD(static_cast<unsigned char>(CurTime->tm_mon + 1));
		pCMOS[9] = BCD(static_cast<unsigned char>(CurTime->tm_year % 100));
	}

	RTCTimeOffset = SysTime - RTCConvertClock();
}

/*-------------------------------------------------------------------------*/

static void RTCUpdate()
{
	time_t SysTime;
	time(&SysTime);
	SysTime -= RTCTimeOffset;
	struct tm *CurTime = localtime(&SysTime);

	if (pCMOS[RTC146818_REG_B] & RTC146818_REG_B_BINARY)
	{
		// Update with current time values in binary format
		pCMOS[0] = static_cast<unsigned char>(CurTime->tm_sec);
		pCMOS[2] = static_cast<unsigned char>(CurTime->tm_min);
		pCMOS[4] = static_cast<unsigned char>(CurTime->tm_hour);
		pCMOS[6] = static_cast<unsigned char>(CurTime->tm_wday + 1);
		pCMOS[7] = static_cast<unsigned char>(CurTime->tm_mday);
		pCMOS[8] = static_cast<unsigned char>(CurTime->tm_mon + 1);
		pCMOS[9] = static_cast<unsigned char>(CurTime->tm_year % 100);
	}
	else
	{
		// Update with current time values in BCD format
		pCMOS[0] = BCD(static_cast<unsigned char>(CurTime->tm_sec));
		pCMOS[2] = BCD(static_cast<unsigned char>(CurTime->tm_min));
		pCMOS[4] = BCD(static_cast<unsigned char>(CurTime->tm_hour));
		pCMOS[6] = BCD(static_cast<unsigned char>(CurTime->tm_wday + 1));
		pCMOS[7] = BCD(static_cast<unsigned char>(CurTime->tm_mday));
		pCMOS[8] = BCD(static_cast<unsigned char>(CurTime->tm_mon + 1));
		pCMOS[9] = BCD(static_cast<unsigned char>(CurTime->tm_year % 100));
	}
}

/*-------------------------------------------------------------------------*/

void RTCChipEnable(bool Enable)
{
	CMOS.ChipEnable = Enable;
}

bool RTCIsChipEnable()
{
	return CMOS.ChipEnable;
}

/*-------------------------------------------------------------------------*/

void RTCWriteAddress(unsigned char Address)
{
	DebugTrace("RTC: Write address %02X\n", Address);

	CMOS.Address = Address;
}

/*-------------------------------------------------------------------------*/

void RTCWriteData(unsigned char Value)
{
	DebugTrace("RTC: Write data: address %02X value %02X\n", CMOS.Address, Value);

	if (DebugEnabled)
	{
	  DebugDisplayTraceF(DebugType::CMOS, true,
	                     "CMOS: Write address %X value %02X",
	                     CMOS.Address, Value);
	}

	// Many thanks to Tom Lees for supplying me with info on the 146818 registers
	// for these two functions.
	if (CMOS.Address <= 0x9)
	{
		// Clock registers

		// BCD or binary format?
		if (pCMOS[RTC146818_REG_B] & RTC146818_REG_B_BINARY)
		{
			pCMOS[CMOS.Address] = BCD(Value);
		}
		else
		{
			pCMOS[CMOS.Address] = Value;
		}
	}
	else if (CMOS.Address == RTC146818_REG_A)
	{
		// Control register A
		pCMOS[CMOS.Address] = Value & 0x7f; // Top bit not writable
	}
	else if (CMOS.Address == RTC146818_REG_B)
	{
		// Control register B
		// Bit-7 SET - 0=clock running, 1=clock update halted
		if (Value & RTC146818_REG_B_SET)
		{
			RTCUpdate();
		}
		else if ((pCMOS[CMOS.Address] & RTC146818_REG_B_SET) && !(Value & RTC146818_REG_B_SET))
		{
			// New clock settings
			time_t SysTime;
			time(&SysTime);
			RTCTimeOffset = SysTime - RTCConvertClock();
		}

		// Write the value to CMOS anyway
		pCMOS[CMOS.Address] = Value;
	}
	else if (CMOS.Address == RTC146818_REG_C || CMOS.Address == RTC146818_REG_D)
	{
		// Control register C and D - read only
	}
	else if (CMOS.Address < 0x40)
	{
		// User RAM
		pCMOS[CMOS.Address] = Value;
	}
}

/*-------------------------------------------------------------------------*/

unsigned char RTCReadData()
{
	if (CMOS.Address <= 0x9)
	{
		RTCUpdate();
	}

	unsigned char Value = pCMOS[CMOS.Address];

	if (DebugEnabled)
	{
	  DebugDisplayTraceF(DebugType::CMOS, true,
	                     "CMOS: Read address %X value %02X",
	                     CMOS.Address, Value);
	}

	DebugTrace("RTC: Read address %02X value %02X\n", CMOS.Address, Value);

	return Value;
}

/*-------------------------------------------------------------------------*/

void RTCSetCMOSDefaults(Model model)
{
	switch (model)
	{
		case Model::Master128:
		default:
			memcpy(&CMOS.Register[0][14], CMOSDefault_Master128, 50);
			break;

		case Model::MasterET:
			memcpy(&CMOS.Register[1][14], CMOSDefault_MasterET, 50);
			break;
	}
}

/*-------------------------------------------------------------------------*/

unsigned char RTCGetData(Model model, unsigned char Address)
{
	const int Index = model == Model::Master128 ? 0 : 1;

	return CMOS.Register[Index][Address];
}

const unsigned char* RTCGetCMOSData(Model model)
{
	const int Index = model == Model::Master128 ? 0 : 1;

	return &CMOS.Register[Index][14];
}

void RTCSetCMOSData(Model model, const unsigned char* pData, int Size)
{
	const int Index = model == Model::Master128 ? 0 : 1;

	memcpy(&CMOS.Register[Index][14], pData, Size);
}