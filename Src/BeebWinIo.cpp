/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 1994  Nigel Magnay
Copyright (C) 1997  Mike Wyatt

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

// BeebEm IO support - disk, tape, state, printer, AVI capture

#pragma warning(push)
#pragma warning(disable: 4091) // ignored on left of 'tagGPFIDL_FLAGS' when no variable is declared
#include <shlobj.h>
#pragma warning(pop)

#include <assert.h>
#include <stdio.h>

#include <algorithm>

#pragma warning(push)
#pragma warning(disable: 4458) // declaration of 'xxx' hides class member
using std::min;
using std::max;
#include <gdiplus.h>
#pragma warning(pop)

#include "BeebWin.h"
#include "6502core.h"
#include "AviWriter.h"
#include "BeebMem.h"
#include "BeebWinPrefs.h"
#include "Disc1770.h"
#include "Disc8271.h"
#include "DiscEdit.h"
#include "DiscInfo.h"
#include "DiscType.h"
#include "ExportFileDialog.h"
#include "Ext1770.h"
#include "FileDialog.h"
#include "FileType.h"
#include "FileUtils.h"
#include "KeyMap.h"
#include "Main.h"
#include "Resource.h"
#include "Serial.h"
#include "Sound.h"
#include "TapeControlDialog.h"
#include "Tube.h"
#include "UEFState.h"
#include "UserVia.h"
#include "Version.h"

using namespace Gdiplus;

/****************************************************************************/

void BeebWin::SetImageName(const char *DiscName, int Drive, DiscType Type)
{
	strcpy(DiscInfo[Drive].FileName, DiscName);
	DiscInfo[Drive].Type = Type;
	DiscInfo[Drive].Loaded = true;

	const int maxMenuItemLen = 100;
	char menuStr[maxMenuItemLen+1];
	char *fname = strrchr(DiscInfo[Drive].FileName, '\\');
	if (fname == NULL)
		fname = strrchr(DiscInfo[Drive].FileName, '/');
	if (fname == NULL)
		fname = DiscInfo[Drive].FileName;
	else
		fname++;

	sprintf(menuStr, "Eject Disc %d: ", Drive);
	strncat(menuStr, fname, maxMenuItemLen-strlen(menuStr));
	menuStr[maxMenuItemLen] = '\0';

	MENUITEMINFO mii = {0};
	mii.cbSize = sizeof(mii);
	mii.fMask = MIIM_STRING;
	mii.fType = MFT_STRING;
	mii.dwTypeData = menuStr;
	SetMenuItemInfo(m_hMenu, Drive == 0 ? IDM_EJECTDISC0 : IDM_EJECTDISC1, FALSE, &mii);
}

/****************************************************************************/

void BeebWin::EjectDiscImage(int Drive)
{
	FreeDiscImage(Drive);
	Close1770Disc(Drive);

	DiscInfo[Drive].FileName[0] = '\0';
	DiscInfo[Drive].Type = DiscType::SSD;
	DiscInfo[Drive].Loaded = false;

	MENUITEMINFO mii = {0};
	mii.cbSize = sizeof(mii);
	mii.fMask = MIIM_STRING;
	mii.fType = MFT_STRING;

	if (Drive == 0)
	{
		mii.dwTypeData = "Eject Disc 0";
		SetMenuItemInfo(m_hMenu, IDM_EJECTDISC0, FALSE, &mii);
	}
	else
	{
		mii.dwTypeData = "Eject Disc 1";
		SetMenuItemInfo(m_hMenu, IDM_EJECTDISC1, FALSE, &mii);
	}

	SetDiscWriteProtects();
}

/****************************************************************************/

bool BeebWin::ReadDisc(int Drive, bool bCheckForPrefs)
{
	char DefaultPath[_MAX_PATH];
	char FileName[_MAX_PATH];
	FileName[0] = '\0';

	const char* filter =
		"Auto (*.ssd;*.dsd;*.ad*;*.img;*.dos;*.fsd)\0*.ssd;*.dsd;*.adl;*.adf;*.img;*.dos;*.fsd\0"
		"ADFS Disc (*.adl;*.adf)\0*.adl;*.adf\0"
		"Single Sided Disc (*.ssd)\0*.ssd\0"
		"Double Sided Disc (*.dsd)\0*.dsd\0"
		"Single Sided Disc (*.*)\0*.*\0"
		"Double Sided Disc (*.*)\0*.*\0";

	m_Preferences.GetStringValue(CFG_DISCS_PATH, DefaultPath);
	GetDataPath(m_UserDataPath, DefaultPath);

	FileDialog Dialog(m_hWnd, FileName, sizeof(FileName), DefaultPath, filter);

	bool gotName = Dialog.Open();

	if (gotName)
	{
		if (bCheckForPrefs)
		{
			// Check for file specific preferences files
			CheckForLocalPrefs(FileName, true);
		}

		if (m_AutoSavePrefsFolders)
		{
			unsigned int PathLength = (unsigned int)(strrchr(FileName, '\\') - FileName);
			strncpy(DefaultPath, FileName, PathLength);
			DefaultPath[PathLength] = 0;
			m_Preferences.SetStringValue(CFG_DISCS_PATH, DefaultPath);
		}

		FileType Type = FileType::None;

		switch (Dialog.GetFilterIndex())
		{
			case 1:
				Type = GetFileTypeFromExtension(FileName);
				break;

			case 2:
				Type = FileType::ADFS;
				break;

			case 3:
			case 5:
				Type = FileType::SSD;
				break;

			case 4:
			case 6:
				Type = FileType::DSD;
				break;
		}

		// Another Master 128 Update, brought to you by Richard Gellman
		if (MachineType != Model::Master128 && MachineType != Model::MasterET)
		{
			if (Type == FileType::SSD)
			{
				if (NativeFDC)
				{
					Load8271DiscImage(FileName, Drive, 80, DiscType::SSD);
				}
				else
				{
					Load1770DiscImage(FileName, Drive, DiscType::SSD);
				}
			}
			else if (Type == FileType::DSD)
			{
				if (NativeFDC)
				{
					Load8271DiscImage(FileName, Drive, 80, DiscType::DSD);
				}
				else
				{
					Load1770DiscImage(FileName, Drive, DiscType::DSD);
				}
			}
			else if (Type == FileType::ADFS)
			{
				if (NativeFDC)
				{
					Report(MessageType::Error, "The native 8271 FDC cannot read ADFS discs");
				}
				else
				{
					Load1770DiscImage(FileName, Drive, DiscType::ADFS);
				}
			}
			else if (Type == FileType::FSD)
			{
				if (NativeFDC)
				{
					LoadFSDDiscImage(FileName, Drive);
					SetDiscWriteProtect(Drive, true);
				}
				else
				{
					Report(MessageType::Error, "FSD disc images are only supported with the 8271 FDC");
					return false;
				}
			}
			else if (Type == FileType::DOS || Type == FileType::IMG)
			{
				Report(MessageType::Error, "DOS and IMG format discs can only be opened when in Master 128 mode");
				return false;
			}
		}
		else
		{
			// Master 128
			if (Type == FileType::SSD)
			{
				Load1770DiscImage(FileName, Drive, DiscType::SSD);
			}
			else if (Type == FileType::DSD)
			{
				Load1770DiscImage(FileName, Drive, DiscType::DSD);
			}
			else if (Type == FileType::ADFS)
			{
				Load1770DiscImage(FileName, Drive, DiscType::ADFS); // ADFS OO La La!
			}
			else if (Type == FileType::IMG)
			{
				Load1770DiscImage(FileName, Drive, DiscType::IMG);
			}
			else if (Type == FileType::DOS)
			{
				Load1770DiscImage(FileName, Drive, DiscType::DOS);
			}
			else if (Type == FileType::FSD)
			{
				Report(MessageType::Error, "FSD disc images are only supported with the 8271 FDC");
				return false;
			}
		}

		// Write protect the disc
		if (m_WriteProtectOnLoad)
		{
			SetDiscWriteProtect(Drive, true);
		}
	}

	return(gotName);
}

/****************************************************************************/

