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
/* System VIA support file for the beeb emulator- includes things like the
keyboard emulation - David Alan Gilbert 30/10/94 */

#include <iostream.h>

#include "6502core.h"
#include "beebsound.h"
#include "sysvia.h"
#include "via.h"

#ifdef WIN32
#include <windows.h>
#endif

/* Fire button for joystick 1, 0=not pressed, 1=pressed */
int JoystickButton = 0;

extern int DumpAfterEach;
/* My raw VIA state */
VIAState SysVIAState;

/* State of the 8bit latch IC32 - bit 0 is WE for sound gen, B1 is read select on speech proc, B2 is write select on speech proc, b4,b5 select screen start address offset , b6 is CAPS lock, b7 is shift lock */
unsigned char IC32State=0;

/* Last value written to the slow data bus - sound reads it later */
static unsigned char SlowDataBusWriteValue=0;

/* Currently selected keyboard row, column */
static unsigned int KBDRow=0;
static unsigned int KBDCol=0;

static char SysViaKbdState[10][8]; /* Col,row */
static int KeysDown=0;

/*--------------------------------------------------------------------------*/
static void UpdateIFRTopBit(void) {
  /* Update top bit of IFR */
  if (SysVIAState.ifr&(SysVIAState.ier&0x7f))
    SysVIAState.ifr|=0x80;
  else
    SysVIAState.ifr&=0x7f;
  intStatus&=~(1<<sysVia);
  intStatus|=((SysVIAState.ifr & 128)?(1<<sysVia):0);
}; /* UpdateIFRTopBit */

/*--------------------------------------------------------------------------*/
void BeebKeyUp(int row,int col) {
  /* Update keys down count - unless its shift/control */
  if ((SysViaKbdState[col][row]) && (row!=0)) KeysDown--;

  SysViaKbdState[col][row]=0;
}; /* BeebKeyUp */

/*--------------------------------------------------------------------------*/
void DoKbdIntCheck() {
  /* Now lets see if we just caused a CA2 interrupt - note we will flag
     it multiply - we aren't going to test for the edge */
  /* Two cases - write enable is OFF the keyboard - basically any key will cause an
     interrupt in a few cycles.
     */
  int Oldflag=(SysVIAState.ifr & 1);
  if ((KeysDown>0) && ((SysVIAState.pcr & 0xc)==4)) {
    if ((IC32State & 8)==8) {
      SysVIAState.ifr|=1; /* CA2 */
      /*cerr << "DoKbdIntCheck: Caused interrupt case 1\n"; */
      UpdateIFRTopBit();
    } else {
      if (KBDCol<10) {
        int presrow;
	for(presrow=1;presrow<8;presrow++) {
	  if (SysViaKbdState[KBDCol][presrow]) {
	    SysVIAState.ifr|=1;
	    /*cerr << "DoKbdIntCheck: Caused interrupt case 2\n"; */
	    UpdateIFRTopBit();
	  };
	}; /* presrow */
      }; /* KBDCol range */
    }; /* WriteEnable on */
  }; /* Keys down and CA2 input enabled */
#ifdef KBDDEBUG
  cerr << "DoKbdIntCheck KeysDown=" << KeysDown << "pcr & c=" << (int)(SysVIAState.pcr & 0xc);
  cerr << " IC32State & 8=" << (int)(IC32State & 8) << " KBDRow=" << KBDRow << "KBDCol=" << KBDCol;
  cerr << " oldIFRflag=" << Oldflag << " Newflag=" << (int)(SysVIAState.ifr & 1) << "\n";
#endif
} /* DoKbdIntCheck */

/*--------------------------------------------------------------------------*/
void BeebKeyDown(int row,int col) {
  /* Update keys down count - unless its shift/control */
  if ((!SysViaKbdState[col][row]) && (row!=0)) KeysDown++;

  SysViaKbdState[col][row]=1;

  DoKbdIntCheck();
#ifdef KBDDEBUG
  DumpAfterEach=1;
#endif
}; /* BeebKeyDown */


/*--------------------------------------------------------------------------*/
/* Return current state of the single bi output of the keyboard matrix - NOT the
  any keypressed interrupt */
static int KbdOP(void) {
  /* Check range validity */
  if ((KBDCol>9) || (KBDRow>7)) return(0); /* Key not down if overrange - perhaps we should do something more? */

  return(SysViaKbdState[KBDCol][KBDRow]);
} /* KbdOP */


/*--------------------------------------------------------------------------*/
static void IC32Write(unsigned char Value) {
  int bit;
  int oldval=IC32State;

  bit=Value & 7;
  if (Value & 8) {
    IC32State|=(1<<bit);
  } else {
    IC32State&=0xff-(1<<bit);
  }
  
  /* Must do sound reg access when write line changes */
#ifdef SOUNDSUPPORT
  if ((!(oldval & 1)) && ((IC32State & 1)))  Sound_RegWrite(SlowDataBusWriteValue);
#endif
  /* cerr << "IC32State now=" << hex << int(IC32State) << dec << "\n"; */

  DoKbdIntCheck(); /* Should really only if write enable on KBD changes */
} /* IC32Write */


