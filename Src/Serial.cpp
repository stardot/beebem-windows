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
#include <string>

#include "6502core.h"
#include "uef.h"
#include "main.h"
#include "Serial.h"
#include "beebsound.h"
#include "beebwin.h"
#include "beebemrc.h"
#include "debug.h"
#include "uefstate.h"
#include "csw.h"
#include "SerialDevices.h"
#include "debug.h"
#include "log.h"
#include "TapeControlDialog.h"
#include "TapeMap.h"
#include "Thread.h"

// MC6850 control register bits
constexpr unsigned char MC6850_CONTROL_COUNTER_DIVIDE   = 0x03;
constexpr unsigned char MC6850_CONTROL_MASTER_RESET     = 0x03;
constexpr unsigned char MC6850_CONTROL_WORD_SELECT      = 0x1c;
constexpr unsigned char MC6850_CONTROL_TRANSMIT_CONTROL = 0x60;
constexpr unsigned char MC6850_CONTROL_RIE              = 0x80;

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

static bool RTS;
static bool FirstReset = true;
static bool DCD = false;
static bool DCDI = true;
static bool ODCDI = true;
static unsigned char DCDClear = 0; // count to clear DCD bit

static unsigned char Parity, StopBits, DataBits;

bool RIE, TIE; // Receive Interrupt Enable and Transmit Interrupt Enable

unsigned char TxD,RxD; // Transmit and Receive destinations (data or shift register)

static UEFFileWriter UEFWriter;
static char TapeFileName[256]; // Filename of current tape file

static UEFFileReader UEFReader;
static bool UEFFileOpen = false;
static bool CSWFileOpen = false;

static bool TapePlaying = true;
bool TapeRecording = false;

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

static const unsigned int Baud_Rates[8] =
{
	19200, 1200, 4800, 150, 9600, 300, 2400, 75
};

bool OldRelayState = false;
CycleCountT TapeTrigger=CycleCountTMax;
constexpr int TAPECYCLES = 2000000 / 5600; // 5600 is normal tape speed

static int UEF_BUF=0,NEW_UEF_BUF=0;
static int TapeClock = 0;
static int OldClock = 0;
int TapeClockSpeed = 5600;
bool UnlockTape = true;

// This bit is the Serial Port stuff
bool SerialPortEnabled;
char SerialPortName[_MAX_PATH];

class SerialPortReadThread : public Thread
{
	public:
		virtual unsigned int ThreadFunc();
};

class SerialPortStatusThread : public Thread
{
	public:
		virtual unsigned int ThreadFunc();
};

SerialPortReadThread SerialReadThread;
SerialPortStatusThread SerialStatusThread;

static HANDLE hSerialPort = INVALID_HANDLE_VALUE; // Serial port handle
static unsigned int SerialBuffer=0,SerialWriteBuffer=0;
static DWORD BytesIn,BytesOut;
static DWORD dwCommEvent;
static OVERLAPPED olSerialPort={0},olSerialWrite={0},olStatus={0};
volatile bool bSerialStateChanged = false;
static volatile bool bWaitingForData = false;
static volatile bool bWaitingForStat = false;
static volatile bool bCharReady = false;

