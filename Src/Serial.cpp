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
#include <assert.h>

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
#include "SerialPort.h"
#include "Sound.h"
#include "TapeControlDialog.h"
#include "TapeMap.h"
#include "Thread.h"
#include "TouchScreen.h"
#include "Uef.h"
#include "UefState.h"

SerialType SerialDestination;

static bool FirstReset = true;

UEFTapeImage UEFFile;

char TapeFileName[MAX_PATH]; // Filename of current tape file

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

static SerialPort SerialPort;

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

// mm
static void SerialUpdateACIAInterruptStatus()
{
	if (SerialULA.RS423)
	{
		// irq = (tie && tdre && !ncts) || (rie && (rdrf || ovr || ndcd))
		if ((SerialACIA.TIE && ((SerialACIA.Status & (MC6850_STATUS_CTS | MC6850_STATUS_TDRE)) ^ MC6850_STATUS_CTS)) ||
		    (SerialACIA.RIE && (SerialACIA.Status & (MC6850_STATUS_OVRN | MC6850_STATUS_DCD | MC6850_STATUS_RDRF))))
		{
			intStatus |= 1 << serial;
			SerialACIA.Status |= MC6850_STATUS_IRQ;
		}
		else
		{
			intStatus &= ~(1 << serial);
			SerialACIA.Status &= ~MC6850_STATUS_IRQ;
		}
	}
}

/*--------------------------------------------------------------------------*/

void SerialACIAWriteControl(unsigned char Value)
{
	if (DebugEnabled)
	{
		DebugDisplayTraceF(DebugType::Serial, true,
		                   "Serial: Write ACIA control %02X", (int)Value);
	}

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

			if (SerialDestination == SerialType::IP232)
			{
				IP232SetRTS(SerialACIA.RTS);
			}
		}

		SerialACIA.TxD = 0;
		SerialACIA.Status |= MC6850_STATUS_TDRE; // Transmit data register empty

		SetTrigger(TAPECYCLES, TapeTrigger);
	}

	unsigned char OldWordSelect = SerialACIA.Control & MC6850_CONTROL_WORD_SELECT;
	unsigned char NewWordSelect = Value & MC6850_CONTROL_WORD_SELECT;

	bool OldRTS = SerialACIA.RTS;

	SerialACIA.Control = Value; // This is done for safe keeping

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
	if (SerialULA.RS423)
	{
		if (SerialDestination == SerialType::SerialPort)
		{
			if (NewWordSelect != OldWordSelect)
			{
				SerialPort.Configure(SerialACIA.DataBits,
				                     SerialACIA.StopBits,
				                     SerialACIA.Parity);
			}

			// Check RTS
			if (SerialACIA.RTS != OldRTS)
			{
				SerialPort.SetRTS(SerialACIA.RTS);
			}
		}
		else if (SerialDestination == SerialType::IP232)
		{
			if (SerialACIA.RTS != OldRTS)
			{
				IP232SetRTS(SerialACIA.RTS);
			}
		}
	}

	SerialUpdateACIAInterruptStatus();
}

/*--------------------------------------------------------------------------*/

