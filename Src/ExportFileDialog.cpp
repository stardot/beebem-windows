/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2023  Chris Needham

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

#pragma warning(push)
#pragma warning(disable: 4091) // ignored on left of 'tagGPFIDL_FLAGS' when no variable is declared
#include <shlobj.h>
#pragma warning(pop)

#include <string>

#include "ExportFileDialog.h"
#include "DebugTrace.h"
#include "DiscEdit.h"
#include "FileDialog.h"
#include "FileUtils.h"
#include "FolderSelectDialog.h"
#include "ListView.h"
#include "Main.h"
#include "RenameFileDialog.h"
#include "Resource.h"
#include "StringUtils.h"

/****************************************************************************/

ExportFileDialog::ExportFileDialog(HINSTANCE hInstance,
                                   HWND hwndParent,
                                   const char* szDiscFile,
                                   int NumSides,
                                   int Side,
                                   DFS_DISC_CATALOGUE* dfsCat,
                                   const char* ExportPath) :
	Dialog(hInstance, hwndParent, IDD_DISCEXPORT),
	m_DiscFile(szDiscFile),
	m_NumSides(NumSides),
	m_Side(Side),
	m_ExportPath(ExportPath),
	m_hwndListView(nullptr),
	m_NumSelected(0)
{
	for (int i = 0; i < dfsCat->numFiles; ++i)
	{
		FileExportEntry Entry;
		Entry.DfsAttrs = dfsCat->fileAttrs[i];
		Entry.BeebFileName = std::string(1, dfsCat->fileAttrs[i].directory) + "." + dfsCat->fileAttrs[i].filename;
		Entry.HostFileName = BeebToLocalFileName(Entry.BeebFileName);

		m_ExportFiles.push_back(Entry);
	}
}

/****************************************************************************/

static LPCTSTR Columns[] = {
	"File name",
	"Load Addr",
	"Exec Addr",
	"Length",
	"Host File name"
};

INT_PTR ExportFileDialog::DlgProc(UINT   nMessage,
                                  WPARAM wParam,
                                  LPARAM lParam)
{
	switch (nMessage)
	{
	case WM_INITDIALOG: {
		m_hwndListView = GetDlgItem(m_hwnd, IDC_EXPORTFILELIST);

		ListView_SetExtendedListViewStyle(m_hwndListView, LVS_EX_FULLROWSELECT | LVS_EX_GRIDLINES);

		for (int i = 0; i < _countof(Columns); ++i)
		{
			LVInsertColumn(m_hwndListView, i, Columns[i], LVCFMT_LEFT, 50);
		}

		int Row = 0;

		for (const FileExportEntry& Entry : m_ExportFiles)
		{
			// List is sorted so store catalogue index in list's item data
			LVInsertItem(m_hwndListView,
			             Row,
			             0,
			             const_cast<LPTSTR>(Entry.BeebFileName.c_str()),
			             reinterpret_cast<LPARAM>(&Entry));

			char str[100];
			sprintf(str, "%06X", Entry.DfsAttrs.loadAddr & 0xffffff);
			LVSetItemText(m_hwndListView, Row, 1, str);

			sprintf(str, "%06X", Entry.DfsAttrs.execAddr & 0xffffff);
			LVSetItemText(m_hwndListView, Row, 2, str);

			sprintf(str, "%06X", Entry.DfsAttrs.length);
			LVSetItemText(m_hwndListView, Row, 3, str);

			LVSetItemText(m_hwndListView, Row, 4, const_cast<LPTSTR>(Entry.HostFileName.c_str()));

			Row++;
		}

		for (int i = 0; i < _countof(Columns); ++i)
		{
			ListView_SetColumnWidth(m_hwndListView, i, LVSCW_AUTOSIZE_USEHEADER);
		}

		return TRUE;
	}

	case WM_NOTIFY: {
		LPNMHDR nmhdr = (LPNMHDR)lParam;

		if (nmhdr->hwndFrom == m_hwndListView && nmhdr->code == NM_DBLCLK)
		{
			LPNMITEMACTIVATE pItemActivate = (LPNMITEMACTIVATE)lParam;

			LVHITTESTINFO HitTestInfo = { 0 };
			HitTestInfo.pt = pItemActivate->ptAction;
			ListView_SubItemHitTest(m_hwndListView, &HitTestInfo);

			FileExportEntry* Entry = reinterpret_cast<FileExportEntry*>(
				LVGetItemData(m_hwndListView, HitTestInfo.iItem)
			);

			if (Entry)
			{
				RenameFileDialog Dialog(hInst,
				                        m_hwnd,
				                        Entry->BeebFileName.c_str(),
				                        Entry->HostFileName.c_str());

				if (Dialog.DoModal())
				{
					Entry->HostFileName = Dialog.GetHostFileName();

					LVSetItemText(m_hwndListView, HitTestInfo.iItem, 4, const_cast<LPTSTR>(Entry->HostFileName.c_str()));
				}
			}
		}
		break;
	}

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			ExportSelectedFiles();
			return TRUE;

		case IDCANCEL:
			EndDialog(m_hwnd, wParam);
			return TRUE;
		}
	}

	return FALSE;
}

