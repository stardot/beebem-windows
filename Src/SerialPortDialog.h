/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2023 Chris Needham

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

#ifndef SERIAL_PORT_DIALOG_HEADER
#define SERIAL_PORT_DIALOG_HEADER

#include <string>

#include "Dialog.h"
#include "Serial.h"

class SerialPortDialog : public Dialog
{
	public:
		SerialPortDialog(
			HINSTANCE hInstance,
			HWND hwndParent,
			SerialType Destination,
			const char* PortName,
			const char* IPAddress,
			int IPPort,
			bool IP232RawComms,
			bool IP232Handshake
		);

	public:
		SerialType GetDestination() const { return m_Destination; }
		const std::string& GetSerialPortName() const { return m_SerialPortName; }
		const std::string& GetIPAddress() const { return m_IPAddress; }
		int GetIPPort() const { return m_IPPort; }
		bool GetIP232RawComms() const { return m_IP232RawComms; }
		bool GetIP232Handshake() const { return m_IP232Handshake; }

	private:
		virtual INT_PTR DlgProc(
			UINT   nMessage,
			WPARAM wParam,
			LPARAM lParam
		);

		void EnableSerialPortControls(bool bEnable);
		void EnableIP232Controls(bool bEnable);

	private:
		SerialType m_Destination;
		std::string m_SerialPortName;
		std::string m_IPAddress;
		int m_IPPort;
		bool m_IP232RawComms;
		bool m_IP232Handshake;
};

#endif
