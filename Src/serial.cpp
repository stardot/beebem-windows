/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2001  Richard Gellman
Copyright (C) 2004  Mike Wyatt

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

// Serial/Cassette Support for BeebEm
// Written by Richard Gellman - March 2001
//
// P.S. If anybody knows how to emulate this, do tell me - 16/03/2001 - Richard Gellman
//
// See https://beebwiki.mdfs.net/Acorn_cassette_format
// and http://electrem.emuunlim.com/UEFSpecs.html

#include <windows.h>
#include <process.h>
#include <stdio.h>
#include <algorithm>

#include "6502core.h"
#include "uef.h"
#include "main.h"
#include "serial.h"
#include "beebsound.h"
#include "beebwin.h"
#include "beebemrc.h"
#include "debug.h"
#include "uefstate.h"
#include "csw.h"
#include "serialdevices.h"
#include "debug.h"
#include "log.h"

// MC6850 control register bits
constexpr unsigned char MC6850_CONTROL_COUNTER_DIVIDE   = 0x03;
constexpr unsigned char MC6850_CONTROL_MASTER_RESET     = 0x03;
constexpr unsigned char MC6850_CONTROL_WORD_SELECT      = 0x1c;
constexpr unsigned char MC6850_CONTROL_TRANSMIT_CONTROL = 0x60;
constexpr unsigned char MC6850_CONTROL_RIE              = 0x80;

// MC6850 status register bits
constexpr unsigned char MC6850_STATUS_RDRF = 0x01;
constexpr unsigned char MC6850_STATUS_TDRE = 0x02;
constexpr unsigned char MC6850_STATUS_DCD  = 0x04;
constexpr unsigned char MC6850_STATUS_CTS  = 0x08;
constexpr unsigned char MC6850_STATUS_FE   = 0x10;
constexpr unsigned char MC6850_STATUS_OVRN = 0x20;
constexpr unsigned char MC6850_STATUS_PE   = 0x40;
constexpr unsigned char MC6850_STATUS_IRQ  = 0x80;

enum class SerialDevice : unsigned char {
	Cassette,
	RS423
};

static bool CassetteRelay = false; // Cassette Relay state
static SerialDevice SerialChannel = SerialDevice::Cassette; // Device in use

static unsigned char RDR, TDR; // Receive and Transmit Data Registers
static unsigned char RDSR, TDSR; // Receive and Transmit Data Shift Registers (buffers)
unsigned int Tx_Rate=1200,Rx_Rate=1200; // Recieve and Transmit baud rates.
unsigned char Clk_Divide=1; // Clock divide rate

unsigned char ACIA_Status, ACIA_Control; // 6850 ACIA Status & Control
static unsigned char SerialULAControl; // Serial ULA / SERPROC control register;

static unsigned char RTS;
static bool FirstReset = true;
static unsigned char DCD = 0;
static unsigned char DCDI = 1;
static unsigned char ODCDI = 1;
static unsigned char DCDClear = 0; // count to clear DCD bit

static unsigned char Parity, StopBits, DataBits;

bool RIE, TIE; // Receive Interrupt Enable and Transmit Interrupt Enable

unsigned char TxD,RxD; // Transmit and Receive destinations (data or shift register)

char UEFTapeName[256]; // Filename of current tape file
bool UEFOpen = false;

struct WordSelectBits
{
	unsigned char DataBits;
	unsigned char Parity;
	unsigned char StopBits;
};

static const WordSelectBits WordSelect[8] =
{
	{ 7, EVENPARITY, 2 },
	{ 7, ODDPARITY,  2 },
	{ 7, EVENPARITY, 1 },
	{ 7, ODDPARITY,  1 },
	{ 8, NOPARITY,   2 },
	{ 8, NOPARITY,   1 },
	{ 8, EVENPARITY, 1 },
	{ 8, ODDPARITY,  1 },
};

struct TransmitterControlBits
{
	bool RTS;
	bool TIE;
};

static const TransmitterControlBits TransmitterControl[8] =
{
	{ false, false },
	{ false, true  },
	{ true,  false },
	{ false, false },
};

static const unsigned int Baud_Rates[8] = {
	19200, 1200, 4800, 150, 9600, 300, 2400, 75
};

bool OldRelayState = false;
CycleCountT TapeTrigger=CycleCountTMax;
constexpr int TAPECYCLES = 2000000 / 5600; // 5600 is normal tape speed

static int UEF_BUF=0,NEW_UEF_BUF=0;
int TapeClock=0,OldClock=0;
int TapeClockSpeed = 5600;
bool UnlockTape = true;

// Tape control variables
int map_lines;
char map_desc[MAX_MAP_LINES][40];
int map_time[MAX_MAP_LINES];

bool TapeControlEnabled = false;
bool TapePlaying = true;
bool TapeRecording = false;
static HWND hwndTapeControl;
static HWND hwndMap;
void TapeControlUpdateCounter(int tape_time);
void TapeControlStopRecording(bool RefreshControl);

// This bit is the Serial Port stuff
bool SerialPortEnabled;
unsigned char SerialPort;

static HANDLE hSerialThread = nullptr;
static HANDLE hStatThread = nullptr;
static HANDLE hSerialPort = INVALID_HANDLE_VALUE; // Serial port handle
static DCB dcbSerialPort; // Serial port device control block
static char nSerialPort[5]; // Serial port name
static const char *pnSerialPort = nSerialPort;
bool SerialPortOpen = false; // Indicates status of serial port (on the host)
static unsigned int SerialBuffer=0,SerialWriteBuffer=0;
static DWORD BytesIn,BytesOut;
static DWORD LineStat;
static DWORD dwCommEvent;
static OVERLAPPED olSerialPort={0},olSerialWrite={0},olStatus={0};
volatile bool bSerialStateChanged = false;
static volatile bool bWaitingForData = false;
static volatile bool bWaitingForStat = false;
static volatile bool bCharReady = false;
static COMMTIMEOUTS ctSerialPort;
static DWORD dwClrCommError;

