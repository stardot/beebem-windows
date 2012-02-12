/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2006  Jon Welch

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
/* SASI Support for Beebem */
/* Written by Jon Welch */

#ifndef SCSI_HEADER
#define SCSI_HEADER

extern char SCSIDriveEnabled;

void SCSIReset(void);
void SCSIWrite(int Address, int Value) ;
int SCSIRead(int Address);
int ReadData(void);
void WriteData(int data);
void BusFree(void);
void Message(void);
void Selection(int data);
void Command(void);
void Execute(void);
void Status(void);
void TestUnitReady(void);
void RequestSense(void);
int DiscRequestSense(unsigned char *cdb, unsigned char *buf);
void Read6(void);
void Write6(void);
int ReadSector(unsigned char *buf, int block);
bool WriteSector(unsigned char *buf, int block);
void StartStop(void);
void ModeSense(void);
int DiscModeSense(unsigned char *cdb, unsigned char *buf);
void ModeSelect(void);
bool WriteGeometory(unsigned char *buf);
bool DiscFormat(unsigned char *buf);
void Format(void);
bool DiscVerify(unsigned char *buf);
void Verify(void);
void Translate(void);
#endif
