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
// and https://stardot.org.uk/forums/viewtopic.php?t=22935

#include <windows.h>

#include <process.h>
#include <stdio.h>

#include <algorithm>
#include <string>

#include "Serial.h"
#include "6502core.h"
#include "BeebWin.h"
#include "Csw.h"
#include "Debug.h"
#include "DebugTrace.h"
#include "IP232.h"
#include "Log.h"
#include "Main.h"
#include "Resource.h"
#include "Sound.h"
#include "TapeControlDialog.h"
#include "TapeMap.h"
#include "Thread.h"
#include "TouchScreen.h"
#include "Uef.h"
#include "UEFState.h"

// MC6850 control register bits
constexpr unsigned char MC6850_CONTROL_COUNTER_DIVIDE   = 0x03;
constexpr unsigned char MC6850_CONTROL_MASTER_RESET     = 0x03;
constexpr unsigned char MC6850_CONTROL_WORD_SELECT      = 0x1c;
constexpr unsigned char MC6850_CONTROL_TRANSMIT_CONTROL = 0x60;
constexpr unsigned char MC6850_CONTROL_RIE              = 0x80;

SerialType SerialDestination;

static bool FirstReset = true;

static UEFFileReader UEFReader;
static UEFFileWriter UEFWriter;

static char TapeFileName[256]; // Filename of current tape file

static bool UEFFileOpen = false;
static bool CSWFileOpen = false;

TapeStateType TapeState;

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

CycleCountT TapeTrigger = CycleCountTMax;
constexpr int TAPECYCLES = CPU_CYCLES_PER_SECOND / 5600; // 5600 is normal tape speed

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
volatile bool bSerialStateChanged = false;

class Win32SerialPort
{
	public:
		Win32SerialPort();

		bool Init(const char* PortName);
		void Close();
		void SetBaudRate(int BaudRate);
		void Configure(unsigned char DataBits, unsigned char StopBits, unsigned char Parity);
		void SetRTS(bool RTS);

	public:
		HANDLE hSerialPort; // Serial port handle
		unsigned int SerialBuffer;
		unsigned int SerialWriteBuffer;
		DWORD BytesIn;
		DWORD BytesOut;
		DWORD dwCommEvent;
		OVERLAPPED olSerialPort;
		OVERLAPPED olSerialWrite;
		OVERLAPPED olStatus;
		volatile bool bWaitingForData;
		volatile bool bWaitingForStat;
		volatile bool bCharReady;
};

static Win32SerialPort SerialPort;

static void InitSerialPort();

SerialACIAType SerialACIA;

struct SerialULAType
{
	unsigned char Control; // Serial ULA / SERPROC control register
	bool CassetteRelay; // Cassette relay state
	bool RS423; // Device in use (RS423 or cassette)
	bool TapeCarrier; // Carrier high tone detected
	int CarrierCycleCount; // Countdown timer to assert DCD when carrier detected
};

static SerialULAType SerialULA;

/*--------------------------------------------------------------------------*/

Win32SerialPort::Win32SerialPort() :
	hSerialPort(INVALID_HANDLE_VALUE),
	SerialBuffer(0),
	SerialWriteBuffer(0),
	BytesIn(0),
	BytesOut(0),
	dwCommEvent(0),
	bWaitingForData(false),
	bWaitingForStat(false),
	bCharReady(false)
{
	memset(&olSerialPort, 0, sizeof(olSerialPort));
	memset(&olSerialWrite, 0, sizeof(olSerialWrite));
	memset(&olStatus, 0, sizeof(olStatus));
}

/*--------------------------------------------------------------------------*/

bool Win32SerialPort::Init(const char* PortName)
{
	char FileName[_MAX_PATH];
	sprintf(FileName, "\\\\.\\%s", PortName);

	SerialPort.hSerialPort = CreateFile(FileName,
	                                    GENERIC_READ | GENERIC_WRITE,
	                                    0, // dwShareMode
	                                    nullptr, // lpSecurityAttributes
	                                    OPEN_EXISTING,
	                                    FILE_FLAG_OVERLAPPED,
	                                    nullptr); // hTemplateFile

	if (SerialPort.hSerialPort == INVALID_HANDLE_VALUE)
	{
		return false;
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

		return true;
	}
}

/*--------------------------------------------------------------------------*/

void Win32SerialPort::Close()
{
	if (hSerialPort != INVALID_HANDLE_VALUE)
	{
		CloseHandle(hSerialPort);
		hSerialPort = INVALID_HANDLE_VALUE;
	}
}

/*--------------------------------------------------------------------------*/

void Win32SerialPort::SetBaudRate(int BaudRate)
{
	DCB dcbSerialPort{}; // Serial port device control block
	dcbSerialPort.DCBlength = sizeof(dcbSerialPort);

	GetCommState(hSerialPort, &dcbSerialPort);
	dcbSerialPort.BaudRate  = BaudRate;
	dcbSerialPort.DCBlength = sizeof(dcbSerialPort);
	SetCommState(hSerialPort, &dcbSerialPort);
}

/*--------------------------------------------------------------------------*/