void SetACIAStatus(unsigned char bit) {
	ACIA_Status|=1<<bit;
}

void ResetACIAStatus(unsigned char bit) {
	ACIA_Status&=~(1<<bit);
}

void SerialACIAWriteControl(unsigned char Value)
{
	if (DebugEnabled)
	{
		DebugDisplayTraceF(DebugType::Serial, true,
		                   "Serial: Write ACIA control %02X", (int)Value);
	}

	ACIA_Control = Value; // This is done for safe keeping

	// Master reset - clear all bits in the status register, except for
	// external conditions on CTS and DCD.
	if ((Value & MC6850_CONTROL_COUNTER_DIVIDE) == MC6850_CONTROL_MASTER_RESET)
	{
		ACIA_Status &= MC6850_STATUS_CTS;
		ACIA_Status |= MC6850_STATUS_DCD;

		// Master reset clears IRQ
		ACIA_Status &= ~MC6850_STATUS_IRQ;
		intStatus &= ~(1 << serial);

		if (FirstReset)
		{
			// RTS High on first Master reset.
			ACIA_Status |= MC6850_STATUS_CTS;
			FirstReset = false;
			RTS = true;
		}

		ACIA_Status &= ~MC6850_STATUS_DCD; DCD=0; DCDI=0; DCDClear=0;
		ACIA_Status |= MC6850_STATUS_TDRE; // Transmit data register empty
		SetTrigger(TAPECYCLES, TapeTrigger);
	}

	// Clock Divide
	if ((Value & MC6850_CONTROL_COUNTER_DIVIDE) == 0x00) Clk_Divide = 1;
	if ((Value & MC6850_CONTROL_COUNTER_DIVIDE) == 0x01) Clk_Divide = 16;
	if ((Value & MC6850_CONTROL_COUNTER_DIVIDE) == 0x02) Clk_Divide = 64;

	// Word select
	Parity   = WordSelect[(Value & MC6850_CONTROL_WORD_SELECT) >> 2].Parity;
	StopBits = WordSelect[(Value & MC6850_CONTROL_WORD_SELECT) >> 2].StopBits;
	DataBits = WordSelect[(Value & MC6850_CONTROL_WORD_SELECT) >> 2].DataBits;

	// Transmitter control
	RTS = TransmitterControl[(Value & MC6850_CONTROL_TRANSMIT_CONTROL) >> 5].RTS;
	TIE = TransmitterControl[(Value & MC6850_CONTROL_TRANSMIT_CONTROL) >> 5].TIE;
	RIE = (Value & MC6850_CONTROL_RIE) != 0;

	// Seem to need an interrupt immediately for tape writing when TIE set
	if (SerialChannel == SerialDevice::Cassette && TIE && CassetteRelay)
	{
		ACIA_Status |= MC6850_STATUS_IRQ;
		intStatus |= 1 << serial;
	}

	// Change serial port settings
	if (SerialChannel == SerialDevice::RS423 && SerialPortEnabled && !TouchScreenEnabled && !EthernetPortEnabled)
	{
		GetCommState(hSerialPort, &dcbSerialPort);
		dcbSerialPort.ByteSize  = DataBits;
		dcbSerialPort.StopBits  = (StopBits == 1) ? ONESTOPBIT : TWOSTOPBITS;
		dcbSerialPort.Parity    = Parity;
		dcbSerialPort.DCBlength = sizeof(dcbSerialPort);
		SetCommState(hSerialPort, &dcbSerialPort);
		// Check RTS
		EscapeCommFunction(hSerialPort, RTS ? CLRRTS : SETRTS);
	}
}

void SerialACIAWriteTxData(unsigned char Data)
{
	if (DebugEnabled)
	{
		DebugDisplayTraceF(DebugType::Serial, true,
		                   "Serial: Write ACIA Tx %02X", (int)Data);
	}

//	WriteLog("Serial: Write ACIA Tx %02X, SerialChannel = %d\n", (int)Data, SerialChannel);

	ACIA_Status &= ~MC6850_STATUS_IRQ;
	intStatus &= ~(1 << serial);

	// 10/09/06
	// JW - A bug in swarm loader overwrites the rs423 output buffer counter
	// Unless we do something with the data, the loader hangs so just swallow it (see below)

	if (SerialChannel == SerialDevice::Cassette || (SerialChannel == SerialDevice::RS423 && !SerialPortEnabled))
	{
		TDR = Data;
		TxD = 1;
		ACIA_Status &= ~MC6850_STATUS_TDRE;
		int baud = Tx_Rate * ((Clk_Divide == 1) ? 64 : (Clk_Divide==64) ? 1 : 4);

		SetTrigger(2000000 / (baud / 8) * TapeClockSpeed / 5600, TapeTrigger);
	}

	if (SerialChannel == SerialDevice::RS423 && SerialPortEnabled)
	{
		if (ACIA_Status & MC6850_STATUS_TDRE)
		{
			ACIA_Status &= ~MC6850_STATUS_TDRE;
			SerialWriteBuffer = Data;

			if (TouchScreenEnabled)
			{
				TouchScreenWrite(Data);
			}
			else if (EthernetPortEnabled)
			{
				// if (Data_Bits == 7) {
				//	IP232Write(Data & 127);
				// } else {
					IP232Write(Data);
					if (!IP232raw && Data == 255) IP232Write(Data);
				// }
			}
			else
			{
				WriteFile(hSerialPort, &SerialWriteBuffer, 1, &BytesOut, &olSerialWrite);
			}

			ACIA_Status |= MC6850_STATUS_TDRE;
		}
	}
}

