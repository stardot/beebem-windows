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

#include <stdio.h>
#include <algorithm>

#pragma warning(push)
#pragma warning(disable: 4091) // ignored on left of 'tagGPFIDL_FLAGS' when no variable is declared
#include <shlobj.h>
#pragma warning(pop)

#pragma warning(push)
#pragma warning(disable: 4458) // declaration of 'xxx' hides class member
using std::min;
using std::max;
#include <gdiplus.h>
#pragma warning(pop)

#include "beebwin.h"
#include "main.h"
#include "6502core.h"
#include "beebmem.h"
#include "beebemrc.h"
#include "filedialog.h"
#include "uservia.h"
#include "beebsound.h"
#include "disc8271.h"
#include "disc1770.h"
#include "disctype.h"
#include "uefstate.h"
#include "serial.h"
#include "avi.h"
#include "ext1770.h"
#include "tube.h"
#include "userkybd.h"
#include "discedit.h"
#include "version.h"

using namespace Gdiplus;

// Token written to start of map file
#define KEYMAP_TOKEN "*** BeebEm Keymap ***"

extern EDCB ExtBoard;
extern bool DiscLoaded[2]; // Set to true when a disc image has been loaded.
extern char CDiscName[2][256]; // Filename of disc current in drive 0 and 1;
extern DiscType CDiscType[2]; // Current disc types
extern char FDCDLL[256];
extern HMODULE hFDCBoard;
extern AVIWriter *aviWriter;

typedef void (*lGetBoardProperties)(struct DriveControlBlock *);
typedef unsigned char (*lSetDriveControl)(unsigned char);
typedef unsigned char (*lGetDriveControl)(unsigned char);
lGetBoardProperties PGetBoardProperties;
lSetDriveControl PSetDriveControl;
lGetDriveControl PGetDriveControl;

static bool hasFileExt(const char* fileName, const char* fileExt)
{
	const size_t fileExtLen = strlen(fileExt);
	const size_t fileNameLen = strlen(fileName);

	return fileNameLen >= fileExtLen &&
	       _stricmp(fileName + fileNameLen - fileExtLen, fileExt) == 0;
}

/****************************************************************************/
void BeebWin::SetImageName(const char *DiscName, int Drive, DiscType DscType) {
	strcpy(CDiscName[Drive],DiscName);
	CDiscType[Drive] = DscType;
	DiscLoaded[Drive] = true;

	const int maxMenuItemLen = 100;
	char menuStr[maxMenuItemLen+1];
	char *fname = strrchr(CDiscName[Drive], '\\');
	if (fname == NULL)
		fname = strrchr(CDiscName[Drive], '/');
	if (fname == NULL)
		fname = CDiscName[Drive];
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

void BeebWin::EjectDiscImage(int Drive)
{
	Eject8271DiscImage(Drive);
	Close1770Disc(Drive);

	strcpy(CDiscName[Drive], "");
	CDiscType[Drive] = DiscType::SSD;
	DiscLoaded[Drive] = false;

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
int BeebWin::ReadDisc(int Drive, bool bCheckForPrefs)
{
	char DefaultPath[_MAX_PATH];
	char FileName[256];
	int gotName = false;
	const char* filter =
		"Auto (*.ssd;*.dsd;*.ad*;*.img)\0*.ssd;*.dsd;*.adl;*.adf;*.img;*.dos\0"
		"ADFS Disc (*.adl;*.adf)\0*.adl;*.adf\0"
		"Single Sided Disc (*.ssd)\0*.ssd\0"
		"Double Sided Disc (*.dsd)\0*.dsd\0"
		"Single Sided Disc (*.*)\0*.*\0"
		"Double Sided Disc (*.*)\0*.*\0";

	m_Preferences.GetStringValue("DiscsPath", DefaultPath);
	GetDataPath(m_UserDataPath, DefaultPath);

	FileDialog fileDialog(m_hWnd, FileName, sizeof(FileName), DefaultPath, filter);
	gotName = fileDialog.Open();
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
			m_Preferences.SetStringValue("DiscsPath", DefaultPath);
		}

		bool dsd = false;
		bool adfs = false;
		bool img = false;
		bool dos = false;

		switch (fileDialog.GetFilterIndex())
		{
		case 1:
			{
				char *ext = strrchr(FileName, '.');
				if (ext != nullptr)
				{
					if (_stricmp(ext+1, "dsd") == 0)
						dsd = true;
					else if (_stricmp(ext+1, "adl") == 0)
						adfs = true;
					else if (_stricmp(ext+1, "adf") == 0)
						adfs = true;
					else if (_stricmp(ext+1, "img") == 0)
						img = true;
					else if (_stricmp(ext+1, "dos") == 0)
						dos = true;
				}
				break;
			}
		case 2:
			adfs = true;
			break;
		case 4:
		case 6:
			dsd = true;
		}

		// Another Master 128 Update, brought to you by Richard Gellman
		if (MachineType != Model::Master128)
		{
			if (dsd)
			{
				if (NativeFDC)
					LoadSimpleDSDiscImage(FileName, Drive, 80);
				else
					Load1770DiscImage(FileName, Drive, DiscType::DSD);
			}
			if (!dsd && !adfs && !dos)
			{
				if (NativeFDC)
					LoadSimpleDiscImage(FileName, Drive, 0, 80);
				else
					Load1770DiscImage(FileName, Drive, DiscType::SSD);
			}
			if (adfs)
			{
				if (NativeFDC)
					Report(MessageType::Error, "The native 8271 FDC cannot read ADFS discs");
				else
					Load1770DiscImage(FileName, Drive, DiscType::ADFS);
			}
		}
		else
		{
			// Master 128
			if (dsd)
				Load1770DiscImage(FileName, Drive, DiscType::DSD);
			if (!dsd && !adfs && !img && !dos)				 // Here we go a transposing...
				Load1770DiscImage(FileName, Drive, DiscType::SSD);
			if (adfs)
				Load1770DiscImage(FileName, Drive, DiscType::ADFS); // ADFS OO La La!
			if (img)
				Load1770DiscImage(FileName, Drive, DiscType::IMG);
			if (dos)
				Load1770DiscImage(FileName, Drive, DiscType::DOS);
		}

		/* Write protect the disc */
		if (m_WriteProtectOnLoad != m_WriteProtectDisc[Drive])
			ToggleWriteProtect(Drive);
	}

	return(gotName);
}

