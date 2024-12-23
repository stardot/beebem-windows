/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 1994  David Alan Gilbert
Copyright (C) 1997  Mike Wyatt
Copyright (C) 2004  Ken Lowe

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

#include "UserPortBreakoutBox.h"
#include "KeyMap.h"
#include "Main.h"
#include "Messages.h"
#include "Resource.h"
#include "SelectKeyDialog.h"
#include "UserVia.h"
#include "WindowUtils.h"

/****************************************************************************/

/* User Port Breakout Box */

UserPortBreakoutDialog* userPortBreakoutDialog = nullptr;

int BitKeys[8] = { 48, 49, 50, 51, 52, 53, 54, 55 };

static const UINT BitKeyButtonIDs[8] = {
	IDK_BIT0, IDK_BIT1, IDK_BIT2, IDK_BIT3,
	IDK_BIT4, IDK_BIT5, IDK_BIT6, IDK_BIT7,
};

/****************************************************************************/

UserPortBreakoutDialog::UserPortBreakoutDialog(
	HINSTANCE hInstance,
	HWND hwndParent) :
	m_hInstance(hInstance),
	m_hwnd(nullptr),
	m_hwndParent(hwndParent),
	m_BitKey(0),
	m_LastInputData(0),
	m_LastOutputData(0)
{
}

/****************************************************************************/

bool UserPortBreakoutDialog::Open()
{
	if (m_hwnd != nullptr)
	{
		// The dialog box is already open.
		return false;
	}

	m_hwnd = CreateDialogParam(
		m_hInstance,
		MAKEINTRESOURCE(IDD_BREAKOUT),
		m_hwndParent,
		sDlgProc,
		reinterpret_cast<LPARAM>(this)
	);

	if (m_hwnd != nullptr)
	{
		DisableRoundedCorners(m_hwnd);

		hCurrentDialog = m_hwnd;
		return true;
	}

	return false;
}

/****************************************************************************/

void UserPortBreakoutDialog::Close()
{
	if (m_hwnd == nullptr)
	{
		return;
	}

	if (selectKeyDialog != nullptr)
	{
		selectKeyDialog->Close(IDCANCEL);
	}

	EnableWindow(m_hwndParent, TRUE);
	DestroyWindow(m_hwnd);
	m_hwnd = nullptr;
	hCurrentDialog = nullptr;

	PostMessage(m_hwndParent, WM_USER_PORT_BREAKOUT_DIALOG_CLOSED, 0, 0);
}

/****************************************************************************/

bool UserPortBreakoutDialog::KeyDown(int Key)
{
	int mask = 0x01;
	bool bit = false;

	for (int i = 0; i < 8; ++i)
	{
		if (BitKeys[i] == Key)
		{
			if ((UserVIAState.ddrb & mask) == 0x00)
			{
				UserVIAState.irb &= ~mask;
				ShowInputs((UserVIAState.orb & UserVIAState.ddrb) | (UserVIAState.irb & (~UserVIAState.ddrb)));
				bit = true;
			}
		}
		mask <<= 1;
	}

	return bit;
}

/****************************************************************************/

bool UserPortBreakoutDialog::KeyUp(int Key)
{
	int mask = 0x01;
	bool bit = false;

	for (int i = 0; i < 8; ++i)
	{
		if (BitKeys[i] == Key)
		{
			if ((UserVIAState.ddrb & mask) == 0x00)
			{
				UserVIAState.irb |= mask;
				ShowInputs((UserVIAState.orb & UserVIAState.ddrb) | (UserVIAState.irb & (~UserVIAState.ddrb)));
				bit = true;
			}
		}
		mask <<= 1;
	}

	return bit;
}

/****************************************************************************/

