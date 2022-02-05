/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2020  Chris Needham

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

#ifndef SELECT_KEY_DIALOG_HEADER
#define SELECT_KEY_DIALOG_HEADER

#include <windows.h>

class SelectKeyDialog
{
	public:
		SelectKeyDialog(
			HINSTANCE hInstance,
			HWND hwndParent,
			const std::string& Title,
			const std::string& SelectedKey,
			bool EnableShift = true
		);

		bool Open();
		void Close(UINT nResultID);

		bool HandleMessage(const MSG& msg);

		int Key() const;

		static LPCSTR KeyName(int Key);

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

	private:
		HINSTANCE m_hInstance;
		HWND m_hwnd;
		HWND m_hwndParent;
		std::string m_Title;
		std::string m_SelectedKey;
		bool m_EnableShift;
		int m_Key;
		bool m_Shift;
};

extern SelectKeyDialog* selectKeyDialog;

#endif
