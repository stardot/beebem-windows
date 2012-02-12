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

/* SCSI Support for Beebem */
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
#include "scsi.h"
#include "beebmem.h"

enum phase_t {
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
	phase_t phase;
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
} scsi_t;

scsi_t scsi;
FILE *SCSIDisc[4] = {0};
int SCSISize[4];

char SCSIDriveEnabled = 0;

void SCSIReset(void)
{
FILE *f;
int i;
char buff[256];

	scsi.code = 0x00;
	scsi.sector = 0x00;
	
	for (i = 0; i < 4; ++i)
    {
//      sprintf(buff, "%s/discims/scsi%d.dat", mainWin->GetUserDataPath(), i);
        sprintf(buff, "%s/scsi%d.dat", DiscPath, i);

        if (SCSIDisc[i] != NULL)
		{
			fclose(SCSIDisc[i]);
			SCSIDisc[i]=NULL;
		}

        if (!SCSIDriveEnabled)
            continue;

        SCSIDisc[i] = fopen(buff, "rb+");
    
        if (SCSIDisc[i] == NULL)
        {
            SCSIDisc[i] = fopen(buff, "wb");
            if (SCSIDisc[i] != NULL) fclose(SCSIDisc[i]);
            SCSIDisc[i] = fopen(buff, "rb+");
        }

		SCSISize[i] = 0;
        if (SCSIDisc[i] != NULL)
		{
    
//			sprintf(buff, "%s/discims/scsi%d.dsc", mainWin->GetUserDataPath(), i);
			sprintf(buff, "%s/scsi%d.dsc", DiscPath, i);
			
			f = fopen(buff, "rb");
			
			if (f != NULL)
			{
				fread(buff, 1, 22, f);
			
				// heads = buf[15];
				// cyl   = buf[13] * 256 + buf[14];

				SCSISize[i] = buff[15] * (buff[13] * 256 + buff[14]) * 33;		// Number of sectors on disk = heads * cyls * 33
			
				fclose(f);
			}
		}
	}

	BusFree();
}

void SCSIWrite(int Address, int Value) 
{
    if (!SCSIDriveEnabled)
        return;

//	SCSILog("SCSIWrite Address = 0x%02x, Value = 0x%02x, Phase = %d, PC = 0x%04x\n", Address, Value, scsi.phase, ProgramCounter);
	
    switch (Address)
    {
		case 0x00:
			scsi.sel = true;
			WriteData(Value);
			break;
		case 0x01:
			scsi.sel = true;
			break;
		case 0x02:
			scsi.sel = false;
            WriteData(Value);
			break;
		case 0x03:
			scsi.sel = true;
            if (Value == 0xff)
            {
                scsi.irq = true;
                intStatus |= (1 << hdc);
				scsi.status = 0x00;
			}
            else
            {
                scsi.irq = true;
                intStatus &= ~(1 << hdc);
            }

			break;
    }
}

int SCSIRead(int Address)
{
    if (!SCSIDriveEnabled)
        return 0xff;

int data = 0xff;

    switch (Address)
    {
    case 0x00 :         // Data Register
        data = ReadData();
        break;
    case 0x01:			// Status Register
		data = 0x20;	// Hmmm.. don't know why req has to always be active ? If start at 0x00, ADFS lock up on entry
		if (scsi.cd) data |= 0x80;
		if (scsi.io) data |= 0x40;
		if (scsi.req) data |= 0x20;
		if (scsi.irq) data |= 0x10;
		if (scsi.bsy) data |= 0x02;
		if (scsi.msg) data |= 0x01;
        break;
    case 0x02:
        break;
    case 0x03:
        break;
    }

//	fprintf(stderr, "SCSIRead Address = 0x%02x, Value = 0x%02x, Phase = %d, PC = 0x%04x\n", Address, data, scsi.phase, ProgramCounter);
	
    return data;
}

