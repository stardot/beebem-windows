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
#include "viastate.h"
#include "debug.h"
#include "tube.h"
#include "beebemrc.h"

using namespace std;

/* Real Time Clock */
int RTC_bit = 0;
int RTC_cmd = 0;
int RTC_data = 0;        // Mon    Yr   Day         Hour        Min
unsigned char RTC_ram[8] = {0x12, 0x01, 0x05, 0x00, 0x05, 0x00, 0x07, 0x00};
int RTC_Enabled = 0;
void RTCWrite(int Value, int lastValue);

bool mBreakOutWindow = false; 

int	BitKey;			// Used to store the bit key pressed while we wait 
int BitKeys[8] = {48, 49, 50, 51, 52, 53, 54, 55};
int LastBitButton = 0;
static HWND hwndBreakOut;
static HWND hwndGetBitKey;
extern HWND hCurrentDialog;

/* AMX mouse (see uservia.h) */
int AMXMouseEnabled = 0;
int AMXLRForMiddle = 0;
int AMXTrigger = 0;
int AMXButtons = 0;
int AMXTargetX = 0;
int AMXTargetY = 0;
int AMXCurrentX = 0;
int AMXCurrentY = 0;

/* Printer port */
int PrinterEnabled = 0;
int PrinterTrigger = 0;
static char PrinterFileName[256];
static FILE *PrinterFileHandle = NULL;

/* SW RAM board */
unsigned char SWRAMBoardEnabled = 0;

extern int DumpAfterEach;
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
}; /* UpdateIFRTopBit */

/*--------------------------------------------------------------------------*/
/* Address is in the range 0-f - with the fe60 stripped out */
void UserVIAWrite(int Address, int Value) {

  static int lastValue = 0xff;

  /* cerr << "UserVIAWrite: Address=0x" << hex << Address << " Value=0x" << Value << dec << " at " << TotalCycles << "\n";
  DumpRegs(); */

	if (DebugEnabled) {
		char info[200];
		sprintf(info, "UsrVia: Write address %X value %02X", (int)(Address & 0xf), Value & 0xff);
		DebugDisplayTrace(DEBUG_USERVIA, true, info);
	}

  switch (Address) {
    case 0:
      UserVIAState.orb=Value & 0xff;
      if ((UserVIAState.ifr & 8) && ((UserVIAState.pcr & 0x20)==0)) {
        UserVIAState.ifr&=0xf7;
        UpdateIFRTopBit();
      };
	  if (mBreakOutWindow)
		  ShowOutputs(UserVIAState.orb);

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
      };
      UpdateIFRTopBit();
      UserVIAState.timer1hasshot=0; //Added by K.Lowe 24/08/03
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
      /* cerr << "UserVia Reg9 Timer2 hi Counter Write val=0x" << hex << Value << dec << "\n";
      core_dumpstate(); */
      UserVIAState.timer2l&=0xff;
      UserVIAState.timer2l|=(Value & 0xff)<<8;
      UserVIAState.timer2c=UserVIAState.timer2l * 2 + 1;
      UserVIAState.ifr &=0xdf; /* clear timer 2 ifr */
      UpdateIFRTopBit();
      UserVIAState.timer2hasshot=0; //Added by K.Lowe 24/08/03
      break;

    case 10:
      break;

    case 11:
      UserVIAState.acr=Value & 0xff;
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
  } /* Address switch */
} /* UserVIAWrite */

