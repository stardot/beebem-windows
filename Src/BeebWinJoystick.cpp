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
#include "filedialog.h"
#include "FileUtils.h"
#include "JoystickHandler.h"
#include "SelectKeyDialog.h"
#include "StringUtils.h"
#include "sysvia.h"
#include "userkybd.h"

#include <algorithm>

/* Backward compatibility */
static const char* CFG_OPTIONS_STICKS = "Sticks";

/* New joystick options */
static const char* CFG_OPTIONS_STICK_PCSTICK = "Joystick%dPCStick";
static const char* CFG_OPTIONS_STICK_PCAXES = "Joystick%dPCAxes";
static const char* CFG_OPTIONS_STICK_ANALOG = "Joystick%dAnalogMousepad";
static const char* CFG_OPTIONS_STICK_DIGITAL = "Joystick%dDigitalMousepad";
static const char* CFG_OPTIONS_JOYSTICK_SENSITIVITY = "JoystickSensitivity";
static const char* CFG_OPTIONS_JOYSTICK_ORDER = "JoystickOrder%d";

static const char* CFG_OPTIONS_STICKS_TO_KEYS = "JoysticksToKeys";
static const char* CFG_OPTIONS_AUTOLOAD_JOYSICK_MAP = "AutoloadJoystickMap";
static const char* CFG_OPTIONS_STICKS_THRESHOLD = "JoysticksToKeysThreshold";

/*****************************************************************************/
/* All this stuff will go away when joystick configuration is done via dialog
 */
struct JoystickMenuIdsType {
	UINT Joysticks[2];
	UINT Axes[2];
	UINT AnalogMousestick;
	UINT DigitalMousestick;
};

/*****************************************************************************/
static const JoystickMenuIdsType JoystickMenuIds[NUM_BBC_JOYSTICKS] = {
	{
		{ IDM_JOYSTICK, IDM_JOY1_PCJOY2 },
		{ IDM_JOY1_PRIMARY, IDM_JOY1_SECONDARY1 },
		IDM_ANALOGUE_MOUSESTICK,
		IDM_DIGITAL_MOUSESTICK,
	},
	{
		{ IDM_JOY2_PCJOY1, IDM_JOY2_PCJOY2 },
		{ IDM_JOY2_PRIMARY, IDM_JOY2_SECONDARY1 },
		IDM_JOY2_ANALOGUE_MOUSESTICK,
		IDM_JOY2_DIGITAL_MOUSESTICK,
	}
};

/****************************************************************************/
static int MenuIdToStick(int bbcIdx, UINT menuId)
{
	for (int pcIdx = 0; pcIdx < _countof(JoystickMenuIdsType::Joysticks); ++pcIdx)
	{
		if (menuId == JoystickMenuIds[bbcIdx].Joysticks[pcIdx])
		{
			return pcIdx + 1;
		}
	}

	return 0;
}

/****************************************************************************/
static UINT StickToMenuId(int bbcIdx, int pcStick)
{
	if (pcStick > 0 && pcStick - 1 < _countof(JoystickMenuIdsType::Joysticks))
	{
		return JoystickMenuIds[bbcIdx].Joysticks[pcStick - 1];
	}

	return 0;
}

/****************************************************************************/
static int MenuIdToAxes(int bbcIdx, UINT menuId)
{
	for (int axesIdx = 0; axesIdx < _countof(JoystickMenuIdsType::Axes); ++axesIdx)
	{
		if (menuId == JoystickMenuIds[bbcIdx].Axes[axesIdx])
		{
			return axesIdx;
		}
	}

	return 0;
}

/****************************************************************************/
static UINT AxesToMenuId(int bbcIdx, int pcAxes)
{
	if (pcAxes >= 0 && pcAxes < _countof(JoystickMenuIdsType::Axes))
	{
		return JoystickMenuIds[bbcIdx].Axes[pcAxes];
	}

	return 0;
}