// The Serial ULA control register controls the cassette motor relay,
// transmit and receive baud rates, and RS423/cassette switch

void SerialULAWrite(unsigned char Value)
{
	if (DebugEnabled)
	{
		DebugDisplayTraceF(DebugType::Serial, true,
		                   "Serial: Write serial ULA %02X", (int)Value);
	}

	SerialULAControl = Value;

	// Slightly easier this time.
	// just the Rx and Tx baud rates, and the selectors.
	CassetteRelay = (Value & 0x80) != 0;
	TapeAudio.Enabled = CassetteRelay && (TapePlaying || TapeRecording);
	LEDs.Motor = CassetteRelay;

	if (CassetteRelay)
	{
		SetTrigger(TAPECYCLES, TapeTrigger);
	}

	if (CassetteRelay != OldRelayState)
	{
		OldRelayState = CassetteRelay;
		ClickRelay(CassetteRelay);
	}

	SerialChannel = (Value & 0x40) != 0 ? SerialDevice::RS423 : SerialDevice::Cassette;
	Tx_Rate = Baud_Rates[(Value & 0x07)];
	Rx_Rate = Baud_Rates[(Value & 0x38) >> 3];

	// Note, the PC serial port (or at least win32) does not allow different
	// transmit/receive rates So we will use the higher of the two

	if (SerialChannel == SerialDevice::RS423)
	{
		unsigned int HigherRate = std::max(Rx_Rate, Tx_Rate);
		GetCommState(hSerialPort,&dcbSerialPort);
		dcbSerialPort.BaudRate=HigherRate;
		dcbSerialPort.DCBlength=sizeof(dcbSerialPort);
		SetCommState(hSerialPort,&dcbSerialPort);
	}
}

unsigned char SerialULARead()
{
	if (DebugEnabled)
	{
		DebugDisplayTraceF(DebugType::Serial, true,
		                   "Serial: Read serial ULA %02X", (int)SerialULAControl);
	}

	return SerialULAControl;
}

unsigned char SerialACIAReadStatus()
{
//	if (DCDI==0 && DCD!=0)
//	{
//		DCDClear++;
//		if (DCDClear > 1) {
//			DCD=0;
//			ACIA_Status &= ~(1 << MC6850_STATUS_DCS);
//			DCDClear=0;
//		}
//	}
	if (DebugEnabled)
	{
		DebugDisplayTraceF(DebugType::Serial, true,
		                   "Serial: Read ACIA status %02X", (int)ACIA_Status);
	}

	// WriteLog("Serial: Read ACIA status %02X\n", (int)ACIA_Status);

	// See https://github.com/stardot/beebem-windows/issues/47
	return ACIA_Status;
}

void HandleData(unsigned char Data)
{
	// This proc has to dump data into the serial chip's registers

	ACIA_Status &= ~MC6850_STATUS_OVRN;

	if (RxD == 0)
	{
		RDR = Data;
		ACIA_Status |= MC6850_STATUS_RDRF; // Rx Reg full
		RxD++;
	}
	else if (RxD == 1)
	{
		RDSR = Data;
		ACIA_Status |= MC6850_STATUS_RDRF;
		RxD++;
	}
	else if (RxD == 2)
	{
		RDR = RDSR;
		RDSR = Data;
		ACIA_Status |= MC6850_STATUS_OVRN;
	}

	if (RIE)
	{
		// interrupt on receive/overun
		ACIA_Status |= MC6850_STATUS_IRQ;
		intStatus |= 1 << serial;
	}
}

unsigned char SerialACIAReadRxData()
{
	unsigned char TData;
//	if (DCDI==0 && DCD!=0)
//	{
//		DCDClear++;
//		if (DCDClear > 1) {
//			DCD=0;
//			ACIA_Status &= ~(1 << MC6850_STATUS_DCS);
//			DCDClear=0;
//		}
//	}
	ACIA_Status &= ~MC6850_STATUS_IRQ;
	intStatus &= ~(1 << serial);

	TData=RDR; RDR=RDSR; RDSR=0;
	if (RxD > 0) RxD--;
	if (RxD == 0) ACIA_Status &= ~MC6850_STATUS_RDRF;
	if (RxD > 0 && RIE)
	{
		ACIA_Status |= MC6850_STATUS_IRQ;
		intStatus |= 1 << serial;
	}

	if (DataBits == 7) TData &= 127;

	if (DebugEnabled)
	{
		DebugDisplayTraceF(DebugType::Serial, true,
		                   "Serial: Read ACIA Rx %02X", (int)TData);
	}

//	WriteLog("Serial: Read ACIA Rx %02X, ACIA_Status = %02x\n", (int)TData, (int) ACIA_Status);

	return TData;
}

