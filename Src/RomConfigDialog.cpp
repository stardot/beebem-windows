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

// BeebEm ROM Configuration Dialog

#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>

#include <string.h>

#include "beebemrc.h"
#include "main.h"
#include "beebwin.h"
#include "beebmem.h"
#include "rtc.h"
#include "filedialog.h"
#include "Model.h"
#include "RomConfigFile.h"
#include "RomConfigDialog.h"
#include "StringUtils.h"

static char szDefaultROMPath[MAX_PATH] = {0};
static char szDefaultROMConfigPath[MAX_PATH] = {0};

static bool GetRomFile(HWND hWnd, char *pFileName);

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

/****************************************************************************/

RomConfigDialog::RomConfigDialog(HINSTANCE hInstance, HWND hWndParent, Model model, const RomConfigFile& Config) :
	m_hInstance(hInstance),
	m_hWndParent(hWndParent),
	m_Model(model),
	m_RomConfig(Config) // Copy Rom config
{
}

bool RomConfigDialog::DoModal()
{
	return DialogBoxParam(
		m_hInstance,
		MAKEINTRESOURCE(IDD_ROMCONFIG),
		m_hWndParent,
		sDlgProc,
		reinterpret_cast<LPARAM>(this)
	) != 0;
}

const RomConfigFile& RomConfigDialog::GetRomConfig() const
{
	return m_RomConfig;
}

INT_PTR CALLBACK RomConfigDialog::sDlgProc(
	HWND   hwnd,
	UINT   nMessage,
	WPARAM wParam,
	LPARAM lParam)
{
	RomConfigDialog* dialog;

	if (nMessage == WM_INITDIALOG)
	{
		SetWindowLongPtr(hwnd, DWLP_USER, lParam);
		dialog = reinterpret_cast<RomConfigDialog*>(lParam);
	}
	else
	{
		dialog = reinterpret_cast<RomConfigDialog*>(
			GetWindowLongPtr(hwnd, DWLP_USER)
		);
	}

	return dialog->DlgProc(hwnd, nMessage, wParam, lParam);
}

void RomConfigDialog::LVSetFocus(HWND hWnd)
{
	int row = ListView_GetSelectionMark(hWnd);
	ListView_SetItemState(m_hWndRomList,
	                      row,
	                      LVIS_SELECTED | LVIS_FOCUSED,
	                      LVIS_SELECTED | LVIS_FOCUSED);

	SetFocus(hWnd);
}

/****************************************************************************/
void RomConfigDialog::UpdateRomField(int row)
{
	std::string RomFileName;
	bool unplugged = false;

	if (m_Model == Model::Master128)
	{
		int bank = 16 - row;
		if (bank >= 0 && bank <= 7)
			unplugged = (CMOSRAM_M128[20] & (1 << bank)) ? false : true;
		else if (bank >= 8 && bank <= 15)
			unplugged = (CMOSRAM_M128[21] & (1 << (bank-8))) ? false : true;
	}

	RomFileName = m_RomConfig.GetFileName(m_Model, row);
	if (unplugged)
		RomFileName += " (unplugged)";

	LVSetItemText(m_hWndRomList, row, 1, (LPTSTR)RomFileName.c_str());
}

/****************************************************************************/
void RomConfigDialog::FillModelList()
{
	HWND hWndModel = GetDlgItem(m_hWnd, IDC_MODEL);

	for (int i = 0; i < static_cast<int>(Model::Last); i++)
	{
		ComboBox_AddString(hWndModel, GetModelName(static_cast<Model>(i)));
	}

	ComboBox_SetCurSel(hWndModel, static_cast<int>(m_Model));
}

/****************************************************************************/
void RomConfigDialog::FillRomList()
{
	ListView_DeleteAllItems(m_hWndRomList);

	int row = 0;
	
	switch (m_Model)
	{
		case Model::FileStoreE01:
			LVInsertItem(m_hWndRomList, row, 0, (LPTSTR)"OS", 16);
			LVSetItemText(m_hWndRomList, row, 1, (LPTSTR)m_RomConfig.GetFileName(m_Model, 0).c_str());
			UpdateRomField(row);
			row++;
			LVInsertItem(m_hWndRomList, row, 0, (LPTSTR)"FS", 15);
			LVSetItemText(m_hWndRomList, row, 1, (LPTSTR)m_RomConfig.GetFileName(m_Model, 1).c_str());
			UpdateRomField(row);
			break;

		case Model::FileStoreE01S:
			LVInsertItem(m_hWndRomList, row, 0, (LPTSTR)"OS", 16);
			LVSetItemText(m_hWndRomList, row, 1, (LPTSTR)m_RomConfig.GetFileName(m_Model, 0).c_str());
			UpdateRomField(row);
			break;

		default:
			LVInsertItem(m_hWndRomList, row, 0, (LPTSTR)"OS", 16);
			LVSetItemText(m_hWndRomList, row, 1, (LPTSTR)m_RomConfig.GetFileName(m_Model, 0).c_str());

			for (row = 1; row <= 16; ++row)
			{
				int bank = 16 - row;

				char str[20];
				sprintf(str, "%02d (%X)", bank, bank);

				LVInsertItem(m_hWndRomList, row, 0, (LPTSTR)str, bank);
				UpdateRomField(row);
			}
			break;
	}
}

