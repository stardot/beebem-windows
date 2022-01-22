/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2001  Richard Gellman
Copyright (C) 2005  Greg Cook
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

/* 2793 FDC Support for Beebem */
/* based on 1770 Support for BeebEm by Richard Gellman */

#ifndef DISC2793_HEADER
#define DISC2793_HEADER

#include "disctype.h"

// extern bool DWriteable[2]; // Write Protect
 extern bool Disc2793Enabled;
// extern bool InvertTR00;

enum class Disc2793Result {
	OpenedReadWrite,
	OpenedReadOnly,
	Failed
};

unsigned char Read2793Register(unsigned char Register);
void Write2793Register(unsigned char Register, unsigned char Value);
Disc2793Result Load2793DiscImage(const char *DscFileName, int DscDrive, DiscType Type);
void WriteFDC2793ControlReg(unsigned char Value);
unsigned char ReadFDC2793ControlReg();
void Reset2793();
void Poll2793(int NCycles);
bool CreateADFSImage(const char *FileName, int Tracks);
void Close2793Disc(int Drive);
void Get2793DiscInfo(int DscDrive, DiscType *Type, char *pFileName);

#endif
