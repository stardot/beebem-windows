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

#include "beebwin.h"
#include "beebemrc.h"
#include "main.h"
#include "userkybd.h"
#include "sysvia.h"
#include "filedialog.h"
#include "SelectKeyDialog.h"

#define JOYMAP_TOKEN "*** BeebEm Joystick Map ***"

static const char *CFG_OPTIONS_STICKS = "Sticks";
static const char *CFG_OPTIONS_STICKS2 = "Sticks2";
static const char *CFG_OPTIONS_STICKS_TO_KEYS = "SticksToKeys";
static const char *CFG_OPTIONS_AUTOLOAD_JOYSICK_MAP = "AutoloadJoystickMap";
static const char *CFG_OPTIONS_STICK_DEADBAND = "StickToKeysDeadBand";

#define DEFAULT_JOY_DEADBAND 4096

/*****************************************************************************/
HWND JoystickHandler::GetHwnd() { return m_BeebWin->m_hWnd; }

int JoystickHandler::GetXWinSize() { return m_BeebWin->m_XWinSize; }

int JoystickHandler::GetYWinSize() { return m_BeebWin->m_YWinSize; }

void JoystickHandler::CheckMenuItem(UINT id, bool checked) { m_BeebWin->CheckMenuItem(id, checked); }

void JoystickHandler::EnableMenuItem(UINT id, bool enabled) { m_BeebWin->EnableMenuItem(id, enabled); }

/*****************************************************************************/
/* All this stuff will go away when joystick configuration is done via dialog
 */
struct JoystickMenuIdsType {
	UINT Joysticks[2];
	UINT Axes[3];
	UINT AnalogMousestick;
	UINT DigitalMousestick;
};

/*****************************************************************************/
constexpr JoystickMenuIdsType JoystickMenuIds[NUM_BBC_JOYSTICKS] {
	{
		{ IDM_JOYSTICK, IDM_JOY1_PCJOY2 },
		{ IDM_JOY1_PRIMARY, IDM_JOY1_SECONDARY1, IDM_JOY1_SECONDARY2 },
		IDM_ANALOGUE_MOUSESTICK,
		IDM_DIGITAL_MOUSESTICK,
	},
	{
		{ IDM_JOY2_PCJOY1, IDM_JOY2_PCJOY2 },
		{ IDM_JOY2_PRIMARY, IDM_JOY2_SECONDARY1, IDM_JOY2_SECONDARY2 },
		IDM_JOY2_ANALOGUE_MOUSESTICK,
		IDM_JOY2_DIGITAL_MOUSESTICK,
	}
};


/****************************************************************************/
int JoystickHandler::MenuIdToStick(int bbcIdx, UINT menuId)
{
	for (int pcIdx = 0; pcIdx < _countof(JoystickMenuIdsType::Joysticks); ++pcIdx)
	{
		if (menuId == JoystickMenuIds[bbcIdx].Joysticks[pcIdx])
			return pcIdx + 1;
	}
	return 0;
}

/****************************************************************************/
UINT JoystickHandler::StickToMenuId(int bbcIdx, int pcStick)
{
	if (pcStick > 0 && pcStick - 1 < _countof(JoystickMenuIdsType::Joysticks))
		return JoystickMenuIds[bbcIdx].Joysticks[pcStick - 1];
	return 0;
}

/****************************************************************************/
int JoystickHandler::MenuIdToAxes(int bbcIdx, UINT menuId)
{
	for (int axesIdx = 0; axesIdx < _countof(JoystickMenuIdsType::Axes); ++axesIdx)
	{
		if (menuId == JoystickMenuIds[bbcIdx].Axes[axesIdx])
			return axesIdx + 1;
	}
}

/****************************************************************************/
UINT JoystickHandler::AxesToMenuId(int bbcIdx, int pcAxes)
{
	if (pcAxes > 0 && pcAxes - 1 < _countof(JoystickMenuIdsType::Axes))
		return JoystickMenuIds[bbcIdx].Axes[pcAxes - 1];
	return 0;
}

/****************************************************************************/
// Update joystick configuration from checked menu items
void JoystickHandler::UpdateJoystickConfig(int bbcIdx)
{
	m_JoystickConfig[bbcIdx].Enabled = (m_MenuIdAxes[bbcIdx] != 0);
	m_JoystickConfig[bbcIdx].PCStick = MenuIdToStick(bbcIdx, m_MenuIdSticks[bbcIdx]);
	m_JoystickConfig[bbcIdx].PCAxes = MenuIdToAxes(bbcIdx, m_MenuIdAxes[bbcIdx]);
	m_JoystickConfig[bbcIdx].AnalogMousestick = (m_MenuIdAxes[bbcIdx] == JoystickMenuIds[bbcIdx].AnalogMousestick);
	m_JoystickConfig[bbcIdx].DigitalMousestick = (m_MenuIdAxes[bbcIdx] == JoystickMenuIds[bbcIdx].DigitalMousestick);
}