/****************************************************************************/

void BeebWin::Load1770DiscImage(const char *FileName, int Drive, DiscType Type)
{
	Disc1770Result Result = ::Load1770DiscImage(FileName, Drive, Type);

	if (Result == Disc1770Result::OpenedReadWrite) {
		SetImageName(FileName, Drive, Type);
		EnableMenuItem(Drive == 0 ? IDM_WPDISC0 : IDM_WPDISC1, true);
	}
	else if (Result == Disc1770Result::OpenedReadOnly) {
		SetImageName(FileName, Drive, Type);
		EnableMenuItem(Drive == 0 ? IDM_WPDISC0 : IDM_WPDISC1, false);
	}
	else {
		// Disc1770Result::Failed
		Report(MessageType::Error, "Could not open disc file:\n  %s",
		       FileName);
	}
}

/****************************************************************************/
void BeebWin::LoadTape(void)
{
	char DefaultPath[_MAX_PATH];
	char FileName[256];
	const char* filter =
		"Auto (*.uef;*.csw)\0*.uef;*.csw\0"
		"UEF Tape File (*.uef)\0*.uef\0"
		"CSW Tape File (*.csw)\0*.csw\0";

	m_Preferences.GetStringValue("TapesPath", DefaultPath);
	GetDataPath(m_UserDataPath, DefaultPath);

	FileDialog fileDialog(m_hWnd, FileName, sizeof(FileName), DefaultPath, filter);
	if (fileDialog.Open())
	{
		if (m_AutoSavePrefsFolders)
		{
			unsigned int PathLength = (unsigned int)(strrchr(FileName, '\\') - FileName);
			strncpy(DefaultPath, FileName, PathLength);
			DefaultPath[PathLength] = 0;
			m_Preferences.SetStringValue("TapesPath", DefaultPath);
		}

		if (hasFileExt(FileName, ".uef")) {
			LoadUEFTape(FileName);
		}
		else if (hasFileExt(FileName, ".csw")) {
			LoadCSWTape(FileName);
		}
	}
}

bool BeebWin::NewTapeImage(char *FileName)
{
	char DefaultPath[_MAX_PATH];
	const char* filter = "UEF Tape File (*.uef)\0*.uef\0";

	m_Preferences.GetStringValue("TapesPath", DefaultPath);
	GetDataPath(m_UserDataPath, DefaultPath);

	FileDialog fileDialog(m_hWnd, FileName, 256, DefaultPath, filter);

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
			strcpy(FDCDLL,FileName);
		}
		LoadFDC(FDCDLL, true);
	}
}

/****************************************************************************/
void BeebWin::NewDiscImage(int Drive)
{
	char DefaultPath[_MAX_PATH];
	char FileName[256];
	const char* filter =
		"Single Sided Disc (*.ssd)\0*.ssd\0"
		"Double Sided Disc (*.dsd)\0*.dsd\0"
		"Single Sided Disc (*.*)\0*.*\0"
		"Double Sided Disc (*.*)\0*.*\0"
		"ADFS M (80 Track) Disc (*.adf)\0*.adf\0"
		"ADFS L (160 Track) Disc (*.adl)\0*.adl\0";

	m_Preferences.GetStringValue("DiscsPath", DefaultPath);
	GetDataPath(m_UserDataPath, DefaultPath);

	DWORD filterIndex = 1;
	m_Preferences.GetDWORDValue("DiscsFilter", filterIndex);

	if (MachineType != Model::Master128 && NativeFDC && filterIndex >= 5)
		filterIndex = 1;

	FileDialog fileDialog(m_hWnd, FileName, sizeof(FileName), DefaultPath, filter);
	fileDialog.SetFilterIndex(filterIndex);
	if (fileDialog.Save())
	{
		filterIndex = fileDialog.GetFilterIndex();

		if (m_AutoSavePrefsFolders)
		{
			unsigned int PathLength = (unsigned int)(strrchr(FileName, '\\') - FileName);
			strncpy(DefaultPath, FileName, PathLength);
			DefaultPath[PathLength] = 0;
			m_Preferences.SetStringValue("DiscsPath", DefaultPath);
			m_Preferences.SetDWORDValue("DiscsFilter", filterIndex);
		}

		/* Add a file extension if the user did not specify one */
		if (strchr(FileName, '.') == NULL)
		{
			if (filterIndex == 1 || filterIndex == 3)
				strcat(FileName, ".ssd");
			if (filterIndex == 2 || filterIndex == 4)
				strcat(FileName, ".dsd");
			if (filterIndex==5)
				strcat(FileName, ".adf");
			if (filterIndex==6)
				strcat(FileName, ".adl");
		}

		if (filterIndex == 1 || filterIndex == 3) {
			CreateDiscImage(FileName, Drive, 1, 80);
		}
		else if (filterIndex == 2 || filterIndex == 4) {
			CreateDiscImage(FileName, Drive, 2, 80);
		}
		else if (filterIndex == 5 || filterIndex == 6) {
			const int Tracks = filterIndex == 5 ? 80 : 160;
			bool Success = CreateADFSImage(FileName, Tracks);
			if (Success) {
				Load1770DiscImage(FileName, Drive, DiscType::ADFS);
			}
		}

		/* Allow disc writes */
		if (m_WriteProtectDisc[Drive])
			ToggleWriteProtect(Drive);
		DWriteable[Drive] = true;
		DiscLoaded[Drive] = true;
		strcpy(CDiscName[1],FileName);
	}
}

