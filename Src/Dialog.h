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

#ifndef DIALOG_HEADER
#define DIALOG_HEADER

#include <string>

class Dialog
{
	public:
		Dialog(
			HINSTANCE hInstance,
			HWND hwndParent,
			int DialogID
		);

	public:
		bool DoModal();

	private:
		static INT_PTR CALLBACK sDlgProc(
			HWND   hwnd,
			UINT   nMessage,
			WPARAM wParam,
			LPARAM lParam
		);

		virtual INT_PTR DlgProc(
			UINT   nMessage,
			WPARAM wParam,
			LPARAM lParam
		) = 0;

	protected:
		std::string GetDlgItemText(int nID);
		void SetDlgItemText(int nID, const std::string& str);
		bool IsDlgItemChecked(int nID);
		void SetDlgItemChecked(int nID, bool bChecked);
		void SetDlgItemFocus(int nID);
		void EnableDlgItem(int nID, bool bEnable);

	protected:
		HINSTANCE m_hInstance;
		HWND m_hwndParent;
		int m_DialogID;
		HWND m_hwnd;
};

void CenterDialog(HWND hWndParent, HWND hWnd);

#endif
