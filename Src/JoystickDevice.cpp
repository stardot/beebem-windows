/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
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
#include "JoystickDevice.h"
#include "DebugTrace.h"

/****************************************************************************/

JoystickDevice::JoystickDevice()
{
}

/****************************************************************************/

JoystickDevice::~JoystickDevice()
{
	Close();
}

/****************************************************************************/
JoystickDevice::JoystickDevice(JoystickDevice&& r)
{
	m_GUIDInstance = r.m_GUIDInstance;
	m_Id = r.m_Id;
	std::swap(m_Name, r.m_Name);
	std::swap(m_pDevice, r.m_pDevice);
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
JoystickDevice& JoystickDevice::operator=(JoystickDevice&& r)
{
	m_GUIDInstance = r.m_GUIDInstance;
	m_Id = r.m_Id;
	std::swap(m_Name, r.m_Name);
	std::swap(m_pDevice, r.m_pDevice);
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
HRESULT JoystickDevice::Open(HWND hWnd, LPDIRECTINPUT8 pDirectInput)
{
	HRESULT hr;

	// Do nothing if device already open
	if (m_pDevice != nullptr)
		return S_OK;

	if (FAILED(hr = pDirectInput->CreateDevice(m_GUIDInstance, &m_pDevice, nullptr)))
		return hr;

	if (FAILED(hr = m_pDevice->SetDataFormat(&c_dfDIJoystick2)))
	{
		Close();
		return hr;
	}

	if (FAILED(hr = m_pDevice->SetCooperativeLevel(hWnd, DISCL_NONEXCLUSIVE | DISCL_BACKGROUND)))
	{
		Close();
		return hr;
	}

	if (FAILED(hr = m_pDevice->EnumObjects(EnumObjectsCallback, this, DIDFT_ALL)))
	{
		Close();
		return hr;
	}

	m_Captured = true;

	return S_OK;
}

/****************************************************************************/
void JoystickDevice::Close()
{
	m_Captured = false;

	if (m_pDevice != nullptr)
	{
		m_pDevice->Unacquire();
		m_pDevice->Release();
		m_pDevice = nullptr;
	}
}

/*****************************************************************************/
std::string JoystickDevice::DisplayString() const
{
	if (!m_Configured)
		return std::string{};

	std::string name = m_Name;
	if (m_Instance != 1)
		name += " #" + std::to_string(m_Instance);
	return name;
}

/*****************************************************************************/
bool JoystickDevice::Update()
{
	if (!m_Present)
		return false;

	HRESULT hr = m_pDevice->Poll();

	if (FAILED(hr))
	{
		DebugTrace("Poll result %08x\n", hr);

		hr = m_pDevice->Acquire();
		DebugTrace("Acquire result %08x\n", hr);

		if (hr == DIERR_UNPLUGGED)
			return false;

		return true;
	}

	if (FAILED(hr = m_pDevice->GetDeviceState(sizeof(DIJOYSTATE2), &m_JoyState)))
	{
		DebugTrace("GeteviceState result %08x\n", hr);
		return false;
	}

	return true;
}

/*****************************************************************************/
DWORD JoystickDevice::GetButtons() const
{
	DWORD buttons = 0;

	for (int i = 0; i < JOYSTICK_MAX_BUTTONS && i < _countof(DIJOYSTATE2::rgbButtons); ++i)
	{
		if (m_JoyState.rgbButtons[i] & 0x80)
			buttons |= (1 << i);
	}

	return buttons;
}

static constexpr unsigned int setbit(int bit) { return 1u << bit; }
constexpr DWORD xboxAxes = setbit(JOYSTICK_AXIS_RX_P) | setbit(JOYSTICK_AXIS_RY_P);
constexpr DWORD stdpadAxes = setbit(JOYSTICK_AXIS_Z_P) | setbit(JOYSTICK_AXIS_RZ_P);

DWORD JoystickDevice::GetAxesState(int threshold) const
{
	unsigned int axes = 0;

	auto Detect = [&axes, threshold](const int val, const int nbit, const int pbit) {
		if (val < 32767 - threshold)
			axes |= (1 << nbit);
		else if (val > 32767 + threshold)
			axes |= (1 << pbit);
	};

	Detect(m_JoyState.lX, JOYSTICK_AXIS_LEFT, JOYSTICK_AXIS_RIGHT);
	Detect(m_JoyState.lY, JOYSTICK_AXIS_UP, JOYSTICK_AXIS_DOWN);

	// If axes RX and RY are not present, but Z and RZ are,
	// map Z and RZ to RX and RY
	if ((m_PresentAxes & xboxAxes) == 0)
	{
		if (m_PresentAxes & setbit(JOYSTICK_AXIS_Z_P))
			Detect(m_JoyState.lZ, JOYSTICK_AXIS_RX_N, JOYSTICK_AXIS_RX_P);
		if (m_PresentAxes & setbit(JOYSTICK_AXIS_RZ_P))
			Detect(m_JoyState.lZ, JOYSTICK_AXIS_RY_N, JOYSTICK_AXIS_RY_P);
	}
	else
	{
		if (m_PresentAxes & setbit(JOYSTICK_AXIS_Z_P))
			Detect(m_JoyState.lZ, JOYSTICK_AXIS_Z_N, JOYSTICK_AXIS_Z_P);
		if (m_PresentAxes & setbit(JOYSTICK_AXIS_RX_P))
			Detect(m_JoyState.lRx, JOYSTICK_AXIS_RX_N, JOYSTICK_AXIS_RX_P);
		if (m_PresentAxes & setbit(JOYSTICK_AXIS_RY_P))
			Detect(m_JoyState.lRy, JOYSTICK_AXIS_RY_N, JOYSTICK_AXIS_RY_P);
		if (m_PresentAxes & setbit(JOYSTICK_AXIS_RZ_P))
			Detect(m_JoyState.lRz, JOYSTICK_AXIS_RZ_N, JOYSTICK_AXIS_RZ_P);
	}

	if (m_PresentAxes & setbit(JOYSTICK_AXIS_HAT_UP))
	{
		DWORD pov = m_JoyState.rgdwPOV[0];
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

/*****************************************************************************/
void JoystickDevice::GetAxesValue(int axesSet, int& x, int& y) const
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

/****************************************************************************/
void JoystickDevice::EnumObjectsCallback(const DIDEVICEOBJECTINSTANCE* pDeviceObjectInstance)
{
	DebugTrace("JoystickDevice::EnumObjectsCallback: %s\n", pDeviceObjectInstance->tszName);

	// For axes that are returned, set the DIPROP_RANGE property for the
	// enumerated axis in order to scale min/max values.
	if (pDeviceObjectInstance->dwType & DIDFT_AXIS)
	{
		DIPROPRANGE diprg{};
		diprg.diph.dwSize = sizeof(DIPROPRANGE);
		diprg.diph.dwHeaderSize = sizeof(DIPROPHEADER);
		diprg.diph.dwHow = DIPH_BYID;
		diprg.diph.dwObj = pDeviceObjectInstance->dwType; // Specify the enumerated axis
		diprg.lMin = 0;
		diprg.lMax = 65535;

		// Set the range for the axis
		if (FAILED(m_pDevice->SetProperty(DIPROP_RANGE, &diprg.diph)))
			return;
	}

	if (pDeviceObjectInstance->dwType & DIDFT_BUTTON)
	{
		WORD instance = DIDFT_GETINSTANCE(pDeviceObjectInstance->dwType);
		if (instance < JOYSTICK_MAX_BUTTONS)
			m_PresentButtons |= setbit(instance);
	}

	if (pDeviceObjectInstance->guidType == GUID_YAxis)
		m_PresentAxes |= setbit(JOYSTICK_AXIS_UP) | setbit(JOYSTICK_AXIS_DOWN);
	else if (pDeviceObjectInstance->guidType == GUID_XAxis)
		m_PresentAxes |= setbit(JOYSTICK_AXIS_LEFT) | setbit(JOYSTICK_AXIS_RIGHT);
	else if (pDeviceObjectInstance->guidType == GUID_ZAxis)
		m_PresentAxes |= setbit(JOYSTICK_AXIS_Z_N) | setbit(JOYSTICK_AXIS_Z_P);
	else if (pDeviceObjectInstance->guidType == GUID_RxAxis)
		m_PresentAxes |= setbit(JOYSTICK_AXIS_RX_N) | setbit(JOYSTICK_AXIS_RX_P);
	else if (pDeviceObjectInstance->guidType == GUID_RyAxis)
		m_PresentAxes |= setbit(JOYSTICK_AXIS_RY_N) | setbit(JOYSTICK_AXIS_RY_P);
	else if (pDeviceObjectInstance->guidType == GUID_RzAxis)
		m_PresentAxes |= setbit(JOYSTICK_AXIS_RZ_N) | setbit(JOYSTICK_AXIS_RZ_P);
	else if (pDeviceObjectInstance->guidType == GUID_POV)
		m_PresentAxes |= setbit(JOYSTICK_AXIS_HAT_LEFT) | setbit(JOYSTICK_AXIS_HAT_RIGHT) |
		                 setbit(JOYSTICK_AXIS_HAT_UP)   | setbit(JOYSTICK_AXIS_HAT_DOWN);
}

/****************************************************************************/
BOOL CALLBACK JoystickDevice::EnumObjectsCallback(
	const DIDEVICEOBJECTINSTANCE* pDeviceObjectInstance,
	VOID* pContext)
{
	JoystickDevice* pJoystickDevice = reinterpret_cast<JoystickDevice*>(pContext);

	pJoystickDevice->EnumObjectsCallback(pDeviceObjectInstance);

	return DIENUM_CONTINUE;
}