bool BeebWin::Load1770DiscImage(const char *FileName, int Drive, DiscType Type)
{
	Disc1770Result Result = ::Load1770DiscImage(FileName, Drive, Type);

	if (Result == Disc1770Result::OpenedReadWrite)
	{
		SetImageName(FileName, Drive, Type);
		EnableMenuItem(Drive == 0 ? IDM_WRITE_PROTECT_DISC0 : IDM_WRITE_PROTECT_DISC1, true);

		return true;
	}
	else if (Result == Disc1770Result::OpenedReadOnly)
	{
		SetImageName(FileName, Drive, Type);
		EnableMenuItem(Drive == 0 ? IDM_WRITE_PROTECT_DISC0 : IDM_WRITE_PROTECT_DISC1, false);

		return true;
	}
	else
	{
		// Disc1770Result::Failed
		Report(MessageType::Error, "Could not open disc file:\n  %s",
		       FileName);

		return false;
	}
}

/****************************************************************************/

bool BeebWin::Load8271DiscImage(const char *FileName, int Drive, int Tracks, DiscType Type)
{
	Disc8271Result Result;

	if (Type == DiscType::SSD)
	{
		Result = LoadSimpleDiscImage(FileName, Drive, 0, Tracks);
	}
	else if (Type == DiscType::DSD)
	{
		Result = LoadSimpleDSDiscImage(FileName, Drive, Tracks);
	}
	else
	{
		assert(Type == DiscType::FSD);
		Result = LoadFSDDiscImage(FileName, Drive);
	}

	if (Result == Disc8271Result::Success)
	{
		SetImageName(FileName, Drive, Type);

		return true;
	}
	else if (Result == Disc8271Result::InvalidFSD)
	{
		Report(MessageType::Error, "Not a valid FSD file:\n  %s", FileName);
	}
	else if (Result == Disc8271Result::InvalidTracks)
	{
		Report(MessageType::Error,
		       "Could not open disc file:\n  %s\n\nInvalid number of tracks",
		       FileName);

		Report(MessageType::Error, "Not a valid FSD file:\n  %s", FileName);
	}
	else
	{
		Report(MessageType::Error, "Could not open disc file:\n  %s", FileName);
	}

	return false;
}

/****************************************************************************/

void BeebWin::LoadTape(void)
{
	char DefaultPath[_MAX_PATH];
	char FileName[_MAX_PATH];
	FileName[0] = '\0';

	const char* filter =
		"Auto (*.uef;*.csw)\0*.uef;*.csw\0"
		"UEF Tape File (*.uef)\0*.uef\0"
		"CSW Tape File (*.csw)\0*.csw\0";

	m_Preferences.GetStringValue(CFG_TAPES_PATH, DefaultPath);
	GetDataPath(m_UserDataPath, DefaultPath);

	FileDialog fileDialog(m_hWnd, FileName, sizeof(FileName), DefaultPath, filter);
	if (fileDialog.Open())
	{
		if (m_AutoSavePrefsFolders)
		{
			unsigned int PathLength = (unsigned int)(strrchr(FileName, '\\') - FileName);
			strncpy(DefaultPath, FileName, PathLength);
			DefaultPath[PathLength] = 0;
			m_Preferences.SetStringValue(CFG_TAPES_PATH, DefaultPath);
		}

		LoadTape(FileName);
	}
}

/****************************************************************************/

bool BeebWin::LoadTape(const char *FileName)
{
	bool Success = false;

	if (HasFileExt(FileName, ".uef"))
	{
		Success = LoadUEFTape(FileName);
	}
	else if (HasFileExt(FileName, ".csw"))
	{
		Success = LoadCSWTape(FileName);
	}
	else
	{
		Report(MessageType::Error, "Unknown tape format:\n  %a", FileName);
	}

	if (Success && TapeControlEnabled)
	{
		TapeControlSetFileName(FileName);
	}

	return Success;
}

/****************************************************************************/

bool BeebWin::NewTapeImage(char *FileName, int Size)
{
	char DefaultPath[_MAX_PATH];
	const char* filter = "UEF Tape File (*.uef)\0*.uef\0";

	m_Preferences.GetStringValue(CFG_TAPES_PATH, DefaultPath);
	GetDataPath(m_UserDataPath, DefaultPath);

	FileDialog fileDialog(m_hWnd, FileName, Size, DefaultPath, filter);

	bool Result = fileDialog.Save();

	if (Result)
	{
		/* Add a file extension if the user did not specify one */
		if (strchr(FileName, '.') == NULL)
		{
			strcat(FileName, ".uef");
		}
	}
	else
	{
		FileName[0] = '\0';
	}

	return Result;
}

/*******************************************************************/

void BeebWin::SelectFDC()
{
	char DefaultPath[_MAX_PATH];
	char FileName[256];
	FileName[0] = '\0';

	const char* filter = "FDC Extension Board Plugin DLL (*.dll)\0*.dll\0";

	strcpy(DefaultPath, m_AppPath);
	strcat(DefaultPath, "Hardware");

	FileDialog fileDialog(m_hWnd, FileName, sizeof(FileName), DefaultPath, filter);
	if (fileDialog.Open())
	{
		// Make path relative to app path
		if (_strnicmp(FileName, m_AppPath, strlen(m_AppPath)) == 0)
		{
			strcpy(FDCDLL, FileName + strlen(m_AppPath));
		}
		else
		{
			strcpy(FDCDLL, FileName);
		}

		LoadFDC(FDCDLL, true);
	}
}

/****************************************************************************/
void BeebWin::NewDiscImage(int Drive)
{
	char DefaultPath[_MAX_PATH];
	char FileName[_MAX_PATH];
	FileName[0] = '\0';

	const char* filter =
		"Single Sided Disc (*.ssd)\0*.ssd\0"
		"Double Sided Disc (*.dsd)\0*.dsd\0"
		"Single Sided Disc (*.*)\0*.*\0"
		"Double Sided Disc (*.*)\0*.*\0"
		"ADFS M (80 Track) Disc (*.adf)\0*.adf\0"
		"ADFS L (160 Track) Disc (*.adl)\0*.adl\0";

	m_Preferences.GetStringValue(CFG_DISCS_PATH, DefaultPath);
	GetDataPath(m_UserDataPath, DefaultPath);

	int FilterIndex;
	m_Preferences.GetDecimalValue(CFG_DISCS_FILTER, FilterIndex, 1);

	if ((MachineType != Model::Master128 && MachineType != Model::MasterET) && NativeFDC && FilterIndex >= 5)
		FilterIndex = 1;

	FileDialog Dialog(m_hWnd, FileName, sizeof(FileName), DefaultPath, filter);
	Dialog.SetFilterIndex(FilterIndex);

	if (Dialog.Save())
	{
		FilterIndex = Dialog.GetFilterIndex();

		if (m_AutoSavePrefsFolders)
		{
			unsigned int PathLength = (unsigned int)(strrchr(FileName, '\\') - FileName);
			strncpy(DefaultPath, FileName, PathLength);
			DefaultPath[PathLength] = 0;
			m_Preferences.SetStringValue(CFG_DISCS_PATH, DefaultPath);
			m_Preferences.SetDecimalValue(CFG_DISCS_FILTER, FilterIndex);
		}

		/* Add a file extension if the user did not specify one */
		if (strchr(FileName, '.') == NULL)
		{
			if (FilterIndex == 1 || FilterIndex == 3)
				strcat(FileName, ".ssd");
			else if (FilterIndex == 2 || FilterIndex == 4)
				strcat(FileName, ".dsd");
			else if (FilterIndex == 5)
				strcat(FileName, ".adf");
			else if (FilterIndex == 6)
				strcat(FileName, ".adl");
		}

		if (FilterIndex == 1 || FilterIndex == 3)
		{
			CreateDFSDiscImage(FileName, Drive, 1, 80);
		}
		else if (FilterIndex == 2 || FilterIndex == 4)
		{
			CreateDFSDiscImage(FileName, Drive, 2, 80);
		}
		else if (FilterIndex == 5 || FilterIndex == 6)
		{
			const int Tracks = FilterIndex == 5 ? 80 : 160;
			bool Success = CreateADFSImage(FileName, Tracks);
			if (Success)
			{
				Load1770DiscImage(FileName, Drive, DiscType::ADFS);
			}
		}

		// Allow disc writes
		SetDiscWriteProtect(Drive, false);

		DiscInfo[Drive].Loaded = true;
		strcpy(DiscInfo[Drive].FileName, FileName);
	}
}

