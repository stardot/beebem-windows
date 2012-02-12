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
/* Based on code written by Y. Tanaka */
/* 26/12/2011 JGH: Disk images at DiscsPath, not AppPath */

/*

Offset  Description                 Access  
+00     data						R/W  
+01     read status                 R  
+02     write select                W  
+03     write irq enable            W  


*/

#include <stdio.h>
#include <stdlib.h>
#include "6502core.h"
#include "main.h"
#include "sasi.h"
#include "beebmem.h"

enum phase_s {
	busfree,
	selection,
	command,
	execute,
	read,
	write,
	status,
	message
};

typedef struct {
	phase_s phase;
	bool sel;
	bool msg;
	bool cd;
	bool io;
	bool bsy;
	bool req;
	bool irq;
	unsigned char cmd[10];
	int status;
	int message;
	unsigned char buffer[0x800];
	int blocks;
	int next;
	int offset;
	int length;
	int lastwrite;
	int lun;
	int code;
	int sector;
} sasi_t;

sasi_t sasi;
FILE *SASIDisc[4] = {0};

extern char SCSIDriveEnabled;

void SASIReset(void)
{
int i;
char buff[256];

	sasi.code = 0x00;
	sasi.sector = 0x00;
	
	for (i = 0; i < 1; ++i)		// only one drive allowed under Torch Z80 ?
    {
//      sprintf(buff, "%s/discims/sasi%d.dat", mainWin->GetUserDataPath(), i);
        sprintf(buff, "%s/sasi%d.dat", DiscPath, i);

        if (SASIDisc[i] != NULL)
		{
			fclose(SASIDisc[i]);
			SASIDisc[i]=NULL;
		}

        if (!SCSIDriveEnabled)
            continue;

        SASIDisc[i] = fopen(buff, "rb+");
    
        if (SASIDisc[i] == NULL)
        {
            SASIDisc[i] = fopen(buff, "wb");
            if (SASIDisc[i] != NULL) fclose(SASIDisc[i]);
            SASIDisc[i] = fopen(buff, "rb+");
        }
	}

	SASIBusFree();
}

void SASIWrite(int Address, int Value) 
{
    if (!SCSIDriveEnabled)
        return;

//	fprintf(stderr, "SASIWrite Address = 0x%02x, Value = 0x%02x, Phase = %d, PC = 0x%04x\n", Address, Value, sasi.phase, ProgramCounter);
	
    switch (Address)
    {
		case 0x00:
			sasi.sel = true;
			SASIWriteData(Value);
			break;
		case 0x01:
			sasi.sel = true;
			break;
		case 0x02:
			sasi.sel = false;
			SASIWriteData(Value);
			break;
		case 0x03:
			sasi.sel = true;
			sasi.irq = true;
			break;
    }
}

int SASIRead(int Address)
{
    if (!SCSIDriveEnabled)
        return 0xff;

int data = 0xff;

    switch (Address)
    {
    case 0x00 :         // Data Register
        data = SASIReadData();
        break;
    case 0x01:			// Status Register
	case 0x02:
		data = 0x01;

		if (sasi.sel == false) data |= 0x80;
		if (sasi.req = false) data |= 0x40;
		if (sasi.cd == true) data |= 0x20;
		if (sasi.io == true) data |= 0x10;
		if (sasi.irq) data |= 0x08;
		if (sasi.msg == false) data |= 0x04;
		if (sasi.bsy == false) data |= 0x02;
			
		break;
    case 0x03:
        break;
    }

//	fprintf(stderr, "SASIRead Address = 0x%02x, Value = 0x%02x, Phase = %d, PC = 0x%04x\n", Address, data, sasi.phase, ProgramCounter);
	
    return data;
}

int SASIReadData(void)
{
	int data;
	
	switch (sasi.phase)
	{
		case status :
			data = sasi.status;
			sasi.req = false;
			SASIMessage();
			return data;
			
		case message :
			data = sasi.message;
			sasi.req = false;
			SASIBusFree();
			return data;
			
		case read :
			data = sasi.buffer[sasi.offset];
			sasi.offset++;
			sasi.length--;
			sasi.req = false;
			
			if (sasi.length == 0) {
				sasi.blocks--;
				if (sasi.blocks == 0) {
					SASIStatus();
					return data;
				}
				
				sasi.length = SASIReadSector(sasi.buffer, sasi.next);
				if (sasi.length <= 0) {
					sasi.status = (sasi.lun << 5) | 0x02;
					sasi.message = 0x00;
					SASIStatus();
					return data;
				}
				sasi.offset = 0;
				sasi.next++;
			}
			return data;
			break;
	}

	if (sasi.phase == busfree)
		return sasi.lastwrite;

	SASIBusFree();
	return sasi.lastwrite;
}