/****************************************************************************/
// Check if this PC joystick is assigned to this BBC joystick
bool JoystickHandler::IsPCJoystickAssigned(int pcIdx, int bbcIdx)
{
	return m_JoystickConfig[bbcIdx].PCStick == pcIdx + 1;
}

/****************************************************************************/
// Check if PC joystick is assigned to any BBC joystick
// TODO: Update m_PCStickForJoystick early and use it here
bool JoystickHandler::IsPCJoystickOn(int pcIdx)
{
	if (pcIdx >= _countof(JoystickMenuIdsType::Joysticks))
		return false;

	for (int bbcIdx = 0; bbcIdx < NUM_BBC_JOYSTICKS; ++bbcIdx)
	{
		if (IsPCJoystickAssigned(pcIdx, bbcIdx))
			return true;
	}

	return false;
}

/*****************************************************************************/
void JoystickHandler::InitMenu()
{
	for (int bbcIdx = 0; bbcIdx < NUM_BBC_JOYSTICKS; ++bbcIdx)
	{
		for (int pcIdx = 0; pcIdx < _countof(JoystickMenuIdsType::Joysticks); ++pcIdx)
		{
			CheckMenuItem(JoystickMenuIds[bbcIdx].Joysticks[pcIdx], false);
		}
		CheckMenuItem(JoystickMenuIds[bbcIdx].AnalogMousestick, false);
		CheckMenuItem(JoystickMenuIds[bbcIdx].DigitalMousestick, false);
		if (m_MenuIdSticks[bbcIdx] != 0)
			CheckMenuItem(m_MenuIdSticks[bbcIdx], true);

		for (int axesIdx = 0; axesIdx < _countof(JoystickMenuIdsType::Joysticks); ++axesIdx)
		{
			CheckMenuItem(JoystickMenuIds[bbcIdx].Axes[axesIdx], false);
		}
		if (m_MenuIdAxes[bbcIdx] != 0)
			CheckMenuItem(m_MenuIdAxes[bbcIdx], true);

		UpdateJoystickConfig(bbcIdx);
	}

	CheckMenuItem(IDM_JOYSTICK_TO_KEYS, m_JoystickToKeys);
	CheckMenuItem(IDM_AUTOLOADJOYMAP, m_AutoloadJoystickMap);
}

/****************************************************************************/
void JoystickHandler::UpdateJoystickMenu()
{
	bool EnableInit = false;

	// Enable "Initialize Joysticks" menu item if any more joysticks could be possibly captured (or always?)
	for (int pcIdx = 0; pcIdx < NUM_PC_JOYSTICKS; ++pcIdx)
	{
		if ((m_JoystickToKeys || IsPCJoystickOn(pcIdx)) && !m_PCJoystickState[pcIdx].Captured)
			EnableInit = true;
	}
	EnableMenuItem(IDM_INIT_JOYSTICK, EnableInit);

	// Check "Initialize Joysticks" menu item if any joystick is already captured
	CheckMenuItem(IDM_INIT_JOYSTICK, m_PCJoystickState[0].Captured);

	// Enable axes menu items if any PC joystick is assigned to the related BBC joystick
	for (int bbcIdx = 0; bbcIdx < NUM_BBC_JOYSTICKS; ++bbcIdx)
	{
		bool EnableAxes = false;
		for (int pcIdx = 0; pcIdx < NUM_PC_JOYSTICKS; ++pcIdx)
		{
			if (IsPCJoystickAssigned(pcIdx, bbcIdx))
				EnableAxes = true;
		}
		for (int axesIdx = 0; axesIdx < _countof(JoystickMenuIdsType::Axes); ++axesIdx)
			EnableMenuItem(JoystickMenuIds[bbcIdx].Axes[axesIdx], EnableAxes);
	}
}

/****************************************************************************/
void JoystickHandler::ProcessMenuCommand(int bbcIdx, UINT MenuId)
{
	/* Disable current selection */
	if (m_MenuIdSticks[bbcIdx] != 0)
	{
		CheckMenuItem(m_MenuIdSticks[bbcIdx], false);

		// Reset joystick position to centre
		JoystickX[bbcIdx] = 32767;
		JoystickY[bbcIdx] = 32767;

		SetJoystickButton(bbcIdx, false);

		if (bbcIdx == 0 && !m_JoystickConfig[1].Enabled)
		{
			SetJoystickButton(1, false);
		}
	}

	if (MenuId == m_MenuIdSticks[bbcIdx])
	{
		/* Joystick switched off completely */
		m_MenuIdSticks[bbcIdx] = 0;
	}
	else
	{
		/* Initialise new selection */
		m_MenuIdSticks[bbcIdx] = MenuId;

		CheckMenuItem(m_MenuIdSticks[bbcIdx], true);
	}
	UpdateJoystickConfig(bbcIdx);
	InitJoystick(false);

}

/****************************************************************************/
void JoystickHandler::ProcessAxesMenuCommand(int bbcIdx, UINT MenuId)
{
	CheckMenuItem(m_MenuIdAxes[bbcIdx], false);
	m_MenuIdAxes[bbcIdx] = MenuId;
	UpdateJoystickConfig(bbcIdx);
	CheckMenuItem(m_MenuIdAxes[bbcIdx], true);
}

