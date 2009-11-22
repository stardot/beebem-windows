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

#ifndef SASI_HEADER
#define SASI_HEADER

void SASIReset(void);
void SASIWrite(int Address, int Value) ;
int SASIRead(int Address);
int SASIReadData(void);
void SASIWriteData(int data);
void SASIBusFree(void);
void SASIMessage(void);
void SASISelection(int data);
void SASICommand(void);
void SASIExecute(void);
void SASIStatus(void);
void SASITestUnitReady(void);
void SASIRequestSense(void);
int SASIDiscRequestSense(unsigned char *cdb, unsigned char *buf);
void SASIRead(void);
void SASIWrite(void);
int SASIReadSector(unsigned char *buf, int block);
bool SASIWriteSector(unsigned char *buf, int block);
void SASIStartStop(void);
bool SASIDiscFormat(unsigned char *buf);
void SASIFormat(void);
void SASIVerify(void);
void SASITranslate(void);
void SASIRezero(void);
void SASIRamDiagnostics(void);
void SASIControllerDiagnostics(void);
void SASISetGeometory(void);
void SASISeek(void);
bool SASIWriteGeometory(unsigned char *buf);
#endif
