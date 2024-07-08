/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2006  Jon Welch
Copyright (C) 2009  Rob O'Donnell

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

/*
 Remote serial port & IP232 support by Rob O'Donnell Mar 2009.

 . Raw mode connects and disconnects on RTS going up and down.
 . CTS reflects connection status.
 . IP232 mode maintains constant connection. CTS reflects DTR
 . on the modem; you would normally tie this to DCD depending
 . on your application.

 Much taken from Mac version by Jon Welch
 */

#include <windows.h>

#include <assert.h>
#include <stdio.h>

#include "IP232.h"
#include "6502core.h"
#include "BeebWin.h"
#include "Debug.h"
#include "DebugTrace.h"
#include "Main.h"
#include "Messages.h"
#include "RingBuffer.h"
#include "Serial.h"
#include "Thread.h"

constexpr int IP232_CONNECTION_DELAY = 8192; // Cycles to wait after connection

class EthernetPortReadThread : public Thread
{
	public:
		virtual unsigned int ThreadFunc();
};

static CRITICAL_SECTION SharedBufferLock; // MM 26/11/21

// IP232
static SOCKET EthernetSocket = INVALID_SOCKET; // Listen socket

static EthernetPortReadThread EthernetReadThread;

// This bit is the Serial Port stuff
bool IP232Handshake; // If true, RTS set by the emulated serial port
bool IP232Raw; // If true, handling of DTR low/high messages is disabled (255 followed by 0 or 1)
char IP232Address[256];
int IP232Port;

static bool IP232FlagReceived = false;

// IP232 routines use InputBuffer as data coming in from the modem,
// and OutputBuffer for data to be sent out to the modem.
// StatusBuffer is used for changes to the serial ACIA status
// registers
static RingBuffer InputBuffer(1024);
static RingBuffer OutputBuffer(1024);
static RingBuffer StatusBuffer(128);

CycleCountT IP232RxTrigger = CycleCountTMax;

static void EthernetReceivedData(unsigned char* pData, int Length);
static void EthernetPortStoreStatus(unsigned char Data);
static void EthernetPortStore(unsigned char Data);
static void DebugReceivedData(unsigned char* pData, int Length);

/*--------------------------------------------------------------------------*/

bool IP232Open()
{
	DebugTrace("IP232Open\n");

	// Added this to prevent WSAECONNABORTED errors from recv()
	// in EthernetPortReadThread on reconnecting after showing
	// the serial port config dialog box.
	Sleep(50);

	InitializeCriticalSection(&SharedBufferLock); // MM 26/11/21

	InputBuffer.Reset();
	OutputBuffer.Reset();
	StatusBuffer.Reset();

	// Let's prepare some IP sockets

	assert(EthernetSocket == INVALID_SOCKET);

	EthernetSocket = socket(AF_INET, SOCK_STREAM, 0);

	if (EthernetSocket == INVALID_SOCKET)
	{
		DebugTrace("Unable to create IP232 socket\n");

		if (DebugEnabled)
			DebugDisplayTrace(DebugType::RemoteServer, true, "IP232: Unable to create socket");

		mainWin->Report(MessageType::Error, "Unable to create IP232 socket");

		return false; // Couldn't create the socket
	}

	if (DebugEnabled)
		DebugDisplayTrace(DebugType::RemoteServer, true, "IP232: socket created");

	SetTrigger(IP232_CONNECTION_DELAY, IP232RxTrigger);

	DebugTrace("Connecting to IP232 server %s port %d\n", IP232Address, IP232Port);

	sockaddr_in Addr;
	Addr.sin_family = AF_INET; // address family Internet
	Addr.sin_port = htons(static_cast<u_short>(IP232Port)); // Port to connect on
	Addr.sin_addr.s_addr = inet_addr(IP232Address); // Target IP

	if (connect(EthernetSocket, (SOCKADDR *)&Addr, sizeof(Addr)) == SOCKET_ERROR)
	{
		DebugTrace("Unable to connect to IP232 server %s port %d\n", IP232Address, IP232Port);

		if (DebugEnabled)
		{
			DebugDisplayTraceF(DebugType::RemoteServer, true,
			                   "IP232: Unable to connect to server  %s port %d", IP232Address, IP232Port);
		}

		IP232Close();

		mainWin->Report(MessageType::Error, "Unable to connect to server %s port %d", IP232Address, IP232Port);

		return false; // Couldn't connect
	}

	if (DebugEnabled)
		DebugDisplayTrace(DebugType::RemoteServer, true, "IP232: connected to server");

	if (DebugEnabled)
		DebugDisplayTrace(DebugType::RemoteServer, true, "IP232: Init, CTS low");

	assert(!EthernetReadThread.IsStarted());

	EthernetReadThread.Start();

	SerialACIA.Status &= ~MC6850_STATUS_CTS; // CTS goes low
	SerialACIA.Status |= MC6850_STATUS_TDRE; // so TDRE goes high

	return true;
}

