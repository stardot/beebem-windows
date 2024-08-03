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
+00     data                        R/W
+01     read status                 R
+02     write select                W
+03     write irq enable            W
*/

#include <windows.h>

#include <stdio.h>
#include <stdlib.h>

#include "Scsi.h"
#include "6502core.h"
#include "BeebMem.h"
#include "DebugTrace.h"
#include "FileUtils.h"
#include "Log.h"
#include "Main.h"

static unsigned char ReadData();
static void WriteData(unsigned char data);
static void BusFree();
static void Selection(int data);
static void Command();
static void Execute();
static void Status();
static void Message();
static bool DiscTestUnitReady(unsigned char *buf);
static void TestUnitReady();
static bool DiscStartStop(unsigned char *buf);
static void StartStop();
static void RequestSense();
static int DiscRequestSense(unsigned char *cdb, unsigned char *buf);
static void Read6();
static int ReadSector(unsigned char *buf, int block);
static bool WriteSector(unsigned char *buf, int block);
static void Write6();
static void ModeSense();
static int DiscModeSense(unsigned char *cdb, unsigned char *buf);
static void ModeSelect();
static bool WriteGeometory(unsigned char *buf);
static bool DiscFormat(unsigned char *buf);
static void Format();
static bool DiscVerify(unsigned char *buf);
static void Verify();
static void Translate();

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

