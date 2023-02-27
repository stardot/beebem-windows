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
 *  serialdevices.cc
 *  BeebEm3
 *
 *  Created by Jon Welch on 28/08/2006.
 *

 Remote serial port & IP232 support by Rob O'Donnell Mar 2009.
 . Raw mode connects and disconnects on RTS going up and down.
 . CTS reflects connection status.
 . IP232 mode maintains contstant connection. CTS reflects DTR
 . on the modem; you would normally tie this to DCD depending
 . on your application.
 . Currently "custom destination" must be specified by editing
 . Preferences.cfg after saving settings once.

 */

#include <process.h>
#include <stdio.h>

#include <windows.h>

#include "SerialDevices.h"
#include "6502core.h"
#include "uef.h"
#include "main.h"
#include "serial.h"
#include "beebsound.h"
#include "beebwin.h"
#include "debug.h"
#include "DebugTrace.h"
#include "uefstate.h"
#include "csw.h"
#include "uservia.h"
#include "atodconv.h"
#include "RingBuffer.h"

constexpr int TS_DELAY = 8192; // Cycles to wait for data to be TX'd or RX'd
static int TouchScreenMode = 0;
static int TouchScreenDelay;

constexpr int IP232_CXDELAY = 8192; // Cycles to wait after connection

// IP232
static SOCKET EthernetSocket = INVALID_SOCKET; // Listen socket
static HANDLE hEthernetPortReadThread = nullptr;
static HANDLE hEthernetPortStatusThread = nullptr;

static unsigned int __stdcall MyEthernetPortReadThread(void *parameter);
static unsigned int __stdcall MyEthernetPortStatusThread(void *parameter);

// This bit is the Serial Port stuff
SerialType SerialDestination;
bool IP232Mode;
bool IP232Raw;
char IP232Address[256];
int IP232Port;

static bool ip232_flag_received = false;
// static bool mStartAgain = false;

static RingBuffer InputBuffer;
static RingBuffer OutputBuffer;

CycleCountT IP232RxTrigger=CycleCountTMax;

static void EthernetReceivedData(unsigned char* pData, int Length);
static void EthernetPortStore(unsigned char data);
static void DebugReceivedData(unsigned char* pData, int Length);

void TouchScreenOpen()
{
	InputBuffer.Reset();
	OutputBuffer.Reset();
	TouchScreenDelay = 0;
}

bool TouchScreenPoll()
{
	if (TouchScreenDelay > 0)
	{
		TouchScreenDelay--;
		return false;
	}

	// Process data waiting to be received by BeebEm straight away
	if (OutputBuffer.HasData())
	{
		return true;
	}

	if (InputBuffer.HasData())
	{
		unsigned char data = InputBuffer.GetData();

		switch (data)
		{
			case 'M':
				TouchScreenMode = 0;
				break;

			case '0':
			case '1':
			case '2':
			case '3':
			case '4':
			case '5':
			case '6':
			case '7':
			case '8':
			case '9':
				TouchScreenMode = TouchScreenMode * 10 + data - '0';
				break;

			case '.':
				// Mode 1 seems to be polled, sends a '?' and we reply with data
				// Mode 129 seems to be send current values all time
				DebugTrace("Setting touch screen mode to %d\n", TouchScreenMode);
				break;

			case '?':
				if (TouchScreenMode == 1)
				{
					// Polled mode
					TouchScreenReadScreen(false);
				}
				break;
		}
	}

	if (TouchScreenMode == 129)
	{
		// Real time mode
		TouchScreenReadScreen(true);
	}
	else if (TouchScreenMode == 130)
	{
		// Area mode - seems to be pressing with two fingers which we can't really do ??
		TouchScreenReadScreen(true);
	}

	return false;
}

void TouchScreenWrite(unsigned char data)
{
	// DebugTrace("TouchScreenWrite 0x%02x\n", data);

	if (InputBuffer.PutData(data))
	{
		TouchScreenDelay = TS_DELAY;
	}
	else
	{
		DebugTrace("TouchScreenWrite input buffer full\n");
	}
}

unsigned char TouchScreenRead()
{
	unsigned char data = 0;

	if (OutputBuffer.HasData())
	{
		data = OutputBuffer.GetData();
		TouchScreenDelay = TS_DELAY;
	}
	else
	{
		DebugTrace("TouchScreenRead output buffer empty\n");
	}

	// DebugTrace("TouchScreenRead 0x%02x\n", data);

	return data;
}

