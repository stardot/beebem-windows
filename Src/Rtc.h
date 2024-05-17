/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2001  Richard Gellman
Copyright (C) 2024  Mark Usher
Copyright (C) 2024  Chris Needham

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

#include "Model.h"

// Master Series MC146818AP Real-Time Clock and RAM
//
// System VIA PB7         -------------> 146818 AS  (falling edge (high->low) to latch written address)
// System VIA PB6         -> inverter -> 146818 CE' (set PB6 high to assert CE' via inverter)
// IC32 Latch Pin 5 (SC1) -------------> 146818 RW' (set high to read, low to write)
// IC32 Latch Pin 6 (SC2) -------------> 146818 DS  (read: set high for RTC to drive bus,
//                                                   write: trailing edge (high->low) to latch written data)

void RTCInit();

void RTCChipEnable(bool Enable);
bool RTCIsChipEnable();

void RTCWriteAddress(unsigned char Address);
void RTCWriteData(unsigned char CMOSData);

unsigned char RTCReadData();

void DebugRTCState();

void RTCSetCMOSDefaults(Model model);
unsigned char RTCGetData(Model model, unsigned char Address);
const unsigned char* RTCGetCMOSData(Model model);
void RTCSetCMOSData(Model model, const unsigned char* pData, int Size);

#endif
