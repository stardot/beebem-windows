/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 1994  David Alan Gilbert
Copyright (C) 1997  Mike Wyatt
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

#include <windows.h>

#include <stdio.h>
#include <time.h>

#include "UserVia.h"
#include "6502core.h"
#include "Bcd.h"
#include "Debug.h"
#include "Log.h"
#include "Main.h"
#include "Tube.h"
#include "UserPortBreakoutBox.h"
#include "UserPortRTC.h"
#include "Via.h"

// AMX mouse (see UserVia.h)
bool AMXMouseEnabled = false;
bool AMXLRForMiddle = false;
int AMXTrigger = 0;
int AMXButtons = 0;
int AMXTargetX = 0;
int AMXTargetY = 0;
int AMXCurrentX = 0;
int AMXCurrentY = 0;
int AMXDeltaX = 0;
int AMXDeltaY = 0;

/* Printer port */
bool PrinterEnabled = false;
int PrinterTrigger = 0;
static char PrinterFileName[256];
static FILE *PrinterFileHandle = NULL;

// Shift Register
static int SRTrigger = 0;
static void SRPoll();
static void UpdateSRState(bool SRrw);

/* SW RAM board */
bool SWRAMBoardEnabled = false;

/* My raw VIA state */
VIAState UserVIAState;

/*--------------------------------------------------------------------------*/
static void UpdateIFRTopBit()
{
	/* Update top bit of IFR */
	if (UserVIAState.ifr & (UserVIAState.ier & 0x7f))
		UserVIAState.ifr |= 0x80;
	else
		UserVIAState.ifr &= 0x7f;

	intStatus &= ~(1 << userVia);

	if (UserVIAState.ifr & 128)
	{
		intStatus |= 1 << userVia;
	}
}

/*--------------------------------------------------------------------------*/
/* Address is in the range 0-f - with the fe60 stripped out */
void UserVIAWrite(int Address, unsigned char Value)
{
	// DebugTrace("UserVIAWrite: Address=0x%02x Value=0x%02x\n", Address, Value);

	if (DebugEnabled)
	{
		DebugDisplayTraceF(DebugType::UserVIA, true,
		                   "UserVia: Write address %X value %02X",
		                   Address & 0xf, Value);
	}

	switch (Address)
	{
		case 0:
			UserVIAState.orb = Value;

			if ((UserVIAState.ifr & 8) && ((UserVIAState.pcr & 0x20) == 0))
			{
				UserVIAState.ifr &= 0xf7;
				UpdateIFRTopBit();
			}

			if (userPortBreakoutDialog != nullptr)
				userPortBreakoutDialog->ShowOutputs(UserVIAState.orb);

			if (UserPortRTCEnabled)
			{
				UserPortRTCWrite(Value);
			}
			break;

		case 1:
			UserVIAState.ora = Value;
			UserVIAState.ifr &= 0xfc;
			UpdateIFRTopBit();

			if (PrinterEnabled)
			{
				if (PrinterFileHandle != nullptr)
				{
					if (fputc(UserVIAState.ora, PrinterFileHandle) == EOF)
					{
						mainWin->Report(MessageType::Error,
						                "Failed to write to printer file:\n  %s", PrinterFileName);
					}
					else
					{
						fflush(PrinterFileHandle);
						SetTrigger(PRINTER_TRIGGER, PrinterTrigger);
					}
				}
				else
				{
					// Write to clipboard
					mainWin->CopyKey(UserVIAState.ora);
					SetTrigger(PRINTER_TRIGGER, PrinterTrigger);
				}
			}
			break;

		case 2:
			UserVIAState.ddrb = Value;

			if (UserPortRTCEnabled)
			{
				if ((Value & 0x07) == 0x07)
				{
					UserPortRTCResetWrite();
				}
			}
			break;

		case 3:
			UserVIAState.ddra = Value;
			break;

		case 4:
		case 6:
			// DebugTrace("UserVia Reg4 Timer1 lo Counter Write val=0x%02x at %d\n", Value, TotalCycles);
			UserVIAState.timer1l &= 0xff00;
			UserVIAState.timer1l |= Value & 0xff;
			break;

		case 5:
			// DebugTrace("UserVia Reg5 Timer1 hi Counter Write val=0x%02x at %d\n", Value, TotalCycles);
			UserVIAState.timer1l &= 0xff;
			UserVIAState.timer1l |= Value << 8;
			UserVIAState.timer1c = UserVIAState.timer1l * 2 + 1;
			UserVIAState.ifr &= 0xbf; /* clear timer 1 ifr */
			/* If PB7 toggling enabled, then lower PB7 now */
			if (UserVIAState.acr & 128)
			{
				UserVIAState.orb &= 0x7f;
				UserVIAState.irb &= 0x7f;
			}
			UpdateIFRTopBit();
			UserVIAState.timer1hasshot = false; // Added by K.Lowe 24/08/03
			break;

		case 7:
			// DebugTrace("UserVia Reg7 Timer1 hi latch Write val=0x%02x at %d\n", Value, TotalCycles);
			UserVIAState.timer1l &= 0xff;
			UserVIAState.timer1l |= Value << 8;
			UserVIAState.ifr &=0xbf; /* clear timer 1 ifr (this is what Model-B does) */
			UpdateIFRTopBit();
			break;

		case 8:
			// DebugTrace("UserVia Reg8 Timer2 lo Counter Write val=0x%02x at %d\n", Value, TotalCycles);
			UserVIAState.timer2l &= 0xff00;
			UserVIAState.timer2l |= Value;
			break;

		case 9:
			// DebugTrace("UserVia Reg9 Timer2 hi Counter Write val=0x%02x at %d\n", Value, TotalCycles);
			UserVIAState.timer2l &= 0xff;
			UserVIAState.timer2l |= Value << 8;
			UserVIAState.timer2c = UserVIAState.timer2l * 2 + 1;
			UserVIAState.ifr &= 0xdf; /* clear timer 2 ifr */
			UpdateIFRTopBit();
			UserVIAState.timer2hasshot = false; // Added by K.Lowe 24/08/03
			break;

		case 10:
			UserVIAState.sr = Value;
			UpdateSRState(true);
			break;

		case 11:
			UserVIAState.acr = Value;
			UpdateSRState(false);
			break;

		case 12:
			UserVIAState.pcr = Value;
			break;

		case 13:
			UserVIAState.ifr &= ~Value;
			UpdateIFRTopBit();
			break;

		case 14:
			// DebugTrace("User VIA Write ier Value=0x%02x\n", Value);
			if (Value & 0x80)
				UserVIAState.ier |= Value;
			else
				UserVIAState.ier &= ~Value;
			UserVIAState.ier &= 0x7f;
			UpdateIFRTopBit();
			break;

		case 15:
			UserVIAState.ora = Value;
			break;
	}
}

