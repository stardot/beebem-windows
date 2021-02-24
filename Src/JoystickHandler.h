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

#include <memory>

/* Max number of joysticks in joystickapi */
#define MAX_JOYSTICK_DEVS       16
/* Max number of joysticks to capture */
#define NUM_PC_JOYSTICKS        4
/* Max number of entries on joystick order list */
#define MAX_JOYSTICK_ORDER      16

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

class BeebWin;
class JoystickHandlerDetails;

class JoystickHandler
{
	std::unique_ptr<JoystickHandlerDetails> m_Details;

public:
	JoystickHandler(BeebWin* beebWin);
	~JoystickHandler();

	/* Accessors */
	int GetMenuIdSticks(int bbcIdx);
	bool GetJoystickToKeys();
	void SetJoystickToKeys(bool enabled);
	void SetJoystickTarget(HWND target);

	/* Menu handling - will soon be gone */
	void InitMenu(void);
	void ProcessMenuCommand(int bbcIdx, UINT menuId);
	void ProcessAxesMenuCommand(int bbcIdx, UINT menuId);
	void ToggleJoystickToKeys();
	void ToggleAutoloadJoystickMap();

	/* Initialization */
	bool InitJoystick(bool verbose = false);

	/* Mousestick */
	void SetMousestickButton(int index, bool button);
	void ScaleMousestick(unsigned int x, unsigned int y);

	/* Timer handler */
	void UpdateJoysticks(void);

	/* Joystick to keyboard mapping */
	void CheckForJoystickMap(const char* path);
	void ResetJoystickMap(void);
	void ResetJoyMapToDefaultUser(void);
	void LoadJoystickMap(void);
	void SaveJoystickMap(void);

	/* Preferences */
	void ReadPreferences(Preferences& preferences);
	void WritePreferences(Preferences& preferences);
	void WriteJoystickOrder(Preferences& preferences);
};

#endif
