/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2001  Richard Gellman
Copyright (C) 2021  Mark Usher

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

using namespace std;

#include "rtc.h"
#include "beebmem.h"
#include "6502core.h"
#include "debug.h"
#include "log.h"


/******************************************************************************
The Acorn Master 128 and FileStore products use a HD146818.
This provides a	RTC (Real Time Clock), a programmable periodic 
interrupt and square wave generator and 50 bytes CMOS RAM

                                Memory Map
								                                  DM=0    =1
+----+----+---------------------------------------------------+---------------+
| 00 | 00 | Seconds                                           | Binary or BCD |
| 01 | 01 | Seconds Alarm                                     | Binary or BCD |
| 02 | 02 | Minutes                                           | Binary or BCD |
| 03 | 03 | Minutes Alarm                                     | Binary or BCD |
| 04 | 04 | Hours                                             | Binary or BCD |
| 05 | 05 | Hours Alarm                                       | Binary or BCD |
| 06 | 06 | Day of Week                                       | Binary or BCD |
| 07 | 07 | Date of Month                                     | Binary or BCD |
| 08 | 08 | Month                                             | Binary or BCD |
| 09 | 09 | Year                                              | Binary or BCD |
+----+----+---------------------------------------------------+---------------+
| 10 | 0A | Register A    |  UIP | DV2 | DV1 | DV0 |  RS3 | RS2 |  RS1  | RS0 |
| 11 | 0B | Register B    |  SET | PIE | AIE | UIE | SQWE |  DM | 24/12 | DSE |
| 12 | 0C | Register C    | IRQF | PF  | AF  | UF  |   -  |  -  |   -   |  -  |
| 13 | 0D | Register D    |  VRT |  -  |  -  |  -  |   -  |  -  |   -   |  -  |
+----+----+-------------------------------------------------------------------+
| 50 bytes of user RAM                                                        |
|-----------------------------------------------------------------------------|
| Master 128 http://beebwiki.mdfs.net/CMOS_configuration_RAM_allocation       |
|-----------------------------------------------------------------------------|
|         |Default value in BeebEm                                            |
|         |hex | Description                                                  |
+----+----+-------------------------------------------------------------------+
| 14 | 0E | 00 |       Station ID number                    *SetStation nnn
| 15 | 0F | FE |       File server number                	*CO. FS nnn
| 16 | 10 | 00 |       File server network             	    *CO. FS nnn.sss
| 17 | 11 | FE |       Print server number             	    *CO. PS nnn
| 18 | 12 | 00 |       Print server network            	    *CO. PS nnn.sss
| 19 | 13 | C9 | b0-b3 Default filing system ROM     	    *CO. File nn
|    |    |    | b4-b7 Default lanugage ROM      	        *CO. Lang nn
| 20 | 14 | FF |       ROMs 0-7 unplugged/inserted     	    *Insert nn/*Unplug nn
| 21 | 15 | FE |       ROMs 8-F unplugged/inserted     	    *Insert nn/*Unplug nn
| 22 | 16 | 32 | b0-b2 EDIT screen mode
|    |    |    | b3    EDIT TAB to columns/words
|    |    |    | b4    EDIT overwrite/instert
|    |    |    | b5    EDIT display/returns
|    |    |    | b6-b7 not used
| 23 | 17 | 00 |       Telecoms software
| 24 | 18 | 07 | b0-b2 Default screen mode                  *CO. Mode nn
|    |    |    | b3    Shadow 
|    |    |    | b4    Default TV interlace          	    *CO. TV xx,n
|    |    |    | b5-b7 Default TV position 0-3, -4 to -1    *CO. TV nn,x
| 25 | 19 | C1 | b0-b2 Default floppy speed         	    *CO. FDrive n
|    |    |    | b3    Shift Caps on startup         	    *CO. ShCaps
|    |    |    | b4    No CAPS lock on startup              *CO. NoCaps
|    |    |    | b5    CAPS lock on startup                 *CO. Caps
|    |    |    | b6    ADFS load dir on startup  	        *CO. NoDir/Dir
|    |    |    | b7    ADFS floppy/hard drive on startup    *CO. Floppy/Hard
| 26 | 1A | 1E |       Keyboard repeat delay                *CO. Delay nnn
| 27 | 1B | 05 |       Keyboard repeat rate                 *CO. Repeat nnn
| 28 | 1C | 00 |       Printer ignore character             *CO. Ignore nnn
| 29 | 1D | 59 | b0    Ignore/enable Tube		            *CO. NoTube/Tube
|    |    |    | b1    Ignore printer ignore character	    *CO. Ignore/Ignore nnn
|    |    |    | b2-b4 Default serial speed, 0-7	        *CO. Baud n
|    |    |    | b5-b7 Default printer device, 0-7 	        *CO. Print n
| 30 | 1E | A2 | b0    Default to shadow screen on start    *CO. Shadow
|    |    |    | b1    Default BEEP quite/loud   	        *CO. Quiet/Loud
|    |    |    | b2    Internal/External Tube   	        *CO. InTube/ExTube
|    |    |    | b3    Scrolling enabled/protected     	    *CO. Scroll/NoScroll
|    |    |    | b4    Noboot/boot on reset     	        *CO. NoBoot/Boot
|    |    |    | b5-b7 Default serial data format      	    *CO. Data n
| 31 | 1F | 00 | b0    ANFS raise 2 pages of workspace      *CO. NoSpace/Space
|    |    |    | b1    ANFS run *FindLib on logon           *-Net-Opt 5,n
|    |    |    | b2    ANFS uses &0Bxx-&0Cxx or &0Exx-&0Fxx *-Net-Opt 6,n
|    |    |    | b3-b5 unused
|    |    |    | b6    ANFS protected                       *-Net-Opt 8,n
|    |    |    | b7    Display version number on startup
| 32 | 20 | 00 |       unused
| 33 | 21 | 00 |       unused
| 34 | 22 | 34-43 reserved for Acorn future expansion
| 35 | 23 |
| 36 | 24 |
| 37 | 25 |
| 38 | 26 |
| 39 | 27 |
| 40 | 28 |
| 41 | 29 |
| 42 | 2A |
| 43 | 2B |
| 44 | 2C | 44-53 reserved for use by commercial third parties
| 45 | 2D |
| 46 | 2E |
| 47 | 2F |
| 48 | 30 |
| 49 | 31 |
| 50 | 32 |
| 51 | 33 |
| 52 | 34 |
| 53 | 35 |
| 54 | 36 | 54-63 reserved for use by users
| 55 | 37 |
| 56 | 38 |
| 57 | 39 |
| 58 | 3A |
| 59 | 3B |
| 60 | 3C |
| 61 | 3D |
| 62 | 3E |
| 63 | 3F |
+----+----+-------------------------------------------------------------------+
| 50 bytes of user RAM                                                        |
|-----------------------------------------------------------------------------|
| FileStore E01 & E01S                                                        |
|-----------------------------------------------------------------------------|
|         |Default value in BeebEm                                            |
|         |hex | Description                                                  |
+----+----+-------------------------------------------------------------------+
| 14 | 0E | FE |     Station ID
| 15 | 0F | 01 |     Checkbyte Station ID EOR &FF
| 16 | 10 | 00 |     Reserved
| 17 | 11 | 00 |     Reserved
| 18 | 12 | 00 |     Reserved
| 19 | 13 | 00 |     Reserved
| 20 | 14 | 00 |     Last Error Number
| 21 | 15 | 00 |     Last Error X
| 22 | 16 | 00 |     Last Error Y
| 23 | 17 | 00 |     Last Error LSB
| 24 | 18 | 00 |     Last Error MSB
| 25 | 19 | 00 |     SIN LSB
| 26 | 1A | 00 |     SIN CSB
| 27 | 1B | 00 |     SIN MSB
| 28 | 1C | 80 |     Max User
| 29 | 1D | 05 |     Max Drive (+1)
| 30 | 1E | 50 | P | Printer Server Name 6 chars (00=Disabled)
| 31 | 1F | 75 | u |
| 32 | 20 | 63 | c |
| 33 | 21 | 65 | e |
| 34 | 22 | 00 |
| 35 | 23 | 00 |
| 36 | 24 | 53 | S | Maintenance mode username 11 chars
| 37 | 25 | 59 | Y |
| 38 | 26 | 53 | S |
| 39 | 27 | 54 | T |
| 40 | 28 | 00 |
| 41 | 29 | 00 |
| 42 | 2A | 00 |
| 43 | 2B | 00 |
| 44 | 2C | 00 |
| 45 | 2D | 00 |
| 46 | 2E | 00 |
| 47 | 2F | 54 | username checkbyte. :LSB:("S"+"y"+"s"+"t"+&0D)+:MSB:("S"+"y"+"s"+"t"+&0D)
| 48 | 30 | 4E | Printer page feeds. 0 is on, anything else is off
| 49 | 31 | 00 | Hard Drive copyright string check 0=yes, 1=no
| 50 | 32 | 00 |
| 51 | 33 | 00 |
| 52 | 34 | 00 |
| 53 | 35 | 00 |
| 54 | 36 | 00 |
| 55 | 37 | 00 |
| 56 | 38 | 00 |
| 57 | 39 | 00 |
| 58 | 3A | 00 |
| 59 | 3B | 00 |
| 60 | 3C | 00 |
| 61 | 3D | 00 |
| 62 | 3E | 00 |
| 63 | 3F | 00 |
+----+----+--------------------+-------------------+


****************************************************************/

