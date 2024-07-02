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
+00     data                        R/W
+01     read status                 R
+02     write select                W
+03     write irq enable            W
*/

#include <stdio.h>
#include <stdlib.h>

#include "Sasi.h"
#include "6502core.h"
#include "BeebMem.h"
#include "FileUtils.h"
#include "Log.h"
#include "Main.h"

static unsigned char SASIReadData();
static void SASIWriteData(unsigned char data);
static void SASIBusFree();
static void SASISelection(int data);
static void SASICommand();
static void SASIExecute();
static void SASIStatus();
static void SASIMessage();
static bool SASIDiscTestUnitReady(unsigned char *buf);
static void SASITestUnitReady();
static void SASIRequestSense();
static int SASIDiscRequestSense(unsigned char *cdb, unsigned char *buf);
static void SASIRead();
static int SASIReadSector(unsigned char *buf, int block);
static bool SASIWriteSector(unsigned char *buf, int block);
static void SASIWrite();
static void SASISetGeometory();
static bool SASIWriteGeometory(unsigned char *buf);
static bool SASIDiscFormat(unsigned char *buf);
static void SASIFormat();
static bool SASIDiscRezero(unsigned char *buf);
static void SASIRezero();
static void SASIVerify();
static void SASIRamDiagnostics();
static void SASIControllerDiagnostics();
static void SASISeek();

enum class Phase
{
	BusFree,
	Selection,
	Command,
	Execute,
	Read,
	Write,
	Status,
	Message
};

struct sasi_t {
	Phase phase;
	bool sel;
	bool msg;
	bool cd;
	bool io;
	bool bsy;
	bool req;
	bool irq;
	unsigned char cmd[10];
	int status;
	unsigned char message;
	unsigned char buffer[0x800];
	int blocks;
	int next;
	int offset;
	int length;
	unsigned char lastwrite;
	int lun;
	int code;
	int sector;
};

static sasi_t sasi;
static FILE *SASIDisc[4] = {0};

extern bool SCSIDriveEnabled;

void SASIReset()
{
	sasi.code = 0x00;
	sasi.sector = 0x00;

	for (int i = 0; i < 1; ++i) // only one drive allowed under Torch Z80 ?
	{
		if (SASIDisc[i] != nullptr)
		{
			fclose(SASIDisc[i]);
			SASIDisc[i] = nullptr;
		}

		if (!SCSIDriveEnabled)
			continue;

		char FileName[MAX_PATH];
		MakeFileName(FileName, MAX_PATH, HardDrivePath, "sasi%d.dat", i);

		SASIDisc[i] = fopen(FileName, "rb+");

		if (SASIDisc[i] == nullptr)
		{
			char* Error = _strerror(nullptr);
			Error[strlen(Error) - 1] = '\0'; // Remove trailing '\n'

			mainWin->Report(MessageType::Error,
			                "Could not open Torch Z80 SASI disc image:\n  %s\n\n%s", FileName, Error);
		}
	}

	SASIBusFree();
}