void Win32SerialPort::Configure(unsigned char DataBits, unsigned char StopBits, unsigned char Parity)
{
	DCB dcbSerialPort{}; // Serial port device control block
	dcbSerialPort.DCBlength = sizeof(dcbSerialPort);

	GetCommState(hSerialPort, &dcbSerialPort);

	dcbSerialPort.ByteSize  = DataBits;
	dcbSerialPort.StopBits  = (StopBits == 1) ? ONESTOPBIT : TWOSTOPBITS;
	dcbSerialPort.Parity    = Parity;
	dcbSerialPort.DCBlength = sizeof(dcbSerialPort);

	SetCommState(hSerialPort, &dcbSerialPort);
}

/*--------------------------------------------------------------------------*/

void Win32SerialPort::SetRTS(bool RTS)
{
	EscapeCommFunction(hSerialPort, RTS ? CLRRTS : SETRTS);
}

/*--------------------------------------------------------------------------*/

void SerialACIAWriteControl(unsigned char Value)
{
	if (DebugEnabled)
	{
		DebugDisplayTraceF(DebugType::Serial, true,
		                   "Serial: Write ACIA control %02X", (int)Value);
	}

	SerialACIA.Control = Value; // This is done for safe keeping

	// Master reset - clear all bits in the status register, except for
	// external conditions on CTS and DCD.
	if ((Value & MC6850_CONTROL_COUNTER_DIVIDE) == MC6850_CONTROL_MASTER_RESET)
	{
		SerialACIA.Status &= MC6850_STATUS_CTS;
		SerialACIA.Status &= ~MC6850_STATUS_DCD;

		// Master reset clears IRQ
		SerialACIA.Status &= ~MC6850_STATUS_IRQ;
		intStatus &= ~(1 << serial);

		if (FirstReset)
		{
			// RTS High on first Master reset.
			SerialACIA.Status |= MC6850_STATUS_CTS;
			FirstReset = false;
			SerialACIA.RTS = true;
		}

		SerialACIA.TxD = 0;
		SerialACIA.Status |= MC6850_STATUS_TDRE; // Transmit data register empty

		SetTrigger(TAPECYCLES, TapeTrigger);
	}

	// Clock Divide
	if ((Value & MC6850_CONTROL_COUNTER_DIVIDE) == 0x00) SerialACIA.ClkDivide = 1;
	if ((Value & MC6850_CONTROL_COUNTER_DIVIDE) == 0x01) SerialACIA.ClkDivide = 16;
	if ((Value & MC6850_CONTROL_COUNTER_DIVIDE) == 0x02) SerialACIA.ClkDivide = 64;

	// Word select
	SerialACIA.Parity   = WordSelect[(Value & MC6850_CONTROL_WORD_SELECT) >> 2].Parity;
	SerialACIA.StopBits = WordSelect[(Value & MC6850_CONTROL_WORD_SELECT) >> 2].StopBits;
	SerialACIA.DataBits = WordSelect[(Value & MC6850_CONTROL_WORD_SELECT) >> 2].DataBits;

	// Transmitter control
	SerialACIA.RTS = TransmitterControl[(Value & MC6850_CONTROL_TRANSMIT_CONTROL) >> 5].RTS;
	SerialACIA.TIE = TransmitterControl[(Value & MC6850_CONTROL_TRANSMIT_CONTROL) >> 5].TIE;
	SerialACIA.RIE = (Value & MC6850_CONTROL_RIE) != 0;

	// Seem to need an interrupt immediately for tape writing when TIE set
	if (!SerialULA.RS423 && SerialACIA.TIE && SerialULA.CassetteRelay)
	{
		SerialACIA.Status |= MC6850_STATUS_IRQ;
		intStatus |= 1 << serial;
	}

	// Change serial port settings
	if (SerialULA.RS423 && SerialDestination == SerialType::SerialPort)
	{
		SerialPort.Configure(SerialACIA.DataBits, SerialACIA.StopBits, SerialACIA.Parity);

		// Check RTS
		SerialPort.SetRTS(SerialACIA.RTS);
	}
}

/*--------------------------------------------------------------------------*/

void SerialACIAWriteTxData(unsigned char Data)
{
	if (DebugEnabled)
	{
		DebugDisplayTraceF(DebugType::Serial, true,
		                   "Serial: Write ACIA Tx %02X", (int)Data);
	}

	// WriteLog("Serial: Write ACIA Tx %02X, SerialChannel = %d\n", (int)Data, SerialChannel);

	SerialACIA.Status &= ~MC6850_STATUS_IRQ;
	intStatus &= ~(1 << serial);

	// 10/09/06
	// JW - A bug in swarm loader overwrites the rs423 output buffer counter
	// Unless we do something with the data, the loader hangs so just swallow it (see below)

	if (!SerialULA.RS423 || (SerialULA.RS423 && !SerialPortEnabled))
	{
		SerialACIA.TDR = Data;
		SerialACIA.TxD = 1;
		SerialACIA.Status &= ~MC6850_STATUS_TDRE;
		int Baud = SerialACIA.TxRate * ((SerialACIA.ClkDivide == 1) ? 64 : (SerialACIA.ClkDivide == 64) ? 1 : 4);

		SetTrigger(2000000 / (Baud / 8) * TapeState.ClockSpeed / 5600, TapeTrigger);
	}

	if (SerialULA.RS423 && SerialPortEnabled)
	{
		if (SerialACIA.Status & MC6850_STATUS_TDRE)
		{
			SerialACIA.Status &= ~MC6850_STATUS_TDRE;
			SerialPort.SerialWriteBuffer = Data;

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
				WriteFile(SerialPort.hSerialPort, &SerialPort.SerialWriteBuffer, 1, &SerialPort.BytesOut, &SerialPort.olSerialWrite);
			}

			SerialACIA.Status |= MC6850_STATUS_TDRE;
		}
	}
}