/****************************************************************************/
void BeebWin::CreateDFSDiscImage(const char *FileName, int Drive,
                                 int Heads, int Tracks)
{
	FILE *outfile = fopen(FileName, "wb");

	if (outfile == nullptr)
	{
		Report(MessageType::Error, "Could not create disc file:\n  %s",
		       FileName);
		return;
	}

	bool Success = true;

	const int NumSectors = Tracks * 10;

	// Create the first two sectors on each side - the rest will get created when
	// data is written to it
	for (int Sector = 0; Success && Sector < (Heads == 1 ? 2 : 12); Sector++)
	{
		unsigned char SecData[256];
		memset(SecData, 0, sizeof(SecData));

		if (Sector == 1 || Sector == 11)
		{
			SecData[6] = (NumSectors >> 8) & 0xff;
			SecData[7] = NumSectors & 0xff;
		}

		if (fwrite(SecData, 1, 256, outfile) != 256)
			Success = false;
	}

	if (fclose(outfile) != 0)
		Success = false;

	if (!Success)
	{
		Report(MessageType::Error, "Failed writing to disc file:\n  %s",
		       FileName);
	}
	else
	{
		// Now load the new image into the correct drive

		if (MachineType == Model::Master128 || !NativeFDC)
		{
			Load1770DiscImage(FileName, Drive, Heads == 1 ? DiscType::SSD : DiscType::DSD);
		}
		else
		{
			Load8271DiscImage(FileName, Drive, Tracks, Heads == 1 ? DiscType::SSD : DiscType::DSD);
		}
	}
}

/****************************************************************************/
void BeebWin::SaveState()
{
	char DefaultPath[_MAX_PATH];
	char FileName[_MAX_PATH];
	FileName[0] = '\0';

	const char* filter = "UEF State File (*.uefstate)\0*.uefstate\0";

	m_Preferences.GetStringValue(CFG_STATES_PATH, DefaultPath);
	GetDataPath(m_UserDataPath, DefaultPath);

	FileDialog fileDialog(m_hWnd, FileName, sizeof(FileName), DefaultPath, filter);
	if (fileDialog.Save())
	{
		if (m_AutoSavePrefsFolders)
		{
			unsigned int PathLength = (unsigned int)(strrchr(FileName, '\\') - FileName);
			strncpy(DefaultPath, FileName, PathLength);
			DefaultPath[PathLength] = 0;
			m_Preferences.SetStringValue(CFG_STATES_PATH, DefaultPath);
		}

		// Add UEF extension if not already set and is UEF
		if (!HasFileExt(FileName, ".uefstate"))
		{
			strcat(FileName, ".uefstate");
		}

		SaveUEFState(FileName);
	}
}

/****************************************************************************/
void BeebWin::RestoreState()
{
	char DefaultPath[_MAX_PATH];
	char FileName[_MAX_PATH];
	FileName[0] = '\0';

	const char* filter = "UEF State File (*.uefstate; *.uef)\0*.uefstate;*.uef\0";

	m_Preferences.GetStringValue(CFG_STATES_PATH, DefaultPath);
	GetDataPath(m_UserDataPath, DefaultPath);

	FileDialog fileDialog(m_hWnd, FileName, sizeof(FileName), DefaultPath, filter);
	if (fileDialog.Open())
	{
		// Check for file specific preferences files
		CheckForLocalPrefs(FileName, true);

		if (m_AutoSavePrefsFolders)
		{
			unsigned int PathLength = (unsigned int)(strrchr(FileName, '\\') - FileName);
			strncpy(DefaultPath, FileName, PathLength);
			DefaultPath[PathLength] = 0;
			m_Preferences.SetStringValue(CFG_STATES_PATH, DefaultPath);
		}

		LoadUEFState(FileName);
	}
}

/****************************************************************************/

void BeebWin::ToggleWriteProtect(int Drive)
{
	if (DiscInfo[Drive].Type != DiscType::FSD)
	{
		SetDiscWriteProtect(Drive, !m_WriteProtectDisc[Drive]);
	}
}

void BeebWin::SetDiscWriteProtect(int Drive, bool WriteProtect)
{
	m_WriteProtectDisc[Drive] = WriteProtect;

	// Keep 8271 and 1770 write enable flags in sync
	DiscWriteEnable(Drive, !WriteProtect);
	DWriteable[Drive] = !WriteProtect;

	int MenuItemID = Drive == 0 ? IDM_WRITE_PROTECT_DISC0 : IDM_WRITE_PROTECT_DISC1;

	CheckMenuItem(MenuItemID, m_WriteProtectDisc[Drive]);
}

void BeebWin::SetDiscWriteProtects()
{
	if ((MachineType != Model::Master128 && MachineType != Model::MasterET) && NativeFDC)
	{
		m_WriteProtectDisc[0] = !IsDiscWritable(0);
		m_WriteProtectDisc[1] = !IsDiscWritable(1);

		CheckMenuItem(IDM_WRITE_PROTECT_DISC0, m_WriteProtectDisc[0]);
		CheckMenuItem(IDM_WRITE_PROTECT_DISC1, m_WriteProtectDisc[1]);
	}
	else
	{
		CheckMenuItem(IDM_WRITE_PROTECT_DISC0, !DWriteable[0]);
		CheckMenuItem(IDM_WRITE_PROTECT_DISC1, !DWriteable[1]);
	}
}

/****************************************************************************/

void BeebWin::SetPrinterPort(PrinterPortType PrinterPort)
{
	if (PrinterPort == PrinterPortType::File)
	{
		if (GetPrinterFileName())
		{
			// If printer is enabled then need to
			// disable it before changing file
			if (PrinterEnabled)
				TogglePrinter();

			// Add file name to menu
			std::string MenuString = "File: ";
			MenuString += m_PrinterFileName;

			ModifyMenu(m_hMenu, IDM_PRINTER_FILE,
			           MF_BYCOMMAND, IDM_PRINTER_FILE,
			           MenuString.c_str());

			m_PrinterPort = PrinterPort;

			TranslatePrinterPort();
		}
	}
	else if (PrinterPort == PrinterPortType::Clipboard)
	{
		if (PrinterEnabled)
			TogglePrinter();

		m_PrinterPort = PrinterPort;

		TranslatePrinterPort();
	}
	else
	{
		if (PrinterPort != m_PrinterPort)
		{
			// If printer is enabled then need to
			// disable it before changing file
			if (PrinterEnabled)
				TogglePrinter();

			m_PrinterPort = PrinterPort;

			TranslatePrinterPort();
		}
	}

	UpdatePrinterPortMenu();
}

void BeebWin::UpdatePrinterPortMenu()
{
	static struct { UINT ID; PrinterPortType PrinterPort; } MenuItems[] =
	{
		{ IDM_PRINTER_FILE,      PrinterPortType::File },
		{ IDM_PRINTER_CLIPBOARD, PrinterPortType::Clipboard },
		{ IDM_PRINTER_LPT1,      PrinterPortType::Lpt1 },
		{ IDM_PRINTER_LPT2,      PrinterPortType::Lpt2 },
		{ IDM_PRINTER_LPT3,      PrinterPortType::Lpt3 },
		{ IDM_PRINTER_LPT4,      PrinterPortType::Lpt4 },
	};

	UINT SelectedMenuItemID = 0;

	for (int i = 0; i < _countof(MenuItems); i++)
	{
		if (m_PrinterPort == MenuItems[i].PrinterPort)
		{
			SelectedMenuItemID = MenuItems[i].ID;
			break;
		}
	}

	CheckMenuRadioItem(
		IDM_PRINTER_FILE,
		IDM_PRINTER_LPT4,
		SelectedMenuItemID
	);

	EnableMenuItem(IDM_PRINTER_FILE, !PrinterEnabled);
	EnableMenuItem(IDM_PRINTER_CLIPBOARD, !PrinterEnabled);
	EnableMenuItem(IDM_PRINTER_LPT1, !PrinterEnabled);
	EnableMenuItem(IDM_PRINTER_LPT2, !PrinterEnabled);
	EnableMenuItem(IDM_PRINTER_LPT3, !PrinterEnabled);
	EnableMenuItem(IDM_PRINTER_LPT4, !PrinterEnabled);
}