/****************************************************************************/

INT_PTR RomConfigDialog::DlgProc(
	HWND   hwnd,
	UINT   nMessage,
	WPARAM wParam,
	LPARAM /* lParam */)
{
	switch (nMessage)
	{
		case WM_INITDIALOG:
			m_hWnd = hwnd;
			m_hWndRomList = GetDlgItem(m_hWnd, IDC_ROMLIST);
			ListView_SetExtendedListViewStyle(m_hWndRomList, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);
			LVInsertColumn(m_hWndRomList, 0, "Bank", LVCFMT_LEFT, 45);
			LVInsertColumn(m_hWndRomList, 1, "ROM File", LVCFMT_LEFT, 283);
			FillModelList();
			FillRomList();
			return TRUE;

		case WM_COMMAND: {
			int nDlgItemID = GET_WM_COMMAND_ID(wParam, lParam);
			int Notification = GET_WM_COMMAND_CMD(wParam, lParam);
			int row;

			switch (nDlgItemID)
			{
			case IDC_MODEL:
				if (Notification == CBN_SELCHANGE)
				{
					HWND hWndModelCombo = GetDlgItem(m_hWnd, IDC_MODEL);
					m_Model = static_cast<Model>(ComboBox_GetCurSel(hWndModelCombo));
					FillRomList();
				}
				break;

			case IDC_SELECTROM:
				row = ListView_GetSelectionMark(m_hWndRomList);

				if (row >= 0 && row <= 16)
				{
					char szRomFile[MAX_PATH];
					if (GetRomFile(m_hWnd, szRomFile))
					{
						m_RomConfig.SetFileName(m_Model, row, szRomFile);
						UpdateRomField(row);
					}
				}

				LVSetFocus(m_hWndRomList);
				break;

			case IDC_MARKWRITABLE:
				row = ListView_GetSelectionMark(m_hWndRomList);

				if (row >= 1 && row <= 16)
				{
					std::string RomFileName = m_RomConfig.GetFileName(m_Model, row);

					if (RomFileName != BANK_EMPTY && RomFileName != BANK_RAM)
					{
						if (StringEndsWith(RomFileName, ROM_WRITABLE))
						{
							RomFileName = RomFileName.substr(0, RomFileName.size() - strlen(ROM_WRITABLE));
						}
						else
						{
							RomFileName += ROM_WRITABLE;
						}

						m_RomConfig.SetFileName(m_Model, row, RomFileName);

						UpdateRomField(row);
					}
				}

				LVSetFocus(m_hWndRomList);
				break;

			case IDC_RAM:
				row = ListView_GetSelectionMark(m_hWndRomList);

				if (row >= 1 && row <= 16)
				{
					m_RomConfig.SetFileName(m_Model, row, BANK_RAM);
					UpdateRomField(row);
				}

				LVSetFocus(m_hWndRomList);
				break;

			case IDC_EMPTY:
				row = ListView_GetSelectionMark(m_hWndRomList);

				if (row >= 1 && row <= 16)
				{
					m_RomConfig.SetFileName(m_Model, row, BANK_EMPTY);
					UpdateRomField(row);
				}

				LVSetFocus(m_hWndRomList);
				break;

			case IDC_UP:
				row = ListView_GetSelectionMark(m_hWndRomList);

				if (row >= 2)
				{
					// Swap current row with previous row
					int PreviousRow = row - 1;

					std::string PreviousRowFileName = m_RomConfig.GetFileName(m_Model, PreviousRow);
					std::string CurrentRowFileName = m_RomConfig.GetFileName(m_Model, row);

					m_RomConfig.SetFileName(m_Model, PreviousRow, CurrentRowFileName);
					m_RomConfig.SetFileName(m_Model, row, PreviousRowFileName);

					UpdateRomField(PreviousRow);
					UpdateRomField(row);

					ListView_SetSelectionMark(m_hWndRomList, PreviousRow);
				}

				LVSetFocus(m_hWndRomList);
				break;

			case IDC_DOWN:
				row = ListView_GetSelectionMark(m_hWndRomList);

				if (row > 0)
				{
					const int NextRow = row + 1;
					const int Count = ListView_GetItemCount(m_hWndRomList);

					if (NextRow < Count)
					{
						// Swap current row with next row
						std::string CurrentRowFileName = m_RomConfig.GetFileName(m_Model, row);
						std::string NextRowFileName = m_RomConfig.GetFileName(m_Model, NextRow);

						m_RomConfig.SetFileName(m_Model, row, NextRowFileName);
						m_RomConfig.SetFileName(m_Model, NextRow, CurrentRowFileName);

						UpdateRomField(row);
						UpdateRomField(NextRow);

						ListView_SetSelectionMark(m_hWndRomList, NextRow);
					}

					LVSetFocus(m_hWndRomList);
				}
				break;

			case IDC_SAVE:
				SaveRomConfigFile();
				LVSetFocus(m_hWndRomList);
				break;

			case IDC_LOAD:
				LoadRomConfigFile();
				LVSetFocus(m_hWndRomList);
				break;

			case IDOK:
				EndDialog(m_hWnd, TRUE);
				return TRUE;

			case IDCANCEL:
				EndDialog(m_hWnd, FALSE);
				return TRUE;
			}
			break;
		}
		break;
	}

	return FALSE;
}