void SASIWriteData(int data)
{

	sasi.lastwrite = data;
	
	switch (sasi.phase)
	{
		case busfree :
			if (sasi.sel) {
				SASISelection(data);
			}
			return;

		case selection :
			if (!sasi.sel) {
				SASICommand();
				return;
			}
			break;
			
		case command :
			sasi.cmd[sasi.offset] = data;
			sasi.offset++;
			sasi.length--;
			sasi.req = false;

			if (sasi.length == 0) {
				SASIExecute();
				return;
			}
			return;
			
		case write :

			sasi.buffer[sasi.offset] = data;
			sasi.offset++;
			sasi.length--;
			sasi.req = false;
			
			if (sasi.length > 0)
				return;
				
			switch (sasi.cmd[0]) {
				case 0x0a :
				case 0x0c :
					break;
				default :
					SASIStatus();
					return;
			}

			switch (sasi.cmd[0]) {
				case 0x0a :
					if (!SASIWriteSector(sasi.buffer, sasi.next - 1)) {
						sasi.status = (sasi.lun << 5) | 0x02;
						sasi.message = 0;
						SASIStatus();
						return;
					}
					break;
				case 0x0c :
					if (!SASIWriteGeometory(sasi.buffer)) {
						sasi.status = (sasi.lun << 5) | 0x02;
						sasi.message = 0;
						SASIStatus();
						return;
					}
					break;
			}
				
			sasi.blocks--;
			
			if (sasi.blocks == 0) {
				SASIStatus();
				return;
			}
			sasi.length = 256;
			sasi.next++;
			sasi.offset = 0;
			return;
	}

	SASIBusFree();
}

void SASIBusFree(void)
{
	sasi.msg = false;
	sasi.cd = false;
	sasi.io = false;
	sasi.bsy = false;
	sasi.req = false;
	sasi.irq = false;
	
	sasi.phase = busfree;

	LEDs.HDisc[0] = 0;
	LEDs.HDisc[1] = 0;
	LEDs.HDisc[2] = 0;
	LEDs.HDisc[3] = 0;

}

void SASISelection(int data)
{
	sasi.bsy = true;
	sasi.phase = selection;
}


void SASICommand(void)

{
	sasi.phase = command;
	
	sasi.io = false;
	sasi.cd = true;
	sasi.msg = false;
	
	sasi.offset = 0;
	sasi.length = 6;
}

void SASIExecute(void)
{
	sasi.phase = execute;
	
//	fprintf(stderr, "Execute 0x%02x, Param 1=0x%02x, Param 2=0x%02x, Param 3=0x%02x, Param 4=0x%02x, Param 5=0x%02x, Phase = %d, PC = 0x%04x\n", 
//			sasi.cmd[0], sasi.cmd[1], sasi.cmd[2], sasi.cmd[3], sasi.cmd[4], sasi.cmd[5], sasi.phase, ProgramCounter);
	
	sasi.lun = (sasi.cmd[1]) >> 5;

	LEDs.HDisc[sasi.lun] = 1;

	switch (sasi.cmd[0]) {
		case 0x00 :
			SASITestUnitReady();
			return;
		case 0x01 :
			SASIRezero();
			return;
		case 0x03 :
			SASIRequestSense();
			return;
		case 0x04 :
			SASIFormat();
			return;
		case 0x08 :
			SASIRead();
			return;
		case 0x09 :
			SASIVerify();
			return;
		case 0x0a :
			SASIWrite();
			return;
		case 0x0b :
			SASISeek();
			return;
		case 0x0c :
			SASISetGeometory();
			return;
		case 0xe0 :
			SASIRamDiagnostics();
			return;
		case 0xe4 :
			SASIControllerDiagnostics();
			return;
	}
	
	fprintf(stderr, "Unknown Command 0x%02x, Param 1=0x%02x, Param 2=0x%02x, Param 3=0x%02x, Param 4=0x%02x, Param 5=0x%02x, Phase = %d, PC = 0x%04x\n", 
			sasi.cmd[0], sasi.cmd[1], sasi.cmd[2], sasi.cmd[3], sasi.cmd[4], sasi.cmd[5], sasi.phase, ProgramCounter);

	sasi.status = (sasi.lun << 5) | 0x02;
	sasi.message = 0x00;
	SASIStatus();
}