// Storage for the RTC Memory Map
// Seperate instances for each machine type that uses CMOS

unsigned char CMOSRAM_M128[64];		// 10 Bytes Clock, 4 bytes Control Registers, 50 Bytes User RAM for Master 128
unsigned char CMOSRAM_E01[64];		// 10 Bytes Clock, 4 bytes Control Registers, 50 Bytes User RAM for FileStore E01
unsigned char CMOSRAM_E01S[64];		// 10 Bytes Clock, 4 bytes Control Registers, 50 Bytes User RAM for FileStore E01S

// Backup of Master 128 CMOS Defaults from byte 14
extern unsigned char CMOSDefault[50] = { 0,0xFE,0,0xFE,0,0xc9,0xff,0xfe,0x32,0,7,0xc1,0x1e,5,0,0x59,0xa2,0,0,0 }; 

// Backup of FileStore CMOS Defaults from byte 14
extern unsigned char CMOSDefaultFS[50] = { 0xFE,0x01,
									0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF, 0xFF, 0xFF, 0x80, 0x05, 0x50, 0x75,
									0x63, 0x65, 0x00, 0x00, 0x53, 0x59, 0x53, 0x54, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x54,
									0x4E, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 }; 

// pointer used to switch between the CMOS RAM for the different machines
unsigned char* pCMOS;