void TouchScreenClose()
{
}

void TouchScreenStore(unsigned char data)
{
	if (!OutputBuffer.PutData(data))
	{
		DebugTrace("TouchScreenStore output buffer full\n");
	}
}

void TouchScreenReadScreen(bool check)
{
	static int last_x = -1, last_y = -1, last_m = -1;

	int x = (65535 - JoystickX) / (65536 / 120) + 1;
	int y = JoystickY / (65536 / 90) + 1;

	if (last_x != x || last_y != y || last_m != AMXButtons || !check)
	{
		// DebugTrace("JoystickX = %d, JoystickY = %d, last_x = %d, last_y = %d\n", JoystickX, JoystickY, last_x, last_y);

		if (AMXButtons & AMX_LEFT_BUTTON)
		{
			TouchScreenStore(64 + ((x & 0xf0) >> 4));
			TouchScreenStore(64 + (x & 0x0f));
			TouchScreenStore(64 + ((y & 0xf0) >> 4));
			TouchScreenStore(64 + (y & 0x0f));
			TouchScreenStore('.');
			// DebugTrace("Sending X = %d, Y = %d\n", x, y);
		}
		else
		{
			TouchScreenStore(64 + 0x0f);
			TouchScreenStore(64 + 0x0f);
			TouchScreenStore(64 + 0x0f);
			TouchScreenStore(64 + 0x0f);
			TouchScreenStore('.');
			// DebugTrace("Screen not touched\n");
		}

		last_x = x;
		last_y = y;
		last_m = AMXButtons;
	}
}

// Note - touchscreen routines use InputBuffer for data into the ts
// and OutputBuffer as data from the touchscreen.
// IP232 routines use InputBuffer as data coming in from the modem,
// and OutputBuffer for data to be sent out to the modem,
// i.e., the opposite way around!!       watch out!

bool IP232Open()
{
	InputBuffer.Reset();
	OutputBuffer.Reset();

	// Let's prepare some IP sockets

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

	SetTrigger(IP232_CXDELAY, IP232RxTrigger);

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

	// TEMP!
	ACIA_Status &= ~MC6850_STATUS_CTS;
	ACIA_Status |= MC6850_STATUS_TDRE;

	if (DebugEnabled)
		DebugDisplayTrace(DebugType::RemoteServer, true, "IP232: Init, CTS low");

	if (hEthernetPortReadThread == nullptr)
	{
		hEthernetPortReadThread = reinterpret_cast<HANDLE>(_beginthreadex(
			nullptr,                  // security
			0,                        // stack_size
			MyEthernetPortReadThread, // start_address
			nullptr,                  // arglist
			0,                        // initflag
			nullptr                   // thrdaddr
		));

		hEthernetPortStatusThread = reinterpret_cast<HANDLE>(_beginthreadex(
			nullptr,                    // security
			0,                          // stack_size
			MyEthernetPortStatusThread, // start_address
			nullptr,                    // arglist
			0,                          // initflag
			nullptr                     // thrdaddr
		));
	}

	return true;
}

bool IP232Poll()
{
//	fd_set	fds;
//	timeval tv;
//	int i;

/*	if (mStartAgain == true)
	{
		DebugTrace("Closing Comms\n");
		if (DebugEnabled)
				DebugDisplayTrace(DEBUG_REMSER, true, "IP232: Comms Close");
		// mStartAgain = false;
		// mainWin->Report(MessageType::Error, "Could not connect to specified address");
		bSerialStateChanged = true;
		SerialPortEnabled = false;
		mainWin->ExternUpdateSerialMenu();
		
		IP232Close();
		// IP232Open();
		return false;
	}
*/

	if (TotalCycles > IP232RxTrigger && InputBuffer.HasData())
	{
		return true;
	}

	return false;
}

