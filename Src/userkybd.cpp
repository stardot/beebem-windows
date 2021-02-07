/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 1997  Laurie Whiffen

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

#include <stdio.h>
#include <windows.h>
#include <string.h>
#include <string>
#include <vector>
#include "main.h"
#include "beebemrc.h"
#include "userkybd.h"
#include "SelectKeyDialog.h"
#include "Messages.h"

static void SetKeyColour(COLORREF aColour, HWND keyCtrl = NULL);
static void SelectKeyMapping(HWND hwnd, UINT ctrlID, HWND hwndCtrl);
static void SetBBCKeyForVKEY(int Key, bool shift);
static void SetRowCol(UINT ctrlID);
static INT_PTR CALLBACK UserKeyboardDlgProc(HWND   hwnd,
                                            UINT   nMessage,
                                            WPARAM wParam,
                                            LPARAM lParam);
static void OnDrawItem(UINT CtrlID, LPDRAWITEMSTRUCT lpDrawItemStruct);
static void DrawSides(HDC hDC, RECT rect, COLORREF TopLeft, COLORREF BottomRight);
static void DrawBorder(HDC hDC, RECT rect, BOOL Depressed);
static void DrawText(HDC hDC, RECT rect, HWND hwndCtrl, COLORREF colour, bool Depressed);
static COLORREF GetKeyColour(UINT ctrlID);
static std::string GetKeysUsed();
static void FillAssignedKeysCount();
static void UpdateAssignedKeysCount(int row, int col, int change, bool redrawColour = false);
static int GetBBCKeyIndex(const BBCKey* key);

// Colour used to highlight the selected key.
static const COLORREF HighlightColour   = 0x00FF0080; // Purple
static const COLORREF FunctionKeyColour = 0x000000FF; // Red
static const COLORREF NormalKeyColour   = 0x00000000; // Black
static const COLORREF JoyAssignedKeyColour = 0x00802020; // Blueish

static HWND hwndBBCKey; // Holds the BBCKey control handle which is now selected.
static UINT selectedCtrlID; // Holds ctrlId of selected key (or 0 if none selected).
static HWND hwndMain; // Holds the BeebWin window handle.
static HWND hwndUserKeyboard;
static bool doingJoystick; // Doing joystick vs user keyboard mapping

static int BBCRow; // Used to store the Row and Col values while we wait
static int BBCCol; // for a key press from the User.
static bool doingShifted; // Selecting shifted or unshifted key press
static bool doingUnassign; // Removing key assignment

// Initialised to defaultMapping.
KeyMap UserKeymap;

JoyMap JoystickMap;

static const char* szSelectKeyDialogTitle[2][3] = {
	{
		"Press key for unshifted press...",
		"Press key for shifted press...",
		"Press key to unassign..."
	},
	{
		// This can't be too long so 'unshifted' part is visible
		"Move stick for unshifted press...",
		"Move stick for shifted press...",
		"Move stick to unassign..."
	}
};

