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

#ifndef JOYSTICKHANDLER_H
#define JOYSTICKHANDLER_H

#define DIRECTINPUT_VERSION 0x0800

#include <vector>

#include "beebwin.h"

#include <InitGuid.h>
#include <dinput.h>
#include <dinputd.h>

/* Max number of entries on joystick order list */
#define MAX_JOYSTICK_ORDER      16

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
	GUID         m_GUIDInstance{};
	JoystickId   m_Id{ 0, 0 };
	std::string  m_Name;
	LPDIRECTINPUTDEVICE8 m_Device{ nullptr };
	DWORD        m_PresentAxes{ 0 };
	DWORD        m_PresentButtons{ 0 };
	DIJOYSTATE2  m_JoyState;

	int          m_Instance{ 0 };
	bool         m_OnOrderList{ false };
	int          m_JoyIndex{ -1 };
	bool         m_Configured{ false };
	bool         m_Present{ false };
	bool         m_Captured{ false };
	unsigned int m_PrevAxes{ 0 };
	unsigned int m_PrevBtns{ 0 };
	bool         m_JoystickToKeysActive{ false };

	JoystickId   Id() { return m_Id; }
	std::string  DisplayString();
	bool         Update();
	DWORD        GetButtons();
	DWORD        GetAxesState(int threshold);
	void         GetAxesValue(int axesSet, int& x, int& y);
	void         EnumObjectsCallback(const DIDEVICEOBJECTINSTANCE* pdidoi);
	void         CloseDevice();

	JoystickDev() {}
	JoystickDev(const JoystickDev&) = delete;
	JoystickDev(JoystickDev&& r);
	JoystickDev& operator=(const JoystickDev&) = delete;
	~JoystickDev();
};

struct JoystickHandler
{
	bool              m_DirectInputInitialized{ false };
	HRESULT           m_DirectInputInitResult{ E_FAIL };
	LPDIRECTINPUT8    m_pDirectInput{ nullptr };
	DIJOYCONFIG       m_PreferredJoyCfg{};
	bool              m_HavePreferredJoyCfg{ false };

	std::vector<JoystickDev>        m_JoystickDevs;
	std::vector<JoystickOrderEntry> m_JoystickOrder;

	HRESULT      InitDirectInput(void);
	void         AddDeviceInstance(const DIDEVICEINSTANCE*);
	HRESULT      ScanJoysticks(void);
	HRESULT      OpenDevice(HWND mainWindow, JoystickDev* dev);
	JoystickDev* GetDev(int pcIdx)
	{
		return (pcIdx >= 0 && static_cast<unsigned int>(pcIdx) < m_JoystickDevs.size()) ? &m_JoystickDevs[pcIdx] : nullptr;
	}
	bool         IsCaptured(int pcIdx)
	{
		JoystickDev* dev = GetDev(pcIdx);
		return dev && dev->m_Captured;
	}

	JoystickHandler() {}
	~JoystickHandler();
};

#endif
