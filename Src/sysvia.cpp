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

#include <iostream>
#include <fstream>
#include <stdio.h>
#include <time.h>
#include <windows.h>

#include "6502core.h"
#include "beebsound.h"
#include "beebmem.h"
#include "beebwin.h"
#include "sysvia.h"
#include "via.h"
#include "main.h"
#include "viastate.h"
#include "debug.h"
#ifdef SPEECH_ENABLED
#include "speech.h"
#endif

#ifdef WIN32
#include <windows.h>
#endif

using namespace std;

/* Clock stuff for Master 128 RTC */
time_t SysTime;
time_t RTCTimeOffset=0;
unsigned char RTCY2KAdjust=1;

// Shift register stuff
unsigned char SRMode;
unsigned char SRCount;
unsigned char SRData;
unsigned char SREnabled;

/* Fire button for joystick 1, 0=not pressed, 1=pressed */
int JoystickButton = 0;

extern int DumpAfterEach;
/* My raw VIA state */
VIAState SysVIAState;
char WECycles=0;
char WEState=0;

/* State of the 8bit latch IC32 - bit 0 is WE for sound gen, B1 is read
   select on speech proc, B2 is write select on speech proc, b4,b5 select
   screen start address offset , b6 is CAPS lock, b7 is shift lock */
unsigned char IC32State=0;
unsigned char OldCMOSState=0;

// CMOS logging facilities
unsigned char CMOSDebug=0;
FILE *CMDF;
FILE *vialog;
/* Last value written to the slow data bus - sound reads it later */
static unsigned char SlowDataBusWriteValue=0;

/* Currently selected keyboard row, column */
static unsigned int KBDRow=0;
static unsigned int KBDCol=0;

static char SysViaKbdState[16][8]; /* Col,row */
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

void PulseSysViaCB1(void) {
/// Set IFR bit 4 - AtoD end of conversion interrupt
	if (SysVIAState.ier & 16) {
		SysVIAState.ifr|=16;
		UpdateIFRTopBit();
	}
}

/*--------------------------------------------------------------------------*/
void BeebKeyUp(int row,int col) {
  if (row<0 || col<0) return;

  /* Update keys down count - unless its shift/control */
  if ((SysViaKbdState[col][row]) && (row!=0)) KeysDown--;

  SysViaKbdState[col][row]=0;
}; /* BeebKeyUp */

/*--------------------------------------------------------------------------*/
void BeebReleaseAllKeys() {
  KeysDown = 0;
    for(int row=0;row<8;row++)
      for(int col=0;col<16;col++)
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
      if (KBDCol<15) {
        int presrow;
        for(presrow=1;presrow<8;presrow++) {
          if (SysViaKbdState[KBDCol][presrow]) {
            SysVIAState.ifr|=1;
            /*cerr << "DoKbdIntCheck: Caused interrupt case 2\n"; */
            UpdateIFRTopBit();
          }
        } /* presrow */
      } /* KBDCol range */
    } /* WriteEnable on */
  } /* Keys down and CA2 input enabled */
#ifdef KBDDEBUG
  cerr << "DoKbdIntCheck KeysDown=" << KeysDown << "pcr & c=" << (int)(SysVIAState.pcr & 0xc);
  cerr << " IC32State & 8=" << (int)(IC32State & 8) << " KBDRow=" << KBDRow << "KBDCol=" << KBDCol;
  cerr << " oldIFRflag=" << Oldflag << " Newflag=" << (int)(SysVIAState.ifr & 1) << "\n";
#endif
} /* DoKbdIntCheck */

/*--------------------------------------------------------------------------*/
void BeebKeyDown(int row,int col) {
  if (row<0 || col<0) return;

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
  if ((KBDCol>14) || (KBDRow>7)) return(0); /* Key not down if overrange - perhaps we should do something more? */

  return(SysViaKbdState[KBDCol][KBDRow]);
} /* KbdOP */


