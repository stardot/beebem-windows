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

#include "main.h"
#include "beebemrc.h"
#include "SelectKeyDialog.h"
#include "Dialog.h"
#include "Messages.h"

/****************************************************************************/

SelectKeyDialog* selectKeyDialog;

/****************************************************************************/

SelectKeyDialog::SelectKeyDialog(
	HINSTANCE hInstance,
	HWND hwndParent,
	const std::string& Title,
	const std::string& SelectedKey,
	bool doingShifted,
	bool doingJoystick) :
	m_hInstance(hInstance),
	m_hwnd(nullptr),
	m_hwndParent(hwndParent),
	m_Title(Title),
	m_SelectedKey(SelectedKey),
	m_Key(-1),
	m_Shift(doingShifted),
	m_Joystick(doingJoystick)
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

		// If the selected keys is empty (as opposed to "Not assigned"), we are currently unassigning.
		// Hide the "Assigned to:" label
		if (m_SelectedKey.empty())
			SetDlgItemText(m_hwnd, IDC_ASSIGNED_KEYS_LBL, "");
		else if (m_Joystick)
			SetDlgItemText(m_hwnd, IDC_ASSIGNED_KEYS_LBL, "Assigned to:");

		// If doing shifted key, start with the Shift checkbox checked because that's most likely
		// what the user wants
		SetDlgItemChecked(m_hwnd, IDC_SHIFT, m_Shift);

		return TRUE;

	case WM_ACTIVATE:
		if (LOWORD(wParam) == WA_INACTIVE)
		{
			hCurrentDialog = nullptr;
			mainWin->m_JoystickTarget = nullptr;
		}
		else
		{
			hCurrentDialog = m_hwnd;
			mainWin->m_JoystickTarget = m_hwnd;
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

	return dialog->DlgProc(hwnd, nMessage, wParam, lParam);
}

/****************************************************************************/

bool SelectKeyDialog::HandleMessage(const MSG& msg)
{
	if (msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN)
	{
		int key = (int)msg.wParam;

		if (!m_Joystick && key < 256 ||
		    m_Joystick && key >= BEEB_VKEY_JOY_START && key < BEEB_VKEY_JOY_END)
		{
			m_Key = key;
			m_Shift = IsDlgItemChecked(m_hwnd, IDC_SHIFT);
			Close(IDOK);
			return true;
		}
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

LPCSTR SelectKeyDialog::KeyName(int Key)
{
	static CHAR Character[2]; // Used to return single characters.

	if (Key >= BEEB_VKEY_JOY_START && Key < BEEB_VKEY_JOY_END)
	{
		static CHAR Name[16]; // Buffer for joystick button or axis name

		Key -= BEEB_VKEY_JOY_START;

		if (Key >= BEEB_VKEY_JOY2_AXES - BEEB_VKEY_JOY1_AXES)
		{
			strcpy(Name, "Joy2");
			Key -= BEEB_VKEY_JOY2_AXES - BEEB_VKEY_JOY1_AXES;
		}
		else
		{
			strcpy(Name, "Joy1");
		}

		if (Key < BEEB_VKEY_JOY1_BTN1 - BEEB_VKEY_JOY1_AXES)
		{
			if (Key == JOYSTICK_AXIS_UP)
			{
				strcat(Name, "Up");
			}
			else if (Key == JOYSTICK_AXIS_DOWN)
			{
				strcat(Name, "Down");
			}
			else if (Key == JOYSTICK_AXIS_LEFT)
			{
				strcat(Name, "Left");
			}
			else if (Key == JOYSTICK_AXIS_RIGHT)
			{
				strcat(Name, "Right");
			}
			else
			{
				sprintf(Name + strlen(Name), "Axis%d", (Key / 2) + 1);
				strcat(Name, (Key & 1) ? "+" : "-");
			}
		}
		else
		{
			sprintf(Name + strlen(Name), "Btn%d", Key - (BEEB_VKEY_JOY1_BTN1 - BEEB_VKEY_JOY1_AXES) + 1);
		}

		return Name;
	}

	switch (Key)
	{
	case   8: return "Backspace";
	case   9: return "Tab";
	case  13: return "Enter";
	case  16: return "Shift";
	case  17: return "Ctrl";
	case  18: return "Alt";
	case  19: return "Break";
	case  20: return "Caps";
	case  27: return "Esc";
	case  32: return "Spacebar";
	case  33: return "PgUp";
	case  34: return "PgDn";
	case  35: return "End";
	case  36: return "Home";
	case  37: return "Left";
	case  38: return "Up";
	case  39: return "Right";
	case  40: return "Down";
	case  45: return "Insert";
	case  46: return "Del";
	case  93: return "Menu";
	case  96: return "Pad0";
	case  97: return "Pad1";
	case  98: return "Pad2";
	case  99: return "Pad3";
	case 100: return "Pad4";
	case 101: return "Pad5";
	case 102: return "Pad6";
	case 103: return "Pad7";
	case 104: return "Pad8";
	case 105: return "Pad9";
	case 106: return "Pad*";
	case 107: return "Pad+";
	case 109: return "Pad-";
	case 110: return "Pad.";
	case 111: return "Pad/";
	case 112: return "F1";
	case 113: return "F2";
	case 114: return "F3";
	case 115: return "F4";
	case 116: return "F5";
	case 117: return "F6";
	case 118: return "F7";
	case 119: return "F8";
	case 120: return "F9";
	case 121: return "F10";
	case 122: return "F11";
	case 123: return "F12";
	case 144: return "NumLock";
	case 145: return "SclLock";
	case 186: return ";";
	case 187: return "=";
	case 188: return ",";
	case 189: return "-";
	case 190: return ".";
	case 191: return "/";
	case 192: return "\'";
	case 219: return "[";
	case 220: return "\\";
	case 221: return "]";
	case 222: return "#";
	case 223: return "`";

	default:
		Character[0] = (CHAR)LOBYTE(Key);
		Character[1] = '\0';
		return Character;
	}
}

static std::string toupper(const std::string& src)
{
	std::string result = src;

	for (auto& c : result)
	{
		c = static_cast<char>(toupper(c));
	}

	return result;
}

int SelectKeyDialog::JoyVKeyByName(const char* Name)
{
	using vkeyMapType = std::map<std::string, int>;

	// Construct map on first use by lambda
	static vkeyMapType nameToVKeyMap = []() {
		vkeyMapType keyMap{};

		for (int vkey = BEEB_VKEY_JOY_START; vkey < BEEB_VKEY_JOY_END; ++vkey)
			keyMap[toupper(KeyName(vkey))] = vkey;

		return keyMap;
	}();

	auto iter = nameToVKeyMap.find(toupper(Name));
	if (iter == nameToVKeyMap.end())
		return -1;

	return iter->second;
}
