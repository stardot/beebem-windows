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

#ifdef WIN32
#include <windows.h>
#include "main.h"
#endif

#include <iostream>
#include <fstream>
#include <stdio.h>
#include <time.h>

#include "6502core.h"
#include "uservia.h"
#include "sysvia.h"
#include "via.h"
#include "debug.h"
#include "log.h"
#include "tube.h"
#include "UserPortBreakoutBox.h"

using namespace std;

/* Real Time Clock */
int RTC_bit = 0;
int RTC_cmd = 0;
int RTC_data = 0;        // Mon    Yr   Day         Hour        Min
unsigned char RTC_ram[8] = {0x12, 0x01, 0x05, 0x00, 0x05, 0x00, 0x07, 0x00};
bool RTC_Enabled = false;
static void RTCWrite(int Value, int lastValue);

/* AMX mouse (see uservia.h) */
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
static void UpdateIFRTopBit(void) {
  /* Update top bit of IFR */
  if (UserVIAState.ifr&(UserVIAState.ier&0x7f))
    UserVIAState.ifr|=0x80;
  else
    UserVIAState.ifr&=0x7f;
  intStatus&=~(1<<userVia);
  intStatus|=((UserVIAState.ifr & 128)?(1<<userVia):0);
}

/*--------------------------------------------------------------------------*/
/* Address is in the range 0-f - with the fe60 stripped out */
void UserVIAWrite(int Address, int Value) {

  static int lastValue = 0xff;

  /* cerr << "UserVIAWrite: Address=0x" << hex << Address << " Value=0x" << Value << dec << " at " << TotalCycles << "\n";
  DumpRegs(); */

  if (DebugEnabled) {
    char info[200];
    sprintf(info, "UsrVia: Write address %X value %02X", (int)(Address & 0xf), Value & 0xff);
    DebugDisplayTrace(DebugType::UserVIA, true, info);
  }

  switch (Address) {
    case 0:
      UserVIAState.orb=Value & 0xff;
      if ((UserVIAState.ifr & 8) && ((UserVIAState.pcr & 0x20)==0)) {
        UserVIAState.ifr&=0xf7;
        UpdateIFRTopBit();
      }

      if (userPortBreakoutDialog != nullptr)
        userPortBreakoutDialog->ShowOutputs(UserVIAState.orb);

	  if (RTC_Enabled)
		  RTCWrite(Value, lastValue);
	  lastValue = Value;
      break;

    case 1:
		UserVIAState.ora=Value & 0xff;
		UserVIAState.ifr&=0xfc;
		UpdateIFRTopBit();
		if (PrinterEnabled) {
			if (PrinterFileHandle != NULL)
			{
				if (fputc(UserVIAState.ora, PrinterFileHandle) == EOF) {
#ifdef WIN32
					char errstr[200];
					sprintf(errstr, "Failed to write to printer file:\n  %s", PrinterFileName);
					MessageBox(GETHWND,errstr,WindowTitle,MB_OK|MB_ICONERROR);
#else
					cerr << "Failed to write to printer file " << PrinterFileName << "\n";
#endif
				}
				else {
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
      UserVIAState.ddrb=Value & 0xff;
 	  if (RTC_Enabled && ((Value & 0x07) == 0x07)) RTC_bit = 0;
      break;

    case 3:
      UserVIAState.ddra=Value & 0xff;
      break;

    case 4:
    case 6:
      /*cerr << "UserVia Reg4 Timer1 lo Counter Write val=0x " << hex << Value << dec << " at " << TotalCycles << "\n"; */
      UserVIAState.timer1l&=0xff00;
      UserVIAState.timer1l|=(Value & 0xff);
      break;

    case 5:
      /*cerr << "UserVia Reg5 Timer1 hi Counter Write val=0x" << hex << Value << dec  << " at " << TotalCycles << "\n"; */
      UserVIAState.timer1l&=0xff;
      UserVIAState.timer1l|=(Value & 0xff)<<8;
      UserVIAState.timer1c=UserVIAState.timer1l * 2 + 1;
      UserVIAState.ifr &=0xbf; /* clear timer 1 ifr */
      /* If PB7 toggling enabled, then lower PB7 now */
      if (UserVIAState.acr & 128) {
        UserVIAState.orb&=0x7f;
        UserVIAState.irb&=0x7f;
      }
      UpdateIFRTopBit();
      UserVIAState.timer1hasshot = false; // Added by K.Lowe 24/08/03
      break;

    case 7:
      /*cerr << "UserVia Reg7 Timer1 hi latch Write val=0x" << hex << Value << dec  << " at " << TotalCycles << "\n"; */
      UserVIAState.timer1l&=0xff;
      UserVIAState.timer1l|=(Value & 0xff)<<8;
      UserVIAState.ifr &=0xbf; /* clear timer 1 ifr (this is what Model-B does) */
      UpdateIFRTopBit();
      break;

    case 8:
      /* cerr << "UserVia Reg8 Timer2 lo Counter Write val=0x" << hex << Value << dec << "\n"; */
      UserVIAState.timer2l&=0xff00;
      UserVIAState.timer2l|=(Value & 0xff);
      break;

    case 9:
      /* cerr << "UserVia Reg9 Timer2 hi Counter Write val=0x" << hex << Value << dec << "\n"; */
      UserVIAState.timer2l&=0xff;
      UserVIAState.timer2l|=(Value & 0xff)<<8;
      UserVIAState.timer2c=UserVIAState.timer2l * 2 + 1;
      UserVIAState.ifr &=0xdf; /* clear timer 2 ifr */
      UpdateIFRTopBit();
      UserVIAState.timer2hasshot = false; // Added by K.Lowe 24/08/03
      break;

    case 10:
      UserVIAState.sr=Value & 0xff;
      UpdateSRState(true);
      break;

    case 11:
      UserVIAState.acr=Value & 0xff;
      UpdateSRState(false);
      break;

    case 12:
      UserVIAState.pcr=Value & 0xff;
      break;

    case 13:
      UserVIAState.ifr&=~(Value & 0xff);
      UpdateIFRTopBit();
      break;

    case 14:
      // cerr << "User VIA Write ier Value=" << Value << "\n";
      if (Value & 0x80)
        UserVIAState.ier|=Value & 0xff;
      else
        UserVIAState.ier&=~(Value & 0xff);
      UserVIAState.ier&=0x7f;
      UpdateIFRTopBit();
      break;

    case 15:
      UserVIAState.ora=Value & 0xff;
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
  /* cerr << "UserVIARead: Address=0x" << hex << Address << dec << " at " << TotalCycles << "\n";
  DumpRegs(); */

  switch (Address) {
    case 0: /* IRB read */
      tmp=(UserVIAState.orb & UserVIAState.ddrb) | (UserVIAState.irb & (~UserVIAState.ddrb));

	  if (RTC_Enabled)
	  {
		tmp = (tmp & 0xfe) | (RTC_data & 0x01);
		RTC_data = RTC_data >> 1;
	  }

      if (userPortBreakoutDialog != nullptr)
        userPortBreakoutDialog->ShowInputs(tmp);

	  if (AMXMouseEnabled) {
        if (AMXLRForMiddle) {
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
            UserVIAState.ifr&=0xe7;
        }
        
        UpdateIFRTopBit();

        /* Set up another interrupt if not at target */
        if ((AMXTargetX != AMXCurrentX) || (AMXTargetY != AMXCurrentY) || AMXDeltaX || AMXDeltaY) {
          SetTrigger(AMX_TRIGGER, AMXTrigger);
        }
        else {
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
        tmp=0xff;
      else
        tmp=(UserVIAState.timer1c / 2) & 0xff;
      UserVIAState.ifr&=0xbf; /* Clear bit 6 - timer 1 */
      UpdateIFRTopBit();
      break;

    case 5: /* Timer 1 hi counter */
      tmp=(UserVIAState.timer1c>>9) & 0xff;
      break;

    case 6: /* Timer 1 lo latch */
      tmp = UserVIAState.timer1l & 0xff;
      break;

    case 7: /* Timer 1 hi latch */
      tmp = (UserVIAState.timer1l>>8) & 0xff;
      break;

    case 8: /* Timer 2 lo counter */
      if (UserVIAState.timer2c < 0) /* Adjust for dividing -ve count by 2 */
        tmp=((UserVIAState.timer2c - 1) / 2) & 0xff;
      else
        tmp=(UserVIAState.timer2c / 2) & 0xff;
      UserVIAState.ifr&=0xdf; /* Clear bit 5 - timer 2 */
      UpdateIFRTopBit();
      break;

    case 9: /* Timer 2 hi counter */
      tmp=(UserVIAState.timer2c>>9) & 0xff;
      break;

    case 10:
      tmp=UserVIAState.sr;
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
      UserVIAState.ifr&=0xfc;
      UpdateIFRTopBit();
    case 15:
      tmp = 255;
      break;
  } /* Address switch */

  if (DebugEnabled) {
    char info[200];
    sprintf(info, "UsrVia: Read address %X value %02X", (int)(Address & 0xf), tmp & 0xff);
    DebugDisplayTrace(DebugType::UserVIA, true, info);
  }

  return tmp;
}

/*--------------------------------------------------------------------------*/
void UserVIATriggerCA1Int(void) {
  /* We should be concerned with active edges etc. */
  UserVIAState.ifr|=2; /* CA1 */
  UpdateIFRTopBit();
}

/*--------------------------------------------------------------------------*/
void UserVIA_poll_real(void) {
  static bool t1int=false;

  if (UserVIAState.timer1c<-2 && !t1int) {
    t1int=true;
    if (!UserVIAState.timer1hasshot || (UserVIAState.acr & 0x40)) {
      /*cerr << "UserVIA timer1c - int at " << TotalCycles << "\n"; */
      UserVIAState.ifr|=0x40; /* Timer 1 interrupt */
      UpdateIFRTopBit();
      if (UserVIAState.acr & 0x80) {
        UserVIAState.orb^=0x80; /* Toggle PB7 */
        UserVIAState.irb^=0x80; /* Toggle PB7 */
      }
      if ((UserVIAState.ier & 0x40) && CyclesToInt == NO_TIMER_INT_DUE) {
        CyclesToInt = 3 + UserVIAState.timer1c;
      }
      UserVIAState.timer1hasshot = true;
    }
  }

  if (UserVIAState.timer1c<-3) {
    /*cerr << "UserVIA timer1c\n"; */
    UserVIAState.timer1c += (UserVIAState.timer1l * 2) + 4;
    t1int=false;
  }

  if (UserVIAState.timer2c<-2) {
    if (!UserVIAState.timer2hasshot) {
      /* cerr << "UserVIA timer2c - int\n"; */
      UserVIAState.ifr|=0x20; /* Timer 2 interrupt */
      UpdateIFRTopBit();
      if ((UserVIAState.ier & 0x20) && CyclesToInt == NO_TIMER_INT_DUE) {
        CyclesToInt = 3 + UserVIAState.timer2c;
      }
      UserVIAState.timer2hasshot = true; // Added by K.Lowe 24/08/03
    }
  }

  if (UserVIAState.timer2c<-3) {
    /* cerr << "UserVIA timer2c\n"; */
    UserVIAState.timer2c += 0x20000; // Do not reload latches for T2
  }
}

void UserVIA_poll(unsigned int ncycles) {
  // Converted to a proc to allow shift register functions

  UserVIAState.timer1c-=ncycles;
  if (!(UserVIAState.acr & 0x20))
    UserVIAState.timer2c-=ncycles;
  if ((UserVIAState.timer1c<0) || (UserVIAState.timer2c<0)) UserVIA_poll_real();

  if (AMXMouseEnabled && AMXTrigger<=TotalCycles) AMXMouseMovement();
  if (PrinterEnabled && PrinterTrigger<=TotalCycles) PrinterPoll();
  if (SRTrigger<=TotalCycles) SRPoll();
}

/*--------------------------------------------------------------------------*/
void UserVIAReset(void) {
  VIAReset(&UserVIAState);
  ClearTrigger(AMXTrigger);
  ClearTrigger(PrinterTrigger);
  SRTrigger=0;
} /* UserVIAReset */


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
	int xdir = 0, ydir = 0;
	int xpulse, ypulse;

	ClearTrigger(AMXTrigger);

	/* Check if there is a outstanding interrupt */
	if (AMXMouseEnabled && (UserVIAState.ifr & 0x18) == 0)
	{
		int deltaX = AMXDeltaX == 0 ? AMXTargetX - AMXCurrentX : AMXDeltaX;
		int deltaY = AMXDeltaY == 0 ? AMXTargetY - AMXCurrentY : AMXDeltaY;

		if (deltaX != 0 || deltaY != 0)
		{
			xdir = sgn(deltaX);
			ydir = sgn(deltaY);

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

				if (!(UserVIAState.pcr & 0x10))            // Interrupt on falling CB1 edge
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

				if (!(UserVIAState.pcr & 0x40))	           // Interrupt on falling CB2 edge
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
void PrinterEnable(char *FileName) {
	/* Close file if already open */
	if (PrinterFileHandle != NULL)
	{
		fclose(PrinterFileHandle);
		PrinterFileHandle = NULL;
	}

	if (FileName == NULL)
	{
		PrinterEnabled = true;
		SetTrigger(PRINTER_TRIGGER, PrinterTrigger);
		return;
	}

	strcpy(PrinterFileName, FileName);
	PrinterFileHandle = fopen(FileName, "wb");
	if (PrinterFileHandle == NULL)
	{
#ifdef WIN32
		char errstr[200];
		sprintf(errstr, "Failed to open printer:\n  %s", PrinterFileName);
		MessageBox(GETHWND,errstr,WindowTitle,MB_OK|MB_ICONERROR);
#else
		cerr << "Failed to open printer " << PrinterFileName << "\n";
#endif
	}
	else
	{
		PrinterEnabled = true;
		SetTrigger(PRINTER_TRIGGER, PrinterTrigger);
	}
}

/*-------------------------------------------------------------------------*/
void PrinterDisable() {
	if (PrinterFileHandle != NULL)
	{
		fclose(PrinterFileHandle);
		PrinterFileHandle = NULL;
	}

	PrinterEnabled = false;
	ClearTrigger(PrinterTrigger);
}
/*-------------------------------------------------------------------------*/
void PrinterPoll() {
	ClearTrigger(PrinterTrigger);
	UserVIATriggerCA1Int();

	/* The CA1 interrupt is not always picked up,
		set up a trigger just in case. */
	SetTrigger(100000, PrinterTrigger);
}
/*--------------------------------------------------------------------------*/
void RTCWrite(int Value, int lastValue)
{
	if ( ((lastValue & 0x02) == 0x02) && ((Value & 0x02) == 0x00) )		// falling clock edge
	{
		if ((Value & 0x04) == 0x04)
		{
			RTC_cmd = (RTC_cmd >> 1) | ((Value & 0x01) << 15);
			RTC_bit++;

			WriteLog("RTC Shift cmd : 0x%03x, bit : %d\n", RTC_cmd, RTC_bit);
		}
		else
		{
			if (RTC_bit == 11) // Write data
			{
				RTC_cmd >>= 5;

				WriteLog("RTC Write cmd : 0x%03x, reg : 0x%02x, data = 0x%02x\n", RTC_cmd, (RTC_cmd & 0x0f) >> 1, RTC_cmd >> 4);

				RTC_ram[(RTC_cmd & 0x0f) >> 1] = (unsigned char)(RTC_cmd >> 4);
			}
			else
			{
				RTC_cmd >>= 12;

				time_t SysTime;
				time(&SysTime);

				struct tm* CurTime = localtime(&SysTime);

				switch ((RTC_cmd & 0x0f) >> 1)
				{
				case 0 :
					RTC_data = BCD((unsigned char)(CurTime->tm_mon + 1));
					break;
				case 1 :
					RTC_data = BCD((CurTime->tm_year % 100) - 1);
					break;
				case 2 :
					RTC_data = BCD((unsigned char)CurTime->tm_mday);
					break;
				case 3 :
					RTC_data = RTC_ram[3];
					break;
				case 4 :
					RTC_data = BCD((unsigned char)CurTime->tm_hour);
					break;
				case 5 :
					RTC_data = RTC_ram[5];
					break;
				case 6 :
					RTC_data = BCD((unsigned char)CurTime->tm_min);
					break;
				case 7 :
					RTC_data = RTC_ram[7];
					break;
				}
				
				WriteLog("RTC Read cmd : 0x%03x, reg : 0x%02x, data : 0x%02x\n", RTC_cmd, (RTC_cmd & 0x0f) >> 1, RTC_data);
			}
		}
	}
}

/*--------------------------------------------------------------------------*/
void uservia_dumpstate(void) {
  cerr << "Uservia:\n";
  via_dumpstate(&UserVIAState);
}

void DebugUserViaState()
{
	DebugViaState("UsrVia", &UserVIAState);
}

