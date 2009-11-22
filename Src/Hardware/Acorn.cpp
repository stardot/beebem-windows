/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2001  Richard Gellman

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

// Acorn 1770 DFS Board Drive Controller Chip DLL
// (C) September 2001 - Richard Gellman

#include <windows.h>
#include "Acorn.h"

char *AcornName="Acorn 1770 DFS Extension board for BBC Model B";

// The dll must assume the 1770 system accepts a Master 128 type control reg thus:
// Bit 0 Drive Select 0
// Bit 1 Drive Select 1
// Bit 2 Reset controller
// Bit 3 Drive Select 2
// Bit 4 Side Select
// Bit 5 Density Select = 0 - double ; 1 - single
// Bits 6 & 7 not used

EXPORT unsigned char SetDriveControl(unsigned char value) {
	unsigned char temp;
	// from native to master 128
	temp=value & 3; // drive selects
	temp|=(value&12)<<2; // Side select and density select
	return(temp);
}

EXPORT unsigned char GetDriveControl(unsigned char value) {
	unsigned char temp;
	// from master 128 to native
	temp=value & 3; // drive selects
	temp|=(value&48)>>2; // side and density selects
	return(temp);
}

EXPORT void GetBoardProperties(struct DriveControlBlock *FDBoard) {
	FDBoard->FDCAddress=0xfe84;
	FDBoard->DCAddress=0xfe80;
	FDBoard->BoardName=AcornName;
	FDBoard->TR00_ActiveHigh=FALSE;
}
