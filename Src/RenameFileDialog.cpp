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

#include <windows.h>

#include <string>
#include <vector>

#include "RenameFileDialog.h"
#include "beebemrc.h"

/****************************************************************************/

RenameFileDialog::RenameFileDialog(HINSTANCE hInstance,
                                   HWND hwndParent,
                                   const char* pszBeebFileName,
                                   const char* pszHostFileName) :
	Dialog(hInstance, hwndParent, IDD_RENAME_FILE),
	m_BeebFileName(pszBeebFileName),
	m_HostFileName(pszHostFileName)
{
}

/****************************************************************************/

INT_PTR RenameFileDialog::DlgProc(
	UINT   nMessage,
	WPARAM wParam,
	LPARAM /* lParam */)
{
	switch (nMessage)
	{
	case WM_INITDIALOG:
		SetDlgItemText(IDC_BEEB_FILE_NAME, m_BeebFileName.c_str());
		SetDlgItemText(IDC_HOST_FILE_NAME, m_HostFileName.c_str());

		return TRUE;

	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			GetDlgItemText(IDC_HOST_FILE_NAME);

			m_HostFileName = GetDlgItemText(IDC_HOST_FILE_NAME);
			EndDialog(m_hwnd, wParam);
			break;

		case IDCANCEL:
			EndDialog(m_hwnd, wParam);
			return TRUE;
		}
	}

	return FALSE;
}

/****************************************************************************/

const std::string& RenameFileDialog::GetHostFileName() const
{
	return m_HostFileName;
}