/****************************************************************************/

std::string ExportFileDialog::GetPath() const
{
	return m_ExportPath;
}

/****************************************************************************/

void ExportFileDialog::ExportSelectedFiles()
{
	m_NumSelected = ListView_GetSelectedCount(m_hwndListView);

	if (m_NumSelected == 0)
	{
		int Count = ListView_GetItemCount(m_hwndListView);

		for (int i = 0; i < Count; i++)
		{
			ListView_SetItemState(m_hwndListView, i, LVIS_SELECTED, LVIS_SELECTED);
		}

		m_NumSelected = Count;
	}

	// Get folder to export to
	FolderSelectDialog Dialog(m_hwnd,
	                          "Select folder for exported files:",
	                          m_ExportPath.c_str());

	FolderSelectDialog::Result Result = Dialog.DoModal();

	switch (Result)
	{
		case FolderSelectDialog::Result::OK:
			m_ExportPath = Dialog.GetFolder();
			break;

		case FolderSelectDialog::Result::InvalidFolder:
			mainWin->Report(MessageType::Warning, "Invalid folder selected");
			return;

		case FolderSelectDialog::Result::Cancel:
		default:
			return;
	}

	int Item = ListView_GetNextItem(m_hwndListView, -1, LVNI_SELECTED);

	int Count = 0;

	while (Item != -1)
	{
		FileExportEntry* Entry = reinterpret_cast<FileExportEntry*>(
			LVGetItemData(m_hwndListView, Item)
		);

		std::string LocalFileName = AppendPath(m_ExportPath, Entry->HostFileName);

		if (FileExists(LocalFileName.c_str()))
		{
			char FileName[MAX_PATH];
			const char* Filter = "All Files (*.*)\0*.*\0";

			strcpy(FileName, Entry->HostFileName.c_str());

			FileDialog fileDialog(m_hwnd, FileName, sizeof(FileName), m_ExportPath.c_str(), Filter);

			if (fileDialog.Save())
			{
				if (ExportFile(&Entry->DfsAttrs, FileName))
				{
					Count++;
				}
			}
		}
		else
		{
			if (ExportFile(&Entry->DfsAttrs, LocalFileName.c_str()))
			{
				Count++;
			}
		}

		Item = ListView_GetNextItem(m_hwndListView, Item, LVNI_SELECTED);
	}

	mainWin->Report(MessageType::Info, "Files successfully exported: %d", Count);
}

/****************************************************************************/

bool ExportFileDialog::ExportFile(DFS_FILE_ATTR* DfsAttrs, const char* LocalFileName)
{
	char szErrStr[512];

	if (dfs_export_file(m_DiscFile, m_NumSides, m_Side, DfsAttrs, LocalFileName, szErrStr))
	{
		return true;
	}
	else
	{
		mainWin->Report(MessageType::Error, szErrStr);
		return false;
	}
}
