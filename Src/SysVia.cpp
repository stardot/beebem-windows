/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 1994  David Alan Gilbert
Copyright (C) 1997  Mike Wyatt
Copyright (C) 2001  Richard Gellman
Copyright (C) 2004  Ken Lowe

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

/* System VIA support file for the beeb emulator- includes things like the
keyboard emulation - David Alan Gilbert 30/10/94 */
/* CMOS Ram finalised 06/01/2001 - Richard Gellman */

#include <windows.h>

#include <stdio.h>
#include <time.h>

#include "SysVia.h"
#include "6502core.h"
#include "Bcd.h"
#include "BeebMem.h"
#include "BeebWin.h"
#include "Debug.h"
#include "DebugTrace.h"
#include "IC32Latch.h"
#include "Main.h"
#include "Model.h"
#include "Rtc.h"
#include "Sound.h"
#include "Speech.h"
#include "Via.h"

// #define DEBUG_IC32
// #define DEBUG_KEYBOARD
// #define DEBUG_SLOW_DATA_BUS

// Shift register stuff
static unsigned char SRMode;
static unsigned char SRCount;
static unsigned char SRData;
static unsigned char SREnabled;

// Fire button for joystick 1 and 2, false=not pressed, true=pressed
bool JoystickButton[2] = { false, false };

// My raw VIA state
VIAState SysVIAState;
// static char WECycles = 0;
// static char WEState = 0;

// Last value written to the slow data bus - sound reads it later
static unsigned char SlowDataBusWriteValue = 0;

// Currently selected keyboard row, column
static unsigned int KBDRow = 0;
static unsigned int KBDCol = 0;

static bool SysViaKbdState[16][8]; // Col, row
static int KeysDown=0;

unsigned char KeyboardLinks = 0;

/*--------------------------------------------------------------------------*/

static void TranslateKeyboardLinks(unsigned char Value)
{
	for (int i = 0; i < 8; i++)
	{
		const int Row = 0;
		const int Column = 9 - i;

		if (Value & (1 << i))
		{
			BeebKeyDown(Row, Column);
		}
	}
}

/*--------------------------------------------------------------------------*/

void BeebReleaseAllKeys()
{
	KeysDown = 0;

	for (int Row = 0; Row < 8; Row++)
	{
		for (int Column = 0; Column < 16; Column++)
		{
			SysViaKbdState[Column][Row] = false;
		}
	}

	if (MachineType != Model::Master128 && MachineType != Model::MasterET)
	{
		TranslateKeyboardLinks(KeyboardLinks);
	}
}

/*--------------------------------------------------------------------------*/

static void UpdateIFRTopBit()
{
	// Update top bit of IFR
	if (SysVIAState.ifr & (SysVIAState.ier & 0x7f))
	{
		SysVIAState.ifr |= 0x80;
		intStatus |= 1 << sysVia;
	}
	else
	{
		SysVIAState.ifr &= 0x7f;
		intStatus &= ~(1 << sysVia);
	}
}

/*--------------------------------------------------------------------------*/

void PulseSysViaCB1()
{
	// Set IFR bit 4 - AtoD end of conversion interrupt
	if (SysVIAState.ier & 16)
	{
		SysVIAState.ifr |= 16;
		UpdateIFRTopBit();
	}
}

/*--------------------------------------------------------------------------*/