static void InitSerialPort();

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

		ACIA_Status &= ~MC6850_STATUS_DCD;
		DCD = false;
		DCDI = false;
		DCDClear = 0;

		TxD = 0;
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
	if (SerialChannel == SerialDevice::RS423 && SerialDestination == SerialType::SerialPort)
	{
		DCB dcbSerialPort{}; // Serial port device control block
		dcbSerialPort.DCBlength = sizeof(dcbSerialPort);

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

	// WriteLog("Serial: Write ACIA Tx %02X, SerialChannel = %d\n", (int)Data, SerialChannel);

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

			if (SerialDestination == SerialType::TouchScreen)
			{
				TouchScreenWrite(Data);
			}
			else if (SerialDestination == SerialType::IP232)
			{
				// if (Data_Bits == 7) {
				//	IP232Write(Data & 127);
				// } else {
					IP232Write(Data);
					if (!IP232Raw && Data == 255) IP232Write(Data);
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

		DCB dcbSerialPort{}; // Serial port device control block
		dcbSerialPort.DCBlength = sizeof(dcbSerialPort);

		GetCommState(hSerialPort, &dcbSerialPort);
		dcbSerialPort.BaudRate  = HigherRate;
		dcbSerialPort.DCBlength = sizeof(dcbSerialPort);
		SetCommState(hSerialPort, &dcbSerialPort);
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
//	if (!DCDI && DCD)
//	{
//		DCDClear++;
//		if (DCDClear > 1) {
//			DCD = false;
//			ACIA_Status &= ~(1 << MC6850_STATUS_DCS);
//			DCDClear = 0;
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

static void HandleData(unsigned char Data)
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
//	if (!DCDI && DCD)
//	{
//		DCDClear++;
//		if (DCDClear > 1) {
//			DCD = false;
//			ACIA_Status &= ~(1 << MC6850_STATUS_DCS);
//			DCDClear = 0;
//		}
//	}

	ACIA_Status &= ~MC6850_STATUS_IRQ;
	intStatus &= ~(1 << serial);

	unsigned char Data = RDR;
	RDR = RDSR;
	RDSR = 0;

	if (RxD > 0)
	{
		RxD--;
	}

	if (RxD == 0)
	{
		ACIA_Status &= ~MC6850_STATUS_RDRF;
	}

	if (RxD > 0 && RIE)
	{
		ACIA_Status |= MC6850_STATUS_IRQ;
		intStatus |= 1 << serial;
	}

	if (DataBits == 7)
	{
		Data &= 127;
	}

	if (DebugEnabled)
	{
		DebugDisplayTraceF(DebugType::Serial, true,
		                   "Serial: Read ACIA Rx %02X", (int)Data);
	}

	// WriteLog("Serial: Read ACIA Rx %02X, ACIA_Status = %02x\n", (int)Data, (int)ACIA_Status);

	return Data;
}

void SerialPoll()
{
	if (SerialChannel == SerialDevice::Cassette)
	{
		if (TapeRecording)
		{
			if (CassetteRelay && UEFFileOpen && TotalCycles >= TapeTrigger)
			{
				if (TxD > 0)
				{
					// Writing data
					if (UEFWriter.PutData(TDR | UEF_DATA, TapeClock) != UEFResult::Success)
					{
						mainWin->Report(MessageType::Error,
						                "Error writing to UEF file:\n  %s", TapeFileName);

						SerialStopTapeRecording(true);
					}

					TxD = 0;
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
					if (UEFWriter.PutData(UEF_CARRIER_TONE, TapeClock) != UEFResult::Success)
					{
						mainWin->Report(MessageType::Error,
						                "Error writing to UEF file:\n  %s", TapeFileName);

						SerialStopTapeRecording(true);
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

			if (TxD > 0 && TotalCycles >= TapeTrigger)
			{
				// WriteLog("Ignoring Writes\n");

				TxD = 0;
				ACIA_Status |= MC6850_STATUS_TDRE;

				if (TIE)
				{
					ACIA_Status |= MC6850_STATUS_IRQ;
					intStatus |= 1 << serial;
				}
			}

			if (CassetteRelay && UEFFileOpen && TapeClock != OldClock)
			{
				NEW_UEF_BUF = UEFReader.GetData(TapeClock);
				OldClock = TapeClock;
			}

			if ((NEW_UEF_BUF != UEF_BUF || UEFRES_TYPE(NEW_UEF_BUF) == UEF_CARRIER_TONE || UEFRES_TYPE(NEW_UEF_BUF) == UEF_GAP) &&
			    CassetteRelay && UEFFileOpen)
			{
				if (UEFRES_TYPE(UEF_BUF) != UEFRES_TYPE(NEW_UEF_BUF))
					TapeControlUpdateCounter(TapeClock);

				UEF_BUF = NEW_UEF_BUF;

				// New data read in, so do something about it
				if (UEFRES_TYPE(UEF_BUF) == UEF_CARRIER_TONE)
				{
					DCDI = true;
					TapeAudio.Signal = 2;
					// TapeAudio.Samples = 0;
					TapeAudio.BytePos = 11;
				}

				if (UEFRES_TYPE(UEF_BUF) == UEF_GAP)
				{
					DCDI = true;
					TapeAudio.Signal = 0;
				}

				if (UEFRES_TYPE(UEF_BUF) == UEF_DATA)
				{
					DCDI = false;
					HandleData(UEFRES_BYTE(UEF_BUF));
					TapeAudio.Data       = (UEFRES_BYTE(UEF_BUF) << 1) | 1;
					TapeAudio.BytePos    = 1;
					TapeAudio.CurrentBit = 0;
					TapeAudio.Signal     = 1;
					TapeAudio.ByteCount  = 3;
				}
			}

			if (CassetteRelay && RxD < 2 && UEFFileOpen)
			{
				if (TotalCycles >= TapeTrigger)
				{
					if (TapePlaying)
						TapeClock++;

					SetTrigger(TAPECYCLES, TapeTrigger);
				}
			}

			// CSW stuff

			if (CassetteRelay && CSWFileOpen && TapeClock != OldClock)
			{
				CSWState last_state = csw_state;

				int Data = CSWPoll();
				OldClock = TapeClock;

				if (last_state != csw_state)
					TapeControlUpdateCounter(csw_ptr);

				if (csw_state == CSWState::WaitingForTone)
				{
					DCDI = true;
					TapeAudio.Signal = 0;
				}

				// New data read in, so do something about it
				if (csw_state == CSWState::Tone)
				{
					DCDI = true;
					TapeAudio.Signal  = 2;
					TapeAudio.BytePos = 11;
				}

				if (Data >= 0 && csw_state == CSWState::Data)
				{
					DCDI = false;
					HandleData((unsigned char)Data);
					TapeAudio.Data       = (Data << 1) | 1;
					TapeAudio.BytePos    = 1;
					TapeAudio.CurrentBit = 0;
					TapeAudio.Signal     = 1;
					TapeAudio.ByteCount  = 3;
				}
			}

			if (CassetteRelay && RxD < 2 && CSWFileOpen)
			{
				if (TotalCycles >= TapeTrigger)
				{
					if (TapePlaying)
						TapeClock++;

					SetTrigger(CSWPollCycles, TapeTrigger);
				}
			}

			if (DCDI && !ODCDI)
			{
				// low to high transition on the DCD line
				if (RIE)
				{
					ACIA_Status |= MC6850_STATUS_IRQ;
					intStatus |= 1 << serial;
				}

				DCD = true;
				ACIA_Status |= MC6850_STATUS_DCD; // ACIA_Status &= ~MC6850_STATUS_RDRF;
				DCDClear = 0;
			}

			if (!DCDI && ODCDI)
			{
				DCD = false;
				ACIA_Status &= ~MC6850_STATUS_DCD;
				DCDClear = 0;
			}

			if (DCDI != ODCDI)
			{
				ODCDI = DCDI;
			}
		}
	}

	if (SerialChannel == SerialDevice::RS423 && SerialPortEnabled)
	{
		if (SerialDestination == SerialType::TouchScreen)
		{
			if (TouchScreenPoll())
			{
				if (RxD < 2)
				{
					HandleData(TouchScreenRead());
				}
			}
		}
		else if (SerialDestination == SerialType::IP232)
		{
			if (IP232Poll())
			{
				if (RxD < 2)
				{
					HandleData(IP232Read());
				}
			}
		}
		else
		{
			if (!bWaitingForStat && !bSerialStateChanged)
			{
				WaitCommEvent(hSerialPort, &dwCommEvent, &olStatus);
				bWaitingForStat = true;
			}

			if (!bSerialStateChanged && bCharReady && !bWaitingForData && RxD < 2)
			{
				if (!ReadFile(hSerialPort, &SerialBuffer, 1, &BytesIn, &olSerialPort))
				{
					if (GetLastError() == ERROR_IO_PENDING)
					{
						bWaitingForData = true;
					}
					else
					{
						mainWin->Report(MessageType::Error, "Serial Port Error");
					}
				}
				else
				{
					if (BytesIn > 0)
					{
						if (DebugEnabled)
						{
							DebugSerial((unsigned char)SerialBuffer);
						}

						HandleData((unsigned char)SerialBuffer);
					}
					else
					{
						DWORD CommError;
						ClearCommError(hSerialPort, &CommError, nullptr);
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

	if (SerialPortEnabled &&
	    SerialDestination == SerialType::SerialPort &&
	    SerialPortName[0] != '\0')
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

unsigned int SerialPortStatusThread::ThreadFunc()
{
	DWORD dwOvRes = 0;

	while (1)
	{
		if (SerialDestination == SerialType::SerialPort &&
		    SerialPortEnabled &&
		    (WaitForSingleObject(olStatus.hEvent, 10) == WAIT_OBJECT_0))
		{
			if (GetOverlappedResult(hSerialPort, &olStatus, &dwOvRes, FALSE))
			{
				// Event waiting in dwCommEvent
				if ((dwCommEvent & EV_RXCHAR) && !bWaitingForData)
				{
					bCharReady = true;
				}

				if (dwCommEvent & EV_CTS)
				{
					// CTS line change
					DWORD LineStat;
					GetCommModemStatus(hSerialPort, &LineStat);

					// Invert for CTS bit, Keep for TDRE bit
					if (LineStat & MS_CTS_ON)
					{
						ACIA_Status &= ~MC6850_STATUS_CTS;
						ACIA_Status |= MC6850_STATUS_TDRE;
					}
					else
					{
						ACIA_Status |= MC6850_STATUS_CTS;
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
	}

	return 0;
}

unsigned int SerialPortReadThread::ThreadFunc()
{
	// New Serial port thread - 7:35pm 16/10/2001 GMT
	// This sorta runs as a seperate process in effect, checking
	// enable status, and doing the monitoring.

	while (1)
	{
		if (!bSerialStateChanged && SerialPortEnabled && SerialDestination == SerialType::SerialPort && bWaitingForData)
		{
			DWORD Result = WaitForSingleObject(olSerialPort.hEvent, INFINITE); // 10ms to respond

			if (Result == WAIT_OBJECT_0)
			{
				if (GetOverlappedResult(hSerialPort, &olSerialPort, &BytesIn, FALSE))
				{
					// sucessful read, screw any errors.
					if (SerialChannel == SerialDevice::RS423 && BytesIn > 0)
					{
						if (DebugEnabled)
						{
							DebugSerial((unsigned char)SerialBuffer);
						}

						HandleData((unsigned char)SerialBuffer);
					}

					if (BytesIn == 0)
					{
						bCharReady = false;

						DWORD CommError;
						ClearCommError(hSerialPort, &CommError, nullptr);
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
	}

	return 0;
}

static void InitSerialPort()
{
	// Initialise COM port
	if (SerialPortEnabled &&
	    SerialDestination == SerialType::SerialPort &&
	    SerialPortName[0] != '\0')
	{
		char FileName[_MAX_PATH];
		sprintf(FileName, "\\\\.\\%s", SerialPortName);

		hSerialPort = CreateFile(FileName,
		                         GENERIC_READ | GENERIC_WRITE,
		                         0, // dwShareMode
		                         nullptr, // lpSecurityAttributes
		                         OPEN_EXISTING,
		                         FILE_FLAG_OVERLAPPED,
		                         nullptr); // hTemplateFile

		if (hSerialPort == INVALID_HANDLE_VALUE)
		{
			mainWin->Report(MessageType::Error, "Could not open serial port %s", SerialPortName);
			bSerialStateChanged = true;
			SerialPortEnabled = false;
			mainWin->UpdateSerialMenu();
		}
		else
		{
			BOOL bPortStat = SetupComm(hSerialPort, 1280, 1280);

			DCB dcbSerialPort{}; // Serial port device control block
			dcbSerialPort.DCBlength = sizeof(dcbSerialPort);

			bPortStat = GetCommState(hSerialPort, &dcbSerialPort);

			dcbSerialPort.fBinary         = TRUE;
			dcbSerialPort.BaudRate        = 9600;
			dcbSerialPort.Parity          = NOPARITY;
			dcbSerialPort.StopBits        = ONESTOPBIT;
			dcbSerialPort.ByteSize        = 8;
			dcbSerialPort.fDtrControl     = DTR_CONTROL_DISABLE;
			dcbSerialPort.fOutxCtsFlow    = FALSE;
			dcbSerialPort.fOutxDsrFlow    = FALSE;
			dcbSerialPort.fOutX           = FALSE;
			dcbSerialPort.fDsrSensitivity = FALSE;
			dcbSerialPort.fInX            = FALSE;
			dcbSerialPort.fRtsControl     = RTS_CONTROL_DISABLE; // Leave it low (do not send) for now

			bPortStat = SetCommState(hSerialPort, &dcbSerialPort);

			COMMTIMEOUTS CommTimeOuts{};
			CommTimeOuts.ReadIntervalTimeout         = MAXDWORD;
			CommTimeOuts.ReadTotalTimeoutConstant    = 0;
			CommTimeOuts.ReadTotalTimeoutMultiplier  = 0;
			CommTimeOuts.WriteTotalTimeoutConstant   = 0;
			CommTimeOuts.WriteTotalTimeoutMultiplier = 0;

			SetCommTimeouts(hSerialPort, &CommTimeOuts);

			SetCommMask(hSerialPort, EV_CTS | EV_RXCHAR | EV_ERR);
		}
	}
}

static void CloseUEFFile()
{
	if (UEFFileOpen)
	{
		SerialStopTapeRecording(false);
		UEFReader.Close();
		UEFFileOpen = false;
	}
}

static void CloseCSWFile()
{
	if (CSWFileOpen)
	{
		CSWClose();
		CSWFileOpen = false;
	}
}

void SerialInit()
{
	InitThreads();

	SerialReadThread.Start();
	SerialStatusThread.Start();
}

void SerialClose()
{
	CloseTape();

	if (hSerialPort != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hSerialPort);
		hSerialPort = INVALID_HANDLE_VALUE;
	}
}

CSWResult LoadCSWTape(const char *FileName)
{
	CloseTape();

	CSWResult Result = CSWOpen(FileName);

	if (Result == CSWResult::Success)
	{
		CSWFileOpen = true;
		strcpy(TapeFileName, FileName);

		TxD = 0;
		RxD = 0;
		TapeClock = 0;
		OldClock = 0;
		SetTrigger(CSWPollCycles, TapeTrigger);

		CSWCreateTapeMap(TapeMap);

		csw_ptr = 0;

		if (TapeControlEnabled)
		{
			TapeControlAddMapLines(csw_ptr);
		}
	}

	return Result;
}

UEFResult LoadUEFTape(const char *FileName)
{
	CloseTape();

	// Clock values:
	// 5600 - Normal speed - anything higher is a bit slow
	// 750 - Recommended minium settings, fastest reliable load
	UEFReader.SetClock(TapeClockSpeed);
	SetUnlockTape(UnlockTape);

	UEFResult Result = UEFReader.Open(FileName);

	if (Result == UEFResult::Success)
	{
		UEFFileOpen = true;
		strcpy(TapeFileName, FileName);

		UEF_BUF = 0;
		TxD = 0;
		RxD = 0;
		TapeClock = 0;
		OldClock = 0;
		SetTrigger(TAPECYCLES, TapeTrigger);

		int Clock = TapeClock;

		UEFReader.CreateTapeMap(TapeMap);

		TapeClock = Clock;

		if (TapeControlEnabled)
		{
			TapeControlAddMapLines(TapeClock);
		}

		TapeControlUpdateCounter(TapeClock);
	}

	return Result;
}

void CloseTape()
{
	CloseUEFFile();
	CloseCSWFile();

	TxD = 0;
	RxD = 0;

	TapeFileName[0] = '\0';

	if (TapeControlEnabled)
	{
		TapeControlCloseTape();
	}
}

void RewindTape()
{
	SerialStopTapeRecording(true);

	UEF_BUF = 0;
	TapeClock = 0;
	OldClock = 0;
	SetTrigger(TAPECYCLES, TapeTrigger);

	TapeControlUpdateCounter(TapeClock);

	csw_state = CSWState::WaitingForTone;
	csw_bit = 0;
	csw_pulselen = 0;
	csw_ptr = 0;
	csw_pulsecount = -1;
}

void SetTapeSpeed(int Speed)
{
	int NewClock = (int)((double)TapeClock * ((double)Speed / TapeClockSpeed));
	TapeClockSpeed = Speed;

	if (UEFFileOpen)
	{
		std::string FileName = TapeFileName;

		LoadUEFTape(FileName.c_str());
	}

	TapeClock = NewClock;
}

void SetUnlockTape(bool Unlock)
{
	UEFReader.SetUnlock(Unlock);
}

void SetTapePosition(int Time)
{
	if (CSWFileOpen)
	{
		csw_ptr = Time;
	}
	else
	{
		TapeClock = Time;
	}

	OldClock = 0;

	SetTrigger(TAPECYCLES, TapeTrigger);
}

void SerialPlayTape()
{
	TapePlaying = true;
	TapeAudio.Enabled = CassetteRelay;
}

bool SerialRecordTape(const char* FileName)
{
	// Create file
	if (UEFWriter.Open(FileName) == UEFResult::Success)
	{
		strcpy(TapeFileName, FileName);
		UEFFileOpen = true;

		TapeRecording = true;
		TapePlaying = false;
		TapeAudio.Enabled = CassetteRelay;
		return true;
	}
	else
	{
		TapeFileName[0] = '\0';
		return false;
	}
}

void SerialStopTape()
{
	TapePlaying = false;
	TapeAudio.Enabled = false;
}

void SerialStopTapeRecording(bool ReloadTape)
{
	if (TapeRecording)
	{
		UEFWriter.PutData(UEF_EOF, 0);
		UEFWriter.Close();

		TapeRecording = false;

		if (ReloadTape)
		{
			std::string FileName = TapeFileName;

			LoadUEFTape(FileName.c_str());
		}
	}
}

void SerialEjectTape()
{
	TapeAudio.Enabled = false;
	CloseTape();
}

int SerialGetTapeClock()
{
	if (UEFFileOpen)
	{
		return TapeClock;
	}
	else if (CSWFileOpen)
	{
		return csw_ptr;
	}
	else
	{
		return 0;
	}
}

//*******************************************************************

void SaveSerialUEF(FILE *SUEF)
{
	// TODO: Save/Restore state when CSW file is open

	if (UEFFileOpen)
	{
		fput16(0x0473,SUEF);
		fput32(293,SUEF);
		fputc(static_cast<int>(SerialChannel),SUEF);
		fwrite(TapeFileName,1,256,SUEF);
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
	CloseTape();

	SerialChannel = static_cast<SerialDevice>(fgetc(SUEF));

	char FileName[256];
	fread(FileName,1,256,SUEF);

	if (FileName[0])
	{
		if (LoadUEFTape(FileName) != UEFResult::Success)
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
			DCD = fgetbool(SUEF);
			DCDI = fgetbool(SUEF);
			ODCDI = fgetbool(SUEF);
			DCDClear = fget8(SUEF);
			TapeClock=fget32(SUEF);
			int Speed = fget32(SUEF);
			if (Speed != TapeClockSpeed)
			{
				TapeClock = (int)((double)TapeClock * ((double)TapeClockSpeed / Speed));
			}
			TapeControlUpdateCounter(TapeClock);
		}
	}
}
