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
#include "DebugTrace.h"
#include "FileUtils.h"
#include "KeyNames.h"
#include "StringUtils.h"

#include <cctype>
#include <list>
#include <vector>
#include <algorithm>

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

JoystickHandler::JoystickHandler()
{
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
HRESULT JoystickHandler::Init()
{
	if (m_pDirectInput != nullptr)
	{
		// Already initialised
		return S_OK;
	}

	HRESULT hResult = DirectInput8Create(
		hInst,
		DIRECTINPUT_VERSION,
		IID_IDirectInput8,
		(VOID**)&m_pDirectInput,
		nullptr
	);

	if (FAILED(hResult))
	{
		return hResult;
	}

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
void JoystickHandler::AddDeviceInstance(const DIDEVICEINSTANCE* pDeviceInstance)
{
	int mid = LOWORD(pDeviceInstance->guidProduct.Data1);
	int pid = HIWORD(pDeviceInstance->guidProduct.Data1);

	int index = m_JoystickDevs.size();

	JoystickDevice dev;
	dev.m_GUIDInstance = pDeviceInstance->guidInstance;
	dev.m_Id = JoystickId(mid, pid);
	dev.m_Name = pDeviceInstance->tszInstanceName;
	dev.m_Configured = true;
	dev.m_Present = true;
	dev.m_JoyIndex = index;

	m_JoystickDevs.emplace_back(std::move(dev));
}

/****************************************************************************/

BOOL CALLBACK JoystickHandler::EnumJoysticksCallback(const DIDEVICEINSTANCE* pDeviceInstance, VOID* pContext)
{
	JoystickHandler* pJoystickHandler = reinterpret_cast<JoystickHandler*>(pContext);

	pJoystickHandler ->AddDeviceInstance(pDeviceInstance);

	return DIENUM_CONTINUE;
}

/****************************************************************************/
HRESULT JoystickHandler::ScanJoysticks()
{
	bool usePreferredCfg = m_HavePreferredJoyCfg;

	std::map<JoystickId, std::list<JoystickDevice*>> listById;

	// Clear joystick devs vector - closes any open devices
	m_JoystickDevs.clear();

	if (m_pDirectInput == nullptr)
		return DIERR_NOTINITIALIZED;

	HRESULT hr = m_pDirectInput->EnumDevices(
		DI8DEVCLASS_GAMECTRL,
		EnumJoysticksCallback,
		this,
		DIEDFL_ATTACHEDONLY
	);

	if (FAILED(hr))
		return hr;

	for (JoystickDevice& dev : m_JoystickDevs)
	{
		listById[dev.Id()].push_back(&dev);
		dev.m_Instance = listById[dev.Id()].size();
	}

	std::vector<JoystickDevice> sorted;
	// Start with joysticks in the order list
	for (JoystickOrderEntry& entry : m_JoystickOrder)
	{
		std::list<JoystickDevice*>& list = listById[entry];
		if (!list.empty())
		{
			JoystickDevice& dev = *(list.front());
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
		for (JoystickDevice& dev : m_JoystickDevs)
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
	for (JoystickDevice& dev : m_JoystickDevs)
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
	for (JoystickDevice& dev : m_JoystickDevs)
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
HRESULT JoystickHandler::OpenDevice(HWND hWnd, JoystickDevice* pJoystickDevice)
{
	return pJoystickDevice->Open(hWnd, m_pDirectInput);
}

/****************************************************************************/
size_t JoystickHandler::GetJoystickCount() const
{
	return m_JoystickDevs.size();
}
