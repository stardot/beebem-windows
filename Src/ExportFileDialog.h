/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2023 Chris Needham

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

#ifndef EXPORT_FILE_DIALOG_HEADER
#define EXPORT_FILE_DIALOG_HEADER

#include <string>
#include <vector>

#include "Dialog.h"
#include "DiscEdit.h"

struct FileExportEntry
{
	DFS_FILE_ATTR DfsAttrs;
	std::string BeebFileName;
	std::string HostFileName;
};

class ExportFileDialog : public Dialog
{
	public:
		ExportFileDialog(
			HINSTANCE hInstance,
			HWND hwndParent,
			const char* szDiscFile,
			int NumSides,
			int Side,
			DFS_DISC_CATALOGUE* dfsCat,
			const char* ExportPath
		);

	public:
		std::string GetPath() const;

	private:
		void ExportSelectedFiles();
		bool ExportFile(DFS_FILE_ATTR* DfsAttrs, const char* LocalFileName);

	private:
		virtual INT_PTR DlgProc(
			UINT   nMessage,
			WPARAM wParam,
			LPARAM lParam
		);

	private:
		const char* m_DiscFile;
		int m_NumSides;
		int m_Side;
		std::vector<FileExportEntry> m_ExportFiles;
		std::string m_ExportPath;
		HWND m_hwndListView;
		int m_FilesSelected[DFS_MAX_CAT_SIZE];
		int m_NumSelected;
};

#endif