int ReadData(void)
{
	int data;
	
	switch (scsi.phase)
	{
		case status :
			data = scsi.status;
			scsi.req = false;
			Message();
			return data;
			
		case message :
			data = scsi.message;
			scsi.req = false;
			BusFree();
			return data;
			
		case read :
			data = scsi.buffer[scsi.offset];
			scsi.offset++;
			scsi.length--;
			scsi.req = false;
			
			if (scsi.length == 0) {
				scsi.blocks--;
				if (scsi.blocks == 0) {
					Status();
					return data;
				}
				
				scsi.length = ReadSector(scsi.buffer, scsi.next);
				if (scsi.length <= 0) {
					scsi.status = (scsi.lun << 5) | 0x02;
					scsi.message = 0x00;
					Status();
					return data;
				}
				scsi.offset = 0;
				scsi.next++;
			}
			return data;
			break;
	}

	if (scsi.phase == busfree)
		return scsi.lastwrite;

	BusFree();
	return scsi.lastwrite;
}

void WriteData(int data)
{

	scsi.lastwrite = data;
	
	switch (scsi.phase)
	{
		case busfree :
			if (scsi.sel) {
				Selection(data);
			}
			return;

		case selection :
			if (!scsi.sel) {
				Command();
				return;
			}
			break;
			
		case command :
			scsi.cmd[scsi.offset] = data;
			if (scsi.offset == 0) {
				if ((data >= 0x20) && (data <= 0x3f)) {
					scsi.length = 10;
				}
			}
			scsi.offset++;
			scsi.length--;
			scsi.req = false;

			if (scsi.length == 0) {
				Execute();
				return;
			}
			return;
			
		case write :
			scsi.buffer[scsi.offset] = data;
			scsi.offset++;
			scsi.length--;
			scsi.req = false;
			
			if (scsi.length > 0)
				return;
				
			switch (scsi.cmd[0]) {
				case 0x0a :
				case 0x15 :
				case 0x2a :
				case 0x2e :
					break;
				default :
					Status();
					return;
			}

			switch (scsi.cmd[0]) {
				case 0x0a :
					if (!WriteSector(scsi.buffer, scsi.next - 1)) {
						scsi.status = (scsi.lun << 5) | 0x02;
						scsi.message = 0;
						Status();
						return;
					}
					break;
				case 0x15 :
					if (!WriteGeometory(scsi.buffer)) {
						scsi.status = (scsi.lun << 5) | 0x02;
						scsi.message = 0;
						Status();
						return;
					}
					break;
			}
				
			scsi.blocks--;
			
			if (scsi.blocks == 0) {
				Status();
				return;
			}
			scsi.length = 256;
			scsi.next++;
			scsi.offset = 0;
			return;
	}

	BusFree();
}

void BusFree(void)
{
	scsi.msg = false;
	scsi.cd = false;
	scsi.io = false;
	scsi.bsy = false;
	scsi.req = false;
	scsi.irq = false;
	
	scsi.phase = busfree;

	LEDs.HDisc[0] = 0;
	LEDs.HDisc[1] = 0;
	LEDs.HDisc[2] = 0;
	LEDs.HDisc[3] = 0;
}

void Selection(int data)
{
	scsi.bsy = true;
	scsi.phase = selection;
}


void Command(void)

{
	scsi.phase = command;
	
	scsi.io = false;
	scsi.cd = true;
	scsi.msg = false;
	
	scsi.offset = 0;
	scsi.length = 6;
}

