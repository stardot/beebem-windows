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
/* CMOS Ram finalised 06/01/2001 - Richard Gellman */

#include <iostream.h>
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

#ifdef WIN32
#include <windows.h>
#endif

/* Clock stuff for Master 128 RTC */
time_t SysTime;
struct tm * CurTime;

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

static char SysViaKbdState[10][8]; /* Col,row */
static int KeysDown=0;

unsigned char WCDelay1,WCDelay2;

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
/*	// Set IFR bit 4
	if (SysVIAState.ier & 16) {
		SysVIAState.ifr|=16;
		UpdateIFRTopBit();
	} */
}

/*--------------------------------------------------------------------------*/
void BeebKeyUp(int row,int col) {
  /* Update keys down count - unless its shift/control */
  if ((SysViaKbdState[col][row]) && (row!=0)) KeysDown--;

  SysViaKbdState[col][row]=0;
}; /* BeebKeyUp */

/*--------------------------------------------------------------------------*/
void BeebReleaseAllKeys() {
  KeysDown = 0;
    for(int row=0;row<8;row++)
      for(int col=0;col<10;col++)
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

  DoKbdIntCheck(); /* Should really only if write enable on KBD changes */
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

  if (!(IC32State & 4)) {
  }
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
  if (MachineType!=3) { if (KbdOP()) result|=128; }
  if ((MachineType==3) && (!CMOS.Enabled)) {
	  if (KbdOP()) result|=128; 
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
  switch (Address) {
    case 0:
	  // Clear bit 4 of IFR from ATOD Conversion
      SysVIAState.ifr&=~16;
      SysVIAState.orb=Value & 0xff;
      IC32Write(Value);
	  CMOS.Enabled=((Value & 64)>>6); // CMOS Chip select
	  CMOS.Address=(((Value & 128)>>7)) ? SysVIAState.ora : CMOS.Address; // CMOS Address strobe
      if ((SysVIAState.ifr & 1) && ((SysVIAState.pcr & 2)==0)) {
        SysVIAState.ifr&=0xfe;
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
      SysVIAState.timer1c=SysVIAState.timer1l * 2;
      SysVIAState.ifr &=0xbf; /* clear timer 1 ifr */
      /* If PB7 toggling enabled, then lower PB7 now */
      if (SysVIAState.acr & 128) {
        SysVIAState.orb&=0x7f;
        SysVIAState.irb&=0x7f;
      };
      UpdateIFRTopBit();
      SysVIAState.timer1hasshot=0;
      WCDelay1=4;
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
      SysVIAState.timer2c=SysVIAState.timer2l * 2;
      SysVIAState.ifr &=0xdf; // clear timer 2 ifr 
      UpdateIFRTopBit();
      SysVIAState.timer2hasshot=0;
	  WCDelay2=4;
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
  int tmp;
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

    case 5: /* Timer 1 hi counter */
      tmp=SysVIAState.timer1c>>9; //K.Lowe
      return(tmp & 0xff);

    case 6: /* Timer 1 lo latch */
      return(SysVIAState.timer1l & 0xff);

    case 7: /* Timer 1 hi latch */
      return((SysVIAState.timer1l>>8) & 0xff); //K.Lowe

    case 8: /* Timer 2 lo counter */
      tmp=SysVIAState.timer2c / 2;
      SysVIAState.ifr&=0xdf; /* Clear bit 5 - timer 2 */
      UpdateIFRTopBit();
      return(tmp & 0xff);

    case 9: /* Timer 2 hi counter */
      return((SysVIAState.timer2c>>9) & 0xff); //K.Lowe

	case 10:
		return(SRData);

    case 11:
      return(SysVIAState.acr);

    case 12:
      return(SysVIAState.pcr);

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
  int tCycles;
  if (SysVIAState.timer1c<0) {
    tCycles=abs(SysVIAState.timer1c);
    SysVIAState.timer1c=(SysVIAState.timer1l * 2)-(tCycles-4);
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
    tCycles=abs(SysVIAState.timer2c);
    SysVIAState.timer2c=0x20000-tCycles; // Do not reload latches for T2
    if (SysVIAState.timer2hasshot==0) {
      /* cerr << "SysVia timer2 int at " << TotalCycles << "\n"; */
      SysVIAState.ifr|=0x20; /* Timer 2 interrupt */
      UpdateIFRTopBit();
    };
    SysVIAState.timer2hasshot=1;
  } /* timer2c underflow */

} /* SysVIA_poll */

void SysVIA_poll(unsigned int ncycles) {
	// Converted to a proc to allow shift register functions
//	ChipClock(ncycles);
  SysVIAState.timer1c-=(ncycles-WCDelay1); 
  SysVIAState.timer2c-=(ncycles-WCDelay2); 
  WCDelay1=WCDelay2=0;
  if ((SysVIAState.timer1c<0) || (SysVIAState.timer2c<0)) SysVIA_poll_real();

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
    for(col=0;col<10;col++)
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
/*-------------------------------------------------------------------------*/
void CMOSWrite(unsigned char CMOSAddr,unsigned char CMOSData) {
	char TmpPath[256];
	unsigned char CMA;
	// Many thanks to Tom Lees for supplying me with info on the 146818 registers 
	// for these two functions.
	// Clock registers 0-0x9h shall not be writable
	// ignore all status registers, so this function shall just write to CMOS
	// but we can leave it here for future developments
	if (CMOSAddr>0xd) {
		CMOSRAM[CMOSAddr]=CMOSData;
		// write CMOS Ram
		strcpy(TmpPath,RomPath); strcat(TmpPath,"/beebstate/cmos.ram");
		CMDF=fopen(TmpPath,"wb");
		if (CMDF != NULL)
		{
			for(CMA=0xe;CMA<64;CMA++) fputc(CMOSRAM[CMA],CMDF);
			fclose(CMDF);
		}
	}
}

/*-------------------------------------------------------------------------*/
unsigned char CMOSRead(unsigned char CMOSAddr) {
	// 0x0 to 0x9 - Clock
	// 0xa to 0xd - Regs
	// 0xe to 0x3f - RAM
	if (CMOSAddr>0xd) return(CMOSRAM[CMOSAddr]);
	if (CMOSAddr<0xa) {
		time( &SysTime );
		CurTime = localtime( &SysTime );
		if (CMOSAddr==0x00) return(BCD(CurTime->tm_sec));
		if (CMOSAddr==0x02) return(BCD(CurTime->tm_min));
		if (CMOSAddr==0x04) return(BCD(CurTime->tm_hour));
		if (CMOSAddr==0x06) return(BCD((CurTime->tm_wday)+1));
		if (CMOSAddr==0x07) return(BCD(CurTime->tm_mday));
		if (CMOSAddr==0x08) return(BCD((CurTime->tm_mon)+1));
		if (CMOSAddr==0x09) return(BCD((CurTime->tm_year)-10));
	}
	if (CMOSAddr>0x9 && CMOSAddr<0xe) return(0);
	return(0);
}

/*--------------------------------------------------------------------------*/
void sysvia_dumpstate(void) {
  cerr << "Sysvia:\n";
  cerr << "  IC32State=" << IC32State << "\n";
  via_dumpstate(&SysVIAState);
}; /* sysvia_dumpstate */