/* Clock stuff for Master 128 RTC */
time_t SysTime;
bool RTCY2KAdjust = true;
int RTC_YearCorrection; // this can be specified in the preferences file

time_t RTCTimeOffset = 0;   // holds the offset from system time (01-01-1970) when the RTC was started

/* 
Timer for the update cycle of the RTC
Normally with the Master128, the clock is updated on a read
however the FileStore depends on flags and interrupt being 
correct before, during and after the update cycle
*/

//unsigned int RTC_UpdateCycle = 1000;     // every second
unsigned int RTC_UpdateCycle = 10;     // every 1000th second

/*-------------------------------------------------------------------------*/
void RTCInit(void) {

	struct tm* CurTime;
	time(&SysTime);
	CurTime = localtime(&SysTime);

	intStatus &= ~(1 << rtc);			// Clear interrupt

	switch (MachineType)				// select CMOS memory
	{
		case Model::Master128:
			pCMOS = &CMOSRAM_M128[0];   // pointer to Master 128 CMOS
			break;
		case Model::FileStoreE01:
			pCMOS = &CMOSRAM_E01[0];	// pointer to FileStore E01 CMOS
			break;
		case Model::FileStoreE01S:
			pCMOS = &CMOSRAM_E01[0];    // pointer to FileStore E01S CMOS
			break;
		default:						// machine doesn't use CMOS
			KillTimer(mainWin->m_hWnd, 3);
			pCMOS = NULL;
			if (DebugEnabled)
				DebugDisplayTrace(DebugType::RTC, true, "RTC: Timer off");
			return;
	}

	// Clear Registers A-D
	for (unsigned char reg=RegisterA; reg <=RegisterD; reg++) {
	pCMOS[reg] = 0x00; 
	}

	if ((MachineType == Model::FileStoreE01) || (MachineType == Model::FileStoreE01S)) {
		// start update cycle
		SetTimer(mainWin->m_hWnd, 3, RTC_UpdateCycle, NULL);   // Set the RTC Update cycle
		if (DebugEnabled)
			DebugDisplayTrace(DebugType::RTC, true, "RTC: Timer set");
	}


	if (!(pCMOS[RegisterB] & DM)) {
		// intialise with current time values in BCD format
		pCMOS[0] = BCD(CurTime->tm_sec);
		pCMOS[2] = BCD(CurTime->tm_min);
		pCMOS[4] = BCD(CurTime->tm_hour);
		pCMOS[6] = BCD((CurTime->tm_wday) + 1);
		pCMOS[7] = BCD(CurTime->tm_mday);
		pCMOS[8] = BCD((CurTime->tm_mon) + 1);
		pCMOS[9] = BCD((CurTime->tm_year) - (RTCY2KAdjust ? RTC_YearCorrection : 0));
	}
	else {
		// intialise with current time values in binary format
		pCMOS[0] = (unsigned char)CurTime->tm_sec;
		pCMOS[2] = (unsigned char)CurTime->tm_min;
		pCMOS[4] = (unsigned char)CurTime->tm_hour;
		pCMOS[6] = (unsigned char)(CurTime->tm_wday) + 1;
		pCMOS[7] = (unsigned char)CurTime->tm_mday;
		pCMOS[8] = (unsigned char)(CurTime->tm_mon) + 1;
		pCMOS[9] = (unsigned char)(CurTime->tm_year) - (RTCY2KAdjust ? RTC_YearCorrection : 0);
	}

	// Set the start time offset based on the initialised values above
	// The RTC Y2K adjust works by pretending the system time is 20 years previous
	// if only 20 is taken from the year when the year is queried then the day of the month
	// will be incorrect.

	RTCTimeOffset = SysTime - CMOSConvertClock();

}