void DoKbdIntCheck()
{
	// Now lets see if we just caused a CA2 interrupt - note we will flag
	// it multiply - we aren't going to test for the edge
	// Two cases - write enable is OFF the keyboard - basically any key will cause an
	// interrupt in a few cycles.

	#ifdef DEBUG_KEYBOARD
	int Oldflag = (SysVIAState.ifr & 1);
	#endif

	if (KeysDown > 0 && (SysVIAState.pcr & 0xc) == 4)
	{
		if (IC32State & IC32_KEYBOARD_WRITE)
		{
			SysVIAState.ifr |= 1; // CA2
			//DebugTrace("DoKbdIntCheck: Caused interrupt case 1\n");
			UpdateIFRTopBit();
		}
		else
		{
			if (KBDCol < 15)
			{
				for (int Row = 1; Row < 8; Row++)
				{
					if (SysViaKbdState[KBDCol][Row])
					{
						SysVIAState.ifr |= 1;
						// DebugTrace("DoKbdIntCheck: Caused interrupt case 2\n");
						UpdateIFRTopBit();
					}
				}
			}
		} // WriteEnable on
	} // Keys down and CA2 input enabled

	#ifdef DEBUG_KEYBOARD

	DebugTrace("DoKbdIntCheck KeysDown=%d pcr & c=%d IC32State & 8=%d "
	           "KBDRow=%d KBDCol=%d oldIFRflag=%d Newflag=%d\n",
	           KeysDown, SysVIAState.pcr & 0xc, IC32State & IC32_KEYBOARD_WRITE,
	           KBDRow, KBDCol, Oldflag, SysVIAState.ifr & 1);

	#endif
}

/*--------------------------------------------------------------------------*/

void BeebKeyDown(int Row, int Column)
{
	if (Row < 0 || Column < 0)
	{
		return;
	}

	// Update keys down count - unless it's shift/control
	if (!SysViaKbdState[Column][Row] && Row != 0)
	{
		KeysDown++;
	}

	SysViaKbdState[Column][Row] = true;

	DoKbdIntCheck();
}

/*--------------------------------------------------------------------------*/

void BeebKeyUp(int Row, int Column)
{
	if (Row < 0 || Column < 0)
	{
		return;
	}

	// Update keys down count - unless it's shift/control
	if (SysViaKbdState[Column][Row] && Row != 0)
	{
		KeysDown--;
	}

	SysViaKbdState[Column][Row] = false;
}

/*--------------------------------------------------------------------------*/

// Return current state of the single bit output of the keyboard matrix
// - NOT the any keypressed interrupt

static bool KbdOP()
{
	// Check range validity
	if (KBDCol > 14 || KBDRow > 7) return false; // Key not down if overrange - perhaps we should do something more?

	return SysViaKbdState[KBDCol][KBDRow];
}

/*--------------------------------------------------------------------------*/

static void IC32Write(unsigned char Value)
{
	// Hello. This is Richard Gellman. It is 10:25pm, Friday 2nd February 2001
	// I have to do CMOS RAM now. And I think I'm going slightly potty.
	// Additional, Sunday 4th February 2001. I must have been potty. the line above did read January 2000.
	int PrevIC32State = IC32State;

	int Bit = Value & 7;

	if (Value & 8)
	{
		#ifdef DEBUG_IC32

		if (!(IC32State & (1 << Bit)))
		{
			DebugTrace("IC32 set bit %d (0x%02d)\n", Bit, 1 << Bit);
		}

		#endif

		IC32State |= 1 << Bit;
	}
	else
	{
		#ifdef DEBUG_IC32

		if (IC32State & (1 << Bit))
		{
			DebugTrace("IC32 clear bit %d (0x%02d)\n", Bit, 1 << Bit);
		}

		#endif

		IC32State &= ~(1 << Bit);
	}

	LEDs.CapsLock = (IC32State & IC32_CAPS_LOCK) == 0;
	LEDs.ShiftLock = (IC32State & IC32_SHIFT_LOCK) == 0;

	if (MachineType == Model::Master128 || MachineType == Model::MasterET)
	{
		if (RTCIsChipEnable())
		{
			if (IC32State & IC32_RTC_READ)
			{
				// During read cycles, DS signifies the time that the RTC
				// is to drive the bidirectional bus.
				if (!(PrevIC32State & IC32_RTC_DATA_STROBE) && (IC32State & IC32_RTC_DATA_STROBE))
				{
					SysVIAState.ora = RTCReadData();
				}
			}
			else
			{
				// In write cycles, the trailing edge of DS causes the RTC
				// to latch the written data.
				if ((PrevIC32State & IC32_RTC_DATA_STROBE) && !(IC32State & IC32_RTC_DATA_STROBE))
				{
					RTCWriteData(SysVIAState.ora);
				}
			}
		}
	}

	// Must do sound reg access when write line changes

	if ((PrevIC32State & IC32_SOUND_WRITE) && !(IC32State & IC32_SOUND_WRITE))
	{
		Sound_RegWrite(SlowDataBusWriteValue);
	}

	#if ENABLE_SPEECH

	if (MachineType != Model::Master128 && MachineType != Model::MasterET)
	{
		if ((PrevIC32State & IC32_SPEECH_WRITE) && !(IC32State & IC32_SPEECH_WRITE))
		{
			SpeechWrite(SlowDataBusWriteValue);
		}

		if ((PrevIC32State & IC32_SPEECH_READ) && !(IC32State & IC32_SPEECH_READ))
		{
			SpeechReadEnable();
		}
	}

	#endif

	if (!(IC32State & IC32_KEYBOARD_WRITE) && (PrevIC32State & IC32_KEYBOARD_WRITE))
	{
		KBDRow = (SlowDataBusWriteValue >> 4) & 7;
		KBDCol = (SlowDataBusWriteValue & 0xf);
		DoKbdIntCheck(); /* Should really only if write enable on KBD changes */
	}
}