bool BeebWin::GetPrinterFileName()
{
	char StartPath[_MAX_PATH];
	char FileName[_MAX_PATH];
	FileName[0] = '\0';

	const char* Filter = "Printer Output (*.*)\0*.*\0";

	if (m_PrinterFileName.empty())
	{
		strcpy(StartPath, m_UserDataPath);
		FileName[0] = '\0';
	}
	else
	{
		char drive[_MAX_DRIVE];
		char dir[_MAX_DIR];
		char fname[_MAX_FNAME];
		char ext[_MAX_EXT];
		_splitpath(m_PrinterFileName.c_str(), drive, dir, fname, ext);
		_makepath(StartPath, drive, dir, NULL, NULL);
		_makepath(FileName, NULL, NULL, fname, ext);
	}

	FileDialog Dialog(m_hWnd, FileName, sizeof(FileName), StartPath, Filter);

	bool changed = Dialog.Save();

	if (changed)
	{
		m_PrinterFileName = FileName;
	}

	return changed;
}

/****************************************************************************/

bool BeebWin::TogglePrinter()
{
	bool Success = true;

	m_PrinterBuffer.clear();

	if (PrinterEnabled)
	{
		PrinterDisable();
	}
	else
	{
		if (m_PrinterPort == PrinterPortType::File)
		{
			if (m_PrinterFileName.empty())
			{
				GetPrinterFileName();
			}

			if (!m_PrinterFileName.empty())
			{
				Success = PrinterEnable(m_PrinterFileName.c_str());
			}
		}
		else if (m_PrinterPort == PrinterPortType::Clipboard)
		{
			Success = PrinterEnable(nullptr);
		}
		else
		{
			Success = PrinterEnable(m_PrinterDevice.c_str());
		}
	}

	if (Success)
	{
		CheckMenuItem(IDM_PRINTERONOFF, PrinterEnabled);
	}

	UpdatePrinterPortMenu();

	return Success;
}

/****************************************************************************/

void BeebWin::TranslatePrinterPort()
{
	switch (m_PrinterPort)
	{
		case PrinterPortType::File:
			m_PrinterDevice = m_PrinterFileName;
			break;

		case PrinterPortType::Clipboard:
			m_PrinterDevice.clear();
			break;

		case PrinterPortType::Lpt1:
		default:
			m_PrinterDevice = "LPT1";
			break;

		case PrinterPortType::Lpt2:
			m_PrinterDevice = "LPT2";
			break;

		case PrinterPortType::Lpt3:
			m_PrinterDevice = "LPT3";
			break;

		case PrinterPortType::Lpt4:
			m_PrinterDevice = "LPT4";
			break;
	}
}

/****************************************************************************/

void BeebWin::SetVideoCaptureResolution(VideoCaptureResolution Resolution)
{
	m_VideoCaptureResolution = Resolution;

	UpdateVideoCaptureResolutionMenu();
}

void BeebWin::SetVideoCaptureFrameSkip(int FrameSkip)
{
	m_AviFrameSkip = FrameSkip;

	UpdateVideoCaptureFrameSkipMenu();
}

void BeebWin::UpdateVideoCaptureMenu()
{
	UpdateVideoCaptureResolutionMenu();
	UpdateVideoCaptureFrameSkipMenu();

	EnableMenuItem(IDM_CAPTUREVIDEO, !IsCapturing());
	EnableMenuItem(IDM_ENDVIDEO, IsCapturing());
}

void BeebWin::UpdateVideoCaptureResolutionMenu()
{
	static const struct { UINT ID; VideoCaptureResolution Resolution; } MenuItems[] =
	{
		{ IDM_VIDEORES1, VideoCaptureResolution::Display },
		{ IDM_VIDEORES2, VideoCaptureResolution::_640x512 },
		{ IDM_VIDEORES3, VideoCaptureResolution::_320x256 }
	};

	UINT SelectedMenuItemID = 0;

	for (int i = 0; i < _countof(MenuItems); i++)
	{
		if (m_VideoCaptureResolution == MenuItems[i].Resolution)
		{
			SelectedMenuItemID = MenuItems[i].ID;
		}

		EnableMenuItem(MenuItems[i].ID, !IsCapturing());
	}

	CheckMenuRadioItem(IDM_VIDEORES1, IDM_VIDEORES3, SelectedMenuItemID);
}

void BeebWin::UpdateVideoCaptureFrameSkipMenu()
{
	static const struct { UINT ID; int FrameSkip; } MenuItems[] =
	{
		{ IDM_VIDEOSKIP0, 0 },
		{ IDM_VIDEOSKIP1, 1 },
		{ IDM_VIDEOSKIP2, 2 },
		{ IDM_VIDEOSKIP3, 3 },
		{ IDM_VIDEOSKIP4, 4 },
		{ IDM_VIDEOSKIP5, 5 }
	};

	UINT SelectedMenuItemID = 0;

	for (int i = 0; i < _countof(MenuItems); i++)
	{
		if (m_AviFrameSkip == MenuItems[i].FrameSkip)
		{
			SelectedMenuItemID = MenuItems[i].ID;
		}

		EnableMenuItem(MenuItems[i].ID, !IsCapturing());
	}

	CheckMenuRadioItem(IDM_VIDEOSKIP0, IDM_VIDEOSKIP5, SelectedMenuItemID);
}

/****************************************************************************/

void BeebWin::CaptureVideo()
{
	char DefaultPath[_MAX_PATH];
	char FileName[_MAX_PATH];
	FileName[0] = '\0';

	const char* filter = "AVI File (*.avi)\0*.avi\0";

	m_Preferences.GetStringValue(CFG_AVI_PATH, DefaultPath);
	GetDataPath(m_UserDataPath, DefaultPath);

	FileDialog fileDialog(m_hWnd, FileName, sizeof(FileName), DefaultPath, filter);

	if (fileDialog.Save())
	{
		// Add avi extension
		if (!HasFileExt(FileName, ".avi"))
		{
			strcat(FileName,".avi");
		}

		if (m_AutoSavePrefsFolders)
		{
			unsigned int PathLength = (unsigned int)(strrchr(FileName, '\\') - FileName);
			strncpy(DefaultPath, FileName, PathLength);
			DefaultPath[PathLength] = 0;
			m_Preferences.SetStringValue(CFG_AVI_PATH, DefaultPath);
		}

		// Close AVI file if currently capturing
		EndVideo();

		aviWriter = new AVIWriter();

		WAVEFORMATEX wf;
		WAVEFORMATEX *wfp = nullptr;

		if (SoundEnabled)
		{
			memset(&wf, 0, sizeof(WAVEFORMATEX));
			wf.wFormatTag = WAVE_FORMAT_PCM;
			wf.nChannels = 1;
			wf.nSamplesPerSec = SoundSampleRate;
			wf.wBitsPerSample = 8;
			wf.nBlockAlign = 1;
			wf.nAvgBytesPerSec = SoundSampleRate;
			wf.cbSize = 0;
			wfp = &wf;
		}

		// Create DIB for AVI frames
		m_Avibmi = m_bmi;

		if (m_VideoCaptureResolution == VideoCaptureResolution::Display)
		{
			// Must be multiple of 4
			m_Avibmi.bmiHeader.biWidth = m_XWinSize & (~3);
			m_Avibmi.bmiHeader.biHeight = m_YWinSize;
		}
		else if (m_VideoCaptureResolution == VideoCaptureResolution::_640x512)
		{
			m_Avibmi.bmiHeader.biWidth = 640;
			m_Avibmi.bmiHeader.biHeight = 512;
		}
		else
		{
			m_Avibmi.bmiHeader.biWidth = 320;
			m_Avibmi.bmiHeader.biHeight = 256;
		}

		m_Avibmi.bmiHeader.biSizeImage = m_Avibmi.bmiHeader.biWidth * m_Avibmi.bmiHeader.biHeight;

		m_AviDC = CreateCompatibleDC(nullptr);
		m_AviDIB = CreateDIBSection(m_AviDC,
		                            (BITMAPINFO *)&m_Avibmi,
		                            DIB_RGB_COLORS,
		                            (void**)&m_AviScreen,
		                            nullptr,
		                            0);

		if (SelectObject(m_AviDC, m_AviDIB) == nullptr)
		{
			Report(MessageType::Error, "Failed to initialise AVI buffers");

			EndVideo();
		}
		else
		{
			m_AviFrameSkipCount = 0;
			m_AviFrameCount = 0;

			HRESULT hr = aviWriter->Initialise(FileName,
			                                   wfp,
			                                   &m_Avibmi,
			                                   (int)(50 / (m_AviFrameSkip + 1)));

			if (FAILED(hr))
			{
				Report(MessageType::Error, "Failed to create AVI file");

				EndVideo();
			}
		}
	}
}

/****************************************************************************/