/*--------------------------------------------------------------------------*/

// The Serial ULA control register controls the cassette motor relay,
// transmit and receive baud rates, and RS423/cassette switch

void SerialULAWrite(unsigned char Value)
{
	if (DebugEnabled)
	{
		DebugDisplayTraceF(DebugType::Serial, true,
		                   "Serial: Write serial ULA %02X", (int)Value);
	}

	SerialULA.Control = Value;

	// Slightly easier this time.
	// just the Rx and Tx baud rates, and the selectors.
	bool CassetteRelay = SerialULA.CassetteRelay;

	SerialULA.CassetteRelay = (Value & 0x80) != 0;
	TapeAudio.Enabled = SerialULA.CassetteRelay && (TapeState.Playing || TapeState.Recording);
	LEDs.Motor = SerialULA.CassetteRelay;

	if (SerialULA.CassetteRelay)
	{
		SetTrigger(TAPECYCLES, TapeTrigger);
	}

	if (SerialULA.CassetteRelay != CassetteRelay)
	{
		ClickRelay(SerialULA.CassetteRelay);
	}

	SerialULA.RS423 = (Value & 0x40) != 0;
	SerialACIA.TxRate = Baud_Rates[(Value & 0x07)];
	SerialACIA.RxRate = Baud_Rates[(Value & 0x38) >> 3];

	// Note, the PC serial port (or at least Win32) does not allow different
	// transmit/receive rates, so we will use the higher of the two.

	if (SerialULA.RS423)
	{
		unsigned int BaudRate = std::max(SerialACIA.RxRate, SerialACIA.TxRate);

		SerialPort.SetBaudRate(BaudRate);
	}
}

/*--------------------------------------------------------------------------*/

unsigned char SerialULARead()
{
	if (DebugEnabled)
	{
		DebugDisplayTraceF(DebugType::Serial, true,
		                   "Serial: Read serial ULA %02X", (int)SerialULA.Control);
	}

	return SerialULA.Control;
}

/*--------------------------------------------------------------------------*/

unsigned char SerialACIAReadStatus()
{
	if (DebugEnabled)
	{
		DebugDisplayTraceF(DebugType::Serial, true,
		                   "Serial: Read ACIA status %02X", (int)SerialACIA.Status);
	}

	// WriteLog("Serial: Read ACIA status %02X\n", (int)ACIA_Status);

	// See https://github.com/stardot/beebem-windows/issues/47
	return SerialACIA.Status;
}

/*--------------------------------------------------------------------------*/

static void HandleData(unsigned char Data)
{
	// This proc has to dump data into the serial chip's registers

	SerialACIA.Status &= ~MC6850_STATUS_OVRN;

	if (SerialACIA.RxD == 0)
	{
		SerialACIA.RDR = Data;
		SerialACIA.Status |= MC6850_STATUS_RDRF; // Rx Reg full
		SerialACIA.RxD++;
	}
	else if (SerialACIA.RxD == 1)
	{
		SerialACIA.RDSR = Data;
		SerialACIA.Status |= MC6850_STATUS_RDRF;
		SerialACIA.RxD++;
	}
	else if (SerialACIA.RxD == 2)
	{
		SerialACIA.RDR = SerialACIA.RDSR;
		SerialACIA.RDSR = Data;
		SerialACIA.Status |= MC6850_STATUS_OVRN;
	}

	if (SerialACIA.RIE)
	{
		// interrupt on receive/overun
		SerialACIA.Status |= MC6850_STATUS_IRQ;
		intStatus |= 1 << serial;
	}
}

/*--------------------------------------------------------------------------*/