void ChipClock(int /* nCycles */) {
//	if (WECycles > 0) WECycles -= nCycles;
//	else
//	if (WEState) Sound_RegWrite(SlowDataBusWriteValue);
}

/*--------------------------------------------------------------------------*/

static void SlowDataBusWrite(unsigned char Value)
{
	SlowDataBusWriteValue = Value;

	#ifdef DEBUG_SLOW_DATA_BUS
	DebugTrace("Slow data bus write IC32State=%d Value=0x%02x\n", IC32State, Value);
	#endif

	if ((IC32State & IC32_KEYBOARD_WRITE) == 0)
	{
		// kbd write
		KBDRow = (Value >> 4) & 7;
		KBDCol = Value & 0xf;
		// DebugTrace("SlowDataBusWrite to kbd  Row=%d Col=%d\n", KBDRow, KBDCol);
		DoKbdIntCheck(); /* Should really only if write enable on KBD changes */
	}

	if ((IC32State & IC32_SOUND_WRITE) == 0)
	{
		Sound_RegWrite(SlowDataBusWriteValue);
	}
}

/*--------------------------------------------------------------------------*/

static unsigned char SlowDataBusRead()
{
	if (MachineType == Model::Master128 || MachineType == Model::MasterET)
	{
		if (RTCIsChipEnable() && (IC32State & IC32_RTC_READ))
		{
			return SysVIAState.ora & ~SysVIAState.ddra;
		}
	}

	unsigned char result = SysVIAState.ora & SysVIAState.ddra;

	if (MachineType == Model::Master128 || MachineType == Model::MasterET)
	{
		// I don't know this lot properly - just put in things as we figure them out

		if (KbdOP()) result |= 128;
	}
	else
	{
		if ((IC32State & IC32_KEYBOARD_WRITE) == 0)
		{
			if (KbdOP()) result |= 128;
		}

		#if ENABLE_SPEECH

		if ((IC32State & IC32_SPEECH_READ) == 0)
		{
			result = SpeechRead();
		}

		#endif

		if ((IC32State & IC32_SPEECH_WRITE) == 0)
		{
			result = 0xff;
		}
	}

	#ifdef DEBUG_SLOW_DATA_BUS
	DebugTrace("SlowDataBusRead giving 0x%02x\n", result);
	#endif

	return result;
}

/*--------------------------------------------------------------------------*/

// Address is in the range 0-f - with the fe40 stripped out