/****************************************************************************/
void BeebWin::SetJoystickTargetWindow(HWND hWnd)
{
	m_JoystickTarget = hWnd;
}

/****************************************************************************/
// Update joystick configuration from checked menu items
void BeebWin::UpdateJoystickConfig(int bbcIdx)
{
	m_JoystickConfig[bbcIdx].Enabled = (m_MenuIdSticks[bbcIdx] != 0);
	m_JoystickConfig[bbcIdx].PCStick = MenuIdToStick(bbcIdx, m_MenuIdSticks[bbcIdx]);
	m_JoystickConfig[bbcIdx].PCAxes = MenuIdToAxes(bbcIdx, m_MenuIdAxes[bbcIdx]);
	m_JoystickConfig[bbcIdx].AnalogMousestick =
		m_MenuIdSticks[bbcIdx] == JoystickMenuIds[bbcIdx].AnalogMousestick;
	m_JoystickConfig[bbcIdx].DigitalMousestick =
		m_MenuIdSticks[bbcIdx] == JoystickMenuIds[bbcIdx].DigitalMousestick;
}

/****************************************************************************/
int BeebWin::GetPCJoystick(int bbcIdx)
{
	return m_JoystickConfig[bbcIdx].PCStick;
}

/****************************************************************************/
bool BeebWin::GetAnalogMousestick(int bbcIdx)
{
	return m_JoystickConfig[bbcIdx].AnalogMousestick;
}

/****************************************************************************/
bool BeebWin::GetDigitalMousestick(int bbcIdx)
{
	return m_JoystickConfig[bbcIdx].DigitalMousestick;
}

/****************************************************************************/
// Check if PC joystick is assigned to any BBC joystick
bool BeebWin::IsPCJoystickOn(int pcIdx)
{
	if (pcIdx >= _countof(JoystickMenuIdsType::Joysticks))
		return false;

	for (int bbcIdx = 0; bbcIdx < NUM_BBC_JOYSTICKS; ++bbcIdx)
	{
		if (GetPCJoystick(bbcIdx) == pcIdx + 1)
			return true;
	}

	return false;
}

/*****************************************************************************/
void BeebWin::InitJoystickMenu()
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
	}

	CheckMenuItem(IDM_JOYSTICK_TO_KEYS, m_JoystickToKeys);
	CheckMenuItem(IDM_AUTOLOADJOYMAP, m_AutoloadJoystickMap);
}

/****************************************************************************/
void BeebWin::UpdateJoystickMenu()
{
	// Check "Rescan Joysticks" menu item if any joystick is found
	// CheckMenuItem(IDM_INIT_JOYSTICK, m_pJoystickHandler->GetJoystickCount() > 0);

	// Enable axes menu items if any PC joystick is assigned to the related BBC joystick
	for (int bbcIdx = 0; bbcIdx < NUM_BBC_JOYSTICKS; ++bbcIdx)
	{
		bool EnableAxes = false;
		static const char* const menuItems[2] = { "First PC &Joystick", "Second PC Joystick" };

		for (int pcIdx = 0; pcIdx < _countof(JoystickMenuIdsType::Joysticks); ++pcIdx)
		{
			JoystickDevice* pJoystickDevice = m_pJoystickHandler->GetDev(pcIdx);

			if (pJoystickDevice != nullptr)
			{
				SetMenuItemText(JoystickMenuIds[bbcIdx].Joysticks[pcIdx], pJoystickDevice->DisplayString());
			}
			else
			{
				SetMenuItemText(JoystickMenuIds[bbcIdx].Joysticks[pcIdx], menuItems[pcIdx]);
			}
		}

		if (GetPCJoystick(bbcIdx) != 0)
			EnableAxes = true;

		for (int axesIdx = 0; axesIdx < _countof(JoystickMenuIdsType::Axes); ++axesIdx)
		{
			EnableMenuItem(JoystickMenuIds[bbcIdx].Axes[axesIdx], EnableAxes);
		}
	}

	CheckMenuItem(IDM_JOY_SENSITIVITY_50,  m_JoystickSensitivity == 0.5);
	CheckMenuItem(IDM_JOY_SENSITIVITY_100, m_JoystickSensitivity == 1.0);
	CheckMenuItem(IDM_JOY_SENSITIVITY_200, m_JoystickSensitivity == 2.0);
	CheckMenuItem(IDM_JOY_SENSITIVITY_300, m_JoystickSensitivity == 3.0);

	CheckMenuItem(IDM_JOY_KEY_THRESHOLD_12_5, m_JoystickToKeysThreshold == 4096);
	CheckMenuItem(IDM_JOY_KEY_THRESHOLD_25,   m_JoystickToKeysThreshold == 8192);
	CheckMenuItem(IDM_JOY_KEY_THRESHOLD_50,   m_JoystickToKeysThreshold == 16384);
	CheckMenuItem(IDM_JOY_KEY_THRESHOLD_75,   m_JoystickToKeysThreshold == 24756);
}