void Serial_Poll()
{
	if (SerialChannel == SerialDevice::Cassette)
	{
		if (TapeRecording)
		{
			if (CassetteRelay && UEFOpen && TotalCycles >= TapeTrigger)
			{
				if (TxD > 0)
				{
					// Writing data
					if (!uef_putdata(TDR|UEF_DATA, TapeClock))
					{
						mainWin->Report(MessageType::Error,
						                "Error writing to UEF file:\n  %s", UEFTapeName);

						TapeControlStopRecording(true);
					}
					TxD=0;
					ACIA_Status |= MC6850_STATUS_TDRE;

					if (TIE)
					{
						ACIA_Status |= MC6850_STATUS_IRQ;
						intStatus |= 1 << serial;
					}

					TapeAudio.Data       = (TDR << 1) | 1;
					TapeAudio.BytePos    = 1;
					TapeAudio.CurrentBit = 0;
					TapeAudio.Signal     = 1;
					TapeAudio.ByteCount  = 3;
				}
				else
				{
					// Tone
					if (!uef_putdata(UEF_HTONE, TapeClock))
					{
						mainWin->Report(MessageType::Error,
						                "Error writing to UEF file:\n  %s", UEFTapeName);

						TapeControlStopRecording(true);
					}

					TapeAudio.Signal  = 2;
					TapeAudio.BytePos = 11;
				}

				SetTrigger(TAPECYCLES, TapeTrigger);
				TapeClock++;
			}
		}
		else // Playing or stopped
		{
			// 10/09/06
			// JW - If trying to write data when not recording, just ignore

			if ( (TxD > 0) && (TotalCycles >= TapeTrigger) )
			{

//				WriteLog("Ignoring Writes\n");

				TxD=0;
				ACIA_Status |= MC6850_STATUS_TDRE;

				if (TIE)
				{
					ACIA_Status |= MC6850_STATUS_IRQ;
					intStatus |= 1 << serial;
				}
			}

			if (CassetteRelay && UEFOpen && TapeClock != OldClock)
			{
				NEW_UEF_BUF=uef_getdata(TapeClock);
				OldClock=TapeClock;
			}

			if ((NEW_UEF_BUF!=UEF_BUF || UEFRES_TYPE(NEW_UEF_BUF)==UEF_HTONE || UEFRES_TYPE(NEW_UEF_BUF)==UEF_GAP) &&
				CassetteRelay && UEFOpen)
			{
				if (UEFRES_TYPE(UEF_BUF) != UEFRES_TYPE(NEW_UEF_BUF))
					TapeControlUpdateCounter(TapeClock);

				UEF_BUF=NEW_UEF_BUF;

				// New data read in, so do something about it
				if (UEFRES_TYPE(UEF_BUF) == UEF_HTONE)
				{
					DCDI = 1;
					TapeAudio.Signal = 2;
					// TapeAudio.Samples = 0;
					TapeAudio.BytePos = 11;
				}

				if (UEFRES_TYPE(UEF_BUF) == UEF_GAP)
				{
					DCDI = 1;
					TapeAudio.Signal = 0;
				}

				if (UEFRES_TYPE(UEF_BUF) == UEF_DATA)
				{
					DCDI = 0;
					HandleData(UEFRES_BYTE(UEF_BUF));
					TapeAudio.Data       = (UEFRES_BYTE(UEF_BUF) << 1) | 1;
					TapeAudio.BytePos    = 1;
					TapeAudio.CurrentBit = 0;
					TapeAudio.Signal     = 1;
					TapeAudio.ByteCount  = 3;
				}
			}

			if (CassetteRelay && RxD < 2 && UEFOpen)
			{
				if (TotalCycles >= TapeTrigger)
				{
					if (TapePlaying)
						TapeClock++;

					SetTrigger(TAPECYCLES, TapeTrigger);
				}
			}

			// CSW stuff

			if (CassetteRelay && CSWOpen && TapeClock != OldClock)
			{
				CSWState last_state = csw_state;

				CSW_BUF = csw_poll(TapeClock);
				OldClock = TapeClock;

				if (last_state != csw_state)
					TapeControlUpdateCounter(csw_ptr);

				if (csw_state == CSWState::WaitingForTone)
				{
					DCDI=1;
					TapeAudio.Signal=0;
				}

				// New data read in, so do something about it
				if (csw_state == CSWState::Tone)
				{
					DCDI = 1;
					TapeAudio.Signal  = 2;
					TapeAudio.BytePos = 11;
				}

				if (CSW_BUF >= 0 && csw_state == CSWState::Data)
				{
					DCDI = 0;
					HandleData((unsigned char)CSW_BUF);
					TapeAudio.Data       = (CSW_BUF << 1) | 1;
					TapeAudio.BytePos    = 1;
					TapeAudio.CurrentBit = 0;
					TapeAudio.Signal     = 1;
					TapeAudio.ByteCount  = 3;
				}
			}

			if (CassetteRelay && RxD < 2 && CSWOpen)
			{
				if (TotalCycles >= TapeTrigger)
				{
					if (TapePlaying)
						TapeClock++;

					SetTrigger(CSW_CYCLES, TapeTrigger);
				}
			}

			if (DCDI == 1 && ODCDI == 0)
			{
				// low to high transition on the DCD line
				if (RIE)
				{
					ACIA_Status |= MC6850_STATUS_IRQ;
					intStatus |= 1 << serial;
				}

				DCD = 1;
				ACIA_Status |= MC6850_STATUS_DCD; // ACIA_Status &= ~MC6850_STATUS_RDRF;
				DCDClear = 0;
			}

			if (DCDI == 0 && ODCDI == 1)
			{
				DCD = 0;
				ACIA_Status &= ~MC6850_STATUS_DCD;
				DCDClear = 0;
			}

			if (DCDI != ODCDI)
				ODCDI = DCDI;
		}
	}

	if (SerialChannel == SerialDevice::RS423 && SerialPortEnabled)
	{
		if (TouchScreenEnabled)
		{
			if (TouchScreenPoll())
			{
				if (RxD<2)
					HandleData(TouchScreenRead());
			}
		}
		else if (EthernetPortEnabled)
		{
			if (IP232Poll())
			{
				if (RxD<2)
					HandleData(IP232Read());
			}
		}
		else
		{
			if (!bWaitingForStat && !bSerialStateChanged) {
				WaitCommEvent(hSerialPort,&dwCommEvent,&olStatus);
				bWaitingForStat = true;
			}

			if (!bSerialStateChanged && bCharReady && !bWaitingForData && RxD < 2) {
				if (!ReadFile(hSerialPort,&SerialBuffer,1,&BytesIn,&olSerialPort)) {
					if (GetLastError()==ERROR_IO_PENDING) {
						bWaitingForData = true;
					} else {
						mainWin->Report(MessageType::Error, "Serial Port Error");
					}
				} else {
					if (BytesIn>0) {
						HandleData((unsigned char)SerialBuffer);
					} else {
						ClearCommError(hSerialPort, &dwClrCommError,NULL);
						bCharReady = false;
					}
				}
			}
		}
	}
}