/****************************************************************************/
void HandleRTCTimer()
{
	// FileStores use the RTC update cycle flags

	if ((pCMOS[RegisterB] & SET) == false) {
		pCMOS[RegisterA] += UIP;
		RTCUpdateClock();
		// check alarms - not implemented

		// update ended
		// UF - The update-ended flag (UF) bit is set after each update cycle.
		pCMOS[RegisterC] |= UF;   // IRQF PF AF +UF 0 0 0 0

		// set IRQF state
		if ((pCMOS[RegisterB] & 10) && (pCMOS[RegisterC] & 10) ||	// UIE & UF
			(pCMOS[RegisterB] & 20) && (pCMOS[RegisterC] & 20) ||	// AIE & AF 
			(pCMOS[RegisterB] & 40) && (pCMOS[RegisterC] & 40)) {	// PIE & PF
			pCMOS[RegisterC] |= 0x80;								// set IRQF
			intStatus |= (1 << rtc);								// Interrupt enabled
		}
		else {
			pCMOS[RegisterC] &= 0x7F;								// clear IRQF
			intStatus &= ~(1 << rtc);								// Clear interrupt
		}

		// reset UIP
		pCMOS[RegisterA] -= UIP;
	}

}

/*-------------------------------------------------------------------------*/
void RTCUpdateClock(void) {

	struct tm* CurTime;
	time(&SysTime);
	SysTime -= RTCTimeOffset;
	CurTime = localtime(&SysTime);


	if (!(pCMOS[RegisterB] & DM)) {
		// update with current time values in BCD format
		pCMOS[0] = BCD(CurTime->tm_sec);
		pCMOS[2] = BCD(CurTime->tm_min);
		pCMOS[4] = BCD(CurTime->tm_hour);
		pCMOS[6] = BCD((CurTime->tm_wday) + 1);
		pCMOS[7] = BCD(CurTime->tm_mday);
		pCMOS[8] = BCD((CurTime->tm_mon) + 1);
		pCMOS[9] = ((CurTime->tm_year)>=100) ? BCD(((CurTime->tm_year)-100)) : BCD(CurTime->tm_year);
	}
	else {
		// update with current time values in binary format
		pCMOS[0] = (unsigned char)CurTime->tm_sec;
		pCMOS[2] = (unsigned char)CurTime->tm_min;
		pCMOS[4] = (unsigned char)CurTime->tm_hour;
		pCMOS[6] = (unsigned char)(CurTime->tm_wday) + 1;
		pCMOS[7] = (unsigned char)CurTime->tm_mday;
		pCMOS[8] = (unsigned char)(CurTime->tm_mon) + 1;
		pCMOS[9] = ((CurTime->tm_year) >= 100) ? ((CurTime->tm_year) - 100) : CurTime->tm_year;
	}
	
}