void BeebWin::SetJoystickSensitivity(double Sensitivity)
{
	m_JoystickSensitivity = Sensitivity;
	UpdateJoystickMenu();
}

void BeebWin::SetJoystickToKeysThreshold(int Threshold)
{
	m_JoystickToKeysThreshold = Threshold;
	UpdateJoystickMenu();
}

/****************************************************************************/
void BeebWin::ProcessJoystickMenuCommand(int bbcIdx, UINT MenuId)
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
void BeebWin::ProcessJoystickAxesMenuCommand(int bbcIdx, UINT MenuId)
{
	CheckMenuItem(m_MenuIdAxes[bbcIdx], false);
	m_MenuIdAxes[bbcIdx] = MenuId;
	UpdateJoystickConfig(bbcIdx);
	CheckMenuItem(m_MenuIdAxes[bbcIdx], true);
}

/****************************************************************************/
void BeebWin::ResetJoystick()
{
	for (JoystickDevice& dev : m_pJoystickHandler->m_JoystickDevs)
	{
		if (dev.IsCaptured())
		{
			TranslateJoystick(dev.m_JoyIndex);
		}

		dev.Close();
	}
}

/****************************************************************************/
bool BeebWin::ScanJoysticks(bool Verbose)
{
	ResetJoystick();

	HRESULT hResult = m_pJoystickHandler->Init();

	if (FAILED(hResult) && Verbose)
	{
		Report(MessageType::Error, "DirectInput initialization failed.\nError Code: %08X",
		       hResult);

		return false;
	}

	if (FAILED(hResult = m_pJoystickHandler->ScanJoysticks()))
	{
		if (Verbose)
		{
			Report(MessageType::Error, "Joystick enumeration failed.\nError Code: %08X",
			       hResult);
		}

		return false;
	}

	if (m_pJoystickHandler->GetJoystickCount() == 0)
	{
		if (Verbose)
		{
			Report(MessageType::Error, "No joysticks found");
		}

		return false;
	}

	return true;
}

/****************************************************************************/
void BeebWin::InitJoystick(bool Verbose)
{
	ResetJoystick();

	if (m_JoystickToKeys)
	{
		for (int pcIdx = 0; pcIdx < NUM_PC_JOYSTICKS; ++pcIdx)
		{
			CaptureJoystick(pcIdx, Verbose);
		}
	}

	int pcJoy1 = GetPCJoystick(0);
	if (pcJoy1 != 0)
	{
		if (!m_JoystickToKeys || pcJoy1 > NUM_PC_JOYSTICKS)
			CaptureJoystick(pcJoy1 - 1, Verbose);
	}

	int pcJoy2 = GetPCJoystick(1);
	if (pcJoy2 != 0 && pcJoy2 != pcJoy1)
	{
		if (!m_JoystickToKeys || pcJoy2 > NUM_PC_JOYSTICKS)
			CaptureJoystick(pcJoy2 - 1, Verbose);
	}

	if (GetPCJoystick(0) || GetPCJoystick(1) || m_JoystickToKeys)
	{
		if (!m_JoystickTimerRunning)
		{
			SetTimer(m_hWnd, 3, 20, NULL);
			m_JoystickTimerRunning = true;
		}
	}
	else
	{
		if (m_JoystickTimerRunning)
		{
			KillTimer(m_hWnd, 3);
			m_JoystickTimerRunning = false;
		}
	}

	UpdateJoystickMenu();
}