static void InitThreads()
{
	if (hSerialPort != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hSerialPort);
		hSerialPort = INVALID_HANDLE_VALUE;
	}

	bWaitingForData = false;
	bWaitingForStat = false;

	if (SerialPortEnabled && SerialPort > 0)
	{
		InitSerialPort(); // Set up the serial port if its enabled.

		if (olSerialPort.hEvent)
		{
			CloseHandle(olSerialPort.hEvent);
			olSerialPort.hEvent = nullptr;
		}

		olSerialPort.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr); // Create the serial port event signal

		if (olSerialWrite.hEvent)
		{
			CloseHandle(olSerialWrite.hEvent);
			olSerialWrite.hEvent = nullptr;
		}

		olSerialWrite.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr); // Write event, not actually used.

		if (olStatus.hEvent)
		{
			CloseHandle(olStatus.hEvent);
			olStatus.hEvent = nullptr;
		}

		olStatus.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr); // Status event, for WaitCommEvent
	}

	bSerialStateChanged = false;
}

static unsigned int __stdcall StatThread(void * /* lpParam */)
{
	DWORD dwOvRes = 0;

	do {
		if (!TouchScreenEnabled && !EthernetPortEnabled &&
		    (WaitForSingleObject(olStatus.hEvent, 10) == WAIT_OBJECT_0) && SerialPortEnabled) {
			if (GetOverlappedResult(hSerialPort,&olStatus,&dwOvRes,FALSE)) {
				// Event waiting in dwCommEvent
				if ((dwCommEvent & EV_RXCHAR) && !bWaitingForData) {
					bCharReady = true;
				}
				if (dwCommEvent & EV_CTS) {
					// CTS line change
					GetCommModemStatus(hSerialPort,&LineStat);

					// Invert for CTS bit
					if (LineStat & MS_CTS_ON)
					{
						ACIA_Status &= ~MC6850_STATUS_CTS;
					}
					else
					{
						ACIA_Status |= MC6850_STATUS_CTS;
					}

					// Keep for TDRE bit
					if (LineStat & MS_CTS_ON)
					{
						ACIA_Status |= MC6850_STATUS_TDRE;
					}
					else
					{
						ACIA_Status &= ~MC6850_STATUS_TDRE;
					}
				}
			}

			bWaitingForStat = false;
		}
		else
		{
			Sleep(100); // Don't hog CPU if nothing happening
		}

		if (bSerialStateChanged && !bWaitingForData)
		{
			// Shut off serial port, and re-initialise
			InitThreads();
			bSerialStateChanged = false;
		}
		Sleep(0);
	} while(1);

	return 0;
}

static unsigned int __stdcall SerialThread(void * /* lpParam */)
{
	// New Serial port thread - 7:35pm 16/10/2001 GMT
	// This sorta runs as a seperate process in effect, checking
	// enable status, and doing the monitoring.
	do {
		if (!bSerialStateChanged && SerialPortEnabled && !TouchScreenEnabled && !EthernetPortEnabled && bWaitingForData) {
			DWORD spResult = WaitForSingleObject(olSerialPort.hEvent, INFINITE); // 10ms to respond
			if (spResult==WAIT_OBJECT_0) {
				if (GetOverlappedResult(hSerialPort,&olSerialPort,&BytesIn,FALSE)) {
					// sucessful read, screw any errors.
					if (SerialChannel == SerialDevice::RS423 && BytesIn > 0) {
						HandleData((unsigned char)SerialBuffer);
					}

					if (BytesIn==0) {
						bCharReady = false;
						ClearCommError(hSerialPort, &dwClrCommError,NULL);
					}
					bWaitingForData = false;
				}
			}
		}
		else
		{
			Sleep(100); // Don't hog CPU if nothing happening
		}
		Sleep(0);
	} while (1);

	return 0;
}