// Table of BBC keys
const BBCKey BBCKeys[] = {
	// Unassigned must go first
	{ IDK_UNASSIGN, "NONE", -9, 0 },

	// Letter keys
	{ IDK_A, "A", 4, 1 },
	{ IDK_B, "B", 6, 4 },
	{ IDK_C, "C", 5, 2 },
	{ IDK_D, "D", 3, 2 },
	{ IDK_E, "E", 2, 2 },
	{ IDK_F, "F", 4, 3 },
	{ IDK_G, "G", 5, 3 },
	{ IDK_H, "H", 5, 4 },
	{ IDK_I, "I", 2, 5 },
	{ IDK_J, "J", 4, 5 },
	{ IDK_K, "K", 4, 6 },
	{ IDK_L, "L", 5, 6 },
	{ IDK_M, "M", 6, 5 },
	{ IDK_N, "N", 5, 5 },
	{ IDK_O, "O", 3, 6 },
	{ IDK_P, "P", 3, 7 },
	{ IDK_Q, "Q", 1, 0 },
	{ IDK_R, "R", 3, 3 },
	{ IDK_S, "S", 5, 1 },
	{ IDK_T, "T", 2, 3 },
	{ IDK_U, "U", 3, 5 },
	{ IDK_V, "V", 6, 3 },
	{ IDK_W, "W", 2, 1 },
	{ IDK_X, "X", 4, 2 },
	{ IDK_Y, "Y", 4, 4 },
	{ IDK_Z, "Z", 6, 1 },

	// Number keys
	{ IDK_0, "0", 2, 7 },
	{ IDK_1, "1", 3, 0 },
	{ IDK_2, "2", 3, 1 },
	{ IDK_3, "3", 1, 1 },
	{ IDK_4, "4", 1, 2 },
	{ IDK_5, "5", 1, 3 },
	{ IDK_6, "6", 3, 4 },
	{ IDK_7, "7", 2, 4 },
	{ IDK_8, "8", 1, 5 },
	{ IDK_9, "9", 2, 6 },

	// Function keys.
	{ IDK_F0, "F0", 2, 0 },
	{ IDK_F1, "F1", 7, 1 },
	{ IDK_F2, "F2", 7, 2 },
	{ IDK_F3, "F3", 7, 3 },
	{ IDK_F4, "F4", 1, 4 },
	{ IDK_F5, "F5", 7, 4 },
	{ IDK_F6, "F6", 7, 5 },
	{ IDK_F7, "F7", 1, 6 },
	{ IDK_F8, "F8", 7, 6 },
	{ IDK_F9, "F9", 7, 7 },

	// Special keys.
	{ IDK_LEFT, "LEFT", 1, 9 },
	{ IDK_RIGHT, "RIGHT", 7, 9 },
	{ IDK_UP, "UP", 3, 9 },
	{ IDK_DOWN, "DOWN", 2, 9 },
	{ IDK_BREAK, "BREAK", -2, -2 },
	{ IDK_COPY, "COPY", 6, 9 },
	{ IDK_DEL, "DELETE", 5, 9 },
	{ IDK_CAPS, "CAPS-LOCK", 4, 0 },
	{ IDK_TAB, "TAB", 6, 0 },
	{ IDK_CTRL, "CTRL", 0, 1 },
	{ IDK_SPACE, "SPACE", 6, 2 },
	{ IDK_RETURN, "RETURN", 4, 9 },
	{ IDK_ESC, "ESCAPE", 7, 0 },
	{ IDK_SHIFT_L, "SHIFT", 0, 0 },
	{ IDK_SHIFT_R, "SHIFT", 0, 0 },
	{ IDK_SHIFT_LOCK, "SHIFT-LOCK", 5, 0 },

	// Special Character keys.
	{ IDK_SEMI_COLON, ";", 5, 7 },
	{ IDK_EQUALS, "=", 1, 7 },
	{ IDK_COMMA, ",", 6, 6 },
	{ IDK_CARET, "^", 1, 8 },
	{ IDK_DOT, ".", 6, 7 },
	{ IDK_FWDSLASH, "/", 6, 8 },
	{ IDK_STAR, "*", 4, 8 },
	{ IDK_OPEN_SQUARE, "[", 3, 8 },
	{ IDK_BACKSLASH, "\\", 7, 8 },
	{ IDK_CLOSE_SQUARE, "]", 5, 8 },
	{ IDK_AT, "@", 4, 7 },
	{ IDK_UNDERSCORE, "_", 2, 8 },
};

const std::pair<const char*, const char*> BBCKeyAliases[] = {
	{ "DEL", "DELETE" },
	{ "CAPS", "CAPS-LOCK" },
	{ "CAPSLOCK", "CAPS-LOCK" },
	{ "CONTROL", "CTRL" },
	{ "ESC", "ESCAPE" },
	{ "SHIFTLOCK", "SHIFT-LOCK" }
};

