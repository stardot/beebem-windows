/****************************************************************************/
/*              Beebem - (c) David Alan Gilbert 1994                        */
/*              ------------------------------------                        */
/* This program may be distributed freely within the following restrictions:*/
/*                                                                          */
/* 1) You may not charge for this program or for any part of it.            */
/* 2) This copyright message must be distributed with all copies.           */
/* 3) This program must be distributed complete with source code.  Binary   */
/*    only distribution is not permitted.                                   */
/* 4) The author offers no warrenties, or guarentees etc. - you use it at   */
/*    your own risk.  If it messes something up or destroys your computer   */
/*    thats YOUR problem.                                                   */
/* 5) You may use small sections of code from this program in your own      */
/*    applications - but you must acknowledge its use.  If you plan to use  */
/*    large sections then please ask the author.                            */
/*                                                                          */
/* If you do not agree with any of the above then please do not use this    */
/* program.                                                                 */
/* Please report any problems to the author at beebem@treblig.org           */
/****************************************************************************/
/* User VIA support file for the beeb emulator - David Alan Gilbert 11/12/94 */
/* Modified from the system via */

#include <iostream.h>
#include <stdio.h>

#include "6502core.h"
#include "uservia.h"
#include "via.h"
#include "viastate.h"
#include "debug.h"

#ifdef WIN32
#include <windows.h>
#include "main.h"
#endif

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

extern int DumpAfterEach;
/* My raw VIA state */
VIAState UserVIAState;

unsigned char WTDelay1,WTDelay2; // Timer write delay - one cycle

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
      if ((UserVIAState.ifr & 1) && ((UserVIAState.pcr & 2)==0)) {
        UserVIAState.ifr&=0xfe;
        UpdateIFRTopBit();
      };
      break;

    case 1:
      UserVIAState.ora=Value & 0xff;
      UserVIAState.ifr&=0xfc;
      UpdateIFRTopBit();
      if (PrinterEnabled) {
        if (fputc(UserVIAState.ora, PrinterFileHandle) == EOF) {
#ifdef WIN32
          char errstr[200];
          sprintf(errstr, "Failed to write to printer file:\n  %s", PrinterFileName);
          MessageBox(GETHWND,errstr,"BBC Emulator",MB_OK|MB_ICONERROR);
#else
          cerr << "Failed to write to printer file " << PrinterFileName << "\n";
#endif
        }
        else {
          fflush(PrinterFileHandle);
          SetTrigger(PRINTER_TRIGGER, PrinterTrigger);
        }
      }
      break;

    case 2:
      UserVIAState.ddrb=Value & 0xff;
      break;

    case 3:
      UserVIAState.ddra=Value & 0xff;
      break;

    case 4:
      /*cerr << "UserVia Reg4 Timer1 lo Counter Write val=0x " << hex << Value << dec << " at " << TotalCycles << "\n"; */
      UserVIAState.timer1l&=0xff00;
      UserVIAState.timer1l|=(Value & 0xff);
      break;

    case 5:
      /*cerr << "UserVia Reg5 Timer1 hi Counter Write val=0x" << hex << Value << dec  << " at " << TotalCycles << "\n"; */
      UserVIAState.timer1l&=0xff;
      UserVIAState.timer1l|=(Value & 0xff)<<8;
      UserVIAState.timer1c=UserVIAState.timer1l * 2;
      UserVIAState.ifr &=0xbf; /* clear timer 1 ifr */
      /* If PB7 toggling enabled, then lower PB7 now */
      if (UserVIAState.acr & 128) {
        UserVIAState.orb&=0x7f;
        UserVIAState.irb&=0x7f;
      };
      UpdateIFRTopBit();
      UserVIAState.timer1hasshot=0; //Added by K.Lowe 24/08/03
      WTDelay1=4;
      break;

//Added by K.Lowe 24/08/03
    case 6:
      /*cerr << "UserVia Reg6 Timer1 lo Counter Write val=0x " << hex << Value << dec << " at " << TotalCycles << "\n"; */
      UserVIAState.timer1l&=0xff00;
      UserVIAState.timer1l|=(Value & 0xff);
      break;