void SASIStatus(void)
{
	sasi.phase = status;
	
	sasi.io = true;
	sasi.cd = true;
	sasi.msg = false;
	sasi.req = true;
}

void SASIMessage(void)
{
	sasi.phase = message;
	
	sasi.io = true;
	sasi.cd = true;
	sasi.msg = true;
	sasi.req = true;
}

bool SASIDiscTestUnitReady(unsigned char *buf)

{
	if (SASIDisc[sasi.lun] == NULL) return false;
	return true;
}

void SASITestUnitReady(void)
{
	bool status;
	
	status = SASIDiscTestUnitReady(sasi.cmd);
	if (status) {
		sasi.status = 0x00;
		sasi.message = 0x00;
	} else {
		sasi.status = (sasi.lun << 5) | 0x02;
		sasi.message = 0x00;
	}
	SASIStatus();
}

void SASIRequestSense(void)
{
	sasi.length = SASIDiscRequestSense(sasi.cmd, sasi.buffer);
	
	if (sasi.length > 0) {
		sasi.offset = 0;
		sasi.blocks = 1;
		sasi.phase = read;
		sasi.io = true;
		sasi.cd = false;
		sasi.msg = false;
		
		sasi.status = 0x00;
		sasi.message = 0x00;
		
		sasi.req = true;
	}
	else
	{
		sasi.status = (sasi.lun << 5) | 0x02;
		sasi.message = 0x00;
		SASIStatus();
	}
}

int SASIDiscRequestSense(unsigned char *cdb, unsigned char *buf)
{
	int size;
	
	size = cdb[4];
	if (size == 0)
		size = 4;
	
	switch (sasi.code) {
		case 0x00 :
			buf[0] = 0x00;
			buf[1] = 0x00;
			buf[2] = 0x00;
			buf[3] = 0x00;
			break;
		case 0x80 :
			buf[0] = 0x80;
			buf[1] = ((sasi.sector >> 16) & 0xff) | (sasi.lun << 5);
			buf[2] = (sasi.sector >> 8) & 0xff;
			buf[3] = (sasi.sector & 0xff);
			break;
	}
	
	fprintf(stderr, "Returning Sense 0x%02x, 0x%02x, 0x%02x, 0x%02x\n", buf[0], buf[1], buf[2], buf[3]);

	sasi.code = 0x00;
	sasi.sector = 0x00;
	
	return size;
}

void SASIRead(void)
{
	int record;
	
	record = sasi.cmd[1] & 0x1f;
	record <<= 8;
	record |= sasi.cmd[2];
	record <<= 8;
	record |= sasi.cmd[3];
	sasi.blocks = sasi.cmd[4];
	if (sasi.blocks == 0)
		sasi.blocks = 0x100;

//	fprintf(stderr, "ReadSector 0x%08x, Blocks %d\n", record, sasi.blocks);

	sasi.length = SASIReadSector(sasi.buffer, record);
	
	if (sasi.length <= 0) {
		sasi.status = (sasi.lun << 5) | 0x02;
		sasi.message = 0x00;
		SASIStatus();
		return;
	}
	
	sasi.status = 0x00;
	sasi.message = 0x00;
	
	sasi.offset = 0;
	sasi.next = record + 1;
	
	sasi.phase = read;
	sasi.io = true;
	sasi.cd = false;
	sasi.msg = false;
	
	sasi.req = true;
}

int SASIReadSector(unsigned char *buf, int block)

{

	memset(buf, 0x55, 256);
	
//	fprintf(stderr, "Reading Sector 0x%08x\n", block);
	
	if (SASIDisc[sasi.lun] == NULL) return 0;
	
    fseek(SASIDisc[sasi.lun], block * 256, SEEK_SET);
	
	fread(buf, 256, 1, SASIDisc[sasi.lun]);
    
	return 256;
}

bool SASIWriteSector(unsigned char *buf, int block)

{
	if (SASIDisc[sasi.lun] == NULL) return false;
	
    fseek(SASIDisc[sasi.lun], block * 256, SEEK_SET);
	
	fwrite(buf, 256, 1, SASIDisc[sasi.lun]);
    
	return true;
}