void SysVIAWrite(int Address, unsigned char Value)
{
	// DebugTrace("SysVIAWrite: Address=0x%02x Value=0x%02x\n", Address, Value);

	if (DebugEnabled)
	{
		DebugDisplayTraceF(DebugType::SysVIA, true,
		                   "SysVia: Write address %X value %02X",
		                   Address & 0xf, Value);
	}

	switch (Address)
	{
		case 0: // ORB
			// Clear bit 4 of IFR from AtoD Conversion
			SysVIAState.ifr &= ~16;
			SysVIAState.orb = Value;

			if (MachineType == Model::Master128 || MachineType == Model::MasterET)
			{
				// In the Master series, PB6 is the MC146818AP RTC Chip Enable
				// and PB7 is the MC146818AP RTC Address Strobe (AS)

				RTCChipEnable((Value & 0x40) != 0);

				if ((Value & 0xC0)== 0xC0)
				{
					// Address must be valid just prior to the fall of AS,
					// at which time the address is latched.
					RTCWriteAddress(SysVIAState.ora);
				}
			}

			// The bottom 4 bits of ORB connect to the IC32 latch.
			IC32Write(Value);

			if ((SysVIAState.ifr & 8) && ((SysVIAState.pcr & 0x20) == 0))
			{
				SysVIAState.ifr &= 0xf7;
				UpdateIFRTopBit();
			}
			SysVIAState.ifr &= ~16;
			UpdateIFRTopBit();
			break;

		case 1: // ORA
			SysVIAState.ora = Value;
			SlowDataBusWrite(Value);
			SysVIAState.ifr&=0xfc;
			UpdateIFRTopBit();
			break;

		case 2:
			SysVIAState.ddrb = Value;
			break;

		case 3:
			SysVIAState.ddra = Value;
			break;

		case 4:
		case 6:
			SysVIAState.timer1l &= 0xff00;
			SysVIAState.timer1l |= Value;
			break;

		case 5:
			SysVIAState.timer1l &= 0xff;
			SysVIAState.timer1l |= Value << 8;
			SysVIAState.timer1c = SysVIAState.timer1l * 2 + 1;
			SysVIAState.ifr &= 0xbf; // clear timer 1 ifr
			// If PB7 toggling enabled, then lower PB7 now
			if (SysVIAState.acr & 0x80)
			{
				SysVIAState.orb &= 0x7f;
				SysVIAState.irb &= 0x7f;
			}
			UpdateIFRTopBit();
			SysVIAState.timer1hasshot = false;
			break;

		case 7:
			SysVIAState.timer1l &= 0xff;
			SysVIAState.timer1l |= Value << 8;
			SysVIAState.ifr &= 0xbf; // clear timer 1 ifr (this is what Model-B does)
			UpdateIFRTopBit();
			break;

		case 8:
			SysVIAState.timer2l &= 0xff00;
			SysVIAState.timer2l |= Value;
			break;

		case 9:
			SysVIAState.timer2l &= 0xff;
			SysVIAState.timer2l |= Value << 8;
			SysVIAState.timer2c=SysVIAState.timer2l * 2 + 1;
			if (SysVIAState.timer2c == 0) SysVIAState.timer2c = 0x20000;
			SysVIAState.ifr &= 0xdf; // Clear timer 2 IFR
			UpdateIFRTopBit();
			SysVIAState.timer2hasshot = false;
			break;

		case 10:
			SRData = Value;
			break;

		case 11:
			SysVIAState.acr = Value;
			SRMode = (Value >> 2) & 7;
			break;

		case 12:
			SysVIAState.pcr = Value;

			if ((Value & PCR_CA2_CONTROL) == PCR_CA2_OUTPUT_HIGH)
			{
				SysVIAState.ca2 = true;
			}
			else if ((Value & PCR_CA2_CONTROL) == PCR_CA2_OUTPUT_LOW)
			{
				SysVIAState.ca2 = false;
			}

			if ((Value & PCR_CB2_CONTROL) == PCR_CB2_OUTPUT_HIGH)
			{
				if (!SysVIAState.cb2)
				{
					// Light pen strobe on CB2 low -> high transition
					VideoLightPenStrobe();
				}

				SysVIAState.cb2 = true;
			}
			else if ((Value & PCR_CB2_CONTROL) == PCR_CB2_OUTPUT_LOW)
			{
				SysVIAState.cb2 = false;
			}
			break;

		case 13:
			SysVIAState.ifr &= ~Value;
			UpdateIFRTopBit();
			break;

		case 14:
			// DebugTrace("Write ier Value=0x%02x\n", Value);

			if (Value & 0x80)
			{
				SysVIAState.ier |= Value;
			}
			else
			{
				SysVIAState.ier &= ~Value;
			}
			SysVIAState.ier &= 0x7f;
			UpdateIFRTopBit();
			break;

		case 15:
			SysVIAState.ora = Value;
			SlowDataBusWrite(Value);
			break;
	}
}