/*--------------------------------------------------------------------------*/

// Address is in the range 0-f - with the fe60 stripped out

unsigned char UserVIARead(int Address)
{
	unsigned char tmp = 0xff;
	// Local copy for processing middle button
	int amxButtons = AMXButtons;

	// DebugTrace("UserVIARead: Address=0x%02x at %d\n", Address, TotalCycles);

	switch (Address)
	{
		case 0: /* IRB read */
			tmp = (UserVIAState.orb & UserVIAState.ddrb) | (UserVIAState.irb & ~UserVIAState.ddrb);

			if (UserPortRTCEnabled)
			{
				tmp = (tmp & 0xfe) | (unsigned char)UserPortRTCReadBit();
			}

			if (userPortBreakoutDialog != nullptr)
				userPortBreakoutDialog->ShowInputs(tmp);

			if (AMXMouseEnabled)
			{
				if (AMXLRForMiddle)
				{
					if ((amxButtons & AMX_LEFT_BUTTON) && (amxButtons & AMX_RIGHT_BUTTON))
						amxButtons = AMX_MIDDLE_BUTTON;
				}

				if (TubeType == Tube::Master512CoPro)
				{
					tmp &= 0xf8;
					tmp |= (amxButtons ^ 7);
				}
				else
				{
					tmp &= 0x1f;
					tmp |= (amxButtons ^ 7) << 5;
					UserVIAState.ifr &= 0xe7;
				}

				UpdateIFRTopBit();

				/* Set up another interrupt if not at target */
				if ((AMXTargetX != AMXCurrentX) || (AMXTargetY != AMXCurrentY) || AMXDeltaX || AMXDeltaY)
				{
					SetTrigger(AMX_TRIGGER, AMXTrigger);
				}
				else
				{
					ClearTrigger(AMXTrigger);
				}
			}
			break;

		case 2:
			tmp = UserVIAState.ddrb;
			break;

		case 3:
			tmp = UserVIAState.ddra;
			break;

		case 4: /* Timer 1 lo counter */
			if (UserVIAState.timer1c < 0)
				tmp = 0xff;
			else
				tmp = (UserVIAState.timer1c / 2) & 0xff;
			UserVIAState.ifr &= 0xbf; /* Clear bit 6 - timer 1 */
			UpdateIFRTopBit();
			break;

		case 5: /* Timer 1 hi counter */
			tmp = (UserVIAState.timer1c >> 9) & 0xff;
			break;

		case 6: /* Timer 1 lo latch */
			tmp = UserVIAState.timer1l & 0xff;
			break;

		case 7: /* Timer 1 hi latch */
			tmp = (UserVIAState.timer1l >> 8) & 0xff;
			break;

		case 8: /* Timer 2 lo counter */
			if (UserVIAState.timer2c < 0) /* Adjust for dividing -ve count by 2 */
				tmp = ((UserVIAState.timer2c - 1) / 2) & 0xff;
			else
				tmp = (UserVIAState.timer2c / 2) & 0xff;
			UserVIAState.ifr &= 0xdf; /* Clear bit 5 - timer 2 */
			UpdateIFRTopBit();
			break;

		case 9: /* Timer 2 hi counter */
			tmp = (UserVIAState.timer2c >> 9) & 0xff;
			break;

		case 10:
			tmp = UserVIAState.sr;
			UpdateSRState(true);
			break;

		case 11:
			tmp = UserVIAState.acr;
			break;

		case 12:
			tmp = UserVIAState.pcr;
			break;

		case 13:
			UpdateIFRTopBit();
			tmp = UserVIAState.ifr;
			break;

		case 14:
			tmp = UserVIAState.ier | 0x80;
			break;

		case 1:
			UserVIAState.ifr &= 0xfc;
			UpdateIFRTopBit();
		case 15:
			tmp = 255;
			break;
	}

	if (DebugEnabled)
	{
	  DebugDisplayTraceF(DebugType::UserVIA, true,
	                     "UserVia: Read address %X value %02X",
	                     Address & 0xf, tmp & 0xff);
	}

	return tmp;
}