/****************************************************************************/
void JoystickHandler::ToggleJoystickToKeys(void)
{
	m_JoystickToKeys = !m_JoystickToKeys;
	InitJoystick(false);
	CheckMenuItem(IDM_JOYSTICK_TO_KEYS, m_JoystickToKeys);
}

/****************************************************************************/
void JoystickHandler::ToggleAutoloadJoystickMap(void)
{
	m_AutoloadJoystickMap = !m_AutoloadJoystickMap;
	CheckMenuItem(IDM_AUTOLOADJOYMAP, m_AutoloadJoystickMap);
}

/****************************************************************************/
void JoystickHandler::ResetJoystick(void)
{
	for (int pcIdx = 0; pcIdx < NUM_PC_JOYSTICKS; ++pcIdx)
	{
		m_PCJoystickState[pcIdx].Captured = false;
		TranslateJoystick(pcIdx);
	}

}

/****************************************************************************/
bool JoystickHandler::InitJoystick(bool verbose)
{
	bool Success = true;

	ResetJoystick();

	// TODO: Joystick ordering here
	if (IsPCJoystickOn(0) || IsPCJoystickOn(1) || m_JoystickToKeys)
	{
		for (int pcIdx = 0; pcIdx < NUM_PC_JOYSTICKS; ++pcIdx)
		{
			Success = CaptureJoystick(pcIdx, verbose);
			if (!Success)
				break;
		}

		if (!m_JoystickTimerRunning)
		{
			SetTimer(GetHwnd(), 3, 20, NULL);
			m_JoystickTimerRunning = true;
		}
	}
	else
	{
		if (m_JoystickTimerRunning)
		{
			KillTimer(GetHwnd(), 3);
			m_JoystickTimerRunning = false;
		}
	}

	UpdateJoystickMenu();

	return Success;
}

/****************************************************************************/
// TODO: Change it all - scan all joysticks first and then order and assign
bool JoystickHandler::CaptureJoystick(int Index, bool verbose)
{
	bool success = false;

	DWORD JoyIndex;
	DWORD Result;
	JOYINFOEX joyInfoEx;
	UINT numDevs = joyGetNumDevs();

	// Scan for first present joystick index. It doesn't have to be
	// consecutive number.
	if (Index == 0)
		JoyIndex = 0;
	else
		JoyIndex = m_PCJoystickState[Index - 1].JoyIndex + 1;

	// Find first joystick that is known AND connected
	Result = JOYERR_UNPLUGGED;
	while (Result != JOYERR_NOERROR && JoyIndex != numDevs)
	{
		memset(&joyInfoEx, 0, sizeof(joyInfoEx));
		joyInfoEx.dwSize = sizeof(joyInfoEx);
		joyInfoEx.dwFlags = JOY_RETURNBUTTONS;

		Result = joyGetDevCaps(JoyIndex, &m_PCJoystickState[Index].Caps, sizeof(JOYCAPS));
		if (Result == JOYERR_NOERROR)
			Result = joyGetPosEx(JoyIndex, &joyInfoEx);
		if (Result != JOYERR_NOERROR)
			++JoyIndex;
	}

	if (Result == ERROR_SUCCESS)
	{
		m_PCJoystickState[Index].Captured = true;
		m_PCJoystickState[Index].JoyIndex = JoyIndex;
		success = true;
	}
	else if (verbose)
	{
		if (Result == ERROR_DEVICE_NOT_CONNECTED || Result == JOYERR_UNPLUGGED)
		{
			mainWin->Report(MessageType::Warning,
			                "Joystick %d is not plugged in", Index + 1);
		}
		else
		{
			mainWin->Report(MessageType::Warning,
			                "Failed to initialise joystick %d", Index + 1);
		}
	}

	return success;
}

/****************************************************************************/
void JoystickHandler::SetJoystickButton(int index, bool value)
{
	JoystickButton[index] = value;
}

/****************************************************************************/
void JoystickHandler::ScaleJoystick(int index, unsigned int x, unsigned int y,
	unsigned int minX, unsigned int minY, unsigned int maxX, unsigned int maxY)
{
	/* Scale and reverse the readings */
	JoystickX[index] = (int)((double)(maxX - x) * 65535.0 /
		(double)(maxX - minX));
	JoystickY[index] = (int)((double)(maxY - y) * 65535.0 /
		(double)(maxY - minY));
}

/****************************************************************************/
void JoystickHandler::SetMousestickButton(int index, bool value)
{
	if (index == 0)
	{
		// Left mouse button
		for (int bbcIdx = 0; bbcIdx < NUM_BBC_JOYSTICKS; ++bbcIdx)
		{
			if (m_JoystickConfig[bbcIdx].AnalogMousestick ||
				m_JoystickConfig[bbcIdx].DigitalMousestick)
			{
				SetJoystickButton(bbcIdx, value);
			}
		}
	}
	else
	{
		// Map right mouse button to secondary fire if second joystick is
		// not enabled.
		if (!m_JoystickConfig[1].Enabled && 
			(m_JoystickConfig[0].AnalogMousestick || m_JoystickConfig[0].DigitalMousestick))
		{ 
			SetJoystickButton(1, value);
		}
	}
}