/****************************************************************************/
bool BeebWin::CaptureJoystick(int Index, bool Verbose)
{
	bool Result = false;

	JoystickDevice* pJoystick = m_pJoystickHandler->GetDev(Index);

	if (pJoystick != nullptr)
	{
		HRESULT hr = m_pJoystickHandler->OpenDevice(m_hWnd, pJoystick);

		if (SUCCEEDED(hr))
		{
			Result = true;
		}
		else if (Verbose)
		{
			Report(MessageType::Error, "Failed to initialise %s",
			       pJoystick->DisplayString().c_str());
		}
	}

	return Result;
}

/****************************************************************************/
void BeebWin::SetJoystickButton(int index, bool value)
{
	JoystickButton[index] = value;
}

/****************************************************************************/
void BeebWin::ScaleJoystick(int index, unsigned int x, unsigned int y,
	unsigned int minX, unsigned int minY, unsigned int maxX, unsigned int maxY)
{
	/* Gain and reverse the readings */
	double sx = 0.5 + ((double)(maxX - x) / (double)(maxX - minX) - 0.5) * m_JoystickSensitivity;
	double sy = 0.5 + ((double)(maxY - y) / (double)(maxY - minY) - 0.5) * m_JoystickSensitivity;

	/* Scale to 0-65535 range */
	sx = std::max(0.0, std::min(65535.0, sx * 65535.0));
	sy = std::max(0.0, std::min(65535.0, sy * 65535.0));

	JoystickX[index] = (int)sx;
	JoystickY[index] = (int)sy;
}

