/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2009  Mike Wyatt

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

// BeebWin ROM Configuration Dialog

#include <stdio.h>
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <initguid.h>
#include "beebemrc.h"
#include "main.h"
#include "beebwin.h"
#include "beebmem.h"

static HWND hWndROMList = NULL;
static HWND hWndModel = NULL;
static int nModel = 0;
static const LPCSTR szModel[] = { "BBC B", "Integra-B", "B Plus", "Master 128" };
static ROMConfigFile ROMCfg;
static char szDefaultROMPath[MAX_PATH] = {0};
static char szDefaultROMConfigPath[MAX_PATH] = {0};

static BOOL CALLBACK ROMConfigDlgProc(
	HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam);
static bool LoadROMConfigFile(HWND hWnd);
static bool SaveROMConfigFile(HWND hWnd);
static bool GetROMFile(HWND hWnd, char *pFileName);
static bool WriteROMFile(const char *filename, ROMConfigFile RomConfig);

/****************************************************************************/
void BeebWin::EditROMConfig(void)
{
	INT_PTR nResult;

	// Copy Rom config
	memcpy(&ROMCfg, &RomConfig, sizeof(ROMConfigFile));

	nResult = DialogBox(hInst, MAKEINTRESOURCE(IDD_ROMCONFIG), m_hWnd, (DLGPROC)ROMConfigDlgProc);
	if (nResult == TRUE)
	{
		// Copy in new config and read ROMs
		memcpy(&RomConfig, &ROMCfg, sizeof(ROMConfigFile));
		BeebReadRoms();
	}
}

/****************************************************************************/
static int LVInsertColumn(
	HWND hWnd, UINT uCol, const LPTSTR pszText, int iAlignment, UINT uWidth)
{
	LVCOLUMN lc = {0};
	lc.mask = LVCF_SUBITEM | LVCF_TEXT | LVCF_FMT | LVCF_WIDTH;
	lc.fmt = iAlignment;
	lc.pszText = pszText;
	lc.iSubItem = uCol;
	lc.cx = uWidth;
	return ListView_InsertColumn(hWnd, uCol, &lc);
}

static int LVInsertItem(
	HWND hWnd, UINT uRow, UINT uCol, const LPTSTR pszText, LPARAM lParam)
{
	LVITEM li = {0};
	li.mask = LVIF_TEXT | LVIF_PARAM;
	li.iItem = uRow;
	li.iSubItem = uCol;
	li.pszText = pszText;
	li.lParam = (lParam ? lParam : uRow);
	return ListView_InsertItem(hWnd, &li);
}

static void LVSetItemText(
	HWND hWnd, UINT uRow, UINT uCol, const LPTSTR pszText)
{
	ListView_SetItemText(hWnd, uRow, uCol, pszText);
}

static void LVSetFocus(HWND hWnd)
{
	int row = ListView_GetSelectionMark(hWnd);
	ListView_SetItemState(hWndROMList, row,
						  LVIS_SELECTED|LVIS_FOCUSED, LVIS_SELECTED|LVIS_FOCUSED);
	SetFocus(hWnd);
}

/****************************************************************************/
static void UpdateROMField(int row)
{
	char szROMFile[_MAX_PATH];
	bool unplugged = false;
	int bank;

	if (nModel == 3)
	{
		bank = 16 - row;
		if (bank >= 0 && bank <= 7)
			unplugged = (CMOSRAM[20] & (1 << bank)) ? false : true;
		else if (bank >= 8 && bank <= 15)
			unplugged = (CMOSRAM[21] & (1 << (bank-8))) ? false : true;
	}

	strncpy(szROMFile, ROMCfg[nModel][row], _MAX_PATH);
	if (unplugged)
		strncat(szROMFile, " (unplugged)", _MAX_PATH);
	LVSetItemText(hWndROMList, row, 1, (LPTSTR)szROMFile);
}

/****************************************************************************/
static void FillROMList(void)
{
	int bank;
	int row;
	char str[20];

	Edit_SetText(hWndModel, szModel[nModel]);

	ListView_DeleteAllItems(hWndROMList);

	row = 0;
	LVInsertItem(hWndROMList, row, 0, (LPTSTR)"OS", 16);
	LVSetItemText(hWndROMList, row, 1, (LPTSTR)ROMCfg[nModel][0]);

	for (row = 1; row <= 16; ++row)
	{
		bank = 16 - row;
		sprintf(str, "%02d (%X)", bank, bank);
		LVInsertItem(hWndROMList, row, 0, (LPTSTR)str, bank);
		UpdateROMField(row);
	}
}

