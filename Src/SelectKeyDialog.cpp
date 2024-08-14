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

// User defined keyboard functionality.

#include <windows.h>

#include <string>

#include "SelectKeyDialog.h"
#include "KeyMap.h"
#include "Main.h"
#include "Messages.h"
#include "Resource.h"
#include "WindowUtils.h"

/****************************************************************************/

static bool IsDlgItemChecked(HWND hDlg, int nIDDlgItem);

SelectKeyDialog* selectKeyDialog;

/****************************************************************************/

SelectKeyDialog::SelectKeyDialog(
	HINSTANCE hInstance,
	HWND hwndParent,
	const std::string& Title,
	const std::string& SelectedKey,
	bool BeebKey,
	int Row,
	int Column,
	bool DoingShifted) :
	m_hInstance(hInstance),
	m_hwnd(nullptr),
	m_hwndParent(hwndParent),
	m_Title(Title),
	m_SelectedKey(SelectedKey),
	m_BeebKey(BeebKey),
	m_Key(-1),
	m_Row(Row),
	m_Column(Column),
	m_DoingShifted(DoingShifted),
	m_Shift(false)
{
}

/****************************************************************************/

bool SelectKeyDialog::Open()
{
	// Create modeless dialog box
	m_hwnd = CreateDialogParam(
		m_hInstance,
		MAKEINTRESOURCE(IDD_SELECT_KEY),
		m_hwndParent,
		sDlgProc,
		reinterpret_cast<LPARAM>(this)
	);

	if (m_hwnd != nullptr)
	{
		DisableRoundedCorners(m_hwnd);

		EnableWindow(m_hwndParent, FALSE);

		return true;
	}

	return false;
}

/****************************************************************************/

void SelectKeyDialog::Close(UINT nResultID)
{
	EnableWindow(m_hwndParent, TRUE);
	DestroyWindow(m_hwnd);
	m_hwnd = nullptr;

	PostMessage(m_hwndParent, WM_SELECT_KEY_DIALOG_CLOSED, nResultID, 0);
}

/****************************************************************************/

INT_PTR SelectKeyDialog::DlgProc(
	HWND   hwnd,
	UINT   nMessage,
	WPARAM wParam,
	LPARAM /* lParam */)
{
	switch (nMessage)
	{
	case WM_INITDIALOG:
		m_hwnd = hwnd;

		SetWindowText(m_hwnd, m_Title.c_str());

		SetDlgItemText(m_hwnd, IDC_ASSIGNED_KEYS, m_SelectedKey.c_str());

		if (!m_BeebKey)
		{
			ShowWindow(GetDlgItem(m_hwnd, IDC_SHIFT), SW_HIDE);
		}
		return TRUE;

	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE)
		{
			hCurrentDialog = nullptr;
		}
		else
		{
			hCurrentDialog = m_hwnd;
			hCurrentAccelTable = nullptr;
		}
		break;

	case WM_SYSCOMMAND:
		if (wParam == SC_CLOSE)
		{
			Close(IDCANCEL);
			return TRUE;
		}
		break;

	case WM_COMMAND:
		switch (wParam)
		{
		case IDC_CLEAR:
			SendMessage(m_hwndParent, WM_CLEAR_KEY_MAPPING, 0, 0);

			if (m_BeebKey)
			{
				m_SelectedKey = GetKeysUsed(m_Row, m_Column, m_DoingShifted);
			}
			else
			{
				m_SelectedKey = "";
			}

			SetDlgItemText(m_hwnd, IDC_ASSIGNED_KEYS, m_SelectedKey.c_str());
			return TRUE;

		case IDOK:
			Close(IDCONTINUE);
			return TRUE;
		}
		break;
	}

	return FALSE;
}

/****************************************************************************/

INT_PTR CALLBACK SelectKeyDialog::sDlgProc(
	HWND   hwnd,
	UINT   nMessage,
	WPARAM wParam,
	LPARAM lParam)
{
	SelectKeyDialog* dialog;

	if (nMessage == WM_INITDIALOG)
	{
		SetWindowLongPtr(hwnd, DWLP_USER, lParam);
		dialog = reinterpret_cast<SelectKeyDialog*>(lParam);
	}
	else
	{
		dialog = reinterpret_cast<SelectKeyDialog*>(
			GetWindowLongPtr(hwnd, DWLP_USER)
		);
	}

	if (dialog)
	{
		return dialog->DlgProc(hwnd, nMessage, wParam, lParam);
	}
	else
	{
		return FALSE;
	}
}

/****************************************************************************/

bool SelectKeyDialog::HandleMessage(const MSG& msg)
{
	if (msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN)
	{
		m_Key = (int)msg.wParam;

		m_Shift = IsDlgItemChecked(m_hwnd, IDC_SHIFT);

		Close(IDOK);
		return true;
	}

	return false;
}

/****************************************************************************/

int SelectKeyDialog::Key() const
{
	return m_Key;
}

/****************************************************************************/

bool SelectKeyDialog::Shift() const
{
	return m_Shift;
}

/****************************************************************************/

static bool IsDlgItemChecked(HWND hDlg, int nIDDlgItem)
{
	return SendDlgItemMessage(hDlg, nIDDlgItem, BM_GETCHECK, 0, 0) == BST_CHECKED;
}
