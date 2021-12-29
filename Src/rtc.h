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

#ifndef RTC_HEADER
#define RTC_HEADER

// RTC Registers in memory
const unsigned char RegisterA = 0x0A;
const unsigned char RegisterB = 0x0B;
const unsigned char RegisterC = 0x0C;
const unsigned char RegisterD = 0x0D;

// Register A
const unsigned char UIP = 0x80; // bit7

// Register B
const unsigned char SET = 0x80; // bit7
const unsigned char UIE = 0x10; // bit4
const unsigned char SQWE = 0x8; // bit3
const unsigned char DM = 0x4;   // bit2

// Register C
const unsigned char IRQF = 0x80; // bit7
const unsigned char UF = 0x10; // bit4

// RTC Flags
extern bool RTCY2KAdjust;
extern int RTC_YearCorrection; // this can be specified in the preferences file

extern unsigned int RTC_UpdateCycle;

extern unsigned char CMOSRAM_M128[64]; // 10 bytes Clock, 4 bytes Registers, 50 bytes CMOS RAM
extern unsigned char CMOSRAM_E01[64];  // 10 bytes Clock, 4 bytes Registers, 50 bytes CMOS RAM
extern unsigned char CMOSRAM_E01S[64]; // 10 bytes Clock, 4 bytes Registers, 50 bytes CMOS RAM

extern unsigned char CMOSDefault[50];   // Master 128 CMOS default values
extern unsigned char CMOSDefaultFS[50]; // FileStore CMOS default values

void RTCInit();
extern void HandleRTCTimer();
void RTCUpdateClock();

void CMOSWrite(unsigned char CMOSAddr, unsigned char CMOSData);
unsigned char CMOSRead(unsigned char CMOSAddr);

unsigned char BCD(int nonBCD);
unsigned char BCDToBin(unsigned char BCD);
time_t CMOSConvertClock();

void DebugRTCState();

#endif