/****************************************************************************/
void JoystickHandler::ScaleMousestick(unsigned int x, unsigned int y)
{
	for (int index = 0; index < 2; ++index)
	{
		int XPos = (GetXWinSize() - x) * 65535 / GetXWinSize();
		int YPos = (GetYWinSize() - y) * 65535 / GetYWinSize();

		if (m_JoystickConfig[index].AnalogMousestick)
		{
			JoystickX[index] = XPos;
			JoystickY[index] = YPos;
		}
		else if (m_JoystickConfig[index].DigitalMousestick)
		{
			const int Threshold = 2000;

			if (XPos < 32768 - Threshold)
			{
				JoystickX[index] = 0;
			}
			else if (XPos > 32768 + Threshold)
			{
				JoystickX[index] = 65535;
			}
			else
			{
				JoystickX[index] = 32768;
			}

			if (YPos < 32768 - Threshold)
			{
				JoystickY[index] = 0;
			}
			else if (YPos > 32768 + Threshold)
			{
				JoystickY[index] = 65535;
			}
			else
			{
				JoystickY[index] = 32768;
			}
		}
	}
}

/****************************************************************************/
unsigned int JoystickHandler::GetJoystickAxes(const JOYCAPS& caps, int deadband, const JOYINFOEX& joyInfoEx)
{
	using Info = JOYINFOEX;
	using Caps = JOYCAPS;
	unsigned int axes = 0;

	auto Scale = [&caps, &joyInfoEx](DWORD Info::*pos, UINT Caps::*min, UINT Caps::*max) {
		return (int)((double)(joyInfoEx.*pos - caps.*min) * 65535.0 /
			(double)(caps.*max - caps.*min));
	};

	auto Detect = [&axes, deadband](const int val, const int nbit, const int pbit) {
		if (val < 32768 - deadband)
			axes |= (1 << nbit);
		else if (val > 32768 + deadband)
			axes |= (1 << pbit);
	};

	int ty = Scale(&Info::dwYpos, &Caps::wYmin, &Caps::wYmax);
	int tx = Scale(&Info::dwXpos, &Caps::wXmin, &Caps::wXmax);
	int tz = Scale(&Info::dwZpos, &Caps::wZmin, &Caps::wZmin);
	int tr = Scale(&Info::dwRpos, &Caps::wRmin, &Caps::wRmax);
	int tu = Scale(&Info::dwUpos, &Caps::wUmin, &Caps::wUmax);
	int tv = Scale(&Info::dwVpos, &Caps::wVmin, &Caps::wVmax);

	Detect(ty, JOYSTICK_AXIS_UP, JOYSTICK_AXIS_DOWN);
	Detect(tx, JOYSTICK_AXIS_LEFT, JOYSTICK_AXIS_RIGHT);
	Detect(tz, JOYSTICK_AXIS_Z_N, JOYSTICK_AXIS_Z_P);
	Detect(tr, JOYSTICK_AXIS_R_N, JOYSTICK_AXIS_R_P);
	Detect(tu, JOYSTICK_AXIS_U_N, JOYSTICK_AXIS_U_P);
	Detect(tv, JOYSTICK_AXIS_V_N, JOYSTICK_AXIS_V_P);

	if (joyInfoEx.dwPOV != JOY_POVCENTERED)
	{
		if (joyInfoEx.dwPOV >= 29250 || joyInfoEx.dwPOV < 6750)
			axes |= (1 << JOYSTICK_AXIS_HAT_UP);
		if (joyInfoEx.dwPOV >= 2250 && joyInfoEx.dwPOV < 15750)
			axes |= (1 << JOYSTICK_AXIS_HAT_RIGHT);
		if (joyInfoEx.dwPOV >= 11250 && joyInfoEx.dwPOV < 24750)
			axes |= (1 << JOYSTICK_AXIS_HAT_DOWN);
		if (joyInfoEx.dwPOV >= 20250 && joyInfoEx.dwPOV < 33750)
			axes |= (1 << JOYSTICK_AXIS_HAT_LEFT);
	}

	return axes;
}

/****************************************************************************/
// Translate joystick position changes to key up or down message
void JoystickHandler::TranslateAxes(int joyId, unsigned int axesState)
{
	int vkeys = BEEB_VKEY_JOY_START + joyId * (JOYSTICK_MAX_AXES + JOYSTICK_MAX_BTNS);
	unsigned int& prevAxes = m_PCJoystickState[joyId].PrevAxes;

	if (axesState != prevAxes)
	{
		for (int axId = 0; axId < JOYSTICK_AXES_COUNT; ++axId)
		{
			if ((axesState & ~prevAxes) & (1 << axId))
			{
				TranslateOrSendKey(vkeys + axId, false);
			}
			else if ((~axesState & prevAxes) & (1 << axId))
			{
				TranslateOrSendKey(vkeys + axId, true);
			}
		}
		prevAxes = axesState;
	}
}