/****************************************************************************/
void BeebWin::CreateDiscImage(const char *FileName, int DriveNum,
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
	for (int Sector = 0; Success && Sector < (Heads == 1 ? 2 : 12); Sector++) {
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

	if (!Success) {
		Report(MessageType::Error, "Failed writing to disc file:\n  %s",
		       FileName);
	}
	else
	{
		// Now load the new image into the correct drive
		if (Heads == 1)
		{
			if (MachineType == Model::Master128 || !NativeFDC) {
				Load1770DiscImage(FileName, DriveNum, DiscType::SSD);
			}
			else {
				LoadSimpleDiscImage(FileName, DriveNum, 0, Tracks);
			}
		}
		else
		{
			if (MachineType == Model::Master128 || !NativeFDC) {
				Load1770DiscImage(FileName, DriveNum, DiscType::DSD);
			}
			else {
				LoadSimpleDSDiscImage(FileName, DriveNum, Tracks);
			}
		}
	}
}

/****************************************************************************/
void BeebWin::SaveState()
{
	char DefaultPath[_MAX_PATH];
	char FileName[260];
	const char* filter = "UEF State File (*.uef)\0*.uef\0";

	m_Preferences.GetStringValue("StatesPath", DefaultPath);
	GetDataPath(m_UserDataPath, DefaultPath);

	FileDialog fileDialog(m_hWnd, FileName, sizeof(FileName), DefaultPath, filter);
	if (fileDialog.Save())
	{
		if (m_AutoSavePrefsFolders)
		{
			unsigned int PathLength = (unsigned int)(strrchr(FileName, '\\') - FileName);
			strncpy(DefaultPath, FileName, PathLength);
			DefaultPath[PathLength] = 0;
			m_Preferences.SetStringValue("StatesPath", DefaultPath);
		}

		// Add UEF extension if not already set and is UEF
		if (!hasFileExt(FileName, ".uef"))
		{
			strcat(FileName,".uef");
		}
		SaveUEFState(FileName);
	}
}

/****************************************************************************/
void BeebWin::RestoreState()
{
	char DefaultPath[_MAX_PATH];
	char FileName[256];
	const char* filter = "UEF State File (*.uef)\0*.uef\0";

	m_Preferences.GetStringValue("StatesPath", DefaultPath);
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
			m_Preferences.SetStringValue("StatesPath", DefaultPath);
		}

		LoadUEFState(FileName);
	}
}

/****************************************************************************/
void BeebWin::ToggleWriteProtect(int Drive)
{
	// Keep 8271 and 1770 write enable flags in sync
	if (m_WriteProtectDisc[Drive])
	{
		m_WriteProtectDisc[Drive] = false;
		DiscWriteEnable(Drive, true);
		DWriteable[Drive] = true;
	}
	else
	{
		m_WriteProtectDisc[Drive] = true;
		DiscWriteEnable(Drive, false);
		DWriteable[Drive] = false;
	}

	if (Drive == 0)
		CheckMenuItem(IDM_WPDISC0, m_WriteProtectDisc[0]);
	else
		CheckMenuItem(IDM_WPDISC1, m_WriteProtectDisc[1]);
}

void BeebWin::SetDiscWriteProtects(void)
{
	if (MachineType != Model::Master128 && NativeFDC)
	{
		m_WriteProtectDisc[0] = !IsDiscWritable(0);
		m_WriteProtectDisc[1] = !IsDiscWritable(1);

		CheckMenuItem(IDM_WPDISC0, m_WriteProtectDisc[0]);
		CheckMenuItem(IDM_WPDISC1, m_WriteProtectDisc[1]);
	}
	else
	{
		CheckMenuItem(IDM_WPDISC0, !DWriteable[0]);
		CheckMenuItem(IDM_WPDISC1, !DWriteable[1]);
	}
}

/****************************************************************************/
bool BeebWin::PrinterFile()
{
	char StartPath[_MAX_PATH];
	char FileName[_MAX_PATH];
	bool changed;
	const char* filter = "Printer Output (*.*)\0*.*\0";

	if (strlen(m_PrinterFileName) == 0)
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
		_splitpath(m_PrinterFileName, drive, dir, fname, ext);
		_makepath(StartPath, drive, dir, NULL, NULL);
		_makepath(FileName, NULL, NULL, fname, ext);
	}

	FileDialog fileDialog(m_hWnd, FileName, sizeof(FileName), StartPath, filter);
	changed = fileDialog.Save();
	if (changed)
	{
		strcpy(m_PrinterFileName, FileName);
	}

	return(changed);
}

/****************************************************************************/
void BeebWin::TogglePrinter()
{
	m_printerbufferlen = 0;

	if (PrinterEnabled)
	{
		PrinterDisable();
	}
	else
	{
		if (m_MenuIdPrinterPort == IDM_PRINTER_FILE)
		{
			if (strlen(m_PrinterFileName) == 0)
			{
				PrinterFile();
			}

			if (strlen(m_PrinterFileName) != 0)
			{
				PrinterEnable(m_PrinterFileName);
			}
		}
		else if (m_MenuIdPrinterPort == IDM_PRINTER_CLIPBOARD)
		{
			PrinterEnable(NULL);
		}
		else
		{
			PrinterEnable(m_PrinterDevice);
		}
	}

	CheckMenuItem(IDM_PRINTERONOFF, PrinterEnabled);
}

/****************************************************************************/
void BeebWin::TranslatePrinterPort()
{
	switch (m_MenuIdPrinterPort)
	{
	case IDM_PRINTER_FILE:
		strcpy(m_PrinterDevice, m_PrinterFileName);
		break;
	case IDM_PRINTER_CLIPBOARD:
		strcpy(m_PrinterDevice, "CLIPBOARD");
		break;
	default:
	case IDM_PRINTER_LPT1:
		strcpy(m_PrinterDevice, "LPT1");
		break;
	case IDM_PRINTER_LPT2:
		strcpy(m_PrinterDevice, "LPT2");
		break;
	case IDM_PRINTER_LPT3:
		strcpy(m_PrinterDevice, "LPT3");
		break;
	case IDM_PRINTER_LPT4:
		strcpy(m_PrinterDevice, "LPT4");
		break;
	}
}

