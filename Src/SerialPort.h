/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2024  Chris Needham

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

#ifndef SERIAL_PORT_HEADER
#define SERIAL_PORT_HEADER

#include "RingBuffer.h"

class SerialPort
{
	public:
		SerialPort();
		SerialPort(const SerialPort&) = delete;
		SerialPort& operator=(const SerialPort&) = delete;
		~SerialPort();

	public:
		bool Init(const char* PortName);
		void Close();

		bool SetBaudRate(int BaudRate);
		bool Configure(unsigned char DataBits, unsigned char StopBits, unsigned char Parity);
		bool SetRTS(bool RTS);

		bool WriteChar(unsigned char Data);

		bool HasRxData() const;
		unsigned char GetRxData();

		bool HasStatus() const;
		unsigned char GetStatus();

	private:
		bool InitThread();

		static unsigned int __stdcall ReadThreadFunc(void* pParameter);
		void ReadThreadFunc();

		void PutRxData(const unsigned char* pData, int Length);
		void PutCommStatus(unsigned char Status);

	private:
		HANDLE m_hSerialPort;
		HANDLE m_hReadThread;
		HANDLE m_hReadStartUpEvent;
		HANDLE m_hReadShutDownEvent;
		DWORD m_dwCommEvent;
		OVERLAPPED m_OverlappedRead;
		OVERLAPPED m_OverlappedWrite;
		RingBuffer m_RxBuffer;
		RingBuffer m_StatusBuffer;
		CRITICAL_SECTION m_BufferLock;
};

#endif