/*--------------------------------------------------------------------------*/

bool IP232Poll()
{
	if (TotalCycles > IP232RxTrigger && InputBuffer.HasData())
	{
		return true;
	}

	return false;
}

/*--------------------------------------------------------------------------*/

bool IP232PollStatus()
{
	return StatusBuffer.HasData();
}

/*--------------------------------------------------------------------------*/

void IP232Close()
{
	DebugTrace("IP232Close\n");

	if (EthernetReadThread.IsStarted())
	{
		EthernetReadThread.Join();
	}

	DeleteCriticalSection(&SharedBufferLock); // MM 26/11/21

	if (EthernetSocket != INVALID_SOCKET)
	{
		DebugTrace("Closing IP232 socket\n");

		if (DebugEnabled)
			DebugDisplayTrace(DebugType::RemoteServer, true, "IP232: Closing socket");

		int Result = closesocket(EthernetSocket);

		if (Result == SOCKET_ERROR)
		{
			int Error = WSAGetLastError();
			DebugTrace("closesocket() returned error %d\n", Error);
		}

		EthernetSocket = INVALID_SOCKET;
	}
}

/*--------------------------------------------------------------------------*/

void IP232Write(unsigned char Data)
{
	EnterCriticalSection(&SharedBufferLock); // MM 26/11/21

	DebugTrace("IP232Write %02X\n", Data);

	if (!OutputBuffer.PutData(Data))
	{
		DebugTrace("IP232Write send buffer full\n");

		if (DebugEnabled)
			DebugDisplayTrace(DebugType::RemoteServer, true, "IP232: Write send Buffer Full");
	}

	LeaveCriticalSection(&SharedBufferLock); // MM 26/11/21
}

/*--------------------------------------------------------------------------*/

unsigned char IP232Read()
{
	unsigned char Data = 0;

	EnterCriticalSection(&SharedBufferLock); // MM 26/11/21

	if (InputBuffer.HasData())
	{
		Data = InputBuffer.GetData();

		// Simulated baud rate delay between bytes..
		SetTrigger(2000000 / (SerialACIA.RxRate / 9), IP232RxTrigger);
	}
	else
	{
		DebugTrace("IP232 receive buffer empty\n");

		if (DebugEnabled)
			DebugDisplayTrace(DebugType::RemoteServer, true, "IP232: receive buffer empty");
	}

	LeaveCriticalSection(&SharedBufferLock); // MM 26/11/21

	return Data;
}

/*--------------------------------------------------------------------------*/

unsigned char IP232ReadStatus()
{
	unsigned char Data = 0;

	EnterCriticalSection(&SharedBufferLock); // MM 26/11/21

	Data = StatusBuffer.GetData();

	LeaveCriticalSection(&SharedBufferLock); // MM 26/11/21

	return Data;
}

/*--------------------------------------------------------------------------*/

void IP232SetRTS(bool RTS)
{
	DebugTrace("IP232SetRTS: RTS=%d\n", (int)RTS);

	if (IP232Handshake)
	{
		IP232Write(255);
		IP232Write(static_cast<unsigned char>(RTS));
	}
}

/*--------------------------------------------------------------------------*/