/****************************************************************************/
void BeebWin::CaptureVideo()
{
	char DefaultPath[_MAX_PATH];
	char FileName[256];
	const char* filter = "AVI File (*.avi)\0*.avi\0";

	m_Preferences.GetStringValue("AVIPath", DefaultPath);
	GetDataPath(m_UserDataPath, DefaultPath);

	FileDialog fileDialog(m_hWnd, FileName, sizeof(FileName), DefaultPath, filter);
	bool changed = fileDialog.Save();
	if (changed)
	{
		// Add avi extension
		if (!hasFileExt(FileName, ".avi"))
		{
			strcat(FileName,".avi");
		}

		if (m_AutoSavePrefsFolders)
		{
			unsigned int PathLength = (unsigned int)(strrchr(FileName, '\\') - FileName);
			strncpy(DefaultPath, FileName, PathLength);
			DefaultPath[PathLength] = 0;
			m_Preferences.SetStringValue("AVIPath", DefaultPath);
		}

		// Close AVI file if currently capturing
		EndVideo();

		aviWriter = new AVIWriter();

		WAVEFORMATEX wf, *wfp;
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
		else
		{
			wfp = NULL;
		}

		// Create DIB for AVI frames
		if (m_AviDIB != NULL)
			DeleteObject(m_AviDIB);
		if (m_AviDC != NULL)
			DeleteDC(m_AviDC);
		m_Avibmi = m_bmi;
		if (m_MenuIdAviResolution == IDM_VIDEORES1)
		{
			// Must be multiple of 4
			m_Avibmi.bmiHeader.biWidth = m_XWinSize & (~3);
			m_Avibmi.bmiHeader.biHeight = m_YWinSize;
		}
		else if (m_MenuIdAviResolution == IDM_VIDEORES2)
		{
			m_Avibmi.bmiHeader.biWidth = 640;
			m_Avibmi.bmiHeader.biHeight = 512;
		}
		else
		{
			m_Avibmi.bmiHeader.biWidth = 320;
			m_Avibmi.bmiHeader.biHeight = 256;
		}
		m_Avibmi.bmiHeader.biSizeImage = m_Avibmi.bmiHeader.biWidth*m_Avibmi.bmiHeader.biHeight;
		m_AviDC = CreateCompatibleDC(NULL);
		m_AviDIB = CreateDIBSection(m_AviDC, (BITMAPINFO *)&m_Avibmi,
									DIB_RGB_COLORS, (void**)&m_AviScreen, NULL, 0);
		if (SelectObject(m_AviDC, m_AviDIB) == NULL)
		{
			Report(MessageType::Error, "Failed to initialise AVI buffers");

			delete aviWriter;
			aviWriter = NULL;
		}
		else
		{
			// Calc frame rate based on users frame skip selection
			switch (m_MenuIdAviSkip)
			{
			case IDM_VIDEOSKIP0: m_AviFrameSkip = 0; break;
			case IDM_VIDEOSKIP1: m_AviFrameSkip = 1; break;
			case IDM_VIDEOSKIP2: m_AviFrameSkip = 2; break;
			case IDM_VIDEOSKIP3: m_AviFrameSkip = 3; break;
			case IDM_VIDEOSKIP4: m_AviFrameSkip = 4; break;
			case IDM_VIDEOSKIP5: m_AviFrameSkip = 5; break;
			default: m_AviFrameSkip = 1; break;
			}
			m_AviFrameSkipCount = 0;
			m_AviFrameCount = 0;

			HRESULT hr = aviWriter->Initialise(FileName, wfp, &m_Avibmi,
				(int)(50 / (m_AviFrameSkip + 1)));
			if (FAILED(hr))
			{
				Report(MessageType::Error, "Failed to create AVI file");

				delete aviWriter;
				aviWriter = NULL;
			}
		}
	}
}

void BeebWin::EndVideo()
{
	if (aviWriter != nullptr)
	{
		delete aviWriter;
		aviWriter = nullptr;
	}
}

/****************************************************************************/
void BeebWin::QuickLoad()
{
	char FileName[_MAX_PATH];
	strcpy(FileName, m_UserDataPath);
	strcat(FileName, "beebstate\\quicksave.uef");
	LoadUEFState(FileName);
}

void BeebWin::QuickSave()
{
	char FileName1[_MAX_PATH];
	char FileName2[_MAX_PATH];
	int i;

	// Bump old quicksave files down
	for (i = 1; i <= 9; ++i)
	{
		sprintf(FileName1, "%sbeebstate\\quicksave%d.uef", m_UserDataPath, i);

		if (i == 9)
			sprintf(FileName2, "%sbeebstate\\quicksave.uef", m_UserDataPath);
		else
			sprintf(FileName2, "%sbeebstate\\quicksave%d.uef", m_UserDataPath, i+1);

		MoveFileEx(FileName2, FileName1, MOVEFILE_REPLACE_EXISTING);
	}

	SaveUEFState(FileName2);
}

