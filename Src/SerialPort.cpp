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

#include <windows.h>

#include <process.h>
#include <stdio.h>

#include "SerialPort.h"
#include "DebugTrace.h"

/*--------------------------------------------------------------------------*/

SerialPort::SerialPort() :
	m_hSerialPort(INVALID_HANDLE_VALUE),
	m_hReadThread(nullptr),
	m_hReadStartUpEvent(nullptr),
	m_hReadShutDownEvent(nullptr),
	m_dwCommEvent(0),
	m_RxBuffer(1024),
	m_StatusBuffer(128)
{
	ZeroMemory(&m_OverlappedRead, sizeof(m_OverlappedRead));
	ZeroMemory(&m_OverlappedWrite, sizeof(m_OverlappedWrite));
	ZeroMemory(&m_BufferLock, sizeof(m_BufferLock));
}

/*--------------------------------------------------------------------------*/

SerialPort::~SerialPort()
{
	Close();
}

/*--------------------------------------------------------------------------*/

bool SerialPort::Init(const char* PortName)
{
	char FileName[MAX_PATH];
	sprintf(FileName, "\\\\.\\%s", PortName);

	COMMTIMEOUTS CommTimeOuts{};
	DCB dcb{}; // Serial port device control block

	m_hSerialPort = CreateFile(FileName,
	                           GENERIC_READ | GENERIC_WRITE,
	                           0, // dwShareMode
	                           nullptr, // lpSecurityAttributes
	                           OPEN_EXISTING,
	                           FILE_FLAG_OVERLAPPED,
	                           nullptr); // hTemplateFile

	if (m_hSerialPort == INVALID_HANDLE_VALUE)
	{
		goto Fail;
	}

	if (!SetupComm(m_hSerialPort, 1024, 1024))
	{
		goto Fail;
	}

	dcb.DCBlength = sizeof(dcb);

	if (!GetCommState(m_hSerialPort, &dcb))
	{
		goto Fail;
	}

	dcb.fBinary         = TRUE;
	dcb.BaudRate        = 9600;
	dcb.Parity          = NOPARITY;
	dcb.StopBits        = ONESTOPBIT;
	dcb.ByteSize        = 8;
	dcb.fDtrControl     = DTR_CONTROL_DISABLE;
	dcb.fOutxCtsFlow    = FALSE;
	dcb.fOutxDsrFlow    = FALSE;
	dcb.fOutX           = FALSE;
	dcb.fDsrSensitivity = FALSE;
	dcb.fInX            = FALSE;
	dcb.fRtsControl     = RTS_CONTROL_DISABLE; // Leave it low (do not send) for now

	if (!SetCommState(m_hSerialPort, &dcb))
	{
		goto Fail;
	}

	CommTimeOuts.ReadIntervalTimeout         = MAXDWORD;
	CommTimeOuts.ReadTotalTimeoutConstant    = 0;
	CommTimeOuts.ReadTotalTimeoutMultiplier  = 0;
	CommTimeOuts.WriteTotalTimeoutConstant   = 0;
	CommTimeOuts.WriteTotalTimeoutMultiplier = 0;

	if (!SetCommTimeouts(m_hSerialPort, &CommTimeOuts))
	{
		goto Fail;
	}

	// Configure the conditions for WaitCommEvent() to signal an event.
	if (!SetCommMask(m_hSerialPort, EV_CTS | EV_RXCHAR | EV_ERR))
	{
		goto Fail;
	}

	return InitThread();

Fail:
	Close();

	return false;
}

/*--------------------------------------------------------------------------*/

// Create the thread to wait on the I/O event object. Wait for the thread
// to start up before returning.

bool SerialPort::InitThread()
{
	InitializeCriticalSection(&m_BufferLock);

	ZeroMemory(&m_OverlappedRead, sizeof(m_OverlappedRead));
	ZeroMemory(&m_OverlappedWrite, sizeof(m_OverlappedWrite));

	m_hReadStartUpEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

	if (m_hReadStartUpEvent == nullptr)
	{
		return false;
	}

	m_hReadShutDownEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

	if (m_hReadShutDownEvent == nullptr)
	{
		return false;
	}

	m_OverlappedRead.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

	if (m_OverlappedRead.hEvent == nullptr)
	{
		return false;
	}

	// Create the thread.

	unsigned int ThreadID = 0;

	m_hReadThread = (HANDLE)_beginthreadex(
		nullptr,        // security
		0,              // stack_size
		ReadThreadFunc, // start_address
		this,           // arglist
		0,              // initflag
		&ThreadID       // thrdaddr
	);

	if (m_hReadThread == nullptr)
	{
		return false;
	}

	DWORD dwResult = WaitForSingleObject(m_hReadStartUpEvent, INFINITE);

	if (dwResult != WAIT_OBJECT_0)
	{
		return false;
	}

	// Wait for a comm event.

	m_dwCommEvent = 0;

	if (!WaitCommEvent(m_hSerialPort, &m_dwCommEvent, &m_OverlappedRead))
	{
		if (GetLastError() != ERROR_IO_PENDING)
		{
			DebugTrace("Failed to initiate port event wait\n");
			return false;
		}
	}

	m_RxBuffer.Reset();
	m_StatusBuffer.Reset();

	return true;
}