/*--------------------------------------------------------------------------*/

// Address is in the range 0-f - with the fe40 stripped out

unsigned char SysVIARead(int Address)
{
	unsigned char tmp = 0xff;

	// DebugTrace("SysVIARead: Address=0x%02x at %d\n", Address, TotalCycles);

	switch (Address)
	{
		case 0: // IRB read
			// Clear bit 4 of IFR from AtoD Conversion
			SysVIAState.ifr &= ~16;
			tmp = SysVIAState.orb & SysVIAState.ddrb;

			if (!JoystickButton[1])
			{
				tmp |= 0x20;
			}

			if (!JoystickButton[0])
			{
				tmp |= 0x10;
			}

			#if ENABLE_SPEECH

			if (!SpeechStarted)
			{
				tmp |= 0xc0; // Speech system non existant
			}
			else
			{
				if (SpeechInterrupt()) // Flag is active low
				{
					tmp |= 0x40;
				}
				else
				{
					tmp &= ~0x40;
				}

				if (SpeechReady()) // Flag is active low
				{
					tmp |= 0x80;
				}
				else
				{
					tmp &= ~0x80;
				}
			}

			#else

			tmp |= 0xc0; // Speech system non existant

			#endif

			UpdateIFRTopBit();
			break;

		case 2:
			tmp = SysVIAState.ddrb;
			break;

		case 3:
			tmp = SysVIAState.ddra;
			break;

		case 4: // Timer 1 lo counter
			if (SysVIAState.timer1c < 0)
			{
				tmp = 0xff;
			}
			else
			{
				tmp = (SysVIAState.timer1c / 2) & 0xff;
			}

			SysVIAState.ifr &= 0xbf; // Clear bit 6 - timer 1
			UpdateIFRTopBit();
			break;

		case 5: // Timer 1 hi counter
			tmp = (SysVIAState.timer1c >> 9) & 0xff; // K.Lowe
			break;

		case 6: // Timer 1 lo latch
			tmp = SysVIAState.timer1l & 0xff;
			break;

		case 7: // Timer 1 hi latch
			tmp = (SysVIAState.timer1l >> 8) & 0xff; // K.Lowe
			break;

		case 8: // Timer 2 lo counter
			if (SysVIAState.timer2c < 0) // Adjust for dividing -ve count by 2
			{
				tmp = ((SysVIAState.timer2c - 1) / 2) & 0xff;
			}
			else
			{
				tmp = (SysVIAState.timer2c / 2) & 0xff;
			}
			SysVIAState.ifr &= 0xdf; // Clear bit 5 - timer 2
			UpdateIFRTopBit();
			break;

		case 9: // Timer 2 hi counter
			tmp = (SysVIAState.timer2c >> 9) & 0xff; // K.Lowe
			break;

		case 10:
			tmp = SRData;
			break;

		case 11:
			tmp = SysVIAState.acr;
			break;

		case 12:
			tmp = SysVIAState.pcr;
			break;

		case 13:
			UpdateIFRTopBit();

			#ifdef DEBUG_KEYBOARD
			// DebugTrace("Read IFR got=0x%02x\n", SysVIAState.ifr);
			#endif

			tmp = SysVIAState.ifr;
			break;

		case 14:
			tmp = SysVIAState.ier | 0x80;
			break;

		case 1:
			SysVIAState.ifr &= 0xfc;
			UpdateIFRTopBit();
		case 15:
			// slow data bus read
			tmp = SlowDataBusRead();
			break;
	}

	if (DebugEnabled)
	{
		DebugDisplayTraceF(DebugType::SysVIA, true,
		                   "SysVia: Read address %X value %02X",
		                   Address & 0xf, tmp & 0xff);
	}

	return tmp;
}