/*--------------------------------------------------------------------------*/
static void IC32Write(unsigned char Value) {
  // Hello. This is Richard Gellman. It is 10:25pm, Friday 2nd February 2001
  // I have to do CMOS RAM now. And I think I'm going slightly potty.
  // Additional, Sunday 4th February 2001. I must have been potty. the line above did read January 2000.
  int bit;
  int oldval=IC32State;
  unsigned char tmpCMOSState;


  bit=Value & 7;
  if (Value & 8) {
    IC32State|=(1<<bit);
  } else {
    IC32State&=0xff-(1<<bit);
  }
  LEDs.CapsLock=((IC32State&64)==0);
  LEDs.ShiftLock=((IC32State&128)==0);
  /* hmm, CMOS RAM? */
  // Monday 5th February 2001 - Scrapped my CMOS code, and restarted as according to the bible of the god Tom Lees
  CMOS.Op=((IC32State & 2)>>1);
  tmpCMOSState=(IC32State & 4)>>1;
  CMOS.DataStrobe=(tmpCMOSState==OldCMOSState)?0:1;
  OldCMOSState=tmpCMOSState;
  if (CMOS.DataStrobe && CMOS.Enabled && !CMOS.Op && MachineType==3) {
	  CMOSWrite(CMOS.Address,SlowDataBusWriteValue);
	  if (CMOSDebug) fprintf(CMDF,"Wrote %02x to %02x\n",SlowDataBusWriteValue,CMOS.Address);
  }
  if (CMOS.Enabled && CMOS.Op && MachineType==3) {
	  SysVIAState.ora=CMOSRead(CMOS.Address);
	  if (CMOSDebug) fprintf(CMDF,"Read %02x from %02x\n",SysVIAState.ora,CMOS.Address);
  }

  /* Must do sound reg access when write line changes */
#ifdef SOUNDSUPPORT
  if (((oldval & 1)) && (!(IC32State & 1))) { Sound_RegWrite(SlowDataBusWriteValue); }
  // now, this was a change from 0 to 1, but my docs say its a change from 1 to 0. might work better this way.
#endif
  /* cerr << "IC32State now=" << hex << int(IC32State) << dec << "\n"; */

#ifdef SPEECH_ENABLED
  if ( (bit == 2) && ( (Value & 8)  == 0) && (MachineType != 3) )		//  Write Command
  {
	  tms5220_data_w(SlowDataBusWriteValue);
  }
#endif

  if (!(IC32State & 8) && (oldval & 8)) {
    KBDRow=(SlowDataBusWriteValue>>4) & 7;
    KBDCol=(SlowDataBusWriteValue & 0xf);
    DoKbdIntCheck(); /* Should really only if write enable on KBD changes */
  }
} /* IC32Write */


void ChipClock(int Cycles) {
//	if (WECycles>0) WECycles-=Cycles;
//	else
//	if (WEState) Sound_RegWrite(SlowDataBusWriteValue);
}

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

  if (CMOS.DataStrobe && CMOS.Enabled && !CMOS.Op && MachineType==3) 
  {
        CMOSWrite(CMOS.Address,Value);
  }

#ifdef SOUNDSUPPORT
  if (!(IC32State & 1)) {
		Sound_RegWrite(SlowDataBusWriteValue);
  } 
#endif


} /* SlowDataBusWrite */


/*--------------------------------------------------------------------------*/
static int SlowDataBusRead(void) {
  int result;
  if (CMOS.Enabled && CMOS.Op && MachineType==3) 
  {
       SysVIAState.ora=CMOSRead(CMOS.Address); //SysVIAState.ddra ^ CMOSRAM[CMOS.Address];
	  if (CMOSDebug) fprintf(CMDF,"Read %02x from %02x\n",SysVIAState.ora,CMOS.Address);
  }
  result=(SysVIAState.ora & SysVIAState.ddra);
  if (CMOS.Enabled) result=(SysVIAState.ora & ~SysVIAState.ddra);
  /* I don't know this lot properly - just put in things as we figure them out */
  if (MachineType!=3) if (!(IC32State & 8)) { if (KbdOP()) result|=128; }
  if ((MachineType==3) && (!CMOS.Enabled)) {
	  if (KbdOP()) result|=128; 
  }

#ifdef SPEECH_ENABLED
  if ((!(IC32State & 2)) && (MachineType != 3) ) {
    result = tms5220_status_r();
  }
#endif

  if ((!(IC32State & 4)) && (MachineType != 3) ) {
	  result = 0xff;
  }

  /* cerr << "SlowDataBusRead giving 0x" << hex << result << dec << "\n"; */
  return(result);
} /* SlowDataBusRead */