/****************************************************************************/
static BOOL CALLBACK ROMConfigDlgProc(
	HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	int row;
	char *cfg;

	switch (message)
	{
		case WM_INITDIALOG:
			nModel = MachineType;
			hWndModel = GetDlgItem(hwndDlg, IDC_MODEL);
			hWndROMList = GetDlgItem(hwndDlg, IDC_ROMLIST);
			ListView_SetExtendedListViewStyle(hWndROMList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
			LVInsertColumn(hWndROMList, 0, "Bank", LVCFMT_LEFT, 45);
			LVInsertColumn(hWndROMList, 1, "ROM File", LVCFMT_LEFT, 283);
			FillROMList();
			return TRUE;

		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
			case IDC_BBCB:
				nModel = 0;
				FillROMList();
				return TRUE;
			case IDC_INTEGRAB:
				nModel = 1;
				FillROMList();
				return TRUE;
			case IDC_BBCBPLUS:
				nModel = 2;
				FillROMList();
				return TRUE;
			case IDC_MASTER128:
				nModel = 3;
				FillROMList();
				return TRUE;
				
			case IDC_SELECTROM:
				row = ListView_GetSelectionMark(hWndROMList);
				if (row >= 0 && row <= 16)
				{
					char szROMFile[MAX_PATH];
					if (GetROMFile(hwndDlg, szROMFile))
					{
						strcpy(ROMCfg[nModel][row], szROMFile);
						UpdateROMField(row);
					}
				}
				LVSetFocus(hWndROMList);
				break;
			case IDC_MARKWRITABLE:
				row = ListView_GetSelectionMark(hWndROMList);
				if (row >= 1 && row <= 16)
				{
					cfg = ROMCfg[nModel][row];
					if (strcmp(cfg, BANK_EMPTY) != 0 && strcmp(cfg, BANK_RAM) != 0)
					{
						if (strlen(cfg) > 4 && strcmp(cfg + strlen(cfg) - 4, ROM_WRITABLE) == 0)
							cfg[strlen(cfg) - 4] = 0;
						else
							strcat(cfg, ROM_WRITABLE);
						UpdateROMField(row);
					}
				}
				LVSetFocus(hWndROMList);
				break;
			case IDC_RAM:
				row = ListView_GetSelectionMark(hWndROMList);
				if (row >= 1 && row <= 16)
				{
					strcpy(ROMCfg[nModel][row], BANK_RAM);
					UpdateROMField(row);
				}
				LVSetFocus(hWndROMList);
				break;
			case IDC_EMPTY:
				row = ListView_GetSelectionMark(hWndROMList);
				if (row >= 1 && row <= 16)
				{
					strcpy(ROMCfg[nModel][row], BANK_EMPTY);
					UpdateROMField(row);
				}
				LVSetFocus(hWndROMList);
				break;

			case IDC_SAVE:
				SaveROMConfigFile(hwndDlg);
				LVSetFocus(hWndROMList);
				break;
			case IDC_LOAD:
				LoadROMConfigFile(hwndDlg);
				LVSetFocus(hWndROMList);
				break;
				
			case IDOK:
				EndDialog(hwndDlg, TRUE);
				return TRUE;
				
			case IDCANCEL:
				EndDialog(hwndDlg, FALSE);
				return TRUE;
			}
			break;
	}
	return FALSE;
}

/****************************************************************************/
static bool LoadROMConfigFile(HWND hWnd)
{
	char DefaultPath[MAX_PATH];
	char szROMConfigPath[MAX_PATH];
	char *pFileName = szROMConfigPath;
	OPENFILENAME ofn;
	bool success = false;

	szROMConfigPath[0] = 0;
	mainWin->GetDataPath(mainWin->GetUserDataPath(), szROMConfigPath);
	int nROMPathLen = (int)strlen(szROMConfigPath);

	if (szDefaultROMConfigPath[0])
		strcpy(DefaultPath, szDefaultROMConfigPath);
	else
		strcpy(DefaultPath, szROMConfigPath);

	pFileName[0] = 0;

	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hWnd;
	ofn.hInstance = NULL;
	ofn.lpstrFilter = "ROM File (*.cfg)\0*.cfg\0";
	ofn.lpstrCustomFilter = NULL;
	ofn.nMaxCustFilter = 0;
	ofn.nFilterIndex = 0;
	ofn.lpstrFile = pFileName;
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

	if (GetOpenFileName(&ofn))
	{
		// Save directory as default for next time
		unsigned int PathLength = (unsigned int)(strrchr(pFileName, '\\') - pFileName);
		strncpy(szDefaultROMConfigPath, pFileName, PathLength);
		szDefaultROMConfigPath[PathLength] = 0;

		// Read the file
		ROMConfigFile LoadedROMCfg;
		if (ReadROMFile(pFileName, LoadedROMCfg))
		{
			// Copy in loaded config
			memcpy(&ROMCfg, &LoadedROMCfg, sizeof(ROMConfigFile));
			FillROMList();
			success = true;
		}
	}

	return success;
}