void InitSerialPort()
{
	BOOL bPortStat;
	// Initialise COM port
	if (SerialPortEnabled && SerialPort > 0) {
		if (SerialPort==1) pnSerialPort="Com1";
		if (SerialPort==2) pnSerialPort="Com2";
		if (SerialPort==3) pnSerialPort="Com3";
		if (SerialPort==4) pnSerialPort="Com4";
		hSerialPort=CreateFile(pnSerialPort,GENERIC_READ|GENERIC_WRITE,0,0,OPEN_EXISTING,FILE_FLAG_OVERLAPPED,0);
		if (hSerialPort==INVALID_HANDLE_VALUE) {
			mainWin->Report(MessageType::Error, "Could not open specified serial port");
			bSerialStateChanged = true;
			SerialPortEnabled = false;
			mainWin->UpdateSerialMenu();
		}
		else {
			bPortStat=SetupComm(hSerialPort, 1280, 1280);
			bPortStat=GetCommState(hSerialPort, &dcbSerialPort);
			dcbSerialPort.fBinary=TRUE;
			dcbSerialPort.BaudRate=9600;
			dcbSerialPort.Parity=NOPARITY;
			dcbSerialPort.StopBits=ONESTOPBIT;
			dcbSerialPort.ByteSize=8;
			dcbSerialPort.fDtrControl=DTR_CONTROL_DISABLE;
			dcbSerialPort.fOutxCtsFlow=FALSE;
			dcbSerialPort.fOutxDsrFlow=FALSE;
			dcbSerialPort.fOutX=FALSE;
			dcbSerialPort.fDsrSensitivity=FALSE;
			dcbSerialPort.fInX=FALSE;
			dcbSerialPort.fRtsControl=RTS_CONTROL_DISABLE; // Leave it low (do not send) for now
			dcbSerialPort.DCBlength=sizeof(dcbSerialPort);
			bPortStat=SetCommState(hSerialPort, &dcbSerialPort);
			ctSerialPort.ReadIntervalTimeout=MAXDWORD;
			ctSerialPort.ReadTotalTimeoutConstant=0;
			ctSerialPort.ReadTotalTimeoutMultiplier=0;
			ctSerialPort.WriteTotalTimeoutConstant=0;
			ctSerialPort.WriteTotalTimeoutMultiplier=0;
			SetCommTimeouts(hSerialPort,&ctSerialPort);
			SetCommMask(hSerialPort,EV_CTS | EV_RXCHAR | EV_ERR);
		}
	}
}

void CloseUEF(void) {
	if (UEFOpen) {
		TapeControlStopRecording(false);
		uef_close();
		UEFOpen = false;
		TxD=0;
		RxD=0;
		if (TapeControlEnabled)
			SendMessage(hwndMap, LB_RESETCONTENT, 0, 0);
	}
}

void SerialInit()
{
	InitThreads();

	hSerialThread = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, SerialThread, nullptr, 0, nullptr));
	hStatThread = reinterpret_cast<HANDLE>(_beginthreadex(nullptr, 0, StatThread, nullptr, 0, nullptr));
}

void Kill_Serial()
{
	CloseUEF();
	CloseCSW();

	if (SerialPortOpen)
	{
		CloseHandle(hSerialPort);
		hSerialPort = INVALID_HANDLE_VALUE;
	}
}

bool LoadUEFTape(const char *UEFName) {
	CloseUEF();

	strcpy(UEFTapeName, UEFName);

	if (TapeControlEnabled)
		TapeControlOpenFile(UEFName);

	// Clock values:
	// 5600 - Normal speed - anything higher is a bit slow
	// 750 - Recommended minium settings, fastest reliable load
	uef_setclock(TapeClockSpeed);
	SetUnlockTape(UnlockTape);

	bool Success = uef_open(UEFName);

	if (Success) {
		UEFOpen = true;
		UEF_BUF=0;
		TxD=0;
		RxD=0;
		TapeClock=0;
		OldClock=0;
		SetTrigger(TAPECYCLES, TapeTrigger);
		TapeControlUpdateCounter(TapeClock);
	}
	else {
		UEFTapeName[0]=0;
	}

	return Success;
}

void RewindTape()
{
	TapeControlStopRecording(true);
	UEF_BUF=0;
	TapeClock=0;
	OldClock=0;
	SetTrigger(TAPECYCLES, TapeTrigger);
	TapeControlUpdateCounter(TapeClock);

	csw_state = CSWState::WaitingForTone;
	csw_bit = 0;
	csw_pulselen = 0;
	csw_ptr = 0;
	csw_pulsecount = -1;
}

void SetTapeSpeed(int speed) {
	int NewClock = (int)((double)TapeClock * ((double)speed / TapeClockSpeed));
	TapeClockSpeed=speed;
	if (UEFOpen)
		LoadUEFTape(UEFTapeName);
	TapeClock=NewClock;
}

void SetUnlockTape(bool unlock) {
	uef_setunlock(unlock);
}

//*******************************************************************