/*--------------------------------------------------------------------------*/
/* Address is in the range 0-f - with the fe40 stripped out */
void SysVIAWrite(int Address, int Value) {
  //fprintf(vialog,"SYSTEM VIA Write of %d (%02x) to address %d\n",Value,Value,Address);
  /* cerr << "SysVIAWrite: Address=0x" << hex << Address << " Value=0x" << Value << dec << " at " << TotalCycles << "\n";
  DumpRegs(); */

	if (DebugEnabled) {
		char info[200];
		sprintf(info, "SysVia: Write address %X value %02X", (int)(Address & 0xf), Value & 0xff);
		DebugDisplayTrace(DEBUG_SYSVIA, true, info);
	}

  switch (Address) {
    case 0:
	  // Clear bit 4 of IFR from ATOD Conversion
      SysVIAState.ifr&=~16;
      SysVIAState.orb=Value & 0xff;
      IC32Write(Value);
	  CMOS.Enabled=((Value & 64)>>6); // CMOS Chip select
	  CMOS.Address=(((Value & 128)>>7)) ? SysVIAState.ora : CMOS.Address; // CMOS Address strobe
      if ((SysVIAState.ifr & 8) && ((SysVIAState.pcr & 0x20)==0)) {
        SysVIAState.ifr&=0xf7;
        UpdateIFRTopBit();
      };
	  SysVIAState.ifr&=~16;
	  UpdateIFRTopBit();
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
      SysVIAState.timer1c=SysVIAState.timer1l * 2 + 1;
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
      SysVIAState.ifr &=0xbf; /* clear timer 1 ifr (this is what Model-B does) */
      UpdateIFRTopBit();
      break;

    case 8:
      SysVIAState.timer2l&=0xff00;
      SysVIAState.timer2l|=(Value & 0xff);
      break;

    case 9:
      SysVIAState.timer2l&=0xff;
      SysVIAState.timer2l|=(Value & 0xff)<<8;
      SysVIAState.timer2c=SysVIAState.timer2l * 2 + 1;
      if (SysVIAState.timer2c == 0) SysVIAState.timer2c = 0x20000; 
      SysVIAState.ifr &=0xdf; // clear timer 2 ifr 
      UpdateIFRTopBit();
      SysVIAState.timer2hasshot=0;
      break;

    case 10:
      SRData=Value;
      break;

    case 11:
      SysVIAState.acr=Value & 0xff;
      SRMode=(Value>>2)&7;
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
  int tmp = 0xff;
  //fprintf(vialog,"SYSTEM VIA Read of address %02x (%d)\n",Address,Address);
  /* cerr << "SysVIARead: Address=0x" << hex << Address << dec << " at " << TotalCycles << "\n";
  DumpRegs(); */
  switch (Address) {
    case 0: /* IRB read */
	  // Clear bit 4 of IFR from ATOD Conversion
      SysVIAState.ifr&=~16;
      tmp=SysVIAState.orb & SysVIAState.ddrb;
      tmp |= 32;    /* Fire button 2 released */
      if (!JoystickButton)
        tmp |= 16;
      if (MachineType == 3)
      {
        tmp |= 192; /* Speech system non existant */
      }
      else
      {
#ifdef SPEECH_ENABLED
        if (SpeechDefault)
        {
          if (tms5220_int_r()) tmp |= 64;
          if (tms5220_ready_r() == 0) tmp |= 128;
        }
        else
#endif
        {
          tmp |= 192; /* Speech system non existant */
        }
      }
      UpdateIFRTopBit();
      break;

    case 2:
      tmp = SysVIAState.ddrb;
      break;

    case 3:
      tmp = SysVIAState.ddra;
      break;

    case 4: /* Timer 1 lo counter */
      if (SysVIAState.timer1c < 0)
        tmp=0xff;
      else
        tmp=(SysVIAState.timer1c / 2) & 0xff;
      SysVIAState.ifr&=0xbf; /* Clear bit 6 - timer 1 */
      UpdateIFRTopBit();
      break;

    case 5: /* Timer 1 hi counter */
      tmp=(SysVIAState.timer1c>>9) & 0xff; //K.Lowe
      break;

    case 6: /* Timer 1 lo latch */
      tmp = SysVIAState.timer1l & 0xff;
      break;

    case 7: /* Timer 1 hi latch */
      tmp = (SysVIAState.timer1l>>8) & 0xff; //K.Lowe
      break;

    case 8: /* Timer 2 lo counter */
      if (SysVIAState.timer2c < 0) /* Adjust for dividing -ve count by 2 */
        tmp=((SysVIAState.timer2c - 1) / 2) & 0xff;
      else
        tmp=(SysVIAState.timer2c / 2) & 0xff;
      SysVIAState.ifr&=0xdf; /* Clear bit 5 - timer 2 */
      UpdateIFRTopBit();
      break;

    case 9: /* Timer 2 hi counter */
      tmp = (SysVIAState.timer2c>>9) & 0xff; //K.Lowe
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
#ifdef KBDDEBUG
      cerr << "Read IFR got=0x" << hex << int(SysVIAState.ifr) << dec << "\n";
#endif
      tmp = SysVIAState.ifr;
      break;

    case 14:
      tmp = SysVIAState.ier | 0x80;
      break;

    case 1:
      SysVIAState.ifr&=0xfc;
      UpdateIFRTopBit();
    case 15:
      /* slow data bus read */
      tmp = SlowDataBusRead();
      break;
  } /* Address switch */

	if (DebugEnabled) {
		char info[200];
		sprintf(info, "SysVia: Read address %X value %02X", (int)(Address & 0xf), tmp & 0xff);
		DebugDisplayTrace(DEBUG_SYSVIA, true, info);
	}

  return(tmp);
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
  static bool t1int=false;

  if (SysVIAState.timer1c<-2 && !t1int) {
    t1int=true;
    if ((SysVIAState.timer1hasshot==0) || (SysVIAState.acr & 0x40)) {
      /* cerr << "SysVia timer1 int at " << TotalCycles << "\n"; */
      SysVIAState.ifr|=0x40; /* Timer 1 interrupt */
      UpdateIFRTopBit();
      if (SysVIAState.acr & 0x80) {
        SysVIAState.orb^=0x80; /* Toggle PB7 */
        SysVIAState.irb^=0x80; /* Toggle PB7 */
      }
      if ((SysVIAState.ier & 0x40) && CyclesToInt == NO_TIMER_INT_DUE) {
          CyclesToInt = 3 + SysVIAState.timer1c;
      }
      SysVIAState.timer1hasshot=1;
    }
  }

  if (SysVIAState.timer1c<-3) {
    SysVIAState.timer1c += (SysVIAState.timer1l * 2) + 4;
    t1int=false;
  }

  if (SysVIAState.timer2c<-2) {
    if (SysVIAState.timer2hasshot==0) {
      /* cerr << "SysVia timer2 int at " << TotalCycles << "\n"; */
      SysVIAState.ifr|=0x20; /* Timer 2 interrupt */
      UpdateIFRTopBit();
      if ((SysVIAState.ier & 0x20) && CyclesToInt == NO_TIMER_INT_DUE) {
        CyclesToInt = 3 + SysVIAState.timer2c;
      }
      SysVIAState.timer2hasshot=1;
    }
  }

  if (SysVIAState.timer2c<-3) {
    SysVIAState.timer2c += 0x20000; // Do not reload latches for T2
  }
} /* SysVIA_poll */

void SysVIA_poll(unsigned int ncycles) {
	// Converted to a proc to allow shift register functions
//	ChipClock(ncycles);

  SysVIAState.timer1c-=ncycles;
  if (!(SysVIAState.acr & 0x20))
    SysVIAState.timer2c-=ncycles;
  if ((SysVIAState.timer1c<0) || (SysVIAState.timer2c<0)) SysVIA_poll_real();

  // Ensure that CA2 keyboard interrupt is asserted when key pressed
  DoKbdIntCheck(); 

  // Do Shift register stuff
//  if (SRMode==2) {
	  // Shift IN under control of Clock 2
//	  SRCount=8-(ncycles%8);
//  }
}

/*--------------------------------------------------------------------------*/
void SysVIAReset(void) {
  int row,col;
  VIAReset(&SysVIAState);
  //vialog=fopen("/via.log","wt");

  /* Make it no keys down and no dip switches set */
  for(row=0;row<8;row++)
    for(col=0;col<16;col++)
      SysViaKbdState[col][row]=0;
	SRData=0;
	SRMode=0;
    SRCount=0;
	SREnabled=0; // Disable Shift register shifting shiftily. (I am nuts) - Richard Gellman
} /* SysVIAReset */

/*-------------------------------------------------------------------------*/
unsigned char BCD(unsigned char nonBCD) {
	// convert a decimal value to a BCD value
	return(((nonBCD/10)*16)+nonBCD%10);
}
unsigned char BCDToBin(unsigned char BCD) {
	// convert a BCD value to decimal value
	return((BCD>>4)*10+(BCD&15));
}
/*-------------------------------------------------------------------------*/
time_t CMOSConvertClock(void) {
	time_t tim;
	struct tm Base;
	Base.tm_sec = BCDToBin(CMOSRAM[0]);
	Base.tm_min = BCDToBin(CMOSRAM[2]);
	Base.tm_hour = BCDToBin(CMOSRAM[4]);
	Base.tm_mday = BCDToBin(CMOSRAM[7]);
	Base.tm_mon = BCDToBin(CMOSRAM[8])-1;
	Base.tm_year = BCDToBin(CMOSRAM[9]);
	Base.tm_wday = -1;
	Base.tm_yday = -1;
	Base.tm_isdst = -1;
	tim = mktime(&Base);
	return tim;
}
/*-------------------------------------------------------------------------*/
void RTCInit(void) {
	struct tm *CurTime;
	time( &SysTime );
	CurTime = localtime( &SysTime );
	CMOSRAM[0] = BCD(CurTime->tm_sec);
	CMOSRAM[2] = BCD(CurTime->tm_min);
	CMOSRAM[4] = BCD(CurTime->tm_hour);
	CMOSRAM[6] = BCD((CurTime->tm_wday)+1);
	CMOSRAM[7] = BCD(CurTime->tm_mday);
	CMOSRAM[8] = BCD((CurTime->tm_mon)+1);
	CMOSRAM[9] = BCD((CurTime->tm_year)-(RTCY2KAdjust ? 20 : 0));
	RTCTimeOffset = SysTime - CMOSConvertClock();
}
/*-------------------------------------------------------------------------*/
void RTCUpdate(void) {
	struct tm *CurTime;
	time( &SysTime );
	SysTime -= RTCTimeOffset;
	CurTime = localtime( &SysTime );
	CMOSRAM[0] = BCD(CurTime->tm_sec);
	CMOSRAM[2] = BCD(CurTime->tm_min);
	CMOSRAM[4] = BCD(CurTime->tm_hour);
	CMOSRAM[6] = BCD((CurTime->tm_wday)+1);
	CMOSRAM[7] = BCD(CurTime->tm_mday);
	CMOSRAM[8] = BCD((CurTime->tm_mon)+1);
	CMOSRAM[9] = BCD(CurTime->tm_year);
}
/*-------------------------------------------------------------------------*/
void CMOSWrite(unsigned char CMOSAddr,unsigned char CMOSData) {
	// Many thanks to Tom Lees for supplying me with info on the 146818 registers 
	// for these two functions.
	if (CMOSAddr>0xd) {
		CMOSRAM[CMOSAddr]=CMOSData;
	}
	else if (CMOSAddr==0xa) {
		// Control register A
		CMOSRAM[CMOSAddr]=CMOSData & 0x7f; // Top bit not writable
	}
	else if (CMOSAddr==0xb) {
		// Control register B
		// Bit-7 SET - 0=clock running, 1=clock update halted
		if (CMOSData & 0x80) {
			RTCUpdate();
		}
		else if ((CMOSRAM[CMOSAddr] & 0x80) && !(CMOSData & 0x80)) {
			// New clock settings
			time(&SysTime);
			RTCTimeOffset = SysTime - CMOSConvertClock();
		}
		CMOSRAM[CMOSAddr]=CMOSData;
	}
	else if (CMOSAddr==0xc) {
		// Control register C - read only
	}
	else if (CMOSAddr==0xd) {
		// Control register D - read only
	}
	else {
		// Clock registers
		CMOSRAM[CMOSAddr]=CMOSData;
	}
}

/*-------------------------------------------------------------------------*/
unsigned char CMOSRead(unsigned char CMOSAddr) {
	// 0x0 to 0x9 - Clock
	// 0xa to 0xd - Regs
	// 0xe to 0x3f - RAM
	if (CMOSAddr<0xa)
		RTCUpdate();
	return(CMOSRAM[CMOSAddr]);
}

/*--------------------------------------------------------------------------*/
void sysvia_dumpstate(void) {
  cerr << "Sysvia:\n";
  cerr << "  IC32State=" << IC32State << "\n";
  via_dumpstate(&SysVIAState);
}; /* sysvia_dumpstate */

void DebugSysViaState()
{
	DebugViaState("SysVia", &SysVIAState);
}
