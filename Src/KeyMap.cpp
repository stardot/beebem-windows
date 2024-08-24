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

#include "KeyMap.h"
#include "Main.h"

// Keyboard mappings
KeyMap UserKeyMap;
KeyMap DefaultKeyMap;
KeyMap LogicalKeyMap;

// Currently selected translation table
const KeyMap *transTable = &DefaultKeyMap;

// Token written to start of map file
#define KEYMAP_TOKEN "*** BeebEm Keymap ***"

static const char* GetVirtualKeyCode(int Key);

/*--------------------------------------------------------------------------*/

static void ResetKeyMap(KeyMap* keymap)
{
	for (int PCKey = 0; PCKey < KEYMAP_SIZE; PCKey++)
	{
		for (int PCShift = 0; PCShift < 2; PCShift++)
		{
			(*keymap)[PCKey][PCShift].row = -9;
			(*keymap)[PCKey][PCShift].row = 0;
			UserKeyMap[PCKey][PCShift].shift = PCShift == 1;
		}
	}
}

/*--------------------------------------------------------------------------*/

void InitKeyMap()
{
	ResetKeyMap(&DefaultKeyMap);
	ResetKeyMap(&LogicalKeyMap);
	ResetKeyMap(&UserKeyMap);
}

/*--------------------------------------------------------------------------*/

bool ReadKeyMap(const char *filename, KeyMap *keymap)
{
	bool success = true;
	char buf[256];

	FILE *infile = fopen(filename,"r");

	if (infile == NULL)
	{
		mainWin->Report(MessageType::Error,
		                "Failed to read key map file:\n  %s", filename);

		success = false;
	}
	else
	{
		if (fgets(buf, 255, infile) == nullptr ||
		    strcmp(buf, KEYMAP_TOKEN "\n") != 0)
		{
			mainWin->Report(MessageType::Error,
			                "Invalid key map file:\n  %s\n", filename);

			success = false;
		}
		else
		{
			fgets(buf, 255, infile);

			for (int i = 0; i < KEYMAP_SIZE; ++i)
			{
				if (fgets(buf, 255, infile) == NULL)
				{
					mainWin->Report(MessageType::Error,
					                "Data missing from key map file:\n  %s\n", filename);

					success = false;
					break;
				}

				int shift0 = 0, shift1 = 0;

				sscanf(buf, "%d %d %d %d %d %d",
				       &(*keymap)[i][0].row,
				       &(*keymap)[i][0].col,
				       &shift0,
				       &(*keymap)[i][1].row,
				       &(*keymap)[i][1].col,
				       &shift1);

				(*keymap)[i][0].shift = shift0 != 0;
				(*keymap)[i][1].shift = shift1 != 0;
			}
		}

		fclose(infile);
	}

	return success;
}

/*--------------------------------------------------------------------------*/

bool WriteKeyMap(const char *filename, KeyMap *keymap)
{
	FILE *outfile = fopen(filename, "w");

	if (outfile == nullptr)
	{
		mainWin->Report(MessageType::Error,
		                "Failed to write key map file:\n  %s", filename);
		return false;
	}

	fprintf(outfile, KEYMAP_TOKEN "\n\n");

	for (int i = 0; i < KEYMAP_SIZE; i++)
	{
		fprintf(outfile, "%d %d %d %d %d %d # 0x%02X",
		        (*keymap)[i][0].row,
		        (*keymap)[i][0].col,
		        (*keymap)[i][0].shift,
		        (*keymap)[i][1].row,
		        (*keymap)[i][1].col,
		        (*keymap)[i][1].shift,
		        i);

		const char* KeyCode = GetVirtualKeyCode(i);

		if (KeyCode != nullptr)
		{
			fprintf(outfile, " %s", KeyCode);
		}

		fprintf(outfile, "\n");
	}

	fclose(outfile);

	return true;
}

/*--------------------------------------------------------------------------*/

#define CASE(constant) case constant: return (# constant)

