// BeebEm IO support - disk, tape, state, printer, AVI capture

#include <stdio.h>
#include <windows.h>
#include <initguid.h>
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

extern EDCB ExtBoard;
extern bool DiscLoaded[2]; // Set to TRUE when a disc image has been loaded.
extern char CDiscName[2][256]; // Filename of disc current in drive 0 and 1;
extern char CDiscType[2]; // Current disc types
extern char FDCDLL[256];
extern HMODULE hFDCBoard;
extern AVIWriter *aviWriter;

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
		EnableMenuItem(m_hMenu, IDM_WPDISC0, MF_ENABLED );
		CheckMenuItem(m_hMenu, IDM_WPDISC0, MF_CHECKED);
	}
	else
	{
		mii.dwTypeData = "Eject Disc 1";
		SetMenuItemInfo(m_hMenu, IDM_EJECTDISC1, FALSE, &mii);
		EnableMenuItem(m_hMenu, IDM_WPDISC1, MF_ENABLED );
		CheckMenuItem(m_hMenu, IDM_WPDISC1, MF_CHECKED);
	}
}

/****************************************************************************/
int BeebWin::ReadDisc(int Drive,HMENU dmenu)
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
		PrefsSetDWORDValue("DiscsFilter",ofn.nFilterIndex);

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
		LoadUEFState(FileName);
	}
}

/****************************************************************************/
void BeebWin::ToggleWriteProtect(int Drive)
{
	HMENU hMenu = m_hMenu;
	if (MachineType!=3 && NativeFDC)
	{
		if (m_WriteProtectDisc[Drive])
		{
			m_WriteProtectDisc[Drive] = 0;
			DiscWriteEnable(Drive, 1);
		}
		else
		{
			m_WriteProtectDisc[Drive] = 1;
			DiscWriteEnable(Drive, 0);
		}
		DWriteable[Drive]=1-m_WriteProtectDisc[Drive];

		if (Drive == 0)
			CheckMenuItem(hMenu, IDM_WPDISC0, m_WriteProtectDisc[0] ? MF_CHECKED : MF_UNCHECKED);
		else
			CheckMenuItem(hMenu, IDM_WPDISC1, m_WriteProtectDisc[1] ? MF_CHECKED : MF_UNCHECKED);
	}
	else
	{
		DWriteable[Drive]=1-DWriteable[Drive];
		m_WriteProtectDisc[Drive]=1-DWriteable[Drive];
		if (Drive == 0)
			CheckMenuItem(hMenu, IDM_WPDISC0, DWriteable[0] ? MF_UNCHECKED : MF_CHECKED);
		else
			CheckMenuItem(hMenu, IDM_WPDISC1, DWriteable[1] ? MF_UNCHECKED : MF_CHECKED);
	}
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
	char FileName[256];
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
	char StartPath[_MAX_PATH];
	char FileName[256];
	OPENFILENAME ofn;
	BOOL changed;

	PrefsGetStringValue("AVIPath",StartPath);
	GetDataPath(m_UserDataPath, StartPath);

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
		// Add avi extension
		if (strlen(FileName) < 5 ||
			((strcmp(FileName + (strlen(FileName)-4), ".AVI")!=0) &&
			 (strcmp(FileName + (strlen(FileName)-4), ".avi")!=0)) )
		{
			strcat(FileName,".avi");
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

			HRESULT hr = aviWriter->Initialise(FileName, wfp, &m_Avibmi,
				(int)((m_FramesPerSecond > 46 ? 50 : m_FramesPerSecond) / (m_AviFrameSkip+1)), m_hWnd);
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
void SaveEmuUEF(FILE *SUEF) {
	char EmuName[16];
	fput16(0x046C,SUEF);
	fput32(16,SUEF);
	// BeebEm Title Block
	strcpy(EmuName,"BeebEm");
	EmuName[14]=1;
	EmuName[15]=4; // Version, 1.4
	fwrite(EmuName,16,1,SUEF);
	//
	fput16(0x046a,SUEF);
	fput32(16,SUEF);
	// Emulator Specifics
	// Note about this block: It should only be handled by beebem from uefstate.cpp if
	// the UEF has been determined to be from BeebEm (Block 046C)
	fputc(MachineType,SUEF);
	fputc((NativeFDC)?0:1,SUEF);
	fputc(TubeEnabled,SUEF);
	fputc(0,SUEF); // Monitor type, reserved
	fputc(0,SUEF); // Speed Setting, reserved
	fput32(0,SUEF);
	fput32(0,SUEF);
	fput16(0,SUEF);
	fputc(0,SUEF);
}

void LoadEmuUEF(FILE *SUEF, int Version) {
	MachineType=fgetc(SUEF);
	if (Version <= 8 && MachineType == 1)
		MachineType = 3;
	NativeFDC=(fgetc(SUEF)==0)?TRUE:FALSE;
	TubeEnabled=fgetc(SUEF);
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
