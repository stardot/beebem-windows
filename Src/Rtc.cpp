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

// The Acorn Master Series uses a HD146818. This provides a RTC (Real Time
// Clock), a programmable periodic interrupt and square wave generator
// (which BeebEm doesn't emulate), and 50 bytes CMOS RAM.
//
//                                 Memory Map
//
// +----+----+-------------------------------------------------+---------------+
// | Address | Description                                     |   DM=0    =1  |
// +----+----+-------------------------------------------------+---------------+
// | 00 | 00 | Seconds                                         | Binary or BCD |
// | 01 | 01 | Seconds Alarm                                   | Binary or BCD |
// | 02 | 02 | Minutes                                         | Binary or BCD |
// | 03 | 03 | Minutes Alarm                                   | Binary or BCD |
// | 04 | 04 | Hours                                           | Binary or BCD |
// | 05 | 05 | Hours Alarm                                     | Binary or BCD |
// | 06 | 06 | Day of Week (1 = Sun, 7 = Sat)                  | Binary or BCD |
// | 07 | 07 | Date of Month                                   | Binary or BCD |
// | 08 | 08 | Month (1 = Jan, 12 = Dec)                       | Binary or BCD |
// | 09 | 09 | Year                                            | Binary or BCD |
// +----+----+-------------------------------------------------+---------------+
// | 10 | 0A | Register A  |  UIP | DV2 | DV1 | DV0 |  RS3 | RS2 |  RS1  | RS0 |
// | 11 | 0B | Register B  |  SET | PIE | AIE | UIE | SQWE |  DM | 24/12 | DSE |
// | 12 | 0C | Register C  | IRQF | PF  | AF  | UF  |   -  |  -  |   -   |  -  |
// | 13 | 0D | Register D  |  VRT |  -  |  -  |  -  |   -  |  -  |   -   |  -  |
// +----+----+-----------------------------------------------------------------+
// | 50 bytes of user RAM                                                      |
// |---------------------------------------------------------------------------|
// | Master 128 http://beebwiki.mdfs.net/CMOS_configuration_RAM_allocation     |
// | See https://chrisacorns.computinghistory.org.uk/docs/Acorn/AN/203.pdf for |
// | factory default settings                                                  |
// |---------------------------------------------------------------------------|
// | 14 | 0E |       Econet station number                *SetStation nnn      |
// | 15 | 0F |       file server number                   *CO. FS nnn          |
// | 16 | 10 |       File server network                  *CO. FS nnn.sss      |
// | 17 | 11 |       Printer server number                *CO. PS nnn          |
// | 18 | 12 |       Printer server network               *CO. PS nnn.sss      |
// | 19 | 13 | b0-b3 Default filing system ROM            *CO. File nn         |
// |    |    | b4-b7 Default lanugage ROM                 *CO. Lang nn         |
// | 20 | 14 |       ROMs 0-7 unplugged/inserted          *Insert n/*Unplug n  |
// | 21 | 15 |       ROMs 8-F unplugged/inserted          *Insert n/*Unplug n  |
// | 22 | 16 | b0-b2 EDIT screen mode                                          |
// |    |    | b3    EDIT TAB to columns/words                                 |
// |    |    | b4    EDIT overwrite/instert                                    |
// |    |    | b5    EDIT display/returns                                      |
// |    |    | b6-b7 Not used                                                  |
// | 23 | 17 |       Telecoms software                                         |
// | 24 | 18 | b0-b2 Default screen mode                  *CO. Mode nn         |
// |    |    | b3    Shadow screen mode (128-135)                              |
// |    |    | b4    Default TV interlace                 *CO. TV xx,n         |
// |    |    | b5-b7 Default TV position 0-3, -4 to -1    *CO. TV nn,x         |
// | 25 | 19 | b0-b2 Default floppy speed                 *CO. FDrive n        |
// |    |    | b3    Shift Caps on startup                *CO. ShCaps          |
// |    |    | b4    No CAPS lock on startup              *CO. NoCaps          |
// |    |    | b5    CAPS lock on startup                 *CO. Caps            |
// |    |    | b6    ADFS load dir on startup             *CO. NoDir/Dir       |
// |    |    | b7    ADFS floppy/hard drive on startup    *CO. Floppy/Hard     |
// | 26 | 1A |       Keyboard repeat delay                *CO. Delay nnn       |
// | 27 | 1B |       Keyboard repeat rate                 *CO. Repeat nnn      |
// | 28 | 1C |       Printer ignore character             *CO. Ignore nnn      |
// | 29 | 1D | b0    Ignore/enable Tube                   *CO. NoTube/Tube     |
// |    |    | b1    Ignore printer ignore character      *CO. Ignore nnn      |
// |    |    | b2-b4 Default RS232 serial speed, 0-7      *CO. Baud n          |
// |    |    | b5-b7 Default printer device, 0-7          *CO. Print n         |
// | 30 | 1E | b0    Default to shadow screen on start    *CO. Shadow          |
// |    |    | b1    Default BELL (Ctrl+G) volume         *CO. Quiet/Loud      |
// |    |    | b2    Internal/External Tube               *CO. InTube/ExTube   |
// |    |    | b3    Scrolling enabled/protected          *CO. Scroll/NoScroll |
// |    |    | b4    Noboot/boot on reset                 *CO. NoBoot/Boot     |
// |    |    | b5-b7 Default serial data format           *CO. Data n          |
// | 31 | 1F | b0    ANFS raise 2 pages of workspace      *CO. NoSpace/Space   |
// |    |    | b1    ANFS run *FindLib on logon           *-Net-Opt 5,n        |
// |    |    | b2    ANFS uses &0Bxx-&0Cxx or &0Exx-&0Fxx *-Net-Opt 6,n        |
// |    |    | b3-b5 Unused                                                    |
// |    |    | b6    ANFS protected                       *-Net-Opt 8,n        |
// |    |    | b7    Display version number on startup                         |
// | 32 | 20 |       Unused                                                    |
// | 33 | 21 |       Unused                                                    |
// | 34 | 22 |       34-43 Reserved for Acorn future expansion                 |
// | 35 | 23 |                                                                 |
// | 36 | 24 |                                                                 |
// | 37 | 25 |                                                                 |
// | 38 | 26 |                                                                 |
// | 39 | 27 |                                                                 |
// | 40 | 28 |                                                                 |
// | 41 | 29 |                                                                 |
// | 42 | 2A |                                                                 |
// | 43 | 2B |                                                                 |
// | 44 | 2C |       44-53 Reserved for use by commercial third parties        |
// | 45 | 2D |                                                                 |
// | 46 | 2E |                                                                 |
// | 47 | 2F |                                                                 |
// | 48 | 30 |                                                                 |
// | 49 | 31 |                                                                 |
// | 50 | 32 |                                                                 |
// | 51 | 33 |                                                                 |
// | 52 | 34 |                                                                 |
// | 53 | 35 |                                                                 |
// | 54 | 36 |       54-63 Reserved for use by users                           |
// | 55 | 37 |                                                                 |
// | 56 | 38 |                                                                 |
// | 57 | 39 |                                                                 |
// | 58 | 3A |                                                                 |
// | 59 | 3B |                                                                 |
// | 60 | 3C |                                                                 |
// | 61 | 3D |                                                                 |
// | 62 | 3E |                                                                 |
// | 63 | 3F |                                                                 |
// +----+----+-----------------------------------------------------------------+

