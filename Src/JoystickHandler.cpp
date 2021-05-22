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
	UINT Axes[2];
	UINT AnalogMousestick;
	UINT DigitalMousestick;
};

/*****************************************************************************/
constexpr JoystickMenuIdsType JoystickMenuIds[NUM_BBC_JOYSTICKS]{
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

/*****************************************************************************/
std::string JoystickOrderEntry::to_string()
{
	char buf[10];
	sprintf_s(buf, "%04x:%04x", mId(), pId());
	std::string result(buf);
	if (Name.length() != 0)
		result += " # " + Name;
	return result;
}

/*****************************************************************************/
bool JoystickOrderEntry::from_string(const std::string& str)
{
	Name = "";

	size_t split = str.find('#');
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
	if (!m_Configured)
		return std::string{};

	std::string name = m_Name;
	if (m_Instance != 1)
		name += " #" + std::to_string(m_Instance);
	return name;
}

#ifndef NDEBUG
void OutputDebug(const char* format, ...)
{
	char buf[256];
	va_list var;
	va_start(var, format);
	vsnprintf(buf, sizeof(buf), format, var);
	OutputDebugString(buf);
	va_end(var);
}
#else
void OutputDebug(const char* format, ...) { (void)format; }
#endif

/*****************************************************************************/
bool JoystickDev::Update()
{
	HRESULT hr;
	if (!m_Present)
		return false;

	hr = m_Device->Poll();
	if (FAILED(hr)) { OutputDebug("Poll result %08x\n", hr); }
	if (FAILED(hr))
	{
		hr = m_Device->Acquire();
		OutputDebug("Acquire result %08x\n", hr);

		if (hr == DIERR_UNPLUGGED)
			return false;

		return true;
	}
	if (FAILED(hr = m_Device->GetDeviceState(sizeof(DIJOYSTATE2), &m_JoyState)))
	{
		OutputDebug("GeteviceState result %08x\n", hr);
		return false;
	}
	return true;
}

/*****************************************************************************/
DWORD JoystickDev::GetButtons()
{
	DWORD buttons{ 0 };
	for (int i = 0; i < JOYSTICK_MAX_BTNS && i < sizeof(DIJOYSTATE2::rgbButtons); ++i)
	{
		if (m_JoyState.rgbButtons[i] & 0x80)
			buttons |= (1 << i);
	}
	return buttons;
}

static constexpr unsigned int setbit(int bit) { return 1u << bit; }
constexpr DWORD xboxAxes = setbit(JOYSTICK_AXIS_RX_P) | setbit(JOYSTICK_AXIS_RY_P);
constexpr DWORD stdpadAxes = setbit(JOYSTICK_AXIS_Z_P) | setbit(JOYSTICK_AXIS_RZ_P);

/*****************************************************************************/
void JoystickDev::GetAxesValue(int axesSet, int& x, int& y)
{
	switch (axesSet)
	{
	case 1:
	case 2:
		if ((m_PresentAxes & xboxAxes) == xboxAxes)
		{
			x = m_JoyState.lRx;
			y = m_JoyState.lRy;
		}
		else if ((m_PresentAxes & stdpadAxes) == stdpadAxes)
		{
			x = m_JoyState.lZ;
			y = m_JoyState.lRz;
		}
		else
		{
			x = 32767;
			y = 32767;
		}
		break;
	default:
		x = m_JoyState.lX;
		y = m_JoyState.lY;
		break;
	}
}

DWORD JoystickDev::GetAxesState(int threshold)
{
	DIJOYSTATE2& state = m_JoyState;
	unsigned int axes = 0;

	auto Detect = [&axes, threshold](const int val, const int nbit, const int pbit) {
		if (val < 32767 - threshold)
			axes |= (1 << nbit);
		else if (val > 32767 + threshold)
			axes |= (1 << pbit);
	};

	Detect(state.lX, JOYSTICK_AXIS_LEFT, JOYSTICK_AXIS_RIGHT);
	Detect(state.lY, JOYSTICK_AXIS_UP, JOYSTICK_AXIS_DOWN);

	// If axes RX and RY are not present, but Z and RZ are,
	// map Z and RZ to RX and RY
	if ((m_PresentAxes & xboxAxes) == 0)
	{
		if (m_PresentAxes & setbit(JOYSTICK_AXIS_Z_P))
			Detect(state.lZ, JOYSTICK_AXIS_RX_N, JOYSTICK_AXIS_RX_P);
		if (m_PresentAxes & setbit(JOYSTICK_AXIS_RZ_P))
			Detect(state.lZ, JOYSTICK_AXIS_RY_N, JOYSTICK_AXIS_RY_P);
	}
	else
	{
		if (m_PresentAxes & setbit(JOYSTICK_AXIS_Z_P))
			Detect(state.lZ, JOYSTICK_AXIS_Z_N, JOYSTICK_AXIS_Z_P);
		if (m_PresentAxes & setbit(JOYSTICK_AXIS_RX_P))
			Detect(state.lRx, JOYSTICK_AXIS_RX_N, JOYSTICK_AXIS_RX_P);
		if (m_PresentAxes & setbit(JOYSTICK_AXIS_RY_P))
			Detect(state.lRy, JOYSTICK_AXIS_RY_N, JOYSTICK_AXIS_RY_P);
		if (m_PresentAxes & setbit(JOYSTICK_AXIS_RZ_P))
			Detect(state.lRz, JOYSTICK_AXIS_RZ_N, JOYSTICK_AXIS_RZ_P);
	}

	if (m_PresentAxes & setbit(JOYSTICK_AXIS_HAT_UP))
	{
		DWORD pov = state.rgdwPOV[0];
		if (pov != -1)
		{
			if (pov >= 29250 || pov < 6750)
				axes |= (1 << JOYSTICK_AXIS_HAT_UP);
			if (pov >= 2250 && pov < 15750)
				axes |= (1 << JOYSTICK_AXIS_HAT_RIGHT);
			if (pov >= 11250 && pov < 24750)
				axes |= (1 << JOYSTICK_AXIS_HAT_DOWN);
			if (pov >= 20250 && pov < 33750)
				axes |= (1 << JOYSTICK_AXIS_HAT_LEFT);
		}
	}

	return axes;
}

/****************************************************************************/
void JoystickDev::EnumObjectsCallback(const DIDEVICEOBJECTINSTANCE* pdidoi)
{
	// For axes that are returned, set the DIPROP_RANGE property for the
	// enumerated axis in order to scale min/max values.
	if (pdidoi->dwType & DIDFT_AXIS)
	{
		DIPROPRANGE diprg{};
		diprg.diph.dwSize = sizeof(DIPROPRANGE);
		diprg.diph.dwHeaderSize = sizeof(DIPROPHEADER);
		diprg.diph.dwHow = DIPH_BYID;
		diprg.diph.dwObj = pdidoi->dwType; // Specify the enumerated axis
		diprg.lMin = 0;
		diprg.lMax = 65535;

		// Set the range for the axis
		if (FAILED(m_Device->SetProperty(DIPROP_RANGE, &diprg.diph)))
			return;
	}

	if (pdidoi->dwType & DIDFT_BUTTON)
	{
		WORD instance = DIDFT_GETINSTANCE(pdidoi->dwType);
		if (instance < JOYSTICK_MAX_BTNS)
			m_PresentButtons |= setbit(instance);
	}

	if (pdidoi->guidType == GUID_YAxis)
		m_PresentAxes |= setbit(JOYSTICK_AXIS_UP) | setbit(JOYSTICK_AXIS_DOWN);
	if (pdidoi->guidType == GUID_XAxis)
		m_PresentAxes |= setbit(JOYSTICK_AXIS_LEFT) | setbit(JOYSTICK_AXIS_RIGHT);
	if (pdidoi->guidType == GUID_ZAxis)
		m_PresentAxes |= setbit(JOYSTICK_AXIS_Z_N) | setbit(JOYSTICK_AXIS_Z_P);
	if (pdidoi->guidType == GUID_RxAxis)
		m_PresentAxes |= setbit(JOYSTICK_AXIS_RX_N) | setbit(JOYSTICK_AXIS_RX_P);
	if (pdidoi->guidType == GUID_RyAxis)
		m_PresentAxes |= setbit(JOYSTICK_AXIS_RY_N) | setbit(JOYSTICK_AXIS_RY_P);
	if (pdidoi->guidType == GUID_RzAxis)
		m_PresentAxes |= setbit(JOYSTICK_AXIS_RZ_N) | setbit(JOYSTICK_AXIS_RZ_P);
	if (pdidoi->guidType == GUID_POV)
		m_PresentAxes |= setbit(JOYSTICK_AXIS_HAT_LEFT) | setbit(JOYSTICK_AXIS_HAT_RIGHT)
				| setbit(JOYSTICK_AXIS_HAT_UP) | setbit(JOYSTICK_AXIS_HAT_DOWN);
}

/****************************************************************************/
JoystickDev::JoystickDev(JoystickDev&& r)
{
	m_GUIDInstance = r.m_GUIDInstance;
	m_Id = r.m_Id;
	std::swap(m_Name, r.m_Name);
	std::swap(m_Device, r.m_Device);
	m_PresentAxes = r.m_PresentAxes;
	m_PresentButtons = r.m_PresentButtons;
	m_Instance = r.m_Instance;
	m_OnOrderList = r.m_OnOrderList;
	m_JoyIndex = r.m_JoyIndex;
	m_Configured = r.m_Configured;
	m_Present = r.m_Present;
	m_Captured = r.m_Captured;
	m_PrevAxes = r.m_PrevAxes;
	m_PrevBtns = r.m_PrevBtns;
	m_JoystickToKeysActive = r.m_JoystickToKeysActive;
	r.m_Present = false;
}

/****************************************************************************/
JoystickDev& JoystickDev::operator=(JoystickDev&& r)
{
	m_GUIDInstance = r.m_GUIDInstance;
	m_Id = r.m_Id;
	std::swap(m_Name, r.m_Name);
	std::swap(m_Device, r.m_Device);
	m_PresentAxes = r.m_PresentAxes;
	m_PresentButtons = r.m_PresentButtons;
	m_Instance = r.m_Instance;
	m_OnOrderList = r.m_OnOrderList;
	m_JoyIndex = r.m_JoyIndex;
	m_Configured = r.m_Configured;
	m_Present = r.m_Present;
	m_Captured = r.m_Captured;
	m_PrevAxes = r.m_PrevAxes;
	m_PrevBtns = r.m_PrevBtns;
	m_JoystickToKeysActive = r.m_JoystickToKeysActive;
	r.m_Present = false;
	return *this;
}

/****************************************************************************/
void JoystickDev::CloseDevice()
{
	m_Captured = false;
	if (m_Device)
	{
		m_Device->Unacquire();
		m_Device->Release();
		m_Device = nullptr;
	}
}

/****************************************************************************/
JoystickDev::~JoystickDev()
{
	CloseDevice();
}

/****************************************************************************/
void JoystickHandler::AddDeviceInstance(const DIDEVICEINSTANCE* pdidInstance)
{
	int mid = LOWORD(pdidInstance->guidProduct.Data1);
	int pid = HIWORD(pdidInstance->guidProduct.Data1);

	int index = m_JoystickDevs.size();

	JoystickDev dev;
	dev.m_GUIDInstance = pdidInstance->guidInstance;
	dev.m_Id = JoystickId(mid, pid);
	dev.m_Name = pdidInstance->tszInstanceName;
	dev.m_Configured = true;
	dev.m_Present = true;
	dev.m_JoyIndex = index;

	m_JoystickDevs.emplace_back(std::move(dev));
}

/****************************************************************************/

static BOOL CALLBACK EnumJoysticksCallback(const DIDEVICEINSTANCE* pdidInstance, VOID* pContext)
{
	reinterpret_cast<JoystickHandler*>(pContext)->AddDeviceInstance(pdidInstance);
	return DIENUM_CONTINUE;
}

/****************************************************************************/
HRESULT JoystickHandler::ScanJoysticks()
{
	bool usePreferredCfg = m_HavePreferredJoyCfg;

	std::map<JoystickId, std::list<JoystickDev*>> listById;

	// Clear joystick devs vector - closes any open devices
	m_JoystickDevs.clear();

	if (FAILED(m_DirectInputInitResult))
		return m_DirectInputInitResult;

	HRESULT hr = m_pDirectInput->EnumDevices(DI8DEVCLASS_GAMECTRL, EnumJoysticksCallback, this, DIEDFL_ATTACHEDONLY);
	if (FAILED(hr))
		return hr;

	for (unsigned int devIdx = 0; devIdx < m_JoystickDevs.size(); ++devIdx)
	{
		JoystickDev& dev = m_JoystickDevs[devIdx];
		listById[dev.Id()].push_back(&dev);
		dev.m_Instance = listById[dev.Id()].size();
	}

	std::vector<JoystickDev> sorted;
	// Start with joysticks in the order list
	for (JoystickOrderEntry& entry : m_JoystickOrder)
	{
		std::list<JoystickDev*>& list = listById[entry];
		if (list.size() != 0)
		{
			JoystickDev& dev = *(list.front());
			list.pop_front();
			dev.m_OnOrderList = true;
			dev.m_JoyIndex = sorted.size();
			if (usePreferredCfg && IsEqualGUID(dev.m_GUIDInstance, m_PreferredJoyCfg.guidInstance))
				usePreferredCfg = false;
			sorted.emplace_back(std::move(dev));
			entry.JoyIndex = dev.m_JoyIndex;
		}
		else
		{
			entry.JoyIndex = -1;
		}
	}

	int newJoysticks = 0;

	// Add preferred joystick first
	if (usePreferredCfg)
	{
		for (JoystickDev& dev : m_JoystickDevs)
		{
			if (dev.m_Present && !dev.m_OnOrderList && IsEqualGUID(dev.m_GUIDInstance, m_PreferredJoyCfg.guidInstance))
			{
				dev.m_JoyIndex = sorted.size();
				sorted.emplace_back(std::move(dev));
				++newJoysticks;
				break;
			}
		}
	}

	// Add joysticks not in the order list
	for (JoystickDev& dev : m_JoystickDevs)
	{
		if (dev.m_Present && !dev.m_OnOrderList)
		{
			dev.m_JoyIndex = sorted.size();
			sorted.emplace_back(std::move(dev));
			++newJoysticks;
		}
	}

	// If joystick order list is too long, remove some unconnected entries
	// Remove entries from the beginning or from the end, or some other order?
	int to_remove = newJoysticks + m_JoystickOrder.size() - MAX_JOYSTICK_ORDER;
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

	// Replace joystick device vector with sorted one
	m_JoystickDevs = std::move(sorted);

	// Add new joysticks at the end of order list
	for (JoystickDev& dev : m_JoystickDevs)
	{
		if (!dev.m_OnOrderList)
		{
			m_JoystickOrder.emplace_back(dev.Id(), dev.DisplayString(), dev.m_JoyIndex);
			dev.m_OnOrderList = true;
		}
	}

	return S_OK;
}

/****************************************************************************/

static BOOL CALLBACK EnumJoystickObjectsCallback(const DIDEVICEOBJECTINSTANCE* pdidoi, VOID* pContext)
{
	reinterpret_cast<JoystickDev*>(pContext)->EnumObjectsCallback(pdidoi);
	return DIENUM_CONTINUE;
}

/****************************************************************************/
HRESULT JoystickHandler::OpenDevice(HWND mainWindow, JoystickDev* dev)
{
	HRESULT hr;

	// Do nothing if device already open
	if (dev->m_Device)
		return S_OK;

	if (FAILED(hr = m_pDirectInput->CreateDevice(dev->m_GUIDInstance, &dev->m_Device, nullptr)))
		return hr;

	if (FAILED(hr = dev->m_Device->SetDataFormat(&c_dfDIJoystick2)))
	{
		dev->CloseDevice();
		return hr;
	}

	if (FAILED(hr = dev->m_Device->SetCooperativeLevel(mainWindow, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND)))
	{
		dev->CloseDevice();
		return hr;
	}

	if (FAILED(hr = dev->m_Device->EnumObjects(EnumJoystickObjectsCallback, (VOID*)dev, DIDFT_ALL)))
	{
		dev->CloseDevice();
		return hr;
	}

	return S_OK;
}

/****************************************************************************/
HRESULT JoystickHandler::InitDirectInput()
{
	if (FAILED(m_DirectInputInitResult = DirectInput8Create(GetModuleHandle(NULL),
			DIRECTINPUT_VERSION, IID_IDirectInput8, (VOID**)&m_pDirectInput, NULL)))
		return m_DirectInputInitResult;

	HRESULT hr;
	IDirectInputJoyConfig8* pJoyConfig = nullptr;
	if (FAILED(hr = m_pDirectInput->QueryInterface(IID_IDirectInputJoyConfig8, (void**)&pJoyConfig)))
		return S_OK;

	m_PreferredJoyCfg.dwSize = sizeof(m_PreferredJoyCfg);
	if (SUCCEEDED(pJoyConfig->GetConfig(0, &m_PreferredJoyCfg, DIJC_GUIDINSTANCE)))
		m_HavePreferredJoyCfg = true;

	if (pJoyConfig)
		pJoyConfig->Release();

	return S_OK;
}

/****************************************************************************/
JoystickHandler::~JoystickHandler()
{
	if (m_pDirectInput)
	{
		m_pDirectInput->Release();
		m_pDirectInput = nullptr;
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
		(static_cast<UINT>(m_MenuIdSticks[bbcIdx]) == JoystickMenuIds[bbcIdx].AnalogMousestick);
	m_JoystickConfig[bbcIdx].DigitalMousestick =
		(static_cast<UINT>(m_MenuIdSticks[bbcIdx]) == JoystickMenuIds[bbcIdx].DigitalMousestick);
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
	CheckMenuItem(IDM_INIT_JOYSTICK, m_JoystickHandler->m_JoystickDevs.size() != 0);

	// Enable axes menu items if any PC joystick is assigned to the related BBC joystick
	for (int bbcIdx = 0; bbcIdx < NUM_BBC_JOYSTICKS; ++bbcIdx)
	{
		bool EnableAxes = false;
		static const char* const menuItems[2] = { "First PC &Joystick", "Second PC Joystick" };

		for (int pcIdx = 0; pcIdx < _countof(JoystickMenuIdsType::Joysticks); ++pcIdx)
		{
			JoystickDev* dev = m_JoystickHandler->GetDev(pcIdx);
			if (dev)
			{
				SetMenuItemText(JoystickMenuIds[bbcIdx].Joysticks[pcIdx], dev->DisplayString());
			}
			else
			{
				SetMenuItemText(JoystickMenuIds[bbcIdx].Joysticks[pcIdx], menuItems[pcIdx]);
			}
		}

		if (GetPCJoystick(bbcIdx) != 0)
			EnableAxes = true;

		for (int axesIdx = 0; axesIdx < _countof(JoystickMenuIdsType::Axes); ++axesIdx)
			EnableMenuItem(JoystickMenuIds[bbcIdx].Axes[axesIdx], EnableAxes);
	}

	CheckMenuItem(IDM_JOY_SENSITIVITY_50, m_JoystickSensitivity == 0.5);
	CheckMenuItem(IDM_JOY_SENSITIVITY_100, m_JoystickSensitivity == 1.0);
	CheckMenuItem(IDM_JOY_SENSITIVITY_200, m_JoystickSensitivity == 2.0);
	CheckMenuItem(IDM_JOY_SENSITIVITY_300, m_JoystickSensitivity == 3.0);

	CheckMenuItem(IDM_JOY_KEY_THRESHOLD_12_5, m_JoystickToKeysThreshold == 4096);
	CheckMenuItem(IDM_JOY_KEY_THRESHOLD_25, m_JoystickToKeysThreshold == 8192);
	CheckMenuItem(IDM_JOY_KEY_THRESHOLD_50, m_JoystickToKeysThreshold == 16384);
	CheckMenuItem(IDM_JOY_KEY_THRESHOLD_75, m_JoystickToKeysThreshold == 24756);
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
	for (JoystickDev& dev : m_JoystickHandler->m_JoystickDevs)
	{
		if (dev.m_Captured)
		{
			dev.m_Captured = false;
			TranslateJoystick(dev.m_JoyIndex);
		}
		dev.CloseDevice();
	}
}

/****************************************************************************/
bool BeebWin::ScanJoysticks(bool verbose)
{
	ResetJoystick();

	if (!m_JoystickHandler->m_DirectInputInitialized)
		m_JoystickHandler->InitDirectInput();

	if (FAILED(m_JoystickHandler->m_DirectInputInitResult))
	{
		if (verbose)
		{
			Report(MessageType::Error,
			       "DirectInput initialization failed.\nError Code: %08X",
			       m_JoystickHandler->m_DirectInputInitResult);
		}
		return false;
	}

	HRESULT hr;
	if (FAILED(hr = m_JoystickHandler->ScanJoysticks()))
	{
		if (verbose)
		{
			Report(MessageType::Error,
			       "Joystick enumeration failed.\nError Code: %08X", hr);
		}
		return false;
	}

	if (m_JoystickHandler->m_JoystickDevs.size() == 0)
	{
		if (verbose)
		{
			Report(MessageType::Error, "No joysticks found");
		}
		return false;
	}

	return true;
}

/****************************************************************************/
void BeebWin::InitJoystick(bool verbose)
{

	ResetJoystick();

	if (m_JoystickToKeys)
	{
		for (int pcIdx = 0; pcIdx < NUM_PC_JOYSTICKS; ++pcIdx)
		{
			CaptureJoystick(pcIdx, verbose);
		}
	}

	int pcJoy1 = GetPCJoystick(0);
	if (pcJoy1 != 0)
	{
		if (!m_JoystickToKeys || pcJoy1 > NUM_PC_JOYSTICKS)
			CaptureJoystick(pcJoy1 - 1, verbose);
	}

	int pcJoy2 = GetPCJoystick(1);
	if (pcJoy2 != 0 && pcJoy2 != pcJoy1)
	{
		if (!m_JoystickToKeys || pcJoy2 > NUM_PC_JOYSTICKS)
			CaptureJoystick(pcJoy2 - 1, verbose);
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
bool BeebWin::CaptureJoystick(int Index, bool verbose)
{
	bool success = false;
	JoystickDev* dev = m_JoystickHandler->GetDev(Index);

	if (dev)
	{
		HRESULT hr = m_JoystickHandler->OpenDevice(m_hWnd, dev);
		if (!FAILED(hr))
		{
			dev->m_Captured = true;
			success = true;
		}
		else if (verbose)
		{
			mainWin->Report(MessageType::Warning,
			                "Failed to initialise %s",
			                dev->DisplayString().c_str());
		}
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
	JoystickDev* dev = m_JoystickHandler->GetDev(joyId);
	unsigned int& prevAxes = dev->m_PrevAxes;
	int vkeys = BEEB_VKEY_JOY_START + joyId * (JOYSTICK_MAX_AXES + JOYSTICK_MAX_BTNS);

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
	JoystickDev* dev = m_JoystickHandler->GetDev(joyId);
	unsigned int& prevBtns = dev->m_PrevBtns;
	int vkeys = BEEB_VKEY_JOY_START + JOYSTICK_MAX_AXES
			+ joyId * (JOYSTICK_MAX_AXES + JOYSTICK_MAX_BTNS);

	if (buttons != prevBtns)
	{
		for (int btnId = 0; btnId < JOYSTICK_MAX_BTNS; ++btnId)
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
	JoystickDev* joyDev = m_JoystickHandler->GetDev(joyId);
	bool success = false;
	DWORD buttons = 0;

	if (!joyDev)
		return;

	if (joyDev->m_Captured && joyDev->Update())
	{
		success = true;
		buttons = joyDev->GetButtons();
	}

	// If joystick is disabled or error occurs, reset all axes and buttons
	if (!success)
	{
		if (joyDev->m_Captured)
		{
			// Reset 'captured' flag and update menu entry
			joyDev->m_Captured = false;
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
	if (joyId < NUM_PC_JOYSTICKS && m_JoystickToKeys && success)
	{
		DWORD axes = joyDev->GetAxesState(m_JoystickToKeysThreshold);
		TranslateAxes(joyId, axes);
		TranslateJoystickButtons(joyId, buttons);
		joyDev->m_JoystickToKeysActive = true;
	}

	// Make sure to reset keyboard state
	if (!m_JoystickToKeys && joyDev->m_JoystickToKeysActive)
	{
		TranslateAxes(joyId, 0);
		TranslateJoystickButtons(joyId, 0);
		joyDev->m_JoystickToKeysActive = false;
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
			JoystickDev* dev = m_JoystickHandler->GetDev(idx);
			if (dev && dev->m_Captured)
			{
				TranslateJoystick(idx);
				translated |= setbit(idx);
			}
		}
	}

	// Translate joystick for BBC joystick 1
	if (m_JoystickConfig[0].Enabled && m_JoystickConfig[0].PCStick != 0)
	{
		int pcStick = m_JoystickConfig[0].PCStick - 1;
		if (!(translated & setbit(pcStick)))
		{
			TranslateJoystick(pcStick);
			translated |= setbit(pcStick);
		}
	}

	// Translate joystick for BBC joystick 2
	if (m_JoystickConfig[1].Enabled && m_JoystickConfig[1].PCStick != 0)
	{
		int pcStick = m_JoystickConfig[1].PCStick - 1;
		if (!(translated & setbit(pcStick)))
		{
			TranslateJoystick(pcStick);
			translated |= setbit(pcStick);
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
	for (KeyPair& mapping : *joymap)
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
	std::vector<JoystickOrderEntry>& order = m_JoystickHandler->m_JoystickOrder;
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

	m_JoystickHandler->m_JoystickOrder.clear();
	for (int idx = 0; idx < MAX_JOYSTICK_ORDER; ++idx)
	{
		std::string value;
		JoystickOrderEntry entry;
		if (!GetNthStringValue(m_Preferences, CFG_OPTIONS_JOYSTICK_ORDER, idx + 1, value)
		    || !entry.from_string(value))
			break;
		m_JoystickHandler->m_JoystickOrder.push_back(entry);

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