/****************************************************************************/
void JoystickHandler::TranslateJoystickButtons(int joyId, unsigned int buttons)
{
	const int BUTTON_COUNT = 32;

	unsigned int& prevBtns = m_PCJoystickState[joyId].PrevBtns;
	int vkeys = BEEB_VKEY_JOY_START + JOYSTICK_MAX_AXES
			+ joyId * (JOYSTICK_MAX_AXES + JOYSTICK_MAX_BTNS);

	if (buttons != prevBtns)
	{
		for (int btnId = 0; btnId < BUTTON_COUNT; ++btnId)
		{
			if ((buttons & ~prevBtns) & (1 << btnId))
			{
				TranslateOrSendKey(vkeys + btnId, false);
			}
			else if ((~buttons & prevBtns) & (1 << btnId))
			{
				TranslateOrSendKey(vkeys + btnId, true);
			}
		}

		prevBtns = buttons;
	}
}

/****************************************************************************/
void JoystickHandler::TranslateOrSendKey(int vkey, bool keyUp)
{
	if (m_JoystickTarget == nullptr)
	{
		int row, col;
		m_BeebWin->TranslateKey(vkey, keyUp, row, col);
	}
	else if (!keyUp)
	{
		// Keyboard input dialog is visible - translate button down to key down
		// message and send to dialog
		PostMessage(m_JoystickTarget, WM_KEYDOWN, vkey, 0);
	}
}

/****************************************************************************/
void JoystickHandler::TranslateJoystick(int joyId)
{
	static const JOYCAPS dummyJoyCaps = {
		0, 0, "",
		0, 65535, 0, 65535, 0, 65535,
		16, 10, 1000,
		0, 65535, 0, 65535, 0, 65535
	};
	bool success = false;
	JOYINFOEX joyInfoEx;

	const JOYCAPS* joyCaps = &m_PCJoystickState[joyId].Caps;
	DWORD joyIndex = m_PCJoystickState[joyId].JoyIndex;

	joyInfoEx.dwSize = sizeof(joyInfoEx);
	joyInfoEx.dwFlags = JOY_RETURNALL | JOY_RETURNPOVCTS;

	if (m_PCJoystickState[joyId].Captured &&
	    joyGetPosEx(joyIndex, &joyInfoEx) == JOYERR_NOERROR)
		success = true;

	// If joystick is disabled or error occurs, reset all axes and buttons
	if (!success)
	{
		joyCaps = &dummyJoyCaps;
		joyInfoEx.dwXpos = joyInfoEx.dwYpos = joyInfoEx.dwZpos = 32768;
		joyInfoEx.dwRpos = joyInfoEx.dwUpos = joyInfoEx.dwVpos = 32768;
		joyInfoEx.dwPOV = JOY_POVCENTERED;
		// Reset 'captured' flag and update menu entry
		m_PCJoystickState[joyId].Captured = false;
		UpdateJoystickMenu();
	}

	// PC joystick to BBC joystick
	for (int bbcIndex = 0; bbcIndex < NUM_BBC_JOYSTICKS; ++bbcIndex)
	{
		if (IsPCJoystickAssigned(joyId, bbcIndex))
		{
			if (bbcIndex == 1 && m_JoystickConfig[0].PCStick == m_JoystickConfig[1].PCStick
				&& m_JoystickConfig[0].PCAxes != m_JoystickConfig[1].PCAxes)
			{
				// If both BBC joysticks are mapped to the same PC gamepad, but not
				// the same axes, map buttons 2 and 4 to the Beeb's PB1 input
				SetJoystickButton(bbcIndex, (joyInfoEx.dwButtons & (JOY_BUTTON2 | JOY_BUTTON4)) != 0);
			}
			else
			{
				SetJoystickButton(bbcIndex, (joyInfoEx.dwButtons & (JOY_BUTTON1 | JOY_BUTTON3)) != 0);
			}

			if (bbcIndex == 0 && !m_JoystickConfig[1].Enabled)
			{
				// Second BBC joystick not enabled - map buttons 2 and 4 to Beeb's PB1 input
				SetJoystickButton(1, (joyInfoEx.dwButtons & (JOY_BUTTON2 | JOY_BUTTON4)) != 0);
			}

			int axesConfig = m_JoystickConfig[bbcIndex].PCAxes;
			if (axesConfig == 1)
			{
				ScaleJoystick(bbcIndex, joyInfoEx.dwXpos, joyInfoEx.dwYpos,
					joyCaps->wXmin, joyCaps->wYmin,
					joyCaps->wXmax, joyCaps->wYmax);
			}
			else if (axesConfig == 2)
			{
				ScaleJoystick(bbcIndex, joyInfoEx.dwUpos, joyInfoEx.dwRpos,
					joyCaps->wUmin, joyCaps->wRmin,
					joyCaps->wUmax, joyCaps->wRmax);
			}
			else if (axesConfig == 3)
			{
				ScaleJoystick(bbcIndex, joyInfoEx.dwZpos, joyInfoEx.dwRpos,
					joyCaps->wZmin, joyCaps->wRmin,
					joyCaps->wZmax, joyCaps->wRmax);
			}
		}
	}

	// Joystick to keyboard mapping
	if (m_JoystickToKeys)
	{
		auto axes = GetJoystickAxes(*joyCaps, m_Deadband, joyInfoEx);
		TranslateAxes(joyId, axes);
		TranslateJoystickButtons(joyId, joyInfoEx.dwButtons);
		m_PCJoystickState[joyId].JoystickToKeysActive = true;
	}

	// Make sure to reset keyboard state
	if (!m_JoystickToKeys && m_PCJoystickState[joyId].JoystickToKeysActive)
	{
		TranslateAxes(joyId, 0);
		TranslateJoystickButtons(joyId, 0);
		m_PCJoystickState[joyId].JoystickToKeysActive = false;
	}
}