void BeebWin::EndVideo()
{
	if (aviWriter != nullptr)
	{
		delete aviWriter;
		aviWriter = nullptr;
	}

	if (m_AviDIB != nullptr)
	{
		DeleteObject(m_AviDIB);
		m_AviDIB = nullptr;
	}

	if (m_AviDC != nullptr)
	{
		DeleteDC(m_AviDC);
		m_AviDC = nullptr;
	}
}

bool BeebWin::IsCapturing() const
{
	return aviWriter != nullptr;
}

/****************************************************************************/
void BeebWin::QuickLoad()
{
	char FileName[_MAX_PATH];
	strcpy(FileName, m_UserDataPath);
	strcat(FileName, "BeebState\\quicksave.uefstate");

	if (FileExists(FileName))
	{
		LoadUEFState(FileName);
	}
	else
	{
		// For backwards compatiblity with existing quicksave files:
		strcpy(FileName, m_UserDataPath);
		strcat(FileName, "BeebState\\quicksave.uef");
		LoadUEFState(FileName);
	}
}

void BeebWin::QuickSave()
{
	char FileName1[_MAX_PATH];
	char FileName2[_MAX_PATH];

	// Bump old quicksave files down
	for (int i = 1; i <= 9; ++i)
	{
		sprintf(FileName1, "%sBeebState\\quicksave%d.uefstate", m_UserDataPath, i);

		if (i == 9)
		{
			sprintf(FileName2, "%sBeebState\\quicksave.uefstate", m_UserDataPath);
		}
		else
		{
			sprintf(FileName2, "%sBeebState\\quicksave%d.uefstate", m_UserDataPath, i + 1);
		}

		MoveFileEx(FileName2, FileName1, MOVEFILE_REPLACE_EXISTING);
	}

	SaveUEFState(FileName2);
}

/****************************************************************************/

void BeebWin::LoadUEFState(const char *FileName)
{
	UEFStateResult Result = ::LoadUEFState(FileName);

	switch (Result)
	{
		case UEFStateResult::Success:
			SetRomMenu();
			SetDiscWriteProtects();
			break;

		case UEFStateResult::OpenFailed:
			Report(MessageType::Error, "Cannot open state file:\n  %s",
			       FileName);
			break;

		case UEFStateResult::InvalidUEFFile:
			Report(MessageType::Error, "The file selected is not a UEF file:\n  %s",
			       FileName);
			break;

		case UEFStateResult::InvalidUEFVersion:
			Report(MessageType::Error,
			       "Cannot open state file:\n  %s\n\nPlease upgrade to the latest version of BeebEm",
			       FileName);
			break;
	}
}

/****************************************************************************/

void BeebWin::SaveUEFState(const char *FileName)
{
	UEFStateResult Result = ::SaveUEFState(FileName);

	switch (Result)
	{
		case UEFStateResult::Success:
			break;

		case UEFStateResult::WriteFailed:
			Report(MessageType::Error, "Failed to write state file:\n  %s",
			       FileName);
			break;
	}
}

/****************************************************************************/

bool BeebWin::LoadUEFTape(const char *FileName)
{
	UEFResult Result = ::LoadUEFTape(FileName);

	switch (Result)
	{
		case UEFResult::Success:
			return true;

		case UEFResult::NotUEF:
		case UEFResult::NotTape:
			Report(MessageType::Error, "The file selected is not a UEF tape image:\n  %s",
			       FileName);
			return false;

		case UEFResult::NoFile:
		default:
			Report(MessageType::Error, "Cannot open UEF file:\n  %s", FileName);
			return false;
	}
}

/****************************************************************************/

bool BeebWin::LoadCSWTape(const char *FileName)
{
	CSWResult Result = ::LoadCSWTape(FileName);

	switch (Result)
	{
		case CSWResult::Success:
			return true;

		case CSWResult::InvalidCSWFile:
			Report(MessageType::Error, "The file selected is not a CSW file:\n  %s",
			       FileName);
			return false;

		case CSWResult::InvalidHeaderExtension:
			Report(MessageType::Error, "Failed to read CSW header extension:\n  %s",
			       FileName);
			return false;

		case CSWResult::OpenFailed:
		default:
			Report(MessageType::Error, "Cannot open CSW file:\n  %s", FileName);
			return false;
	}
}

/****************************************************************************/
// if DLLName is NULL then FDC setting is read from the registry
// else the named DLL is read in
// if save is true then DLL selection is saved in registry

void BeebWin::LoadFDC(char *DLLName, bool save)
{
	char CfgName[20];
	sprintf(CfgName, "FDCDLL%d", static_cast<int>(MachineType));

	Ext1770Reset();

	NativeFDC = true;

	if (DLLName == nullptr)
	{
		if (!m_Preferences.GetStringValue(CfgName, FDCDLL))
			strcpy(FDCDLL, "None");
		DLLName = FDCDLL;
	}

	if (strcmp(DLLName, "None") != 0)
	{
		char path[_MAX_PATH];
		strcpy(path, DLLName);
		GetDataPath(m_AppPath, path);

		Ext1770Result Result = Ext1770Init(path);

		if (Result == Ext1770Result::Success)
		{
			NativeFDC = false; // at last, a working DLL!
		}
		else if (Result == Ext1770Result::LoadFailed)
		{
			Report(MessageType::Error, "Unable to load FDD Extension Board DLL\nReverting to native 8271");
			strcpy(DLLName, "None");
		}
		else // if (Result == InvalidDLL)
		{
			Report(MessageType::Error, "Invalid FDD Extension Board DLL\nReverting to native 8271");
			strcpy(DLLName, "None");
		}
	}

	if (save)
	{
		m_Preferences.SetStringValue(CfgName, DLLName);
	}

	// Set menu options
	CheckMenuItem(IDM_8271, NativeFDC);
	CheckMenuItem(IDM_FDC_DLL, !NativeFDC);

	DisplayCycles = 7000000;

	if (NativeFDC || MachineType == Model::Master128)
		DisplayCycles = 0;
}

void BeebWin::KillDLLs()
{
	Ext1770Reset();
}

void BeebWin::SetDriveControl(unsigned char Value)
{
	// This takes a value from the mem/io decoder, as written by the CPU,
	// runs it through the DLL's translator, then sends it on to the
	// 1770 FDC in Master 128 form.
	WriteFDCControlReg(::SetDriveControl(Value));
}

unsigned char BeebWin::GetDriveControl()
{
	// Same as above, but in reverse, i.e., reading.
	unsigned char temp = ReadFDCControlReg();
	return ::GetDriveControl(temp);
}

/****************************************************************************/

void BeebWin::SaveBeebEmID(FILE *SUEF)
{
	char EmuName[16];
	memset(EmuName, 0, sizeof(EmuName));

	// BeebEm Title Block
	strcpy(EmuName, "BeebEm");
	EmuName[14] = VERSION_MAJOR;
	EmuName[15] = VERSION_MINOR;
	fwrite(EmuName, 1, sizeof(EmuName), SUEF);
}

void BeebWin::SaveEmuUEF(FILE *SUEF)
{
	// Emulator Specifics
	// Note about this block: It should only be handled by beebem from uefstate.cpp if
	// the UEF has been determined to be from BeebEm (Block 046C)
	fputc(static_cast<unsigned char>(MachineType), SUEF);
	fputc(NativeFDC ? 0 : 1, SUEF);
	fputc(static_cast<unsigned char>(TubeType), SUEF);
	fput16((int)m_KeyboardMapping, SUEF);
	if (m_KeyboardMapping == KeyboardMappingType::User)
	{
		fputstring(m_UserKeyMapPath, SUEF);
	}
	else
	{
		char blank[256];
		memset(blank, 0, sizeof(blank));

		fwrite(blank, 1, sizeof(blank), SUEF);
	}

	fputc(0,SUEF);
}