/****************************************************************************/
void BeebWin::SetMousestickButton(int index, bool value)
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
void BeebWin::ScaleMousestick(unsigned int x, unsigned int y)
{
	for (int index = 0; index < 2; ++index)
	{
		if (m_JoystickConfig[index].AnalogMousestick)
		{
			ScaleJoystick(index, x, y, 0, 0, m_XWinSize, m_YWinSize);
		}
		else if (m_JoystickConfig[index].DigitalMousestick)
		{
			const int Threshold = 4000;

			/* Keep 32768.0 double to convert from unsigned int */
			double XPos = (((m_XWinSize - x) * 65535 / m_XWinSize) - 32768.0) * m_JoystickSensitivity;
			double YPos = (((m_YWinSize - y) * 65535 / m_YWinSize) - 32768.0) * m_JoystickSensitivity;

			if (XPos < -Threshold)
			{
				JoystickX[index] = 0;
			}
			else if (XPos > Threshold)
			{
				JoystickX[index] = 65535;
			}
			else
			{
				JoystickX[index] = 32768;
			}

			if (YPos < -Threshold)
			{
				JoystickY[index] = 0;
			}
			else if (YPos > Threshold)
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
// Translate joystick position changes to key up or down message
void BeebWin::TranslateAxes(int joyId, unsigned int axesState)
{
	JoystickDevice* dev = m_pJoystickHandler->GetDev(joyId);
	unsigned int& prevAxes = dev->m_PrevAxes;
	int vkeys = BEEB_VKEY_JOY_START + joyId * (JOYSTICK_MAX_AXES + JOYSTICK_MAX_BUTTONS);

	if (axesState != prevAxes)
	{
		for (int axId = 0; axId < JOYSTICK_MAX_AXES; ++axId)
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
void BeebWin::TranslateJoystickButtons(int joyId, unsigned int buttons)
{
	JoystickDevice* dev = m_pJoystickHandler->GetDev(joyId);
	unsigned int& prevBtns = dev->m_PrevBtns;
	int vkeys = BEEB_VKEY_JOY_START + JOYSTICK_MAX_AXES
	          + joyId * (JOYSTICK_MAX_AXES + JOYSTICK_MAX_BUTTONS);

	if (buttons != prevBtns)
	{
		for (int btnId = 0; btnId < JOYSTICK_MAX_BUTTONS; ++btnId)
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
void BeebWin::TranslateOrSendKey(int vkey, bool keyUp)
{
	if (m_JoystickTarget == nullptr)
	{
		int row, col;
		TranslateKey(vkey, keyUp, row, col);
	}
	else if (!keyUp)
	{
		// Joystick mapping dialog is visible - translate button down to key down
		// message and send to dialog
		PostMessage(m_JoystickTarget, WM_KEYDOWN, vkey, 0);
	}
}

/****************************************************************************/
void BeebWin::TranslateJoystick(int joyId)
{
	JoystickDevice* pJoystickDevice = m_pJoystickHandler->GetDev(joyId);
	bool success = false;
	DWORD buttons = 0;

	if (pJoystickDevice == nullptr)
		return;

	if (pJoystickDevice->IsCaptured() && pJoystickDevice->Update())
	{
		success = true;
		buttons = pJoystickDevice->GetButtons();
	}

	// If joystick is disabled or an error occurs, reset all axes and buttons
	if (!success)
	{
		if (pJoystickDevice->IsCaptured())
		{
			// Reset 'captured' flag and update menu entry
			pJoystickDevice->m_Captured = false;
			UpdateJoystickMenu();
		}
	}

	// PC joystick to BBC joystick
	for (int bbcIndex = 0; bbcIndex < NUM_BBC_JOYSTICKS; ++bbcIndex)
	{
		if (GetPCJoystick(bbcIndex) == joyId + 1)
		{
			if (bbcIndex == 1 &&
			    m_JoystickConfig[0].PCStick == m_JoystickConfig[1].PCStick &&
			    m_JoystickConfig[0].PCAxes != m_JoystickConfig[1].PCAxes)
			{
				// If both BBC joysticks are mapped to the same PC gamepad, but not
				// the same axes, map buttons 2 and 4 to the Beeb's PB1 input
				SetJoystickButton(bbcIndex, (buttons & (JOY_BUTTON2 | JOY_BUTTON4)) != 0);
			}
			else
			{
				SetJoystickButton(bbcIndex, (buttons & (JOY_BUTTON1 | JOY_BUTTON3)) != 0);
			}

			if (bbcIndex == 0 && !m_JoystickConfig[1].Enabled)
			{
				// Second BBC joystick not enabled - map buttons 2 and 4 to Beeb's PB1 input
				SetJoystickButton(1, (buttons & (JOY_BUTTON2 | JOY_BUTTON4)) != 0);
			}

			int axesConfig = m_JoystickConfig[bbcIndex].PCAxes;
			int x = 32767, y = 32767;
			if (success)
				pJoystickDevice->GetAxesValue(axesConfig, x, y);
			ScaleJoystick(bbcIndex, x, y, 0, 0, 65535, 65535);
		}
	}

	// Joystick to keyboard mapping
	if (joyId < NUM_PC_JOYSTICKS && m_JoystickToKeys && success)
	{
		DWORD axes = pJoystickDevice->GetAxesState(m_JoystickToKeysThreshold);
		TranslateAxes(joyId, axes);
		TranslateJoystickButtons(joyId, buttons);
		pJoystickDevice->m_JoystickToKeysActive = true;
	}

	// Make sure to reset keyboard state
	if (!m_JoystickToKeys && pJoystickDevice->m_JoystickToKeysActive)
	{
		TranslateAxes(joyId, 0);
		TranslateJoystickButtons(joyId, 0);
		pJoystickDevice->m_JoystickToKeysActive = false;
	}
}

/*****************************************************************************/
void BeebWin::UpdateJoysticks()
{
	int translated = 0;

	// Don't update joysticks if not in foreground
	if (!m_active && m_JoystickTarget == nullptr)
		return;

	// Translate first four joysticks if joystick to keyboard is enabled
	if (m_JoystickToKeys)
	{
		for (int idx = 0; idx < NUM_PC_JOYSTICKS; ++idx)
		{
			JoystickDevice* pJoystickDevice = m_pJoystickHandler->GetDev(idx);

			if (pJoystickDevice != nullptr && pJoystickDevice->IsCaptured())
			{
				TranslateJoystick(idx);
				translated |= 1 << idx;
			}
		}
	}

	// Translate joystick for BBC joystick 1
	if (m_JoystickConfig[0].Enabled && m_JoystickConfig[0].PCStick != 0)
	{
		int pcStick = m_JoystickConfig[0].PCStick - 1;
		if (!(translated & (1 << pcStick)))
		{
			TranslateJoystick(pcStick);
			translated |= 1 << pcStick;
		}
	}

	// Translate joystick for BBC joystick 2
	if (m_JoystickConfig[1].Enabled && m_JoystickConfig[1].PCStick != 0)
	{
		int pcStick = m_JoystickConfig[1].PCStick - 1;
		if (!(translated & (1 << pcStick)))
		{
			TranslateJoystick(pcStick);
			translated |= 1 << pcStick;
		}
	}
}

/*****************************************************************************/
// Look for file specific joystick map
void BeebWin::CheckForJoystickMap(const char *path)
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
		ReadJoystickMap(file, &JoystickMap);
	}
	else
	{
		// Mapping file not found - reset to default
		ResetJoystickMapToDefaultUser();

		// Set jmap path even if the file doesn't exist
		strcpy(m_JoystickMapPath, file);
	}
}

/****************************************************************************/
void BeebWin::ResetJoystickMap()
{
	if (Report(MessageType::Question,
	           "Clear joystick to keyboard mapping table?") == MessageResult::Yes)
	{
		ResetJoystickMap(&JoystickMap);
	}
}

/****************************************************************************/
void BeebWin::ResetJoystickMap(JoyMap* joymap)
{
	// Initialize all input to unassigned
	for (KeyPair& mapping : *joymap)
	{
		mapping[0].row = mapping[1].row = UNASSIGNED_ROW;
		mapping[0].col = mapping[1].col = 0;
		mapping[0].shift = false;
		mapping[1].shift = true;
	}
}

/****************************************************************************/
void BeebWin::ResetJoystickMapToDefaultUser()
{
	char FileName[_MAX_PATH];
	strcpy(FileName, "DefaultUser.jmap");
	GetDataPath(m_UserDataPath, FileName);
	ResetJoystickMap(&JoystickMap);

	if (GetFileAttributes(FileName) != INVALID_FILE_ATTRIBUTES)
		ReadJoystickMap(FileName, &JoystickMap);
}

/****************************************************************************/
static bool GetNthBoolValue(Preferences& preferences, const char* format, int idx, bool& value)
{
	char option_str[256];
	sprintf_s(option_str, format, idx);
	return preferences.GetBoolValue(option_str, value);
}

/****************************************************************************/
static bool GetNthDWORDValue(Preferences& preferences, const char* format, int idx, DWORD& value)
{
	char option_str[256];
	sprintf_s(option_str, format, idx);
	return preferences.GetDWORDValue(option_str, value);
}

/****************************************************************************/
static bool GetNthStringValue(Preferences& preferences, const char* format, int idx, std::string& value)
{
	char option_str[256];
	sprintf_s(option_str, format, idx);
	return preferences.GetStringValue(option_str, value);
}

/****************************************************************************/
static void SetNthBoolValue(Preferences& preferences, const char* format, int idx, bool value)
{
	char option_str[256];
	sprintf_s(option_str, format, idx);
	preferences.SetBoolValue(option_str, value);
}

/****************************************************************************/
static void SetNthDWORDValue(Preferences& preferences, const char* format, int idx, DWORD value)
{
	char option_str[256];
	sprintf_s(option_str, format, idx);
	preferences.SetDWORDValue(option_str, value);
}

/****************************************************************************/
static void SetNthStringValue(Preferences& preferences, const char* format, int idx, const std::string& value)
{
	char option_str[256];
	sprintf_s(option_str, format, idx);
	preferences.SetStringValue(option_str, value);
}

/****************************************************************************/
static void EraseNthValue(Preferences& preferences, const char* format, int idx)
{
	char option_str[256];
	sprintf_s(option_str, format, idx);
	preferences.EraseValue(option_str);
}

/****************************************************************************/

void BeebWin::LoadJoystickPreferences()
{
	bool flag;
	DWORD dword;

	/* Clear joystick configuration */
	for (int bbcIdx = 0; bbcIdx < NUM_BBC_JOYSTICKS; ++bbcIdx)
	{
		BBCJoystickConfig& config = m_JoystickConfig[bbcIdx];

		m_MenuIdSticks[bbcIdx] = 0;
		m_MenuIdAxes[bbcIdx] = 0;

		config.Enabled = false;
		config.PCStick = 0;
		config.PCAxes = 0;
		config.AnalogMousestick = false;
		config.DigitalMousestick = false;
	}

	// Backward compatibility - "Sticks" contains MenuId

	if (m_Preferences.GetDWORDValue(CFG_OPTIONS_STICKS, dword))
	{
		BBCJoystickConfig& config = m_JoystickConfig[0];
		m_MenuIdSticks[0] = dword;

		config.Enabled = true;
		config.PCStick = MenuIdToStick(0, dword); // TODO: map menu item ids
		config.AnalogMousestick = (dword == JoystickMenuIds[0].AnalogMousestick);
		config.DigitalMousestick = (dword == JoystickMenuIds[0].DigitalMousestick);
	}

	/* New joystick options */
	for (int bbcIdx = 0; bbcIdx < NUM_BBC_JOYSTICKS; ++bbcIdx)
	{
		BBCJoystickConfig& config = m_JoystickConfig[bbcIdx];

		if (GetNthBoolValue(m_Preferences, CFG_OPTIONS_STICK_ANALOG, bbcIdx + 1, flag) && flag)
		{
			config.AnalogMousestick = true;
			config.Enabled = true;
			m_MenuIdSticks[bbcIdx] = JoystickMenuIds[bbcIdx].AnalogMousestick;
		}
		else if (GetNthBoolValue(m_Preferences, CFG_OPTIONS_STICK_DIGITAL, bbcIdx + 1, flag) && flag)
		{
			config.DigitalMousestick = true;
			config.Enabled = true;
			m_MenuIdSticks[bbcIdx] = JoystickMenuIds[bbcIdx].DigitalMousestick;
		}
		else if (GetNthDWORDValue(m_Preferences, CFG_OPTIONS_STICK_PCSTICK, bbcIdx + 1, dword) && dword != 0)
		{
			config.PCStick = dword;
			config.Enabled = true;
			m_MenuIdSticks[bbcIdx] = StickToMenuId(bbcIdx, dword);
		}

		if (GetNthDWORDValue(m_Preferences, CFG_OPTIONS_STICK_PCAXES, bbcIdx + 1, dword))
		{
			// Axes value 2 is obsolete, replace with 1
			if (dword == 2)
				dword = 1;
			config.PCAxes = dword;
		}

		m_MenuIdAxes[bbcIdx] = AxesToMenuId(bbcIdx, config.PCAxes);
	}

	if (!m_Preferences.GetBoolValue(CFG_OPTIONS_STICKS_TO_KEYS, m_JoystickToKeys))
		m_JoystickToKeys = false;

	if (!m_Preferences.GetBoolValue(CFG_OPTIONS_AUTOLOAD_JOYSICK_MAP, m_AutoloadJoystickMap))
		m_AutoloadJoystickMap = false;

	if (m_Preferences.GetDWORDValue(CFG_OPTIONS_STICKS_THRESHOLD, dword))
		m_JoystickToKeysThreshold = dword;
	else
		m_JoystickToKeysThreshold = DEFAULT_JOYSTICK_THRESHOLD;

	if (m_Preferences.GetDWORDValue(CFG_OPTIONS_JOYSTICK_SENSITIVITY, dword))
		m_JoystickSensitivity = static_cast<double>(dword) / 0x10000;
	else
		m_JoystickSensitivity = 1.0;

	m_pJoystickHandler->m_JoystickOrder.clear();
	for (int idx = 0; idx < MAX_JOYSTICK_ORDER; ++idx)
	{
		std::string value;
		JoystickOrderEntry entry;
		if (!GetNthStringValue(m_Preferences, CFG_OPTIONS_JOYSTICK_ORDER, idx + 1, value)
		    || !entry.from_string(value))
			break;
		m_pJoystickHandler->m_JoystickOrder.push_back(entry);
	}
}

/****************************************************************************/
void BeebWin::SaveJoystickPreferences()
{
	for (int bbcIdx = 0; bbcIdx < NUM_BBC_JOYSTICKS; ++bbcIdx)
	{
		BBCJoystickConfig& config = m_JoystickConfig[bbcIdx];
		SetNthBoolValue(m_Preferences, CFG_OPTIONS_STICK_ANALOG, bbcIdx + 1, config.AnalogMousestick);
		SetNthBoolValue(m_Preferences, CFG_OPTIONS_STICK_DIGITAL, bbcIdx + 1, config.DigitalMousestick);
		SetNthDWORDValue(m_Preferences, CFG_OPTIONS_STICK_PCSTICK, bbcIdx + 1, config.PCStick);
		SetNthDWORDValue(m_Preferences, CFG_OPTIONS_STICK_PCAXES, bbcIdx + 1, config.PCAxes);
	}

	m_Preferences.SetBoolValue(CFG_OPTIONS_STICKS_TO_KEYS, m_JoystickToKeys);
	m_Preferences.SetBoolValue(CFG_OPTIONS_AUTOLOAD_JOYSICK_MAP, m_AutoloadJoystickMap);
	m_Preferences.SetDWORDValue(CFG_OPTIONS_STICKS_THRESHOLD, m_JoystickToKeysThreshold);
	m_Preferences.SetDWORDValue(CFG_OPTIONS_JOYSTICK_SENSITIVITY, static_cast<DWORD>(m_JoystickSensitivity * 0x10000));

	/* Remove obsolete values */
	m_Preferences.EraseValue(CFG_OPTIONS_STICKS);
}

/****************************************************************************/
void BeebWin::SaveJoystickOrder()
{
	std::vector<JoystickOrderEntry>& order = m_pJoystickHandler->m_JoystickOrder;

	for (UINT idx = 0; idx < MAX_JOYSTICK_ORDER; ++idx)
	{
		if (idx < order.size())
		{
			SetNthStringValue(m_Preferences, CFG_OPTIONS_JOYSTICK_ORDER, idx + 1,
			order[idx].to_string());
		}
		else
		{
			EraseNthValue(m_Preferences, CFG_OPTIONS_JOYSTICK_ORDER, idx + 1);
		}
	}
}