/*****************************************************************************/
void JoystickHandler::UpdateJoysticks()
{
	for (int idx = 0; idx < NUM_PC_JOYSTICKS; ++idx)
	{
		if (m_PCJoystickState[0].Captured)
			TranslateJoystick(idx);
	}
}

/*****************************************************************************/
// Look for file specific joystick map
void JoystickHandler::CheckForJoystickMap(const char *path)
{
	char file[_MAX_PATH];
	char drive[_MAX_DRIVE];
	char dir[_MAX_DIR];
	char filename[_MAX_FNAME];

	if (!m_AutoloadJoystickMap || !path || !path[0])
		return;

	_splitpath(path, drive, dir, filename, NULL);

	// Look for prefs file with the same name with ".jmap" extension
	_makepath(file, drive, dir, filename, "jmap");

	if (GetFileAttributes(file) != INVALID_FILE_ATTRIBUTES)
	{
		ReadJoyMap(file, &JoystickMap);
	}
	else
	{
		// Mapping file not found - reset to default
		ResetJoyMapToDefaultUser();

		// Set jmap path even if the file doesn't exist
		strcpy(m_JoystickMapPath, file);
	}
}

/****************************************************************************/
void JoystickHandler::ResetJoystickMap()
{
	if (mainWin->Report(MessageType::Question,
	                    "Clear joystick to keyboard mapping table?") != MessageResult::Yes)
	{
		return;
	}

	ResetJoyMap(&JoystickMap);
}

/****************************************************************************/
void JoystickHandler::ResetJoyMap(JoyMap* joymap)
{
	// Initialize all input to unassigned
	for (auto& mapping : *joymap)
	{
		mapping[0].row = mapping[1].row = UNASSIGNED_ROW;
		mapping[0].col = mapping[1].col = 0;
		mapping[0].shift = false;
		mapping[1].shift = true;
	}
}

/****************************************************************************/
void JoystickHandler::ResetJoyMapToDefaultUser(void)
{
	char keymap[_MAX_PATH];
	strcpy(keymap, "DefaultUser.jmap");
	m_BeebWin->GetDataPath(m_BeebWin->m_UserDataPath, keymap);
	ResetJoyMap(&JoystickMap);
	if (GetFileAttributes(keymap) != INVALID_FILE_ATTRIBUTES)
		ReadJoyMap(keymap, &JoystickMap);
}

/****************************************************************************/
static void makeupper(char* str)
{
	if (str == NULL)
		return;
	while (*str)
	{
		*str = static_cast<char>(toupper(*str));
		++str;
	}
}