// Number of inputs assigned to each key (for joystick dialog highlight)
static int assignedKeysCount[_countof(BBCKeys)] = {};

/****************************************************************************/

const BBCKey* GetBBCKeyByResId(int ctrlId)
{
    using resIdToKeyMapType = std::map<int, const BBCKey*>;

    // Construct map on first use by lambda
    static const resIdToKeyMapType resIdToKeyMap = []()
    {
        resIdToKeyMapType keyMap{};

        for (auto& theKey : BBCKeys)
            keyMap[theKey.ctrlId] = &theKey;
        return keyMap;
    } ();

    auto iter = resIdToKeyMap.find(ctrlId);
    if (iter == resIdToKeyMap.end())
        return &BBCKeys[0];

    return iter->second;
}

/****************************************************************************/

const BBCKey* GetBBCKeyByName(const std::string& name)
{
    using nameToKeyMapType = std::map<std::string, const BBCKey*>;

    // Construct map on first use by lambda
    static const nameToKeyMapType nameToKeyMap = []()
    {
        nameToKeyMapType keyMap{};

        for (auto& theKey : BBCKeys)
            keyMap[theKey.name] = &theKey;

        for (auto& alias : BBCKeyAliases)
            keyMap[alias.first] = keyMap[alias.second];

        return keyMap;
    } ();

    auto iter = nameToKeyMap.find(name);
    if (iter == nameToKeyMap.end())
        return &BBCKeys[0];

    return iter->second;
}

/****************************************************************************/

const BBCKey* GetBBCKeyByRowAndCol(int row, int col)
{
    using posPair = std::pair<int, int>;
    using posToKeyMapType = std::map<posPair, const BBCKey*>;

    // Construct map on first use by lambda
    static const posToKeyMapType posToKeyMap = []()
    {
        posToKeyMapType keyMap{};

        for (auto& theKey : BBCKeys)
            keyMap[posPair{ theKey.row, theKey.column }] = &theKey;
        return keyMap;
    } ();

    auto iter = posToKeyMap.find(posPair{ row, col });
    if (iter == posToKeyMap.end())
        return &BBCKeys[0];

    return iter->second;
}

/****************************************************************************/
// Get index for assignedKeysCount. The 'key' parameter must be a pointer to
// item in BBCKeys table, not a copy of it.
int GetBBCKeyIndex(const BBCKey* key)
{
    int index = key - BBCKeys;
    if (index >= 0 && index < _countof(BBCKeys))
	return index;
    return 0;
}

/****************************************************************************/

bool UserKeyboardDialog(HWND hwndParent, bool joystick)
{
	// Initialise locals used during this window's life.
	hwndMain = hwndParent;
	selectedCtrlID = 0;
	doingJoystick = joystick;

	if (doingJoystick)
	    FillAssignedKeysCount();

	// Open the dialog box. This is created as a modeless dialog so that
	// the "select key" dialog box can handle key-press messages.
	hwndUserKeyboard = CreateDialog(
		hInst,
		MAKEINTRESOURCE(IDD_USERKYBRD),
		hwndParent,
		UserKeyboardDlgProc
	);

	if (hwndUserKeyboard != nullptr)
	{
		EnableWindow(hwndParent, FALSE);

		return true;
	}

	return false;
}

/****************************************************************************/

static void SetKeyColour(COLORREF aColour, HWND keyCtrl)
{
	if (keyCtrl == NULL)
		keyCtrl = hwndBBCKey;

	HDC hdc = GetDC(keyCtrl);
	SetBkColor(hdc, aColour);
	ReleaseDC(keyCtrl, hdc);
	InvalidateRect(keyCtrl, nullptr, TRUE);
	UpdateWindow(keyCtrl);
}

/****************************************************************************/

