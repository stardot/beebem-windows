/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2021  Chris Needham

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

#ifndef ROM_CONFIG_DIALOG_HEADER
#define ROM_CONFIG_DIALOG_HEADER

#include "RomConfigFile.h"
#include <windows.h>

class RomConfigDialog
{
	public:
		RomConfigDialog(
			HINSTANCE hInstance,
			HWND hwndParent,
			Model model,
			const RomConfigFile& Config
		);

		bool DoModal();

		const RomConfigFile& GetRomConfig() const;

	private:
		static INT_PTR CALLBACK sDlgProc(
			HWND   hwnd,
			UINT   nMessage,
			WPARAM wParam,
			LPARAM lParam
		);

		INT_PTR DlgProc(
			HWND   hwnd,
			UINT   nMessage,
			WPARAM wParam,
			LPARAM lParam
		);

		void LVSetFocus(HWND hWnd);
		void FillModelList();
		void FillRomList();
		void UpdateRomField(int row);

		bool LoadRomConfigFile();
		bool SaveRomConfigFile();

	private:
		HINSTANCE m_hInstance;
		HWND m_hWnd;
		HWND m_hWndParent;
		HWND m_hWndRomList;
		Model m_Model;
		RomConfigFile m_RomConfig;
};

#endif