/*-------------------------------------------------------------------------
* Write values directly to Clock, Registers A,B,C,D or the CMOS area
* -------------------------------------------------------------------------*/

void CMOSWrite(unsigned char CMOSAddr, unsigned char CMOSData) {
	// Many thanks to Tom Lees for supplying me with info on the 146818 registers 
	// for these two functions.

	if (DebugEnabled) {
		char info[200];
		sprintf(info, "RTC: Write address %02X value %02X", (int)(CMOSAddr), CMOSData & 0xff);
		DebugDisplayTrace(DebugType::RTC, true, info);
	}

	switch (CMOSAddr)
	{
		case 1:		// clock data
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
		case 7:
		case 8:
		case 9:
			if (!(pCMOS[RegisterB] & DM))	// BCD or binary format
				pCMOS[CMOSAddr] = BCD(CMOSData);
			else
				pCMOS[CMOSAddr] = CMOSData;
			break;
		case RegisterA:
			pCMOS[RegisterA] = CMOSData & 0x7f; // Top bit not writable
			break;
		case RegisterB:
			// Bit-7 SET - When the SET bit is 0, the update cycle functions normally by advancing the 
			// counts once per second. When 1, any update is aborted and the program may write the time
			// and calendar bytes without an update occuring

			if ((MachineType != Model::FileStoreE01) && (MachineType != Model::FileStoreE01S)) {
				if (CMOSData & 0x80) { // SET Bit being written
					RTCUpdateClock();
				}
				else if ((pCMOS[RegisterB] & 0x80) && !(CMOSData & 0x80)) {
					// New clock settings
					time(&SysTime);
					RTCTimeOffset = SysTime - CMOSConvertClock();
				}
			}

			pCMOS[CMOSAddr] = CMOSData;  // write the value to CMOS anyway
			break;
		case RegisterC:	// do nothing, read only
			break;
		case RegisterD: // do nothing, read only
			break;
		default:   // otherwise CMOS data
			pCMOS[CMOSAddr] = CMOSData;
	}


}