unsigned char SerialACIAReadRxData()
{
	SerialACIA.Status &= ~MC6850_STATUS_DCD;

	SerialACIA.Status &= ~MC6850_STATUS_IRQ;
	intStatus &= ~(1 << serial);

	unsigned char Data = SerialACIA.RDR;
	SerialACIA.RDR = SerialACIA.RDSR;
	SerialACIA.RDSR = 0;

	if (SerialACIA.RxD > 0)
	{
		SerialACIA.RxD--;
	}

	if (SerialACIA.RxD == 0)
	{
		SerialACIA.Status &= ~MC6850_STATUS_RDRF;
	}

	if (SerialACIA.RxD > 0 && SerialACIA.RIE)
	{
		SerialACIA.Status |= MC6850_STATUS_IRQ;
		intStatus |= 1 << serial;
	}

	if (SerialACIA.DataBits == 7)
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

/*--------------------------------------------------------------------------*/

void SerialPoll(int Cycles)
{
	if (!SerialULA.RS423)
	{
		// Tape input

		// The following code controls the timing of the DCD output from
		// the Serial ULA to the ACIA. Normally, the ACIA DCD input is
		// active low (i.e., logic high indicates carrier not detected),
		// but the Beeb's tape input pulses this high when carrier tone
		// is detected. The pulse starts after about 200ms of 2400Hz
		// carrier tone and lasts about 200us.
		//
		// See https://stardot.org.uk/forums/viewtopic.php?p=327136#p327136

		if (SerialULA.CarrierCycleCount > 0)
		{
			SerialULA.CarrierCycleCount -= Cycles;

			if (SerialULA.CarrierCycleCount <= 0)
			{
				if (SerialULA.TapeCarrier && !SerialACIA.DCD)
				{
					DebugTrace("Serial: Set DCD\n");
					SerialACIA.DCD = true;
					SerialACIA.Status |= MC6850_STATUS_DCD;

					SerialACIA.Status |= MC6850_STATUS_IRQ;
					intStatus |= 1 << serial;

					SerialULA.CarrierCycleCount = MicrosecondsToCycles(200); // To reset DCD
				}
				else
				{
					DebugTrace("Serial: Clear DCD\n");
					SerialACIA.DCD = false;
					SerialACIA.Status &= ~MC6850_STATUS_DCD;
				}
			}
		}

		bool TapeCarrier = false;
		bool UpdateTapeCarrier = false;

		if (TapeState.Recording)
		{
			if (SerialULA.CassetteRelay && UEFFileOpen && TotalCycles >= TapeTrigger)
			{
				if (SerialACIA.TxD > 0)
				{
					// Writing data
					if (UEFWriter.PutData(SerialACIA.TDR | UEF_DATA, TapeState.Clock) != UEFResult::Success)
					{
						mainWin->Report(MessageType::Error,
						                "Error writing to UEF file:\n  %s", TapeFileName);

						SerialStopTapeRecording(true);
					}

					SerialACIA.TxD = 0;
					SerialACIA.Status |= MC6850_STATUS_TDRE;

					if (SerialACIA.TIE)
					{
						SerialACIA.Status |= MC6850_STATUS_IRQ;
						intStatus |= 1 << serial;
					}

					TapeAudio.Data       = (SerialACIA.TDR << 1) | 1;
					TapeAudio.BytePos    = 1;
					TapeAudio.CurrentBit = 0;
					TapeAudio.Signal     = 1;
					TapeAudio.ByteCount  = 3;
				}
				else
				{
					// Tone
					if (UEFWriter.PutData(UEF_CARRIER_TONE, TapeState.Clock) != UEFResult::Success)
					{
						mainWin->Report(MessageType::Error,
						                "Error writing to UEF file:\n  %s", TapeFileName);

						SerialStopTapeRecording(true);
					}

					TapeAudio.Signal  = 2;
					TapeAudio.BytePos = 11;
				}

				SetTrigger(TAPECYCLES, TapeTrigger);
				TapeState.Clock++;
			}
		}
		else // Playing or stopped
		{
			// 10/09/06
			// JW - If trying to write data when not recording, just ignore

			if (SerialACIA.TxD > 0 && TotalCycles >= TapeTrigger)
			{
				// WriteLog("Ignoring Writes\n");

				SerialACIA.TxD = 0;
				SerialACIA.Status |= MC6850_STATUS_TDRE;

				if (SerialACIA.TIE)
				{
					SerialACIA.Status |= MC6850_STATUS_IRQ;
					intStatus |= 1 << serial;
				}
			}

			if (SerialULA.CassetteRelay)
			{
				if (UEFFileOpen)
				{
					if (TapeState.Clock != TapeState.OldClock)
					{
						TapeState.UEFBuf = UEFReader.GetData(TapeState.Clock);
						TapeState.OldClock = TapeState.Clock;
					}

					if (TapeState.UEFBuf != TapeState.OldUEFBuf ||
					    UEFRES_TYPE(TapeState.UEFBuf) == UEF_CARRIER_TONE ||
					    UEFRES_TYPE(TapeState.UEFBuf) == UEF_GAP)
					{
						if (UEFRES_TYPE(TapeState.UEFBuf) != UEFRES_TYPE(TapeState.OldUEFBuf))
						{
							TapeControlUpdateCounter(TapeState.Clock);
						}

						TapeState.OldUEFBuf = TapeState.UEFBuf;

						// New data read in, so do something about it
						switch (UEFRES_TYPE(TapeState.UEFBuf))
						{
							case UEF_CARRIER_TONE:
								TapeCarrier = true;
								UpdateTapeCarrier = true;

								TapeAudio.Signal = 2;
								// TapeAudio.Samples = 0;
								TapeAudio.BytePos = 11;
								break;

							case UEF_GAP:
								TapeCarrier = false;
								UpdateTapeCarrier = true;

								TapeAudio.Signal = 0;
								break;

							case UEF_DATA: {
								TapeCarrier = false;
								UpdateTapeCarrier = true;

								unsigned char Data = UEFRES_BYTE(TapeState.UEFBuf);
								HandleData(Data);

								TapeAudio.Data       = (Data << 1) | 1;
								TapeAudio.BytePos    = 1;
								TapeAudio.CurrentBit = 0;
								TapeAudio.Signal     = 1;
								TapeAudio.ByteCount  = 3;
								break;
							}
						}
					}

					if (SerialACIA.RxD < 2)
					{
						if (TotalCycles >= TapeTrigger)
						{
							if (TapeState.Playing)
							{
								TapeState.Clock++;
							}

							SetTrigger(TAPECYCLES, TapeTrigger);
						}
					}
				}
				else if (CSWFileOpen)
				{
					if (TapeState.Clock != TapeState.OldClock)
					{
						CSWState last_state = csw_state;

						int Data = CSWPoll();
						TapeState.OldClock = TapeState.Clock;

						if (last_state != csw_state)
						{
							TapeControlUpdateCounter(csw_ptr);
						}

						switch (csw_state)
						{
							case CSWState::WaitingForTone:
								TapeCarrier = false;
								UpdateTapeCarrier = true;

								TapeAudio.Signal = 0;
								break;

							case CSWState::Tone:
								TapeCarrier = true;
								UpdateTapeCarrier = true;

								TapeAudio.Signal  = 2;
								TapeAudio.BytePos = 11;
								break;

							case CSWState::Data:
								if (Data >= 0)
								{
									// New data read in, so do something about it
									TapeCarrier = false;
									UpdateTapeCarrier = true;

									HandleData((unsigned char)Data);

									TapeAudio.Data       = (Data << 1) | 1;
									TapeAudio.BytePos    = 1;
									TapeAudio.CurrentBit = 0;
									TapeAudio.Signal     = 1;
									TapeAudio.ByteCount  = 3;
								}
								break;
						}
					}

					if (SerialACIA.RxD < 2)
					{
						if (TotalCycles >= TapeTrigger)
						{
							if (TapeState.Playing)
							{
								TapeState.Clock++;
							}

							SetTrigger(CSWPollCycles, TapeTrigger);
						}
					}
				}

				if (UpdateTapeCarrier && TapeCarrier != SerialULA.TapeCarrier)
				{
					if (TapeCarrier && !SerialULA.TapeCarrier)
					{
						DebugTrace("Serial: Tape carrier detected\n");

						// Onset of carrier tone. Set DCD after about 200ms.
						// Reduce the delay if using fast tape speed.
						const int Delay = TapeState.ClockSpeed == 5600 ? 200 : 5;

						SerialULA.CarrierCycleCount = MillisecondsToCycles(Delay);
					}
					else if (!TapeCarrier && SerialULA.TapeCarrier)
					{
						DebugTrace("Serial: Tape carrier gone\n");
						SerialULA.CarrierCycleCount = 0;
					}

					SerialULA.TapeCarrier = TapeCarrier;
				}
			}
		}
	}
	else if (SerialULA.RS423 && SerialPortEnabled)
	{
		if (SerialDestination == SerialType::TouchScreen)
		{
			if (TouchScreenPoll())
			{
				if (SerialACIA.RxD < 2)
				{
					HandleData(TouchScreenRead());
				}
			}
		}
		else if (SerialDestination == SerialType::IP232)
		{
			if (IP232Poll())
			{
				if (SerialACIA.RxD < 2)
				{
					HandleData(IP232Read());
				}
			}
		}
		else // SerialType::SerialPort
		{
			if (!SerialPort.bWaitingForStat && !bSerialStateChanged)
			{
				WaitCommEvent(SerialPort.hSerialPort, &SerialPort.dwCommEvent, &SerialPort.olStatus);
				SerialPort.bWaitingForStat = true;
			}

			if (!bSerialStateChanged && SerialPort.bCharReady && !SerialPort.bWaitingForData && SerialACIA.RxD < 2)
			{
				if (!ReadFile(SerialPort.hSerialPort, &SerialPort.SerialBuffer, 1, &SerialPort.BytesIn, &SerialPort.olSerialPort))
				{
					if (GetLastError() == ERROR_IO_PENDING)
					{
						SerialPort.bWaitingForData = true;
					}
					else
					{
						mainWin->Report(MessageType::Error, "Serial Port Error");
					}
				}
				else
				{
					if (SerialPort.BytesIn > 0)
					{
						HandleData((unsigned char)SerialPort.SerialBuffer);
					}
					else
					{
						DWORD CommError;
						ClearCommError(SerialPort.hSerialPort, &CommError, nullptr);
						SerialPort.bCharReady = false;
					}
				}
			}
		}
	}
}

/*--------------------------------------------------------------------------*/

static void InitThreads()
{
	SerialPort.Close();

	SerialPort.bWaitingForData = false;
	SerialPort.bWaitingForStat = false;

	if (SerialPortEnabled &&
	    SerialDestination == SerialType::SerialPort &&
	    SerialPortName[0] != '\0')
	{
		InitSerialPort(); // Set up the serial port if its enabled.

		if (SerialPort.olSerialPort.hEvent)
		{
			CloseHandle(SerialPort.olSerialPort.hEvent);
			SerialPort.olSerialPort.hEvent = nullptr;
		}

		SerialPort.olSerialPort.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr); // Create the serial port event signal

		if (SerialPort.olSerialWrite.hEvent)
		{
			CloseHandle(SerialPort.olSerialWrite.hEvent);
			SerialPort.olSerialWrite.hEvent = nullptr;
		}

		SerialPort.olSerialWrite.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr); // Write event, not actually used.

		if (SerialPort.olStatus.hEvent)
		{
			CloseHandle(SerialPort.olStatus.hEvent);
			SerialPort.olStatus.hEvent = nullptr;
		}

		SerialPort.olStatus.hEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr); // Status event, for WaitCommEvent
	}

	bSerialStateChanged = false;
}

