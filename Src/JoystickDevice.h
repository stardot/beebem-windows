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

#ifndef JOYSTICK_DEVICE_HEADER
#define JOYSTICK_DEVICE_HEADER

#define DIRECTINPUT_VERSION 0x0800

#include <InitGuid.h>
#include <dinput.h>
#include <dinputd.h>

#include <string>
#include <utility>

struct JoystickId : std::pair<int, int>
{
	using std::pair<int, int>::pair;

	// Manufacturer ID aka Vendor ID
	int& mId() { return first; }
	// Product ID
	int& pId() { return second; }
};

class JoystickDevice
{
	public:
		JoystickDevice();
		JoystickDevice(const JoystickDevice&) = delete;
		JoystickDevice(JoystickDevice&& r);
		JoystickDevice& operator=(const JoystickDevice&) = delete;
		JoystickDevice& operator=(JoystickDevice&& r);
		~JoystickDevice();

	public:
		HRESULT Open(HWND hWnd, LPDIRECTINPUT8 pDirectInput);
		void Close();

		bool IsCaptured() const { return m_Captured; }
		JoystickId Id() const { return m_Id; }
		std::string DisplayString() const;
		bool Update();
		DWORD GetButtons() const;
		DWORD GetAxesState(int threshold) const;
		void GetAxesValue(int axesSet, int& x, int& y) const;

	private:
		void EnumObjectsCallback(const DIDEVICEOBJECTINSTANCE* pDeviceObjectInstance);

		static BOOL CALLBACK EnumObjectsCallback(
			const DIDEVICEOBJECTINSTANCE* pDeviceObjectInstance,
			VOID* pContext
		);

	public:
		GUID         m_GUIDInstance{};
		JoystickId   m_Id{ 0, 0 };
		std::string  m_Name;
		LPDIRECTINPUTDEVICE8 m_pDevice{ nullptr };
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
};

#endif