static void HandleTXData(unsigned char Data)
{
	/* TxD  TDR    TDSR
	   0    EMPTY  EMPTY
	   1    EMPTY  FULL
	   2    FULL   FULL */

	if (SerialACIA.TxD++ == 0)
	{
		// TDSR empty
		SerialACIA.TDSR = Data;
		SerialACIA.Status |= MC6850_STATUS_TDRE;

		if (SerialDestination == SerialType::TouchScreen)
		{
			TouchScreenWrite(SerialACIA.TDSR);
		}
		else if (SerialDestination == SerialType::IP232)
		{
			DebugTrace("SerialTX: %02X\n", SerialACIA.TDSR);

			IP232Write(SerialACIA.TDSR);
			if (!IP232Raw && SerialACIA.TDSR == 255) IP232Write(SerialACIA.TDSR);
		}
		else
		{
			SerialPort.WriteChar(SerialACIA.TDSR);
		}

		//WriteLog("Serial: TX TDR=%02X, TDSR=%02x TxD=%d Status=%02x Tx_Rate=%d\n", TDR, TDSR, TxD, SerialACIA.Status, Tx_Rate);

		int Baud = SerialACIA.TxRate; // *((Clk_Divide == 1) ? 64 : (Clk_Divide == 64) ? 1 : 4);
		TapeTrigger = TotalCycles + 2000000 / (Baud / 10) * TapeState.ClockSpeed / 5600;
	}
	else
	{
		// TDSR full
		SerialACIA.TDR = Data;
		SerialACIA.Status &= ~MC6850_STATUS_TDRE;
	}

	SerialUpdateACIAInterruptStatus();
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
			HandleTXData(Data);
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

		// AUG p.389 says DCD will always be low when the RS423 interface
		// is selected.
		SerialACIA.DCD = false;
		SerialACIA.Status &= ~MC6850_STATUS_DCD;
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

	// See https://github.com/stardot/beebem-windows/issues/47
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

	// WriteLog("Serial: Read ACIA status %02X\n", (int)SerialACIA.Status);

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

	SerialUpdateACIAInterruptStatus();
	//WriteLog("HandleData: Data=%02x Status=%02x RxD=%d\n", AData, SerialACIA.Status, RxD);
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

	// WriteLog("Serial: Read ACIA Rx %02X, SerialACIA.Status = %02x\n", (int)Data, (int)SerialACIA.Status);

	SerialUpdateACIAInterruptStatus();
	// WriteLog("Read_ACIA_Rx_Data: Data = %02x Status=%02x RxD=%d\n", TData, SerialACIA.Status, RxD);

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
					if (UEFFile.PutData(SerialACIA.TDR | UEF_DATA, TapeState.Clock) != UEFResult::Success)
					{
						mainWin->Report(MessageType::Error,
						                "Error writing to UEF file:\n  %s", TapeFileName);

						SerialStopTapeRecording();
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
					if (UEFFile.PutData(UEF_CARRIER_TONE, TapeState.Clock) != UEFResult::Success)
					{
						mainWin->Report(MessageType::Error,
						                "Error writing to UEF file:\n  %s", TapeFileName);

						SerialStopTapeRecording();
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
						TapeState.UEFBuf = UEFFile.GetData(TapeState.Clock);
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

							default:
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
		// MM
		if (SerialACIA.TxD > 0 && TotalCycles >= TapeTrigger)
		{
			//WriteLog("Serial: Send Tx TDSR=%02X  TIE=%d\n", TDSR, TIE);

			if (SerialACIA.TxD-- == 2) {
				// TDR full
				SerialACIA.TxD = 0;
				HandleTXData(SerialACIA.TDR);
			}
		}

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
					// if ((SerialACIA.Control & 0x60) != 0x40) // if nrts = 0
					HandleData(IP232Read());
				}
			}

			if (IP232PollStatus())
			{
				unsigned char Status = IP232ReadStatus();

				switch (Status)
				{
					case IP232_DTR_HIGH:
						DebugTrace("IP232_DTR_HIGH\n");

						if (DebugEnabled)
							DebugDisplayTrace(DebugType::RemoteServer, true, "Flag,1 DCD True, CTS");

						SerialACIA.Status &= ~MC6850_STATUS_CTS; // CTS goes active low
						SerialACIA.Status |= MC6850_STATUS_TDRE; // so TDRE goes high ??
						break;

					case IP232_DTR_LOW:
						DebugTrace("IP232_DTR_LOW\n");

						if (DebugEnabled)
							DebugDisplayTrace(DebugType::RemoteServer, true, "Flag,0 DCD False, clear CTS");

						SerialACIA.Status |= MC6850_STATUS_CTS; // CTS goes inactive high
						SerialACIA.Status &= ~MC6850_STATUS_TDRE; // so TDRE goes low
						break;

					default:
						break;
				}
			}
		}
		else // SerialType::SerialPort
		{
			if (SerialPort.HasRxData() && SerialACIA.RxD < 2)
			{
				unsigned char Data = SerialPort.GetRxData();

				HandleData(Data);
			}

			if (SerialPort.HasStatus())
			{
				// CTS line change
				unsigned char ModemStatus = SerialPort.GetStatus();

				// Invert for CTS bit, Keep for TDRE bit
				if (ModemStatus & MS_CTS_ON)
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
	}
}