void Execute(void)
{
	scsi.phase = execute;
	
//	if (scsi.cmd[0] <= 0x1f) {
//		fprintf(stderr, "Execute 0x%02x, Param 1=0x%02x, Param 2=0x%02x, Param 3=0x%02x, Param 4=0x%02x, Param 5=0x%02x, Phase = %d, PC = 0x%04x\n", 
//				scsi.cmd[0], scsi.cmd[1], scsi.cmd[2], scsi.cmd[3], scsi.cmd[4], scsi.cmd[5], scsi.phase, ProgramCounter);
//	} else {
//		fprintf(stderr, "Execute 0x%02x, Param 1=0x%02x, Param 2=0x%02x, Param 3=0x%02x, Param 4=0x%02x, Param 5=0x%02x, Param 6=0x%02x, Param 7=0x%02x, Param 8=0x%02x, Param 9=0x%02x, Phase = %d, PC = 0x%04x\n", 
//				scsi.cmd[0], scsi.cmd[1], scsi.cmd[2], scsi.cmd[3], scsi.cmd[4], scsi.cmd[5], scsi.cmd[6], scsi.cmd[7], scsi.cmd[8], scsi.cmd[9], scsi.phase, ProgramCounter);
//	}
	
	scsi.lun = (scsi.cmd[1]) >> 5;

	LEDs.HDisc[scsi.lun] = 1;

	switch (scsi.cmd[0]) {
		case 0x00 :
			TestUnitReady();
			return;
		case 0x03 :
			RequestSense();
			return;
		case 0x04 :
			Format();
			return;
		case 0x08 :
			Read6();
			return;
		case 0x0a :
			Write6();
			return;
		case 0x0f :
			Translate();
			return;
		case 0x15 :
			ModeSelect();
			return;
		case 0x1a :
			ModeSense();
			return;
		case 0x1b :
			StartStop();
			return;
		case 0x2f :
			Verify();
			return;
	}
	
	scsi.status = (scsi.lun << 5) | 0x02;
	scsi.message = 0x00;
	Status();
}

void Status(void)
{
	scsi.phase = status;
	
	scsi.io = true;
	scsi.cd = true;
	scsi.req = true;
}

void Message(void)
{
	scsi.phase = message;
	
	scsi.msg = true;
	scsi.req = true;
}

bool DiscTestUnitReady(unsigned char *buf)

{
	if (SCSIDisc[scsi.lun] == NULL) return false;
	return true;
}

void TestUnitReady(void)
{
	bool status;
	
	status = DiscTestUnitReady(scsi.cmd);
	if (status) {
		scsi.status = (scsi.lun << 5) | 0x00;
		scsi.message = 0x00;
	} else {
		scsi.status = (scsi.lun << 5) | 0x02;
		scsi.message = 0x00;
	}
	Status();
}

bool DiscStartStop(unsigned char *buf)

{
	if (buf[4] & 0x02) {

// Eject Disc
		
	}
	return true;
}

void StartStop(void)
{
	bool status;
	
	status = DiscStartStop(scsi.cmd);
	if (status) {
		scsi.status = (scsi.lun << 5) | 0x00;
		scsi.message = 0x00;
	} else {
		scsi.status = (scsi.lun << 5) | 0x02;
		scsi.message = 0x00;
	}
	Status();
}

void RequestSense(void)
{
	scsi.length = DiscRequestSense(scsi.cmd, scsi.buffer);
	
	if (scsi.length > 0) {
		scsi.offset = 0;
		scsi.blocks = 1;
		scsi.phase = read;
		scsi.io = TRUE;
		scsi.cd = FALSE;
		
		scsi.status = (scsi.lun << 5) | 0x00;
		scsi.message = 0x00;
		
		scsi.req = true;
	}
	else
	{
		scsi.status = (scsi.lun << 5) | 0x02;
		scsi.message = 0x00;
		Status();
	}
}

int DiscRequestSense(unsigned char *cdb, unsigned char *buf)
{
	int size;
	
	size = cdb[4];
	if (size == 0)
		size = 4;
	
	switch (scsi.code) {
		case 0x00 :
			buf[0] = 0x00;
			buf[1] = 0x00;
			buf[2] = 0x00;
			buf[3] = 0x00;
			break;
		case 0x21 :
			buf[0] = 0x21;
			buf[1] = (scsi.sector >> 16) & 0xff;
			buf[2] = (scsi.sector >> 8) & 0xff;
			buf[3] = (scsi.sector & 0xff);
			break;
	}
	
	scsi.code = 0x00;
	scsi.sector = 0x00;
	
	return size;
}