/*--------------------------------------------------------------------------*/

unsigned int SerialPortStatusThread::ThreadFunc()
{
	DWORD dwOvRes = 0;

	while (1)
	{
		if (SerialDestination == SerialType::SerialPort &&
		    SerialPortEnabled &&
		    (WaitForSingleObject(SerialPort.olStatus.hEvent, 10) == WAIT_OBJECT_0))
		{
			if (GetOverlappedResult(SerialPort.hSerialPort, &SerialPort.olStatus, &dwOvRes, FALSE))
			{
				// Event waiting in dwCommEvent
				if ((SerialPort.dwCommEvent & EV_RXCHAR) && !SerialPort.bWaitingForData)
				{
					SerialPort.bCharReady = true;
				}

				if (SerialPort.dwCommEvent & EV_CTS)
				{
					// CTS line change
					DWORD LineStat;
					GetCommModemStatus(SerialPort.hSerialPort, &LineStat);

					// Invert for CTS bit, Keep for TDRE bit
					if (LineStat & MS_CTS_ON)
					{
						SerialACIA.Status &= ~MC6850_STATUS_CTS;
						SerialACIA.Status |= MC6850_STATUS_TDRE;
					}
					else
					{
						SerialACIA.Status |= MC6850_STATUS_CTS;
						SerialACIA.Status &= ~MC6850_STATUS_TDRE;
					}
				}
			}

			SerialPort.bWaitingForStat = false;
		}
		else
		{
			Sleep(100); // Don't hog CPU if nothing happening
		}

		if (bSerialStateChanged && !SerialPort.bWaitingForData)
		{
			// Shut off serial port, and re-initialise
			InitThreads();
			bSerialStateChanged = false;
		}

		Sleep(0);
	}

	return 0;
}

