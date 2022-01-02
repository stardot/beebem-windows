/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2021  Tadeusz Kijkowski
Copyright (C) 2021  Chris Needham

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
#include <map>

#include "KeyNames.h"
#include "beebwin.h"
#include "StringUtils.h"

const char* KeyName(int Key)
{
	static const std::map<int, const char*> axisNamesMap = {
		{ JOYSTICK_AXIS_LEFT, "Left" },
		{ JOYSTICK_AXIS_RIGHT, "Right" },
		{ JOYSTICK_AXIS_UP, "Up" },
		{ JOYSTICK_AXIS_DOWN, "Down" },
		{ JOYSTICK_AXIS_Z_N, "Z-" },
		{ JOYSTICK_AXIS_Z_P, "Z+" },
		{ JOYSTICK_AXIS_RX_N, "RLeft" },
		{ JOYSTICK_AXIS_RX_P, "RRight" },
		{ JOYSTICK_AXIS_RY_N, "RUp" },
		{ JOYSTICK_AXIS_RY_P, "RDown" },
		{ JOYSTICK_AXIS_RZ_N, "RZ-" },
		{ JOYSTICK_AXIS_RZ_P, "RZ+" },
		{ JOYSTICK_AXIS_HAT_LEFT, "HatLeft" },
		{ JOYSTICK_AXIS_HAT_RIGHT, "HatRight" },
		{ JOYSTICK_AXIS_HAT_UP, "HatUp" },
		{ JOYSTICK_AXIS_HAT_DOWN, "HatDown" }
	};

	static char Character[2]; // Used to return single characters.

	if (Key >= BEEB_VKEY_JOY_START && Key < BEEB_VKEY_JOY_END)
	{
		static char Name[16]; // Buffer for joystick button or axis name

		Key -= BEEB_VKEY_JOY_START;

		int joyIdx = Key / (JOYSTICK_MAX_AXES + JOYSTICK_MAX_BUTTONS);
		Key -= joyIdx * (JOYSTICK_MAX_AXES + JOYSTICK_MAX_BUTTONS);

		sprintf(Name, "Joy%d", joyIdx + 1);

		if (Key < JOYSTICK_MAX_AXES)
		{
			auto iter = axisNamesMap.find(Key);
			if (iter != axisNamesMap.end())
			{
				strcat(Name, iter->second);
			}
			else
			{
				sprintf(Name + strlen(Name), "Axis%d", (Key / 2) + 1);
				strcat(Name, (Key & 1) ? "+" : "-");
			}
		}
		else
		{
			sprintf(Name + strlen(Name), "Btn%d", Key - JOYSTICK_MAX_AXES + 1);
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
		Character[0] = (char)LOBYTE(Key);
		Character[1] = '\0';
		return Character;
	}
}

int JoyVKeyByName(const char* Name)
{
	using VKeyMapType = std::map<std::string, int>;

	// Construct map on first use by lambda
	static const VKeyMapType nameToVKeyMap = []() {
		VKeyMapType keyMap{};

		for (int vkey = BEEB_VKEY_JOY_START; vkey < BEEB_VKEY_JOY_END; ++vkey)
		{
			keyMap[toupper(KeyName(vkey))] = vkey;
		}

		return keyMap;
	}();

	std::string uname = toupper(Name);
	auto iter = nameToVKeyMap.find(uname);
	if (iter != nameToVKeyMap.end())
		return iter->second;

	return -1;
}