void Read6(void)
{
	int record;
	
	record = scsi.cmd[1] & 0x1f;
	record <<= 8;
	record |= scsi.cmd[2];
	record <<= 8;
	record |= scsi.cmd[3];
	scsi.blocks = scsi.cmd[4];
	if (scsi.blocks == 0)
		scsi.blocks = 0x100;
	scsi.length = ReadSector(scsi.buffer, record);
	
	if (scsi.length <= 0) {
		scsi.status = (scsi.lun << 5) | 0x02;
		scsi.message = 0x00;
		Status();
		return;
	}
	
	scsi.status = (scsi.lun << 5) | 0x00;
	scsi.message = 0x00;
	
	scsi.offset = 0;
	scsi.next = record + 1;
	
	scsi.phase = read;
	scsi.io = true;
	scsi.cd = false;
	
	scsi.req = true;
}

int ReadSector(unsigned char *buf, int block)

{
	if (SCSIDisc[scsi.lun] == NULL) return 0;
	
    fseek(SCSIDisc[scsi.lun], block * 256, SEEK_SET);
	
	fread(buf, 256, 1, SCSIDisc[scsi.lun]);
    
	return 256;
}

bool WriteSector(unsigned char *buf, int block)

{
	if (SCSIDisc[scsi.lun] == NULL) return false;
	
    fseek(SCSIDisc[scsi.lun], block * 256, SEEK_SET);
	
	fwrite(buf, 256, 1, SCSIDisc[scsi.lun]);
    
	return true;
}

void Write6(void)
{
	int record;
	
	record = scsi.cmd[1] & 0x1f;
	record <<= 8;
	record |= scsi.cmd[2];
	record <<= 8;
	record |= scsi.cmd[3];
	scsi.blocks = scsi.cmd[4];
	if (scsi.blocks == 0)
		scsi.blocks = 0x100;

	scsi.length = 256;
	
	scsi.status = (scsi.lun << 5) | 0x00;
	scsi.message = 0x00;
	
	scsi.next = record + 1;
	scsi.offset = 0;
	
	scsi.phase = write;
	scsi.cd = false;
	
	scsi.req = true;
}

void ModeSense(void)

{
	scsi.length = DiscModeSense(scsi.cmd, scsi.buffer);
	
	if (scsi.length > 0) {
		scsi.offset = 0;
		scsi.blocks = 1;
		scsi.phase = read;
		scsi.io = TRUE;
		scsi.cd = FALSE;
		
		scsi.status = (scsi.lun << 5) | 0x00;
		scsi.message = 0x00;
		
		scsi.req = true;
	}
	else
	{
		scsi.status = (scsi.lun << 5) | 0x02;
		scsi.message = 0x00;
		Status();
	}
}

int DiscModeSense(unsigned char *cdb, unsigned char *buf)
{
	FILE *f;
	
	int size;
	
	char buff[256];
		
	if (SCSIDisc[scsi.lun] == NULL) return 0;

//	sprintf(buff, "%s/discims/scsi%d.dsc", mainWin->GetUserDataPath(), scsi.lun);
	sprintf(buff, "%s/scsi%d.dsc", DiscPath, scsi.lun);
			
	f = fopen(buff, "rb");
			
	if (f == NULL) return 0;

	size = cdb[4];
	if (size == 0)
		size = 22;

	size = (int)fread(buf, 1, size, f);
	
// heads = buf[15];
// cyl   = buf[13] * 256 + buf[14];
// step  = buf[21];
// rwcc  = buf[16] * 256 + buf[17];
// lz    = buf[20];
	
	fclose(f);

	return size;
}

void ModeSelect(void)
{

	scsi.length = scsi.cmd[4];
	scsi.blocks = 1;
	
	scsi.status = (scsi.lun << 5) | 0x00;
	scsi.message = 0x00;
	
	scsi.next = 0;
	scsi.offset = 0;
	
	scsi.phase = write;
	scsi.cd = false;
	
	scsi.req = true;
}

