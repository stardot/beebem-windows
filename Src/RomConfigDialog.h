/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2009 Mike Wyatt
Copyright (C) 2024 Chris Needham

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

#include "Dialog.h"
#include "Model.h"
#include "RomConfigFile.h"

class RomConfigDialog : public Dialog
{
	public:
		RomConfigDialog(
			HINSTANCE hInstance,
			HWND hwndParent,
			const RomConfigFile& Config
		);

	public:
		const RomConfigFile& GetRomConfig() const;

	private:
		virtual INT_PTR DlgProc(
			UINT   nMessage,
			WPARAM wParam,
			LPARAM lParam
		);

		void UpdateROMField(int Row);
		void FillModelList();
		void FillROMList();
		bool LoadROMConfigFile();
		bool SaveROMConfigFile();
		bool GetROMFile(char *pszFileName);

	private:
		HWND m_hWndROMList;
		HWND m_hWndModel;
		RomConfigFile m_RomConfig;
		Model m_Model;
};

#endif