INT_PTR CALLBACK UserPortBreakoutDialog::sDlgProc(
	HWND   hwnd,
	UINT   nMessage,
	WPARAM wParam,
	LPARAM lParam)
{
	UserPortBreakoutDialog* dialog;

	if (nMessage == WM_INITDIALOG)
	{
		SetWindowLongPtr(hwnd, DWLP_USER, lParam);
		dialog = reinterpret_cast<UserPortBreakoutDialog*>(lParam);
	}
	else
	{
		dialog = reinterpret_cast<UserPortBreakoutDialog*>(
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

INT_PTR UserPortBreakoutDialog::DlgProc(
	HWND   /* hwnd */,
	UINT   nMessage,
	WPARAM wParam,
	LPARAM /* lParam */)
{
	bool bit;

	switch (nMessage)
	{
	case WM_INITDIALOG:
		for (size_t i = 0; i < _countof(BitKeyButtonIDs); i++)
		{
			ShowBitKey(i, BitKeyButtonIDs[i]);
		}
		return TRUE;

	case WM_COMMAND:
		switch (wParam)
		{
		case IDOK:
		case IDCANCEL:
			Close();
			return TRUE;

		case IDK_BIT0:
			PromptForBitKeyInput(0);
			break;

		case IDK_BIT1:
			PromptForBitKeyInput(1);
			break;

		case IDK_BIT2:
			PromptForBitKeyInput(2);
			break;

		case IDK_BIT3:
			PromptForBitKeyInput(3);
			break;

		case IDK_BIT4:
			PromptForBitKeyInput(4);
			break;

		case IDK_BIT5:
			PromptForBitKeyInput(5);
			break;

		case IDK_BIT6:
			PromptForBitKeyInput(6);
			break;

		case IDK_BIT7:
			PromptForBitKeyInput(7);
			break;

		case IDC_IB7:
			bit = GetValue((int)wParam);
			if ((UserVIAState.ddrb & 0x80) == 0x00)
			{
				if (bit) UserVIAState.irb &= ~0x80; else UserVIAState.irb |= 0x80;
			}
			break;

		case IDC_IB6:
			bit = GetValue((int)wParam);
			if ((UserVIAState.ddrb & 0x40) == 0x00)
			{
				if (bit) UserVIAState.irb &= ~0x40; else UserVIAState.irb |= 0x40;
			}
			break;

		case IDC_IB5:
			bit = GetValue((int)wParam);
			if ((UserVIAState.ddrb & 0x20) == 0x00)
			{
				if (bit) UserVIAState.irb &= ~0x20; else UserVIAState.irb |= 0x20;
			}
			break;

		case IDC_IB4:
			bit = GetValue((int)wParam);
			if ((UserVIAState.ddrb & 0x10) == 0x00)
			{
				if (bit) UserVIAState.irb &= ~0x10; else UserVIAState.irb |= 0x10;
			}
			break;

		case IDC_IB3:
			bit = GetValue((int)wParam);
			if ((UserVIAState.ddrb & 0x08) == 0x00)
			{
				if (bit) UserVIAState.irb &= ~0x08; else UserVIAState.irb |= 0x08;
			}
			break;

		case IDC_IB2:
			bit = GetValue((int)wParam);
			if ((UserVIAState.ddrb & 0x04) == 0x00)
			{
				if (bit) UserVIAState.irb &= ~0x04; else UserVIAState.irb |= 0x04;
			}
			break;

		case IDC_IB1:
			bit = GetValue((int)wParam);
			if ((UserVIAState.ddrb & 0x02) == 0x00)
			{
				if (bit) UserVIAState.irb &= ~0x02; else UserVIAState.irb |= 0x02;
			}
			break;

		case IDC_IB0:
			bit = GetValue((int)wParam);
			if ((UserVIAState.ddrb & 0x01) == 0x00)
			{
				if (bit) UserVIAState.irb &= ~0x01; else UserVIAState.irb |= 0x01;
			}
			break;
		}
		return TRUE;

	case WM_CLEAR_KEY_MAPPING:
		BitKeys[m_BitKey] = 0;
		ShowBitKey(m_BitKey, BitKeyButtonIDs[m_BitKey]);
		break;

	case WM_SELECT_KEY_DIALOG_CLOSED:
		if (wParam == IDOK)
		{
			// Assign the BBC key to the PC key.
			BitKeys[m_BitKey] = selectKeyDialog->Key();
			ShowBitKey(m_BitKey, BitKeyButtonIDs[m_BitKey]);
		}

		delete selectKeyDialog;
		selectKeyDialog = nullptr;

		return TRUE;
	}

	return FALSE;
}

/****************************************************************************/

bool UserPortBreakoutDialog::GetValue(int ctrlID)
{
	return SendDlgItemMessage(m_hwnd, ctrlID, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

/****************************************************************************/

void UserPortBreakoutDialog::SetValue(int ctrlID, bool State)
{
	SendDlgItemMessage(m_hwnd, ctrlID, BM_SETCHECK, State ? 1 : 0, 0);
}

/****************************************************************************/

void UserPortBreakoutDialog::ShowOutputs(unsigned char data)
{
	if (m_hwnd != nullptr)
	{
		if (data != m_LastOutputData)
		{
			unsigned char changed_bits = data ^ m_LastOutputData;
			if (changed_bits & 0x80) { if ((UserVIAState.ddrb & 0x80) == 0x80) SetValue(IDC_OB7, (data & 0x80) != 0); else SetValue(IDC_OB7, 0); }
			if (changed_bits & 0x40) { if ((UserVIAState.ddrb & 0x40) == 0x40) SetValue(IDC_OB6, (data & 0x40) != 0); else SetValue(IDC_OB6, 0); }
			if (changed_bits & 0x20) { if ((UserVIAState.ddrb & 0x20) == 0x20) SetValue(IDC_OB5, (data & 0x20) != 0); else SetValue(IDC_OB5, 0); }
			if (changed_bits & 0x10) { if ((UserVIAState.ddrb & 0x10) == 0x10) SetValue(IDC_OB4, (data & 0x10) != 0); else SetValue(IDC_OB4, 0); }
			if (changed_bits & 0x08) { if ((UserVIAState.ddrb & 0x08) == 0x08) SetValue(IDC_OB3, (data & 0x08) != 0); else SetValue(IDC_OB3, 0); }
			if (changed_bits & 0x04) { if ((UserVIAState.ddrb & 0x04) == 0x04) SetValue(IDC_OB2, (data & 0x04) != 0); else SetValue(IDC_OB2, 0); }
			if (changed_bits & 0x02) { if ((UserVIAState.ddrb & 0x02) == 0x02) SetValue(IDC_OB1, (data & 0x02) != 0); else SetValue(IDC_OB1, 0); }
			if (changed_bits & 0x01) { if ((UserVIAState.ddrb & 0x01) == 0x01) SetValue(IDC_OB0, (data & 0x01) != 0); else SetValue(IDC_OB0, 0); }
			m_LastOutputData = data;
		}
	}
}

/****************************************************************************/

void UserPortBreakoutDialog::ShowInputs(unsigned char data)
{
	if (m_hwnd != nullptr)
	{
		if (data != m_LastInputData)
		{
			unsigned char changed_bits = data ^ m_LastInputData;
			if (changed_bits & 0x80) { if ((UserVIAState.ddrb & 0x80) == 0x00) SetValue(IDC_IB7, (data & 0x80) == 0); else SetValue(IDC_IB7, false); }
			if (changed_bits & 0x40) { if ((UserVIAState.ddrb & 0x40) == 0x00) SetValue(IDC_IB6, (data & 0x40) == 0); else SetValue(IDC_IB6, false); }
			if (changed_bits & 0x20) { if ((UserVIAState.ddrb & 0x20) == 0x00) SetValue(IDC_IB5, (data & 0x20) == 0); else SetValue(IDC_IB5, false); }
			if (changed_bits & 0x10) { if ((UserVIAState.ddrb & 0x10) == 0x00) SetValue(IDC_IB4, (data & 0x10) == 0); else SetValue(IDC_IB4, false); }
			if (changed_bits & 0x08) { if ((UserVIAState.ddrb & 0x08) == 0x00) SetValue(IDC_IB3, (data & 0x08) == 0); else SetValue(IDC_IB3, false); }
			if (changed_bits & 0x04) { if ((UserVIAState.ddrb & 0x04) == 0x00) SetValue(IDC_IB2, (data & 0x04) == 0); else SetValue(IDC_IB2, false); }
			if (changed_bits & 0x02) { if ((UserVIAState.ddrb & 0x02) == 0x00) SetValue(IDC_IB1, (data & 0x02) == 0); else SetValue(IDC_IB1, false); }
			if (changed_bits & 0x01) { if ((UserVIAState.ddrb & 0x01) == 0x00) SetValue(IDC_IB0, (data & 0x01) == 0); else SetValue(IDC_IB0, false); }
			m_LastInputData = data;
		}
	}
}

/****************************************************************************/

void UserPortBreakoutDialog::ShowBitKey(int key, int ctrlID)
{
	SetDlgItemText(m_hwnd, ctrlID, GetPCKeyName(BitKeys[key]));
}

/****************************************************************************/

void UserPortBreakoutDialog::PromptForBitKeyInput(int bitKey)
{
	m_BitKey = bitKey;

	ShowBitKey(bitKey, BitKeyButtonIDs[bitKey]);

	std::string PCKeys = GetPCKeyName(BitKeys[m_BitKey]);

	selectKeyDialog = new SelectKeyDialog(
		m_hInstance,
		m_hwnd,
		"Press the key to use...",
		PCKeys,
		false,
		0,
		0,
		false
	);

	selectKeyDialog->Open();
}