// Offset from PC clock (seconds)
static time_t RTCTimeOffset = 0;

struct CMOSType
{
	bool ChipEnable;
	unsigned char Address;
	// RTC registers + 50 bytes CMOS RAM for Master 128 and Master ET
	unsigned char Register[2][64];
};

static CMOSType CMOS;

const unsigned char RTC146818_REG_A = 0x0A;
const unsigned char RTC146818_REG_B = 0x0B;
const unsigned char RTC146818_REG_C = 0x0C;
const unsigned char RTC146818_REG_D = 0x0D;

const unsigned char RTC146818_REG_B_SET    = 0x80;
const unsigned char RTC146818_REG_B_BINARY = 0x04;

// Master 128 CMOS defaults from byte 14 (RAM bytes only)

const unsigned char CMOSDefault_Master128[50] =
{
	0x01, // 14 - Econet station number: 1
	0xfe, // 15 - File server number: 254
	0x00, // 16 - File server network: 0
	0xea, // 17 - Printer server number: 235
	0x00, // 18 - Printer server network: 0
	0xc9, // 19 - Default language ROM: 12, Default filing system ROM: 9
	0xff, // 20 - ROMS 0-7 unplugged/inserted
	0xfe, // 21 - ROMS 8-F unplugged/inserted
	0x32, // 22 - Edit mode K
	0x00, // 23
	0x07, // 24 - Default screen mode 7, *TV 0, 0
	0xc0, // 25 - Default floppy speed: 10, Floppy, NoDir, Caps
	0x1e, // 26 - Keyboard auto-repeat delay: 30cs
	0x08, // 27 - Keyboard repeat rate: 8cs
	0x00, // 28 - Printer ignore character
	0x31, // 29 - Parallel port printer, RS232 1200 baud, Tube enabled
	0xa2  // 30 - Serial format 8N1, no boot on reset, scroll enabled, external tube, Loud
};

// Master ET CMOS defaults from byte 14 (RAM bytes only)