static void SelectKeyMapping(HWND hwnd, UINT ctrlID, HWND hwndCtrl)
{
	// Set the placeholders.
	SetRowCol(ctrlID);

	hwndBBCKey = hwndCtrl;
	selectedCtrlID = ctrlID;

	doingShifted = false;
	doingUnassign = (BBCRow == -9);

	std::string UsedKeys = doingUnassign ? "" : GetKeysUsed();

	// Now ask the user to input the PC key to assign to the BBC key.
	selectKeyDialog = new SelectKeyDialog(
		hInst,
		hwnd,
		szSelectKeyDialogTitle[(int)doingJoystick][doingUnassign ? 2 : doingShifted ? 1 : 0],
		UsedKeys,
		doingShifted,
		doingJoystick
	);

	selectKeyDialog->Open();
}

/****************************************************************************/

static void SetBBCKeyForVKEY(int Key, bool Shift)
{
	if (!doingJoystick && Key >= 0 && Key < BEEB_VKEY_JOY_START)
	{
		UserKeymap[Key][static_cast<int>(Shift)].row = BBCRow;
		UserKeymap[Key][static_cast<int>(Shift)].col = BBCCol;
		UserKeymap[Key][static_cast<int>(Shift)].shift = doingShifted;

		// char info[256];
		// sprintf(info, "SetBBCKey: key=%d, shift=%d, row=%d, col=%d, bbcshift=%d\n",
		//         Key, shift, BBCRow, BBCCol, doingShifted);
		// OutputDebugString(info);
	}
	else if (doingJoystick && Key >= BEEB_VKEY_JOY_START && Key < BEEB_VKEY_JOY_END)
	{
		KeyMapping& entry = JoystickMap[Key - BEEB_VKEY_JOY_START][static_cast<int>(Shift)];
		UpdateAssignedKeysCount(entry.row, entry.col, -1, true);
		entry.row = BBCRow;
		entry.col = BBCCol;
		entry.shift = doingShifted;
		UpdateAssignedKeysCount(entry.row, entry.col, +1);
	}
}

/****************************************************************************/

static void SetRowCol(UINT ctrlID)
{
    const BBCKey* key = GetBBCKeyByResId(ctrlID);
    BBCRow = key->row;
    BBCCol = key->column;
}

/****************************************************************************/

static INT_PTR CALLBACK UserKeyboardDlgProc(HWND   hwnd,
                                            UINT   nMessage,
                                            WPARAM wParam,
                                            LPARAM lParam)
{
	switch (nMessage)
	{
	case WM_INITDIALOG:
		if (doingJoystick)
		{
			SetWindowText(hwnd, "Joystick To Keyboard Mapping");
		}
		return FALSE;

	case WM_COMMAND:
		switch (wParam)
		{
		case IDOK:
		case IDCANCEL:
			EnableWindow(hwndMain, TRUE);
			DestroyWindow(hwnd);
			hwndUserKeyboard = nullptr;

			PostMessage(hwndMain, WM_USER_KEYBOARD_DIALOG_CLOSED, 0, 0);
			break;

		default:
			SelectKeyMapping(hwnd, (UINT)wParam, (HWND)lParam);
			break;
		}
		return TRUE;

	case WM_DRAWITEM:
		// Draw the key.
		OnDrawItem((UINT)wParam, (LPDRAWITEMSTRUCT)lParam);
		return TRUE;

	case WM_SELECT_KEY_DIALOG_CLOSED:
		if (wParam == IDOK)
		{
			// Assign the BBC key to the PC key.
			SetBBCKeyForVKEY(
				selectKeyDialog->Key(),
				selectKeyDialog->Shift()
			);
		}

		delete selectKeyDialog;
		selectKeyDialog = nullptr;

		if ((wParam == IDOK || wParam == IDCONTINUE) && !doingShifted)
		{
			doingShifted = !doingShifted;

			std::string UsedKeys = doingUnassign ? "" : GetKeysUsed();

			selectKeyDialog = new SelectKeyDialog(
				hInst,
				hwndUserKeyboard,
				szSelectKeyDialogTitle[(int)doingJoystick][doingUnassign ? 2 : doingShifted ? 1 : 0],
				UsedKeys,
				doingShifted,
				doingJoystick
			);

			selectKeyDialog->Open();
		}
		else
		{
			int ctrlID = selectedCtrlID;
			selectedCtrlID = 0;

			// Show the key as not depressed, i.e., normal.
			SetKeyColour(GetKeyColour(ctrlID));
		}
		return TRUE;

	default:
		break;
	}

	return FALSE;
}

