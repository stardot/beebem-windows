/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2001  Richard Gellman
Copyright (C) 2005  Greg Cook

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

/* 1770 FDC Support for Beebem */
/* Written by Richard Gellman */

#ifndef DISC1770_HEADER
#define DISC1770_HEADER

#include "disctype.h"

extern bool DWriteable[2]; // Write Protect
extern bool Disc1770Enabled;
extern bool InvertTR00;

enum class Disc1770Result {
	OpenedReadWrite,
	OpenedReadOnly,
	Failed
};

unsigned char Read1770Register(int Register);
void Write1770Register(int Register, unsigned char Value);
Disc1770Result Load1770DiscImage(const char *DscFileName, int DscDrive, DiscType Type);
void WriteFDCControlReg(unsigned char Value);
unsigned char ReadFDCControlReg();
void Reset1770();
void Poll1770(int NCycles);
bool CreateADFSImage(const char *FileName, int Tracks);
void Close1770Disc(int Drive);
void Save1770UEF(FILE *SUEF);
void Load1770UEF(FILE *SUEF,int Version);
void Get1770DiscInfo(int DscDrive, DiscType *Type, char *pFileName);

#endif