/*--------------------------------------------------------------------------*/
/* Address is in the range 0-f - with the fe60 stripped out */
int UserVIARead(int Address) {
  int tmp = 0xff;
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

 	  if (mBreakOutWindow) ShowInputs(tmp);

	  if (AMXMouseEnabled) {
        if (AMXLRForMiddle) {
          if ((AMXButtons & AMX_LEFT_BUTTON) && (AMXButtons & AMX_RIGHT_BUTTON))
            AMXButtons = AMX_MIDDLE_BUTTON;
          else
            AMXButtons &= ~AMX_MIDDLE_BUTTON;
        }

#ifdef M512COPRO_ENABLED
        if (Tube186Enabled)
        {
            tmp &= 0xf8;
            tmp |= (AMXButtons ^ 7);
        }
        else
#endif
        {
            tmp &= 0x1f;
            tmp |= (AMXButtons ^ 7) << 5;
            UserVIAState.ifr&=0xe7;
        }
        
        UpdateIFRTopBit();

        /* Set up another interrupt if not at target */
        if ( (AMXTargetX != AMXCurrentX) || (AMXTargetY != AMXCurrentY) ) {
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
		DebugDisplayTrace(DEBUG_USERVIA, true, info);
	}

  return(tmp);
} /* UserVIARead */

/*--------------------------------------------------------------------------*/
void UserVIATriggerCA1Int(void) {
  /* We should be concerned with active edges etc. */
  UserVIAState.ifr|=2; /* CA1 */
  UpdateIFRTopBit();
}; /* UserVIATriggerCA1Int */

/*--------------------------------------------------------------------------*/
void UserVIA_poll_real(void) {
  static bool t1int=false;

  if (UserVIAState.timer1c<-2 && !t1int) {
    t1int=true;
    if ((UserVIAState.timer1hasshot==0) || (UserVIAState.acr & 0x40)) {
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
      UserVIAState.timer1hasshot=1;
	}
  }

  if (UserVIAState.timer1c<-3) {
    /*cerr << "UserVIA timer1c\n"; */
    UserVIAState.timer1c += (UserVIAState.timer1l * 2) + 4;
    t1int=false;
  }

  if (UserVIAState.timer2c<-2) {
    if (UserVIAState.timer2hasshot==0) {
      /* cerr << "UserVIA timer2c - int\n"; */
      UserVIAState.ifr|=0x20; /* Timer 2 interrupt */
      UpdateIFRTopBit();
      if ((UserVIAState.ier & 0x20) && CyclesToInt == NO_TIMER_INT_DUE) {
        CyclesToInt = 3 + UserVIAState.timer2c;
      }
      UserVIAState.timer2hasshot=1; // Added by K.Lowe 24/08/03
    }
  }

  if (UserVIAState.timer2c<-3) {
    /* cerr << "UserVIA timer2c\n"; */
    UserVIAState.timer2c += 0x20000; // Do not reload latches for T2
  }
} /* UserVIA_poll */

void UserVIA_poll(unsigned int ncycles) {
  // Converted to a proc to allow shift register functions

  UserVIAState.timer1c-=ncycles;
  if (!(UserVIAState.acr & 0x20))
    UserVIAState.timer2c-=ncycles;
  if ((UserVIAState.timer1c<0) || (UserVIAState.timer2c<0)) UserVIA_poll_real();

  if (AMXMouseEnabled && AMXTrigger<=TotalCycles) AMXMouseMovement();
  if (PrinterEnabled && PrinterTrigger<=TotalCycles) PrinterPoll();

  // Do Shift register stuff
//  if (SRMode==2) {
	  // Shift IN under control of Clock 2
//	  SRCount=8-(ncycles%8);
//  }
}


/*--------------------------------------------------------------------------*/
void UserVIAReset(void) {
  VIAReset(&UserVIAState);
  ClearTrigger(AMXTrigger);
  ClearTrigger(PrinterTrigger);
} /* UserVIAReset */


int sgn(int number)
{
	if (number > 0) return 1;
	if (number < 0) return -1;
	return 0;
}

/*-------------------------------------------------------------------------*/
void AMXMouseMovement() {
static int xdir = 0;
static int ydir = 0;
static int xpulse = 0x08;
static int ypulse = 0x10;
static int lastxdir = 0;
static int lastydir = 0;
static bool first = true;
    
    ClearTrigger(AMXTrigger);

	/* Check if there is a outstanding interrupt */
	if (AMXMouseEnabled && (UserVIAState.ifr & 0x18) == 0)
	{

    	if ( (AMXTargetX != AMXCurrentX) || (AMXTargetY != AMXCurrentY) )
		{

#ifdef M512COPRO_ENABLED
            if (Tube186Enabled)
            {
    
		    	xdir = sgn(AMXTargetX - AMXCurrentX);

			    if (xdir != 0)
    			{
	    			UserVIAState.ifr |= 0x10;
		    		if (lastxdir == xdir) UserVIAState.irb ^= xpulse;
			    	lastxdir = xdir;
				    AMXCurrentX += xdir;
    			}

	    		ydir = sgn(AMXTargetY - AMXCurrentY);

    			if (ydir != 0)
	    		{
		    		UserVIAState.ifr |= 0x08;
    
	    			if (first)
    				{
	    				UserVIAState.irb &= ~ypulse;
		    			first = false;
				    }

    				if (lastydir == ydir) UserVIAState.irb ^= ypulse;
	    			lastydir = ydir;
		    		AMXCurrentY += ydir;

			    }
            }
            else
#endif
            {

		    	if (AMXTargetX != AMXCurrentX)
			    {
    				UserVIAState.ifr |= 0x10;
	    			if (AMXTargetX < AMXCurrentX)
		    		{
			    		UserVIAState.irb &= ~0x01;
				    	AMXCurrentX--;
    				}
	    			else
		    		{
			    		UserVIAState.irb |= 0x01;
				    	AMXCurrentX++;
    				}
	    		}
		    	if (AMXTargetY != AMXCurrentY)
			    {
    				UserVIAState.ifr |= 0x08;
	    			if (AMXTargetY > AMXCurrentY)
		    		{
			    		UserVIAState.irb &= ~0x04;
				    	AMXCurrentY++;
    				}
	    			else
		    		{
			    		UserVIAState.irb |= 0x04;
    					AMXCurrentY--;
	    			}
		    	}
            }
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
		PrinterEnabled = 1;
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
		PrinterEnabled = 1;
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

	PrinterEnabled = 0;
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
		} else {
			
			if (RTC_bit == 11)		// Write data
			{
				RTC_cmd >>= 5;
				
				WriteLog("RTC Write cmd : 0x%03x, reg : 0x%02x, data = 0x%02x\n", RTC_cmd, (RTC_cmd & 0x0f) >> 1, RTC_cmd >> 4);
				
				RTC_ram[(RTC_cmd & 0x0f) >> 1] = RTC_cmd >> 4;
			} else {
				RTC_cmd >>= 12;
				
				time_t SysTime;
				struct tm * CurTime;
				
				time( &SysTime );
				CurTime = localtime( &SysTime );
				
				switch ((RTC_cmd & 0x0f) >> 1)
				{
				case 0 :
					RTC_data = BCD((CurTime->tm_mon)+1);
					break;
				case 1 :
					RTC_data = BCD((CurTime->tm_year % 100) - 1);
					break;
				case 2 :
					RTC_data = BCD(CurTime->tm_mday);
					break;
				case 3 :
					RTC_data = RTC_ram[3];
					break;
				case 4 :
					RTC_data = BCD(CurTime->tm_hour);
					break;
				case 5 :
					RTC_data = RTC_ram[5];
					break;
				case 6 :
					RTC_data = BCD(CurTime->tm_min);
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
}; /* uservia_dumpstate */

void DebugUserViaState()
{
	DebugViaState("UsrVia", &UserVIAState);
}


/*
 * Breakout box stuff
 */

void BreakOutOpenDialog(HINSTANCE hinst, HWND hwndMain)

{
	mBreakOutWindow = true;

	if (!IsWindow(hwndBreakOut)) 
	{ 
		hwndBreakOut = CreateDialog(hinst, MAKEINTRESOURCE(IDD_BREAKOUT),
										NULL, (DLGPROC)BreakOutDlgProc);
		hCurrentDialog = hwndBreakOut;
		ShowWindow(hwndBreakOut, SW_SHOW);

		ShowBitKey(0, IDK_BIT0);
		ShowBitKey(1, IDK_BIT1);
		ShowBitKey(2, IDK_BIT2);
		ShowBitKey(3, IDK_BIT3);
		ShowBitKey(4, IDK_BIT4);
		ShowBitKey(5, IDK_BIT5);
		ShowBitKey(6, IDK_BIT6);
		ShowBitKey(7, IDK_BIT7);

	}
}

void BreakOutCloseDialog()
{
	DestroyWindow(hwndBreakOut);
	hwndBreakOut = NULL;
	mBreakOutWindow = false;
	hCurrentDialog = NULL;
}

BOOL CALLBACK BreakOutDlgProc( HWND   hwnd,
									UINT   nMessage,
									WPARAM wParam,
									LPARAM lParam )
{
int bit;

	switch( nMessage )
	{
	// 
	case WM_COMMAND:
		switch( wParam )
		{
		case IDOK:
		case IDCANCEL:
			BreakOutCloseDialog();
			return TRUE;
		
		case IDK_BIT0:
			BitKey = 0;
			ShowBitKey(BitKey, IDK_BIT0);

			if ( hwndGetBitKey != NULL )
				SendMessage( hwndGetBitKey, WM_CLOSE, 0, 0L );

			hwndGetBitKey = PromptForBitKeyInput( hwnd, (UINT)wParam );

			break;

		case IDK_BIT1:
			BitKey = 1;

			ShowBitKey(BitKey, IDK_BIT1);

			if ( hwndGetBitKey != NULL )
				SendMessage( hwndGetBitKey, WM_CLOSE, 0, 0L );

			hwndGetBitKey = PromptForBitKeyInput( hwnd, (UINT)wParam );

			break;

		case IDK_BIT2:
			BitKey = 2;

			ShowBitKey(BitKey, IDK_BIT2);

			if ( hwndGetBitKey != NULL )
				SendMessage( hwndGetBitKey, WM_CLOSE, 0, 0L );

			hwndGetBitKey = PromptForBitKeyInput( hwnd, (UINT)wParam );

			break;

		case IDK_BIT3:
			BitKey = 3;

			ShowBitKey(BitKey, IDK_BIT3);

			if ( hwndGetBitKey != NULL )
				SendMessage( hwndGetBitKey, WM_CLOSE, 0, 0L );

			hwndGetBitKey = PromptForBitKeyInput( hwnd, (UINT)wParam );

			break;

		case IDK_BIT4:
			BitKey = 4;

			ShowBitKey(BitKey, IDK_BIT4);

			if ( hwndGetBitKey != NULL )
				SendMessage( hwndGetBitKey, WM_CLOSE, 0, 0L );

			hwndGetBitKey = PromptForBitKeyInput( hwnd, (UINT)wParam );

			break;

		case IDK_BIT5:
			BitKey = 5;

			ShowBitKey(BitKey, IDK_BIT5);

			if ( hwndGetBitKey != NULL )
				SendMessage( hwndGetBitKey, WM_CLOSE, 0, 0L );

			hwndGetBitKey = PromptForBitKeyInput( hwnd, (UINT)wParam );

			break;

		case IDK_BIT6:
			BitKey = 6;

			ShowBitKey(BitKey, IDK_BIT6);

			if ( hwndGetBitKey != NULL )
				SendMessage( hwndGetBitKey, WM_CLOSE, 0, 0L );

			hwndGetBitKey = PromptForBitKeyInput( hwnd, (UINT)wParam );

			break;

		case IDK_BIT7:
			BitKey = 7;

			ShowBitKey(BitKey, IDK_BIT7);

			if ( hwndGetBitKey != NULL )
				SendMessage( hwndGetBitKey, WM_CLOSE, 0, 0L );

			hwndGetBitKey = PromptForBitKeyInput( hwnd, (UINT)wParam );

			break;

		case IDC_IB7:
			bit = GetValue((int)wParam);
			if ((UserVIAState.ddrb & 0x80) == 0x00)
			{
				if (bit == 1) UserVIAState.irb &= ~0x80; else UserVIAState.irb |= 0x80;
			}
			break;

		case IDC_IB6:
			bit = GetValue((int)wParam);
			if ((UserVIAState.ddrb & 0x40) == 0x00)
			{
				if (bit == 1) UserVIAState.irb &= ~0x40; else UserVIAState.irb |= 0x40;
			}
			break;

		case IDC_IB5:
			bit = GetValue((int)wParam);
			if ((UserVIAState.ddrb & 0x20) == 0x00)
			{
				if (bit == 1) UserVIAState.irb &= ~0x20; else UserVIAState.irb |= 0x20;
			}
			break;

		case IDC_IB4:
			bit = GetValue((int)wParam);
			if ((UserVIAState.ddrb & 0x10) == 0x00)
			{
				if (bit == 1) UserVIAState.irb &= ~0x10; else UserVIAState.irb |= 0x10;
			}
			break;

		case IDC_IB3:
			bit = GetValue((int)wParam);
			if ((UserVIAState.ddrb & 0x08) == 0x00)
			{
				if (bit == 1) UserVIAState.irb &= ~0x08; else UserVIAState.irb |= 0x08;
			}
			break;

		case IDC_IB2:
			bit = GetValue((int)wParam);
			if ((UserVIAState.ddrb & 0x04) == 0x00)
			{
				if (bit == 1) UserVIAState.irb &= ~0x04; else UserVIAState.irb |= 0x04;
			}
			break;

		case IDC_IB1:
			bit = GetValue((int)wParam);
			if ((UserVIAState.ddrb & 0x02) == 0x00)
			{
				if (bit == 1) UserVIAState.irb &= ~0x02; else UserVIAState.irb |= 0x02;
			}
			break;

		case IDC_IB0:
			bit = GetValue((int)wParam);
			if ((UserVIAState.ddrb & 0x01) == 0x00)
			{
				if (bit == 1) UserVIAState.irb &= ~0x01; else UserVIAState.irb |= 0x01;
			}
			break;

		default:
		return TRUE;
		}

	default:
		return FALSE;
	};		

}

char *BitKeyName( int Key )
{
	static CHAR Character[2]; // Used to return single characters.

	switch( Key )
	{
	case   8: return "Backspace";
	case   9: return "Tab";
	case  13: return "Enter";
	case  17: return "Ctrl";
	case  18: return "Alt";
	case  19: return "Break";
	case  20: return "Caps";
	case  27: return "Esc";
	case  32: return "Spacebar";
	case  33: return "PgUp";
	case  34: return "PgDn";
	case  35: return "End";
	case  36: return "Home";
	case  37: return "Left";
	case  38: return "Up";
	case  39: return "Right";
	case  40: return "Down";
	case  45: return "Insert";
	case  46: return "Del";
	case  93: return "Menu";
	case  96: return "Pad0";
	case  97: return "Pad1";
	case  98: return "Pad2";
	case  99: return "Pad3";
	case 100: return "Pad4";
	case 101: return "Pad5";
	case 102: return "Pad6";
	case 103: return "Pad7";
	case 104: return "Pad8";
	case 105: return "Pad9";
	case 106: return "Pad*";
	case 107: return "Pad+";
	case 109: return "Pad-";
	case 110: return "Pad.";
	case 111: return "Pad/";
	case 112: return "F1";
	case 113: return "F2";
	case 114: return "F3";
	case 115: return "F4";
	case 116: return "F5";
	case 117: return "F6";
	case 118: return "F7";
	case 119: return "F8";
	case 120: return "F9";
	case 121: return "F10";
	case 122: return "F11";
	case 123: return "F12";
	case 144: return "NumLock";
	case 145: return "SclLock";
	case 186: return ";";
	case 187: return "=";
	case 188: return ",";
	case 189: return "-";
	case 190: return ".";
	case 191: return "/";
	case 192: return "\'";
	case 219: return "[";
	case 220: return "\\";
	case 221: return "]";
	case 222: return "#";
	case 223: return "`";

	default:
		Character[0] = (CHAR) LOBYTE( Key );
		Character[1] = '\0';
		return Character;
	}

}

int GetValue(int ctrlID)

{
	return (SendDlgItemMessage(hwndBreakOut, ctrlID, BM_GETCHECK, 0, 0) == BST_CHECKED);
}

void SetValue(int ctrlID, int State)
{
	SendDlgItemMessage(hwndBreakOut, ctrlID, BM_SETCHECK, State, 0);
}

void ShowOutputs(unsigned char data)
{
static unsigned char last_data = 0;
unsigned char changed_bits;
	
	if (mBreakOutWindow)
	{
		if (data != last_data)
		{
			changed_bits = data ^ last_data;
			if (changed_bits & 0x80) { if ((UserVIAState.ddrb & 0x80) == 0x80) SetValue(IDC_OB7, (data & 0x80) != 0); else SetValue(IDC_OB7, 0); }
			if (changed_bits & 0x40) { if ((UserVIAState.ddrb & 0x40) == 0x40) SetValue(IDC_OB6, (data & 0x40) != 0); else SetValue(IDC_OB6, 0); }
			if (changed_bits & 0x20) { if ((UserVIAState.ddrb & 0x20) == 0x20) SetValue(IDC_OB5, (data & 0x20) != 0); else SetValue(IDC_OB5, 0); }
			if (changed_bits & 0x10) { if ((UserVIAState.ddrb & 0x10) == 0x10) SetValue(IDC_OB4, (data & 0x10) != 0); else SetValue(IDC_OB4, 0); }
			if (changed_bits & 0x08) { if ((UserVIAState.ddrb & 0x08) == 0x08) SetValue(IDC_OB3, (data & 0x08) != 0); else SetValue(IDC_OB3, 0); }
			if (changed_bits & 0x04) { if ((UserVIAState.ddrb & 0x04) == 0x04) SetValue(IDC_OB2, (data & 0x04) != 0); else SetValue(IDC_OB2, 0); }
			if (changed_bits & 0x02) { if ((UserVIAState.ddrb & 0x02) == 0x02) SetValue(IDC_OB1, (data & 0x02) != 0); else SetValue(IDC_OB1, 0); }
			if (changed_bits & 0x01) { if ((UserVIAState.ddrb & 0x01) == 0x01) SetValue(IDC_OB0, (data & 0x01) != 0); else SetValue(IDC_OB0, 0); }
			last_data = data;
		}
	}
}

void ShowInputs(unsigned char data)
{
static unsigned char last_data = 0;
unsigned char changed_bits;

	if (mBreakOutWindow)
	{
		if (data != last_data)
		{
			changed_bits = data ^ last_data;
			if (changed_bits & 0x80) { if ((UserVIAState.ddrb & 0x80) == 0x00) SetValue(IDC_IB7, (data & 0x80) == 0); else SetValue(IDC_IB7, 0); }
			if (changed_bits & 0x40) { if ((UserVIAState.ddrb & 0x40) == 0x00) SetValue(IDC_IB6, (data & 0x40) == 0); else SetValue(IDC_IB6, 0); }
			if (changed_bits & 0x20) { if ((UserVIAState.ddrb & 0x20) == 0x00) SetValue(IDC_IB5, (data & 0x20) == 0); else SetValue(IDC_IB5, 0); }
			if (changed_bits & 0x10) { if ((UserVIAState.ddrb & 0x10) == 0x00) SetValue(IDC_IB4, (data & 0x10) == 0); else SetValue(IDC_IB4, 0); }
			if (changed_bits & 0x08) { if ((UserVIAState.ddrb & 0x08) == 0x00) SetValue(IDC_IB3, (data & 0x08) == 0); else SetValue(IDC_IB3, 0); }
			if (changed_bits & 0x04) { if ((UserVIAState.ddrb & 0x04) == 0x00) SetValue(IDC_IB2, (data & 0x04) == 0); else SetValue(IDC_IB2, 0); }
			if (changed_bits & 0x02) { if ((UserVIAState.ddrb & 0x02) == 0x00) SetValue(IDC_IB1, (data & 0x02) == 0); else SetValue(IDC_IB1, 0); }
			if (changed_bits & 0x01) { if ((UserVIAState.ddrb & 0x01) == 0x00) SetValue(IDC_IB0, (data & 0x01) == 0); else SetValue(IDC_IB0, 0); }
			last_data = data;
		}
	}
}

void ShowBitKey(int key, int ctrlID)
{
	LastBitButton = ctrlID;

	SetDlgItemText(hwndBreakOut, ctrlID, BitKeyName(BitKeys[key]));
}

/****************************************************************************/

void SetBitKey( int ctrlID )
{
	switch( ctrlID )
	{
	// Character keys.
	case IDK_BIT0 : BitKey = 0; break;
	case IDK_BIT1 : BitKey = 1; break;
	case IDK_BIT2 : BitKey = 2; break;
	case IDK_BIT3 : BitKey = 3; break;
	case IDK_BIT4 : BitKey = 4; break;
	case IDK_BIT5 : BitKey = 5; break;
	case IDK_BIT6 : BitKey = 6; break;
	case IDK_BIT7 : BitKey = 7; break;
	
	default:
		BitKey = -1;
	}
}

HWND PromptForBitKeyInput( HWND hwndParent, UINT ctrlID )
{
	int Error;
	HWND Success;
	WNDCLASS  wc;
	CHAR	szClass[12] = "BEEBGETKEY";
	CHAR	szTitle[35] = "Press the key to use..";

	// Fill in window class structure with parameters that describe the
	// main window, if it doesn't already exist.
	if (!GetClassInfo( hInst, szClass, &wc ))
	{

		wc.style		 = CS_HREDRAW | CS_VREDRAW;// Class style(s).
		wc.lpfnWndProc	 = (WNDPROC)GetBitKeyWndProc;	   // Window Procedure
		wc.cbClsExtra	 = 0;					   // No per-class extra data.
		wc.cbWndExtra	 = 0;					   // No per-window extra data.
		wc.hInstance	 = hInst;				   // Owner of this class
		wc.hIcon		 = LoadIcon(hInst, MAKEINTRESOURCE(IDI_BEEBEM));
		wc.hCursor		 = NULL;
		wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1);// Default color
		wc.lpszMenuName  = NULL; // Menu from .RC
		wc.lpszClassName = "BEEBGETKEY"; //szAppName;				// Name to register as

		// Register the window class and return success/failure code.
		(RegisterClass(&wc));
	}

	Success = CreateWindow(	szClass,	// pointer to registered class name
							szTitle,	// pointer to window name
							WS_OVERLAPPED|				  
							WS_CAPTION| DS_MODALFRAME | DS_SYSMODAL,
//				WS_SYSMENU|
//				WS_MINIMIZEBOX, // Window style.			    
							// WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CAPTION,
							//WS_SYSMENU, // Window style.			    
							80, 40,
							200,	// window width
							90,	// window height
							hwndParent,	// handle to parent or owner window
							NULL,//HMENU(IDD_GETKEY),	// handle to menu or child-window identifier
							hInst,	// handle to application instance
							NULL // pointer to window-creation data

							);
	if ( Success == NULL )
		Error = GetLastError();
	else
		ShowWindow( Success, SW_SHOW );
	
	return Success;
}

/****************************************************************************/

LRESULT CALLBACK GetBitKeyWndProc( HWND hWnd,		   // window handle
								UINT message,	   // type of message
								WPARAM uParam,	   // additional information
								LPARAM lParam)	   // additional information
{
#define IDI_TEXT 100

	switch( message )
	{
	case WM_CREATE:
		// Add the parameters required for this window. ie a Stic text control
		// and a cancel button.

		HWND hwndCtrl;
		CHAR szUsedKeys[80];

		// Change Window Font.
		PostMessage( hWnd, WM_SETFONT, (WPARAM)GetStockObject( ANSI_VAR_FONT ), 
					 MAKELPARAM(FALSE, 0) );

		// Create the static text for keys used
		hwndCtrl = CreateWindow( "STATIC", "Assigned to PC key(s): ", WS_CHILD | SS_SIMPLE | WS_VISIBLE,
								 4, 4, 
								 200-10, 16, hWnd, HMENU(IDI_TEXT), 
								 ((LPCREATESTRUCT)lParam)->hInstance, NULL );
		PostMessage( hwndCtrl, WM_SETFONT, (WPARAM)GetStockObject( ANSI_VAR_FONT ), 
					 MAKELPARAM(FALSE, 0) );

		// The keys used list.

		strcpy(szUsedKeys, BitKeyName(BitKeys[BitKey]));

		CharToOem( szUsedKeys, szUsedKeys );
		hwndCtrl = CreateWindow( "STATIC", szUsedKeys, WS_CHILD | SS_SIMPLE | WS_VISIBLE,
								 8, 20, 
								 200-10, 16, hWnd, HMENU(IDI_TEXT), 
								 ((LPCREATESTRUCT)lParam)->hInstance, NULL );
		PostMessage( hwndCtrl, WM_SETFONT, (WPARAM)GetStockObject( ANSI_VAR_FONT ), 
					 MAKELPARAM(FALSE, 0) );

		// Create the OK button.
		hwndCtrl = CreateWindow( "BUTTON", "&Ok", WS_CHILD | BS_DEFPUSHBUTTON | WS_VISIBLE, 
								 ( 200 - 50 ) / 2, 90 - 50, 
								 60, 18,
								 hWnd, HMENU(IDOK), 
								 ((LPCREATESTRUCT)lParam)->hInstance, NULL );

		PostMessage( hwndCtrl, WM_SETFONT, (WPARAM)GetStockObject( ANSI_VAR_FONT ), 
					 MAKELPARAM(FALSE, 0) );
	break;

	case WM_COMMAND:
		// Respond to the Cancel button click ( the only button ).
		PostMessage( hWnd, WM_CLOSE, 0, 0L );
		break;

	case WM_SYSKEYUP:
	case WM_KEYUP:
		// Assign the BBC key to the PC key.

		if (LastBitButton != 0)
		{
			BitKeys[BitKey] = (int)uParam;
			ShowBitKey(BitKey, LastBitButton);
		}
		
		// Close the window.
		PostMessage( hWnd, WM_CLOSE, 0, 0L );
		break;

	default:
		// Let the default procedure ehandle everything else.
		return DefWindowProc( hWnd, message, uParam, lParam );
	}
	return FALSE; // Return zero because we have processed this message.
}
