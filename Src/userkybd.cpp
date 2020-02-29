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
#include "main.h"
#include "beebemrc.h"
#include "userkybd.h"

static void SetKeyColour(COLORREF aColour);
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
static bool PromptForInput(HWND hwndParent, bool doShiftedKey);
static INT_PTR CALLBACK SelectKeyDlgProc(HWND   hWnd,
                                         UINT   nMessage,
                                         WPARAM wParam,
                                         LPARAM lParam);
static bool IsDlgItemChecked(HWND hDlg, int nIDDlgItem);
static void CloseSelectKeyDialog();
static std::string GetKeysUsed();
static LPCSTR KeyName(UINT Key);

// Colour used to highlight the selected key.
static const COLORREF HighlightColour   = 0x00FF0080; // Purple
static const COLORREF FunctionKeyColour = 0x000000FF; // Red
static const COLORREF NormalKeyColour   = 0x00000000; // Black
static COLORREF oldKeyColour;

static HWND hwndBBCKey; // Holds the BBCKey control handle which is now selected.
static UINT selectedCtrlID; // Holds ctrlId of selected key (or 0 if none selected).
static HWND hwndMain; // Holds the BeebWin window handle.
static HWND hwndUserKeyboard;

static int BBCRow; // Used to store the Row and Col values while we wait
static int BBCCol; // for a key press from the User.
static HFONT hGetkeyFont; // The font used by the PromptForInput() prompt window.
static bool doingShifted; // Selecting shifted or unshifted key press
HWND hwndSelectKey;

// Initialised to defaultMapping.
KeyMap UserKeymap;

const UINT WM_SELECT_KEY_DIALOG_CLOSED = WM_APP;

/****************************************************************************/