const unsigned char CMOSDefault_MasterET[50] =
{
	0x01, // 14 - Econet station number: 1
	0xfe, // 15 - File server number: 254
	0x00, // 16 - File server network: 0
	0xea, // 17 - Printer server number: 235
	0x00, // 18 - Printer server network: 0
	0xde, // 19 - Default language ROM: 13, Default filing system ROM: 14
	0xff, // 20 - ROMS 0-7 unplugged/inserted
	0xff, // 21 - ROMS 8-F unplugged/inserted
	0x32, // 22 - Edit mode K
	0x00, // 23
	0x07, // 24 - Default screen mode 7, *TV 0, 0
	0xc0, // 25 - Default floppy speed: 10, Floppy, NoDir, Caps
	0x1e, // 26 - Keyboard auto-repeat delay: 30cs
	0x08, // 27 - Keyboard repeat rate: 8cs
	0x00, // 28 - Printer ignore character
	0x31, // 29 - Parallel port printer, RS232 1200 baud, Tube enabled
	0xa2  // 30 - Serial format 8N1, no boot on reset, scroll enabled, external tube, Loud
};

// Pointer used to switch between the CMOS RAM for the different machines
unsigned char* pCMOS = nullptr;

/*-------------------------------------------------------------------------*/

// Convert the contents of the RTC registers to a time_t, applying a
// correction based on the PC clock because the RTC only stores a two-digit
// year.

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
		Base.tm_year  = BCDToBin(pCMOS[9]); // Years since 1900
	}

	Base.tm_wday  = -1;
	Base.tm_yday  = -1;
	Base.tm_isdst = -1;

	// Add the century from the PC clock.
	time_t SysTime;
	time(&SysTime);
	struct tm *CurTime = localtime(&SysTime);
	Base.tm_year += (CurTime->tm_year / 100) * 100;

	return mktime(&Base);
}

/*-------------------------------------------------------------------------*/

static void RTCSetTimeAndDateRegisters(const struct tm* pTime)
{
	if (pCMOS[RTC146818_REG_B] & RTC146818_REG_B_BINARY)
	{
		// Update with current time values in binary format
		pCMOS[0] = static_cast<unsigned char>(pTime->tm_sec);
		pCMOS[2] = static_cast<unsigned char>(pTime->tm_min);
		pCMOS[4] = static_cast<unsigned char>(pTime->tm_hour);
		pCMOS[6] = static_cast<unsigned char>(pTime->tm_wday + 1);
		pCMOS[7] = static_cast<unsigned char>(pTime->tm_mday);
		pCMOS[8] = static_cast<unsigned char>(pTime->tm_mon + 1);
		pCMOS[9] = static_cast<unsigned char>(pTime->tm_year % 100);
	}
	else
	{
		// Update with current time values in BCD format
		pCMOS[0] = BCD(static_cast<unsigned char>(pTime->tm_sec));
		pCMOS[2] = BCD(static_cast<unsigned char>(pTime->tm_min));
		pCMOS[4] = BCD(static_cast<unsigned char>(pTime->tm_hour));
		pCMOS[6] = BCD(static_cast<unsigned char>(pTime->tm_wday + 1));
		pCMOS[7] = BCD(static_cast<unsigned char>(pTime->tm_mday));
		pCMOS[8] = BCD(static_cast<unsigned char>(pTime->tm_mon + 1));
		pCMOS[9] = BCD(static_cast<unsigned char>(pTime->tm_year % 100));
	}
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

	RTCSetTimeAndDateRegisters(CurTime);

	RTCTimeOffset = SysTime - RTCConvertClock();

	DebugTrace("RTCInit: RTCTimeOffset set to %d\n", (int)RTCTimeOffset);
}

/*-------------------------------------------------------------------------*/

static void RTCUpdate()
{
	time_t SysTime;
	time(&SysTime);
	SysTime -= RTCTimeOffset;
	struct tm *CurTime = localtime(&SysTime);

	RTCSetTimeAndDateRegisters(CurTime);
}

/*-------------------------------------------------------------------------*/

void RTCChipEnable(bool Enable)
{
	CMOS.ChipEnable = Enable;
}

/*-------------------------------------------------------------------------*/

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
		DebugTrace("RTCWriteData: Set register %d to %02X\n", CMOS.Address, Value);

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

			DebugTrace("RTCWriteData: RTCTimeOffset set to %d\n", (int)RTCTimeOffset);
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

/*-------------------------------------------------------------------------*/

const unsigned char* RTCGetCMOSData(Model model)
{
	const int Index = model == Model::Master128 ? 0 : 1;

	return &CMOS.Register[Index][14];
}

/*-------------------------------------------------------------------------*/

void RTCSetCMOSData(Model model, const unsigned char* pData, int Size)
{
	const int Index = model == Model::Master128 ? 0 : 1;

	memcpy(&CMOS.Register[Index][14], pData, Size);
}

/*-------------------------------------------------------------------------*/