void BeebWin::LoadUEFState(const char *FileName)
{
	UEFStateResult Result = ::LoadUEFState(FileName);

	switch (Result) {
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

void BeebWin::SaveUEFState(const char *FileName)
{
	UEFStateResult Result = ::SaveUEFState(FileName);

	switch (Result) {
		case UEFStateResult::Success:
			break;

		case UEFStateResult::WriteFailed:
			Report(MessageType::Error, "Failed to write state file:\n  %s",
			       FileName);
			break;
	}
}

void BeebWin::LoadUEFTape(const char *FileName)
{
	UEFResult Result = ::LoadUEFTape(FileName);

	switch (Result) {
		case UEFResult::Success:
			break;

		case UEFResult::NotUEF:
		case UEFResult::NotTape:
			Report(MessageType::Error, "The file selected is not a UEF tape image:\n  %s",
			       FileName);
			break;

		case UEFResult::NoFile:
		default:
			Report(MessageType::Error, "Cannot open UEF file:\n  %s", FileName);
			break;
	}
}

void BeebWin::LoadCSWTape(const char *FileName)
{
	CSWResult Result = ::LoadCSWTape(FileName);

	switch (Result) {
		case CSWResult::Success:
			break;

		case CSWResult::InvalidCSWFile:
			Report(MessageType::Error, "The file selected is not a CSW file:\n  %s",
			       FileName);
			break;

		case CSWResult::InvalidHeaderExtension:
			Report(MessageType::Error, "Failed to read CSW header extension:\n  %s",
			       FileName);
			break;

		case CSWResult::OpenFailed:
		default:
			Report(MessageType::Error, "Cannot open CSW file:\n  %s", FileName);
			break;
	}
}

/****************************************************************************/
// if DLLName is NULL then FDC setting is read from the registry
// else the named DLL is read in
// if save is true then DLL selection is saved in registry
void BeebWin::LoadFDC(char *DLLName, bool save) {
	char CfgName[20];

	sprintf(CfgName, "FDCDLL%d", static_cast<int>(MachineType));

	if (hFDCBoard!=NULL) FreeLibrary(hFDCBoard); 
	hFDCBoard = NULL;
	NativeFDC = true;

	if (DLLName == NULL) {
		if (!m_Preferences.GetStringValue(CfgName, FDCDLL))
			strcpy(FDCDLL,"None");
		DLLName = FDCDLL;
	}

	if (strcmp(DLLName, "None")) {
		char path[_MAX_PATH];
		strcpy(path, DLLName);
		GetDataPath(m_AppPath, path);

		hFDCBoard=LoadLibrary(path);
		if (hFDCBoard==NULL) {
			Report(MessageType::Error, "Unable to load FDD Extension Board DLL\nReverting to native 8271");
			strcpy(DLLName, "None");
		}
		else {
			PGetBoardProperties=(lGetBoardProperties) GetProcAddress(hFDCBoard,"GetBoardProperties");
			PSetDriveControl=(lSetDriveControl) GetProcAddress(hFDCBoard,"SetDriveControl");
			PGetDriveControl=(lGetDriveControl) GetProcAddress(hFDCBoard,"GetDriveControl");
			if ((PGetBoardProperties==NULL) || (PSetDriveControl==NULL) || (PGetDriveControl==NULL)) {
				Report(MessageType::Error, "Invalid FDD Extension Board DLL\nReverting to native 8271");
				strcpy(DLLName, "None");
			}
			else {
				PGetBoardProperties(&ExtBoard);
				EFDCAddr=ExtBoard.FDCAddress;
				EDCAddr=ExtBoard.DCAddress;
				NativeFDC = false; // at last, a working DLL!
				InvertTR00=ExtBoard.TR00_ActiveHigh;
			}
		} 
	}

	if (save)
		m_Preferences.SetStringValue(CfgName, DLLName);

	// Set menu options
	if (NativeFDC) {
		CheckMenuItem(ID_8271, true);
		CheckMenuItem(ID_FDC_DLL, false);
	} else {
		CheckMenuItem(ID_8271, false);
		CheckMenuItem(ID_FDC_DLL, true);
	}

	DisplayCycles=7000000;
	if (NativeFDC || MachineType == Model::Master128)
		DisplayCycles=0;
}

void BeebWin::KillDLLs(void) {
	if (hFDCBoard!=NULL)
		FreeLibrary(hFDCBoard);
	hFDCBoard=NULL;
}

void BeebWin::SetDriveControl(unsigned char value) {
	// This takes a value from the mem/io decoder, as written by the cpu, runs it through the
	// DLL's translator, then sends it on to the 1770 FDC in master 128 form.
	WriteFDCControlReg(PSetDriveControl(value));
}

unsigned char BeebWin::GetDriveControl(void) {
	// Same as above, but in reverse, i.e. reading
	unsigned char temp=ReadFDCControlReg();
	unsigned char temp2=PGetDriveControl(temp);
	return(temp2);
}

/****************************************************************************/
void BeebWin::SaveEmuUEF(FILE *SUEF) {
	char EmuName[16];
	char blank[256];
	memset(blank,0,256);

	fput16(0x046C,SUEF);
	fput32(16,SUEF);
	// BeebEm Title Block
	memset(EmuName,0,sizeof(EmuName));
	strcpy(EmuName,"BeebEm");
	EmuName[14]=VERSION_MAJOR;
	EmuName[15]=VERSION_MINOR;
	fwrite(EmuName,16,1,SUEF);

	fput16(0x046a,SUEF);
	fput32(262,SUEF);
	// Emulator Specifics
	// Note about this block: It should only be handled by beebem from uefstate.cpp if
	// the UEF has been determined to be from BeebEm (Block 046C)
	fputc(static_cast<unsigned char>(MachineType), SUEF);
	fputc(NativeFDC ? 0 : 1, SUEF);
	fputc(TubeType == Tube::Acorn65C02, SUEF); // TubeEnabled // TODO: save TubeType
	fput16(m_MenuIdKeyMapping,SUEF);
	if (m_MenuIdKeyMapping == IDM_USERKYBDMAPPING)
		fwrite(m_UserKeyMapPath,1,256,SUEF);
	else
		fwrite(blank,1,256,SUEF);
	fputc(0,SUEF);
}

void BeebWin::LoadEmuUEF(FILE *SUEF, int Version) {
	int id;
	char fileName[_MAX_PATH];

	int type = fgetc(SUEF);
	if (Version <= 8 && type == 1)
		MachineType = Model::Master128;
	else
		MachineType = static_cast<Model>(type);

	NativeFDC = fgetc(SUEF) == 0;
	int TubeEnabled = fgetc(SUEF) != 0; // TODO: get TubeType

	if (TubeEnabled)
	{
		TubeType = Tube::Acorn65C02;
	}

	if (Version >= 11)
	{
		const int UEF_KEYBOARD_MAPPING = 40060; // UEF Chunk ID

		id = fget16(SUEF);
		if (id == UEF_KEYBOARD_MAPPING)
		{
			fread(fileName,1,256,SUEF);
			GetDataPath(m_UserDataPath, fileName);
			if (ReadKeyMap(fileName, &UserKeymap))
				strcpy(m_UserKeyMapPath, fileName);
			else
				id = m_MenuIdKeyMapping;
		}
		CheckMenuItem(m_MenuIdKeyMapping, false);
		m_MenuIdKeyMapping = id;
		CheckMenuItem(m_MenuIdKeyMapping, true);
		TranslateKeyMapping();
	}

	mainWin->ResetBeebSystem(MachineType, true);
	mainWin->UpdateModelMenu();
	mainWin->UpdateTubeMenu();
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
	const char* filter = "Key Map File (*.kmap)\0*.kmap\0";

	FileDialog fileDialog(m_hWnd, FileName, sizeof(FileName), m_UserDataPath, filter);
	if (fileDialog.Open())
	{
		if (ReadKeyMap(FileName, &UserKeymap))
			strcpy(m_UserKeyMapPath, FileName);
	}
}

/****************************************************************************/
void BeebWin::SaveUserKeyMap()
{
	char FileName[_MAX_PATH];
	const char* filter = "Key Map File (*.kmap)\0*.kmap\0";

	FileDialog fileDialog(m_hWnd, FileName, sizeof(FileName), m_UserDataPath, filter);
	if (fileDialog.Save())
	{
		if (!hasFileExt(FileName, ".kmap"))
		{
			strcat(FileName,".kmap");
		}

		if (WriteKeyMap(FileName, &UserKeymap))
		{
			strcpy(m_UserKeyMapPath, FileName);
		}
	}
}

/****************************************************************************/
bool BeebWin::ReadKeyMap(const char *filename, KeyMap *keymap)
{
	bool success = true;
	char buf[256];

	FILE *infile = fopen(filename,"r");

	if (infile == NULL)
	{
		Report(MessageType::Error,
		       "Failed to read key map file:\n  %s", filename);

		success = false;
	}
	else
	{
		if (fgets(buf, 255, infile) == NULL || 
			strcmp(buf, KEYMAP_TOKEN "\n") != 0)
		{
			Report(MessageType::Error,
			       "Invalid key map file:\n  %s\n", filename);

			success = false;
		}
		else
		{
			fgets(buf, 255, infile);

			for (int i = 0; i < 256; ++i)
			{
				if (fgets(buf, 255, infile) == NULL)
				{
					Report(MessageType::Error,
					       "Data missing from key map file:\n  %s\n", filename);

					success = false;
					break;
				}

				int shift0 = 0, shift1 = 0;

				sscanf(buf, "%d %d %d %d %d %d",
				       &(*keymap)[i][0].row,
				       &(*keymap)[i][0].col,
				       &shift0,
				       &(*keymap)[i][1].row,
				       &(*keymap)[i][1].col,
				       &shift1);

				(*keymap)[i][0].shift = shift0 != 0;
				(*keymap)[i][1].shift = shift1 != 0;
			}
		}

		fclose(infile);
	}

	return success;
}

/****************************************************************************/
bool BeebWin::WriteKeyMap(const char *filename, KeyMap *keymap)
{
	FILE *outfile = fopen(filename, "w");

	if (outfile == nullptr)
	{
		Report(MessageType::Error, "Failed to write key map file:\n  %s", filename);
		return false;
	}

	fprintf(outfile, KEYMAP_TOKEN "\n\n");

	for (int i = 0; i < 256; ++i)
	{
		fprintf(outfile, "%d %d %d %d %d %d\n",
		        (*keymap)[i][0].row,
		        (*keymap)[i][0].col,
		        (*keymap)[i][0].shift,
		        (*keymap)[i][1].row,
		        (*keymap)[i][1].col,
		        (*keymap)[i][1].shift);
	}

	fclose(outfile);

	return true;
}

/****************************************************************************/
/* Clipboard support */

void BeebWin::doCopy()
{
	if (PrinterEnabled)
		TogglePrinter();

	if (IDM_PRINTER_CLIPBOARD != m_MenuIdPrinterPort)
	{
		CheckMenuItem(m_MenuIdPrinterPort, false);
		m_MenuIdPrinterPort = IDM_PRINTER_CLIPBOARD;
		CheckMenuItem(m_MenuIdPrinterPort, true);
	}
	TranslatePrinterPort();
	TogglePrinter();		// Turn printer back on

	m_printerbufferlen = 0;

	m_ClipboardBuffer[0] = 2;
	m_ClipboardBuffer[1] = 'L';
	m_ClipboardBuffer[2] = '.';
	m_ClipboardBuffer[3] = 13;
	m_ClipboardBuffer[4] = 3;
	m_ClipboardLength = 5;
	m_ClipboardIndex = 0;
	m_printerbufferlen = 0;
}

void BeebWin::doPaste()
{
	if (!IsClipboardFormatAvailable(CF_TEXT))
		return;

	if (!OpenClipboard(m_hWnd))
		return;

	HGLOBAL hglb = GetClipboardData(CF_TEXT);
	if (hglb != NULL)
	{
		LPTSTR lptstr = (LPTSTR)GlobalLock(hglb);
		if (lptstr != NULL)
		{
			strncpy(m_ClipboardBuffer, lptstr, ClipboardBufferSize - 1);
			GlobalUnlock(hglb);
			m_ClipboardIndex = 0;
			m_ClipboardLength = (int)strlen(m_ClipboardBuffer);
		}
	}

	CloseClipboard();
}

void BeebWin::ClearClipboardBuffer()
{
	mainWin->m_ClipboardBuffer[0] = '\0';
	mainWin->m_ClipboardIndex = 0;
	mainWin->m_ClipboardLength = 0;
}

void BeebWin::CopyKey(unsigned char Value)
{
	if (m_printerbufferlen >= 1024 * 1024)
		return;

	m_printerbuffer[m_printerbufferlen++] = static_cast<char>(Value);
	if (m_translateCRLF && Value == 0xD)
		m_printerbuffer[m_printerbufferlen++] = 0xA;

	if (!OpenClipboard(m_hWnd))
		return;

	EmptyClipboard();

	HGLOBAL hglbCopy = GlobalAlloc(GMEM_MOVEABLE, m_printerbufferlen + 1);
	if (hglbCopy == NULL)
	{
		CloseClipboard();
		return;
	}

	LPTSTR lptstrCopy = (LPTSTR)GlobalLock(hglbCopy);
	memcpy(lptstrCopy, m_printerbuffer, m_printerbufferlen);
	lptstrCopy[m_printerbufferlen] = 0;
	GlobalUnlock(hglbCopy);

	SetClipboardData(CF_TEXT, hglbCopy);

	CloseClipboard();
}

/****************************************************************************/
/* Disc Import / Export */

static DFS_DISC_CATALOGUE dfsCat;
static int filesSelected[DFS_MAX_CAT_SIZE];
static int numSelected;
static char szExportFolder[MAX_PATH];

// File export
int CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM /* lParam */, LPARAM /* lpData */)
{
	switch (uMsg)
	{
	case BFFM_INITIALIZED:
		if (szExportFolder[0])
		{
			SendMessage(hwnd, BFFM_SETEXPANDED, TRUE, (LPARAM)szExportFolder);
			SendMessage(hwnd, BFFM_SETSELECTION, TRUE, (LPARAM)szExportFolder);
		}
		break;
	}
	return 0;
}

INT_PTR CALLBACK DiscExportDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM /* lParam */)
{
	char str[100];
	char szDisplayName[MAX_PATH];
	int i, j;

	HWND hwndList = GetDlgItem(hwndDlg, IDC_EXPORTFILELIST);

	switch (message)
	{
	case WM_INITDIALOG:
		SendMessage(hwndList, WM_SETFONT, (WPARAM)GetStockObject(ANSI_FIXED_FONT), (LPARAM)MAKELPARAM(FALSE,0));

		for (i = 0; i < dfsCat.numFiles; ++i)
		{
			sprintf(str, "%c.%-7s %06X %06X %06X",
			        dfsCat.fileAttrs[i].directory,
			        dfsCat.fileAttrs[i].filename,
			        dfsCat.fileAttrs[i].loadAddr & 0xffffff,
			        dfsCat.fileAttrs[i].execAddr & 0xffffff,
			        dfsCat.fileAttrs[i].length);
			j = (int)SendMessage(hwndList, LB_ADDSTRING, 0, (LPARAM)str);
			// List is sorted so store catalogue index in list's item data
			SendMessage(hwndList, LB_SETITEMDATA, j, (LPARAM)i);
		}
		return TRUE;
		
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			numSelected = (int)SendMessage(hwndList,
			                               LB_GETSELITEMS,
			                               (WPARAM)DFS_MAX_CAT_SIZE,
			                               (LPARAM)filesSelected);

			if (numSelected)
			{
				// Convert list indices to catalogue indices
				for (i = 0; i < numSelected; ++i)
				{
					filesSelected[i] = (int)SendMessage(hwndList, LB_GETITEMDATA, filesSelected[i], 0);
				}

				// Get folder to export to
				BROWSEINFO bi;
				memset(&bi, 0, sizeof(bi));
				bi.hwndOwner = hwndDlg; // m_hWnd;
				bi.pszDisplayName = szDisplayName;
				bi.lpszTitle = "Select folder for exported files:";
				bi.ulFlags = BIF_EDITBOX | BIF_NEWDIALOGSTYLE;
				bi.lpfn = BrowseCallbackProc;
				LPITEMIDLIST idList = SHBrowseForFolder(&bi);
				if (idList == NULL)
				{
					wParam = IDCANCEL;
				}
				else if (!SHGetPathFromIDList(idList, szExportFolder))
				{
					mainWin->Report(MessageType::Warning, "Invalid folder selected");
					wParam = IDCANCEL;
				}

				if (idList != NULL)
					CoTaskMemFree(idList);
			}

			EndDialog(hwndDlg, wParam);
			return TRUE;

		case IDCANCEL:
			EndDialog(hwndDlg, wParam);
			return TRUE;
		}
	}

	return FALSE;
}