/*--------------------------------------------------------------------------*/
void UserVIATriggerCA1Int()
{
	/* We should be concerned with active edges etc. */
	UserVIAState.ifr |= 2; /* CA1 */
	UpdateIFRTopBit();
}

/*--------------------------------------------------------------------------*/
void UserVIA_poll_real()
{
	static bool t1int = false;

	if (UserVIAState.timer1c<-2 && !t1int)
	{
		t1int = true;
		if (!UserVIAState.timer1hasshot || (UserVIAState.acr & 0x40))
		{
			// DebugTrace("UserVIA timer1c - int at %d\n", TotalCycles);
			UserVIAState.ifr|=0x40; /* Timer 1 interrupt */
			UpdateIFRTopBit();

			if (UserVIAState.acr & 0x80)
			{
				UserVIAState.orb ^= 0x80; /* Toggle PB7 */
				UserVIAState.irb ^= 0x80; /* Toggle PB7 */
			}

			if ((UserVIAState.ier & 0x40) && CyclesToInt == NO_TIMER_INT_DUE)
			{
				CyclesToInt = 3 + UserVIAState.timer1c;
			}

			UserVIAState.timer1hasshot = true;
		}
	}

  if (UserVIAState.timer1c<-3) {
    // DebugTrace("UserVIA timer1c\n");
    UserVIAState.timer1c += (UserVIAState.timer1l * 2) + 4;
    t1int=false;
  }

	if (UserVIAState.timer2c < -2)
	{
		if (!UserVIAState.timer2hasshot)
		{
			// DebugTrace("UserVIA timer2c - int\n");
			UserVIAState.ifr |= 0x20; /* Timer 2 interrupt */
			UpdateIFRTopBit();

			if ((UserVIAState.ier & 0x20) && CyclesToInt == NO_TIMER_INT_DUE)
			{
				CyclesToInt = 3 + UserVIAState.timer2c;
			}

			UserVIAState.timer2hasshot = true; // Added by K.Lowe 24/08/03
		}
	}

	if (UserVIAState.timer2c < -3)
	{
		// DebugTrace("UserVIA timer2c\n");
		UserVIAState.timer2c += 0x20000; // Do not reload latches for T2
	}
}

void UserVIA_poll(unsigned int ncycles)
{
	// Converted to a proc to allow shift register functions

	UserVIAState.timer1c -= ncycles;

	if (!(UserVIAState.acr & 0x20))
		UserVIAState.timer2c -= ncycles;

	if (UserVIAState.timer1c < 0 || UserVIAState.timer2c < 0)
	{
		UserVIA_poll_real();
	}

	if (AMXMouseEnabled && AMXTrigger<=TotalCycles) AMXMouseMovement();
	if (PrinterEnabled && PrinterTrigger<=TotalCycles) PrinterPoll();
	if (SRTrigger<=TotalCycles) SRPoll();
}

/*--------------------------------------------------------------------------*/
void UserVIAReset()
{
	VIAReset(&UserVIAState);
	ClearTrigger(AMXTrigger);
	ClearTrigger(PrinterTrigger);
	SRTrigger = 0;
}

int sgn(int number)
{
	if (number > 0) return 1;
	if (number < 0) return -1;
	return 0;
}