/****************************************************************************/
bool JoystickHandler::ReadJoyMap(const char *filename, JoyMap *joymap)
{
	bool success = true;
	FILE *infile = m_BeebWin->OpenReadFile(filename, "key map", JOYMAP_TOKEN "\n");
	char buf[256];
	JoyMap newJoyMap;
	int line = 1;

	if (infile == NULL)
		return false;

	ResetJoyMap(&newJoyMap);

	while (fgets(buf, 255, infile) != NULL)
	{
		char *ptr, *inputName, *keyName1, *keyName2;

		++line;

		// Read at most three tokens from line
		inputName = strtok_s(buf, " \t\n", &ptr);
		keyName1 = strtok_s(NULL, " \t\n", &ptr);
		keyName2 = strtok_s(NULL, " \t\n", &ptr);

		// Ignore empty lines or lines starting with '#'
		if (inputName == NULL || inputName[0] == '#')
			continue;

		if (keyName1 == NULL)
		{
			mainWin->Report(MessageType::Error,
			                "Invalid line in joystick mapping file:\n  %s\n  line %d",
			                filename, line);

			success = false;
			break;
		}

		// Get vkey number and mapping entry from input name
		int vkey = SelectKeyDialog::JoyVKeyByName(inputName);
		if (vkey < BEEB_VKEY_JOY_START || vkey >= BEEB_VKEY_JOY_END)
		{
			mainWin->Report(MessageType::Error,
			                "Invalid input name in joystick mapping file:\n  %s\n  line %d",
			                filename, line);

			success = false;
			break;
		}
		KeyMapping* mapping = newJoyMap[vkey - BEEB_VKEY_JOY_START];

		// Get shift state and BBC key from key name
		bool shift1 = false;
		makeupper(keyName1);

		if (strncmp(keyName1, "SH+", 3) == 0)
		{
			shift1 = true;
			keyName1 += 3;
		}

		const BBCKey* key1 = GetBBCKeyByName(keyName1);
		if (key1->row == UNASSIGNED_ROW && strcmp(keyName1, "NONE") != 0)
		{
			mainWin->Report(MessageType::Error,
			                "Invalid key name in joystick mapping file:\n  %s\n  line %d\n",
			                filename, line);

			success = false;
			break;
		}

		if (keyName2 == NULL)
		{
			// Shifted and unshifted input map to the same key
			mapping[0].row = mapping[1].row = key1->row;
			mapping[0].col = mapping[1].col = key1->column;
			mapping[0].shift = false;
			mapping[1].shift = true;
		}
		else
		{
			bool shift2 = false;
			makeupper(keyName2);

			if (strncmp(keyName2, "SH+", 3) == 0)
			{
				shift2 = true;
				keyName2 += 3;
			}

			const BBCKey* key2 = GetBBCKeyByName(keyName2);
			if (key2->row == UNASSIGNED_ROW && strcmp(keyName2, "NONE") != 0)
			{
				mainWin->Report(MessageType::Error,
				                "Invalid key name in joystick mapping file:\n  %s\n  line %d\n",
				                filename, line);

				success = false;
				break;
			}

			mapping[0].row = key1->row;
			mapping[0].col = key1->column;
			mapping[0].shift = shift1;
			mapping[1].row = key2->row;
			mapping[1].col = key2->column;
			mapping[1].shift = shift2;
		}
	}

	if (success)
	{
		memcpy(joymap, &newJoyMap, sizeof(newJoyMap));
	}

	return success;
}

/****************************************************************************/
void JoystickHandler::LoadJoystickMap()
{
	char DefaultPath[_MAX_PATH];
	char FileName[_MAX_PATH];
	const char* filter = "Joystick Map File (*.jmap)\0*.jmap\0";

	// If Autoload is enabled, look for joystick mapping near disks,
	// otherwise start in user data path.
	if (m_AutoloadJoystickMap)
	{
		m_BeebWin->GetPreferences().GetStringValue("DiscsPath", DefaultPath);
		m_BeebWin->GetDataPath(m_BeebWin->GetUserDataPath(), DefaultPath);
	}
	else
	{
		strcpy(DefaultPath, m_BeebWin->GetUserDataPath());
	}

	FileDialog fileDialog(GetHwnd(), FileName, sizeof(FileName), DefaultPath, filter);

	if (m_JoystickMapPath[0] == '\0' && !m_AutoloadJoystickMap)
	{
		fileDialog.SetInitial("DefaultUser.jmap");
	}
	else
	{
		fileDialog.SetInitial(m_JoystickMapPath);
	}

	if (fileDialog.Open())
	{
		if (ReadJoyMap(FileName, &JoystickMap))
			strcpy(m_JoystickMapPath, FileName);
	}
}

/****************************************************************************/
bool JoystickHandler::WriteJoyMap(const char *filename, JoyMap *joymap)
{
	FILE* outfile = m_BeebWin->OpenWriteFile(filename, "joystick map");

	if (outfile == NULL)
		return false;

	fprintf(outfile, JOYMAP_TOKEN "\n\n");

	for (int i = 0; i < BEEB_VKEY_JOY_COUNT; ++i)
	{
		KeyMapping* mapping = (*joymap)[i];
		if (mapping[0].row != UNASSIGNED_ROW
			|| mapping[1].row != UNASSIGNED_ROW)
		{
			const char* inputName = SelectKeyDialog::KeyName(i + BEEB_VKEY_JOY_START);

			if (mapping[0].row == mapping[1].row
				&& mapping[0].col == mapping[1].col
				&& !mapping[0].shift && mapping[1].shift)
			{
				// Shifted and unshifted input map to the same key - write one name
				const BBCKey* key = GetBBCKeyByRowAndCol(mapping[0].row, mapping[0].col);
				fprintf(outfile, "%-13s %s\n",
					inputName,
					key->name);
			}
			else
			{
				// Separate mapping for shifted and unshifted
				const BBCKey* key1 = GetBBCKeyByRowAndCol(mapping[0].row, mapping[0].col);
				bool key1shift = mapping[0].shift && mapping[0].row != UNASSIGNED_ROW;
				const BBCKey* key2 = GetBBCKeyByRowAndCol(mapping[1].row, mapping[1].col);
				bool key2shift = mapping[1].shift && mapping[1].row != UNASSIGNED_ROW;
				fprintf(outfile, "%-13s %s%-*s %s%s\n",
					inputName,
					key1shift ? "SH+" : "",
					// Align for longest possible: "SHIFT-LOCK+SH"
					key1shift ? 10 : 13,
					key1->name,
					key2shift ? "SH+" : "",
					key2->name);
			}
		}
	}

	fclose(outfile);

	return true;
}