/****************************************************************************/

static void OnDrawItem(UINT CtrlID, LPDRAWITEMSTRUCT lpDrawItemStruct)
{
	// set the Pen and Backgorund Brush.
	HBRUSH aBrush = CreateSolidBrush(GetKeyColour(CtrlID));
	HPEN aPen = CreatePen(PS_NULL, 1, RGB(0, 0, 0));

	// Copy into the Device Context.
	aBrush = (HBRUSH)SelectObject(lpDrawItemStruct->hDC, aBrush);
	aPen = (HPEN)SelectObject(lpDrawItemStruct->hDC, aPen);

	// Draw the rectanlge.
	SetBkColor(lpDrawItemStruct->hDC, GetKeyColour(CtrlID));
	Rectangle(lpDrawItemStruct->hDC,
		lpDrawItemStruct->rcItem.left,
		lpDrawItemStruct->rcItem.top,
		lpDrawItemStruct->rcItem.right,
		lpDrawItemStruct->rcItem.bottom);

	// Draw border.
	DrawBorder(lpDrawItemStruct->hDC,
		lpDrawItemStruct->rcItem,
		lpDrawItemStruct->itemState == (ODS_FOCUS | ODS_SELECTED));

	// Draw the text.
	DrawText(lpDrawItemStruct->hDC,
		lpDrawItemStruct->rcItem,
		lpDrawItemStruct->hwndItem,
		0x00FFFFFF,
		lpDrawItemStruct->itemState == (ODS_FOCUS | ODS_SELECTED));

	// Clear up.
	DeleteObject(SelectObject(lpDrawItemStruct->hDC, aBrush));
	DeleteObject(SelectObject(lpDrawItemStruct->hDC, aPen));
}

/****************************************************************************/

static void DrawSides(HDC hDC, RECT rect, COLORREF TopLeft, COLORREF BottomRight)
{
	HPEN hTopLeftPen = CreatePen(PS_SOLID, 1, TopLeft);
	HPEN hBottomRightPen = CreatePen(PS_SOLID, 1, BottomRight);

	HPEN hOldPen = (HPEN)SelectObject(hDC, hTopLeftPen);

	POINT point;
	MoveToEx(hDC, rect.left, rect.bottom - 1, &point);
	LineTo(hDC, rect.left, rect.top);
	LineTo(hDC, rect.right - 1, rect.top);

	SelectObject(hDC, hBottomRightPen);
	LineTo(hDC, rect.right - 1, rect.bottom - 1);
	LineTo(hDC, rect.left, rect.bottom - 1);

	// Clean up.
	SelectObject(hDC, hOldPen);
	DeleteObject(hTopLeftPen);
	DeleteObject(hBottomRightPen);
}

/****************************************************************************/

static void DrawBorder(HDC hDC, RECT rect, BOOL Depressed)
{
	// Draw outer border.
	if (Depressed)
		DrawSides( hDC, rect, 0x00000000, 0x00FFFFFF );
	else
		DrawSides( hDC, rect, 0x00FFFFFF, 0x00000000 );
	
	// Draw inner border.
	rect.top++;
	rect.left++;
	rect.right--;
	rect.bottom--;

	if (Depressed)
		DrawSides( hDC, rect, 0x00777777, 0x00FFFFFF );
	else
		DrawSides( hDC, rect, 0x00FFFFFF, 0x00777777 );
}

/****************************************************************************/