void IP232Close()
{
	if (EthernetSocket != INVALID_SOCKET)
	{
		DebugTrace("Closing IP232 socket\n");

		if (DebugEnabled)
			DebugDisplayTrace(DebugType::RemoteServer, true, "IP232: Closing Sockets");

		closesocket(EthernetSocket);
		EthernetSocket = INVALID_SOCKET;
	}

/*
	if (mEthernetPortReadTaskID != NULL)
	{
		MPTerminateTask(mEthernetPortReadTaskID, 0);
	}
	if (mEthernetPortStatusTaskID != NULL)
	{
		MPTerminateTask(mEthernetPortStatusTaskID, 0);
	}
	
	if (mListenTaskID != NULL)
	{
		MPTerminateTask(mListenTaskID, 0);
	}

	mListenHandle = NULL;
	mEthernetPortStatusTaskID = NULL;
	mEthernetPortReadTaskID = NULL;
*/
}

void IP232Write(unsigned char data)
{
	// DebugTrace("TouchScreenWrite 0x%02x\n", data);

	if (!OutputBuffer.PutData(data))
	{
		DebugTrace("IP232Write send buffer full\n");

		if (DebugEnabled)
			DebugDisplayTrace(DebugType::RemoteServer, true, "IP232: Write send Buffer Full");
	}
}

unsigned char IP232Read()
{
	unsigned char data = 0;

	if (InputBuffer.HasData())
	{
		data = InputBuffer.GetData();

		// Simulated baud rate delay between bytes..
		SetTrigger(2000000 / (Rx_Rate / 9), IP232RxTrigger);
	}
	else
	{
		DebugTrace("IP232 receive buffer empty\n");

		if (DebugEnabled)
			DebugDisplayTrace(DebugType::RemoteServer, true, "IP232: receive buffer empty");
	}

	return data;
}

static unsigned int __stdcall MyEthernetPortReadThread(void * /* parameter */)
{
	// Much taken from Mac version by Jon Welch
	fd_set fds;
	timeval tv;
	unsigned char Buffer[256];
	int BufferLength;

	Sleep(3000);

	while (1)
	{
		if (EthernetSocket != INVALID_SOCKET)
		{
			if (InputBuffer.GetSpace() > 256)
			{
				FD_ZERO(&fds);
				tv.tv_sec = 0;
				tv.tv_usec = 0;

				FD_SET(EthernetSocket, &fds);

				int NumReady = select(32, &fds, NULL, NULL, &tv); // Read

				if (NumReady > 0)
				{
					int BytesReceived = recv(EthernetSocket, (char *)Buffer, 256, 0);

					if (BytesReceived != SOCKET_ERROR)
					{
						// DebugTrace("Read %d bytes\n%s\n", BytesReceived, Buffer);
						EthernetReceivedData(Buffer, BytesReceived);
					}
					else
					{
						// Should really check what the error was ...
						int Error = WSAGetLastError();

						DebugTrace("Read error %d\n", Error);
						DebugTrace("Remote session disconnected\n");

						if (DebugEnabled)
							DebugDisplayTrace(DebugType::RemoteServer, true, "IP232: Remote session disconnected");

						// mEthernetPortReadTaskID = NULL;
						bSerialStateChanged = true;
						SerialPortEnabled = false;
						mainWin->UpdateSerialMenu();
						IP232Close();
						mainWin->Report(MessageType::Error,
						                "Lost connection. Serial port has been disabled");
						return 0;
					}
				}
				else
				{
					// DebugTrace("Nothing to read %d\n", i);
				}
			}

			if (OutputBuffer.HasData() && EthernetSocket != INVALID_SOCKET)
			{
				DebugTrace("Sending %d bytes to IP232 server\n", OutputBuffer.GetLength());

				if (DebugEnabled)
					DebugDisplayTrace(DebugType::RemoteServer, true, "IP232: Sending to remote server");

				BufferLength = 0;

				while (OutputBuffer.HasData())
				{
					Buffer[BufferLength++] = OutputBuffer.GetData();
				}

				FD_ZERO(&fds);
				tv.tv_sec = 0;
				tv.tv_usec = 0;

				FD_SET(EthernetSocket, &fds);

				int NumReady = select(32, NULL, &fds, NULL, &tv); // Write

				if (NumReady <= 0)
				{
					DebugTrace("Select Error %i\n", NumReady);

					if (DebugEnabled)
						DebugDisplayTrace(DebugType::RemoteServer, true, "IP232: Select error on send");
				}
				else
				{
					int BytesSent = send(EthernetSocket, (char *)Buffer, BufferLength, 0);

					if (BytesSent < BufferLength)
					{
						// Should really check what the error was ...
						DebugTrace("Send Error %i\n", BytesSent);

						if (DebugEnabled)
							DebugDisplayTrace(DebugType::RemoteServer, true, "IP232: Send Error");
						bSerialStateChanged = true;
						SerialPortEnabled = false;
						mainWin->UpdateSerialMenu();

						IP232Close();
						mainWin->Report(MessageType::Error,
						                "Lost connection. Serial port has been disabled");
					}
				}
			}
		}

		Sleep(50); // Sleep for 50 msec
	}

	return 0;
}

