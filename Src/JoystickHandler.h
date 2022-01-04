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

#ifndef JOYSTICK_HANDLER_HEADER
#define JOYSTICK_HANDLER_HEADER

#define DIRECTINPUT_VERSION 0x0800

#include "beebwin.h"
#include "JoystickDevice.h"

#include <dinput.h>
#include <dinputd.h>

#include <vector>

// Max number of entries on joystick order list
constexpr int MAX_JOYSTICK_ORDER = 16;

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

class JoystickHandler
{
	public:
		JoystickHandler();
		~JoystickHandler();

		JoystickHandler(const JoystickHandler&) = delete;
		JoystickHandler& operator=(const JoystickHandler&) = delete;

	public:
		HRESULT Init();
		HRESULT ScanJoysticks();
		HRESULT OpenDevice(HWND hWnd, JoystickDevice* pJoystickDevice);

		JoystickDevice* GetDev(int pcIdx)
		{
			return (pcIdx >= 0 && static_cast<unsigned int>(pcIdx) < m_JoystickDevs.size()) ? &m_JoystickDevs[pcIdx] : nullptr;
		}

		size_t GetJoystickCount() const;

	private:
		void AddDeviceInstance(const DIDEVICEINSTANCE*);

		static BOOL CALLBACK EnumJoysticksCallback(const DIDEVICEINSTANCE* pDeviceInstance, VOID* pContext);

	public:
		LPDIRECTINPUT8 m_pDirectInput{ nullptr };
		DIJOYCONFIG m_PreferredJoyCfg{};
		bool m_HavePreferredJoyCfg{ false };

		std::vector<JoystickDevice> m_JoystickDevs;
		std::vector<JoystickOrderEntry> m_JoystickOrder;
};

#endif