/*--------------------------------------------------------------------------*/

// Value denotes the new value - i.e. 1 for a rising edge

void SysVIATriggerCA1Int(int value)
{
	// value^=1;
	// DebugTrace("SysVIATriggerCA1Int at %d\n", TotalCycles);

	// Cause interrupt on appropriate edge
	if (!((SysVIAState.pcr & 1) ^ value))
	{
		SysVIAState.ifr |= 2; // CA1
		UpdateIFRTopBit();
	}
}

/*--------------------------------------------------------------------------*/

void SysVIA_poll_real()
{
	static bool t1int = false;

	if (SysVIAState.timer1c < -2 && !t1int)
	{
		t1int = true;

		if (!SysVIAState.timer1hasshot || (SysVIAState.acr & 0x40))
		{
			// DebugTrace("SysVia timer1 int at %d\n", TotalCycles);
			SysVIAState.ifr |= 0x40; // Timer 1 interrupt
			UpdateIFRTopBit();

			if (SysVIAState.acr & 0x80)
			{
				SysVIAState.orb ^= 0x80; // Toggle PB7
				SysVIAState.irb ^= 0x80; // Toggle PB7
			}

			if ((SysVIAState.ier & 0x40) && CyclesToInt == NO_TIMER_INT_DUE)
			{
				CyclesToInt = 3 + SysVIAState.timer1c;
			}

			SysVIAState.timer1hasshot = true;
		}
	}

	if (SysVIAState.timer1c < -3)
	{
		SysVIAState.timer1c += (SysVIAState.timer1l * 2) + 4;
		t1int = false;
	}

	if (SysVIAState.timer2c < -2)
	{
		if (!SysVIAState.timer2hasshot)
		{
			// DebugTrace("SysVia timer2 int at %d\n", TotalCycles);
			SysVIAState.ifr |= 0x20; // Timer 2 interrupt
			UpdateIFRTopBit();

			if ((SysVIAState.ier & 0x20) && CyclesToInt == NO_TIMER_INT_DUE)
			{
				CyclesToInt = 3 + SysVIAState.timer2c;
			}

			SysVIAState.timer2hasshot = true;
		}
	}

	if (SysVIAState.timer2c < -3)
	{
		SysVIAState.timer2c += 0x20000; // Do not reload latches for T2
	}
}

void SysVIA_poll(unsigned int ncycles)
{
	// Converted to a proc to allow shift register functions
	// ChipClock(ncycles);

	SysVIAState.timer1c -= ncycles;

	if (!(SysVIAState.acr & 0x20))
	{
		SysVIAState.timer2c -= ncycles;
	}

	if (SysVIAState.timer1c < 0 || SysVIAState.timer2c < 0)
	{
		SysVIA_poll_real();
	}

	// Ensure that CA2 keyboard interrupt is asserted when key pressed
	DoKbdIntCheck();

	// Do Shift register stuff
	// if (SRMode == 2) {
	//   Shift IN under control of Clock 2
	// SRCount = 8 - (ncycles % 8);
	// }
}

/*--------------------------------------------------------------------------*/

void SysVIAReset()
{
	VIAReset(&SysVIAState);

	// Make it no keys down and set dip switches
	BeebReleaseAllKeys();

	SRData = 0;
	SRMode = 0;
	SRCount = 0;
	SREnabled = 0; // Disable Shift register shifting shiftily. (I am nuts) - Richard Gellman
}

/*--------------------------------------------------------------------------*/

void DebugSysViaState()
{
	DebugViaState("SysVia", &SysVIAState);
}

/*--------------------------------------------------------------------------*/