/*--------------------------------------------------------------------------*/

void CloseUEFFile()
{
	if (UEFFileOpen)
	{
		SerialStopTapeRecording();
		UEFFile.Close();
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

// Set up the serial port.

bool SerialInit(const char* PortName)
{
	return SerialPort.Init(PortName);
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
			TapeControlAddMapLines();
			TapeControlUpdateCounter(csw_ptr);
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
	UEFFile.SetClock(TapeState.ClockSpeed);
	SetUnlockTape(TapeState.Unlock);

	UEFResult Result = UEFFile.Open(FileName);

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

		UEFFile.CreateTapeMap(TapeMap);

		if (TapeControlEnabled)
		{
			TapeControlAddMapLines();
			TapeControlUpdateCounter(TapeState.Clock);
		}
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
	SerialStopTapeRecording();

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
	UEFFile.SetUnlock(Unlock);
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

void SerialNewTape()
{
	UEFFile.New();

	TapeState.Recording = false;
	TapeState.Playing = true;
	TapeAudio.Enabled = SerialULA.CassetteRelay;

	UEFFileOpen = true;
}

/*--------------------------------------------------------------------------*/

void SerialRecordTape()
{
	assert(UEFFileOpen);
	assert(!CSWFileOpen);

	TapeState.Recording = true;
	TapeState.Playing = false;
	TapeAudio.Enabled = SerialULA.CassetteRelay;
}

/*--------------------------------------------------------------------------*/

bool SerialTapeIsModified()
{
	if (CSWFileOpen)
	{
		return false;
	}
	else if (UEFFileOpen)
	{
		return UEFFile.IsModified();
	}
	else
	{
		return false;
	}
}

/*--------------------------------------------------------------------------*/

bool SerialTapeIsUef()
{
	if (CSWFileOpen)
	{
		return false;
	}
	else
	{
		return UEFFileOpen;
	}
}

/*--------------------------------------------------------------------------*/

void SerialUpdateTapeClock()
{
	UEFFile.SetChunkClock();
}

/*--------------------------------------------------------------------------*/

void SerialStopTape()
{
	TapeState.Playing = false;
	TapeAudio.Enabled = false;
}

/*--------------------------------------------------------------------------*/

void SerialStopTapeRecording()
{
	if (TapeState.Recording)
	{
		UEFFile.PutData(UEF_EOF, 0);

		TapeState.Recording = false;
	}
}

/*--------------------------------------------------------------------------*/

void SerialEjectTape()
{
	TapeAudio.Enabled = false;
	CloseTape();
}

/*--------------------------------------------------------------------------*/

SerialTapeState SerialGetTapeState()
{
	if (!UEFFileOpen && !CSWFileOpen)
	{
		return SerialTapeState::NoTape;
	}

	if (TapeState.Recording)
	{
		return SerialTapeState::Recording;
	}

	if (TapeState.Playing)
	{
		return SerialTapeState::Playing;
	}

	return SerialTapeState::Stopped;
}

/*--------------------------------------------------------------------------*/

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
	UEFWrite8(SerialULA.RS423, SUEF);
	UEFWriteString(TapeFileName, SUEF);
	UEFWrite8(SerialULA.CassetteRelay, SUEF);
	UEFWrite32(SerialACIA.TxRate, SUEF);
	UEFWrite32(SerialACIA.RxRate, SUEF);
	UEFWrite8(SerialACIA.ClkDivide, SUEF);
	UEFWrite8(SerialACIA.Parity, SUEF);
	UEFWrite8(SerialACIA.StopBits, SUEF);
	UEFWrite8(SerialACIA.DataBits, SUEF);
	UEFWrite8(SerialACIA.RIE, SUEF);
	UEFWrite8(SerialACIA.TIE, SUEF);
	UEFWrite8(SerialACIA.TxD, SUEF);
	UEFWrite8(SerialACIA.RxD, SUEF);
	UEFWrite8(SerialACIA.RDR, SUEF);
	UEFWrite8(SerialACIA.TDR, SUEF);
	UEFWrite8(SerialACIA.RDSR, SUEF);
	UEFWrite8(SerialACIA.TDSR, SUEF);
	UEFWrite8(SerialACIA.Status, SUEF);
	UEFWrite8(SerialACIA.Control, SUEF);
	UEFWrite8(SerialULA.Control, SUEF);
	UEFWrite8(0, SUEF); // DCD
	UEFWrite8(0, SUEF); // DCDI
	UEFWrite8(0, SUEF); // ODCDI
	UEFWrite8(0, SUEF); // DCDClear
	UEFWrite32(TapeState.Clock, SUEF);
	UEFWrite32(TapeState.ClockSpeed, SUEF);
	UEFWrite8(SerialULA.TapeCarrier, SUEF);
	UEFWrite32(SerialULA.CarrierCycleCount, SUEF);
	UEFWrite8(TapeState.Playing, SUEF);
	UEFWrite8(TapeState.Recording, SUEF);
	UEFWrite8(TapeState.Unlock, SUEF);
	UEFWrite32(TapeState.UEFBuf, SUEF);
	UEFWrite32(TapeState.OldUEFBuf, SUEF);
	UEFWrite32(TapeState.OldClock, SUEF);

	if (CSWFileOpen)
	{
		SaveCSWState(SUEF);
	}
}

/*--------------------------------------------------------------------------*/

void LoadSerialUEF(FILE *SUEF, int Version)
{
	CloseTape();

	SerialULA.RS423 = UEFReadBool(SUEF);

	char FileName[256];
	memset(FileName, 0, sizeof(FileName));

	if (Version >= 14)
	{
		UEFReadString(FileName, sizeof(FileName), SUEF);
	}
	else
	{
		UEFReadBuf(FileName, 256, SUEF);
	}

	if (FileName[0])
	{
		mainWin->LoadTape(FileName);

		SerialULA.CassetteRelay = UEFReadBool(SUEF);
		SerialACIA.TxRate = UEFRead32(SUEF);
		SerialACIA.RxRate = UEFRead32(SUEF);
		SerialACIA.ClkDivide = UEFRead8(SUEF);
		SerialACIA.Parity = UEFRead8(SUEF);
		SerialACIA.StopBits = UEFRead8(SUEF);
		SerialACIA.DataBits = UEFRead8(SUEF);
		SerialACIA.RIE = UEFReadBool(SUEF);
		SerialACIA.TIE = UEFReadBool(SUEF);
		SerialACIA.TxD = UEFRead8(SUEF);
		SerialACIA.RxD = UEFRead8(SUEF);
		SerialACIA.RDR = UEFRead8(SUEF);
		SerialACIA.TDR = UEFRead8(SUEF);
		SerialACIA.RDSR = UEFRead8(SUEF);
		SerialACIA.TDSR = UEFRead8(SUEF);
		SerialACIA.Status = UEFRead8(SUEF);
		SerialACIA.Control = UEFRead8(SUEF);
		SerialULA.Control = UEFRead8(SUEF);
		UEFReadBool(SUEF); // DCD
		UEFReadBool(SUEF); // DCDI
		UEFReadBool(SUEF); // ODCDI
		UEFRead8(SUEF); // DCDClear
		TapeState.Clock = UEFRead32(SUEF);

		int Speed = UEFRead32(SUEF);

		if (Speed != TapeState.ClockSpeed)
		{
			TapeState.Clock = (int)((double)TapeState.Clock * ((double)TapeState.ClockSpeed / Speed));
		}

		if (Version >= 14)
		{
			// These replace DCD, DCDI, ODCDI, DCDClear from
			// previous BeebEm versions.
			SerialULA.TapeCarrier = UEFReadBool(SUEF);
			SerialULA.CarrierCycleCount = UEFRead32(SUEF);

			TapeState.Playing = UEFReadBool(SUEF);
			TapeState.Recording = UEFReadBool(SUEF);
			bool Unlock = UEFReadBool(SUEF);
			TapeState.UEFBuf = UEFRead32(SUEF);
			TapeState.OldUEFBuf = UEFRead32(SUEF);
			TapeState.OldClock = UEFRead32(SUEF);

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

/*--------------------------------------------------------------------------*/