void BeebWin::LoadEmuUEF(FILE *SUEF, int Version)
{
	int Type = fgetc(SUEF);

	if (Version <= 8 && Type == 1)
	{
		MachineType = Model::Master128;
	}
	else
	{
		MachineType = static_cast<Model>(Type);
	}

	NativeFDC = fgetc(SUEF) == 0;
	TubeType = static_cast<Tube>(fgetc(SUEF));

	if (Version >= 11)
	{
		int KeyboardMapping = fget16(SUEF);

		const int UEF_USER_KEYBOARD_MAPPING = 40060; // Was a menu item ID in BeebEm <= 4.19

		if (KeyboardMapping == (int)KeyboardMappingType::User || KeyboardMapping == UEF_USER_KEYBOARD_MAPPING)
		{
			char FileName[256];
			memset(FileName, 0, sizeof(FileName));

			if (Version >= 14)
			{
				fgetstring(FileName, sizeof(FileName), SUEF);
			}
			else
			{
				fread(FileName, 1, sizeof(FileName), SUEF);
			}

			GetDataPath(m_UserDataPath, FileName);

			if (ReadKeyMap(FileName, &UserKeyMap))
			{
				strcpy(m_UserKeyMapPath, FileName);
			}
			else
			{
				KeyboardMapping = (int)m_KeyboardMapping;
			}
		}

		SetKeyboardMapping(static_cast<KeyboardMappingType>(KeyboardMapping));
	}

	ResetBeebSystem(MachineType, true);
	UpdateModelMenu();
	UpdateTubeMenu();
}

/****************************************************************************/
void BeebWin::GetDataPath(const char *folder, char *path)
{
	char newPath[_MAX_PATH];

	// If path is absolute then use it
	if (path[0] == '\\' || path[0] == '/' ||
		(strlen(path) > 2 && path[1] == ':' && (path[2] == '\\' || path[2] == '/')))
	{
		// Absolute path - just use it as is
	}
	else
	{
		// Relative path
		strcpy(newPath, folder);
		strcat(newPath, path);
		strcpy(path, newPath);
	}
}

/****************************************************************************/
void BeebWin::LoadUserKeyMap()
{
	char FileName[_MAX_PATH];
	FileName[0] = '\0';

	const char* filter = "Key Map File (*.kmap)\0*.kmap\0";

	FileDialog fileDialog(m_hWnd, FileName, sizeof(FileName), m_UserDataPath, filter);
	if (fileDialog.Open())
	{
		if (ReadKeyMap(FileName, &UserKeyMap))
			strcpy(m_UserKeyMapPath, FileName);
	}
}

/****************************************************************************/
void BeebWin::SaveUserKeyMap()
{
	char FileName[_MAX_PATH];
	FileName[0] = '\0';

	const char* filter = "Key Map File (*.kmap)\0*.kmap\0";

	FileDialog fileDialog(m_hWnd, FileName, sizeof(FileName), m_UserDataPath, filter);

	if (fileDialog.Save())
	{
		if (!HasFileExt(FileName, ".kmap"))
		{
			strcat(FileName,".kmap");
		}

		if (WriteKeyMap(FileName, &UserKeyMap))
		{
			strcpy(m_UserKeyMapPath, FileName);
		}
	}
}

/****************************************************************************/

// Clipboard support

// Handles the Edit/Copy menu command

void BeebWin::OnCopy()
{
	if (PrinterEnabled)
		TogglePrinter();

	m_PrinterPort = PrinterPortType::Clipboard;

	TranslatePrinterPort();
	TogglePrinter(); // Turn printer back on
	UpdatePrinterPortMenu();

	m_PrinterBuffer.resize(5);

	m_ClipboardBuffer[0] = 2;
	m_ClipboardBuffer[1] = 'L';
	m_ClipboardBuffer[2] = '.';
	m_ClipboardBuffer[3] = 13;
	m_ClipboardBuffer[4] = 3;
	m_ClipboardIndex = 0;
}

// Handles the Edit/Paste menu command

void BeebWin::OnPaste()
{
	if (!IsClipboardFormatAvailable(CF_TEXT))
		return;

	if (!OpenClipboard(m_hWnd))
		return;

	HGLOBAL hClipboardData = GetClipboardData(CF_TEXT);

	if (hClipboardData != nullptr)
	{
		size_t Size = GlobalSize(hClipboardData); // Includes NUL-terminator

		LPTSTR pData = (LPTSTR)GlobalLock(hClipboardData);

		if (pData != nullptr)
		{
			m_ClipboardBuffer.resize(Size);
			memcpy(&m_ClipboardBuffer[0], pData, Size);

			GlobalUnlock(hClipboardData);

			m_ClipboardIndex = 0;
			m_ClipboardLength = Size - 1;
		}
	}

	CloseClipboard();
}

void BeebWin::ClearClipboardBuffer()
{
	m_ClipboardBuffer.clear();
	m_ClipboardIndex = 0;
	m_ClipboardLength = 0;
}

/****************************************************************************/

// Called when a character is written to the Beeb's printer port,
// and adds to the data buffered to send to the clipboard

void BeebWin::PrintChar(unsigned char Value)
{
	m_PrinterBuffer.push_back(Value);

	if (m_TranslateCRLF && Value == 0xD)
	{
		m_PrinterBuffer.push_back(0xA);
	}

	// To avoid unnecessary copies, wait 1 second after the last write
	// to put the data into the clipboard.
	SetTimer(m_hWnd, TIMER_PRINTER, 1000, nullptr);
}

void BeebWin::CopyPrinterBufferToClipboard()
{
	if (!OpenClipboard(m_hWnd))
		return;

	EmptyClipboard();

	HGLOBAL hClipboardData = GlobalAlloc(GMEM_MOVEABLE, m_PrinterBuffer.size() + 1);

	if (hClipboardData == nullptr)
	{
		CloseClipboard();
		return;
	}

	unsigned char* pData = (unsigned char*)GlobalLock(hClipboardData);
	memcpy(pData, &m_PrinterBuffer[0], m_PrinterBuffer.size());
	pData[m_PrinterBuffer.size()] = '\0';
	GlobalUnlock(hClipboardData);

	SetClipboardData(CF_TEXT, hClipboardData);

	CloseClipboard();
}

/****************************************************************************/
/* Disc Import / Export */

void BeebWin::ExportDiscFiles(int menuId)
{
	const int Drive = menuId == IDM_DISC_EXPORT_0 || menuId == IDM_DISC_EXPORT_2 ? 0 : 1;

	if (MachineType != Model::Master128 && NativeFDC)
	{
		// 8271 controller
		if (DiscInfo[Drive].Type == DiscType::FSD)
		{
			Report(MessageType::Warning, "Export from FSD disc not supported");
			return;
		}
	}
	else
	{
		// 1770 controller
		if (DiscInfo[Drive].Type != DiscType::SSD && DiscInfo[Drive].Type != DiscType::DSD)
		{
			// ADFS - not currently supported
			Report(MessageType::Warning, "Export from ADFS disc not supported");
			return;
		}
	}

	// Check for no disk loaded
	if (DiscInfo[Drive].FileName[0] == '\0' ||
	    (DiscInfo[Drive].Type == DiscType::SSD && !DiscInfo[Drive].DoubleSidedSSD && (menuId == IDM_DISC_EXPORT_2 || menuId == IDM_DISC_EXPORT_3)))
	{
		Report(MessageType::Warning, "No disc loaded in drive %d", menuId - IDM_DISC_EXPORT_0);
		return;
	}

	// Read the catalogue
	const int Heads = DiscInfo[Drive].Type == DiscType::DSD || DiscInfo[Drive].DoubleSidedSSD ? 2 : 1;
	const int Side = menuId == IDM_DISC_EXPORT_0 || menuId == IDM_DISC_EXPORT_1 ? 0 : 1;

	DFS_DISC_CATALOGUE dfsCat;

	bool Success = dfs_get_catalogue(DiscInfo[Drive].FileName, Heads, Side, &dfsCat);

	if (!Success)
	{
		Report(MessageType::Error, "Failed to read catalogue from disc:\n  %s", DiscInfo[Drive].FileName);
		return;
	}

	char szExportPath[MAX_PATH];
	szExportPath[0] = '\0';

	m_Preferences.GetStringValue(CFG_EXPORT_PATH, szExportPath);
	GetDataPath(m_UserDataPath, szExportPath);

	// Show export dialog
	ExportFileDialog Dialog(hInst, m_hWnd, DiscInfo[Drive].FileName, Heads, Side, &dfsCat, szExportPath);

	if (!Dialog.DoModal())
	{
		return;
	}

	const std::string& ExportPath = Dialog.GetPath();

	if (m_AutoSavePrefsFolders)
	{
		m_Preferences.SetStringValue(CFG_EXPORT_PATH, ExportPath.c_str());
	}
}