/****************************************************************************/
static bool SaveROMConfigFile(HWND hWnd)
{
	char DefaultPath[MAX_PATH];
	char szROMConfigPath[MAX_PATH];
	char *pFileName = szROMConfigPath;
	OPENFILENAME ofn;
	bool success = false;

	szROMConfigPath[0] = 0;
	mainWin->GetDataPath(mainWin->GetUserDataPath(), szROMConfigPath);
	int nROMPathLen = (int)strlen(szROMConfigPath);

	if (szDefaultROMConfigPath[0])
		strcpy(DefaultPath, szDefaultROMConfigPath);
	else
		strcpy(DefaultPath, szROMConfigPath);

	pFileName[0] = 0;

	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hWnd;
	ofn.hInstance = NULL;
	ofn.lpstrFilter = "ROM File (*.cfg)\0*.cfg\0";
	ofn.lpstrCustomFilter = NULL;
	ofn.nMaxCustFilter = 0;
	ofn.nFilterIndex = 0;
	ofn.lpstrFile = pFileName;
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
		// Save directory as default for next time
		unsigned int PathLength = (unsigned int)(strrchr(pFileName, '\\') - pFileName);
		strncpy(szDefaultROMConfigPath, pFileName, PathLength);
		szDefaultROMConfigPath[PathLength] = 0;

		// Add a file extension if required
		if (strchr(pFileName, '.') == NULL)
		{
			strcat(pFileName, ".cfg");
		}

		// Save the file
		if (WriteROMFile(pFileName, ROMCfg))
		{
			success = true;
		}
	}

	return success;
}

/****************************************************************************/
bool WriteROMFile(const char *filename, ROMConfigFile ROMConfig)
{
	FILE *fd;
	int model;
	int bank;

	fd = fopen(filename, "r");
	if (fd)
	{
		fclose(fd);
		char errstr[200];
		sprintf(errstr, "File already exists:\n  %s\n\nOverwrite file?", filename);
		if (MessageBox(GETHWND,errstr,WindowTitle,MB_YESNO|MB_ICONQUESTION) != IDYES)
			return false;
	}

	fd = fopen(filename, "w");
	if (!fd)
	{
		char errstr[200];
		sprintf(errstr, "Failed to write ROM configuration file:\n  %s", filename);
		MessageBox(GETHWND,errstr,WindowTitle,MB_OK|MB_ICONERROR);
		return false;
	}

	for (model = 0; model < 4; ++model)
	{
		for (bank = 0; bank < 17; ++bank)
		{
			fprintf(fd, "%s\n", ROMConfig[model][bank]);
		}
	}

	fclose(fd);

	return true;
}

/****************************************************************************/
static bool GetROMFile(HWND hWnd, char *pFileName)
{
	char DefaultPath[MAX_PATH];
	char szROMPath[MAX_PATH];
	OPENFILENAME ofn;
	bool success = false;

	strcpy(szROMPath, "BeebFile");
	mainWin->GetDataPath(mainWin->GetUserDataPath(), szROMPath);
	int nROMPathLen = (int)strlen(szROMPath);

	if (szDefaultROMPath[0])
		strcpy(DefaultPath, szDefaultROMPath);
	else
		strcpy(DefaultPath, szROMPath);

	pFileName[0] = '\0';

	ofn.lStructSize = sizeof(OPENFILENAME);
	ofn.hwndOwner = hWnd;
	ofn.hInstance = NULL;
	ofn.lpstrFilter = "ROM File (*.rom)\0*.rom\0";
	ofn.lpstrCustomFilter = NULL;
	ofn.nMaxCustFilter = 0;
	ofn.nFilterIndex = 0;
	ofn.lpstrFile = pFileName;
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

	if (GetOpenFileName(&ofn))
	{
		// Save directory as default for next time
		unsigned int PathLength = (unsigned int)(strrchr(pFileName, '\\') - pFileName);
		strncpy(szDefaultROMPath, pFileName, PathLength);
		szDefaultROMPath[PathLength] = 0;

		// Strip user data path
		if (strncmp(pFileName, szROMPath, nROMPathLen) == 0)
		{
			strcpy(pFileName, pFileName + nROMPathLen + 1);
		}

		success = true;
	}

	return success;
}
