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

#include "beebemrc.h"
#include "main.h"
#include "Messages.h"
#include "SelectKeyDialog.h"
#include "uservia.h"

/* User Port Breakout Box */

static HWND hwndMain = nullptr; // Holds the BeebWin window handle.
HWND hwndBreakOut = nullptr;
static int BitKey; // Used to store the bit key pressed while we wait
int BitKeys[8] = { 48, 49, 50, 51, 52, 53, 54, 55 };
// static int SelectedBitKey = 0;
static const UINT BitKeyButtonIDs[8] = {
    IDK_BIT0, IDK_BIT1, IDK_BIT2, IDK_BIT3,
    IDK_BIT4, IDK_BIT5, IDK_BIT6, IDK_BIT7,
};

// static void SetBitKey(int ctrlID);
static bool GetValue(int ctrlID);
static void SetValue(int ctrlID, bool State);
static void ShowBitKey(int key, int ctrlID);

static INT_PTR CALLBACK BreakOutDlgProc(HWND   hwnd,
                                        UINT   nMessage,
                                        WPARAM wParam,
                                        LPARAM lParam);

static void PromptForBitKeyInput(HWND hwndParent, int bitKey);

/****************************************************************************/

bool BreakOutOpenDialog(HINSTANCE hinst, HWND hwndParent)
{
	if (hwndBreakOut != nullptr)
	{
		// The dialog box is already open.
		return false;
	}

	hwndMain = hwndParent;

	hwndBreakOut = CreateDialog(
		hinst,
		MAKEINTRESOURCE(IDD_BREAKOUT),
		hwndParent,
		BreakOutDlgProc
	);

	if (hwndBreakOut != nullptr)
	{
		hCurrentDialog = hwndBreakOut;
		return true;
	}

	return false;
}

/****************************************************************************/

void BreakOutCloseDialog()
{
	if (hwndBreakOut == nullptr)
	{
		return;
	}

	if (selectKeyDialog != nullptr)
	{
		selectKeyDialog->Close(IDCANCEL);
	}

	EnableWindow(hwndMain, TRUE);
	DestroyWindow(hwndBreakOut);
	hwndBreakOut = nullptr;
	hCurrentDialog = nullptr;
}

/****************************************************************************/

