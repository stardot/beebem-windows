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

// IDE Support for BeebEm
//       2006, Jon Welch  : Initial code
// 26/12/2010, J.G.Harston:
//             Disk images at DiscsPath, not AppPath
// 28/12/2016, J.G.Harston:
//             Checks for valid drive moved to allow command/status
//             to be accessible. SetGeometry sets IDEStatus correctly.
//             Seek returns correct IDEStatus for invalid/absent drive.

/*
IDE Registers
Offset  Description             Access
+00     data (low byte)           RW
+01     error if status b0 set    R
+02     count                     RW
+03     sector                    RW
+04     cylinder (low)            RW
+05     cylinder (high)           RW
+06     head and device           RW
+07     status/command            RW
*/

#include <windows.h>

#include <stdio.h>
#include <stdlib.h>

#include "Ide.h"
#include "6502core.h"
#include "BeebMem.h"
#include "BeebWin.h"
#include "FileUtils.h"
#include "Main.h"
#include "Resource.h"

bool IDEDriveEnabled = false;

static unsigned char IDERegs[8];
static unsigned char IDEStatus;
static unsigned char IDEError;
static int IDEData;

constexpr int IDEDriveMax = 4;

static FILE *IDEDisc[IDEDriveMax] = { nullptr, nullptr, nullptr, nullptr };
static int IDEDrive;
static int IDEnumSectors = 64;
static int IDEnumHeads = 4;

static void DoIDESeek();
static void DoIDEGeometry();

void IDEReset()
{
	IDEStatus = 0x50;
	IDEError = 0;
	IDEData = 0;

	// NB: Only mount drives 0-IDEDriveMax
	for (int i = 0; i < IDEDriveMax; ++i)
	{
		char FileName[MAX_PATH];
		MakeFileName(FileName, MAX_PATH, HardDrivePath, "ide%d.dat", i);

		IDEDisc[i] = fopen(FileName, "rb+");

		if (IDEDisc[i] == nullptr)
		{
			char* Error = _strerror(nullptr);
			Error[strlen(Error) - 1] = '\0'; // Remove trailing '\n'

			mainWin->Report(MessageType::Error,
			                "Could not open IDE disc image:\n  %s\n\n%s", FileName, Error);
		}
	}
}

void IDEWrite(int Address, unsigned char Value)
{
	IDERegs[Address] = Value;
	IDEError = 0;

	if (Address == 0x07) // Command Register
	{
		if (Value == 0x20) DoIDESeek();         // Read command
		if (Value == 0x30) DoIDESeek();         // Write command
		if (Value == 0x91) DoIDEGeometry();     // Set Geometry command
		// if (Value == 0xEC) DoIDEIdentify();     // Identify command
		return;
	}

	if (IDEDrive >= IDEDriveMax)    return; // This check must be after command register
	if (IDEDisc[IDEDrive] == NULL)  return; //  to allow another drive to be selected

	if (Address == 0x00) // Write Data
	{
		// If in data write cycle, write data byte to file
		if (IDEData > 0)
		{
			fputc(Value, IDEDisc[IDEDrive]);
			IDEData--;

			// If written all data, reset Data Ready
			if (IDEData == 0)
			{
				IDEStatus &= ~0x08;
			}
		}
	}
}

unsigned char IDERead(int Address)
{
	unsigned char data = 0xff;

	switch (Address)
	{
		case 0x00: // Data register
			// This check must be here to allow status registers to be read
			if (IDEDrive >= IDEDriveMax)
			{
				IDEData = 0;
			}
			else
			{
				if (IDEDisc[IDEDrive] == nullptr)
				{
					IDEData = 0;
				}
			}

			// If in data read cycle, read data byte from file
			if (IDEData > 0)
			{
				data = fgetc(IDEDisc[IDEDrive]);
				IDEData--;

				// If read all data, reset Data Ready
				if (IDEData == 0)
				{
					IDEStatus &= ~0x08;
				}
			}
			break;

		case 0x01: // Error
			data = IDEError;
			break;

		case 0x07: // Status
			data = IDEStatus;
			break;

		default: // Other registers
			data = IDERegs[Address];
			break;
	}

	return data;
}

void IDEClose()
{
	for (int i = 0; i < IDEDriveMax; ++i)
	{
		if (IDEDisc[i] != nullptr)
		{
			fclose(IDEDisc[i]);
			IDEDisc[i] = nullptr;
		}
	}
}

/*                    Heads<5  Heads>4
 *    Track     Head   Drive    Drive
 *     0-8191    0+n     0        0
 *  8192-16383   0+n     1        0
 * 16384-32767   0+n              1
 * 32768-49151   0+n              2
 * 49152-65535   0+n              3
 *     0-8191   16+n     2        4
 *  8192-16383  16+n     3        4
 * 16384-32767  16+n              5
 * 32768-49151  16+n              6
 * 49152-65535  16+n              7
 */

void DoIDESeek()
{
	int Sector;
	int Track;
	int Head;
	int MS;

	IDEData = IDERegs[2] * 256;                 // Number of sectors to read/write, * 256 = bytes
	Sector = IDERegs[3] - 1;                    // Sector number 0 - 63
	Track = (IDERegs[4] + IDERegs[5] * 256);    // Track number

	if (IDEnumHeads < 5) {
		MS = Track / 8192;                        // Drive bit 0 (0/1 or 2/3)
		Track = Track & 8191;                     // Track 0 - 8191
		Head = IDERegs[6] & 0x03;                 // Head 0 - 3
		IDEDrive = (IDERegs[6] & 16) / 8 + MS;    // Drive 0 - 3
	} else {
		MS = Track / 16384;                       // Drive bit 0-1 (0-3 or 4-7)
		Track = Track & 16383;                    // Track 0 - 16383
		Head = IDERegs[6] & 0x0F;                 // Head 0 - 15
		IDEDrive = (IDERegs[6] & 16) / 4 + MS;    // Drive 0 - 7
	}

	//  = (Track * 4L * 64L + Head * 64L + Sector ) * 256L; // Absolute position within file
	long pos = (Track * IDEnumHeads * IDEnumSectors + Head * IDEnumSectors + Sector) * 256L;

	if (IDEDrive >= IDEDriveMax) {
		// Drive out of range
		IDEStatus = 0x01; // Not busy, error occured
		IDEError = 0x10;  // Sector not found (no media present)
		return;
	}

	if (IDEDisc[IDEDrive] == nullptr) {
		// No drive image present
		IDEStatus = 0x01; // Not busy, error occured
		IDEError = 0x10;  // Sector not found (no media present)
		return;
	}

	fseek(IDEDisc[IDEDrive], pos, SEEK_SET);
	IDEStatus |= 0x08; // Data Ready
}

void DoIDEGeometry()
{
	IDEnumSectors = IDERegs[3];                 // Number of sectors
	IDEnumHeads = (IDERegs[6] & 0x0F) + 1;      // Number of heads
	IDEStatus = 0x50;                           // Not busy, Ready
}
