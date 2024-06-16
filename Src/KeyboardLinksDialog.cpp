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

#include "KeyboardLinksDialog.h"
#include "Resource.h"

/****************************************************************************/

KeyboardLinksDialog::KeyboardLinksDialog(HINSTANCE hInstance,
                                         HWND hwndParent,
                                         unsigned char Value) :
	Dialog(hInstance, hwndParent, IDD_KEYBOARD_LINKS),
	m_Value(Value)
{
}

/****************************************************************************/

static const UINT nControlIDs[] = {
	IDC_KEYBOARD_BIT0,
	IDC_KEYBOARD_BIT1,
	IDC_KEYBOARD_BIT2,
	IDC_KEYBOARD_BIT3,
	IDC_KEYBOARD_BIT4,
	IDC_KEYBOARD_BIT5,
	IDC_KEYBOARD_BIT6,
	IDC_KEYBOARD_BIT7
};

INT_PTR KeyboardLinksDialog::DlgProc(UINT   nMessage,
                                     WPARAM wParam,
                                     LPARAM /* lParam */)
{
	switch (nMessage)
	{
		case WM_INITDIALOG:
			for (int i = 0; i < 8; i++)
			{
				SetDlgItemChecked(nControlIDs[i], (m_Value & (1 << i)) != 0);
			}

			return TRUE;

		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case IDOK:
					m_Value = 0;

					for (int i = 0; i < 8; i++)
					{
						if (IsDlgItemChecked(nControlIDs[i]))
						{
							m_Value |= 1 << i;
						}
					}

					EndDialog(m_hwnd, wParam);
					return TRUE;

				case IDCANCEL:
					EndDialog(m_hwnd, wParam);
					return TRUE;
			}
			break;
	}

	return FALSE;
}

/****************************************************************************/

unsigned char KeyboardLinksDialog::GetValue() const
{
	return m_Value;
}