void BeebWin::ExportDiscFiles(int menuId)
{
	int drive;
	DiscType type;
	char szDiscFile[MAX_PATH];
	int heads;
	int side;

	if (menuId == IDM_DISC_EXPORT_0 || menuId == IDM_DISC_EXPORT_2)
		drive = 0;
	else
		drive = 1;

	if (MachineType != Model::Master128 && NativeFDC)
	{
		// 8271 controller
		Get8271DiscInfo(drive, szDiscFile, &heads);
	}
	else
	{
		// 1770 controller
		Get1770DiscInfo(drive, &type, szDiscFile);
		if (type == DiscType::SSD)
			heads = 1;
		else if (type == DiscType::DSD)
			heads = 2;
		else
		{
			// ADFS - not currently supported
			Report(MessageType::Warning, "Export from ADFS disc not supported");
			return;
		}
	}

	// Check for no disk loaded
	if (szDiscFile[0] == 0 || heads == 1 && (menuId == IDM_DISC_EXPORT_2 || menuId == IDM_DISC_EXPORT_3))
	{
		Report(MessageType::Warning, "No disc loaded in drive %d", menuId - IDM_DISC_EXPORT_0);
		return;
	}

	// Read the catalogue
	if (menuId == IDM_DISC_EXPORT_0 || menuId == IDM_DISC_EXPORT_1)
		side = 0;
	else
		side = 1;

	bool success = dfs_get_catalogue(szDiscFile, heads, side, &dfsCat);

	if (!success)
	{
		Report(MessageType::Error, "Failed to read catalogue from disc:\n  %s", szDiscFile);
		return;
	}

	// Show export dialog
	if (DialogBox(hInst, MAKEINTRESOURCE(IDD_DISCEXPORT), m_hWnd, DiscExportDlgProc) != IDOK ||
	    numSelected == 0)
	{
		return;
	}

	// Export the files
	int Count = 0;

	for (int i = 0; i < numSelected; ++i)
	{
		char szErrStr[500];

		success = dfs_export_file(szDiscFile, heads, side, &dfsCat,
		                          filesSelected[i], szExportFolder, szErrStr);
		if (success)
		{
			Count++;
		}
		else
		{
			success = true;

			if (Report(MessageType::Confirm, szErrStr) == MessageResult::Cancel)
			{
				success = false;
				break;
			}
		}
	}

	Report(MessageType::Info, "Files successfully exported: %d", Count);
}