bool map_file(const char *file_name)
{
	bool done=false;
	int i;
	int start_time;
	int n;
	int data;
	int last_data;
	int blk = 0;
	int blk_num;
	char block[500];
	bool std_last_block=true;
	char name[11];

	uef_setclock(TapeClockSpeed);

	int file = uef_open(file_name);
	if (file == 0)
	{
		return(false);
	}

	i=0;
	start_time=0;
	map_lines=0;
	last_data=0;
	blk_num=0;

	memset(map_desc, 0, sizeof(map_desc));
	memset(map_time, 0, sizeof(map_time));

	while (!done && map_lines < MAX_MAP_LINES)
	{
		data = uef_getdata(i);
		if (data != last_data)
		{
			if (UEFRES_TYPE(data) != UEFRES_TYPE(last_data))
			{
				if (UEFRES_TYPE(last_data) == UEF_DATA)
				{
					// End of block, standard header?
					if (blk > 20 && block[0] == 0x2A)
					{
						if (!std_last_block)
						{
							// Change of block type, must be first block
							blk_num=0;
							if (map_lines > 0 && map_desc[map_lines-1][0] != 0)
							{
								strcpy(map_desc[map_lines], "");
								map_time[map_lines]=start_time;
								map_lines++;
							}
						}

						// Pull file name from block
						n = 1;
						while (block[n] != 0 && block[n] >= 32 && block[n] <= 127 && n <= 10)
						{
							name[n-1] = block[n];
							n++;
						}
						name[n-1] = 0;
						if (name[0] != 0)
							sprintf(map_desc[map_lines], "%-12s %02X  Length %04X", name, blk_num, blk);
						else
							sprintf(map_desc[map_lines], "<No name>    %02X  Length %04X", blk_num, blk);

						map_time[map_lines]=start_time;

						// Is this the last block for this file?
						if (block[strlen(name) + 14] & 0x80)
						{
							blk_num=-1;
							++map_lines;
							strcpy(map_desc[map_lines], "");
							map_time[map_lines]=start_time;
						}
						std_last_block=true;
					}
					else
					{
						sprintf(map_desc[map_lines], "Non-standard %02X  Length %04X", blk_num, blk);
						map_time[map_lines]=start_time;
						std_last_block=false;
					}

					// Replace time counter in previous blank lines
					if (map_lines > 0 && map_desc[map_lines-1][0] == 0)
						map_time[map_lines-1]=start_time;

					// Data block recorded
					map_lines++;
					blk_num++;
				}

				if (UEFRES_TYPE(data) == UEF_HTONE)
				{
					// strcpy(map_desc[map_lines++], "Tone");
					start_time=i;
				}
				else if (UEFRES_TYPE(data) == UEF_GAP)
				{
					if (map_lines > 0 && map_desc[map_lines-1][0] != 0)
						strcpy(map_desc[map_lines++], "");
					start_time=i;
					blk_num=0;
				}
				else if (UEFRES_TYPE(data) == UEF_DATA)
				{
					blk=0;
					block[blk++]=UEFRES_BYTE(data);
				}
				else if (UEFRES_TYPE(data) == UEF_EOF)
				{
					done=true;
				}
			}
			else
			{
				if (UEFRES_TYPE(data) == UEF_DATA)
				{
					if (blk < 500)
						block[blk++]=UEFRES_BYTE(data);
					else
						blk++;
				}
			}
		}
		last_data=data;
		i++;
	}

	uef_close();

	return(true);
}

//*******************************************************************

INT_PTR CALLBACK TapeControlDlgProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam);

void TapeControlOpenDialog(HINSTANCE hinst, HWND /* hwndMain */)
{
	int Clock;

	TapeControlEnabled = true;

	if (!IsWindow(hwndTapeControl)) {
		hwndTapeControl = CreateDialog(hinst, MAKEINTRESOURCE(IDD_TAPECONTROL),
		                               NULL, TapeControlDlgProc);
		hCurrentDialog = hwndTapeControl;
		ShowWindow(hwndTapeControl, SW_SHOW);

		hwndMap = GetDlgItem(hwndTapeControl, IDC_TCMAP);
		SendMessage(hwndMap, WM_SETFONT, (WPARAM)GetStockObject(ANSI_FIXED_FONT),
		            (LPARAM)MAKELPARAM(FALSE,0));

		if (UEFOpen)
		{
			Clock = TapeClock;
			LoadUEFTape(UEFTapeName);
			TapeClock = Clock;
			TapeControlUpdateCounter(TapeClock);
		}

		if (CSWOpen)
		{
			Clock = csw_ptr;
			LoadCSW(UEFTapeName);
			csw_ptr = Clock;
			TapeControlUpdateCounter(csw_ptr);
		}
	}
}

void TapeControlCloseDialog()
{
	DestroyWindow(hwndTapeControl);
	hwndTapeControl = NULL;
	hwndMap = NULL;
	TapeControlEnabled = false;
	hCurrentDialog = NULL;
	map_lines = 0;
	TapePlaying=true;
	TapeRecording=false;
}

void TapeControlOpenFile(const char *UEFName)
{
	if (TapeControlEnabled)
	{
		if (!CSWOpen)
		{
			if (!map_file(UEFName))
			{
				mainWin->Report(MessageType::Error,
				                "Cannot open UEF file:\n  %s", UEFName);

				return;
			}
		}

		SendMessage(hwndMap, LB_RESETCONTENT, 0, 0);

		for (int i = 0; i < map_lines; ++i)
			SendMessage(hwndMap, LB_ADDSTRING, 0, (LPARAM)map_desc[i]);

		TapeControlUpdateCounter(0);
	}
}

void TapeControlUpdateCounter(int tape_time)
{
	if (TapeControlEnabled)
	{
		int i = 0;
		while (i < map_lines && map_time[i] <= tape_time)
			i++;

		if (i > 0)
			i--;

		SendMessage(hwndMap, LB_SETCURSEL, (WPARAM)i, 0);
	}
}

