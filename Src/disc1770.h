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

/* !770 FDC Support for Beebem */
/* Written by Richard Gellman */

#ifndef DISC1770_HEADER
#define DISC1770_HEADER
extern unsigned char DWriteable[2]; // Write Protect
extern unsigned char Disc1770Enabled;
unsigned char Read1770Register(unsigned char Register1770);
void Write1770Register(unsigned char Register1770, unsigned char Value);
void Load1770DiscImage(char *DscFileName,int DscDrive,unsigned char DscType,HMENU dmenu);
void WriteFDCControlReg(unsigned char Value);
unsigned char ReadFDCControlReg(void);
void Reset1770(void);
void Poll1770(int NCycles);
void CreateADFSImage(char *ImageName,unsigned char Drive,unsigned char Tracks, HMENU dmenu);
void Close1770Disc(char Drive);
void Save1770UEF(FILE *SUEF);
void Load1770UEF(FILE *SUEF,int Version);
void Get1770DiscInfo(int DscDrive, int *Type, char *pFileName);
extern bool InvertTR00;
#endif