// File import
void BeebWin::ImportDiscFiles(int menuId)
{
	const int Drive = menuId == IDM_DISC_IMPORT_0 || menuId == IDM_DISC_IMPORT_2 ? 0 : 1;

	if (MachineType != Model::Master128 && NativeFDC)
	{
		// 8271 controller
		if (DiscInfo[Drive].Type == DiscType::FSD)
		{
			Report(MessageType::Warning, "Import to FSD disc not supported");
			return;
		}
	}
	else
	{
		// 1770 controller
		if (DiscInfo[Drive].Type != DiscType::SSD && DiscInfo[Drive].Type != DiscType::DSD)
		{
			// ADFS - not currently supported
			Report(MessageType::Error, "Import to ADFS disc not supported");
			return;
		}
	}

	// Check for no disk loaded
	if (DiscInfo[Drive].FileName[0] == '\0' ||
	    (DiscInfo[Drive].Type == DiscType::SSD && !DiscInfo[Drive].DoubleSidedSSD && (menuId == IDM_DISC_IMPORT_2 || menuId == IDM_DISC_IMPORT_3)))
	{
		Report(MessageType::Warning, "No disc loaded in drive %d", menuId - IDM_DISC_IMPORT_0);
		return;
	}

	// Read the catalogue
	const int Heads = DiscInfo[Drive].Type == DiscType::DSD || DiscInfo[Drive].DoubleSidedSSD ? 2 : 1;
	const int Side = menuId == IDM_DISC_IMPORT_0 || menuId == IDM_DISC_IMPORT_1 ? 0 : 1;

	DFS_DISC_CATALOGUE dfsCat;

	bool Success = dfs_get_catalogue(DiscInfo[Drive].FileName, Heads, Side, &dfsCat);

	if (!Success)
	{
		Report(MessageType::Error, "Failed to read catalogue from disc:\n  %s", DiscInfo[Drive].FileName);
		return;
	}

	// Get list of files to import
	char fileSelection[4096];
	fileSelection[0] = '\0';

	char szExportPath[MAX_PATH];
	szExportPath[0] = '\0';

	m_Preferences.GetStringValue(CFG_EXPORT_PATH, szExportPath);
	GetDataPath(m_UserDataPath, szExportPath);

	const char* filter = "INF files (*.inf)\0*.inf\0" "All files (*.*)\0*.*\0";

	FileDialog fileDialog(m_hWnd, fileSelection, sizeof(fileSelection), szExportPath, filter);
	fileDialog.AllowMultiSelect();
	fileDialog.SetTitle("Select files to import");

	if (!fileDialog.Open())
	{
		return;
	}

	// Parse the file selection string
	char *fileName = fileSelection + strlen(fileSelection) + 1;
	if (fileName[0] == 0)
	{
		// Only one file selected
		fileName = strrchr(fileSelection, '\\');
		if (fileName != NULL)
			*fileName = 0;
		fileName++;
	}
	strcpy(szExportPath, fileSelection);

	if (m_AutoSavePrefsFolders)
	{
		m_Preferences.SetStringValue(CFG_EXPORT_PATH, szExportPath);
	}

	int numFiles = 0;

	char fileNames[DFS_MAX_CAT_SIZE * 2][MAX_PATH]; // Allow user to select > cat size

	while (numFiles < DFS_MAX_CAT_SIZE*2 && fileName[0] != '\0')
	{
		// Strip .inf off
		char baseName[MAX_PATH];
		strcpy(baseName, fileName);
		int Len = (int)strlen(baseName);
		if (Len > 4 && _stricmp(baseName + Len - 4, ".inf") == 0)
			baseName[Len - 4] = 0;

		// Check for duplicate
		int i;

		for (i = 0; i < numFiles; ++i)
		{
			if (_stricmp(baseName, fileNames[i]) == 0)
				break;
		}

		if (i == numFiles)
		{
			strcpy(fileNames[numFiles], baseName);
			numFiles++;
		}

		fileName = fileName + strlen(fileName) + 1;
	}

	// Import the files
	int NumImported = 0;

	for (int i = 0; i < numFiles; ++i)
	{
		char szErrStr[500];

		Success = dfs_import_file(DiscInfo[Drive].FileName, Heads, Side, &dfsCat, fileNames[i], szExportPath, szErrStr);

		if (Success)
		{
			NumImported++;
		}
		else
		{
			Success = true;

			if (Report(MessageType::Confirm, szErrStr) == MessageResult::Cancel)
			{
				Success = false;
				break;
			}
		}
	}

	Report(MessageType::Info, "Files successfully imported: %d", NumImported);

	// Re-read disc image
	char szFileName[MAX_PATH];
	strcpy(szFileName, DiscInfo[Drive].FileName);

	if (MachineType != Model::Master128 && NativeFDC)
	{
		// 8271 controller
		FreeDiscImage(Drive);

		Load8271DiscImage(szFileName, Drive, 80, DiscInfo[Drive].Type);
	}
	else
	{
		// 1770 controller
		Close1770Disc(Drive);

		Load1770DiscImage(szFileName, Drive, DiscInfo[Drive].Type);
	}
}

void BeebWin::SelectHardDriveFolder()
{
	char DefaultPath[_MAX_PATH];
	char FileName[_MAX_PATH];
	FileName[0] = '\0';

	const char* filter =
		"Hard Drive Images (*.dat)\0*.dat\0"
		"All files (*.*)\0*.*\0";

	m_Preferences.GetStringValue(CFG_HARD_DRIVE_PATH, DefaultPath);
	GetDataPath(m_UserDataPath, DefaultPath);

	FileDialog fileDialog(m_hWnd, FileName, sizeof(FileName), DefaultPath, filter);

	if (fileDialog.Open())
	{
		unsigned int PathLength = (unsigned int)(strrchr(FileName, '\\') - FileName);
		strncpy(DefaultPath, FileName, PathLength);
		DefaultPath[PathLength] = 0;

		strcpy(HardDrivePath, DefaultPath);
		m_Preferences.SetStringValue(CFG_HARD_DRIVE_PATH, DefaultPath);

		MessageResult Result = Report(MessageType::Question,
		                              "The emulator must be reset for the change to take effect.\n\nDo you want to reset now?");

		if (Result == MessageResult::Yes)
		{
			ResetBeebSystem(MachineType, false);
		}
	}
}

/****************************************************************************/

// Bitmap capture support

void BeebWin::SetBitmapCaptureFormat(BitmapCaptureFormat Format)
{
	m_BitmapCaptureFormat = Format;

	UpdateBitmapCaptureFormatMenu();
}

void BeebWin::UpdateBitmapCaptureFormatMenu()
{
	static const struct { UINT ID; BitmapCaptureFormat Format; } MenuItems[] =
	{
		{ IDM_CAPTUREBMP,  BitmapCaptureFormat::Bmp },
		{ IDM_CAPTUREJPEG, BitmapCaptureFormat::Jpeg },
		{ IDM_CAPTUREGIF,  BitmapCaptureFormat::Gif },
		{ IDM_CAPTUREPNG,  BitmapCaptureFormat::Png }
	};

	UINT SelectedMenuItemID = 0;

	for (int i = 0; i < _countof(MenuItems); i++)
	{
		if (m_BitmapCaptureFormat == MenuItems[i].Format)
		{
			SelectedMenuItemID = MenuItems[i].ID;
			break;
		}
	}

	CheckMenuRadioItem(IDM_CAPTUREBMP, IDM_CAPTUREPNG, SelectedMenuItemID);
}

void BeebWin::SetBitmapCaptureResolution(BitmapCaptureResolution Resolution)
{
	m_BitmapCaptureResolution = Resolution;

	UpdateBitmapCaptureResolutionMenu();
}

void BeebWin::UpdateBitmapCaptureResolutionMenu()
{
	static const struct { UINT ID; BitmapCaptureResolution Resolution; } MenuItems[] =
	{
		{ IDM_CAPTURERES_DISPLAY, BitmapCaptureResolution::Display },
		{ IDM_CAPTURERES_1280,    BitmapCaptureResolution::_1280x1024 },
		{ IDM_CAPTURERES_640,     BitmapCaptureResolution::_640x512 },
		{ IDM_CAPTURERES_320,     BitmapCaptureResolution::_320x256 }
	};

	UINT SelectedMenuItemID = 0;

	for (int i = 0; i < _countof(MenuItems); i++)
	{
		if (m_BitmapCaptureResolution == MenuItems[i].Resolution)
		{
			SelectedMenuItemID = MenuItems[i].ID;
			break;
		}
	}

	CheckMenuRadioItem(IDM_CAPTURERES_DISPLAY, IDM_CAPTURERES_320, SelectedMenuItemID);
}