/*--------------------------------------------------------------------------*/

unsigned int SerialPortReadThread::ThreadFunc()
{
	// New Serial port thread - 7:35pm 16/10/2001 GMT
	// This sorta runs as a seperate process in effect, checking
	// enable status, and doing the monitoring.

	while (1)
	{
		if (!bSerialStateChanged && SerialPortEnabled && SerialDestination == SerialType::SerialPort && SerialPort.bWaitingForData)
		{
			DWORD Result = WaitForSingleObject(SerialPort.olSerialPort.hEvent, INFINITE); // 10ms to respond

			if (Result == WAIT_OBJECT_0)
			{
				if (GetOverlappedResult(SerialPort.hSerialPort, &SerialPort.olSerialPort, &SerialPort.BytesIn, FALSE))
				{
					// sucessful read, screw any errors.
					if (SerialULA.RS423 && SerialPort.BytesIn > 0)
					{
						HandleData((unsigned char)SerialPort.SerialBuffer);
					}

					if (SerialPort.BytesIn == 0)
					{
						SerialPort.bCharReady = false;

						DWORD CommError;
						ClearCommError(SerialPort.hSerialPort, &CommError, nullptr);
					}

					SerialPort.bWaitingForData = false;
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

/*--------------------------------------------------------------------------*/

static void InitSerialPort()
{
	// Initialise COM port
	if (SerialPortEnabled &&
	    SerialDestination == SerialType::SerialPort &&
	    SerialPortName[0] != '\0')
	{
		if (!SerialPort.Init(SerialPortName))
		{
			mainWin->Report(MessageType::Error, "Could not open serial port %s", SerialPortName);
			bSerialStateChanged = true;
			SerialPortEnabled = false;
			mainWin->UpdateSerialMenu();
		}
	}
}

/*--------------------------------------------------------------------------*/

static void CloseUEFFile()
{
	if (UEFFileOpen)
	{
		SerialStopTapeRecording(false);
		UEFReader.Close();
		UEFFileOpen = false;
	}
}

/*--------------------------------------------------------------------------*/

static void CloseCSWFile()
{
	if (CSWFileOpen)
	{
		CSWClose();
		CSWFileOpen = false;
	}
}

/*--------------------------------------------------------------------------*/

void SerialInit()
{
	InitThreads();

	SerialReadThread.Start();
	SerialStatusThread.Start();
}

/*--------------------------------------------------------------------------*/

void SerialReset()
{
	SerialACIA.DCD = false;
	SerialACIA.TxRate = 1200;
	SerialACIA.RxRate = 1200;
	SerialACIA.ClkDivide = 1;

	SerialULA.CassetteRelay = false;
	SerialULA.RS423 = false;
	SerialULA.TapeCarrier = false;
	SerialULA.CarrierCycleCount = 0;

	TapeState.Recording = false;
	TapeState.Playing = true;
	TapeState.ClockSpeed = 5600;
}

/*--------------------------------------------------------------------------*/

void SerialClose()
{
	CloseTape();
	SerialPort.Close();
}

/*--------------------------------------------------------------------------*/

CSWResult LoadCSWTape(const char *FileName)
{
	CloseTape();

	CSWResult Result = CSWOpen(FileName);

	if (Result == CSWResult::Success)
	{
		CSWFileOpen = true;
		strcpy(TapeFileName, FileName);

		SerialACIA.TxD = 0;
		SerialACIA.RxD = 0;
		TapeState.Clock = 0;
		TapeState.OldClock = 0;

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

/*--------------------------------------------------------------------------*/

UEFResult LoadUEFTape(const char *FileName)
{
	CloseTape();

	// Clock values:
	// 5600 - Normal speed - anything higher is a bit slow
	// 750 - Recommended minimum setting, fastest reliable load
	UEFReader.SetClock(TapeState.ClockSpeed);
	SetUnlockTape(TapeState.Unlock);

	UEFResult Result = UEFReader.Open(FileName);

	if (Result == UEFResult::Success)
	{
		UEFFileOpen = true;
		strcpy(TapeFileName, FileName);

		TapeState.UEFBuf = 0;
		TapeState.OldUEFBuf = 0;
		TapeState.Clock = 0;
		TapeState.OldClock = 0;

		SerialACIA.TxD = 0;
		SerialACIA.RxD = 0;

		SetTrigger(TAPECYCLES, TapeTrigger);

		UEFReader.CreateTapeMap(TapeMap);

		TapeState.Clock = 0;

		if (TapeControlEnabled)
		{
			TapeControlAddMapLines(TapeState.Clock);
		}

		TapeControlUpdateCounter(TapeState.Clock);
	}

	return Result;
}

/*--------------------------------------------------------------------------*/

void CloseTape()
{
	CloseUEFFile();
	CloseCSWFile();

	SerialACIA.TxD = 0;
	SerialACIA.RxD = 0;

	TapeFileName[0] = '\0';

	if (TapeControlEnabled)
	{
		TapeControlCloseTape();
	}
}

/*--------------------------------------------------------------------------*/

void RewindTape()
{
	SerialStopTapeRecording(true);

	TapeState.UEFBuf = 0;
	TapeState.OldUEFBuf = 0;
	TapeState.Clock = 0;
	TapeState.OldClock = 0;
	SerialULA.TapeCarrier = false;
	SerialULA.CarrierCycleCount = 0;

	SetTrigger(TAPECYCLES, TapeTrigger);

	TapeControlUpdateCounter(0);

	csw_state = CSWState::WaitingForTone;
	csw_bit = 0;
	csw_pulselen = 0;
	csw_ptr = 0;
	csw_pulsecount = -1;
}

/*--------------------------------------------------------------------------*/

void SetTapeSpeed(int Speed)
{
	int NewClock = (int)((double)TapeState.Clock * ((double)Speed / TapeState.ClockSpeed));
	TapeState.ClockSpeed = Speed;

	if (UEFFileOpen)
	{
		std::string FileName = TapeFileName;

		LoadUEFTape(FileName.c_str());
	}

	TapeState.Clock = NewClock;
}

/*--------------------------------------------------------------------------*/

void SetUnlockTape(bool Unlock)
{
	UEFReader.SetUnlock(Unlock);
}

/*--------------------------------------------------------------------------*/

void SetTapePosition(int Time)
{
	if (CSWFileOpen)
	{
		csw_ptr = Time;
	}
	else
	{
		TapeState.Clock = Time;
	}

	TapeState.OldClock = 0;

	SetTrigger(TAPECYCLES, TapeTrigger);
}

/*--------------------------------------------------------------------------*/

void SerialPlayTape()
{
	TapeState.Playing = true;
	TapeAudio.Enabled = SerialULA.CassetteRelay;
}

/*--------------------------------------------------------------------------*/

bool SerialRecordTape(const char* FileName)
{
	// Create file
	if (UEFWriter.Open(FileName) == UEFResult::Success)
	{
		strcpy(TapeFileName, FileName);
		UEFFileOpen = true;

		TapeState.Recording = true;
		TapeState.Playing = false;
		TapeAudio.Enabled = SerialULA.CassetteRelay;
		return true;
	}
	else
	{
		TapeFileName[0] = '\0';
		return false;
	}
}

/*--------------------------------------------------------------------------*/

void SerialStopTape()
{
	TapeState.Playing = false;
	TapeAudio.Enabled = false;
}

/*--------------------------------------------------------------------------*/

void SerialStopTapeRecording(bool ReloadTape)
{
	if (TapeState.Recording)
	{
		UEFWriter.PutData(UEF_EOF, 0);
		UEFWriter.Close();

		TapeState.Recording = false;

		if (ReloadTape)
		{
			std::string FileName = TapeFileName;

			LoadUEFTape(FileName.c_str());
		}
	}
}

/*--------------------------------------------------------------------------*/

void SerialEjectTape()
{
	TapeAudio.Enabled = false;
	CloseTape();
}

int SerialGetTapeClock()
{
	if (UEFFileOpen)
	{
		return TapeState.Clock;
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

/*--------------------------------------------------------------------------*/

void SaveSerialUEF(FILE *SUEF)
{
	if (UEFFileOpen || CSWFileOpen)
	{
		fput16(0x0473, SUEF); // UEF Chunk ID
		fput32(0, SUEF); // Chunk length (updated after writing data)
		long StartPos = ftell(SUEF);

		fputc(SerialULA.RS423, SUEF);
		fwrite(TapeFileName, 1, 256, SUEF);
		fputc(SerialULA.CassetteRelay, SUEF);
		fput32(SerialACIA.TxRate, SUEF);
		fput32(SerialACIA.RxRate, SUEF);
		fputc(SerialACIA.ClkDivide, SUEF);
		fputc(SerialACIA.Parity, SUEF);
		fputc(SerialACIA.StopBits, SUEF);
		fputc(SerialACIA.DataBits, SUEF);
		fputc(SerialACIA.RIE, SUEF);
		fputc(SerialACIA.TIE, SUEF);
		fputc(SerialACIA.TxD, SUEF);
		fputc(SerialACIA.RxD, SUEF);
		fputc(SerialACIA.RDR, SUEF);
		fputc(SerialACIA.TDR, SUEF);
		fputc(SerialACIA.RDSR, SUEF);
		fputc(SerialACIA.TDSR, SUEF);
		fputc(SerialACIA.Status, SUEF);
		fputc(SerialACIA.Control, SUEF);
		fputc(SerialULA.Control, SUEF);
		fputc(0, SUEF); // DCD
		fputc(0, SUEF); // DCDI
		fputc(0, SUEF); // ODCDI
		fputc(0, SUEF); // DCDClear
		fput32(TapeState.Clock, SUEF);
		fput32(TapeState.ClockSpeed, SUEF);
		fputc(SerialULA.TapeCarrier, SUEF);
		fput32(SerialULA.CarrierCycleCount, SUEF);
		fputc(TapeState.Playing, SUEF);
		fputc(TapeState.Recording, SUEF);
		fputc(TapeState.Unlock, SUEF);
		fput32(TapeState.UEFBuf, SUEF);
		fput32(TapeState.OldUEFBuf, SUEF);
		fput32(TapeState.OldClock, SUEF);

		if (CSWFileOpen)
		{
			SaveCSWState(SUEF);
		}

		long EndPos = ftell(SUEF);
		long Length = EndPos - StartPos;
		fseek(SUEF, StartPos - 4, SEEK_SET);
		fput32(Length, SUEF); // Size
		fseek(SUEF, EndPos, SEEK_SET);
	}
}

/*--------------------------------------------------------------------------*/

void LoadSerialUEF(FILE *SUEF, int Version)
{
	CloseTape();

	SerialULA.RS423 = fgetbool(SUEF);

	char FileName[256];
	fread(FileName,1,256,SUEF);

	if (FileName[0])
	{
		mainWin->LoadTape(FileName);

		SerialULA.CassetteRelay = fgetbool(SUEF);
		SerialACIA.TxRate = fget32(SUEF);
		SerialACIA.RxRate = fget32(SUEF);
		SerialACIA.ClkDivide = fget8(SUEF);
		SerialACIA.Parity = fget8(SUEF);
		SerialACIA.StopBits = fget8(SUEF);
		SerialACIA.DataBits = fget8(SUEF);
		SerialACIA.RIE = fgetbool(SUEF);
		SerialACIA.TIE = fgetbool(SUEF);
		SerialACIA.TxD = fget8(SUEF);
		SerialACIA.RxD = fget8(SUEF);
		SerialACIA.RDR = fget8(SUEF);
		SerialACIA.TDR = fget8(SUEF);
		SerialACIA.RDSR = fget8(SUEF);
		SerialACIA.TDSR = fget8(SUEF);
		SerialACIA.Status = fget8(SUEF);
		SerialACIA.Control = fget8(SUEF);
		SerialULA.Control = fget8(SUEF);
		fgetbool(SUEF); // DCD
		fgetbool(SUEF); // DCDI
		fgetbool(SUEF); // ODCDI
		fget8(SUEF); // DCDClear
		TapeState.Clock = fget32(SUEF);

		int Speed = fget32(SUEF);
		if (Speed != TapeState.ClockSpeed)
		{
			TapeState.Clock = (int)((double)TapeState.Clock * ((double)TapeState.ClockSpeed / Speed));
		}

		if (Version >= 14)
		{
			// These replace DCD, DCDI, ODCDI, DCDClear from
			// previous BeebEm versions.
			SerialULA.TapeCarrier = fgetbool(SUEF);
			SerialULA.CarrierCycleCount = fget32(SUEF);

			TapeState.Playing = fgetbool(SUEF);
			TapeState.Recording = fgetbool(SUEF);
			bool Unlock = fgetbool(SUEF);
			TapeState.UEFBuf = fget32(SUEF);
			TapeState.OldUEFBuf = fget32(SUEF);
			TapeState.OldClock = fget32(SUEF);

			// Read CSW state
			if (CSWFileOpen)
			{
				LoadCSWState(SUEF);
			}

			mainWin->SetUnlockTape(Unlock);
		}

		TapeControlUpdateCounter(CSWFileOpen ? csw_ptr : TapeState.Clock);
	}
}

/*--------------------------------------------------------------------------*/

void DebugSerialState()
{
	DebugDisplayInfo("");

	static const char* szParity[] = { "N", "O", "E" };

	DebugDisplayInfoF("SerialACIA ControlReg: RIE=%d TIE=%d RTS=%d WordSelect=%d%s%d CounterDivide=%02X",
		(SerialACIA.Control & MC6850_CONTROL_RIE) != 0,
		SerialACIA.TIE,
		SerialACIA.RTS,
		SerialACIA.DataBits,
		szParity[SerialACIA.Parity],
		SerialACIA.StopBits,
		SerialACIA.Control & MC6850_CONTROL_COUNTER_DIVIDE);

	DebugDisplayInfoF("SerialACIA StatusReg: IRQ=%d PE=%d OVRN=%d FE=%d CTS=%d DCD=%d TDRE=%d RDRF=%d",
		(SerialACIA.Status & MC6850_STATUS_IRQ) != 0,
		(SerialACIA.Status & MC6850_STATUS_PE) != 0,
		(SerialACIA.Status & MC6850_STATUS_OVRN) != 0,
		(SerialACIA.Status & MC6850_STATUS_FE) != 0,
		(SerialACIA.Status & MC6850_STATUS_CTS) != 0,
		(SerialACIA.Status & MC6850_STATUS_DCD) != 0,
		(SerialACIA.Status & MC6850_STATUS_TDRE) != 0,
		(SerialACIA.Status & MC6850_STATUS_RDRF) != 0);

	DebugDisplayInfoF("SerialULA ControlReg: Relay=%d RS423=%d RxRate=%d TxRate=%d",
		SerialULA.CassetteRelay,
		SerialULA.RS423,
		SerialACIA.RxRate,
		SerialACIA.TxRate);
}