static void EthernetReceivedData(unsigned char* pData, int Length)
{
	if (DebugEnabled)
	{
		DebugReceivedData(pData, Length);
	}

	for (int i = 0; i < Length; i++)
	{
		if (ip232_flag_received)
		{
			ip232_flag_received = false;

			if (pData[i] == 1)
			{
				if (DebugEnabled)
					DebugDisplayTrace(DebugType::RemoteServer, true, "Flag,1 DCD True, CTS");

				// dtr on modem high
				ACIA_Status &= ~MC6850_STATUS_CTS; // CTS goes active low
				ACIA_Status |= MC6850_STATUS_TDRE; // so TDRE goes high ??
			}
			else if (pData[i] == 0)
			{
				if (DebugEnabled)
					DebugDisplayTrace(DebugType::RemoteServer, true, "Flag,0 DCD False, clear CTS");

				// dtr on modem low
				ACIA_Status |= MC6850_STATUS_CTS; // CTS goes inactive high
				ACIA_Status &= ~MC6850_STATUS_TDRE; // so TDRE goes low
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
				ip232_flag_received = true;
			}
			else
			{
				EthernetPortStore(pData[i]);
			}
		}
	}
}

void EthernetPortStore(unsigned char data)
{
	// much taken from Mac version by Jon Welch

	if (!InputBuffer.PutData(data))
	{
		DebugTrace("EthernetPortStore output buffer full\n");

		if (DebugEnabled)
			DebugDisplayTrace(DebugType::RemoteServer, true, "IP232: EthernetPortStore output buffer full");
	}
}

static unsigned int __stdcall MyEthernetPortStatusThread(void * /* parameter */)
{
	// much taken from Mac version by Jon Welch
	int dcd = 0;
	int odcd = 0;
	int rts = 0;
	int orts = 0;

	// Put bits in here for DCD when got active IP connection

	Sleep(2000);

	while (1)
	{
		if (!IP232Mode)
		{
			if (EthernetSocket != INVALID_SOCKET)
			{
				dcd = 1;
			}
			else
			{
				dcd = 0;
			}

			if (dcd == 1 && odcd == 0)
			{
				// RaiseDCD();
				ACIA_Status &= ~MC6850_STATUS_CTS; // CTS goes low
				ACIA_Status |= MC6850_STATUS_TDRE; // so TDRE goes high

				if (DebugEnabled)
					DebugDisplayTrace(DebugType::RemoteServer, true, "IP232: StatusThread DCD up, set CTS low");
			}

			if (dcd == 0 && odcd == 1)
			{
				// LowerDCD();
				ACIA_Status |= MC6850_STATUS_CTS; // CTS goes high
				ACIA_Status &= ~MC6850_STATUS_TDRE; // so TDRE goes low

				if (DebugEnabled)
					DebugDisplayTrace(DebugType::RemoteServer, true, "IP232: StatusThread lost DCD, set CTS high");
			}
		}
		else // IP232mode == true
		{
			if ((ACIA_Control & 96) == 64)
			{
				rts = 1;
			}
			else
			{
				rts = 0;
			}

			if (rts != orts)
			{
				if (EthernetSocket != INVALID_SOCKET)
				{
					if (DebugEnabled)
					{
						DebugDisplayTraceF(DebugType::RemoteServer, true,
						                   "IP232: Sending RTS status of %i", rts);
					}
					
					IP232Write(255);
					IP232Write(static_cast<unsigned char>(rts));
				}

				orts = rts;
			}
		}

		if (dcd != odcd)
			odcd = dcd;

		Sleep(50); // sleep for 50 milliseconds
	}

	DebugTrace("Exited MySerialStatusThread\n");

	return 0;
}

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