INT_PTR CALLBACK TapeControlDlgProc(HWND /* hwndDlg */, UINT message, WPARAM wParam, LPARAM /* lParam */)
{
	switch (message)
	{
		case WM_INITDIALOG:
			return TRUE;

		case WM_ACTIVATE:
			if (LOWORD(wParam) == WA_INACTIVE)
				hCurrentDialog = NULL;
			else
				hCurrentDialog = hwndTapeControl;
			return FALSE;

		case WM_COMMAND:
			switch (LOWORD(wParam))
			{
				case IDC_TCMAP:
					if (HIWORD(wParam) == LBN_SELCHANGE)
					{
						LRESULT s = SendMessage(hwndMap, LB_GETCURSEL, 0, 0);

						if (s != LB_ERR && s >= 0 && s < map_lines)
						{
							if (CSWOpen)
							{
								csw_ptr = map_time[s];
							}
							else
							{
								TapeClock=map_time[s];
							}
							OldClock=0;
							SetTrigger(TAPECYCLES, TapeTrigger);
						}
					}
					return FALSE;

				case IDC_TCPLAY:
					TapePlaying = true;
					TapeControlStopRecording(true);
					TapeAudio.Enabled = CassetteRelay;
					return TRUE;

				case IDC_TCSTOP:
					TapePlaying = false;
					TapeControlStopRecording(true);
					TapeAudio.Enabled = false;
					return TRUE;

				case IDC_TCEJECT:
					TapeControlStopRecording(false);
					TapeAudio.Enabled = false;
					CloseUEF();
					CloseCSW();
					return TRUE;

				case IDC_TCRECORD:
					if (CSWOpen) break;
					if (!TapeRecording)
					{
						bool Continue = false;

						if (UEFOpen)
						{
							if (mainWin->Report(MessageType::Confirm,
							                    "Append to current tape file:\n  %s",
							                    UEFTapeName) == MessageResult::OK)
							{
								SendMessage(hwndMap, LB_SETCURSEL, (WPARAM)map_lines-1, 0);

								Continue = true;
							}
						}
						else
						{
							// Query for new file name
							CloseUEF();
							mainWin->NewTapeImage(UEFTapeName);
							if (UEFTapeName[0])
							{
								Continue = true;

								FILE *fd = fopen(UEFTapeName,"rb");
								if (fd != NULL)
								{
									fclose(fd);

									if (mainWin->Report(MessageType::Question,
									                    "File already exists:\n  %s\n\nOverwrite file?",
									                    UEFTapeName) != MessageResult::Yes)
									{
										Continue = false;
									}
								}

								if (Continue)
								{
									// Create file
									if (uef_create(UEFTapeName))
									{
										UEFOpen = true;
									}
									else
									{
										mainWin->Report(MessageType::Error,
										                "Error creating tape file:\n  %s", UEFTapeName);

										UEFTapeName[0] = 0;
										Continue = false;
									}
								}
							}
						}

						if (Continue)
						{
							TapeRecording = true;
							TapePlaying = false;
							TapeAudio.Enabled = CassetteRelay;
						}
					}
					return TRUE;

				case IDCANCEL:
					TapeControlCloseDialog();
					return TRUE;
			}
	}
	return FALSE;
}

void TapeControlStopRecording(bool RefreshControl)
{
	if (TapeRecording)
	{
		uef_putdata(UEF_EOF, 0);
		TapeRecording = false;

		if (RefreshControl)
		{
			LoadUEFTape(UEFTapeName);
		}
	}
}

//*******************************************************************

void SaveSerialUEF(FILE *SUEF)
{
	if (UEFOpen)
	{
		fput16(0x0473,SUEF);
		fput32(293,SUEF);
		fputc(static_cast<int>(SerialChannel),SUEF);
		fwrite(UEFTapeName,1,256,SUEF);
		fputc(CassetteRelay,SUEF);
		fput32(Tx_Rate,SUEF);
		fput32(Rx_Rate,SUEF);
		fputc(Clk_Divide,SUEF);
		fputc(Parity,SUEF);
		fputc(StopBits,SUEF);
		fputc(DataBits,SUEF);
		fputc(RIE,SUEF);
		fputc(TIE,SUEF);
		fputc(TxD,SUEF);
		fputc(RxD,SUEF);
		fputc(RDR,SUEF);
		fputc(TDR,SUEF);
		fputc(RDSR,SUEF);
		fputc(TDSR,SUEF);
		fputc(ACIA_Status,SUEF);
		fputc(ACIA_Control,SUEF);
		fputc(SerialULAControl,SUEF);
		fputc(DCD,SUEF);
		fputc(DCDI,SUEF);
		fputc(ODCDI,SUEF);
		fputc(DCDClear,SUEF);
		fput32(TapeClock,SUEF);
		fput32(TapeClockSpeed,SUEF);
	}
}

void LoadSerialUEF(FILE *SUEF)
{
	char FileName[256];
	int sp;

	CloseUEF();

	SerialChannel = static_cast<SerialDevice>(fgetc(SUEF));
	fread(FileName,1,256,SUEF);
	if (FileName[0])
	{
		LoadUEFTape(FileName);
		if (!UEFOpen)
		{
			if (!TapeControlEnabled)
			{
				mainWin->Report(MessageType::Error,
				                "Cannot open UEF file:\n  %s", FileName);
			}
		}
		else
		{
			CassetteRelay = fgetbool(SUEF);
			Tx_Rate = fget32(SUEF);
			Rx_Rate = fget32(SUEF);
			Clk_Divide = fget8(SUEF);
			Parity = fget8(SUEF);
			StopBits = fget8(SUEF);
			DataBits = fget8(SUEF);
			RIE = fgetbool(SUEF);
			TIE = fgetbool(SUEF);
			TxD = fget8(SUEF);
			RxD = fget8(SUEF);
			RDR = fget8(SUEF);
			TDR = fget8(SUEF);
			RDSR = fget8(SUEF);
			TDSR = fget8(SUEF);
			ACIA_Status = fget8(SUEF);
			ACIA_Control = fget8(SUEF);
			SerialULAControl = fget8(SUEF);
			DCD = fget8(SUEF);
			DCDI = fget8(SUEF);
			ODCDI = fget8(SUEF);
			DCDClear = fget8(SUEF);
			TapeClock=fget32(SUEF);
			sp=fget32(SUEF);
			if (sp != TapeClockSpeed)
			{
				TapeClock = (int)((double)TapeClock * ((double)TapeClockSpeed / sp));
			}
			TapeControlUpdateCounter(TapeClock);
		}
	}
}