struct scsi_t
{
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

static scsi_t scsi;
static FILE *SCSIDisc[4] = { nullptr, nullptr, nullptr, nullptr };
static int SCSISize[4];

bool SCSIDriveEnabled = false;

void SCSIReset()
{
	scsi.code = 0x00;
	scsi.sector = 0x00;

	SCSIClose();

	if (!SCSIDriveEnabled)
	{
		return;
	}

	for (int i = 0; i < 4; ++i)
	{
		char FileName[MAX_PATH];
		MakeFileName(FileName, MAX_PATH, HardDrivePath, "scsi%d.dat", i);

		SCSIDisc[i] = fopen(FileName, "rb+");

		if (SCSIDisc[i] == nullptr)
		{
			char* Error = _strerror(nullptr);
			Error[strlen(Error) - 1] = '\0'; // Remove trailing '\n'

			mainWin->Report(MessageType::Error,
			                "Could not open SCSI disc image:\n  %s\n\n%s", FileName, Error);
		}

		SCSISize[i] = 0;

		if (SCSIDisc[i] != nullptr)
		{
			MakeFileName(FileName, MAX_PATH, HardDrivePath, "scsi%d.dsc", i);

			FILE *f = fopen(FileName, "rb");

			if (f != nullptr)
			{
				unsigned char buff[22];
				fread(buff, 1, 22, f);

				// heads = buf[15];
				// cyl   = buf[13] * 256 + buf[14];

				SCSISize[i] = buff[15] * (buff[13] * 256 + buff[14]) * 33; // Number of sectors on disk = heads * cyls * 33

				fclose(f);
			}
		}
	}

	BusFree();
}

void SCSIWrite(int Address, unsigned char Value)
{
	if (!SCSIDriveEnabled)
		return;

	// WriteLog("SCSIWrite Address = 0x%02x, Value = 0x%02x, Phase = %d, PC = 0x%04x\n", Address, Value, scsi.phase, ProgramCounter);

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

unsigned char SCSIRead(int Address)
{
	if (!SCSIDriveEnabled)
		return 0xff;

	unsigned char data = 0xff;

	switch (Address)
	{
		case 0x00: // Data Register
			data = ReadData();
			break;

		case 0x01: // Status Register
			data = 0x20; // Hmmm.. don't know why req has to always be active ? If start at 0x00, ADFS lock up on entry
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

	// WriteLog("SCSIRead Address = 0x%02x, Value = 0x%02x, Phase = %d, PC = 0x%04x\n", Address, data, scsi.phase, ProgramCounter);

	return data;
}

void SCSIClose()
{
	for (int i = 0; i < 4; ++i)
	{
		if (SCSIDisc[i] != nullptr)
		{
			fclose(SCSIDisc[i]);
			SCSIDisc[i] = nullptr;
		}
	}
}

static unsigned char ReadData()
{
	unsigned char data;

	switch (scsi.phase)
	{
		case Phase::Status:
			data = (unsigned char)scsi.status;
			scsi.req = false;
			Message();
			return data;

		case Phase::Message:
			data = scsi.message;
			scsi.req = false;
			BusFree();
			return data;

		case Phase::Read:
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

		case Phase::BusFree:
			return scsi.lastwrite;

		default:
			BusFree();
			return scsi.lastwrite;
	}
}

static void WriteData(unsigned char data)
{
	scsi.lastwrite = data;

	switch (scsi.phase)
	{
		case Phase::BusFree:
			if (scsi.sel) {
				Selection(data);
			}
			return;

		case Phase::Selection:
			if (!scsi.sel) {
				Command();
				return;
			}
			break;

		case Phase::Command:
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

		case Phase::Write:
			scsi.buffer[scsi.offset] = data;
			scsi.offset++;
			scsi.length--;
			scsi.req = false;

			if (scsi.length > 0)
				return;

			switch (scsi.cmd[0]) {
				case 0x0a:
				case 0x15:
				case 0x2a:
				case 0x2e:
					break;
				default:
					Status();
					return;
			}

			switch (scsi.cmd[0]) {
				case 0x0a:
					if (!WriteSector(scsi.buffer, scsi.next - 1)) {
						scsi.status = (scsi.lun << 5) | 0x02;
						scsi.message = 0;
						Status();
						return;
					}
					break;

				case 0x15:
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

		default:
			break;
	}

	BusFree();
}

static void BusFree()
{
	scsi.msg = false;
	scsi.cd = false;
	scsi.io = false;
	scsi.bsy = false;
	scsi.req = false;
	scsi.irq = false;

	scsi.phase = Phase::BusFree;

	LEDs.HDisc[0] = false;
	LEDs.HDisc[1] = false;
	LEDs.HDisc[2] = false;
	LEDs.HDisc[3] = false;
}

static void Selection(int /* data */)
{
	scsi.bsy = true;
	scsi.phase = Phase::Selection;
}

static void Command(void)
{
	scsi.phase = Phase::Command;

	scsi.io = false;
	scsi.cd = true;
	scsi.msg = false;

	scsi.offset = 0;
	scsi.length = 6;
}

static void Execute(void)
{
	scsi.phase = Phase::Execute;

	// if (scsi.cmd[0] <= 0x1f) {
	// 		WriteLog("Execute 0x%02x, Param 1=0x%02x, Param 2=0x%02x, Param 3=0x%02x, Param 4=0x%02x, Param 5=0x%02x, Phase = %d, PC = 0x%04x\n",
	// 				scsi.cmd[0], scsi.cmd[1], scsi.cmd[2], scsi.cmd[3], scsi.cmd[4], scsi.cmd[5], scsi.phase, ProgramCounter);
	// } else {
	// 		WriteLog("Execute 0x%02x, Param 1=0x%02x, Param 2=0x%02x, Param 3=0x%02x, Param 4=0x%02x, Param 5=0x%02x, Param 6=0x%02x, Param 7=0x%02x, Param 8=0x%02x, Param 9=0x%02x, Phase = %d, PC = 0x%04x\n",
	// 				scsi.cmd[0], scsi.cmd[1], scsi.cmd[2], scsi.cmd[3], scsi.cmd[4], scsi.cmd[5], scsi.cmd[6], scsi.cmd[7], scsi.cmd[8], scsi.cmd[9], scsi.phase, ProgramCounter);
	// }

	scsi.lun = (scsi.cmd[1]) >> 5;

	LEDs.HDisc[scsi.lun] = 1;

	switch (scsi.cmd[0])
	{
		case 0x00:
			TestUnitReady();
			return;

		case 0x03:
			RequestSense();
			return;

		case 0x04:
			Format();
			return;

		case 0x08:
			Read6();
			return;

		case 0x0a:
			Write6();
			return;

		case 0x0f:
			Translate();
			return;

		case 0x15:
			ModeSelect();
			return;

		case 0x1a:
			ModeSense();
			return;

		case 0x1b:
			StartStop();
			return;

		case 0x2f:
			Verify();
			return;
	}

	scsi.status = (scsi.lun << 5) | 0x02;
	scsi.message = 0x00;
	Status();
}

static void Status()
{
	scsi.phase = Phase::Status;

	scsi.io = true;
	scsi.cd = true;
	scsi.req = true;
}

static void Message()
{
	scsi.phase = Phase::Message;

	scsi.msg = true;
	scsi.req = true;
}

static bool DiscTestUnitReady(unsigned char * /* buf */)
{
	if (SCSIDisc[scsi.lun] == NULL) return false;
	return true;
}

static void TestUnitReady()
{
	bool status = DiscTestUnitReady(scsi.cmd);

	if (status) {
		scsi.status = (scsi.lun << 5) | 0x00;
		scsi.message = 0x00;
	} else {
		scsi.status = (scsi.lun << 5) | 0x02;
		scsi.message = 0x00;
	}

	Status();
}

static bool DiscStartStop(unsigned char *buf)
{
	if (buf[4] & 0x02) {
		// Eject Disc
	}

	return true;
}

static void StartStop()
{
	bool status = DiscStartStop(scsi.cmd);

	if (status) {
		scsi.status = (scsi.lun << 5) | 0x00;
		scsi.message = 0x00;
	} else {
		scsi.status = (scsi.lun << 5) | 0x02;
		scsi.message = 0x00;
	}

	Status();
}

static void RequestSense()
{
	scsi.length = DiscRequestSense(scsi.cmd, scsi.buffer);

	if (scsi.length > 0)
	{
		scsi.offset = 0;
		scsi.blocks = 1;
		scsi.phase = Phase::Read;
		scsi.io = true;
		scsi.cd = false;

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

static int DiscRequestSense(unsigned char *cdb, unsigned char *buf)
{
	int size = cdb[4];

	if (size == 0)
		size = 4;

	switch (scsi.code) {
		case 0x00:
			buf[0] = 0x00;
			buf[1] = 0x00;
			buf[2] = 0x00;
			buf[3] = 0x00;
			break;

		case 0x21:
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

static void Read6()
{
	int record = scsi.cmd[1] & 0x1f;
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

	scsi.phase = Phase::Read;
	scsi.io = true;
	scsi.cd = false;

	scsi.req = true;
}

static int ReadSector(unsigned char *buf, int block)
{
	if (SCSIDisc[scsi.lun] == NULL) return 0;

	fseek(SCSIDisc[scsi.lun], block * 256, SEEK_SET);

	fread(buf, 256, 1, SCSIDisc[scsi.lun]);

	return 256;
}

static bool WriteSector(unsigned char *buf, int block)
{
	if (SCSIDisc[scsi.lun] == NULL) return false;

	fseek(SCSIDisc[scsi.lun], block * 256, SEEK_SET);

	fwrite(buf, 256, 1, SCSIDisc[scsi.lun]);

	return true;
}

static void Write6()
{
	int record = scsi.cmd[1] & 0x1f;
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

	scsi.phase = Phase::Write;
	scsi.cd = false;

	scsi.req = true;
}

static void ModeSense()
{
	scsi.length = DiscModeSense(scsi.cmd, scsi.buffer);

	if (scsi.length > 0)
	{
		scsi.offset = 0;
		scsi.blocks = 1;
		scsi.phase = Phase::Read;
		scsi.io = true;
		scsi.cd = false;

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

static int DiscModeSense(unsigned char *cdb, unsigned char *buf)
{
	if (SCSIDisc[scsi.lun] == NULL) return 0;

	char FileName[MAX_PATH];
	MakeFileName(FileName, MAX_PATH, HardDrivePath, "scsi%d.dsc", scsi.lun);

	FILE *f = fopen(FileName, "rb");

	if (f == nullptr) return 0;

	int size = cdb[4];
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

static void ModeSelect()
{
	scsi.length = scsi.cmd[4];
	scsi.blocks = 1;

	scsi.status = (scsi.lun << 5) | 0x00;
	scsi.message = 0x00;

	scsi.next = 0;
	scsi.offset = 0;

	scsi.phase = Phase::Write;
	scsi.cd = false;

	scsi.req = true;
}

static bool WriteGeometory(unsigned char *buf)
{
	if (SCSIDisc[scsi.lun] == NULL) return false;

	char FileName[MAX_PATH];
	MakeFileName(FileName, MAX_PATH, HardDrivePath, "scsi%d.dsc", scsi.lun);

	FILE *f = fopen(FileName, "wb");

	if (f == nullptr) return false;

	fwrite(buf, 22, 1, f);

	fclose(f);

	return true;
}

static bool DiscFormat(unsigned char * /* buf */)
{
	// Ignore defect list

	if (SCSIDisc[scsi.lun] != nullptr) {
		fclose(SCSIDisc[scsi.lun]);
		SCSIDisc[scsi.lun] = nullptr;
	}

	char FileName[MAX_PATH];
	MakeFileName(FileName, MAX_PATH, HardDrivePath, "scsi%d.dat", scsi.lun);

	SCSIDisc[scsi.lun] = fopen(FileName, "wb");
	if (SCSIDisc[scsi.lun] != nullptr) fclose(SCSIDisc[scsi.lun]);
	SCSIDisc[scsi.lun] = fopen(FileName, "rb+");

	if (SCSIDisc[scsi.lun] == nullptr) return false;

	MakeFileName(FileName, MAX_PATH, HardDrivePath, "scsi%d.dsc", scsi.lun);

	FILE *f = fopen(FileName, "rb");

	if (f != nullptr)
	{
		unsigned char buff[22];
		fread(buff, 1, 22, f);

		// heads = buf[15];
		// cyl   = buf[13] * 256 + buf[14];

		// Number of sectors on disk = heads * cyls * 33
		SCSISize[scsi.lun] = buff[15] * (buff[13] * 256 + buff[14]) * 33;

		fclose(f);
	}

	return true;
}

static void Format()
{
	bool status = DiscFormat(scsi.cmd);

	if (status) {
		scsi.status = (scsi.lun << 5) | 0x00;
		scsi.message = 0x00;
	} else {
		scsi.status = (scsi.lun << 5) | 0x02;
		scsi.message = 0x00;
	}

	Status();
}

static bool DiscVerify(unsigned char * /* buf */)
{
	int sector = scsi.cmd[1] & 0x1f;
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

static void Verify()
{
	bool status = DiscVerify(scsi.cmd);

	if (status) {
		scsi.status = (scsi.lun << 5) | 0x00;
		scsi.message = 0x00;
	} else {
		scsi.status = (scsi.lun << 5) | 0x02;
		scsi.message = 0x00;
	}

	Status();
}

static void Translate()
{
	int record = scsi.cmd[1] & 0x1f;
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
	scsi.phase = Phase::Read;
	scsi.io = true;
	scsi.cd = false;

	scsi.status = (scsi.lun << 5) | 0x00;
	scsi.message = 0x00;

	scsi.req = true;
}