/*-------------------------------------------------------------------------*/
unsigned char CMOSRead(unsigned char CMOSAddr) {

	unsigned char tmp = 0x00;

	// 0xc Register is a special case when reading as it clears after a read
    // only appears to be used by Filestore

	if (CMOSAddr == RegisterC) { // interrupt check

	// Reading returns the status and causes IRQ Flag to be cleared
		if (pCMOS[RegisterC] == 0x90) {  // +IRQF +UF interrupt flag
			pCMOS[RegisterC] = 0x00; // clear register
			intStatus &= ~(1 << rtc); // Clear interrupt

			if (DebugEnabled)
				DebugDisplayTrace(DebugType::RTC, true, "RTC: IRQF cleared. RegC Read");

			tmp = 0x90; // return value before it was cleared
		}
		// else return 0x00 which is already set at initialisation

	}
	else {

		// 0x0 to 0x9 - Clock
		if (CMOSAddr < RegisterA)
			RTCUpdateClock();  // update the clock before returning the value

		tmp = (pCMOS[CMOSAddr]);
	}

	if (DebugEnabled) {
		char info[200];
		sprintf(info, "RTC: Read address %02X value %02X", (int)(CMOSAddr), tmp & 0xff);
		DebugDisplayTrace(DebugType::RTC, true, info);
	}

	return tmp;
}


/*-------------------------------------------------------------------------
             Useful functions
-------------------------------------------------------------------------*/

/*-------------------------------------------------------------------------*/
unsigned char BCD(int nonBCD) {
	// convert a decimal value to a BCD value
	return((unsigned char)((nonBCD / 10) * 16) + nonBCD % 10);
}

/*-------------------------------------------------------------------------*/
unsigned char BCDToBin(unsigned char BCD) {
	// convert a BCD value to decimal value
	return((BCD >> 4) * 10 + (BCD & 15));
}

/*-------------------------------------------------------------------------*/
// Read the CMOS clock

time_t CMOSConvertClock(void) {

	time_t tim;
	struct tm Base;

	if (!(pCMOS[RegisterB] & DM)) {
		Base.tm_sec = BCDToBin(pCMOS[0]);
		Base.tm_min = BCDToBin(pCMOS[2]);
		Base.tm_hour = BCDToBin(pCMOS[4]);
		Base.tm_mday = BCDToBin(pCMOS[7]);
		Base.tm_mon = BCDToBin(pCMOS[8]) - 1;
		Base.tm_year = BCDToBin(pCMOS[9]);
	}
	else {
		Base.tm_sec = pCMOS[0];
		Base.tm_min = pCMOS[2];
		Base.tm_hour = pCMOS[4];
		Base.tm_mday = pCMOS[7];
		Base.tm_mon = pCMOS[8] - 1;
		Base.tm_year = pCMOS[9];
	}

	Base.tm_wday = -1;
	Base.tm_yday = -1;
	Base.tm_isdst = -1;
	tim = mktime(&Base);
	return tim;
}