bool UserKeyboardDialog(HWND hwndParent)
{
	// Initialise locals used during this window's life.
	hwndMain = hwndParent;
	selectedCtrlID = 0;

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

static void SetKeyColour(COLORREF aColour)
{
	HDC hdc = GetDC(hwndBBCKey);
	SetBkColor(hdc, aColour);
	ReleaseDC(hwndBBCKey, hdc);
	InvalidateRect(hwndBBCKey, nullptr, TRUE);
	UpdateWindow(hwndBBCKey);
}

/****************************************************************************/

static void SelectKeyMapping(HWND hwnd, UINT ctrlID, HWND hwndCtrl)
{
	// Set the placeholders.
	SetRowCol(ctrlID);

	oldKeyColour = GetKeyColour(ctrlID);

	hwndBBCKey = hwndCtrl;
	selectedCtrlID = ctrlID;

	// Now ask the user to input the PC key to assign to the BBC key.
	PromptForInput(hwnd, false);
}

/****************************************************************************/

static void SetBBCKeyForVKEY(int Key, bool shift)
{
	if (Key >= 0 && Key < 256)
	{
		UserKeymap[Key][static_cast<int>(shift)].row = BBCRow;
		UserKeymap[Key][static_cast<int>(shift)].col = BBCCol;
		UserKeymap[Key][static_cast<int>(shift)].shift = doingShifted;

		char info[256];
		sprintf(info, "SetBBCKey: key=%d, shift=%d, row=%d, col=%d, bbcshift=%d\n",
		        Key, shift, BBCRow, BBCCol, doingShifted);
		OutputDebugString(info);
	}
}

/****************************************************************************/

static void SetRowCol(UINT ctrlID)
{
	switch (ctrlID)
	{
	// Character keys.
	case IDK_A: BBCRow = 4; BBCCol = 1; break;
	case IDK_B: BBCRow = 6; BBCCol = 4; break;
	case IDK_C: BBCRow = 5; BBCCol = 2; break;
	case IDK_D: BBCRow = 3; BBCCol = 2; break;
	case IDK_E: BBCRow = 2; BBCCol = 2; break;
	case IDK_F: BBCRow = 4; BBCCol = 3; break;
	case IDK_G: BBCRow = 5; BBCCol = 3; break;
	case IDK_H: BBCRow = 5; BBCCol = 4; break;
	case IDK_I: BBCRow = 2; BBCCol = 5; break;
	case IDK_J: BBCRow = 4; BBCCol = 5; break;
	case IDK_K: BBCRow = 4; BBCCol = 6; break;
	case IDK_L: BBCRow = 5; BBCCol = 6; break;
	case IDK_M: BBCRow = 6; BBCCol = 5; break;
	case IDK_N: BBCRow = 5; BBCCol = 5; break;
	case IDK_O: BBCRow = 3; BBCCol = 6; break;
	case IDK_P: BBCRow = 3; BBCCol = 7; break;
	case IDK_Q: BBCRow = 1; BBCCol = 0; break;
	case IDK_R: BBCRow = 3; BBCCol = 3; break;
	case IDK_S: BBCRow = 5; BBCCol = 1; break;
	case IDK_T: BBCRow = 2; BBCCol = 3; break;
	case IDK_U: BBCRow = 3; BBCCol = 5; break;
	case IDK_V: BBCRow = 6; BBCCol = 3; break;
	case IDK_W: BBCRow = 2; BBCCol = 1; break;
	case IDK_X: BBCRow = 4; BBCCol = 2; break;
	case IDK_Y: BBCRow = 4; BBCCol = 4; break;
	case IDK_Z: BBCRow = 6; BBCCol = 1; break;

	// Number keys.
	case IDK_0: BBCRow = 2; BBCCol = 7; break;
	case IDK_1: BBCRow = 3; BBCCol = 0; break;
	case IDK_2: BBCRow = 3; BBCCol = 1; break;
	case IDK_3: BBCRow = 1; BBCCol = 1; break;
	case IDK_4: BBCRow = 1; BBCCol = 2; break;
	case IDK_5: BBCRow = 1; BBCCol = 3; break;
	case IDK_6: BBCRow = 3; BBCCol = 4; break;
	case IDK_7: BBCRow = 2; BBCCol = 4; break;
	case IDK_8: BBCRow = 1; BBCCol = 5; break;
	case IDK_9: BBCRow = 2; BBCCol = 6; break;

	// Function keys.
	case IDK_F0: BBCRow = 2; BBCCol = 0; break;
	case IDK_F1: BBCRow = 7; BBCCol = 1; break;
	case IDK_F2: BBCRow = 7; BBCCol = 2; break;
	case IDK_F3: BBCRow = 7; BBCCol = 3; break;
	case IDK_F4: BBCRow = 1; BBCCol = 4; break;
	case IDK_F5: BBCRow = 7; BBCCol = 4; break;
	case IDK_F6: BBCRow = 7; BBCCol = 5; break;
	case IDK_F7: BBCRow = 1; BBCCol = 6; break;
	case IDK_F8: BBCRow = 7; BBCCol = 6; break;
	case IDK_F9: BBCRow = 7; BBCCol = 7; break;

	// Special keys.
	case IDK_LEFT:       BBCRow = 1;  BBCCol = 9; break;
	case IDK_RIGHT:      BBCRow = 7;  BBCCol = 9; break;
	case IDK_UP:         BBCRow = 3;  BBCCol = 9; break;
	case IDK_DOWN:       BBCRow = 2;  BBCCol = 9; break;
	case IDK_BREAK:      BBCRow = -2; BBCCol = -2; break;
	case IDK_COPY:       BBCRow = 6;  BBCCol = 9; break;
	case IDK_DEL:        BBCRow = 5;  BBCCol = 9; break;
	case IDK_CAPS:       BBCRow = 4;  BBCCol = 0; break;
	case IDK_TAB:        BBCRow = 6;  BBCCol = 0; break;
	case IDK_CTRL:       BBCRow = 0;  BBCCol = 1; break;
	case IDK_SPACE:      BBCRow = 6;  BBCCol = 2; break;
	case IDK_RETURN:     BBCRow = 4;  BBCCol = 9; break;
	case IDK_ESC:        BBCRow = 7;  BBCCol = 0; break;
	case IDK_SHIFT_L:    BBCRow = 0;  BBCCol = 0; break;
	case IDK_SHIFT_R:    BBCRow = 0;  BBCCol = 0; break;
	case IDK_SHIFT_LOCK: BBCRow = 5;  BBCCol = 0; break;

	// Special Character keys.
	case IDK_SEMI_COLON:   BBCRow = 5; BBCCol = 7; break;
	case IDK_EQUALS:       BBCRow = 1; BBCCol = 7; break;
	case IDK_COMMA:        BBCRow = 6; BBCCol = 6; break;
	case IDK_CARET:        BBCRow = 1; BBCCol = 8; break;
	case IDK_DOT:          BBCRow = 6; BBCCol = 7; break;
	case IDK_FWDSLASH:     BBCRow = 6; BBCCol = 8; break;
	case IDK_STAR:         BBCRow = 4; BBCCol = 8; break;
	case IDK_OPEN_SQUARE:  BBCRow = 3; BBCCol = 8; break;
	case IDK_BACKSLASH:    BBCRow = 7; BBCCol = 8; break;
	case IDK_CLOSE_SQUARE: BBCRow = 5; BBCCol = 8; break;
	case IDK_AT:           BBCRow = 4; BBCCol = 7; break;
	case IDK_UNDERSCORE:   BBCRow = 2; BBCCol = 8; break;

	default:
		BBCRow = 0; BBCCol = 0;
	}
}

/****************************************************************************/

static INT_PTR CALLBACK UserKeyboardDlgProc(HWND   hwnd,
                                            UINT   nMessage,
                                            WPARAM wParam,
                                            LPARAM lParam)
{
	switch (nMessage)
	{
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
		if (!doingShifted)
		{
			PromptForInput(hwnd, true);
		}
		else
		{
			selectedCtrlID = 0;

			// Show the key as not depressed, i.e., normal.
			SetKeyColour(oldKeyColour);
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

static bool PromptForInput(HWND hwndParent, bool doShiftedKey)
{
	doingShifted = doShiftedKey;

	// Create modeless dialog box
	hwndSelectKey = CreateDialog(
		hInst,
		MAKEINTRESOURCE(IDD_SELECT_KEY),
		hwndParent,
		SelectKeyDlgProc
	);

	if (hwndSelectKey != nullptr)
	{
		EnableWindow(hwndParent, FALSE);

		return true;
	}

	return false;
}

/****************************************************************************/

static INT_PTR CALLBACK SelectKeyDlgProc(HWND   hwnd,
                                         UINT   nMessage,
                                         WPARAM wParam,
                                         LPARAM /* lParam */)
{
	const char* szTitle1 = "Press key for unshifted press...";
	const char* szTitle2 = "Press key for shifted press...";

	// The keys used list.
	std::string UsedKeys;

	switch (nMessage)
	{
		case WM_INITDIALOG:
			SetWindowText(hwnd, doingShifted ? szTitle2 : szTitle1);

			UsedKeys = GetKeysUsed();
			SetDlgItemText(hwnd, IDC_ASSIGNED_KEYS, UsedKeys.c_str());
			return TRUE;

		case WM_ACTIVATE:
			if (LOWORD(wParam) == WA_INACTIVE)
			{
				hCurrentDialog = nullptr;
			}
			else
			{
				hCurrentDialog = hwnd;
				hCurrentAccelTable = nullptr;
			}
			return FALSE;

		case WM_COMMAND:
			switch (wParam)
			{
				case IDOK:
				case IDCANCEL:
					CloseSelectKeyDialog();
					return TRUE;

				default:
					break;
			}
			break;
	}

	return FALSE;
}

/****************************************************************************/

bool SelectKeyHandleMessage(const MSG& msg)
{
	if (msg.message == WM_KEYDOWN || msg.message == WM_SYSKEYDOWN)
	{
		// Assign the BBC key to the PC key.
		bool shift = IsDlgItemChecked(hwndSelectKey, IDC_SHIFT);
		SetBBCKeyForVKEY((int)msg.wParam, shift);

		CloseSelectKeyDialog();
		return true;
	}

	return false;
}

/****************************************************************************/

static bool IsDlgItemChecked(HWND hDlg, int nIDDlgItem)
{
	return SendDlgItemMessage(hDlg, nIDDlgItem, BM_GETCHECK, 0, 0) == BST_CHECKED;
}

/****************************************************************************/

static void CloseSelectKeyDialog()
{
	EnableWindow(hwndUserKeyboard, TRUE);
	DestroyWindow(hwndSelectKey);
	hwndSelectKey = nullptr;

	PostMessage(hwndUserKeyboard, WM_SELECT_KEY_DIALOG_CLOSED, 0, 0);
}

/****************************************************************************/

static std::string GetKeysUsed()
{
	std::string Keys;

	// First see if this key is defined.
	if (BBCRow != 0 || BBCCol != 0)
	{
		for (int i = 1; i < 256; i++)
		{
			for (int s = 0; s < 2; ++s)
			{
				if (UserKeymap[i][s].row == BBCRow &&
					UserKeymap[i][s].col == BBCCol &&
					UserKeymap[i][s].shift == doingShifted)
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

					Keys += KeyName(i);
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

static LPCSTR KeyName(UINT Key)
{
	static CHAR Character[2]; // Used to return single characters.

	switch (Key)
	{
	case   8: return "Backspace";
	case   9: return "Tab";
	case  13: return "Enter";
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
