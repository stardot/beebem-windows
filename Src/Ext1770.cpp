/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2001  Richard Gellman

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

#include <windows.h>

#include "Ext1770.h"
#include "BeebMem.h"
#include "Disc1770.h"

/*--------------------------------------------------------------------------*/

struct DriveControlBlock
{
	int FDCAddress; // 1770 FDC chip address
	int DCAddress; // Drive Control Register Address
	const char *BoardName; // FDC Board name
	bool TR00_ActiveHigh; // Set true if the TR00 input is Active High
};

// FDC Board extension DLL variables
char FDCDLL[256] = { 0 };

static HMODULE hFDCBoard = nullptr;
static DriveControlBlock ExtBoard = { 0, 0, nullptr };

typedef void (*GetBoardPropertiesFunc)(struct DriveControlBlock *);
typedef unsigned char (*SetDriveControlFunc)(unsigned char);
typedef unsigned char (*GetDriveControlFunc)(unsigned char);

GetBoardPropertiesFunc PGetBoardProperties;
SetDriveControlFunc PSetDriveControl;
GetDriveControlFunc PGetDriveControl;

/*--------------------------------------------------------------------------*/

void Ext1770Reset()
{
	if (hFDCBoard != nullptr)
	{
		FreeLibrary(hFDCBoard);
		hFDCBoard = nullptr;
	}

	PGetBoardProperties = nullptr;
	PSetDriveControl = nullptr;
	PGetDriveControl = nullptr;
}

/*--------------------------------------------------------------------------*/

Ext1770Result Ext1770Init(const char *FileName)
{
	hFDCBoard = LoadLibrary(FileName);

	if (hFDCBoard == nullptr)
	{
		return Ext1770Result::LoadFailed;
	}

	PGetBoardProperties = (GetBoardPropertiesFunc)GetProcAddress(hFDCBoard, "GetBoardProperties");
	PSetDriveControl = (SetDriveControlFunc)GetProcAddress(hFDCBoard, "SetDriveControl");
	PGetDriveControl = (GetDriveControlFunc)GetProcAddress(hFDCBoard, "GetDriveControl");

	if (PGetBoardProperties == nullptr ||
	    PSetDriveControl == nullptr ||
	    PGetDriveControl == nullptr)
	{
		Ext1770Reset();

		return Ext1770Result::InvalidDLL;
	}

	PGetBoardProperties(&ExtBoard);

	EFDCAddr = ExtBoard.FDCAddress;
	EDCAddr = ExtBoard.DCAddress;
	InvertTR00 = ExtBoard.TR00_ActiveHigh;

	return Ext1770Result::Success;
}

/*--------------------------------------------------------------------------*/

bool HasFDCBoard()
{
	return hFDCBoard != nullptr;
}

/*--------------------------------------------------------------------------*/

const char* GetFDCBoardName()
{
	return ExtBoard.BoardName;
}

/*--------------------------------------------------------------------------*/

unsigned char GetDriveControl(unsigned char Value)
{
	return PGetDriveControl(Value);
}

/*--------------------------------------------------------------------------*/

unsigned char SetDriveControl(unsigned char Value)
{
	return PSetDriveControl(Value);
}