/*--------------------------------------------------------------------------*/
static int SRMode = 0;

static void SRPoll()
{
	if (SRTrigger == 0)
	{
		ClearTrigger(SRTrigger);
		UpdateSRState(false);
	}
	else if (SRMode == 6)
	{
		if (!(UserVIAState.ifr & 0x04))
		{
			// Shift complete
			UserVIAState.ifr|=0x04;
			UpdateIFRTopBit();
		}
		ClearTrigger(SRTrigger);
	}
}

static void UpdateSRState(bool SRrw)
{
	SRMode = ((UserVIAState.acr >> 2) & 7);

	if (SRMode == 6 && SRTrigger == CycleCountTMax)
	{
		SetTrigger(16, SRTrigger);
	}

	if (SRrw)
	{
		if (UserVIAState.ifr & 0x04)
		{
			UserVIAState.ifr &= 0xfb;
			UpdateIFRTopBit();
		}
	}
}

/*-------------------------------------------------------------------------*/
void AMXMouseMovement()
{
	ClearTrigger(AMXTrigger);

	// Check if there is an outstanding interrupt.
	if (AMXMouseEnabled && (UserVIAState.ifr & 0x18) == 0)
	{
		int deltaX = AMXDeltaX == 0 ? AMXTargetX - AMXCurrentX : AMXDeltaX;
		int deltaY = AMXDeltaY == 0 ? AMXTargetY - AMXCurrentY : AMXDeltaY;

		if (deltaX != 0 || deltaY != 0)
		{
			int xdir = sgn(deltaX);
			int ydir = sgn(deltaY);

			int xpulse, ypulse;

			if (TubeType == Tube::Master512CoPro)
			{
				xpulse = 0x08;
				ypulse = 0x10;
			}
			else
			{
				xpulse = 0x01;
				ypulse = 0x04;
			}

			if (xdir)
			{
				if (xdir > 0)
					UserVIAState.irb &= ~xpulse;
				else
					UserVIAState.irb |= xpulse;

				if (!(UserVIAState.pcr & 0x10)) // Interrupt on falling CB1 edge
				{
					// Warp time to the falling edge, invert the input
					UserVIAState.irb ^= xpulse;
				}

				// Trigger the interrupt
				UserVIAState.ifr |= 0x10;
			}

			if (ydir)
			{
				if (ydir > 0)
					UserVIAState.irb |= ypulse;
				else
					UserVIAState.irb &= ~ypulse;

				if (!(UserVIAState.pcr & 0x40)) // Interrupt on falling CB2 edge
				{
					// Warp time to the falling edge, invert the input
					UserVIAState.irb ^= ypulse;
				}

				// Trigger the interrupt
				UserVIAState.ifr |= 0x08;
			}

			if (AMXDeltaX != 0)
				AMXDeltaX -= xdir;
			else
				AMXCurrentX += xdir;

			if (AMXDeltaY != 0)
				AMXDeltaY -= ydir;
			else
				AMXCurrentY += ydir;

			UpdateIFRTopBit();
		}
	}
}

/*-------------------------------------------------------------------------*/

// Close file if already open

static void ClosePrinterOutputFile()
{
	if (PrinterFileHandle != nullptr)
	{
		fclose(PrinterFileHandle);
		PrinterFileHandle = nullptr;
	}
}

/*-------------------------------------------------------------------------*/

bool PrinterEnable(const char *FileName)
{
	ClosePrinterOutputFile();

	if (FileName == nullptr)
	{
		PrinterEnabled = true;
		SetTrigger(PRINTER_TRIGGER, PrinterTrigger);
		return true;
	}

	strcpy(PrinterFileName, FileName);

	PrinterFileHandle = fopen(FileName, "wb");

	if (PrinterFileHandle == nullptr)
	{
		mainWin->Report(MessageType::Error, "Failed to open printer:\n  %s", PrinterFileName);
		return false;
	}
	else
	{
		PrinterEnabled = true;
		SetTrigger(PRINTER_TRIGGER, PrinterTrigger);
		return true;
	}
}

/*-------------------------------------------------------------------------*/

void PrinterDisable()
{
	ClosePrinterOutputFile();

	PrinterEnabled = false;
	ClearTrigger(PrinterTrigger);
}

/*-------------------------------------------------------------------------*/

void PrinterPoll()
{
	ClearTrigger(PrinterTrigger);
	UserVIATriggerCA1Int();

	// The CA1 interrupt is not always picked up,
	// set up a trigger just in case.
	SetTrigger(100000, PrinterTrigger);
}

/*--------------------------------------------------------------------------*/

void DebugUserViaState()
{
	DebugViaState("UserVia", &UserVIAState);
}