void SASIWrite(int Address, unsigned char Value)
{
	if (!SCSIDriveEnabled)
		return;

	// WriteLog("SASIWrite Address = 0x%02x, Value = 0x%02x, Phase = %d, PC = 0x%04x\n", Address, Value, sasi.phase, ProgramCounter);

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

unsigned char SASIRead(int Address)
{
	if (!SCSIDriveEnabled)
		return 0xff;

	unsigned char data = 0xff;

	switch (Address)
	{
		case 0x00: // Data Register
			data = SASIReadData();
			break;

		case 0x01: // Status Register
		case 0x02:
			data = 0x01;

			if (!sasi.sel) data |= 0x80;
			if (!sasi.req) data |= 0x40;
			if (sasi.cd) data |= 0x20;
			if (sasi.io) data |= 0x10;
			if (sasi.irq) data |= 0x08;
			if (!sasi.msg) data |= 0x04;
			if (!sasi.bsy) data |= 0x02;
			break;

		case 0x03:
			break;
	}

	// WriteLog("SASIRead Address = 0x%02x, Value = 0x%02x, Phase = %d, PC = 0x%04x\n", Address, data, sasi.phase, ProgramCounter);

	return data;
}

void SASIClose()
{
	for (int i = 0; i < 4; ++i)
	{
		if (SASIDisc[i] != nullptr)
		{
			fclose(SASIDisc[i]);
			SASIDisc[i] = nullptr;
		}
	}
}

static unsigned char SASIReadData()
{
	unsigned char data;

	switch (sasi.phase)
	{
		case Phase::Status:
			data = (unsigned char)sasi.status;
			sasi.req = false;
			SASIMessage();
			return data;

		case Phase::Message:
			data = sasi.message;
			sasi.req = false;
			SASIBusFree();
			return data;

		case Phase::Read:
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

		case Phase::BusFree:
			return sasi.lastwrite;

		default:
			SASIBusFree();
			return sasi.lastwrite;
	}
}

static void SASIWriteData(unsigned char data)
{
	sasi.lastwrite = data;

	switch (sasi.phase)
	{
		case Phase::BusFree:
			if (sasi.sel) {
				SASISelection(data);
			}
			return;

		case Phase::Selection:
			if (!sasi.sel) {
				SASICommand();
				return;
			}
			break;

		case Phase::Command:
			sasi.cmd[sasi.offset] = data;
			sasi.offset++;
			sasi.length--;
			sasi.req = false;

			if (sasi.length == 0) {
				SASIExecute();
				return;
			}
			return;

		case Phase::Write:
			sasi.buffer[sasi.offset] = data;
			sasi.offset++;
			sasi.length--;
			sasi.req = false;

			if (sasi.length > 0)
				return;

			switch (sasi.cmd[0]) {
				case 0x0a:
				case 0x0c:
					break;
				default:
					SASIStatus();
					return;
			}

			switch (sasi.cmd[0]) {
				case 0x0a:
					if (!SASIWriteSector(sasi.buffer, sasi.next - 1)) {
						sasi.status = (sasi.lun << 5) | 0x02;
						sasi.message = 0;
						SASIStatus();
						return;
					}
					break;
				case 0x0c:
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

		default:
			break;
	}

	SASIBusFree();
}

static void SASIBusFree()
{
	sasi.msg = false;
	sasi.cd = false;
	sasi.io = false;
	sasi.bsy = false;
	sasi.req = false;
	sasi.irq = false;

	sasi.phase = Phase::BusFree;

	LEDs.HDisc[0] = 0;
	LEDs.HDisc[1] = 0;
	LEDs.HDisc[2] = 0;
	LEDs.HDisc[3] = 0;
}

static void SASISelection(int /* data */)
{
	sasi.bsy = true;
	sasi.phase = Phase::Selection;
}

static void SASICommand()
{
	sasi.phase = Phase::Command;

	sasi.io = false;
	sasi.cd = true;
	sasi.msg = false;

	sasi.offset = 0;
	sasi.length = 6;
}

static void SASIExecute()
{
	sasi.phase = Phase::Execute;

	// WriteLog("Execute 0x%02x, Param 1=0x%02x, Param 2=0x%02x, Param 3=0x%02x, Param 4=0x%02x, Param 5=0x%02x, Phase = %d, PC = 0x%04x\n",
	//     sasi.cmd[0], sasi.cmd[1], sasi.cmd[2], sasi.cmd[3], sasi.cmd[4], sasi.cmd[5], sasi.phase, ProgramCounter);

	sasi.lun = (sasi.cmd[1]) >> 5;

	LEDs.HDisc[sasi.lun] = 1;

	switch (sasi.cmd[0])
	{
		case 0x00:
			SASITestUnitReady();
			return;

		case 0x01:
			SASIRezero();
			return;

		case 0x03:
			SASIRequestSense();
			return;

		case 0x04:
			SASIFormat();
			return;

		case 0x08:
			SASIRead();
			return;

		case 0x09:
			SASIVerify();
			return;

		case 0x0a:
			SASIWrite();
			return;

		case 0x0b:
			SASISeek();
			return;

		case 0x0c:
			SASISetGeometory();
			return;

		case 0xe0:
			SASIRamDiagnostics();
			return;

		case 0xe4:
			SASIControllerDiagnostics();
			return;
	}

	// WriteLog("Unknown Command 0x%02x, Param 1=0x%02x, Param 2=0x%02x, Param 3=0x%02x, Param 4=0x%02x, Param 5=0x%02x, Phase = %d, PC = 0x%04x\n",
	//          sasi.cmd[0], sasi.cmd[1], sasi.cmd[2], sasi.cmd[3], sasi.cmd[4], sasi.cmd[5], sasi.phase, ProgramCounter);

	sasi.status = (sasi.lun << 5) | 0x02;
	sasi.message = 0x00;
	SASIStatus();
}

static void SASIStatus()
{
	sasi.phase = Phase::Status;

	sasi.io = true;
	sasi.cd = true;
	sasi.msg = false;
	sasi.req = true;
}

static void SASIMessage()
{
	sasi.phase = Phase::Message;

	sasi.io = true;
	sasi.cd = true;
	sasi.msg = true;
	sasi.req = true;
}

static bool SASIDiscTestUnitReady(unsigned char * /* buf */)
{
	return SASIDisc[sasi.lun] != nullptr;
}

static void SASITestUnitReady()
{
	bool status = SASIDiscTestUnitReady(sasi.cmd);

	if (status)
	{
		sasi.status = 0x00;
		sasi.message = 0x00;
	}
	else
	{
		sasi.status = (sasi.lun << 5) | 0x02;
		sasi.message = 0x00;
	}

	SASIStatus();
}

static void SASIRequestSense()
{
	sasi.length = SASIDiscRequestSense(sasi.cmd, sasi.buffer);

	if (sasi.length > 0)
	{
		sasi.offset = 0;
		sasi.blocks = 1;
		sasi.phase = Phase::Read;
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

static int SASIDiscRequestSense(unsigned char *cdb, unsigned char *buf)
{
	int size = cdb[4];
	if (size == 0)
		size = 4;

	switch (sasi.code)
	{
		case 0x00:
			buf[0] = 0x00;
			buf[1] = 0x00;
			buf[2] = 0x00;
			buf[3] = 0x00;
			break;

		case 0x80:
			buf[0] = 0x80;
			buf[1] = (unsigned char)(((sasi.sector >> 16) & 0xff) | (sasi.lun << 5));
			buf[2] = (sasi.sector >> 8) & 0xff;
			buf[3] = (sasi.sector & 0xff);
			break;
	}

	// WriteLog("Returning Sense 0x%02x, 0x%02x, 0x%02x, 0x%02x\n", buf[0], buf[1], buf[2], buf[3]);

	sasi.code = 0x00;
	sasi.sector = 0x00;

	return size;
}

static void SASIRead()
{
	int record = sasi.cmd[1] & 0x1f;
	record <<= 8;
	record |= sasi.cmd[2];
	record <<= 8;
	record |= sasi.cmd[3];
	sasi.blocks = sasi.cmd[4];
	if (sasi.blocks == 0)
		sasi.blocks = 0x100;

	// WriteLog("ReadSector 0x%08x, Blocks %d\n", record, sasi.blocks);

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

	sasi.phase = Phase::Read;
	sasi.io = true;
	sasi.cd = false;
	sasi.msg = false;

	sasi.req = true;
}

static int SASIReadSector(unsigned char *buf, int block)
{
	memset(buf, 0x55, 256);

	// WriteLog"Reading Sector 0x%08x\n", block);

	if (SASIDisc[sasi.lun] == NULL) return 0;

	fseek(SASIDisc[sasi.lun], block * 256, SEEK_SET);

	fread(buf, 256, 1, SASIDisc[sasi.lun]);

	return 256;
}

static bool SASIWriteSector(unsigned char *buf, int block)
{
	if (SASIDisc[sasi.lun] == NULL) return false;

	fseek(SASIDisc[sasi.lun], block * 256, SEEK_SET);

	fwrite(buf, 256, 1, SASIDisc[sasi.lun]);

	return true;
}

static void SASIWrite()
{
	int record = sasi.cmd[1] & 0x1f;
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

	sasi.phase = Phase::Write;
	sasi.io = false;
	sasi.cd = false;
	sasi.msg = false;

	sasi.req = true;
}

static void SASISetGeometory()
{
	sasi.length = 8;
	sasi.blocks = 1;

	sasi.status = 0x00;
	sasi.message = 0x00;

	sasi.next = 0;
	sasi.offset = 0;

	sasi.phase = Phase::Write;
	sasi.io = false;
	sasi.cd = false;
	sasi.msg = false;

	sasi.req = true;
}

static bool SASIWriteGeometory(unsigned char * /* buf */)
{
	// WriteLog("Write Geometory 0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x,0x%02x\n",
	//          buf[0], buf[1], buf[2], buf[3], buf[4], buf[5], buf[6], buf[7], buf[8]);

	if (SASIDisc[sasi.lun] == NULL) return false;

	return true;
}

static bool SASIDiscFormat(unsigned char *buf)
{
	if (SASIDisc[sasi.lun] != nullptr) {
		fclose(SASIDisc[sasi.lun]);
		SASIDisc[sasi.lun] = nullptr;
	}

	int record = buf[1] & 0x1f;
	record <<= 8;
	record |= buf[2];
	record <<= 8;
	record |= buf[3];

	char FileName[MAX_PATH];
	MakeFileName(FileName, MAX_PATH, HardDrivePath, "sasi%d.dat", sasi.lun);

	SASIDisc[sasi.lun] = fopen(FileName, "wb");
	if (SASIDisc[sasi.lun] != nullptr) fclose(SASIDisc[sasi.lun]);
	SASIDisc[sasi.lun] = fopen(FileName, "rb+");

	if (SASIDisc[sasi.lun] == NULL) return false;

	return true;
}

static void SASIFormat()
{
	bool status = SASIDiscFormat(sasi.cmd);

	if (status) {
		sasi.status = 0x00;
		sasi.message = 0x00;
	} else {
		sasi.status = (sasi.lun << 5) | 0x02;
		sasi.message = 0x00;
	}

	SASIStatus();
}

static bool SASIDiscRezero(unsigned char * /* buf */)
{
	return true;
}

void SASIRezero()
{
	bool status = SASIDiscRezero(sasi.cmd);

	if (status) {
		sasi.status = 0x00;
		sasi.message = 0x00;
	} else {
		sasi.status = (sasi.lun << 5) | 0x02;
		sasi.message = 0x00;
	}

	SASIStatus();
}

static void SASIVerify()
{
	int sector = sasi.cmd[1] & 0x1f;
	sector <<= 8;
	sector |= sasi.cmd[2];
	sector <<= 8;
	sector |= sasi.cmd[3];
	// int blocks = sasi.cmd[4];

	// WriteLog("Verifying sector %d, blocks %d\n", sector, blocks);

	bool status = true;

	if (status) {
		sasi.status = 0x00;
		sasi.message = 0x00;
	} else {
		sasi.status = (sasi.lun << 5) | 0x02;
		sasi.message = 0x00;
	}

	SASIStatus();
}

static void SASIRamDiagnostics()
{
	sasi.status = 0x00;
	sasi.message = 0x00;
	SASIStatus();
}

static void SASIControllerDiagnostics()
{
	sasi.status = 0x00;
	sasi.message = 0x00;
	SASIStatus();
}

static void SASISeek()
{
	int sector = sasi.cmd[1] & 0x1f;
	sector <<= 8;
	sector |= sasi.cmd[2];
	sector <<= 8;
	sector |= sasi.cmd[3];

	// WriteLog("Seeking Sector 0x%08x\n", sector);

	sasi.status = 0x00;
	sasi.message = 0x00;

	SASIStatus();
}