/*--------------------------------------------------------------------------*/
static void SlowDataBusWrite(unsigned char Value) {
  SlowDataBusWriteValue=Value;
  /*cerr << "Slow data bus write IC32State=" << int(IC32State) << " Value=" << int(Value) << "\n";*/
  if (!(IC32State & 8)) {
    KBDRow=(Value>>4) & 7;
    KBDCol=(Value & 0xf);
    /*cerr << "SlowDataBusWrite to kbd  Row=" << KBDRow << " Col=" << KBDCol << "\n"; */
    DoKbdIntCheck(); /* Should really only if write enable on KBD changes */
  } /* kbd write */

#ifdef SOUNDSUPPORT
  if (!(IC32State & 1)) {
    Sound_RegWrite(SlowDataBusWriteValue);
  }
#endif

  if (!(IC32State & 4)) {
  }
} /* SlowDataBusWrite */


/*--------------------------------------------------------------------------*/
static int SlowDataBusRead(void) {
  int result=(SysVIAState.ora & SysVIAState.ddra);
  /* I don't know this lot properly - just put in things as we figure them out */
  if (KbdOP()) result|=128;

  /* cerr << "SlowDataBusRead giving 0x" << hex << result << dec << "\n"; */
  return(result);
} /* SlowDataBusRead */


/*--------------------------------------------------------------------------*/
/* Address is in the range 0-f - with the fe40 stripped out */
void SysVIAWrite(int Address, int Value) {
  /* cerr << "SysVIAWrite: Address=0x" << hex << Address << " Value=0x" << Value << dec << " at " << TotalCycles << "\n";
  DumpRegs(); */
  switch (Address) {
    case 0:
      SysVIAState.orb=Value & 0xff;
      IC32Write(Value);
      if ((SysVIAState.ifr & 1) && ((SysVIAState.pcr & 2)==0)) {
        SysVIAState.ifr&=0xfe;
	UpdateIFRTopBit();
      };
      break;

    case 1:
      SysVIAState.ora=Value & 0xff;
      SlowDataBusWrite(Value & 0xff);
      SysVIAState.ifr&=0xfc;
      UpdateIFRTopBit();
      break;

    case 2:
      SysVIAState.ddrb=Value & 0xff;
      break;

    case 3:
      SysVIAState.ddra=Value & 0xff;
      break;

    case 4:
    case 6:
      SysVIAState.timer1l&=0xff00;
      SysVIAState.timer1l|=(Value & 0xff);
      break;

    case 5:
      SysVIAState.timer1l&=0xff;
      SysVIAState.timer1l|=(Value & 0xff)<<8;
      SysVIAState.timer1c=SysVIAState.timer1l * 2;
      SysVIAState.ifr &=0xbf; /* clear timer 1 ifr */
      /* If PB7 toggling enabled, then lower PB7 now */
      if (SysVIAState.acr & 128) {
        SysVIAState.orb&=0x7f;
        SysVIAState.irb&=0x7f;
      };
      UpdateIFRTopBit();
      SysVIAState.timer1hasshot=0;
      break;

    case 7:
      SysVIAState.timer1l&=0xff;
      SysVIAState.timer1l|=(Value & 0xff)<<8;
      break;

    case 8:
      SysVIAState.timer2l&=0xff00;
      SysVIAState.timer2l|=(Value & 0xff);
      break;

    case 9:
      SysVIAState.timer2l&=0xff;
      SysVIAState.timer2l|=(Value & 0xff)<<8;
      SysVIAState.timer2c=SysVIAState.timer2l * 2;
      SysVIAState.ifr &=0xdf; /* clear timer 2 ifr */
      UpdateIFRTopBit();
      SysVIAState.timer2hasshot=0;
      break;

    case 10:
      break;

    case 11:
      SysVIAState.acr=Value & 0xff;
      break;

    case 12:
      SysVIAState.pcr=Value & 0xff;
      break;

    case 13:
      SysVIAState.ifr&=~(Value & 0xff);
      UpdateIFRTopBit();
      break;

    case 14:
      /*cerr << "Write ier Value=" << Value << "\n"; */
      if (Value & 0x80)
        SysVIAState.ier|=Value & 0xff;
      else
        SysVIAState.ier&=~(Value & 0xff);
      SysVIAState.ier&=0x7f;
      UpdateIFRTopBit();
      break;

    case 15:
      SysVIAState.ora=Value & 0xff;
      SlowDataBusWrite(Value & 0xff);
      break;
  } /* Address switch */
} /* SysVIAWrite */