void SASIWrite(void)
{
	int record;
	
	record = sasi.cmd[1] & 0x1f;
	record <<= 8;
	record |= sasi.cmd[2];
	record <<= 8;
	record |= sasi.cmd[3];
	sasi.blocks = sasi.cmd[4];
	if (sasi.blocks == 0)
		sasi.blocks = 0x100;

	sasi.length = 256;
	
	sasi.status = 0x00;
	sasi.message = 0x00;
	
	sasi.next = record + 1;
	sasi.offset = 0;
	
	sasi.phase = write;
	sasi.io = false;
	sasi.cd = false;
	sasi.msg = false;
	
	sasi.req = true;
}

void SASISetGeometory(void)
{

	sasi.length = 8;
	sasi.blocks = 1;
	
	sasi.status = 0x00;
	sasi.message = 0x00;
	
	sasi.next = 0;
	sasi.offset = 0;
	
	sasi.phase = write;
	sasi.io = false;
	sasi.cd = false;
	sasi.msg = false;
	
	sasi.req = true;

}

bool SASIWriteGeometory(unsigned char *buf)
{
	
//	fprintf(stderr, "Write Geometory 0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x\n", 
//			buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8]);
	
	if (SASIDisc[sasi.lun] == NULL) return false;
	
	return true;
}

bool SASIDiscFormat(unsigned char *buf)

{
	char buff[256];
	int record;
	
	if (SASIDisc[sasi.lun] != NULL) {
		fclose(SASIDisc[sasi.lun]);
		SASIDisc[sasi.lun]=NULL;
	}
	
	record = buf[1] & 0x1f;
	record <<= 8;
	record |= buf[2];
	record <<= 8;
	record |= buf[3];
	
//	sprintf(buff, "%s/discims/sasi%d.dat", mainWin->GetUserDataPath(), sasi.lun);
	sprintf(buff, "%s/sasi%d.dat", DiscPath, sasi.lun);
	
	SASIDisc[sasi.lun] = fopen(buff, "wb");
	if (SASIDisc[sasi.lun] != NULL) fclose(SASIDisc[sasi.lun]);
	SASIDisc[sasi.lun] = fopen(buff, "rb+");
	
	if (SASIDisc[sasi.lun] == NULL) return false;

	return true;
}

void SASIFormat(void)
{
	bool status;
	
	status = SASIDiscFormat(sasi.cmd);
	
	if (status) {
		sasi.status = 0x00;
		sasi.message = 0x00;
	} else {
		sasi.status = (sasi.lun << 5) | 0x02;
		sasi.message = 0x00;
	}

	SASIStatus();
}

bool SASIDiscRezero(unsigned char *buf)

{
	return true;
}

void SASIRezero(void)
{
	bool status;
	
	status = SASIDiscRezero(sasi.cmd);
	if (status) {
		sasi.status = 0x00;
		sasi.message = 0x00;
	} else {
		sasi.status = (sasi.lun << 5) | 0x02;
		sasi.message = 0x00;
	}
	SASIStatus();
}

void SASIVerify(void)
{
	bool status;
	int sector, blocks;
	
	sector = sasi.cmd[1] & 0x1f;
	sector <<= 8;
	sector |= sasi.cmd[2];
	sector <<= 8;
	sector |= sasi.cmd[3];
	blocks = sasi.cmd[4];

//	fprintf(stderr, "Verifying sector %d, blocks %d\n", sector, blocks);

	status = true;
	
	if (status) {
		sasi.status = 0x00;
		sasi.message = 0x00;
	} else {
		sasi.status = (sasi.lun << 5) | 0x02;
		sasi.message = 0x00;
	}

	SASIStatus();
}

void SASIRamDiagnostics(void)
{
	sasi.status = 0x00;
	sasi.message = 0x00;
	SASIStatus();
}


void SASIControllerDiagnostics(void)
{
	sasi.status = 0x00;
	sasi.message = 0x00;
	SASIStatus();
}

void SASISeek(void)

{
	int sector;
	
	sector = sasi.cmd[1] & 0x1f;
	sector <<= 8;
	sector |= sasi.cmd[2];
	sector <<= 8;
	sector |= sasi.cmd[3];
	
	fprintf(stderr, "Seeking Sector 0x%08x\n", sector);
	
	sasi.status = 0x00;
	sasi.message = 0x00;

	SASIStatus();
}