/****************************************************************************/
static bool hasFileExt(const char* fileName, const char* fileExt)
{
	const size_t fileExtLen = strlen(fileExt);
	const size_t fileNameLen = strlen(fileName);

	return fileNameLen >= fileExtLen &&
		_stricmp(fileName + fileNameLen - fileExtLen, fileExt) == 0;
}

/****************************************************************************/
void JoystickHandler::SaveJoystickMap()
{
	char DefaultPath[_MAX_PATH];
	// Add space for .jmap exstension
	char FileName[_MAX_PATH + 5];
	const char* filter = "Joystick Map File (*.jmap)\0*.jmap\0";

	// If Autoload is enabled, store joystick mapping near disks,
	// otherwise start in user data path.
	if (m_AutoloadJoystickMap)
	{
		m_BeebWin->GetPreferences().GetStringValue("DiscsPath", DefaultPath);
		m_BeebWin->GetDataPath(m_BeebWin->GetUserDataPath(), DefaultPath);
	}
	else
	{
		strcpy(DefaultPath, m_BeebWin->GetUserDataPath());
	}

	FileDialog fileDialog(GetHwnd(), FileName, sizeof(FileName), DefaultPath, filter);

	if (m_JoystickMapPath[0] == '\0' && !m_AutoloadJoystickMap)
	{
		fileDialog.SetInitial("DefaultUser.jmap");
	}
	else
	{
		fileDialog.SetInitial(m_JoystickMapPath);
	}

	if (fileDialog.Save())
	{
		if (!hasFileExt(FileName, ".jmap"))
			strcat(FileName, ".jmap");

		if (WriteJoyMap(FileName, &JoystickMap))
			strcpy(m_JoystickMapPath, FileName);
	}
}

static const char *CFG_OPTIONS_STICK1_DEADBAND = "Stick1ToKeysDeadBand";
static const char *CFG_OPTIONS_STICK2_DEADBAND = "Stick2ToKeysDeadBand";

/****************************************************************************/
void JoystickHandler::ReadPreferences(Preferences& preferences)
{
	DWORD dword;
	if (preferences.GetDWORDValue(CFG_OPTIONS_STICKS, dword))
		m_MenuIdSticks[0] = dword;
	else
		m_MenuIdSticks[0] = 0;

	if (preferences.GetDWORDValue(CFG_OPTIONS_STICKS2, dword))
		m_MenuIdSticks[1] = dword;
	else
		m_MenuIdSticks[1] = 0;

	if (preferences.GetDWORDValue("JoystickAxes1", dword))
		m_JoystickConfig[0].PCAxes = dword;
	else
		m_JoystickConfig[0].PCAxes = 1;
	m_MenuIdAxes[0] = AxesToMenuId(0, m_JoystickConfig[0].PCAxes);

	if (preferences.GetDWORDValue("JoystickAxes2", dword))
		m_JoystickConfig[1].PCAxes = dword;
	else
		m_JoystickConfig[1].PCAxes = 1;
	m_MenuIdAxes[1] = AxesToMenuId(1, m_JoystickConfig[1].PCAxes);

	if (!preferences.GetBoolValue(CFG_OPTIONS_STICKS_TO_KEYS, m_JoystickToKeys))
		m_JoystickToKeys = false;

	if (!preferences.GetBoolValue(CFG_OPTIONS_AUTOLOAD_JOYSICK_MAP,
		m_AutoloadJoystickMap))
		m_AutoloadJoystickMap = false;

	preferences.EraseValue("Stick1ToKeysDeadBand");
	preferences.EraseValue("Stick2ToKeysDeadBand");

	if (preferences.GetDWORDValue(CFG_OPTIONS_STICK_DEADBAND, dword))
		m_Deadband = dword;
	else
		m_Deadband = DEFAULT_JOY_DEADBAND;
}

/****************************************************************************/
void JoystickHandler::WritePreferences(Preferences& preferences)
{
	preferences.SetDWORDValue(CFG_OPTIONS_STICKS, m_MenuIdSticks[0]);
	preferences.SetDWORDValue(CFG_OPTIONS_STICKS2, m_MenuIdSticks[1]);
	preferences.SetDWORDValue("JoystickAxes1", m_JoystickConfig[0].PCAxes);
	preferences.SetDWORDValue("JoystickAxes2", m_JoystickConfig[1].PCAxes);

	preferences.SetBoolValue(CFG_OPTIONS_STICKS_TO_KEYS, m_JoystickToKeys);
	preferences.SetBoolValue(CFG_OPTIONS_AUTOLOAD_JOYSICK_MAP, m_AutoloadJoystickMap);
	preferences.SetDWORDValue(CFG_OPTIONS_STICK_DEADBAND, m_Deadband);
}