INT_PTR CALLBACK BreakOutDlgProc(HWND   hwnd,
                                 UINT   nMessage,
                                 WPARAM wParam,
                                 LPARAM /* lParam */)
{
	bool bit;

	switch (nMessage)
	{
	case WM_INITDIALOG:
		ShowBitKey(0, IDK_BIT0);
		ShowBitKey(1, IDK_BIT1);
		ShowBitKey(2, IDK_BIT2);
		ShowBitKey(3, IDK_BIT3);
		ShowBitKey(4, IDK_BIT4);
		ShowBitKey(5, IDK_BIT5);
		ShowBitKey(6, IDK_BIT6);
		ShowBitKey(7, IDK_BIT7);
		return TRUE;

	case WM_COMMAND:
		switch (wParam)
		{
		case IDOK:
		case IDCANCEL:
			BreakOutCloseDialog();
			return TRUE;

		case IDK_BIT0:
			PromptForBitKeyInput(hwnd, 0);
			break;

		case IDK_BIT1:
			PromptForBitKeyInput(hwnd, 1);
			break;

		case IDK_BIT2:
			PromptForBitKeyInput(hwnd, 2);
			break;

		case IDK_BIT3:
			PromptForBitKeyInput(hwnd, 3);
			break;

		case IDK_BIT4:
			PromptForBitKeyInput(hwnd, 4);
			break;

		case IDK_BIT5:
			PromptForBitKeyInput(hwnd, 5);
			break;

		case IDK_BIT6:
			PromptForBitKeyInput(hwnd, 6);
			break;

		case IDK_BIT7:
			PromptForBitKeyInput(hwnd, 7);
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

	case WM_SELECT_KEY_DIALOG_CLOSED:
		if (wParam == IDOK)
		{
			// Assign the BBC key to the PC key.
			BitKeys[BitKey] = selectKeyDialog->Key();
			ShowBitKey(BitKey, BitKeyButtonIDs[BitKey]);
		}

		delete selectKeyDialog;
		selectKeyDialog = nullptr;

		return TRUE;
	}

	return FALSE;
}

/****************************************************************************/

static bool GetValue(int ctrlID)
{
	return SendDlgItemMessage(hwndBreakOut, ctrlID, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

/****************************************************************************/

static void SetValue(int ctrlID, bool State)
{
	SendDlgItemMessage(hwndBreakOut, ctrlID, BM_SETCHECK, State ? 1 : 0, 0);
}

/****************************************************************************/

void ShowOutputs(unsigned char data)
{
	static unsigned char last_data = 0;
	unsigned char changed_bits;

	if (hwndBreakOut != nullptr)
	{
		if (data != last_data)
		{
			changed_bits = data ^ last_data;
			if (changed_bits & 0x80) { if ((UserVIAState.ddrb & 0x80) == 0x80) SetValue(IDC_OB7, (data & 0x80) != 0); else SetValue(IDC_OB7, 0); }
			if (changed_bits & 0x40) { if ((UserVIAState.ddrb & 0x40) == 0x40) SetValue(IDC_OB6, (data & 0x40) != 0); else SetValue(IDC_OB6, 0); }
			if (changed_bits & 0x20) { if ((UserVIAState.ddrb & 0x20) == 0x20) SetValue(IDC_OB5, (data & 0x20) != 0); else SetValue(IDC_OB5, 0); }
			if (changed_bits & 0x10) { if ((UserVIAState.ddrb & 0x10) == 0x10) SetValue(IDC_OB4, (data & 0x10) != 0); else SetValue(IDC_OB4, 0); }
			if (changed_bits & 0x08) { if ((UserVIAState.ddrb & 0x08) == 0x08) SetValue(IDC_OB3, (data & 0x08) != 0); else SetValue(IDC_OB3, 0); }
			if (changed_bits & 0x04) { if ((UserVIAState.ddrb & 0x04) == 0x04) SetValue(IDC_OB2, (data & 0x04) != 0); else SetValue(IDC_OB2, 0); }
			if (changed_bits & 0x02) { if ((UserVIAState.ddrb & 0x02) == 0x02) SetValue(IDC_OB1, (data & 0x02) != 0); else SetValue(IDC_OB1, 0); }
			if (changed_bits & 0x01) { if ((UserVIAState.ddrb & 0x01) == 0x01) SetValue(IDC_OB0, (data & 0x01) != 0); else SetValue(IDC_OB0, 0); }
			last_data = data;
		}
	}
}

/****************************************************************************/

void ShowInputs(unsigned char data)
{
	static unsigned char last_data = 0;
	unsigned char changed_bits;

	if (hwndBreakOut != nullptr)
	{
		if (data != last_data)
		{
			changed_bits = data ^ last_data;
			if (changed_bits & 0x80) { if ((UserVIAState.ddrb & 0x80) == 0x00) SetValue(IDC_IB7, (data & 0x80) == 0); else SetValue(IDC_IB7, false); }
			if (changed_bits & 0x40) { if ((UserVIAState.ddrb & 0x40) == 0x00) SetValue(IDC_IB6, (data & 0x40) == 0); else SetValue(IDC_IB6, false); }
			if (changed_bits & 0x20) { if ((UserVIAState.ddrb & 0x20) == 0x00) SetValue(IDC_IB5, (data & 0x20) == 0); else SetValue(IDC_IB5, false); }
			if (changed_bits & 0x10) { if ((UserVIAState.ddrb & 0x10) == 0x00) SetValue(IDC_IB4, (data & 0x10) == 0); else SetValue(IDC_IB4, false); }
			if (changed_bits & 0x08) { if ((UserVIAState.ddrb & 0x08) == 0x00) SetValue(IDC_IB3, (data & 0x08) == 0); else SetValue(IDC_IB3, false); }
			if (changed_bits & 0x04) { if ((UserVIAState.ddrb & 0x04) == 0x00) SetValue(IDC_IB2, (data & 0x04) == 0); else SetValue(IDC_IB2, false); }
			if (changed_bits & 0x02) { if ((UserVIAState.ddrb & 0x02) == 0x00) SetValue(IDC_IB1, (data & 0x02) == 0); else SetValue(IDC_IB1, false); }
			if (changed_bits & 0x01) { if ((UserVIAState.ddrb & 0x01) == 0x00) SetValue(IDC_IB0, (data & 0x01) == 0); else SetValue(IDC_IB0, false); }
			last_data = data;
		}
	}
}

/****************************************************************************/

static void ShowBitKey(int key, int ctrlID)
{
	SetDlgItemText(hwndBreakOut, ctrlID, SelectKeyDialog::KeyName(BitKeys[key]));
}

/****************************************************************************/

/*
void SetBitKey(int ctrlID)
{
	switch( ctrlID )
	{
	// Character keys.
	case IDK_BIT0 : BitKey = 0; break;
	case IDK_BIT1 : BitKey = 1; break;
	case IDK_BIT2 : BitKey = 2; break;
	case IDK_BIT3 : BitKey = 3; break;
	case IDK_BIT4 : BitKey = 4; break;
	case IDK_BIT5 : BitKey = 5; break;
	case IDK_BIT6 : BitKey = 6; break;
	case IDK_BIT7 : BitKey = 7; break;

	default:
		BitKey = -1;
	}
}
*/

/****************************************************************************/

static void PromptForBitKeyInput(HWND hwndParent, int bitKey)
{
	BitKey = bitKey;

	ShowBitKey(bitKey, BitKeyButtonIDs[bitKey]);

	std::string UsedKey = SelectKeyDialog::KeyName(BitKeys[BitKey]);

	selectKeyDialog = new SelectKeyDialog(
		hInst,
		hwndParent,
		"Press the key to use...",
		UsedKey
	);

	selectKeyDialog->Open();
}
