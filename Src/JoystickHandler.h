/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 1994  Nigel Magnay
Copyright (C) 1997  Mike Wyatt
Copyright (C) 1998  Robert Schmidt
Copyright (C) 2001  Richard Gellman
Copyright (C) 2004  Ken Lowe
Copyright (C) 2004  Rob O'Donnell
Copyright (C) 2005  Jon Welch
Copyright (C) 2021  Chris Needham, Tadeusz Kijkowski

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

#ifndef BEEBWINJOYSTICK_H
#define BEEBWINJOYSTICK_H

#include "atodconv.h"
#include "beebemrc.h"
#include "keymapping.h"

#define NUM_PC_JOYSTICKS        4

#define JOYSTICK_MAX_AXES       16
#define JOYSTICK_MAX_BTNS       16

#define JOYSTICK_AXIS_UP        0
#define JOYSTICK_AXIS_DOWN      1
#define JOYSTICK_AXIS_LEFT      2
#define JOYSTICK_AXIS_RIGHT     3
#define JOYSTICK_AXIS_Z_N       4
#define JOYSTICK_AXIS_Z_P       5
#define JOYSTICK_AXIS_R_N       6
#define JOYSTICK_AXIS_R_P       7
#define JOYSTICK_AXIS_U_N       8
#define JOYSTICK_AXIS_U_P       9
#define JOYSTICK_AXIS_V_N       10
#define JOYSTICK_AXIS_V_P       11
#define JOYSTICK_AXIS_HAT_UP    12
#define JOYSTICK_AXIS_HAT_DOWN  13
#define JOYSTICK_AXIS_HAT_LEFT  14
#define JOYSTICK_AXIS_HAT_RIGHT 15
#define JOYSTICK_AXES_COUNT     16

#define BEEB_VKEY_JOY_START  256
#define BEEB_VKEY_JOY_COUNT  (NUM_PC_JOYSTICKS * \
			      (JOYSTICK_MAX_AXES + JOYSTICK_MAX_BTNS))   // 4*32 = 128
#define BEEB_VKEY_JOY_END    (BEEB_VKEY_JOY_START + BEEB_VKEY_JOY_COUNT) // 256+128 = 384

#define BEEB_VKEY_COUNT      BEEB_VKEY_JOY_END

typedef KeyPair     JoyMap[BEEB_VKEY_JOY_COUNT];

class BeebWin;

struct PCJoystickState {
	JOYCAPS      Caps{};
	unsigned int JoyIndex{ 0 };
	bool         Captured{ false };
	unsigned int PrevAxes{ 0 };
	unsigned int PrevBtns{ 0 };
	bool         JoystickToKeysActive{ false };
};

struct BBCJoystickConfig {
	bool Enabled{ false };
	int  PCStick{ 0 };
	int  PCAxes{ 0 };
	bool AnalogMousestick{ false };
	bool DigitalMousestick{ false };
};

class JoystickHandler
{
	BeebWin*	  m_BeebWin;
	bool		  m_JoystickTimerRunning{ false };
	PCJoystickState	  m_PCJoystickState[NUM_PC_JOYSTICKS];
	BBCJoystickConfig m_JoystickConfig[NUM_BBC_JOYSTICKS];
	int		  m_MenuIdSticks[NUM_BBC_JOYSTICKS]{};
	int		  m_MenuIdAxes[NUM_BBC_JOYSTICKS]{};
	int		  m_Deadband;
	bool		  m_JoystickToKeys{ false };
	bool		  m_AutoloadJoystickMap{ false };
	HWND		  m_JoystickTarget{ nullptr };
	char		  m_JoystickMapPath[_MAX_PATH]{};

public:
	JoystickHandler(BeebWin* beebWin) : m_BeebWin{ beebWin } {}

	/* Accessors */
	int GetMenuIdSticks(int bbcIdx) { return m_MenuIdSticks[bbcIdx]; }
	bool GetJoystickToKeys() { return m_JoystickToKeys; }
	void SetJoystickToKeys(bool enabled) { m_JoystickToKeys = enabled; }
	void SetJoystickTarget(HWND target) { m_JoystickTarget = target; }

	/* BeebWin access */
	HWND GetHwnd();
	int GetXWinSize();
	int GetYWinSize();
	void CheckMenuItem(UINT id, bool checked);
	void EnableMenuItem(UINT id, bool enabled);

	/* Menu handling - will soon be gone */
	int MenuIdToStick(int bbcIdx, UINT menuId);
	UINT StickToMenuId(int bbcIdx, int pcStick);
	int MenuIdToAxes(int bbcIdx, UINT menuId);
	UINT AxesToMenuId(int bbcIdx, int pcAxes);
	void UpdateJoystickConfig(int bbcIdx);
	bool IsPCJoystickAssigned(int pcIdx, int bbcIdx);
	bool IsPCJoystickOn(int pcIdx);
	void InitMenu(void);
	void UpdateJoystickMenu(void);
	void ProcessMenuCommand(int bbcIdx, UINT menuId);
	void ProcessAxesMenuCommand(int bbcIdx, UINT menuId);
	void ToggleJoystickToKeys();
	void ToggleAutoloadJoystickMap();

	/* Initialization */
	void ResetJoystick(void);
	bool InitJoystick(bool verbose = false);
	bool CaptureJoystick(int Index, bool verbose);

	/* BBC Analog */
	void SetJoystickButton(int index, bool value);
	void ScaleJoystick(int index, unsigned int x, unsigned int y,
		unsigned int minX, unsigned int minY,
		unsigned int maxX, unsigned int maxY);

	/* Mousestick */
	void SetMousestickButton(int index, bool button);
	void ScaleMousestick(unsigned int x, unsigned int y);

	/* Joystick to keyboard */
	unsigned int GetJoystickAxes(const JOYCAPS& caps, int deadband, const JOYINFOEX& joyInfoEx);
	void TranslateAxes(int joyId, unsigned int axesState);
	void TranslateJoystickButtons(int joyId, unsigned int buttons);
	void TranslateOrSendKey(int vkey, bool keyUp);
	void TranslateJoystick(int joyId);
	void UpdateJoysticks(void);

	/* Joystick to keyboard mapping */
	void CheckForJoystickMap(const char *path);
	void ResetJoystickMap(void);
	void ResetJoyMap(JoyMap* joymap);
	void ResetJoyMapToDefaultUser(void);
	bool ReadJoyMap(const char *filename, JoyMap *joymap);
	void LoadJoystickMap(void);
	void SaveJoystickMap(void);
	bool WriteJoyMap(const char *filename, JoyMap *joymap);

	/* Preferences */
	bool GetNthBoolValue(Preferences& preferences, const char* format, int idx, bool& value);
	bool GetNthDWORDValue(Preferences& preferences, const char* format, int idx, DWORD& value);
	void SetNthBoolValue(Preferences& preferences, const char* format, int idx, bool value);
	void SetNthDWORDValue(Preferences& preferences, const char* format, int idx, DWORD value);
	void ReadPreferences(Preferences& preferences);
	void WritePreferences(Preferences& preferences);
};

#endif