void DebugRTCState()
{

	DebugDisplayInfo("");

	if (!(pCMOS[RegisterB] & DM)) 
		DebugDisplayInfoF("RTC Clock: Day:%02d Mon:%02d Year:%02d  Time %02d:%02d:%02d (BCD values)",
			(int)(pCMOS[7]), (int)(pCMOS[8]),
			(int)(pCMOS[9]), (int)(pCMOS[4]),
			(int)(pCMOS[2]), (int)(pCMOS[0]));
	else
		DebugDisplayInfoF("RTC Clock: Day:%02d Mon:%02d Year:%02d  Time %02d:%02d:%02d",
			(int)(pCMOS[7]), (int)(pCMOS[8]),
			(int)(pCMOS[9]), (int)(pCMOS[4]),
			(int)(pCMOS[2]), (int)(pCMOS[0]));

	DebugDisplayInfoF("RTC RegA: %sUIP   %sDV2  %sDV1  %sDV0  %sRS3   %sRS2  %sRS1    %sRS0",
		(pCMOS[0x0A] & 0x80) ? "+" : "-",
		(pCMOS[0x0A] & 0x40) ? "+" : "-",
		(pCMOS[0x0A] & 0x20) ? "+" : "-",
		(pCMOS[0x0A] & 0x10) ? "+" : "-",
		(pCMOS[0x0A] & 0x8) ? "+" : "-",
		(pCMOS[0x0A] & 0x4) ? "+" : "-",
		(pCMOS[0x0A] & 0x2) ? "+" : "-",
		(pCMOS[0x0A] & 0x1) ? "+" : "-");

	DebugDisplayInfoF("RTC RegB: %sSET   %sPIE  %sAIE  %sUIE  %sSQWE  %sDM   %s24/12  %sDSE",
		(pCMOS[0x0B] & 0x80) ? "+" : "-",
		(pCMOS[0x0B] & 0x40) ? "+" : "-",
		(pCMOS[0x0B] & 0x20) ? "+" : "-",
		(pCMOS[0x0B] & 0x10) ? "+" : "-",
		(pCMOS[0x0B] & 0x8) ? "+" : "-",
		(pCMOS[0x0B] & 0x4) ? "+" : "-",
		(pCMOS[0x0B] & 0x2) ? "+" : "-",
		(pCMOS[0x0B] & 0x1) ? "+" : "-");

	DebugDisplayInfoF("RTC RegC: %sIRQF  %sPF   %sAF   %sUF",
		(pCMOS[0x0C] & 0x80) ? "+" : "-",
		(pCMOS[0x0C] & 0x40) ? "+" : "-",
		(pCMOS[0x0C] & 0x20) ? "+" : "-",
		(pCMOS[0x0C] & 0x10) ? "+" : "-");

	DebugDisplayInfo("");
 	DebugDisplayInfo( "CMOS:     00 01 02 03 04 05 06 07 08 09 0A 0B 0C 0D 0E 0F");
	DebugDisplayInfo("CMOS:     -----------------------------------------------");
	DebugDisplayInfoF("CMOS: 00:                                           %02X %02X",
		(int)pCMOS[0xE],
		(int)pCMOS[0xF]);
	DebugDisplayInfoF("CMOS: 10: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
		pCMOS[0x10], pCMOS[0x11], pCMOS[0x12], pCMOS[0x13], pCMOS[0x14], pCMOS[0x15], pCMOS[0x16], pCMOS[0x17], 
		pCMOS[0x18], pCMOS[0x19], pCMOS[0x1A], pCMOS[0x1B], pCMOS[0x1C], pCMOS[0x1D], pCMOS[0x1E], pCMOS[0x1F]);
	DebugDisplayInfoF("CMOS: 20: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
		pCMOS[0x20], pCMOS[0x21], pCMOS[0x22], pCMOS[0x23], pCMOS[0x24], pCMOS[0x25], pCMOS[0x26], pCMOS[0x27],
		pCMOS[0x28], pCMOS[0x29], pCMOS[0x2A], pCMOS[0x2B], pCMOS[0x2C], pCMOS[0x2D], pCMOS[0x2E], pCMOS[0x2F]);
	DebugDisplayInfoF("CMOS: 30: %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X %02X",
		pCMOS[0x30], pCMOS[0x31], pCMOS[0x32], pCMOS[0x33], pCMOS[0x34], pCMOS[0x35], pCMOS[0x36], pCMOS[0x37],
		pCMOS[0x38], pCMOS[0x39], pCMOS[0x3A], pCMOS[0x3B], pCMOS[0x3C], pCMOS[0x3D], pCMOS[0x3E], pCMOS[0x3F]);

}