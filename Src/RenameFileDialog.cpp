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

#include <string>
#include <vector>

#include "RenameFileDialog.h"
#include "beebemrc.h"

RenameFileDialog::RenameFileDialog(
	HINSTANCE hInstance,
	HWND hwndParent,
	const char* pszBeebFileName,
	const char* pszHostFileName) :
	m_hInstance(hInstance),
	m_hwndParent(hwndParent),
	m_BeebFileName(pszBeebFileName),
	m_HostFileName(pszHostFileName),
	m_hwnd(nullptr)
{
}

bool RenameFileDialog::DoModal()
{
	// Show export dialog
	int Result = DialogBoxParam(m_hInstance,
		MAKEINTRESOURCE(IDD_RENAME_FILE),
		m_hwndParent,
		sDlgProc,
		reinterpret_cast<LPARAM>(this));

	return Result == IDOK;
}

/****************************************************************************/

INT_PTR RenameFileDialog::DlgProc(
	HWND   hwnd,
	UINT   nMessage,
	WPARAM wParam,
	LPARAM /* lParam */)
{
	switch (nMessage)
	{
	case WM_INITDIALOG: {
		m_hwnd = hwnd;

		SetDlgItemText(m_hwnd, IDC_BEEB_FILE_NAME, m_BeebFileName.c_str());
		SetDlgItemText(m_hwnd, IDC_HOST_FILE_NAME, m_HostFileName.c_str());

		return TRUE;
	}

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK: {
			int Length = GetWindowTextLength(GetDlgItem(m_hwnd, IDC_HOST_FILE_NAME));

			std::vector<char> Text;
			Text.resize(Length + 1);

			GetDlgItemText(m_hwnd, IDC_HOST_FILE_NAME, &Text[0], Text.size());

			m_HostFileName = &Text[0];
			EndDialog(m_hwnd, wParam);
			break;
		}

		case IDCANCEL:
			EndDialog(m_hwnd, wParam);
			return TRUE;
		}
	}

	return FALSE;
}

/****************************************************************************/

INT_PTR CALLBACK RenameFileDialog::sDlgProc(
	HWND   hwnd,
	UINT   nMessage,
	WPARAM wParam,
	LPARAM lParam)
{
	RenameFileDialog* dialog;

	if (nMessage == WM_INITDIALOG)
	{
		SetWindowLongPtr(hwnd, DWLP_USER, lParam);
		dialog = reinterpret_cast<RenameFileDialog*>(lParam);
	}
	else
	{
		dialog = reinterpret_cast<RenameFileDialog*>(
			GetWindowLongPtr(hwnd, DWLP_USER)
		);
	}

	return dialog->DlgProc(hwnd, nMessage, wParam, lParam);
}

const std::string& RenameFileDialog::GetHostFileName() const
{
	return m_HostFileName;
}