static void DrawText(HDC hDC, RECT rect, HWND hwndCtrl, COLORREF colour, bool Depressed)
{
	SIZE Size;
	CHAR text[10];

	GetWindowText(hwndCtrl, text, 9);

	if (GetTextExtentPoint32(hDC, text, (int)strlen(text), &Size))
	{
		// Set text colour.
		SetTextColor(hDC, colour);

		// Output text.
		const int Offset = Depressed ? 1 : 0;

		TextOut(hDC,
		        ((rect.right - rect.left) - Size.cx) / 2 + Offset,
		        ((rect.bottom - rect.top) - Size.cy) / 2 + Offset,
		        text,
		        (int)strlen(text));
	}
}

/****************************************************************************/

static COLORREF GetKeyColour(UINT ctrlID)
{
	if (selectedCtrlID == ctrlID)
	{
		return HighlightColour;
	}

	if (doingJoystick)
	{
		int index = GetBBCKeyIndex(GetBBCKeyByResId(ctrlID));
		if (index != 0 && assignedKeysCount[index] > 0)
			return JoyAssignedKeyColour;
	}

	switch (ctrlID)
	{
	case IDK_F0:
	case IDK_F1:
	case IDK_F2:
	case IDK_F3:
	case IDK_F4:
	case IDK_F5:
	case IDK_F6:
	case IDK_F7:
	case IDK_F8:
	case IDK_F9:
		return FunctionKeyColour;

	default:
		return NormalKeyColour;
	}
}

/****************************************************************************/

static std::string GetKeysUsed()
{
	std::string Keys;

	KeyPair* table;
	int start, end;
	int offset; // First Vkey code in table

	if (!doingJoystick)
	{
		table = UserKeymap;
		offset = 0;
		start = 1;
		end = 256;
	}
	else
	{
		table = JoystickMap;
		offset = BEEB_VKEY_JOY_START;
		start = BEEB_VKEY_JOY_START;
		end = BEEB_VKEY_JOY_END;
	}

	// First see if this key is defined.
	// Row 0 is Shift key, row -2 is Break, row -9 is unassigned
	if (BBCRow != -9)
	{
		for (int i = start; i < end; i++)
		{
			for (int s = 0; s < 2; ++s)
			{
				if (table[i - offset][s].row == BBCRow &&
					table[i - offset][s].col == BBCCol &&
					table[i - offset][s].shift == doingShifted)
				{
					// We have found a key that matches.
					if (!Keys.empty())
					{
						Keys += ", ";
					}

					if (s == 1)
					{
						Keys += "Sh-";
					}

					Keys += SelectKeyDialog::KeyName(i);
				}
			}
		}
	}

	if (Keys.empty())
	{
		Keys = "Not Assigned";
	}

	return Keys;
}

/****************************************************************************/

void FillAssignedKeysCount()
{
    std::fill(std::begin(assignedKeysCount), std::end(assignedKeysCount), 0);

    KeyPair* table;
    int start, end;
    int offset; // First Vkey code in table

    if (!doingJoystick)
    {
        table = UserKeymap;
        offset = 0;
        start = 1;
        end = 256;
    }
    else
    {
        table = JoystickMap;
        offset = BEEB_VKEY_JOY_START;
        start = BEEB_VKEY_JOY_START;
        end = BEEB_VKEY_JOY_END;
    }

    for (int i = start; i < end; i++)
    {
        KeyPair& pair = table[i - offset];
        for (int s = 0; s < 2; s++)
        {
            UpdateAssignedKeysCount(pair[s].row, pair[s].col, +1);
        }
    }
}

/****************************************************************************/

static void UpdateAssignedKeysCount(int row, int col, int change, bool redrawColour)
{
    const BBCKey* key = GetBBCKeyByRowAndCol(row, col);
    int index = GetBBCKeyIndex(key);
    if (index < _countof(BBCKeys))
    {
        assignedKeysCount[index] += change;
        if (redrawColour && key->ctrlId != selectedCtrlID)
        {
            HWND keyCtrl = GetDlgItem(hwndUserKeyboard, key->ctrlId);
            SetKeyColour(GetKeyColour(key->ctrlId), keyCtrl);
        }
    }
}
