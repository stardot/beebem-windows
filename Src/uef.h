/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2004  Mike Wyatt

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

#ifndef INCLUDE_UEF_H
#define INCLUDE_UEF_H

// Some defines related to the status byte - these may change!
constexpr int UEF_MMASK    = (3 << 16);
constexpr int UEF_BYTEMASK = 0xff;

// Some macros for reading parts of the status byte
constexpr int UEF_CARRIER_TONE = (0 << 16);
constexpr int UEF_DATA         = (1 << 16);
constexpr int UEF_GAP          = (2 << 16);
constexpr int UEF_EOF          = (3 << 16);

constexpr int           UEFRES_TYPE(int x) { return x & UEF_MMASK;    }
constexpr unsigned char UEFRES_BYTE(int x) { return x & UEF_BYTEMASK; }

// Some possible return states
enum class UEFResult
{
	Success,
	NotUEF,
	NotTape,
	NoFile
};

// Setup
void UEFSetClock(int Speed);
void UEFSetUnlock(bool Unlock);

// Poll mode
int UEFGetData(int Time);

// Open & close
UEFResult UEFOpen(const char *FileName);
void UEFClose();

// Writing
UEFResult UEFCreate(const char *FileName);
UEFResult UEFPutData(int Data, int Time);

#endif