// File import
void BeebWin::ImportDiscFiles(int menuId)
{
	int drive;
	DiscType type;
	char szDiscFile[MAX_PATH];
	int heads;
	int side;
	char szErrStr[500];
	char szFolder[MAX_PATH];
	char fileSelection[4096];
	char baseName[MAX_PATH];
	static char fileNames[DFS_MAX_CAT_SIZE*2][MAX_PATH]; // Allow user to select > cat size
	int numFiles;
	int i, n;
	const char* filter = "INF files (*.inf)\0*.inf\0" "All files (*.*)\0*.*\0";

	if (menuId == IDM_DISC_IMPORT_0 || menuId == IDM_DISC_IMPORT_2)
		drive = 0;
	else
		drive = 1;

	if (MachineType != Model::Master128 && NativeFDC)
	{
		// 8271 controller
		Get8271DiscInfo(drive, szDiscFile, &heads);
	}
	else
	{
		// 1770 controller
		Get1770DiscInfo(drive, &type, szDiscFile);
		if (type == DiscType::SSD)
			heads = 1;
		else if (type == DiscType::DSD)
			heads = 2;
		else
		{
			// ADFS - not currently supported
			Report(MessageType::Error, "Import to ADFS disc not supported");
			return;
		}
	}

	// Check for no disk loaded
	if (szDiscFile[0] == 0 || heads == 1 && (menuId == IDM_DISC_IMPORT_2 || menuId == IDM_DISC_IMPORT_3))
	{
		Report(MessageType::Warning, "No disc loaded in drive %d", menuId - IDM_DISC_IMPORT_0);
		return;
	}

	// Read the catalogue
	if (menuId == IDM_DISC_IMPORT_0 || menuId == IDM_DISC_IMPORT_1)
		side = 0;
	else
		side = 1;

	bool success = dfs_get_catalogue(szDiscFile, heads, side, &dfsCat);

	if (!success)
	{
		Report(MessageType::Error, "Failed to read catalogue from disc:\n  %s", szDiscFile);
		return;
	}

	// Get list of files to import
	memset(fileSelection, 0, sizeof(fileSelection));
	szFolder[0] = 0;
	GetDataPath(m_UserDataPath, szFolder);

	FileDialog fileDialog(m_hWnd, fileSelection, sizeof(fileSelection), szFolder, filter);
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
	strcpy(szFolder, fileSelection);

	numFiles = 0;
	while (numFiles < DFS_MAX_CAT_SIZE*2 && fileName[0] != 0)
	{
		// Strip .INF off
		strcpy(baseName, fileName);
		i = (int)strlen(baseName);
		if (i > 4 && _stricmp(baseName + i - 4, ".inf") == 0)
			baseName[i - 4] = 0;

		// Check for duplicate
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
	n = 0;
	for (i = 0; i < numFiles; ++i)
	{
		success = dfs_import_file(szDiscFile, heads, side, &dfsCat, fileNames[i], szFolder, szErrStr);
		if (success)
		{
			n++;
		}
		else
		{
			success = true;

			if (Report(MessageType::Confirm, szErrStr) == MessageResult::Cancel)
			{
				success = false;
				break;
			}
		}
	}

	Report(MessageType::Info, "Files successfully imported: %d", n);

	// Re-read disc image
	if (MachineType != Model::Master128 && NativeFDC)
	{
		// 8271 controller
		Eject8271DiscImage(drive);
		if (heads == 2)
			LoadSimpleDSDiscImage(szDiscFile, drive, 80);
		else
			LoadSimpleDiscImage(szDiscFile, drive, 0, 80);
	}
	else
	{
		// 1770 controller
		Close1770Disc(drive);
		if (heads == 2)
			Load1770DiscImage(szDiscFile, drive, DiscType::DSD);
		else
			Load1770DiscImage(szDiscFile, drive, DiscType::SSD);
	}
}

void BeebWin::SelectHardDriveFolder()
{
	char DefaultPath[_MAX_PATH];
	char FileName[_MAX_PATH];
	int gotName = false;
	const char* filter =
		"Hard Drive Images (*.dat)\0*.dat\0"
		"All files (*.*)\0*.*\0";

	m_Preferences.GetStringValue("HardDrivePath", DefaultPath);
	GetDataPath(m_UserDataPath, DefaultPath);

	FileDialog fileDialog(m_hWnd, FileName, sizeof(FileName), DefaultPath, filter);
	gotName = fileDialog.Open();
	if (gotName)
	{
		unsigned int PathLength = (unsigned int)(strrchr(FileName, '\\') - FileName);
		strncpy(DefaultPath, FileName, PathLength);
		DefaultPath[PathLength] = 0;

		strcpy(HardDrivePath, DefaultPath);
		m_Preferences.SetStringValue("HardDrivePath", DefaultPath);

		MessageResult Result = Report(MessageType::Question,
		                              "The emulator must be reset for the change to take effect.\n\nDo you want to reset now?");

		if (Result == MessageResult::Yes)
		{
			ResetBeebSystem(MachineType, false);
		}
	}
}

/****************************************************************************/
/* Bitmap capture support */
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

static const char* GetCaptureFormatFileExt(UINT MenuID)
{
	switch (MenuID)
	{
		case IDM_CAPTUREBMP:
		default:
			return ".bmp";

		case IDM_CAPTUREJPEG:
			return ".jpg";

		case IDM_CAPTUREGIF:
			return ".gif";

		case IDM_CAPTUREPNG:
			return ".png";
	}
}

static const WCHAR* GetCaptureFormatMimeType(UINT MenuID)
{
	switch (MenuID)
	{
		case IDM_CAPTUREBMP:
		default:
			return L"image/bmp";

		case IDM_CAPTUREJPEG:
			return L"image/jpeg";

		case IDM_CAPTUREGIF:
			return L"image/gif";

		case IDM_CAPTUREPNG:
			return L"image/png";
	}
}

bool BeebWin::GetImageFile(char *FileName)
{
	char DefaultPath[_MAX_PATH];
	bool success = false;
	char filter[200];

	m_Preferences.GetStringValue("ImagePath", DefaultPath);
	GetDataPath(m_UserDataPath, DefaultPath);

	const char *fileExt = GetCaptureFormatFileExt(m_MenuIdCaptureFormat);

	// A literal \0 in the format string terminates the string so use %c
	sprintf(filter, "Image File (*%s)%c*%s%c", fileExt, 0, fileExt, 0);

	FileDialog fileDialog(m_hWnd, FileName, MAX_PATH, DefaultPath, filter);
	if (fileDialog.Save())
	{
		// Add extension
		if (!hasFileExt(FileName, fileExt))
		{
			strcat(FileName, fileExt);
		}

		if (m_AutoSavePrefsFolders)
		{
			unsigned int PathLength = (unsigned int)(strrchr(FileName, '\\') - FileName);
			strncpy(DefaultPath, FileName, PathLength);
			DefaultPath[PathLength] = 0;
			m_Preferences.SetStringValue("ImagePath", DefaultPath);
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
	const WCHAR *mimeType = GetCaptureFormatMimeType(m_MenuIdCaptureFormat);
	const char *fileExt = GetCaptureFormatFileExt(m_MenuIdCaptureFormat);

	CLSID encoderClsid;

	if (!GetImageEncoderClsid(mimeType, &encoderClsid))
		return;

	// Auto generate filename?
	if (m_CaptureBitmapAutoFilename)
	{
		m_Preferences.GetStringValue("ImagePath", m_CaptureFileName);
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

	switch (m_MenuIdCaptureResolution)
	{
		case IDM_CAPTURERES_DISPLAY:
			BitmapWidth  = m_XWinSize;
			BitmapHeight = m_YWinSize;
			DestWidth  = BitmapWidth;
			DestHeight = BitmapHeight;
			DestX = 0;
			DestY = 0;
			break;

		case IDM_CAPTURERES_1280:
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

		case IDM_CAPTURERES_640:
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

		case IDM_CAPTURERES_320:
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
		Bitmap *bitmap = new Bitmap((HBITMAP)CaptureDIB, 0);

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
			FlashWindow(m_hWnd, TRUE);
			MessageBeep(MB_ICONEXCLAMATION);
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