static const char* GetVirtualKeyCode(int Key)
{
	static char Name[2];

	if ((Key >= '0' && Key <= '9') ||
	    (Key >= 'A' && Key <= 'Z'))
	{
		Name[0] = (char)Key;
		Name[1] = '\0';

		return Name;
	}

	switch (Key)
	{
		CASE(VK_LBUTTON);
		CASE(VK_RBUTTON);
		CASE(VK_CANCEL);
		CASE(VK_MBUTTON);
		CASE(VK_XBUTTON1);
		CASE(VK_XBUTTON2);
		CASE(VK_BACK);
		CASE(VK_TAB);
		CASE(VK_CLEAR);
		CASE(VK_RETURN);
		CASE(VK_SHIFT);
		CASE(VK_CONTROL);
		CASE(VK_MENU);
		CASE(VK_PAUSE);
		CASE(VK_CAPITAL);
		CASE(VK_KANA);
		// CASE(VK_HANGUL);
		// CASE(VK_IME_ON);
		CASE(VK_JUNJA);
		CASE(VK_FINAL);
		CASE(VK_HANJA);
		// CASE(VK_KANJI);
		// CASE(VK_IME_OFF);
		CASE(VK_ESCAPE);
		CASE(VK_CONVERT);
		CASE(VK_NONCONVERT);
		CASE(VK_ACCEPT);
		CASE(VK_MODECHANGE);
		CASE(VK_SPACE);
		CASE(VK_PRIOR);
		CASE(VK_NEXT);
		CASE(VK_END);
		CASE(VK_HOME);
		CASE(VK_LEFT);
		CASE(VK_UP);
		CASE(VK_RIGHT);
		CASE(VK_DOWN);
		CASE(VK_SELECT);
		CASE(VK_PRINT);
		CASE(VK_EXECUTE);
		CASE(VK_SNAPSHOT);
		CASE(VK_INSERT);
		CASE(VK_DELETE);
		CASE(VK_HELP);
		CASE(VK_LWIN);
		CASE(VK_RWIN);
		CASE(VK_APPS);
		CASE(VK_SLEEP);
		CASE(VK_NUMPAD0);
		CASE(VK_NUMPAD1);
		CASE(VK_NUMPAD2);
		CASE(VK_NUMPAD3);
		CASE(VK_NUMPAD4);
		CASE(VK_NUMPAD5);
		CASE(VK_NUMPAD6);
		CASE(VK_NUMPAD7);
		CASE(VK_NUMPAD8);
		CASE(VK_NUMPAD9);
		CASE(VK_MULTIPLY);
		CASE(VK_ADD);
		CASE(VK_SEPARATOR);
		CASE(VK_SUBTRACT);
		CASE(VK_DECIMAL);
		CASE(VK_DIVIDE);
		CASE(VK_F1);
		CASE(VK_F2);
		CASE(VK_F3);
		CASE(VK_F4);
		CASE(VK_F5);
		CASE(VK_F6);
		CASE(VK_F7);
		CASE(VK_F8);
		CASE(VK_F9);
		CASE(VK_F10);
		CASE(VK_F11);
		CASE(VK_F12);
		CASE(VK_F13);
		CASE(VK_F14);
		CASE(VK_F15);
		CASE(VK_F16);
		CASE(VK_F17);
		CASE(VK_F18);
		CASE(VK_F19);
		CASE(VK_F20);
		CASE(VK_F21);
		CASE(VK_F22);
		CASE(VK_F23);
		CASE(VK_F24);
		CASE(VK_NUMLOCK);
		CASE(VK_SCROLL);
		CASE(VK_LSHIFT);
		CASE(VK_RSHIFT);
		CASE(VK_LCONTROL);
		CASE(VK_RCONTROL);
		CASE(VK_LMENU);
		CASE(VK_RMENU);
		CASE(VK_BROWSER_BACK);
		CASE(VK_BROWSER_FORWARD);
		CASE(VK_BROWSER_REFRESH);
		CASE(VK_BROWSER_STOP);
		CASE(VK_BROWSER_SEARCH);
		CASE(VK_BROWSER_FAVORITES);
		CASE(VK_BROWSER_HOME);
		CASE(VK_VOLUME_MUTE);
		CASE(VK_VOLUME_DOWN);
		CASE(VK_VOLUME_UP);
		CASE(VK_MEDIA_NEXT_TRACK);
		CASE(VK_MEDIA_PREV_TRACK);
		CASE(VK_MEDIA_STOP);
		CASE(VK_MEDIA_PLAY_PAUSE);
		CASE(VK_LAUNCH_MAIL);
		CASE(VK_LAUNCH_MEDIA_SELECT);
		CASE(VK_LAUNCH_APP1);
		CASE(VK_LAUNCH_APP2);
		CASE(VK_OEM_1);
		CASE(VK_OEM_PLUS);
		CASE(VK_OEM_COMMA);
		CASE(VK_OEM_MINUS);
		CASE(VK_OEM_PERIOD);
		CASE(VK_OEM_2);
		CASE(VK_OEM_3);
		CASE(VK_OEM_4);
		CASE(VK_OEM_5);
		CASE(VK_OEM_6);
		CASE(VK_OEM_7);
		CASE(VK_OEM_8);
		CASE(VK_OEM_102);
		CASE(VK_PROCESSKEY);
		CASE(VK_PACKET);
		CASE(VK_ATTN);
		CASE(VK_CRSEL);
		CASE(VK_EXSEL);
		CASE(VK_EREOF);
		CASE(VK_PLAY);
		CASE(VK_ZOOM);
		CASE(VK_NONAME);
		CASE(VK_PA1);
		CASE(VK_OEM_CLEAR);
	}

	return nullptr;
}