bool WriteGeometory(unsigned char *buf)
{
	FILE *f;
	
	char buff[256];
	
	if (SCSIDisc[scsi.lun] == NULL) return false;
	
//	sprintf(buff, "%s/discims/scsi%d.dsc", mainWin->GetUserDataPath(), scsi.lun);
	sprintf(buff, "%s/scsi%d.dsc", DiscPath, scsi.lun);
	
	f = fopen(buff, "wb");
	
	if (f == NULL) return false;
	
	fwrite(buf, 22, 1, f);
	
	fclose(f);
	
	return true;
}


bool DiscFormat(unsigned char *buf)

{
// Ignore defect list

	FILE *f;
	char buff[256];
	
	if (SCSIDisc[scsi.lun] != NULL) {
		fclose(SCSIDisc[scsi.lun]);
		SCSIDisc[scsi.lun]=NULL;
	}
	
//	sprintf(buff, "%s/discims/scsi%d.dat", mainWin->GetUserDataPath(), scsi.lun);
	sprintf(buff, "%s/scsi%d.dat", DiscPath, scsi.lun);
	
	SCSIDisc[scsi.lun] = fopen(buff, "wb");
	if (SCSIDisc[scsi.lun] != NULL) fclose(SCSIDisc[scsi.lun]);
	SCSIDisc[scsi.lun] = fopen(buff, "rb+");
	
	if (SCSIDisc[scsi.lun] == NULL) return false;

//	sprintf(buff, "%s/discims/scsi%d.dsc", mainWin->GetUserDataPath(), scsi.lun);
	sprintf(buff, "%s/scsi%d.dsc", DiscPath, scsi.lun);
	
	f = fopen(buff, "rb");
	
	if (f != NULL)
	{
		fread(buff, 1, 22, f);
		
		// heads = buf[15];
		// cyl   = buf[13] * 256 + buf[14];
		
		SCSISize[scsi.lun] = buff[15] * (buff[13] * 256 + buff[14]) * 33;		// Number of sectors on disk = heads * cyls * 33
		
		fclose(f);
	
	}
	
	return true;
}

void Format(void)
{
	bool status;
	
	status = DiscFormat(scsi.cmd);
	if (status) {
		scsi.status = (scsi.lun << 5) | 0x00;
		scsi.message = 0x00;
	} else {
		scsi.status = (scsi.lun << 5) | 0x02;
		scsi.message = 0x00;
	}
	Status();
}

bool DiscVerify(unsigned char *buf)

{
	int sector;
	
	sector = scsi.cmd[1] & 0x1f;
	sector <<= 8;
	sector |= scsi.cmd[2];
	sector <<= 8;
	sector |= scsi.cmd[3];
	
	if (sector >= SCSISize[scsi.lun])
	{
		scsi.code = 0x21;
		scsi.sector = sector;
		return false;
	}

	return true;
}

void Verify(void)
{
	bool status;
	
	status = DiscVerify(scsi.cmd);
	if (status) {
		scsi.status = (scsi.lun << 5) | 0x00;
		scsi.message = 0x00;
	} else {
		scsi.status = (scsi.lun << 5) | 0x02;
		scsi.message = 0x00;
	}
	Status();
}

void Translate(void)
{
	int record;
	
	record = scsi.cmd[1] & 0x1f;
	record <<= 8;
	record |= scsi.cmd[2];
	record <<= 8;
	record |= scsi.cmd[3];

	scsi.buffer[0] = scsi.cmd[3];
	scsi.buffer[1] = scsi.cmd[2];
	scsi.buffer[2] = scsi.cmd[1] & 0x1f;
	scsi.buffer[3] = 0x00;
		
	scsi.length = 4;
	
	scsi.offset = 0;
	scsi.blocks = 1;
	scsi.phase = read;
	scsi.io = TRUE;
	scsi.cd = FALSE;
	
	scsi.status = (scsi.lun << 5) | 0x00;
	scsi.message = 0x00;
		
	scsi.req = true;
}