//End of Add

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
      UserVIAState.timer2c=UserVIAState.timer2l * 2;
      UserVIAState.ifr &=0xdf; /* clear timer 2 ifr */
      UpdateIFRTopBit();
      UserVIAState.timer2hasshot=0; //Added by K.Lowe 24/08/03
      WTDelay2=4;
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

      if (AMXMouseEnabled) {
        if (AMXLRForMiddle) {
          if ((AMXButtons & AMX_LEFT_BUTTON) && (AMXButtons & AMX_RIGHT_BUTTON))
            AMXButtons = AMX_MIDDLE_BUTTON;
          else
            AMXButtons &= ~AMX_MIDDLE_BUTTON;
        }

        tmp &= 0x1f;
        tmp |= (AMXButtons ^ 7) << 5;
        UserVIAState.ifr&=0xe7;
        UpdateIFRTopBit();

        /* Set up another interrupt if not at target */
        if (AMXTargetX != AMXCurrentX || AMXTargetY != AMXCurrentY) {
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
      return(UserVIAState.ifr);
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
  int tCycles;
  if (UserVIAState.timer1c<0) {
    tCycles=abs(UserVIAState.timer1c);
    /*cerr << "UserVIA timer1c\n"; */
    UserVIAState.timer1c=(UserVIAState.timer1l * 2)-(tCycles-4);
    if ((UserVIAState.timer1hasshot==0) || (UserVIAState.acr & 0x40)) {
      /*cerr << "UserVIA timer1c - int at " << TotalCycles << "\n"; */
      UserVIAState.ifr|=0x40; /* Timer 1 interrupt */
      UpdateIFRTopBit();
      if (UserVIAState.acr & 0x80) {
        UserVIAState.orb^=0x80; /* Toggle PB7 */
        UserVIAState.irb^=0x80; /* Toggle PB7 */
      };
    };
    UserVIAState.timer1hasshot=1;
  } /* timer1c underflow */

  if (UserVIAState.timer2c<0) {
    tCycles=abs(UserVIAState.timer2c);
    /* cerr << "UserVIA timer2c\n"; */
    UserVIAState.timer2c=0x20000-tCycles; // Do not reload latches for T2
    if (UserVIAState.timer2hasshot==0) {
     /* cerr << "UserVIA timer2c - int\n"; */
      UserVIAState.ifr|=0x20; /* Timer 2 interrupt */
      UpdateIFRTopBit();
    };
    UserVIAState.timer2hasshot=1; // Added by K.Lowe 24/08/03
  } /* timer2c underflow */
} /* UserVIA_poll */

void UserVIA_poll(unsigned int ncycles) {
  // Converted to a proc to allow shift register functions
  UserVIAState.timer1c-=(ncycles-WTDelay1); 
  UserVIAState.timer2c-=(ncycles-WTDelay2); 
  WTDelay1=WTDelay2=0;
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

/*-------------------------------------------------------------------------*/
void AMXMouseMovement() {
	ClearTrigger(AMXTrigger);

	/* Check if there is a outstanding interrupt */
	if (AMXMouseEnabled && (UserVIAState.ifr & 0x18) == 0)
	{
		if (AMXTargetX != AMXCurrentX || AMXTargetY != AMXCurrentY)
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

	strcpy(PrinterFileName, FileName);
	PrinterFileHandle = fopen(FileName, "wb");
	if (PrinterFileHandle == NULL)
	{
#ifdef WIN32
		char errstr[200];
		sprintf(errstr, "Failed to open printer:\n  %s", PrinterFileName);
		MessageBox(GETHWND,errstr,"BBC Emulator",MB_OK|MB_ICONERROR);
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
void uservia_dumpstate(void) {
  cerr << "Uservia:\n";
  via_dumpstate(&UserVIAState);
}; /* uservia_dumpstate */

void DebugUserViaState()
{
	DebugViaState("UsrVia", &UserVIAState);
}