/****************************************************************************/
bool RomConfigDialog::LoadRomConfigFile()
{
	char DefaultPath[MAX_PATH];
	char szROMConfigPath[MAX_PATH];
	char *pFileName = szROMConfigPath;
	bool success = false;
	const char* filter = "ROM Config File (*.cfg)\0*.cfg\0";

	szROMConfigPath[0] = 0;
	mainWin->GetDataPath(mainWin->GetUserDataPath(), szROMConfigPath);

	if (szDefaultROMConfigPath[0])
		strcpy(DefaultPath, szDefaultROMConfigPath);
	else
		strcpy(DefaultPath, szROMConfigPath);

	FileDialog fileDialog(m_hWnd, pFileName, MAX_PATH, DefaultPath, filter);
	if (fileDialog.Open())
	{
		// Save directory as default for next time
		unsigned int PathLength = (unsigned int)(strrchr(pFileName, '\\') - pFileName);
		strncpy(szDefaultROMConfigPath, pFileName, PathLength);
		szDefaultROMConfigPath[PathLength] = 0;

		// Read the file
		RomConfigFile LoadedROMCfg;
		if (LoadedROMCfg.Load(pFileName))
		{
			// Copy in loaded config
			m_RomConfig = LoadedROMCfg;
			FillRomList();
			success = true;
		}
	}

	return success;
}

/****************************************************************************/
bool RomConfigDialog::SaveRomConfigFile()
{
	char DefaultPath[MAX_PATH];
	char szROMConfigPath[MAX_PATH];
	char *pFileName = szROMConfigPath;
	bool success = false;
	const char* filter = "ROM Config File (*.cfg)\0*.cfg\0";

	szROMConfigPath[0] = 0;
	mainWin->GetDataPath(mainWin->GetUserDataPath(), szROMConfigPath);

	if (szDefaultROMConfigPath[0])
		strcpy(DefaultPath, szDefaultROMConfigPath);
	else
		strcpy(DefaultPath, szROMConfigPath);

	FileDialog fileDialog(m_hWnd, pFileName, MAX_PATH, DefaultPath, filter);
	if (fileDialog.Save())
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
		if (m_RomConfig.Save(pFileName))
		{
			success = true;
		}
	}

	return success;
}

/****************************************************************************/
static bool GetRomFile(HWND hWnd, char *pFileName)
{
	char DefaultPath[MAX_PATH];
	char szROMPath[MAX_PATH];
	bool success = false;
	const char* filter = "ROM File (*.rom)\0*.rom\0";

	strcpy(szROMPath, "BeebFile");
	mainWin->GetDataPath(mainWin->GetUserDataPath(), szROMPath);
	int nROMPathLen = (int)strlen(szROMPath);

	if (szDefaultROMPath[0])
		strcpy(DefaultPath, szDefaultROMPath);
	else
		strcpy(DefaultPath, szROMPath);

	FileDialog fileDialog(hWnd, pFileName, MAX_PATH, DefaultPath, filter);
	if (fileDialog.Open())
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
