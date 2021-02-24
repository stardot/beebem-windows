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
#include "userkybd.h"
#include "sysvia.h"
#include "filedialog.h"
#include "SelectKeyDialog.h"
#include "atodconv.h"

#include <cctype>
#include <list>
#include <vector>
#include <algorithm>

// Unhide std::min, std::max
#undef min
#undef max

#define JOYMAP_TOKEN "*** BeebEm Joystick Map ***"

#define DEFAULT_JOY_DEADBAND 4096

struct JoystickId : std::pair<int, int>
{
	using std::pair<int, int>::pair;

	// Manufacturer ID aka Vendor ID
	int& mId() { return first; }
	// Product ID
	int& pId() { return second; }
};

struct JoystickOrderEntry : JoystickId
{
	std::string   Name{};
	int           JoyIndex{ -1 };

	JoystickOrderEntry() = default;
	JoystickOrderEntry(JoystickId id, const std::string& name, int joyIndex) :
		JoystickId(id), Name(name), JoyIndex(joyIndex) {}
	JoystickOrderEntry(int mid, int pid, const std::string& name) :
		JoystickId(mid, pid), Name(name) {}

	std::string to_string();
	bool from_string(const std::string&);
};

struct JoystickDev
{
	JOYCAPS      Caps{};
	int          Instance{ 0 };
	int          Order{ -1 };
	int          JoyIndex{ -1 };
	bool         Configured{ false };
	bool         Present{ false };

	JoystickId   Id() { return JoystickId{ Caps.wMid, Caps.wPid }; }
	std::string  DisplayString();
};

struct PCJoystickState
{
	JoystickDev* Dev{ nullptr };
	int           JoyIndex{ -1 };
	bool          Captured{ false };
	unsigned int  PrevAxes{ 0 };
	unsigned int  PrevBtns{ 0 };
	bool          JoystickToKeysActive{ false };
};

struct BBCJoystickConfig
{
	bool Enabled{ false };
	int  PCStick{ 0 };
	int  PCAxes{ 0 };
	bool AnalogMousestick{ false };
	bool DigitalMousestick{ false };
};

class JoystickHandlerDetails
{
	BeebWin*          m_BeebWin;

	bool              m_JoystickTimerRunning{ false };
	JoystickDev       m_JoystickDevs[MAX_JOYSTICK_DEVS];
	PCJoystickState	  m_PCJoystickState[NUM_PC_JOYSTICKS];
	BBCJoystickConfig m_JoystickConfig[NUM_BBC_JOYSTICKS];
	int               m_MenuIdSticks[NUM_BBC_JOYSTICKS]{};
	int               m_MenuIdAxes[NUM_BBC_JOYSTICKS]{};
	int               m_Deadband;
	double            m_Gain{ 1.0 };
	bool              m_JoystickToKeys{ false };
	bool              m_AutoloadJoystickMap{ false };
	HWND              m_JoystickTarget{ nullptr };
	char              m_JoystickMapPath[_MAX_PATH]{};
	std::vector<JoystickOrderEntry> m_JoystickOrder;

public:
	JoystickHandlerDetails(BeebWin* beebWin) : m_BeebWin{ beebWin } {}

	/* Accessors */
	int GetMenuIdSticks(int bbcIdx) { return m_MenuIdSticks[bbcIdx]; }
	bool GetJoystickToKeys() { return m_JoystickToKeys; }
	void SetJoystickToKeys(bool enabled) { m_JoystickToKeys = enabled; }
	void SetJoystickTarget(HWND target) { m_JoystickTarget = target; }

	/* BeebWin access */
	HWND GetHWnd();
	HMENU GetHMenu();
	int GetXWinSize();
	int GetYWinSize();
	void CheckMenuItem(UINT id, bool checked);
	void EnableMenuItem(UINT id, bool enabled);
	void SetMenuItemText(UINT id, const std::string& text);

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
	void ScanJoysticks(void);
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

	/* Timer handler */
	void UpdateJoysticks(void);

	/* Joystick to keyboard mapping */
	void CheckForJoystickMap(const char* path);
	void ResetJoystickMap(void);
	void ResetJoyMap(JoyMap* joymap);
	void ResetJoyMapToDefaultUser(void);
	bool ReadJoyMap(const char* filename, JoyMap* joymap);
	void LoadJoystickMap(void);
	void SaveJoystickMap(void);
	bool WriteJoyMap(const char* filename, JoyMap* joymap);

