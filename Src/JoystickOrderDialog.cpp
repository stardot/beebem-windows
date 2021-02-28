/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2021  Tadeusz Kijkowski

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

#include "JoystickOrderDialog.h"
#include "main.h"
#include "beebemrc.h"
#include "JoystickHandler.h"

#include <Commctrl.h>

class JoystickOrderDialog
{
public:
	HWND m_hwndDlg{ nullptr };
	HWND m_hwndList{ nullptr };
	JoystickHandler* m_handler{ nullptr };
	bool m_showAll{ false };

	void PopulateJoystickList()
	{
		SendMessage(m_hwndList, LB_RESETCONTENT, 0, 0);
		int orderIndex = 0;
		for (auto& order : m_handler->m_JoystickOrder)
		{
			int joyIndex = order.JoyIndex;
			if (joyIndex != -1 || m_showAll)
			{
				int index = static_cast<int>(SendMessage(m_hwndList, LB_ADDSTRING, 0, (LPARAM)order.Name.c_str()));
				SendMessage(m_hwndList, LB_SETITEMDATA, index, (LPARAM)orderIndex);
			}
			++orderIndex;
		}
	}

	void MoveSelected(int delta)
	{
		int listCount = static_cast<int>(SendMessage(m_hwndList, LB_GETCOUNT, 0, 0));
		int curSel = SendMessage(m_hwndList, LB_GETCURSEL, 0, 0);

		if (curSel == LB_ERR || delta == 0)
			return;

		if (delta < -1)
			delta = -1;
		else if (delta > 1)
			delta = 1;

		int other = curSel + delta;
		if (other < 0 || other >= listCount)
			return;

		int curSelIndex = static_cast<int>(SendMessage(m_hwndList, LB_GETITEMDATA, curSel, 0));
		int otherIndex = static_cast<int>(SendMessage(m_hwndList, LB_GETITEMDATA, other, 0));

		JoystickOrderEntry& curEntry = m_handler->m_JoystickOrder[curSelIndex];
		JoystickOrderEntry& otherEntry = m_handler->m_JoystickOrder[otherIndex];
		std::swap(curEntry, otherEntry);

		if (curEntry.JoyIndex != -1 && otherEntry.JoyIndex != -1)
		{
			std::swap(m_handler->m_JoystickDevs[curEntry.JoyIndex], m_handler->m_JoystickDevs[otherEntry.JoyIndex]);
			std::swap(curEntry.JoyIndex, otherEntry.JoyIndex);
		}

		if (curSel < other)
		{
			SendMessage(m_hwndList, LB_DELETESTRING, other, 0);
			SendMessage(m_hwndList, LB_DELETESTRING, curSel, 0);

			SendMessage(m_hwndList, LB_INSERTSTRING, curSel, (LPARAM)curEntry.Name.c_str());
			SendMessage(m_hwndList, LB_SETITEMDATA, curSel, (LPARAM)curSelIndex);

			SendMessage(m_hwndList, LB_INSERTSTRING, other, (LPARAM)otherEntry.Name.c_str());
			SendMessage(m_hwndList, LB_SETITEMDATA, other, (LPARAM)otherIndex);
		}
		else
		{
			SendMessage(m_hwndList, LB_DELETESTRING, curSel, 0);
			SendMessage(m_hwndList, LB_DELETESTRING, other, 0);

			SendMessage(m_hwndList, LB_INSERTSTRING, other, (LPARAM)otherEntry.Name.c_str());
			SendMessage(m_hwndList, LB_SETITEMDATA, other, (LPARAM)otherIndex);

			SendMessage(m_hwndList, LB_INSERTSTRING, curSel, (LPARAM)curEntry.Name.c_str());
			SendMessage(m_hwndList, LB_SETITEMDATA, curSel, (LPARAM)curSelIndex);
		}

		SendMessage(m_hwndList, LB_SETCURSEL, other, 0);
	}

	INT_PTR DlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
	{
		switch (message)
		{
		case WM_INITDIALOG:
			m_hwndDlg = hwndDlg;
			m_hwndList = GetDlgItem(hwndDlg, IDC_JOYSTICKLIST);
			PopulateJoystickList();
			return TRUE;

		case WM_NOTIFY:
			if (wParam == IDC_JOYSTICKSPIN)
			{
				LPNMUPDOWN lpnmud = (LPNMUPDOWN)lParam;
				if (lpnmud->hdr.code == UDN_DELTAPOS)
				{
					MoveSelected(lpnmud->iDelta);
					lpnmud->iDelta = 0;
					return TRUE;
				}
			}

		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
			case IDOK:
			case IDCANCEL:
				EndDialog(hwndDlg, wParam);
				return TRUE;
			case IDC_JOYSTICKSHOWALL:
				m_showAll = IsDlgButtonChecked(hwndDlg, IDC_JOYSTICKSHOWALL);
				PopulateJoystickList();
				return TRUE;
			}
		}
		return FALSE;
	}

	JoystickOrderDialog(JoystickHandler* handler) : m_handler(handler) {};
};

INT_PTR CALLBACK JoystickOrderDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam)
{
	if (message == WM_INITDIALOG)
		SetWindowLongPtr(hwndDlg, DWLP_USER, lParam);

	JoystickOrderDialog* dlg = reinterpret_cast<JoystickOrderDialog*>(GetWindowLongPtr(hwndDlg, DWLP_USER));
	if (!dlg)
		return FALSE;
	return dlg->DlgProc(hwndDlg, message, wParam, lParam);
}

void ShowJoystickOrderDialog(HWND hwndParent, JoystickHandler* handler)
{
	JoystickOrderDialog dlg{ handler };
	DialogBoxParam(hInst, MAKEINTRESOURCE(IDD_JOYSTICKORDER), hwndParent, JoystickOrderDlgProc, reinterpret_cast<LPARAM>(&dlg));
}
