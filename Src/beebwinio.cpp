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
#include <windows.h>
#include <initguid.h>
#include <shlobj.h>
#include <gdiplus.h>
#include "main.h"
#include "6502core.h"
#include "beebwin.h"
#include "beebmem.h"
#include "beebemrc.h"
#include "uservia.h"
#include "beebsound.h"
#include "disc8271.h"
#include "disc1770.h"
#include "uefstate.h"
#include "serial.h"
#include "csw.h"
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
extern bool DiscLoaded[2]; // Set to TRUE when a disc image has been loaded.
extern char CDiscName[2][256]; // Filename of disc current in drive 0 and 1;
extern char CDiscType[2]; // Current disc types
extern char FDCDLL[256];
extern HMODULE hFDCBoard;
extern AVIWriter *aviWriter;
extern HWND hCurrentDialog;

typedef void (*lGetBoardProperties)(struct DriveControlBlock *);
typedef unsigned char (*lSetDriveControl)(unsigned char);
typedef unsigned char (*lGetDriveControl)(unsigned char);
lGetBoardProperties PGetBoardProperties;
lSetDriveControl PSetDriveControl;
lGetDriveControl PGetDriveControl;

/****************************************************************************/
void BeebWin::SetImageName(char *DiscName,char Drive,char DType) {
	strcpy(CDiscName[Drive],DiscName);
	CDiscType[Drive]=DType;
	DiscLoaded[Drive]=TRUE;

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
	CDiscType[Drive] = 0;
	DiscLoaded[Drive] = FALSE;

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
int BeebWin::ReadDisc(int Drive,HMENU dmenu, bool bCheckForPrefs)
{
	char DefaultPath[_MAX_PATH];
	char FileName[256];
	OPENFILENAME ofn;
	int gotName = false;

	PrefsGetStringValue("DiscsPath",DefaultPath);
	GetDataPath(m_UserDataPath, DefaultPath);

	ofn.nFilterIndex = 1;
	FileName[0] = '\0';

	/* Hmm, what do I put in all these fields! */
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = m_hWnd;
	ofn.hInstance = NULL;
	ofn.lpstrFilter = "Auto (*.ssd;*.dsd;*.ad*;*.img)\0*.ssd;*.dsd;*.adl;*.adf;*.img;*.dos\0"
                    "ADFS Disc (*.adl *.adf)\0*.adl;*.adf\0"
                    "Single Sided Disc (*.ssd)\0*.ssd\0"
                    "Double Sided Disc (*.dsd)\0*.dsd\0"
                    "Single Sided Disc (*.*)\0*.*\0"
                    "Double Sided Disc (*.*)\0*.*\0";
	ofn.lpstrCustomFilter = NULL;
	ofn.nMaxCustFilter = 0;
	ofn.lpstrFile = FileName;
	ofn.nMaxFile = sizeof(FileName);
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = DefaultPath;
	ofn.lpstrTitle = NULL;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
	ofn.nFileOffset = 0;
	ofn.nFileExtension = 0;
	ofn.lpstrDefExt = NULL;
	ofn.lCustData = 0;
	ofn.lpfnHook = NULL;
	ofn.lpTemplateName = NULL;

	gotName = GetOpenFileName(&ofn);
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
			PrefsSetStringValue("DiscsPath", DefaultPath);
		}

		bool dsd = false;
		bool adfs = false;
		bool img = false;
		bool dos = false;
		switch (ofn.nFilterIndex)
		{
		case 1:
			{
			char *ext = strrchr(FileName, '.');
			if (ext != NULL)
			  if (_stricmp(ext+1, "dsd") == 0)
				dsd = true;
			  if (_stricmp(ext+1, "adl") == 0)
				adfs = true;
			  if (_stricmp(ext+1, "adf") == 0)
				adfs = true;
			  if (_stricmp(ext+1, "img") == 0)
				img = true;
			  if (_stricmp(ext+1, "dos") == 0)
				dos = true;
			break;
			}
		case 2:
			adfs=true;
			break;
		case 4:
		case 6:
			dsd = true;
		}

		// Another Master 128 Update, brought to you by Richard Gellman
		if (MachineType!=3)
		{
			if (dsd)
			{
				if (NativeFDC)
					LoadSimpleDSDiscImage(FileName, Drive, 80);
				else
					Load1770DiscImage(FileName,Drive,1,dmenu); // 1 = dsd
			}
			if ((!dsd) && (!adfs) && (!dos))
			{
				if (NativeFDC)
					LoadSimpleDiscImage(FileName, Drive, 0, 80);
				else
					Load1770DiscImage(FileName,Drive,0,dmenu); // 0 = ssd
			}
			if (adfs)
			{
				if (NativeFDC)
					MessageBox(GETHWND,"The native 8271 FDC cannot read ADFS discs\n","BeebEm",
							   MB_OK|MB_ICONERROR);
				else
					Load1770DiscImage(FileName,Drive,2,dmenu); // 2 = adfs
			}
		}

		if (MachineType==3)
		{
			if (dsd)
				Load1770DiscImage(FileName,Drive,1,dmenu); // 0 = ssd
			if (!dsd && !adfs && !img && !dos)				 // Here we go a transposing...
				Load1770DiscImage(FileName,Drive,0,dmenu); // 1 = dsd
			if (adfs)
				Load1770DiscImage(FileName,Drive,2,dmenu); // ADFS OO La La!
			if (img)
				Load1770DiscImage(FileName,Drive,3,dmenu);
			if (dos)
				Load1770DiscImage(FileName,Drive,4,dmenu);
		}

		/* Write protect the disc */
		if (m_WriteProtectOnLoad != m_WriteProtectDisc[Drive])
			ToggleWriteProtect(Drive);
	}

	return(gotName);
}