void BeebWin::CaptureBitmapPending(bool autoFilename)
{
	m_CaptureBitmapPending = true;
	m_CaptureBitmapAutoFilename = autoFilename;
}

bool BeebWin::GetImageEncoderClsid(const WCHAR *mimeType, CLSID *encoderClsid)
{
	bool Success = false;
	UINT num;
	UINT size;
	UINT i;

	GetImageEncodersSize(&num, &size);

	if (size == 0)
	{
		return false;
	}

	ImageCodecInfo* pImageCodecInfo = (ImageCodecInfo*)malloc(size);

	if (pImageCodecInfo == nullptr)
	{
		return false;
	}

	if (GetImageEncoders(num, size, pImageCodecInfo) != Ok)
	{
		goto Exit;
	}

	for (i = 0; i < num; ++i)
	{
		if (wcscmp(pImageCodecInfo[i].MimeType, mimeType) == 0)
		{
			*encoderClsid = pImageCodecInfo[i].Clsid;
			break;
		}
	}

	if (i == num)
	{
		Report(MessageType::Error, "Failed to get image encoder");
	}

	Success = true;

Exit:
	if (pImageCodecInfo != nullptr)
	{
		free(pImageCodecInfo);
	}

	return Success;
}

static const char* GetCaptureFormatFileExt(BitmapCaptureFormat Format)
{
	switch (Format)
	{
		case BitmapCaptureFormat::Bmp:
		default:
			return ".bmp";

		case BitmapCaptureFormat::Jpeg:
			return ".jpg";

		case BitmapCaptureFormat::Gif:
			return ".gif";

		case BitmapCaptureFormat::Png:
			return ".png";
	}
}

static const WCHAR* GetCaptureFormatMimeType(BitmapCaptureFormat Format)
{
	switch (Format)
	{
		case BitmapCaptureFormat::Bmp:
		default:
			return L"image/bmp";

		case BitmapCaptureFormat::Jpeg:
			return L"image/jpeg";

		case BitmapCaptureFormat::Gif:
			return L"image/gif";

		case BitmapCaptureFormat::Png:
			return L"image/png";
	}
}

bool BeebWin::GetImageFile(char *FileName, int Size)
{
	bool success = false;

	FileName[0] = '\0';

	char DefaultPath[_MAX_PATH];
	m_Preferences.GetStringValue(CFG_IMAGE_PATH, DefaultPath);
	GetDataPath(m_UserDataPath, DefaultPath);

	const char *fileExt = GetCaptureFormatFileExt(m_BitmapCaptureFormat);

	// A literal \0 in the format string terminates the string so use %c
	char filter[200];
	sprintf(filter, "Image File (*%s)%c*%s%c", fileExt, 0, fileExt, 0);

	FileDialog fileDialog(m_hWnd, FileName, Size, DefaultPath, filter);

	if (fileDialog.Save())
	{
		// Add extension
		if (!HasFileExt(FileName, fileExt))
		{
			strcat(FileName, fileExt);
		}

		if (m_AutoSavePrefsFolders)
		{
			unsigned int PathLength = (unsigned int)(strrchr(FileName, '\\') - FileName);
			strncpy(DefaultPath, FileName, PathLength);
			DefaultPath[PathLength] = 0;
			m_Preferences.SetStringValue(CFG_IMAGE_PATH, DefaultPath);
		}

		success = true;
	}

	return success;
}

void BeebWin::CaptureBitmap(int SourceX,
                            int SourceY,
                            int SourceWidth,
                            int SourceHeight,
                            bool Teletext)
{
	const WCHAR *mimeType = GetCaptureFormatMimeType(m_BitmapCaptureFormat);
	const char *fileExt = GetCaptureFormatFileExt(m_BitmapCaptureFormat);

	CLSID encoderClsid;

	if (!GetImageEncoderClsid(mimeType, &encoderClsid))
		return;

	// Auto generate filename?
	if (m_CaptureBitmapAutoFilename)
	{
		m_Preferences.GetStringValue(CFG_IMAGE_PATH, m_CaptureFileName);
		GetDataPath(m_UserDataPath, m_CaptureFileName);

		SYSTEMTIME systemTime;
		GetLocalTime(&systemTime);

		char AutoName[MAX_PATH];

		sprintf(AutoName, "\\BeebEm_%04d%02d%02d_%02d%02d%02d_%d%s",
		        systemTime.wYear, systemTime.wMonth,  systemTime.wDay,
		        systemTime.wHour, systemTime.wMinute, systemTime.wSecond,
		        systemTime.wMilliseconds / 100,
		        fileExt);

		strcat(m_CaptureFileName, AutoName);
	}

	int BitmapWidth, BitmapHeight;
	int DestX, DestY;
	int DestWidth, DestHeight;

	switch (m_BitmapCaptureResolution)
	{
		case BitmapCaptureResolution::Display:
			BitmapWidth  = m_XWinSize;
			BitmapHeight = m_YWinSize;
			DestWidth  = BitmapWidth;
			DestHeight = BitmapHeight;
			DestX = 0;
			DestY = 0;
			break;

		case BitmapCaptureResolution::_1280x1024:
			BitmapWidth  = 1280;
			BitmapHeight = 1024;

			if (Teletext)
			{
				DestWidth  = SourceWidth  * 2;
				DestHeight = SourceHeight * 2;
				DestX = (BitmapWidth  - DestWidth)  / 2;
				DestY = (BitmapHeight - DestHeight) / 2;
			}
			else
			{
				DestWidth  = BitmapWidth;
				DestHeight = BitmapHeight;
				DestX = 0;
				DestY = 0;
			}
			break;

		case BitmapCaptureResolution::_640x512:
		default:
			BitmapWidth  = 640;
			BitmapHeight = 512;

			if (Teletext)
			{
				DestWidth  = SourceWidth;
				DestHeight = SourceHeight;
				DestX = (BitmapWidth  - DestWidth)  / 2;
				DestY = (BitmapHeight - DestHeight) / 2;
			}
			else
			{
				DestWidth  = BitmapWidth;
				DestHeight = BitmapHeight;
				DestX = 0;
				DestY = 0;
			}
			break;

		case BitmapCaptureResolution::_320x256:
			BitmapWidth  = 320;
			BitmapHeight = 256;
			DestWidth  = BitmapWidth;
			DestHeight = BitmapHeight;
			DestX = 0;
			DestY = 0;
			break;
	}

	// Capture the bitmap
	bmiData Capturebmi = m_bmi;
	Capturebmi.bmiHeader.biWidth  = BitmapWidth;
	Capturebmi.bmiHeader.biHeight = BitmapHeight;
	Capturebmi.bmiHeader.biSizeImage = Capturebmi.bmiHeader.biWidth * Capturebmi.bmiHeader.biHeight;

	HDC CaptureDC = CreateCompatibleDC(nullptr);

	char *CaptureScreen = nullptr;

	HBITMAP CaptureDIB = CreateDIBSection(CaptureDC,
	                                      (BITMAPINFO *)&Capturebmi,
	                                      DIB_RGB_COLORS,
	                                      (void**)&CaptureScreen,
	                                      nullptr,
	                                      0);

	HGDIOBJ prevObj = SelectObject(CaptureDC, CaptureDIB);

	if (prevObj == nullptr)
	{
		Report(MessageType::Error, "Failed to initialise capture buffers");
	}
	else
	{
		StretchBlt(CaptureDC,
		           DestX, DestY,
		           DestWidth, DestHeight,
		           m_hDCBitmap,
		           SourceX, SourceY,
		           SourceWidth, SourceHeight,
		           SRCCOPY);

		SelectObject(CaptureDC, prevObj);

		// Use GDI+ Bitmap to save bitmap
		Bitmap *bitmap = new Bitmap(CaptureDIB, 0);

		WCHAR wFileName[MAX_PATH];
		mbstowcs(wFileName, m_CaptureFileName, MAX_PATH);

		Status status = bitmap->Save(wFileName, &encoderClsid, NULL);

		if (status != Ok)
		{
			Report(MessageType::Error, "Failed to save screen capture");
		}
		else if (m_CaptureBitmapAutoFilename)
		{
			// Let user know bitmap has been saved
			FlashWindow();
		}

		delete bitmap;
	}

	if (CaptureDIB != nullptr)
	{
		DeleteObject(CaptureDIB);
	}

	if (CaptureDC != nullptr)
	{
		DeleteDC(CaptureDC);
	}
}
