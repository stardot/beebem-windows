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

#include "AboutDialog.h"
#include "Resource.h"
#include "Version.h"

/****************************************************************************/

AboutDialog::AboutDialog(HINSTANCE hInstance, HWND hwndParent) :
	Dialog(hInstance, hwndParent, IDD_ABOUT)
{
}

/****************************************************************************/

INT_PTR AboutDialog::DlgProc(UINT nMessage, WPARAM wParam, LPARAM /* lParam */)
{
	switch (nMessage)
	{
		case WM_INITDIALOG: {
			char szVersion[256];
			sprintf(szVersion, "Version %s, %s", VERSION_STRING, VERSION_DATE);

			SetDlgItemText(IDC_VERSION, szVersion);
			SetDlgItemText(IDC_COPYRIGHT, VERSION_COPYRIGHT);

			return TRUE;
		}

		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case IDOK:
				case IDCANCEL:
					EndDialog(m_hwnd, wParam);
					return TRUE;
			}
	}

	return FALSE;
}