/****************************************************************************/
void BeebWin::LoadTape(void)
{
	char DefaultPath[_MAX_PATH];
	char FileName[256];
	OPENFILENAME ofn;

	PrefsGetStringValue("TapesPath",DefaultPath);
	GetDataPath(m_UserDataPath, DefaultPath);

	ofn.nFilterIndex = 1;
	FileName[0] = '\0';

	/* Hmm, what do I put in all these fields! */
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = m_hWnd;
	ofn.hInstance = NULL;
	ofn.lpstrFilter = "Auto (*.uef;*.csw)\0*.uef;*.csw\0"
					  "UEF Tape File (*.uef)\0*.uef\0"
					  "CSW Tape File (*.csw)\0*.csw\0";
	ofn.lpstrCustomFilter = NULL;
	ofn.nMaxCustFilter = 0;
	ofn.lpstrFile = FileName;
	ofn.nMaxFile = sizeof(FileName);
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = DefaultPath;
	ofn.lpstrTitle = NULL;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
	ofn.nFileOffset = 0;
	ofn.nFileExtension = 0;
	ofn.lpstrDefExt = NULL;
	ofn.lCustData = 0;
	ofn.lpfnHook = NULL;
	ofn.lpTemplateName = NULL;

	if (GetOpenFileName(&ofn))
	{
		if (m_AutoSavePrefsFolders)
		{
			unsigned int PathLength = (unsigned int)(strrchr(FileName, '\\') - FileName);
			strncpy(DefaultPath, FileName, PathLength);
			DefaultPath[PathLength] = 0;
			PrefsSetStringValue("TapesPath", DefaultPath);
		}

		if (strstr(FileName, ".uef")) LoadUEF(FileName);
		if (strstr(FileName, ".csw")) LoadCSW(FileName);
	}
}

void BeebWin::NewTapeImage(char *FileName)
{
	char DefaultPath[_MAX_PATH];
	OPENFILENAME ofn;

	PrefsGetStringValue("TapesPath",DefaultPath);
	GetDataPath(m_UserDataPath, DefaultPath);

	ofn.nFilterIndex = 1;
	FileName[0] = '\0';

	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = m_hWnd;
	ofn.hInstance = NULL;
	ofn.lpstrFilter = "UEF Tape File (*.uef)\0*.uef\0";
	ofn.lpstrCustomFilter = NULL;
	ofn.nMaxCustFilter = 0;
	ofn.lpstrFile = FileName;
	ofn.nMaxFile = 256;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = DefaultPath;
	ofn.lpstrTitle = NULL;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
	ofn.nFileOffset = 0;
	ofn.nFileExtension = 0;
	ofn.lpstrDefExt = NULL;
	ofn.lCustData = 0;
	ofn.lpfnHook = NULL;
	ofn.lpTemplateName = NULL;

	if (GetSaveFileName(&ofn))
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
}

/*******************************************************************/

void BeebWin::SelectFDC(void)
{
	char DefaultPath[_MAX_PATH];
	char FileName[256];
	OPENFILENAME ofn;

	strcpy(DefaultPath, m_AppPath);
	strcat(DefaultPath, "Hardware");

	ofn.nFilterIndex = 1;
	FileName[0] = '\0';

	/* Hmm, what do I put in all these fields! */
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = m_hWnd;
	ofn.hInstance = NULL;
	ofn.lpstrFilter = "FDC Extension Board Plugin DLL (*.dll)\0*.dll\0";
	ofn.lpstrCustomFilter = NULL;
	ofn.nMaxCustFilter = 0;
	ofn.lpstrFile = FileName;
	ofn.nMaxFile = sizeof(FileName);
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = DefaultPath;
	ofn.lpstrTitle = NULL;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
	ofn.nFileOffset = 0;
	ofn.nFileExtension = 0;
	ofn.lpstrDefExt = NULL;
	ofn.lCustData = 0;
	ofn.lpfnHook = NULL;
	ofn.lpTemplateName = NULL;

	if (GetOpenFileName(&ofn))
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
	OPENFILENAME ofn;

	PrefsGetStringValue("DiscsPath",DefaultPath);
	GetDataPath(m_UserDataPath, DefaultPath);

	ofn.nFilterIndex = 1;
	PrefsGetDWORDValue("DiscsFilter",ofn.nFilterIndex);

	if (MachineType!=3 && NativeFDC && ofn.nFilterIndex >= 5)
		ofn.nFilterIndex = 1;

	FileName[0] = '\0';

	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = m_hWnd;
	ofn.hInstance = NULL;
	ofn.lpstrFilter = "Single Sided Disc (*.ssd)\0*.ssd\0"
					"Double Sided Disc (*.dsd)\0*.dsd\0"
					"Single Sided Disc\0*.*\0"
					"Double Sided Disc\0*.*\0"
					"ADFS M ( 80 Track) Disc\0*.adf\0"
					"ADFS L (160 Track) Disc\0*.adl\0";
	ofn.lpstrCustomFilter = NULL;
	ofn.nMaxCustFilter = 0;
	ofn.lpstrFile = FileName;
	ofn.nMaxFile = sizeof(FileName);
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = DefaultPath;
	ofn.lpstrTitle = NULL;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
	ofn.nFileOffset = 0;
	ofn.nFileExtension = 0;
	ofn.lpstrDefExt = NULL;
	ofn.lCustData = 0;
	ofn.lpfnHook = NULL;
	ofn.lpTemplateName = NULL;

	if (GetSaveFileName(&ofn))
	{
		if (m_AutoSavePrefsFolders)
		{
			unsigned int PathLength = (unsigned int)(strrchr(FileName, '\\') - FileName);
			strncpy(DefaultPath, FileName, PathLength);
			DefaultPath[PathLength] = 0;
			PrefsSetStringValue("DiscsPath", DefaultPath);
			PrefsSetDWORDValue("DiscsFilter",ofn.nFilterIndex);
		}

		/* Add a file extension if the user did not specify one */
		if (strchr(FileName, '.') == NULL)
		{
			if (ofn.nFilterIndex == 1 || ofn.nFilterIndex == 3)
				strcat(FileName, ".ssd");
			if (ofn.nFilterIndex == 2 || ofn.nFilterIndex == 4)
				strcat(FileName, ".dsd");
			if (ofn.nFilterIndex==5)
				strcat(FileName, ".adf");
			if (ofn.nFilterIndex==6)
				strcat(FileName, ".adl");
		}

		if (ofn.nFilterIndex == 1 || ofn.nFilterIndex == 3)
		{
			CreateDiscImage(FileName, Drive, 1, 80);
		}
		if (ofn.nFilterIndex == 2 || ofn.nFilterIndex == 4)
		{
			CreateDiscImage(FileName, Drive, 2, 80);
		}
		if (ofn.nFilterIndex == 5) CreateADFSImage(FileName,Drive,80,m_hMenu);
		if (ofn.nFilterIndex == 6) CreateADFSImage(FileName,Drive,160,m_hMenu);

		/* Allow disc writes */
		if (m_WriteProtectDisc[Drive])
			ToggleWriteProtect(Drive);
		DWriteable[Drive]=1;
		DiscLoaded[Drive]=TRUE;
		strcpy(CDiscName[1],FileName);
	}
}