/*--------------------------------------------------------------------------*/

void SerialPort::Close()
{
	if (m_hReadThread != nullptr)
	{
		SetEvent(m_hReadShutDownEvent);

		WaitForSingleObject(m_hReadThread, INFINITE);

		CloseHandle(m_hReadThread);
		m_hReadThread = nullptr;
	}

	if (m_hSerialPort != INVALID_HANDLE_VALUE)
	{
		CloseHandle(m_hSerialPort);
		m_hSerialPort = INVALID_HANDLE_VALUE;
	}

	if (m_hReadStartUpEvent != nullptr)
	{
		CloseHandle(m_hReadStartUpEvent);
		m_hReadStartUpEvent = nullptr;
	}

	if (m_hReadShutDownEvent != nullptr)
	{
		CloseHandle(m_hReadShutDownEvent);
		m_hReadShutDownEvent = nullptr;
	}

	if (m_OverlappedRead.hEvent != nullptr)
	{
		CloseHandle(m_OverlappedRead.hEvent);
		m_OverlappedRead.hEvent = nullptr;
	}

	DeleteCriticalSection(&m_BufferLock);

	m_RxBuffer.Reset();
	m_StatusBuffer.Reset();
}

/*--------------------------------------------------------------------------*/

bool SerialPort::SetBaudRate(int BaudRate)
{
	DebugTrace("SerialPort::SetBaudRate\n");

	if (m_hSerialPort == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	DCB dcb{}; // Serial port device control block
	dcb.DCBlength = sizeof(dcb);

	if (!GetCommState(m_hSerialPort, &dcb))
	{
		return false;
	}

	dcb.BaudRate  = BaudRate;
	dcb.DCBlength = sizeof(dcb);

	if (!SetCommState(m_hSerialPort, &dcb))
	{
		return false;
	}

	return true;
}

/*--------------------------------------------------------------------------*/

bool SerialPort::Configure(unsigned char DataBits,
                           unsigned char StopBits,
                           unsigned char Parity)
{
	DebugTrace("SerialPort::Configure DataBits=%d StopBits=%d Parity=%d\n", DataBits, StopBits, Parity);

	if (m_hSerialPort == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	DCB dcb{}; // Serial port device control block
	dcb.DCBlength = sizeof(dcb);

	if (!GetCommState(m_hSerialPort, &dcb))
	{
		return false;
	}

	dcb.ByteSize  = DataBits;
	dcb.StopBits  = (StopBits == 1) ? ONESTOPBIT : TWOSTOPBITS;
	dcb.Parity    = Parity;
	dcb.DCBlength = sizeof(dcb);

	if (!SetCommState(m_hSerialPort, &dcb))
	{
		return false;
	}

	return true;
}

/*--------------------------------------------------------------------------*/

bool SerialPort::SetRTS(bool RTS)
{
	DebugTrace("SerialPort::SetRTS\n");

	if (m_hSerialPort == INVALID_HANDLE_VALUE)
	{
		return false;
	}

	if (!EscapeCommFunction(m_hSerialPort, RTS ? CLRRTS : SETRTS))
	{
		return false;
	}

	return true;
}

/*--------------------------------------------------------------------------*/

bool SerialPort::WriteChar(unsigned char Data)
{
	DWORD BytesSent = 0;

	bool Success = !!WriteFile(m_hSerialPort,
	                           &Data,
	                           1,
	                           &BytesSent,
	                           &m_OverlappedWrite);

	if (!Success)
	{
		DWORD Error = GetLastError();

		if (Error == ERROR_IO_PENDING)
		{
			DWORD WaitResult = WaitForSingleObject(m_OverlappedWrite.hEvent, INFINITE);

			Success = WaitResult == WAIT_OBJECT_0;
		}
		else
		{
			DebugTrace("WriteFile failed, code %lu", (ULONG)Error);
		}
	}

	return Success;
}

/*--------------------------------------------------------------------------*/

bool SerialPort::HasRxData() const
{
	return m_RxBuffer.HasData();
}

/*--------------------------------------------------------------------------*/

unsigned char SerialPort::GetRxData()
{
	EnterCriticalSection(&m_BufferLock);

	unsigned char Data = m_RxBuffer.GetData();

	LeaveCriticalSection(&m_BufferLock);

	return Data;
}

/*--------------------------------------------------------------------------*/

bool SerialPort::HasStatus() const
{
	return m_StatusBuffer.HasData();
}

/*--------------------------------------------------------------------------*/

unsigned char SerialPort::GetStatus()
{
	EnterCriticalSection(&m_BufferLock);

	unsigned char Data = m_StatusBuffer.GetData();

	LeaveCriticalSection(&m_BufferLock);

	return Data;
}

/*--------------------------------------------------------------------------*/

unsigned int __stdcall SerialPort::ReadThreadFunc(void* pParameter)
{
	SerialPort* pSerialPort = static_cast<SerialPort*>(pParameter);

	if (pSerialPort != nullptr)
	{
		pSerialPort->ReadThreadFunc();
	}

	return 0;
}

/*--------------------------------------------------------------------------*/

void SerialPort::ReadThreadFunc()
{
	// Notify the parent thread that this thread has started running.
	SetEvent(m_hReadStartUpEvent);

	DebugTrace("ReadThreadFunc started\n");

	bool bQuit = false;

	while (!bQuit)
	{
		HANDLE hEvents[] = { m_OverlappedRead.hEvent, m_hReadShutDownEvent };

		// Wait for one of the event objects to become signalled.
		DWORD Result = WaitForMultipleObjects(_countof(hEvents), hEvents, FALSE, INFINITE);

		switch (Result)
		{
			case WAIT_OBJECT_0 + 0:
				// Clear comms port line errors, if any.
				if (m_dwCommEvent & EV_ERR)
				{
					DWORD Error = 0;

					if (ClearCommError(m_hSerialPort, &Error, nullptr))
					{
						DebugTrace("Comms error %lu (framing, overrun, parity)\n", Error);
					}
				}

				// Process received characters, if any.
				if (m_dwCommEvent & EV_RXCHAR)
				{
					const int BUFFER_LENGTH = 256;
					unsigned char Buffer[BUFFER_LENGTH];

					int Pos = 0;

					for (;;)
					{
						unsigned char Data;
						DWORD BytesRead = 0;

						OVERLAPPED Overlapped{};

						BOOL Success = ReadFile(m_hSerialPort, // hFile
						                        &Data,         // lpBuffer
						                        1,             // nNumberOfBytesToRead
						                        &BytesRead,    // lpNumberOfBytesRead
						                        &Overlapped);  // lpOverlapped

						if (Success && BytesRead == 1)
						{
							Buffer[Pos] = Data;

							if (++Pos == BUFFER_LENGTH)
							{
								PutRxData(Buffer, BUFFER_LENGTH);
								Pos = 0;
							}
						}
						else
						{
							break;
						}
					}

					if (Pos > 0)
					{
						PutRxData(Buffer, Pos);
					}
				}

				if (m_dwCommEvent & EV_CTS)
				{
					DWORD ModemStatus = 0;

					if (GetCommModemStatus(m_hSerialPort, &ModemStatus))
					{
						PutCommStatus((unsigned char)ModemStatus);
					}
				}

				// Prepare for the next comms event.
				ResetEvent(m_OverlappedRead.hEvent);
				WaitCommEvent(m_hSerialPort, &m_dwCommEvent, &m_OverlappedRead);
				break;

			case WAIT_OBJECT_0 + 1:
				// The thread has been signalled to terminate.
				DebugTrace("Overlapped I/O read thread shutdown event signalled\n");

				bQuit = true;
				break;

			default:
				// Unexpected return code - terminate the thread.
				Result = GetLastError();
				DebugTrace("Overlapped I/O thread WaitForMultipleObjects failed, code %lu\n", (ULONG)Result);

				bQuit = true;
				break;
		}
	}

	DebugTrace("ReadThreadFunc stopped\n");
}

/*--------------------------------------------------------------------------*/

void SerialPort::PutRxData(const unsigned char* pData, int Length)
{
	EnterCriticalSection(&m_BufferLock);

	for (int i = 0; i < Length; i++)
	{
		m_RxBuffer.PutData(pData[i]);
	}

	LeaveCriticalSection(&m_BufferLock);
}

/*--------------------------------------------------------------------------*/

void SerialPort::PutCommStatus(unsigned char Status)
{
	EnterCriticalSection(&m_BufferLock);

	m_StatusBuffer.PutData(Status);

	LeaveCriticalSection(&m_BufferLock);
}

/*--------------------------------------------------------------------------*/
