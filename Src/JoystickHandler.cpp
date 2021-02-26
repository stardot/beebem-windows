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
#include "JoystickHandler.h"

#include <cctype>
#include <list>
#include <vector>
#include <algorithm>

// Unhide std::min, std::max
#undef min
#undef max

#define JOYMAP_TOKEN "*** BeebEm Joystick Map ***"

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
	UINT Axes[3];
	UINT AnalogMousestick;
	UINT DigitalMousestick;
};

/*****************************************************************************/
constexpr JoystickMenuIdsType JoystickMenuIds[NUM_BBC_JOYSTICKS]{
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

/*****************************************************************************/
std::string JoystickOrderEntry::to_string()
{
	char buf[10];
	sprintf_s(buf, "%04x:%04x", mId(), pId());
	std::string result(buf);
#if 0 // Joystick names from joyGetDevCaps are useless
	if (Name.length() != 0)
		result += " # " + Name;
#endif
	return result;
}

/*****************************************************************************/
bool JoystickOrderEntry::from_string(const std::string& str)
{
	Name = "";

	auto split = str.find('#');
	if (split != std::string::npos)
	{
		++split;
		while (split < str.length() && std::isspace(str[split]))
			++split;
		Name = str.substr(split);
	}

	return sscanf(str.c_str(), "%x:%x", &mId(), &pId()) == 2;
}

/****************************************************************************/
static int MenuIdToStick(int bbcIdx, UINT menuId)
{
    for (int pcIdx = 0; pcIdx < _countof(JoystickMenuIdsType::Joysticks); ++pcIdx)
    {
	if (menuId == JoystickMenuIds[bbcIdx].Joysticks[pcIdx])
	    return pcIdx + 1;
    }
    return 0;
}

/****************************************************************************/
static UINT StickToMenuId(int bbcIdx, int pcStick)
{
    if (pcStick > 0 && pcStick - 1 < _countof(JoystickMenuIdsType::Joysticks))
	return JoystickMenuIds[bbcIdx].Joysticks[pcStick - 1];
    return 0;
}

/****************************************************************************/
static int MenuIdToAxes(int bbcIdx, UINT menuId)
{
    for (int axesIdx = 0; axesIdx < _countof(JoystickMenuIdsType::Axes); ++axesIdx)
    {
	if (menuId == JoystickMenuIds[bbcIdx].Axes[axesIdx])
	    return axesIdx;
    }
    return 0;
}

/****************************************************************************/
static UINT AxesToMenuId(int bbcIdx, int pcAxes)
{
    if (pcAxes >= 0 && pcAxes < _countof(JoystickMenuIdsType::Axes))
	return JoystickMenuIds[bbcIdx].Axes[pcAxes];
    return 0;
}

/*****************************************************************************/
std::string JoystickDev::DisplayString()
{
	if (!Configured)
		return std::string{};

	std::string name = Caps.szPname;
	if (Instance != 1)
		name += " #" + std::to_string(Instance);
	return name;
}

/*****************************************************************************/
bool JoystickDev::Update()
{
	if (!Present)
		return false;

	InfoEx.dwSize = sizeof(InfoEx);
	InfoEx.dwFlags = JOY_RETURNALL | JOY_RETURNPOVCTS;
	return joyGetPosEx(JoyIndex, &InfoEx) == JOYERR_NOERROR;
}

/*****************************************************************************/
DWORD JoystickDev::GetButtons()
{
	return InfoEx.dwButtons;
}

/*****************************************************************************/
void JoystickDev::GetAxesValue(int axesSet, int& x, int& y)
{
	UINT minX, maxX;
	UINT minY, maxY;
	DWORD dwX, dwY;
	switch (axesSet)
	{
	case 1:
		dwX = InfoEx.dwUpos; minX = Caps.wUmin; maxX = Caps.wUmax;
		dwY = InfoEx.dwRpos; minY = Caps.wRmin; maxY = Caps.wRmax;
		break;
	case 2:
		dwX = InfoEx.dwZpos; minX = Caps.wZmin; maxX = Caps.wZmax;
		dwY = InfoEx.dwRpos; minY = Caps.wRmin; maxY = Caps.wRmax;
		break;
	default:
		dwX = InfoEx.dwXpos; minX = Caps.wXmin; maxX = Caps.wXmax;
		dwY = InfoEx.dwYpos; minY = Caps.wYmin; maxY = Caps.wYmax;
		break;
	}
	x = ((dwX - minX) * 65535 / (maxX - minX));
	y = ((dwY - minY) * 65535 / (maxY - minY));
}

DWORD JoystickDev::GetAxesState(int threshold)
{
	using InfoT = JOYINFOEX;
	using CapsT = JOYCAPS;
	unsigned int axes = 0;

	auto Scale = [this](DWORD InfoT::* pos, UINT CapsT::* min, UINT CapsT::* max) {
		return (int)((double)(InfoEx.*pos - Caps.*min) * 65535.0 /
			(double)(Caps.*max - Caps.*min));
	};

	auto Detect = [&axes, threshold](const int val, const int nbit, const int pbit) {
		if (val < 32767 - threshold)
			axes |= (1 << nbit);
		else if (val > 32767 + threshold)
			axes |= (1 << pbit);
	};

	int ty = Scale(&InfoT::dwYpos, &CapsT::wYmin, &CapsT::wYmax);
	int tx = Scale(&InfoT::dwXpos, &CapsT::wXmin, &CapsT::wXmax);
	int tz = Scale(&InfoT::dwZpos, &CapsT::wZmin, &CapsT::wZmin);
	int tr = Scale(&InfoT::dwRpos, &CapsT::wRmin, &CapsT::wRmax);
	int tu = Scale(&InfoT::dwUpos, &CapsT::wUmin, &CapsT::wUmax);
	int tv = Scale(&InfoT::dwVpos, &CapsT::wVmin, &CapsT::wVmax);

	Detect(ty, JOYSTICK_AXIS_UP, JOYSTICK_AXIS_DOWN);
	Detect(tx, JOYSTICK_AXIS_LEFT, JOYSTICK_AXIS_RIGHT);
	Detect(tz, JOYSTICK_AXIS_Z_N, JOYSTICK_AXIS_Z_P);
	Detect(tr, JOYSTICK_AXIS_R_N, JOYSTICK_AXIS_R_P);
	Detect(tu, JOYSTICK_AXIS_U_N, JOYSTICK_AXIS_U_P);
	Detect(tv, JOYSTICK_AXIS_V_N, JOYSTICK_AXIS_V_P);

	if (InfoEx.dwPOV != JOY_POVCENTERED)
	{
		if (InfoEx.dwPOV >= 29250 || InfoEx.dwPOV < 6750)
			axes |= (1 << JOYSTICK_AXIS_HAT_UP);
		if (InfoEx.dwPOV >= 2250 && InfoEx.dwPOV < 15750)
			axes |= (1 << JOYSTICK_AXIS_HAT_RIGHT);
		if (InfoEx.dwPOV >= 11250 && InfoEx.dwPOV < 24750)
			axes |= (1 << JOYSTICK_AXIS_HAT_DOWN);
		if (InfoEx.dwPOV >= 20250 && InfoEx.dwPOV < 33750)
			axes |= (1 << JOYSTICK_AXIS_HAT_LEFT);
	}

	return axes;
}

/****************************************************************************/
void JoystickHandler::ScanJoysticks()
{
	JOYINFOEX joyInfoEx;
	std::map<JoystickId, std::list<JoystickDev*>> listById;

	// Clear PC joystick list
	for (PCJoystickState& state : m_PCJoystickState)
	{
		state.Captured = false;
		state.Dev = nullptr;
		state.JoyIndex = -1;
	}

	// Get all configured and present joysticks
	for (int devIdx = 0; devIdx < MAX_JOYSTICK_DEVS; ++devIdx)
	{
		JoystickDev& dev = m_JoystickDevs[devIdx];
		dev.Configured = false;
		dev.Present = false;
		dev.Instance = 0;
		dev.Order = -1;
		dev.JoyIndex = devIdx;

		memset(&joyInfoEx, 0, sizeof(joyInfoEx));
		joyInfoEx.dwSize = sizeof(joyInfoEx);
		joyInfoEx.dwFlags = JOY_RETURNBUTTONS;

		DWORD Result = joyGetDevCaps(devIdx, &dev.Caps, sizeof(JOYCAPS));
		if (Result == JOYERR_NOERROR)
		{
			dev.Configured = true;

			Result = joyGetPosEx(devIdx, &joyInfoEx);
			if (Result == JOYERR_NOERROR)
			{
				dev.Present = true;
				listById[dev.Id()].push_back(&dev);
				dev.Instance = listById[dev.Id()].size();
			}
		}
	}

	int joyIdx = 0;
	int order = 0;
	// Start with joysticks in the order list
	for (JoystickOrderEntry& entry : m_JoystickOrder)
	{
		std::list<JoystickDev*>& list = listById[entry];
		if (list.size() != 0)
		{
			JoystickDev& dev = *(list.front());
			list.pop_front();
			dev.Order = order++;
			entry.JoyIndex = dev.JoyIndex;
			if (dev.Present && joyIdx < NUM_PC_JOYSTICKS)
			{
				m_PCJoystickState[joyIdx].Dev = &dev;
				m_PCJoystickState[joyIdx].JoyIndex = dev.JoyIndex;
				++joyIdx;
			}
		}
		else
		{
			entry.JoyIndex = -1;
		}
	}

	std::list<JoystickDev*> newJoysticks;
	// Add joysticks not in the order list
	for (JoystickDev& dev : m_JoystickDevs)
	{
		if (joyIdx >= NUM_PC_JOYSTICKS)
			break;

		if (dev.Present && dev.Order == -1)
		{
			m_PCJoystickState[joyIdx].Dev = &dev;
			m_PCJoystickState[joyIdx].JoyIndex = dev.JoyIndex;
			++joyIdx;
			newJoysticks.push_back(&dev);
		}
	}

	// If joystick order list is too long, remove some unconnected entries
	// Remove entries from the beginning or from the end, or some other order?
	int to_remove = newJoysticks.size() + m_JoystickOrder.size() - MAX_JOYSTICK_ORDER;
	unsigned int idx = 0;
	while (to_remove > 0 && idx < m_JoystickOrder.size())
	{
		if (m_JoystickOrder[idx].JoyIndex == -1)
		{
			m_JoystickOrder.erase(m_JoystickOrder.begin() + idx);
			--to_remove;
		}
		else
			++idx;
	}

	// Add new joystick at the end of order list
	for (JoystickDev* dev : newJoysticks)
		m_JoystickOrder.emplace_back(dev->Id(), dev->DisplayString(), dev->JoyIndex);

	// Update order in joystick list
	for (idx = 0; idx < static_cast<int>(m_JoystickOrder.size()); ++idx)
	{
		JoystickOrderEntry& entry = m_JoystickOrder[idx];
		if (entry.JoyIndex != -1)
			m_JoystickDevs[entry.JoyIndex].Order = idx;
	}
}

/****************************************************************************/
JoystickHandlerPtr::JoystickHandlerPtr() : std::unique_ptr<JoystickHandler>{ std::make_unique<JoystickHandler>() } {}

/****************************************************************************/
JoystickHandlerPtr::~JoystickHandlerPtr() {}

/****************************************************************************/
void BeebWin::SetJoystickTarget(HWND target)
{
	m_JoystickTarget = target;
}

/****************************************************************************/
// Update joystick configuration from checked menu items
void BeebWin::UpdateJoystickConfig(int bbcIdx)
{
	m_JoystickConfig[bbcIdx].Enabled = (m_MenuIdSticks[bbcIdx] != 0);
	m_JoystickConfig[bbcIdx].PCStick = MenuIdToStick(bbcIdx, m_MenuIdSticks[bbcIdx]);
	m_JoystickConfig[bbcIdx].PCAxes = MenuIdToAxes(bbcIdx, m_MenuIdAxes[bbcIdx]);
	m_JoystickConfig[bbcIdx].AnalogMousestick =
		(static_cast<UINT>(m_MenuIdAxes[bbcIdx]) == JoystickMenuIds[bbcIdx].AnalogMousestick);
	m_JoystickConfig[bbcIdx].DigitalMousestick =
		(static_cast<UINT>(m_MenuIdAxes[bbcIdx]) == JoystickMenuIds[bbcIdx].DigitalMousestick);
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
	bool EnableInit = false;

	// Enable "Initialize Joysticks" menu item if any more joysticks could be possibly captured (or always?)
	for (int pcIdx = 0; pcIdx < NUM_PC_JOYSTICKS; ++pcIdx)
	{
		if ((m_JoystickToKeys || IsPCJoystickOn(pcIdx)) && !m_JoystickHandler->m_PCJoystickState[pcIdx].Captured)
			EnableInit = true;
	}
	EnableMenuItem(IDM_INIT_JOYSTICK, EnableInit);

	// Check "Initialize Joysticks" menu item if any joystick is already captured
	CheckMenuItem(IDM_INIT_JOYSTICK, m_JoystickHandler->m_PCJoystickState[0].Captured);

	// Enable axes menu items if any PC joystick is assigned to the related BBC joystick
	for (int bbcIdx = 0; bbcIdx < NUM_BBC_JOYSTICKS; ++bbcIdx)
	{
		bool EnableAxes = false;
#if 0 // This needs rework (proper joystick name from DirectInput)
		static const char* const menuItems[2] = { "First PC &Joystick", "Second PC Joystick" };

		for (int pcIdx = 0; pcIdx < _countof(JoystickMenuIdsType::Joysticks); ++pcIdx)
		{
			auto* dev = m_PCJoystickState[pcIdx].Dev;
			if (dev)
			{
				SetMenuItemText(JoystickMenuIds[bbcIdx].Joysticks[pcIdx], dev->DisplayString());
			}
			else
			{
				SetMenuItemText(JoystickMenuIds[bbcIdx].Joysticks[pcIdx], menuItems[pcIdx]);
			}
		}
#endif

		if (GetPCJoystick(bbcIdx) != 0)
			EnableAxes = true;

		for (int axesIdx = 0; axesIdx < _countof(JoystickMenuIdsType::Axes); ++axesIdx)
			EnableMenuItem(JoystickMenuIds[bbcIdx].Axes[axesIdx], EnableAxes);
	}
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

	if (MenuId == static_cast<UINT>(m_MenuIdSticks[bbcIdx]))
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
void BeebWin::ProcessJoystickToKeysCommand(void)
{
	m_JoystickToKeys = !m_JoystickToKeys;
	InitJoystick(false);
	CheckMenuItem(IDM_JOYSTICK_TO_KEYS, m_JoystickToKeys);
}

/****************************************************************************/
void BeebWin::ProcessAutoloadJoystickMapCommand(void)
{
	m_AutoloadJoystickMap = !m_AutoloadJoystickMap;
	CheckMenuItem(IDM_AUTOLOADJOYMAP, m_AutoloadJoystickMap);
}

/****************************************************************************/
void BeebWin::ResetJoystick()
{
	for (int pcIdx = 0; pcIdx < NUM_PC_JOYSTICKS; ++pcIdx)
	{
		auto& state = m_JoystickHandler->m_PCJoystickState[pcIdx];
		state.Captured = false;
		TranslateJoystick(pcIdx);
	}
}

/****************************************************************************/
bool BeebWin::InitJoystick(bool verbose)
{
	bool Success = true;

	ResetJoystick();

	m_JoystickHandler->ScanJoysticks();

	if (IsPCJoystickOn(0) || IsPCJoystickOn(1) || m_JoystickToKeys)
	{
		for (int pcIdx = 0; pcIdx < NUM_PC_JOYSTICKS; ++pcIdx)
		{
			if (IsPCJoystickOn(pcIdx) || m_JoystickToKeys)
				Success = CaptureJoystick(pcIdx, verbose);
		}

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

	return Success;
}

/****************************************************************************/
bool BeebWin::CaptureJoystick(int Index, bool verbose)
{
	bool success = false;

	if (m_JoystickHandler->m_PCJoystickState[Index].Dev)
	{
		m_JoystickHandler->m_PCJoystickState[Index].Captured = true;
		success = true;
	}
	else if (verbose && (IsPCJoystickOn(Index) || m_JoystickToKeys && Index == 0))
	{
		mainWin->Report(MessageType::Warning,
		                "Failed to initialise joystick %d", Index + 1);
	}

	return success;
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
		int XPos = (m_XWinSize - x) * 65535 / m_XWinSize;
		int YPos = (m_YWinSize - y) * 65535 / m_YWinSize;

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
// Translate joystick position changes to key up or down message
void BeebWin::TranslateAxes(int joyId, unsigned int axesState)
{
	auto& joyState = m_JoystickHandler->m_PCJoystickState[joyId];
	unsigned int& prevAxes = joyState.PrevAxes;
	int vkeys = BEEB_VKEY_JOY_START + joyId * (JOYSTICK_MAX_AXES + JOYSTICK_MAX_BTNS);

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
void BeebWin::TranslateJoystickButtons(int joyId, unsigned int buttons)
{
	const int BUTTON_COUNT = 32;
	auto& joyState = m_JoystickHandler->m_PCJoystickState[joyId];
	unsigned int& prevBtns = joyState.PrevBtns;
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
	auto& handler = m_JoystickHandler;
	auto& joyState = handler->m_PCJoystickState[joyId];
	auto* joyDev = joyState.Dev;
	bool success = false;
	DWORD buttons = 0;

	if (joyState.Captured && joyDev->Update())
	{
		success = true;
		buttons = joyDev->GetButtons();
	}

	// If joystick is disabled or error occurs, reset all axes and buttons
	if (!success)
	{
		if (joyState.Captured)
		{
			// Reset 'captured' flag and update menu entry
			joyState.Captured = false;
			UpdateJoystickMenu();
		}
	}

	// PC joystick to BBC joystick
	for (int bbcIndex = 0; bbcIndex < NUM_BBC_JOYSTICKS; ++bbcIndex)
	{
		if (GetPCJoystick(bbcIndex) == joyId + 1)
		{
			if (bbcIndex == 1 && m_JoystickConfig[0].PCStick == m_JoystickConfig[1].PCStick
				&& m_JoystickConfig[0].PCAxes != m_JoystickConfig[1].PCAxes)
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
				joyDev->GetAxesValue(axesConfig, x, y);
			ScaleJoystick(bbcIndex, x, y, 0, 0, 65535, 65535);
		}
	}

	// Joystick to keyboard mapping
	if (m_JoystickToKeys && success)
	{
		auto axes = joyDev->GetAxesState(m_JoystickToKeysThreshold);
		TranslateAxes(joyId, axes);
		TranslateJoystickButtons(joyId, buttons);
		handler->m_PCJoystickState[joyId].JoystickToKeysActive = true;
	}

	// Make sure to reset keyboard state
	if (!m_JoystickToKeys && handler->m_PCJoystickState[joyId].JoystickToKeysActive)
	{
		TranslateAxes(joyId, 0);
		TranslateJoystickButtons(joyId, 0);
		handler->m_PCJoystickState[joyId].JoystickToKeysActive = false;
	}
}

/*****************************************************************************/
void BeebWin::UpdateJoysticks()
{
	for (int idx = 0; idx < NUM_PC_JOYSTICKS; ++idx)
	{
		if (m_JoystickHandler->m_PCJoystickState[0].Captured)
			TranslateJoystick(idx);
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
void BeebWin::ResetJoystickMap()
{
	if (mainWin->Report(MessageType::Question,
	                    "Clear joystick to keyboard mapping table?") != MessageResult::Yes)
	{
		return;
	}

	ResetJoyMap(&JoystickMap);
}

/****************************************************************************/
void BeebWin::ResetJoyMap(JoyMap* joymap)
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
void BeebWin::ResetJoyMapToDefaultUser(void)
{
	char keymap[_MAX_PATH];
	strcpy(keymap, "DefaultUser.jmap");
	GetDataPath(m_UserDataPath, keymap);
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
bool BeebWin::ReadJoyMap(const char *filename, JoyMap *joymap)
{
	bool success = true;
	FILE *infile = OpenReadFile(filename, "joystick map", JOYMAP_TOKEN "\n");
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
			                "Invalid key name in joystick mapping file:\n  %s\n  line %d",
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
				                "Invalid key name in joystick mapping file:\n  %s\n  line %d",
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
void BeebWin::LoadJoystickMap()
{
	char DefaultPath[_MAX_PATH];
	char FileName[_MAX_PATH];
	const char* filter = "Joystick Map File (*.jmap)\0*.jmap\0";

	// If Autoload is enabled, look for joystick mapping near disks,
	// otherwise start in user data path.
	if (m_AutoloadJoystickMap)
	{
		m_Preferences.GetStringValue("DiscsPath", DefaultPath);
		GetDataPath(GetUserDataPath(), DefaultPath);
	}
	else
	{
		strcpy(DefaultPath, GetUserDataPath());
	}

	FileDialog fileDialog(m_hWnd, FileName, sizeof(FileName), DefaultPath, filter);

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
bool BeebWin::WriteJoyMap(const char *filename, JoyMap *joymap)
{
	FILE* outfile = OpenWriteFile(filename, "joystick map");

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
void BeebWin::SaveJoystickMap()
{
	char DefaultPath[_MAX_PATH];
	// Add space for .jmap exstension
	char FileName[_MAX_PATH + 5];
	const char* filter = "Joystick Map File (*.jmap)\0*.jmap\0";

	// If Autoload is enabled, store joystick mapping near disks,
	// otherwise start in user data path.
	if (m_AutoloadJoystickMap)
	{
		m_Preferences.GetStringValue("DiscsPath", DefaultPath);
		GetDataPath(GetUserDataPath(), DefaultPath);
	}
	else
	{
		strcpy(DefaultPath, GetUserDataPath());
	}

	FileDialog fileDialog(m_hWnd, FileName, sizeof(FileName), DefaultPath, filter);

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
void BeebWin::WriteJoystickOrder()
{
	auto& order = m_JoystickHandler->m_JoystickOrder;
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

/****************************************************************************/
void BeebWin::ReadJoystickPreferences()
{
	DWORD dword;
	bool flag;
	auto& handler = m_JoystickHandler;

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

	/* Backward compatibility - "Sticks" contains MenuId */
	if (m_Preferences.GetDWORDValue(CFG_OPTIONS_STICKS, dword))
	{
		BBCJoystickConfig& config = m_JoystickConfig[0];
		m_MenuIdSticks[0] = dword;

		config.Enabled = true;
		config.PCStick = MenuIdToStick(0, dword);
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
			config.PCAxes = dword;

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

	handler->m_JoystickOrder.clear();
	for (int idx = 0; idx < MAX_JOYSTICK_ORDER; ++idx)
	{
		std::string value;
		JoystickOrderEntry entry;
		if (!GetNthStringValue(m_Preferences, CFG_OPTIONS_JOYSTICK_ORDER, idx + 1, value)
		    || !entry.from_string(value))
			break;
		handler->m_JoystickOrder.push_back(entry);

	}
}

/****************************************************************************/
void BeebWin::WriteJoystickPreferences()
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
	m_Preferences.EraseValue("Sticks2");
	m_Preferences.EraseValue("JoystickAxes1");
	m_Preferences.EraseValue("JoystickAxes2");
	m_Preferences.EraseValue("Stick1ToKeysDeadBand");
	m_Preferences.EraseValue("Stick2ToKeysDeadBand");
}