/****************************************************************************/
void BeebWin::SaveState()
{
	char DefaultPath[_MAX_PATH];
	char FileName[260];
	OPENFILENAME ofn;

	PrefsGetStringValue("StatesPath",DefaultPath);
	GetDataPath(m_UserDataPath, DefaultPath);

	FileName[0] = '\0';

	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = m_hWnd;
	ofn.hInstance = NULL;
	ofn.lpstrFilter = "UEF State File\0*.uef\0";
	ofn.lpstrCustomFilter = NULL;
	ofn.nMaxCustFilter = 0;
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = FileName;
	ofn.nMaxFile = sizeof(FileName);
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = DefaultPath;
	ofn.lpstrTitle = NULL;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
	ofn.nFileOffset = 0;
	ofn.nFileExtension = 0;
	ofn.lpstrDefExt = NULL;
	ofn.lCustData = 0;
	ofn.lpfnHook = NULL;
	ofn.lpTemplateName = NULL;

	if (GetSaveFileName(&ofn))
	{
		if (m_AutoSavePrefsFolders)
		{
			unsigned int PathLength = (unsigned int)(strrchr(FileName, '\\') - FileName);
			strncpy(DefaultPath, FileName, PathLength);
			DefaultPath[PathLength] = 0;
			PrefsSetStringValue("StatesPath", DefaultPath);
		}

		// Add UEF extension if not already set and is UEF
		if ((strcmp(FileName+(strlen(FileName)-4),".UEF")!=0) &&
			(strcmp(FileName+(strlen(FileName)-4),".uef")!=0))
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
	OPENFILENAME ofn;

	PrefsGetStringValue("StatesPath",DefaultPath);
	GetDataPath(m_UserDataPath, DefaultPath);
  
	FileName[0] = '\0';

	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = m_hWnd;
	ofn.hInstance = NULL;
	ofn.lpstrFilter = "UEF State File\0*.uef\0";
	ofn.lpstrCustomFilter = NULL;
	ofn.nMaxCustFilter = 0;
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = FileName;
	ofn.nMaxFile = sizeof(FileName);
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = DefaultPath;
	ofn.lpstrTitle = NULL;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
	ofn.nFileOffset = 0;
	ofn.nFileExtension = 0;
	ofn.lpstrDefExt = NULL;
	ofn.lCustData = 0;
	ofn.lpfnHook = NULL;
	ofn.lpTemplateName = NULL;

	if (GetOpenFileName(&ofn))
	{
		// Check for file specific preferences files
		CheckForLocalPrefs(FileName, true);

		if (m_AutoSavePrefsFolders)
		{
			unsigned int PathLength = (unsigned int)(strrchr(FileName, '\\') - FileName);
			strncpy(DefaultPath, FileName, PathLength);
			DefaultPath[PathLength] = 0;
			PrefsSetStringValue("StatesPath", DefaultPath);
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
		m_WriteProtectDisc[Drive] = 0;
		DiscWriteEnable(Drive, 1);
		DWriteable[Drive]=1;
	}
	else
	{
		m_WriteProtectDisc[Drive] = 1;
		DiscWriteEnable(Drive, 0);
		DWriteable[Drive]=0;
	}

	if (Drive == 0)
		CheckMenuItem(m_hMenu, IDM_WPDISC0, m_WriteProtectDisc[0] ? MF_CHECKED : MF_UNCHECKED);
	else
		CheckMenuItem(m_hMenu, IDM_WPDISC1, m_WriteProtectDisc[1] ? MF_CHECKED : MF_UNCHECKED);
}

void BeebWin::SetDiscWriteProtects(void)
{
	if (MachineType!=3 && NativeFDC)
	{
		m_WriteProtectDisc[0] = !IsDiscWritable(0);
		m_WriteProtectDisc[1] = !IsDiscWritable(1);
		CheckMenuItem(m_hMenu, IDM_WPDISC0, m_WriteProtectDisc[0] ? MF_CHECKED : MF_UNCHECKED);
		CheckMenuItem(m_hMenu, IDM_WPDISC1, m_WriteProtectDisc[1] ? MF_CHECKED : MF_UNCHECKED);
	}
	else
	{
		CheckMenuItem(m_hMenu, IDM_WPDISC0, DWriteable[0] ? MF_UNCHECKED : MF_CHECKED);
		CheckMenuItem(m_hMenu, IDM_WPDISC1, DWriteable[1] ? MF_UNCHECKED : MF_CHECKED);
	}
}

/****************************************************************************/
BOOL BeebWin::PrinterFile()
{
	char StartPath[_MAX_PATH];
	char FileName[_MAX_PATH];
	OPENFILENAME ofn;
	BOOL changed;

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

	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = m_hWnd;
	ofn.hInstance = NULL;
	ofn.lpstrFilter = "Printer Output\0*\0";
	ofn.lpstrCustomFilter = NULL;
	ofn.nMaxCustFilter = 0;
	ofn.nFilterIndex = 0;
	ofn.lpstrFile = FileName;
	ofn.nMaxFile = sizeof(FileName);
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = StartPath;
	ofn.lpstrTitle = NULL;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
	ofn.nFileOffset = 0;
	ofn.nFileExtension = 0;
	ofn.lpstrDefExt = NULL;
	ofn.lCustData = 0;
	ofn.lpfnHook = NULL;
	ofn.lpTemplateName = NULL;

	changed = GetSaveFileName(&ofn);
	if (changed)
	{
		strcpy(m_PrinterFileName, FileName);
	}

	return(changed);
}

/****************************************************************************/
void BeebWin::TogglePrinter()
{
	BOOL FileOK = TRUE;
	HMENU hMenu = m_hMenu;

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
				PrinterFile();
			if (strlen(m_PrinterFileName) != 0)
			{
				/* First check if file already exists */
				FILE *outfile;
				outfile=fopen(m_PrinterFileName,"rb");
				if (outfile != NULL)
				{
					fclose(outfile);
					char errstr[200];
					sprintf(errstr, "File already exists:\n  %s\n\nOverwrite file?", m_PrinterFileName);
					if (MessageBox(m_hWnd,errstr,WindowTitle,MB_YESNO|MB_ICONQUESTION) != IDYES)
						FileOK = FALSE;
				}
				if (FileOK == TRUE)
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

	CheckMenuItem(hMenu, IDM_PRINTERONOFF, PrinterEnabled ? MF_CHECKED : MF_UNCHECKED);
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
	OPENFILENAME ofn;
	BOOL changed;

	PrefsGetStringValue("AVIPath",DefaultPath);
	GetDataPath(m_UserDataPath, DefaultPath);

	FileName[0] = '\0';

	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = m_hWnd;
	ofn.hInstance = NULL;
	ofn.lpstrFilter = "AVI File (*.avi)\0*.avi\0";
	ofn.lpstrCustomFilter = NULL;
	ofn.nMaxCustFilter = 0;
	ofn.nFilterIndex = 0;
	ofn.lpstrFile = FileName;
	ofn.nMaxFile = sizeof(FileName);
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = DefaultPath;
	ofn.lpstrTitle = NULL;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
	ofn.nFileOffset = 0;
	ofn.nFileExtension = 0;
	ofn.lpstrDefExt = NULL;
	ofn.lCustData = 0;
	ofn.lpfnHook = NULL;
	ofn.lpTemplateName = NULL;

	changed = GetSaveFileName(&ofn);
	if (changed)
	{
		// Add avi extension
		if (strlen(FileName) < 5 ||
			((strcmp(FileName + (strlen(FileName)-4), ".AVI")!=0) &&
			 (strcmp(FileName + (strlen(FileName)-4), ".avi")!=0)) )
		{
			strcat(FileName,".avi");
		}

		if (m_AutoSavePrefsFolders)
		{
			unsigned int PathLength = (unsigned int)(strrchr(FileName, '\\') - FileName);
			strncpy(DefaultPath, FileName, PathLength);
			DefaultPath[PathLength] = 0;
			PrefsSetStringValue("AVIPath", DefaultPath);
		}

		// Close AVI file if currently capturing
		if (aviWriter != NULL)
		{
			delete aviWriter;
			aviWriter = NULL;
		}

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
			m_Avibmi.bmiHeader.biWidth = m_XWinSize;
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
			MessageBox(m_hWnd, "Failed to initialise AVI buffers",
				WindowTitle, MB_OK|MB_ICONERROR);
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
				(int)(50 / (m_AviFrameSkip+1)), m_hWnd);
			if (FAILED(hr))
			{
				MessageBox(m_hWnd, "Failed to create AVI file",
					WindowTitle, MB_OK|MB_ICONERROR);
				delete aviWriter;
				aviWriter = NULL;
			}
		}
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

/****************************************************************************/
// if DLLName is NULL then FDC setting is read from the registry
// else the named DLL is read in
// if save is true then DLL selection is saved in registry
void BeebWin::LoadFDC(char *DLLName, bool save) {
	char CfgName[20];

	sprintf(CfgName, "FDCDLL%d", MachineType);

	if (hFDCBoard!=NULL) FreeLibrary(hFDCBoard); 
	hFDCBoard=NULL; NativeFDC=TRUE;

	if (DLLName == NULL) {
		if (!PrefsGetStringValue(CfgName,FDCDLL))
			strcpy(FDCDLL,"None");
		DLLName = FDCDLL;
	}

	if (strcmp(DLLName, "None")) {
		char path[_MAX_PATH];
		strcpy(path, DLLName);
		GetDataPath(m_AppPath, path);

		hFDCBoard=LoadLibrary(path);
		if (hFDCBoard==NULL) {
			MessageBox(GETHWND,"Unable to load FDD Extension Board DLL\nReverting to native 8271\n",WindowTitle,MB_OK|MB_ICONERROR); 
			strcpy(DLLName, "None");
		}
		else {
			PGetBoardProperties=(lGetBoardProperties) GetProcAddress(hFDCBoard,"GetBoardProperties");
			PSetDriveControl=(lSetDriveControl) GetProcAddress(hFDCBoard,"SetDriveControl");
			PGetDriveControl=(lGetDriveControl) GetProcAddress(hFDCBoard,"GetDriveControl");
			if ((PGetBoardProperties==NULL) || (PSetDriveControl==NULL) || (PGetDriveControl==NULL)) {
				MessageBox(GETHWND,"Invalid FDD Extension Board DLL\nReverting to native 8271\n",WindowTitle,MB_OK|MB_ICONERROR);
				strcpy(DLLName, "None");
			}
			else {
				PGetBoardProperties(&ExtBoard);
				EFDCAddr=ExtBoard.FDCAddress;
				EDCAddr=ExtBoard.DCAddress;
				NativeFDC=FALSE; // at last, a working DLL!
				InvertTR00=ExtBoard.TR00_ActiveHigh;
			}
		} 
	}

	if (save)
		PrefsSetStringValue(CfgName,DLLName);

	// Set menu options
	if (NativeFDC) {
		CheckMenuItem(m_hMenu,ID_8271,MF_CHECKED);
		CheckMenuItem(m_hMenu,ID_FDC_DLL,MF_UNCHECKED);
	} else {
		CheckMenuItem(m_hMenu,ID_8271,MF_UNCHECKED);
		CheckMenuItem(m_hMenu,ID_FDC_DLL,MF_CHECKED);
	}

	DisplayCycles=7000000;
	if ((NativeFDC) || (MachineType==3))
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
	unsigned char temp,temp2;
	temp=ReadFDCControlReg();
	temp2=PGetDriveControl(temp);
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
	fputc(MachineType,SUEF);
	fputc((NativeFDC)?0:1,SUEF);
	fputc(TubeEnabled,SUEF);
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

	MachineType=fgetc(SUEF);
	if (Version <= 8 && MachineType == 1)
		MachineType = 3;
	NativeFDC=(fgetc(SUEF)==0)?TRUE:FALSE;
	TubeEnabled=fgetc(SUEF);

	if (Version >= 11)
	{
		id = fget16(SUEF);
		if (id == IDM_USERKYBDMAPPING)
		{
			fread(fileName,1,256,SUEF);
			GetDataPath(m_UserDataPath, fileName);
			if (ReadKeyMap(fileName, &UserKeymap))
				strcpy(m_UserKeyMapPath, fileName);
			else
				id = m_MenuIdKeyMapping;
		}
		CheckMenuItem(m_hMenu, m_MenuIdKeyMapping, MF_UNCHECKED);
		m_MenuIdKeyMapping = id;
		CheckMenuItem(m_hMenu, m_MenuIdKeyMapping, MF_CHECKED);
		TranslateKeyMapping();
	}

	mainWin->ResetBeebSystem(MachineType,TubeEnabled,1);
	mainWin->UpdateModelType();
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
	OPENFILENAME ofn;

	FileName[0] = '\0';

	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = m_hWnd;
	ofn.hInstance = NULL;
	ofn.lpstrFilter = "Key Map File\0*.kmap\0";
	ofn.lpstrCustomFilter = NULL;
	ofn.nMaxCustFilter = 0;
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = FileName;
	ofn.nMaxFile = sizeof(FileName);
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = m_UserDataPath;
	ofn.lpstrTitle = NULL;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
	ofn.nFileOffset = 0;
	ofn.nFileExtension = 0;
	ofn.lpstrDefExt = NULL;
	ofn.lCustData = 0;
	ofn.lpfnHook = NULL;
	ofn.lpTemplateName = NULL;

	if (GetOpenFileName(&ofn))
	{
		if (ReadKeyMap(FileName, &UserKeymap))
			strcpy(m_UserKeyMapPath, FileName);
	}
}

/****************************************************************************/
void BeebWin::SaveUserKeyMap()
{
	char FileName[_MAX_PATH];
	OPENFILENAME ofn;

	FileName[0] = '\0';

	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = m_hWnd;
	ofn.hInstance = NULL;
	ofn.lpstrFilter = "Key Map File\0*.kmap\0";
	ofn.lpstrCustomFilter = NULL;
	ofn.nMaxCustFilter = 0;
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = FileName;
	ofn.nMaxFile = sizeof(FileName);
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = m_UserDataPath;
	ofn.lpstrTitle = NULL;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
	ofn.nFileOffset = 0;
	ofn.nFileExtension = 0;
	ofn.lpstrDefExt = NULL;
	ofn.lCustData = 0;
	ofn.lpfnHook = NULL;
	ofn.lpTemplateName = NULL;

	if (GetSaveFileName(&ofn))
	{
		if (strlen(FileName) < 4 ||
			(strcmp(FileName+(strlen(FileName)-5),".kmap")!=0) &&
			(strcmp(FileName+(strlen(FileName)-5),".KMAP")!=0))
		{
			strcat(FileName,".kmap");
		}
		if (WriteKeyMap(FileName, &UserKeymap))
			strcpy(m_UserKeyMapPath, FileName);
	}
}

/****************************************************************************/
bool BeebWin::ReadKeyMap(char *filename, KeyMap *keymap)
{
	bool success = true;
	char buf[256];
	FILE *infile;

	infile=fopen(filename,"r");
	if (infile == NULL)
	{
		char errstr[500];
		sprintf(errstr, "Failed to read key map file:\n  %s", filename);
		MessageBox(GETHWND, errstr, WindowTitle, MB_OK|MB_ICONERROR);
		success = false;
	}
	else
	{
		if (fgets(buf, 255, infile) == NULL || 
			strcmp(buf, KEYMAP_TOKEN "\n") != 0)
		{
			char errstr[500];
			sprintf(errstr, "Invalid key map file:\n  %s\n", filename);
			MessageBox(GETHWND, errstr, WindowTitle, MB_OK|MB_ICONERROR);
			success = false;
		}
		else
		{
			fgets(buf, 255, infile);

			int i;
			for (i = 0; i < 256; ++i)
			{
				if (fgets(buf, 255, infile) == NULL)
				{
					char errstr[500];
					sprintf(errstr, "Data missing from key map file:\n  %s\n", filename);
					MessageBox(GETHWND, errstr, WindowTitle, MB_OK|MB_ICONERROR);
					success = false;
					break;
				}

				sscanf(buf, "%d %d %d %d %d %d",
					   &(*keymap)[i][0].row,
					   &(*keymap)[i][0].col,
					   &(*keymap)[i][0].shift,
					   &(*keymap)[i][1].row,
					   &(*keymap)[i][1].col,
					   &(*keymap)[i][1].shift);
			}
		}
		fclose(infile);
	}

	return success;
}

/****************************************************************************/
bool BeebWin::WriteKeyMap(char *filename, KeyMap *keymap)
{
	bool success = true;
	FILE *outfile;

	/* First check if file already exists */
	outfile=fopen(filename,"r");
	if (outfile != NULL)
	{
		fclose(outfile);
		char errstr[200];
		sprintf(errstr, "File already exists:\n  %s\n\nOverwrite file?", filename);
		if (MessageBox(GETHWND,errstr,WindowTitle,MB_YESNO|MB_ICONQUESTION) != IDYES)
			return false;
	}

	outfile=fopen(filename,"w");
	if (outfile == NULL)
	{
		char errstr[500];
		sprintf(errstr, "Failed to write key map file:\n  %s", filename);
		MessageBox(GETHWND, errstr, WindowTitle, MB_OK|MB_ICONERROR);
		success = false;
	}
	else
	{
		fprintf(outfile, KEYMAP_TOKEN "\n\n");

		int i;
		for (i = 0; i < 256; ++i)
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
	}

	return success;
}

/****************************************************************************/
/* Clipboard support */

void BeebWin::doCopy()
{
	if (PrinterEnabled)
		TogglePrinter();

	if (IDM_PRINTER_CLIPBOARD != m_MenuIdPrinterPort)
	{
		CheckMenuItem(m_hMenu, m_MenuIdPrinterPort, MF_UNCHECKED);
		m_MenuIdPrinterPort = IDM_PRINTER_CLIPBOARD;
		CheckMenuItem(m_hMenu, m_MenuIdPrinterPort, MF_CHECKED);
	}
	TranslatePrinterPort();
	TogglePrinter();		// Turn printer back on

	m_printerbufferlen = 0;

	m_clipboard[0] = 2;
	m_clipboard[1] = 'L';
	m_clipboard[2] = '.';
	m_clipboard[3] = 13;
	m_clipboard[4] = 3;
	m_clipboardlen = 5;
	m_clipboardptr = 0;
	m_printerbufferlen = 0;
	SetupClipboard();
}

void BeebWin::doPaste()
{
    HGLOBAL   hglb;
    LPTSTR    lptstr;

	if (!IsClipboardFormatAvailable(CF_TEXT))
		return;

	if (!OpenClipboard(m_hWnd))
		return;

	hglb = GetClipboardData(CF_TEXT);
	if (hglb != NULL)
	{
		lptstr = (LPTSTR)GlobalLock(hglb);
		if (lptstr != NULL)
		{
			strncpy(m_clipboard, lptstr, 32767);
			GlobalUnlock(hglb);
			m_clipboardptr = 0;
			m_clipboardlen = (int)strlen(m_clipboard);
			SetupClipboard();
		}
	}
	CloseClipboard();
}

void BeebWin::SetupClipboard(void)
{
	m_OSRDCH = BeebReadMem(0x210) + (BeebReadMem(0x211) << 8);

	BeebWriteMem(0x100, 0x38);	// SEC
	BeebWriteMem(0x101, 0xad);	// LDA &FC51
	BeebWriteMem(0x102, 0x51);
	BeebWriteMem(0x103, 0xfc);
	BeebWriteMem(0x104, 0xd0);	// BNE P% + 6
	BeebWriteMem(0x105, 0x04);
	BeebWriteMem(0x106, 0xad);	// LDA &FC50
	BeebWriteMem(0x107, 0x50);
	BeebWriteMem(0x108, 0xfc);
	BeebWriteMem(0x109, 0x18);	// CLC
	BeebWriteMem(0x10A, 0x60);	// RTS

	BeebWriteMem(0x210, 0x00);
	BeebWriteMem(0x211, 0x01);

	// Just to kick off keyboard input
	int row, col;
	TranslateKey(VK_RETURN, false, row, col);
}

void BeebWin::ResetClipboard(void)
{
	BeebWriteMem(0x210, m_OSRDCH & 255);
	BeebWriteMem(0x211, m_OSRDCH >> 8);
}

int BeebWin::PasteKey(int addr)
{
	int row, col;
	int data = 0x00;
	
	switch (addr)
	{
	case 0 :
		TranslateKey(VK_RETURN, true, row, col);
		if (m_clipboardlen > 0)
		{
			data = m_clipboard[m_clipboardptr++];
			m_clipboardlen--;

			// Drop LF after CR
			if (m_translateCRLF &&
				data == 0xD && m_clipboardlen > 0 &&
				m_clipboard[m_clipboardptr] == 0xA)
			{
				m_clipboardlen--;
				m_clipboardptr++;
			}

			if (m_clipboardlen <= 0)
			{
				ResetClipboard();
			}
		}
		break;
	case 1 :
		data = (m_clipboardlen == 0) ? 255 : 0;
		break;
	}
	return data;
}

void BeebWin::CopyKey(int Value)
{
    HGLOBAL hglbCopy;
    LPTSTR  lptstrCopy;

	if (m_printerbufferlen >= 1024 * 1024)
		return;

	m_printerbuffer[m_printerbufferlen++] = Value;
	if (m_translateCRLF && Value == 0xD)
		m_printerbuffer[m_printerbufferlen++] = 0xA;

	if (!OpenClipboard(m_hWnd))
        return;

    EmptyClipboard();

	hglbCopy = GlobalAlloc(GMEM_MOVEABLE, m_printerbufferlen + 1);
	if (hglbCopy == NULL)
	{
		CloseClipboard();
		return;
	}

	lptstrCopy = (LPTSTR)GlobalLock(hglbCopy);
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
int CALLBACK BrowseCallbackProc(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
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

BOOL CALLBACK DiscExportDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	char str[100];
	HWND hwndList;
	char szDisplayName[MAX_PATH];
	int i, j;

	hwndList = GetDlgItem(hwndDlg, IDC_EXPORTFILELIST);

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
			numSelected = (int)SendMessage(
				hwndList, LB_GETSELITEMS, (WPARAM)DFS_MAX_CAT_SIZE, (LPARAM)filesSelected);
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
				else if (SHGetPathFromIDList(idList, szExportFolder) == FALSE)
				{
					MessageBox(hwndDlg, "Invalid folder selected", WindowTitle, MB_OK|MB_ICONWARNING);
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
	bool success = true;
	int drive;
	int type;
	char szDiscFile[MAX_PATH];
	int heads;
	int side;
	char szErrStr[500];
	int i, n;

	if (menuId == IDM_DISC_EXPORT_0 || menuId == IDM_DISC_EXPORT_2)
		drive = 0;
	else
		drive = 1;

	if (MachineType != 3 && NativeFDC)
	{
		// 8271 controller
		Get8271DiscInfo(drive, szDiscFile, &heads);
	}
	else
	{
		// 1770 controller
		Get1770DiscInfo(drive, &type, szDiscFile);
		if (type == 0)
			heads = 1;
		else if (type == 1)
			heads = 2;
		else
		{
			// ADFS - not currently supported
			MessageBox(m_hWnd, "Export from ADFS disc not supported", WindowTitle, MB_OK|MB_ICONWARNING);
			return;
		}
	}

	// Check for no disk loaded
	if (szDiscFile[0] == 0 || heads == 1 && (menuId == IDM_DISC_EXPORT_2 || menuId == IDM_DISC_EXPORT_3))
	{
		sprintf(szErrStr, "No disc loaded in drive %d", menuId - IDM_DISC_EXPORT_0);
		MessageBox(m_hWnd, szErrStr, WindowTitle, MB_OK|MB_ICONWARNING);
		return;
	}

	// Read the catalogue
	if (menuId == IDM_DISC_EXPORT_0 || menuId == IDM_DISC_EXPORT_1)
		side = 0;
	else
		side = 1;

	success = dfs_get_catalogue(szDiscFile, heads, side, &dfsCat);
	if (!success)
	{
		sprintf(szErrStr, "Failed to read catalogue from disc:\n  %s", szDiscFile);
		MessageBox(m_hWnd, szErrStr, WindowTitle, MB_OK|MB_ICONERROR);
		return;
	}

	// Show export dialog
	if (DialogBox(hInst, MAKEINTRESOURCE(IDD_DISCEXPORT), m_hWnd, (DLGPROC)DiscExportDlgProc) != IDOK ||
		numSelected == 0)
	{
		return;
	}

	// Export the files
	n = 0;
	for (i = 0; i < numSelected; ++i)
	{
		success = dfs_export_file(szDiscFile, heads, side, &dfsCat,
								  filesSelected[i], szExportFolder, szErrStr);
		if (success)
		{
			n++;
		}
		else
		{
			success = true;
			if (MessageBox(m_hWnd, szErrStr, WindowTitle, MB_OKCANCEL|MB_ICONWARNING) == IDCANCEL)
			{
				success = false;
				break;
			}
		}
	}

	sprintf(szErrStr, "Files successfully exported: %d", n);
	MessageBox(m_hWnd, szErrStr, WindowTitle, MB_OK|MB_ICONINFORMATION);
}


// File import
void BeebWin::ImportDiscFiles(int menuId)
{
	bool success = true;
	int drive;
	int type;
	char szDiscFile[MAX_PATH];
	int heads;
	int side;
	char szErrStr[500];
	char szFolder[MAX_PATH];
	char fileSelection[4096];
	char baseName[MAX_PATH];
	OPENFILENAME ofn;
	char *fileName;
	static char fileNames[DFS_MAX_CAT_SIZE*2][MAX_PATH]; // Allow user to select > cat size
	int numFiles;
	int i, n;

	if (menuId == IDM_DISC_IMPORT_0 || menuId == IDM_DISC_IMPORT_2)
		drive = 0;
	else
		drive = 1;

	if (MachineType != 3 && NativeFDC)
	{
		// 8271 controller
		Get8271DiscInfo(drive, szDiscFile, &heads);
	}
	else
	{
		// 1770 controller
		Get1770DiscInfo(drive, &type, szDiscFile);
		if (type == 0)
			heads = 1;
		else if (type == 1)
			heads = 2;
		else
		{
			// ADFS - not currently supported
			MessageBox(m_hWnd, "Import to ADFS disc not supported", WindowTitle, MB_OK|MB_ICONWARNING);
			return;
		}
	}

	// Check for no disk loaded
	if (szDiscFile[0] == 0 || heads == 1 && (menuId == IDM_DISC_IMPORT_2 || menuId == IDM_DISC_IMPORT_3))
	{
		sprintf(szErrStr, "No disc loaded in drive %d", menuId - IDM_DISC_IMPORT_0);
		MessageBox(m_hWnd, szErrStr, WindowTitle, MB_OK|MB_ICONWARNING);
		return;
	}

	// Read the catalogue
	if (menuId == IDM_DISC_IMPORT_0 || menuId == IDM_DISC_IMPORT_1)
		side = 0;
	else
		side = 1;

	success = dfs_get_catalogue(szDiscFile, heads, side, &dfsCat);
	if (!success)
	{
		sprintf(szErrStr, "Failed to read catalogue from disc:\n  %s", szDiscFile);
		MessageBox(m_hWnd, szErrStr, WindowTitle, MB_OK|MB_ICONERROR);
		return;
	}

	// Get list of files to import
	memset(fileSelection, 0, sizeof(fileSelection));
	szFolder[0] = 0;
	GetDataPath(m_UserDataPath, szFolder);

	memset(&ofn, 0, sizeof(ofn));
	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = m_hWnd;
	ofn.lpstrFilter = "INF files (*.inf)\0*.inf\0" "All files (*.*)\0*.*\0";
	ofn.nFilterIndex = 1;
	ofn.lpstrFile = fileSelection;
	ofn.nMaxFile = sizeof(fileSelection);
	ofn.lpstrInitialDir = szFolder;
	ofn.lpstrTitle = "Select files to import";
	ofn.Flags = OFN_ALLOWMULTISELECT | OFN_EXPLORER | OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
	if (!GetOpenFileName(&ofn))
	{
		return;
	}

	// Parse the file selection string
	fileName = fileSelection + strlen(fileSelection) + 1;
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
			if (MessageBox(m_hWnd, szErrStr, WindowTitle, MB_OKCANCEL|MB_ICONWARNING) == IDCANCEL)
			{
				success = false;
				break;
			}
		}
	}

	sprintf(szErrStr, "Files successfully imported: %d", n);
	MessageBox(m_hWnd, szErrStr, WindowTitle, MB_OK|MB_ICONINFORMATION);

	// Re-read disc image
	if (MachineType != 3 && NativeFDC)
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
			Load1770DiscImage(szDiscFile, drive, 1, m_hMenu);
		else
			Load1770DiscImage(szDiscFile, drive, 0, m_hMenu);
	}
}

INT GetEncoderClsid(const WCHAR* format, CLSID* pClsid);

/****************************************************************************/
/* Bitmap capture support */
void BeebWin::CaptureBitmapPending(bool autoFilename)
{
	m_CaptureBitmapPending = true;
	m_CaptureBitmapAutoFilename = autoFilename;
}

bool BeebWin::GetImageEncoderClsid(WCHAR *mimeType, CLSID *encoderClsid)
{
	UINT num;
	UINT size;
	UINT i;
	ImageCodecInfo* pImageCodecInfo;

	GetImageEncodersSize(&num, &size);
	if (size == 0)
		return false;

	pImageCodecInfo = (ImageCodecInfo*)(malloc(size));
	if (pImageCodecInfo == NULL)
		return false;

	GetImageEncoders(num, size, pImageCodecInfo);
	for(i = 0; i < num; ++i)
	{ 
		if (wcscmp(pImageCodecInfo[i].MimeType, mimeType) == 0)
		{
			*encoderClsid = pImageCodecInfo[i].Clsid;
			break;
		}
	}
	free(pImageCodecInfo);

	if (i == num)
	{
		MessageBox(m_hWnd, "Failed to get image encoder",
				   WindowTitle, MB_OK|MB_ICONERROR);
		return false;
	}

	return true;
}

bool BeebWin::GetImageFile(char *FileName)
{
	char DefaultPath[_MAX_PATH];
	OPENFILENAME ofn;
	bool success = false;
	char filter[200];
	char *fileExt = NULL;

	switch (m_MenuIdCaptureFormat)
	{
	default:
	case IDM_CAPTUREBMP:
		fileExt = ".bmp";
		break;
	case IDM_CAPTUREJPEG:
		fileExt = ".jpg";
		break;
	case IDM_CAPTUREGIF:
		fileExt = ".gif";
		break;
	case IDM_CAPTUREPNG:
		fileExt = ".png";
		break;
	}

	PrefsGetStringValue("ImagePath",DefaultPath);
	GetDataPath(m_UserDataPath, DefaultPath);

	FileName[0] = '\0';

	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = m_hWnd;
	ofn.hInstance = NULL;
	sprintf(filter, "Image File (*%s)\0*%s\0", fileExt, fileExt);
	ofn.lpstrFilter = filter;
	ofn.lpstrCustomFilter = NULL;
	ofn.nMaxCustFilter = 0;
	ofn.nFilterIndex = 0;
	ofn.lpstrFile = FileName;
	ofn.nMaxFile = MAX_PATH;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = DefaultPath;
	ofn.lpstrTitle = NULL;
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_HIDEREADONLY;
	ofn.nFileOffset = 0;
	ofn.nFileExtension = 0;
	ofn.lpstrDefExt = NULL;
	ofn.lCustData = 0;
	ofn.lpfnHook = NULL;
	ofn.lpTemplateName = NULL;

	if (GetSaveFileName(&ofn))
	{
		// Add extension
		if (strlen(FileName) < 5 ||
			((strcmp(FileName + (strlen(FileName)-4), fileExt)!=0) &&
			 (strcmp(FileName + (strlen(FileName)-4), fileExt)!=0)) )
		{
			strcat(FileName, fileExt);
		}

		if (m_AutoSavePrefsFolders)
		{
			unsigned int PathLength = (unsigned int)(strrchr(FileName, '\\') - FileName);
			strncpy(DefaultPath, FileName, PathLength);
			DefaultPath[PathLength] = 0;
			PrefsSetStringValue("ImagePath", DefaultPath);
		}

		success = true;
	}

	return success;
}

void BeebWin::CaptureBitmap(int x, int y, int sx, int sy)
{
	WCHAR *mimeType = NULL;
	CLSID encoderClsid;
	HBITMAP CaptureDIB = NULL;
	HDC CaptureDC = NULL;
	bmiData Capturebmi;
	char *CaptureScreen = NULL;
	char *fileExt = NULL;
	char AutoName[MAX_PATH];
	WCHAR wFileName[MAX_PATH];

	switch (m_MenuIdCaptureFormat)
	{
	default:
	case IDM_CAPTUREBMP:
		mimeType = L"image/bmp";
		fileExt = ".bmp";
		break;
	case IDM_CAPTUREJPEG:
		mimeType = L"image/jpeg";
		fileExt = ".jpg";
		break;
	case IDM_CAPTUREGIF:
		mimeType = L"image/gif";
		fileExt = ".gif";
		break;
	case IDM_CAPTUREPNG:
		mimeType = L"image/png";
		fileExt = ".png";
		break;
	}

	if (!GetImageEncoderClsid(mimeType, &encoderClsid))
		return;

	// Auto generate filename?
	if (m_CaptureBitmapAutoFilename)
	{
		PrefsGetStringValue("ImagePath", m_CaptureFileName);
		GetDataPath(m_UserDataPath, m_CaptureFileName);

		SYSTEMTIME systemTime;
		GetLocalTime(&systemTime);
		sprintf(AutoName, "\\BeebEm_%04d%02d%02d_%02d%02d%02d_%d%s",
				systemTime.wYear, systemTime.wMonth, systemTime.wDay,
				systemTime.wHour, systemTime.wMinute, systemTime.wSecond,
				systemTime.wMilliseconds/100,
				fileExt);
		strcat(m_CaptureFileName, AutoName);
	}

	// Capture the bitmap
	Capturebmi = m_bmi;
	if (m_MenuIdCaptureResolution == IDM_CAPTURERES1)
	{
		Capturebmi.bmiHeader.biWidth = m_XWinSize;
		Capturebmi.bmiHeader.biHeight = m_YWinSize;
	}
	else if (m_MenuIdCaptureResolution == IDM_CAPTURERES2)
	{
		Capturebmi.bmiHeader.biWidth = 1280;
		Capturebmi.bmiHeader.biHeight = 1024;
	}
	else if (m_MenuIdCaptureResolution == IDM_CAPTURERES3)
	{
		Capturebmi.bmiHeader.biWidth = 640;
		Capturebmi.bmiHeader.biHeight = 512;
	}
	else
	{
		Capturebmi.bmiHeader.biWidth = 320;
		Capturebmi.bmiHeader.biHeight = 256;
	}
	Capturebmi.bmiHeader.biSizeImage = Capturebmi.bmiHeader.biWidth * Capturebmi.bmiHeader.biHeight;
	CaptureDC = CreateCompatibleDC(NULL);
	CaptureDIB = CreateDIBSection(CaptureDC, (BITMAPINFO *)&Capturebmi,
									DIB_RGB_COLORS, (void**)&CaptureScreen, NULL, 0);
	HGDIOBJ prevObj = SelectObject(CaptureDC, CaptureDIB);
	if (prevObj == NULL)
	{
		MessageBox(m_hWnd, "Failed to initialise capture buffers",
				   WindowTitle, MB_OK|MB_ICONERROR);
	}
	else
	{
		StretchBlt(CaptureDC, 0, 0, Capturebmi.bmiHeader.biWidth, Capturebmi.bmiHeader.biHeight,
				   m_hDCBitmap, x, y, sx, sy, SRCCOPY);
		SelectObject(CaptureDC, prevObj);

		// Use GDI+ Bitmap to save bitmap
		Bitmap *bitmap = new Bitmap((HBITMAP)CaptureDIB, 0);

		mbstowcs(wFileName, m_CaptureFileName, MAX_PATH);
		Status status = bitmap->Save(wFileName, &encoderClsid, NULL);
		if (status != Ok)
		{
			MessageBox(m_hWnd, "Failed to save screen capture",
					   WindowTitle, MB_OK|MB_ICONERROR);
		}
		else if (m_CaptureBitmapAutoFilename)
		{
			// Let user know bitmap has been saved
			FlashWindow(GETHWND, TRUE);
			MessageBeep(MB_ICONEXCLAMATION);
		}

		delete bitmap;
	}

	if (CaptureDIB != NULL)
		DeleteObject(CaptureDIB);
	if (CaptureDC != NULL)
		DeleteDC(CaptureDC);
}