	/* Preferences */
	bool GetNthBoolValue(Preferences& preferences, const char* format, int idx, bool& value);
	bool GetNthDWORDValue(Preferences& preferences, const char* format, int idx, DWORD& value);
	bool GetNthStringValue(Preferences& preferences, const char* format, int idx, std::string& value);
	void SetNthBoolValue(Preferences& preferences, const char* format, int idx, bool value);
	void SetNthDWORDValue(Preferences& preferences, const char* format, int idx, DWORD value);
	void SetNthStringValue(Preferences& preferences, const char* format, int idx, const std::string& value);
	void EraseNthValue(Preferences& preferences, const char* format, int idx);
	void ReadPreferences(Preferences& preferences);
	void WritePreferences(Preferences& preferences);
	void WriteJoystickOrder(Preferences& preferences);
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
HWND JoystickHandlerDetails::GetHWnd() { return m_BeebWin->m_hWnd; }

HMENU JoystickHandlerDetails::GetHMenu() { return m_BeebWin->m_hMenu; }

int JoystickHandlerDetails::GetXWinSize() { return m_BeebWin->m_XWinSize; }

int JoystickHandlerDetails::GetYWinSize() { return m_BeebWin->m_YWinSize; }

void JoystickHandlerDetails::CheckMenuItem(UINT id, bool checked) { m_BeebWin->CheckMenuItem(id, checked); }

void JoystickHandlerDetails::EnableMenuItem(UINT id, bool enabled) { m_BeebWin->EnableMenuItem(id, enabled); }

void JoystickHandlerDetails::SetMenuItemText(UINT id, const std::string& text)
{
	MENUITEMINFO mii{ 0 };
	mii.cbSize = sizeof(mii);
	mii.fMask = MIIM_STRING;
	mii.fType = MFT_STRING;
	mii.dwTypeData = const_cast<char*>(text.c_str());
	SetMenuItemInfo(GetHMenu(), id, FALSE, &mii);
}

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
int JoystickHandlerDetails::MenuIdToStick(int bbcIdx, UINT menuId)
{
	for (int pcIdx = 0; pcIdx < _countof(JoystickMenuIdsType::Joysticks); ++pcIdx)
	{
		if (menuId == JoystickMenuIds[bbcIdx].Joysticks[pcIdx])
			return pcIdx + 1;
	}
	return 0;
}

/****************************************************************************/
UINT JoystickHandlerDetails::StickToMenuId(int bbcIdx, int pcStick)
{
	if (pcStick > 0 && pcStick - 1 < _countof(JoystickMenuIdsType::Joysticks))
		return JoystickMenuIds[bbcIdx].Joysticks[pcStick - 1];
	return 0;
}

/****************************************************************************/
int JoystickHandlerDetails::MenuIdToAxes(int bbcIdx, UINT menuId)
{
	for (int axesIdx = 0; axesIdx < _countof(JoystickMenuIdsType::Axes); ++axesIdx)
	{
		if (menuId == JoystickMenuIds[bbcIdx].Axes[axesIdx])
			return axesIdx;
	}
	return 0;
}

/****************************************************************************/
UINT JoystickHandlerDetails::AxesToMenuId(int bbcIdx, int pcAxes)
{
	if (pcAxes >= 0 && pcAxes < _countof(JoystickMenuIdsType::Axes))
		return JoystickMenuIds[bbcIdx].Axes[pcAxes];
	return 0;
}

/****************************************************************************/
// Update joystick configuration from checked menu items
void JoystickHandlerDetails::UpdateJoystickConfig(int bbcIdx)
{
	m_JoystickConfig[bbcIdx].Enabled = (m_MenuIdAxes[bbcIdx] != 0);
	m_JoystickConfig[bbcIdx].PCStick = MenuIdToStick(bbcIdx, m_MenuIdSticks[bbcIdx]);
	m_JoystickConfig[bbcIdx].PCAxes = MenuIdToAxes(bbcIdx, m_MenuIdAxes[bbcIdx]);
	m_JoystickConfig[bbcIdx].AnalogMousestick = 
		(static_cast<UINT>(m_MenuIdAxes[bbcIdx]) == JoystickMenuIds[bbcIdx].AnalogMousestick);
	m_JoystickConfig[bbcIdx].DigitalMousestick = 
		(static_cast<UINT>(m_MenuIdAxes[bbcIdx]) == JoystickMenuIds[bbcIdx].DigitalMousestick);
}

/****************************************************************************/
// Check if this PC joystick is assigned to this BBC joystick
bool JoystickHandlerDetails::IsPCJoystickAssigned(int pcIdx, int bbcIdx)
{
	return m_JoystickConfig[bbcIdx].PCStick == pcIdx + 1;
}

/****************************************************************************/
// Check if PC joystick is assigned to any BBC joystick
// TODO: Update m_PCStickForJoystick early and use it here
bool JoystickHandlerDetails::IsPCJoystickOn(int pcIdx)
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
void JoystickHandlerDetails::InitMenu()
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
void JoystickHandlerDetails::UpdateJoystickMenu()
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
void JoystickHandlerDetails::ProcessMenuCommand(int bbcIdx, UINT MenuId)
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
void JoystickHandlerDetails::ProcessAxesMenuCommand(int bbcIdx, UINT MenuId)
{
	CheckMenuItem(m_MenuIdAxes[bbcIdx], false);
	m_MenuIdAxes[bbcIdx] = MenuId;
	UpdateJoystickConfig(bbcIdx);
	CheckMenuItem(m_MenuIdAxes[bbcIdx], true);
}

/****************************************************************************/
void JoystickHandlerDetails::ToggleJoystickToKeys(void)
{
	m_JoystickToKeys = !m_JoystickToKeys;
	InitJoystick(false);
	CheckMenuItem(IDM_JOYSTICK_TO_KEYS, m_JoystickToKeys);
}

/****************************************************************************/
void JoystickHandlerDetails::ToggleAutoloadJoystickMap(void)
{
	m_AutoloadJoystickMap = !m_AutoloadJoystickMap;
	CheckMenuItem(IDM_AUTOLOADJOYMAP, m_AutoloadJoystickMap);
}

/****************************************************************************/
void JoystickHandlerDetails::ResetJoystick(void)
{
	for (int pcIdx = 0; pcIdx < NUM_PC_JOYSTICKS; ++pcIdx)
	{
		m_PCJoystickState[pcIdx].Captured = false;
		TranslateJoystick(pcIdx);
	}

}

/****************************************************************************/
void JoystickHandlerDetails::ScanJoysticks(void)
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
	unsigned int idx =  0;
	while (to_remove > 0 && idx < m_JoystickOrder.size())
	{
		if (m_JoystickOrder[idx].JoyIndex == -1)
		{
			m_JoystickOrder.erase(m_JoystickOrder.begin() + idx);
			--to_remove;
		}
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
bool JoystickHandlerDetails::InitJoystick(bool verbose)
{
	bool Success = true;

	ResetJoystick();

	ScanJoysticks();

	if (IsPCJoystickOn(0) || IsPCJoystickOn(1) || m_JoystickToKeys)
	{
		for (int pcIdx = 0; pcIdx < NUM_PC_JOYSTICKS; ++pcIdx)
		{
			if (IsPCJoystickOn(pcIdx) || m_JoystickToKeys)
				Success = CaptureJoystick(pcIdx, verbose);
		}

		if (!m_JoystickTimerRunning)
		{
			SetTimer(GetHWnd(), 3, 20, NULL);
			m_JoystickTimerRunning = true;
		}
	}
	else
	{
		if (m_JoystickTimerRunning)
		{
			KillTimer(GetHWnd(), 3);
			m_JoystickTimerRunning = false;
		}
	}

	UpdateJoystickMenu();

	return Success;
}

/****************************************************************************/
bool JoystickHandlerDetails::CaptureJoystick(int Index, bool verbose)
{
	bool success = false;

	if (m_PCJoystickState[Index].Dev)
	{
		m_PCJoystickState[Index].Captured = true;
		success = true;
	}
	else if (verbose && (IsPCJoystickOn(Index) || m_JoystickToKeys && Index == 0))
	{
		char str[100];
		sprintf(str, "Failed to initialise joystick %d", Index + 1);

		MessageBox(GetHWnd(), str, WindowTitle, MB_OK | MB_ICONWARNING);
	}

	return success;
}

/****************************************************************************/
void JoystickHandlerDetails::SetJoystickButton(int index, bool value)
{
	JoystickButton[index] = value;
}

/****************************************************************************/
void JoystickHandlerDetails::ScaleJoystick(int index, unsigned int x, unsigned int y,
	unsigned int minX, unsigned int minY, unsigned int maxX, unsigned int maxY)
{
	/* Gain and reverse the readings */
	double sx = 0.5 + ((double)(maxX - x) / (double)(maxX - minX) - 0.5) * m_Gain;
	double sy = 0.5 + ((double)(maxY - y) / (double)(maxY - minY) - 0.5) * m_Gain;

	/* Scale to 0-65535 range */
	sx = std::max(0.0, std::min(65535.0, sx * 65535.0));
	sy = std::max(0.0, std::min(65535.0, sy * 65535.0));

	JoystickX[index] = (int)sx;
	JoystickY[index] = (int)sy;
}

/****************************************************************************/
void JoystickHandlerDetails::SetMousestickButton(int index, bool value)
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
void JoystickHandlerDetails::ScaleMousestick(unsigned int x, unsigned int y)
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
unsigned int JoystickHandlerDetails::GetJoystickAxes(const JOYCAPS& caps, int deadband, const JOYINFOEX& joyInfoEx)
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
void JoystickHandlerDetails::TranslateAxes(int joyId, unsigned int axesState)
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
void JoystickHandlerDetails::TranslateJoystickButtons(int joyId, unsigned int buttons)
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
void JoystickHandlerDetails::TranslateOrSendKey(int vkey, bool keyUp)
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
void JoystickHandlerDetails::TranslateJoystick(int joyId)
{
	static const JOYCAPS dummyJoyCaps = {
		0, 0, "",
		0, 65535, 0, 65535, 0, 65535,
		16, 10, 1000,
		0, 65535, 0, 65535, 0, 65535
	};
	bool success = false;
	JOYINFOEX joyInfoEx{};

	const JOYCAPS* joyCaps = &m_PCJoystickState[joyId].Dev->Caps;
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
		if (m_PCJoystickState[joyId].Captured)
		{
			// Reset 'captured' flag and update menu entry
			m_PCJoystickState[joyId].Captured = false;
			UpdateJoystickMenu();
		}
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
			if (axesConfig == 0)
			{
				ScaleJoystick(bbcIndex, joyInfoEx.dwXpos, joyInfoEx.dwYpos,
					joyCaps->wXmin, joyCaps->wYmin,
					joyCaps->wXmax, joyCaps->wYmax);
			}
			else if (axesConfig == 1)
			{
				ScaleJoystick(bbcIndex, joyInfoEx.dwUpos, joyInfoEx.dwRpos,
					joyCaps->wUmin, joyCaps->wRmin,
					joyCaps->wUmax, joyCaps->wRmax);
			}
			else if (axesConfig == 2)
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
void JoystickHandlerDetails::UpdateJoysticks()
{
	for (int idx = 0; idx < NUM_PC_JOYSTICKS; ++idx)
	{
		if (m_PCJoystickState[0].Captured)
			TranslateJoystick(idx);
	}
}

/*****************************************************************************/
// Look for file specific joystick map
void JoystickHandlerDetails::CheckForJoystickMap(const char *path)
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
void JoystickHandlerDetails::ResetJoystickMap()
{
	if (MessageBox(GetHWnd(), "Clear joystick to keyboard mapping table?", WindowTitle, MB_YESNO | MB_ICONQUESTION) != IDYES)
		return;

	ResetJoyMap(&JoystickMap);
}

/****************************************************************************/
void JoystickHandlerDetails::ResetJoyMap(JoyMap* joymap)
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
void JoystickHandlerDetails::ResetJoyMapToDefaultUser(void)
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
bool JoystickHandlerDetails::ReadJoyMap(const char *filename, JoyMap *joymap)
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
			char errstr[500];
			sprintf(errstr, "Invalid line in joystick mapping file:\n  %s\n  line %d\n",
				filename, line);
			MessageBox(GetHWnd(), errstr, WindowTitle, MB_OK | MB_ICONERROR);
			success = false;
			break;
		}

		// Get vkey number and mapping entry from input name
		int vkey = SelectKeyDialog::JoyVKeyByName(inputName);
		if (vkey < BEEB_VKEY_JOY_START || vkey >= BEEB_VKEY_JOY_END)
		{
			char errstr[500];
			sprintf(errstr, "Invalid input name in joystick mapping file:\n  %s\n  line %d\n",
				filename, line);
			MessageBox(GetHWnd(), errstr, WindowTitle, MB_OK | MB_ICONERROR);
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
			char errstr[500];
			sprintf(errstr, "Invalid key name in joystick mapping file:\n  %s\n  line %d\n",
				filename, line);
			MessageBox(GetHWnd(), errstr, WindowTitle, MB_OK | MB_ICONERROR);
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
				char errstr[500];
				sprintf(errstr, "Invalid key name in joystick mapping file:\n  %s\n  line %d\n",
					filename, line);
				MessageBox(GetHWnd(), errstr, WindowTitle, MB_OK | MB_ICONERROR);
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
void JoystickHandlerDetails::LoadJoystickMap()
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

	FileDialog fileDialog(GetHWnd(), FileName, sizeof(FileName), DefaultPath, filter);

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
bool JoystickHandlerDetails::WriteJoyMap(const char *filename, JoyMap *joymap)
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
void JoystickHandlerDetails::SaveJoystickMap()
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

	FileDialog fileDialog(GetHWnd(), FileName, sizeof(FileName), DefaultPath, filter);

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

/* Backward compatibility */
static const char *CFG_OPTIONS_STICKS = "Sticks";

/* New joystick options */
static const char *CFG_OPTIONS_STICK_PCSTICK = "Stick%dPCStick";
static const char *CFG_OPTIONS_STICK_PCAXES = "Stick%dPCAxes";
static const char *CFG_OPTIONS_STICK_ANALOG = "Stick%dAnalogMousepad";
static const char *CFG_OPTIONS_STICK_DIGITAL = "Stick%dDigitalMousepad";
static const char* CFG_OPTIONS_JOYSTICK_GAIN = "JoystickGain";
static const char* CFG_OPTIONS_JOYSTICK_ORDER = "JoystickOrder%d";

static const char *CFG_OPTIONS_STICKS_TO_KEYS = "SticksToKeys";
static const char *CFG_OPTIONS_AUTOLOAD_JOYSICK_MAP = "AutoloadJoystickMap";
static const char *CFG_OPTIONS_STICKS_DEADBAND = "SticksToKeysDeadBand";

/****************************************************************************/
bool JoystickHandlerDetails::GetNthBoolValue(Preferences& preferences, const char* format, int idx, bool& value)
{
	char option_str[256];
	sprintf_s(option_str, format, idx);
	return preferences.GetBoolValue(option_str, value);
}

/****************************************************************************/
bool JoystickHandlerDetails::GetNthDWORDValue(Preferences& preferences, const char* format, int idx, DWORD& value)
{
	char option_str[256];
	sprintf_s(option_str, format, idx);
	return preferences.GetDWORDValue(option_str, value);
}

/****************************************************************************/
bool JoystickHandlerDetails::GetNthStringValue(Preferences& preferences, const char* format, int idx, std::string& value)
{
	char option_str[256];
	sprintf_s(option_str, format, idx);
	return preferences.GetStringValue(option_str, value);
}

/****************************************************************************/
void JoystickHandlerDetails::SetNthBoolValue(Preferences& preferences, const char* format, int idx, bool value)
{
	char option_str[256];
	sprintf_s(option_str, format, idx);
	preferences.SetBoolValue(option_str, value);
}

/****************************************************************************/
void JoystickHandlerDetails::SetNthDWORDValue(Preferences& preferences, const char* format, int idx, DWORD value)
{
	char option_str[256];
	sprintf_s(option_str, format, idx);
	preferences.SetDWORDValue(option_str, value);
}

/****************************************************************************/
void JoystickHandlerDetails::SetNthStringValue(Preferences& preferences, const char* format, int idx, const std::string& value)
{
	char option_str[256];
	sprintf_s(option_str, format, idx);
	preferences.SetStringValue(option_str, value);
}

/****************************************************************************/
void JoystickHandlerDetails::EraseNthValue(Preferences& preferences, const char* format, int idx)
{
	char option_str[256];
	sprintf_s(option_str, format, idx);
	preferences.EraseValue(option_str);
}

/****************************************************************************/
void JoystickHandlerDetails::ReadPreferences(Preferences& preferences)
{
	DWORD dword;
	bool flag;

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
	if (preferences.GetDWORDValue(CFG_OPTIONS_STICKS, dword))
	{
		m_MenuIdSticks[0] = dword;
		m_JoystickConfig[0].Enabled = true;
		m_JoystickConfig[0].PCStick = MenuIdToStick(0, dword);
		m_JoystickConfig[0].AnalogMousestick = (dword == JoystickMenuIds[0].AnalogMousestick);
		m_JoystickConfig[0].DigitalMousestick = (dword == JoystickMenuIds[0].DigitalMousestick);
	}

	/* New joystick options */
	for (int bbcIdx = 0; bbcIdx < NUM_BBC_JOYSTICKS; ++bbcIdx)
	{
		BBCJoystickConfig& config = m_JoystickConfig[bbcIdx];

		if (GetNthBoolValue(preferences, CFG_OPTIONS_STICK_ANALOG, bbcIdx + 1, flag) && flag)
		{
			config.AnalogMousestick = true;
			config.Enabled = true;
			m_MenuIdSticks[bbcIdx] = JoystickMenuIds[bbcIdx].AnalogMousestick;
		}
		else if (GetNthBoolValue(preferences, CFG_OPTIONS_STICK_DIGITAL, bbcIdx + 1, flag) && flag)
		{
			config.DigitalMousestick = true;
			config.Enabled = true;
			m_MenuIdSticks[bbcIdx] = JoystickMenuIds[bbcIdx].DigitalMousestick;

		}
		else if (GetNthDWORDValue(preferences, CFG_OPTIONS_STICK_PCSTICK, bbcIdx + 1, dword) && dword != 0)
		{
			config.PCStick = dword;
			config.Enabled = true;
			m_MenuIdSticks[bbcIdx] = StickToMenuId(bbcIdx, dword);
		}

		if (GetNthDWORDValue(preferences, CFG_OPTIONS_STICK_PCAXES, bbcIdx + 1, dword))
			config.PCAxes = dword;

		m_MenuIdAxes[bbcIdx] = AxesToMenuId(bbcIdx, config.PCAxes);
	}

	if (!preferences.GetBoolValue(CFG_OPTIONS_STICKS_TO_KEYS, m_JoystickToKeys))
		m_JoystickToKeys = false;

	if (!preferences.GetBoolValue(CFG_OPTIONS_AUTOLOAD_JOYSICK_MAP,
		m_AutoloadJoystickMap))
		m_AutoloadJoystickMap = false;

	if (preferences.GetDWORDValue(CFG_OPTIONS_STICKS_DEADBAND, dword))
		m_Deadband = dword;
	else
		m_Deadband = DEFAULT_JOY_DEADBAND;

	if (preferences.GetDWORDValue(CFG_OPTIONS_JOYSTICK_GAIN, dword))
		m_Gain = static_cast<double>(dword) / 0x10000;
	else
		m_Gain = 1.0;

	m_JoystickOrder.clear();
	for (int idx = 0; idx < MAX_JOYSTICK_ORDER; ++idx)
	{
		std::string value;
		JoystickOrderEntry entry;
		if (!GetNthStringValue(preferences, CFG_OPTIONS_JOYSTICK_ORDER, idx + 1, value)
		    || !entry.from_string(value))
			break;
		m_JoystickOrder.push_back(entry);

	}

}

/****************************************************************************/
void JoystickHandlerDetails::WritePreferences(Preferences& preferences)
{
	for (int bbcIdx = 0; bbcIdx < NUM_BBC_JOYSTICKS; ++bbcIdx)
	{
		BBCJoystickConfig& config = m_JoystickConfig[bbcIdx];
		SetNthBoolValue(preferences, CFG_OPTIONS_STICK_ANALOG, bbcIdx + 1, config.AnalogMousestick);
		SetNthBoolValue(preferences, CFG_OPTIONS_STICK_DIGITAL, bbcIdx + 1, config.DigitalMousestick);
		SetNthDWORDValue(preferences, CFG_OPTIONS_STICK_PCSTICK, bbcIdx + 1, config.PCStick);
		SetNthDWORDValue(preferences, CFG_OPTIONS_STICK_PCAXES, bbcIdx + 1, config.PCAxes);
	}

	preferences.SetBoolValue(CFG_OPTIONS_STICKS_TO_KEYS, m_JoystickToKeys);
	preferences.SetBoolValue(CFG_OPTIONS_AUTOLOAD_JOYSICK_MAP, m_AutoloadJoystickMap);
	preferences.SetDWORDValue(CFG_OPTIONS_STICKS_DEADBAND, m_Deadband);
	preferences.SetDWORDValue(CFG_OPTIONS_JOYSTICK_GAIN, static_cast<DWORD>(m_Gain * 0x10000));

	/* Remove obsolete values */
	preferences.EraseValue(CFG_OPTIONS_STICKS);
	preferences.EraseValue("Sticks2");
	preferences.EraseValue("JoystickAxes1");
	preferences.EraseValue("JoystickAxes2");
	preferences.EraseValue("Stick1ToKeysDeadBand");
	preferences.EraseValue("Stick2ToKeysDeadBand");
}

/****************************************************************************/
void JoystickHandlerDetails::WriteJoystickOrder(Preferences& preferences)
{
	for (UINT idx = 0; idx < MAX_JOYSTICK_ORDER; ++idx)
	{
		if (idx < m_JoystickOrder.size())
		{
			SetNthStringValue(preferences, CFG_OPTIONS_JOYSTICK_ORDER, idx + 1,
				m_JoystickOrder[idx].to_string());
		}
		else
		{
			EraseNthValue(preferences, CFG_OPTIONS_JOYSTICK_ORDER, idx + 1);
		}
	}
}

/****************************************************************************/
JoystickHandler::JoystickHandler(BeebWin* beebWin) : m_Details{ std::make_unique<JoystickHandlerDetails>(beebWin) } {}

JoystickHandler::~JoystickHandler() {}

int JoystickHandler::GetMenuIdSticks(int bbcIdx) { return m_Details->GetMenuIdSticks(bbcIdx); }

bool JoystickHandler::GetJoystickToKeys() { return m_Details->GetJoystickToKeys(); }

void JoystickHandler::SetJoystickToKeys(bool enabled) { m_Details->SetJoystickToKeys(enabled); }

void JoystickHandler::SetJoystickTarget(HWND target) { m_Details->SetJoystickTarget(target); }

void JoystickHandler::InitMenu(void) { m_Details->InitMenu(); }

void JoystickHandler::ProcessMenuCommand(int bbcIdx, UINT menuId) { m_Details->ProcessMenuCommand(bbcIdx, menuId); }

void JoystickHandler::ProcessAxesMenuCommand(int bbcIdx, UINT menuId) { m_Details->ProcessAxesMenuCommand(bbcIdx, menuId); }

void JoystickHandler::ToggleJoystickToKeys() { m_Details->ToggleJoystickToKeys(); }

void JoystickHandler::ToggleAutoloadJoystickMap() { m_Details->ToggleAutoloadJoystickMap(); }

bool JoystickHandler::InitJoystick(bool verbose) { return m_Details->InitJoystick(verbose); }

void JoystickHandler::SetMousestickButton(int index, bool button) { m_Details->SetMousestickButton(index, button); }

void JoystickHandler::ScaleMousestick(unsigned int x, unsigned int y) { m_Details->ScaleMousestick(x, y); }

void JoystickHandler::UpdateJoysticks(void) { m_Details->UpdateJoysticks(); }

void JoystickHandler::CheckForJoystickMap(const char* path) { m_Details->CheckForJoystickMap(path); }

void JoystickHandler::ResetJoystickMap(void) { m_Details->ResetJoystickMap(); }

void JoystickHandler::ResetJoyMapToDefaultUser(void) { m_Details->ResetJoyMapToDefaultUser(); }

void JoystickHandler::LoadJoystickMap(void) { m_Details->LoadJoystickMap(); }

void JoystickHandler::SaveJoystickMap(void) { m_Details->SaveJoystickMap(); }

void JoystickHandler::ReadPreferences(Preferences& preferences) { m_Details->ReadPreferences(preferences); }

void JoystickHandler::WritePreferences(Preferences& preferences) { m_Details->WritePreferences(preferences); }

void JoystickHandler::WriteJoystickOrder(Preferences& preferences) { m_Details->WriteJoystickOrder(preferences); }