/*--------------------------------------------------------------------------*/
/* Address is in the range 0-f - with the fe40 stripped out */
int SysVIARead(int Address) {
  int tmp;
  /* cerr << "SysVIARead: Address=0x" << hex << Address << dec << " at " << TotalCycles << "\n";
  DumpRegs(); */
  switch (Address) {
    case 0: /* IRB read */
      tmp=SysVIAState.orb & SysVIAState.ddrb;
      tmp |= 32;    /* Fire button 2 released */
      if (!JoystickButton)
        tmp |= 16;
      tmp |= 192; /* Speech system non existant */
      return(tmp);

    case 2:
      return(SysVIAState.ddrb);

    case 3:
      return(SysVIAState.ddra);

    case 4: /* Timer 1 lo counter */
      tmp=SysVIAState.timer1c / 2;
      SysVIAState.ifr&=0xbf; /* Clear bit 6 - timer 1 */
      UpdateIFRTopBit();
      return(tmp & 0xff);

    case 5: /* Timer 1 ho counter */
      tmp=SysVIAState.timer1c /512;
      return(tmp & 0xff);

    case 6: /* Timer 1 lo latch */
      return(SysVIAState.timer1l & 0xff);

    case 7: /* Timer 1 ho latch */
      return((SysVIAState.timer1l / 256) & 0xff);

    case 8: /* Timer 2 lo counter */
      tmp=SysVIAState.timer2c / 2;
      SysVIAState.ifr&=0xdf; /* Clear bit 5 - timer 2 */
      UpdateIFRTopBit();
      return(tmp & 0xff);

    case 9: /* Timer 2 ho counter */
      return((SysVIAState.timer2c / 512) & 0xff);

    case 13:
      UpdateIFRTopBit();
#ifdef KBDDEBUG
      cerr << "Read IFR got=0x" << hex << int(SysVIAState.ifr) << dec << "\n";
#endif

      return(SysVIAState.ifr);
      break;

    case 14:
      return(SysVIAState.ier | 0x80);

    case 1:
      SysVIAState.ifr&=0xfc;
      UpdateIFRTopBit();
    case 15:
      /* slow data bus read */
      return(SlowDataBusRead());
      break;
  } /* Address switch */
  return(0xff);
} /* SysVIARead */

/*--------------------------------------------------------------------------*/
/* Value denotes the new value - i.e. 1 for a rising edge */
void SysVIATriggerCA1Int(int value) {
  /*value^=1; */
  /*cerr << "SysVIATriggerCA1Int at " << TotalCycles << "\n"; */
  /* Cause interrupt on appropriate edge */
  if (!((SysVIAState.pcr & 1) ^ value)) {
    SysVIAState.ifr|=2; /* CA1 */
    UpdateIFRTopBit();
  };
}; /* SysVIATriggerCA1Int */

/*--------------------------------------------------------------------------*/
void SysVIA_poll_real(void) {
  if (SysVIAState.timer1c<0) {
    SysVIAState.timer1c=SysVIAState.timer1l * 2;
    if ((SysVIAState.timer1hasshot==0) || (SysVIAState.acr & 0x40)) {
      /* cerr << "SysVia timer1 int at " << TotalCycles << "\n"; */
      SysVIAState.ifr|=0x40; /* Timer 1 interrupt */
      UpdateIFRTopBit();
      if (SysVIAState.acr & 0x80) {
        SysVIAState.orb^=0x80; /* Toggle PB7 */
        SysVIAState.irb^=0x80; /* Toggle PB7 */
      };
    };
    SysVIAState.timer1hasshot=1;
  } /* timer1c underflow */

  if (SysVIAState.timer2c<0) {
    SysVIAState.timer2c=SysVIAState.timer2l * 2;
    if (SysVIAState.timer2hasshot==0) {
      /* cerr << "SysVia timer2 int at " << TotalCycles << "\n"; */
      SysVIAState.ifr|=0x20; /* Timer 2 interrupt */
      UpdateIFRTopBit();
    };
    SysVIAState.timer2hasshot=1;
  } /* timer2c underflow */

} /* SysVIA_poll */

/*--------------------------------------------------------------------------*/
void SysVIAReset(void) {
  int row,col;
  VIAReset(&SysVIAState);

  /* Make it no keys down and no dip switches set */
  for(row=0;row<8;row++)
    for(col=0;col<10;col++)
      SysViaKbdState[col][row]=0;
} /* SysVIAReset */

/*-------------------------------------------------------------------------*/
void SaveSysVIAState(unsigned char *StateData) {
	SaveVIAState(&SysVIAState, StateData);
	StateData[24] = IC32State;
}

/*-------------------------------------------------------------------------*/
void RestoreSysVIAState(unsigned char *StateData) {
	RestoreVIAState(&SysVIAState, StateData);
	IC32State = StateData[24];

	/* Reset the other globals as well */
	SlowDataBusWriteValue = 0;
	KBDRow = 0;
	KBDCol = 0;
	KeysDown = 0;
	for(int row=0;row<8;row++)
		for(int col=0;col<10;col++)
			SysViaKbdState[col][row]=0;
}

/*--------------------------------------------------------------------------*/
void sysvia_dumpstate(void) {
  cerr << "Sysvia:\n";
  cerr << "  IC32State=" << IC32State << "\n";
  via_dumpstate(&SysVIAState);
}; /* sysvia_dumpstate */