unsigned int EthernetPortReadThread::ThreadFunc()
{
	DebugTrace("EthernetPortReadThread::ThreadFunc starts\n");

	bool Close = false;

	while (!ShouldQuit())
	{
		if (!Close)
		{
			if (InputBuffer.GetSpace() > 256)
			{
				fd_set fds;
				FD_ZERO(&fds);
				static const timeval TimeOut = {0, 0};

				FD_SET(EthernetSocket, &fds);

				int NumReady = select(32, &fds, nullptr, nullptr, &TimeOut); // Read

				if (NumReady == SOCKET_ERROR)
				{
					DebugTrace("Read Select Error %d\n", WSAGetLastError());

					if (DebugEnabled)
						DebugDisplayTrace(DebugType::RemoteServer, true, "IP232: Select error on read");
				}
				else if (NumReady > 0)
				{
					unsigned char Buffer[256];

					int BytesReceived = recv(EthernetSocket, (char *)Buffer, 256, 0);

					if (BytesReceived != SOCKET_ERROR)
					{
						// DebugTrace("Read %d bytes\n%s\n", BytesReceived, Buffer);
						EthernetReceivedData(Buffer, BytesReceived);
					}
					else
					{
						Close = true;

						// Should really check what the error was ...
						int Error = WSAGetLastError();

						DebugTrace("Read error %d, remote session disconnected\n", Error);

						PostMessage(mainWin->GethWnd(), WM_IP232_ERROR, Error, 0);
					}
				}
			}

			if (OutputBuffer.HasData())
			{
				DebugTrace("Sending %d bytes to IP232 server\n", OutputBuffer.GetLength());

				if (DebugEnabled)
					DebugDisplayTrace(DebugType::RemoteServer, true, "IP232: Sending to remote server");

				EnterCriticalSection(&SharedBufferLock); // MM 26/11/21

				unsigned char Buffer[256];
				int BufferLength = 0;

				while (OutputBuffer.HasData() && BufferLength < 256)
				{
					Buffer[BufferLength++] = OutputBuffer.GetData();
				}

				LeaveCriticalSection(&SharedBufferLock); // MM 26/11/21

				fd_set fds;
				FD_ZERO(&fds);
				static const timeval TimeOut = {0, 0};

				FD_SET(EthernetSocket, &fds);

				int NumReady = select(32, NULL, &fds, NULL, &TimeOut); // Write

				if (NumReady == SOCKET_ERROR)
				{
					DebugTrace("Write Select Error %d\n", WSAGetLastError());

					if (DebugEnabled)
						DebugDisplayTrace(DebugType::RemoteServer, true, "IP232: Select error on write");
				}
				else if (NumReady > 0)
				{
					int BytesSent = send(EthernetSocket, (char *)Buffer, BufferLength, 0);

					if (BytesSent == SOCKET_ERROR)
					{
						Close = true;

						// Should really check what the error was ...
						int Error = WSAGetLastError();

						DebugTrace("Send Error %i, Error %d\n", BytesSent, Error);

						PostMessage(mainWin->GethWnd(), WM_IP232_ERROR, Error, 0);
					}
					else
					{
						DebugTrace("Sent %d of %d bytes\n", BytesSent, BufferLength);
					}
				}
			}
		}

		Sleep(50); // Sleep for 50 msec
	}

	DebugTrace("EthernetPortReadThread::ThreadFunc exits\n");

	return 0;
}

/*--------------------------------------------------------------------------*/

static void EthernetReceivedData(unsigned char* pData, int Length)
{
	if (DebugEnabled)
	{
		DebugReceivedData(pData, Length);
	}

	for (int i = 0; i < Length; i++)
	{
		if (IP232FlagReceived)
		{
			IP232FlagReceived = false;

			if (pData[i] == 1)
			{
				// DTR on modem high
				EthernetPortStoreStatus(IP232_DTR_HIGH);
			}
			else if (pData[i] == 0)
			{
				// DTR on modem low
				EthernetPortStoreStatus(IP232_DTR_LOW);
			}
			else if (pData[i] == 255)
			{
				if (DebugEnabled)
					DebugDisplayTrace(DebugType::RemoteServer, true, "Flag,Flag =255");

				EthernetPortStore(255);
			}
		}
		else
		{
			if (pData[i] == 255 && !IP232Raw)
			{
				// The next value received is the DCD state (0 or 1),
				// or a 255 data value
				IP232FlagReceived = true;
			}
			else
			{
				EthernetPortStore(pData[i]);
			}
		}
	}
}

/*--------------------------------------------------------------------------*/

static void EthernetPortStoreStatus(unsigned char Data)
{
	EnterCriticalSection(&SharedBufferLock);

	if (!StatusBuffer.PutData((unsigned char)Data))
	{
		DebugTrace("EthernetPortStoreStatus buffer full\n");

		if (DebugEnabled)
			DebugDisplayTrace(DebugType::RemoteServer, true, "IP232: EthernetPortStoreStatus buffer full");
	}

	LeaveCriticalSection(&SharedBufferLock);
}

/*--------------------------------------------------------------------------*/

static void EthernetPortStore(unsigned char Data)
{
	EnterCriticalSection(&SharedBufferLock); // MM 26/11/21

	if (!InputBuffer.PutData(Data))
	{
		DebugTrace("EthernetPortStore output buffer full\n");

		if (DebugEnabled)
			DebugDisplayTrace(DebugType::RemoteServer, true, "IP232: EthernetPortStore output buffer full");
	}

	LeaveCriticalSection(&SharedBufferLock); // MM 26/11/21
}

/*--------------------------------------------------------------------------*/

static void DebugReceivedData(unsigned char* pData, int Length)
{
	char info[514];
	int i;

	DebugDisplayTraceF(DebugType::RemoteServer, true,
	                   "IP232: Read %d bytes from server", Length);

	static const char HexDigit[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};

	for (i = 0; i < Length; i++)
	{
		info[i * 2]       = HexDigit[(pData[i] >> 4) & 0xf];
		info[(i * 2) + 1] = HexDigit[pData[i] & 0x0f];
	}

	info[i * 2] = 0;

	DebugDisplayTrace(DebugType::RemoteServer, true, info);
}

/*--------------------------------------------------------------------------*/