/*--------------------------------------------------------------------------*/

const char* GetPCKeyName(int Key)
{
	static char Character[2]; // Used to return single characters.

	switch (Key)
	{
	case 0x08: return "Backspace";
	case 0x09: return "Tab";
	case 0x0D: return "Enter";
	case 0x10: return "Shift";
	case 0x11: return "Ctrl";
	case 0x12: return "Alt";
	case 0x13: return "Break";
	case 0x14: return "Caps";
	case 0x1B: return "Esc";
	case 0x20: return "Spacebar";
	case 0x21: return "PgUp";
	case 0x22: return "PgDn";
	case 0x23: return "End";
	case 0x24: return "Home";
	case 0x25: return "Left";
	case 0x26: return "Up";
	case 0x27: return "Right";
	case 0x28: return "Down";
	case 0x2D: return "Insert";
	case 0x2E: return "Del";
	case 0x5D: return "Menu";
	case 0x60: return "Pad0";
	case 0x61: return "Pad1";
	case 0x62: return "Pad2";
	case 0x63: return "Pad3";
	case 0x64: return "Pad4";
	case 0x65: return "Pad5";
	case 0x66: return "Pad6";
	case 0x67: return "Pad7";
	case 0x68: return "Pad8";
	case 0x69: return "Pad9";
	case 0x6A: return "Pad*";
	case 0x6B: return "Pad+";
	case 0x6D: return "Pad-";
	case 0x6E: return "Pad.";
	case 0x6F: return "Pad/";
	case 0x70: return "F1";
	case 0x71: return "F2";
	case 0x72: return "F3";
	case 0x73: return "F4";
	case 0x74: return "F5";
	case 0x75: return "F6";
	case 0x76: return "F7";
	case 0x77: return "F8";
	case 0x78: return "F9";
	case 0x79: return "F10";
	case 0x7A: return "F11";
	case 0x7B: return "F12";
	case 0x90: return "NumLock";
	case 0x91: return "SclLock";
	case 0xBA: return ";";
	case 0xBB: return "=";
	case 0xBC: return ",";
	case 0xBD: return "-";
	case 0xBE: return ".";
	case 0xBF: return "/";
	case 0xC0: return "\'";
	case 0xDB: return "[";
	case 0xDC: return "\\";
	case 0xDD: return "]";
	case 0xDE: return "#";
	case 0xDF: return "`";

	default:
		Character[0] = (char)LOBYTE(Key);
		Character[1] = '\0';
		return Character;
	}
}

/*--------------------------------------------------------------------------*/

void SetUserKeyMapping(int Row, int Column, bool BBCShift, int PCKey, bool PCShift)
{
	if (PCKey >= 0 && PCKey < KEYMAP_SIZE)
	{
		UserKeyMap[PCKey][static_cast<int>(PCShift)].row = Row;
		UserKeyMap[PCKey][static_cast<int>(PCShift)].col = Column;
		UserKeyMap[PCKey][static_cast<int>(PCShift)].shift = BBCShift;

		// DebugTrace("SetBBCKey: key=%d, shift=%d, row=%d, col=%d, bbcshift=%d\n",
		//            Key, (int)PCShift, Row, Col, BBCShift);
	}
}

/*--------------------------------------------------------------------------*/

// Clear any PC keys that correspond to a given BBC keyboard column, row,
// and shift state.

void ClearUserKeyMapping(int Row, int Column, bool Shift)
{
	for (int PCKey = 0; PCKey < KEYMAP_SIZE; PCKey++)
	{
		for (int PCShift = 0; PCShift < 2; PCShift++)
		{
			if (UserKeyMap[PCKey][PCShift].row == Row &&
			    UserKeyMap[PCKey][PCShift].col == Column &&
			    UserKeyMap[PCKey][PCShift].shift == Shift)
			{
				UserKeyMap[PCKey][PCShift].row = -9;
				UserKeyMap[PCKey][PCShift].col = 0;
				UserKeyMap[PCKey][PCShift].shift = PCShift  == 1;
			}
		}
	}
}

/*--------------------------------------------------------------------------*/

std::string GetKeysUsed(int Row, int Column, bool Shift)
{
	std::string Keys;

	// First see if this key is defined.
	if (Row != -9)
	{
		for (int PCKey = 0; PCKey < KEYMAP_SIZE; PCKey++)
		{
			for (int PCShift = 0; PCShift < 2; PCShift++)
			{
				if (UserKeyMap[PCKey][PCShift].row == Row &&
				    UserKeyMap[PCKey][PCShift].col == Column &&
				    UserKeyMap[PCKey][PCShift].shift == Shift)
				{
					// We have found a key that matches.
					if (!Keys.empty())
					{
						Keys += ", ";
					}

					if (PCShift == 1)
					{
						Keys += "Sh-";
					}

					Keys += GetPCKeyName(PCKey);
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
