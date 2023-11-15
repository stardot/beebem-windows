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

#include <stdio.h>
#include <time.h>
#include <windows.h>

#include "6502core.h"
#include "Bcd.h"
#include "beebsound.h"
#include "beebmem.h"
#include "beebwin.h"
#include "sysvia.h"
#include "via.h"
#include "Main.h"
#include "Debug.h"
#include "DebugTrace.h"
#ifdef SPEECH_ENABLED
#include "speech.h"
#endif

// Shift register stuff
unsigned char SRMode;
unsigned char SRCount;
unsigned char SRData;
unsigned char SREnabled;

// Fire button for joystick 1 and 2, false=not pressed, true=pressed
bool JoystickButton[2] = { false, false };

/* My raw VIA state */
VIAState SysVIAState;
char WECycles=0;
char WEState=0;

/* State of the 8bit latch IC32 - bit 0 is WE for sound gen, B1 is read
   select on speech proc, B2 is write select on speech proc, b4,b5 select
   screen start address offset , b6 is CAPS lock, b7 is shift lock */
unsigned char IC32State=0;

/* Last value written to the slow data bus - sound reads it later */
static unsigned char SlowDataBusWriteValue=0;

/* Currently selected keyboard row, column */
static unsigned int KBDRow=0;
static unsigned int KBDCol=0;

static bool SysViaKbdState[16][8]; // Col, row
static int KeysDown=0;

// Master 128 MC146818AP Real-Time Clock and RAM
static time_t RTCTimeOffset = 0;

struct CMOSType
{
	bool Enabled;
	// unsigned char ChipSelect;
	unsigned char Address;
	// unsigned char StrobedData;
	bool DataStrobe;
	bool Op;
};

static struct CMOSType CMOS;
static bool OldCMOSState = false;
unsigned char CMOSRAM[64]; // RTC registers + 50 bytes CMOS RAM

// 0x0: Seconds (0 to 59)
// 0x1: Alarm Seconds (0 to 59)
// 0x2: Minutes (0 to 59)
// 0x3: Alarm Minutes (0 to 59)
// 0x4: Hours (0 to 23)
// 0x5: Alarm Hours (0 to 23)
// 0x6: Day of week (1 = Sun, 7 = Sat)
// 0x7: Day of month (1 to 31)
// 0x8: Month (1 = Jan, 12 = Dec)
// 0x9: Year (last 2 digits)
// 0xA: Register A
// 0xB: Register B
// 0xC: Register C
// 0xD: Register D
// 0xE to 0x3F: User RAM (50 bytes)

// Backup of CMOS Defaults (RAM bytes only)
const unsigned char CMOSDefault[50] =
{
	0x00, 0x00, 0x00, 0x00, 0x00,
	0xc9, 0xff, 0xfe, 0x32, 0x00,
	0x07, 0xc1, 0x1e, 0x05, 0x00,
	0x59, 0xa2
};

/*--------------------------------------------------------------------------*/
static void UpdateIFRTopBit(void) {
  /* Update top bit of IFR */
  if (SysVIAState.ifr&(SysVIAState.ier&0x7f))
    SysVIAState.ifr|=0x80;
  else
    SysVIAState.ifr&=0x7f;
  intStatus&=~(1<<sysVia);
  intStatus|=((SysVIAState.ifr & 128)?(1<<sysVia):0);
}

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

  SysViaKbdState[col][row] = false;
}

/*--------------------------------------------------------------------------*/
void BeebReleaseAllKeys() {
  KeysDown = 0;

  for (int row = 0;row < 8; row++) {
    for (int col = 0; col < 16; col++) {
      SysViaKbdState[col][row] = false;
    }
  }
}

/*--------------------------------------------------------------------------*/
void DoKbdIntCheck() {
  /* Now lets see if we just caused a CA2 interrupt - note we will flag
     it multiply - we aren't going to test for the edge */
  /* Two cases - write enable is OFF the keyboard - basically any key will cause an
     interrupt in a few cycles.
     */
#ifdef KBDDEBUG
  int Oldflag=(SysVIAState.ifr & 1);
#endif

  if ((KeysDown>0) && ((SysVIAState.pcr & 0xc)==4)) {
    if ((IC32State & 8)==8) {
      SysVIAState.ifr|=1; /* CA2 */
      // DebugTrace("DoKbdIntCheck: Caused interrupt case 1\n");
      UpdateIFRTopBit();
    } else {
      if (KBDCol<15) {
        int presrow;
        for(presrow=1;presrow<8;presrow++) {
          if (SysViaKbdState[KBDCol][presrow]) {
            SysVIAState.ifr|=1;
            // DebugTrace("DoKbdIntCheck: Caused interrupt case 2\n");
            UpdateIFRTopBit();
          }
        } /* presrow */
      } /* KBDCol range */
    } /* WriteEnable on */
  } /* Keys down and CA2 input enabled */

#ifdef KBDDEBUG
  DebugTrace("DoKbdIntCheck KeysDown=%d pcr & c=%d IC32State & 8=%d "
             "KBDRow=%d KBDCol=%d oldIFRflag=%d Newflag=%d\n",
             KeysDown, SysVIAState.pcr & 0xc, IC32State & 8,
             KBDRow, KBDCol, Oldflag, SysVIAState.ifr & 1);
#endif
}

/*--------------------------------------------------------------------------*/
void BeebKeyDown(int row,int col) {
  if (row<0 || col<0) return;

  /* Update keys down count - unless its shift/control */
  if ((!SysViaKbdState[col][row]) && (row!=0)) KeysDown++;

  SysViaKbdState[col][row] = true;

  DoKbdIntCheck();
}

/*--------------------------------------------------------------------------*/
/* Return current state of the single bi output of the keyboard matrix - NOT the
  any keypressed interrupt */
static bool KbdOP() {
  // Check range validity
  if (KBDCol > 14 || KBDRow > 7) return false; // Key not down if overrange - perhaps we should do something more?

  return SysViaKbdState[KBDCol][KBDRow];
}

/*--------------------------------------------------------------------------*/
static void IC32Write(unsigned char Value) {
  // Hello. This is Richard Gellman. It is 10:25pm, Friday 2nd February 2001
  // I have to do CMOS RAM now. And I think I'm going slightly potty.
  // Additional, Sunday 4th February 2001. I must have been potty. the line above did read January 2000.
  int bit;
  int oldval=IC32State;
  bool tmpCMOSState;

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
  CMOS.Op = (IC32State & 2) != 0;
  tmpCMOSState = (IC32State & 4) != 0;
  CMOS.DataStrobe = (tmpCMOSState == OldCMOSState) ? false : true;
  OldCMOSState = tmpCMOSState;
  if (CMOS.DataStrobe && CMOS.Enabled && !CMOS.Op && MachineType == Model::Master128) {
    CMOSWrite(CMOS.Address, SlowDataBusWriteValue);
  }
  if (CMOS.Enabled && CMOS.Op && MachineType == Model::Master128) {
    SysVIAState.ora = CMOSRead(CMOS.Address);
  }

  /* Must do sound reg access when write line changes */
#ifdef SOUNDSUPPORT
  if (((oldval & 1)) && (!(IC32State & 1))) { Sound_RegWrite(SlowDataBusWriteValue); }
  // now, this was a change from 0 to 1, but my docs say its a change from 1 to 0. might work better this way.
#endif
  // DebugTrace("IC32State now=%x\n", IC32State);

#ifdef SPEECH_ENABLED
  if (bit == 2 && (Value & 8) == 0 && MachineType != Model::Master128) // Write Command
  {
	  tms5220_data_w(SlowDataBusWriteValue);
  }
#endif

  if (!(IC32State & 8) && (oldval & 8)) {
    KBDRow=(SlowDataBusWriteValue>>4) & 7;
    KBDCol=(SlowDataBusWriteValue & 0xf);
    DoKbdIntCheck(); /* Should really only if write enable on KBD changes */
  }
}

void ChipClock(int /* nCycles */) {
//	if (WECycles > 0) WECycles -= nCycles;
//	else
//	if (WEState) Sound_RegWrite(SlowDataBusWriteValue);
}

/*--------------------------------------------------------------------------*/
static void SlowDataBusWrite(unsigned char Value) {
  SlowDataBusWriteValue=Value;
  // DebugTrace("Slow data bus write IC32State=%d Value=0x%02x\n", IC32State, Value);
  if (!(IC32State & 8)) {
    KBDRow=(Value>>4) & 7;
    KBDCol=(Value & 0xf);
    // DebugTrace("SlowDataBusWrite to kbd  Row=%d Col=%d\n", KBDRow, KBDCol);
    DoKbdIntCheck(); /* Should really only if write enable on KBD changes */
  } /* kbd write */

  if (CMOS.DataStrobe && CMOS.Enabled && !CMOS.Op && MachineType == Model::Master128)
  {
    CMOSWrite(CMOS.Address,Value);
  }

#ifdef SOUNDSUPPORT
  if (!(IC32State & 1)) {
		Sound_RegWrite(SlowDataBusWriteValue);
  } 
#endif

}

/*--------------------------------------------------------------------------*/
static unsigned char SlowDataBusRead() {
  if (CMOS.Enabled && CMOS.Op && MachineType == Model::Master128)
  {
    SysVIAState.ora = CMOSRead(CMOS.Address); // SysVIAState.ddra ^ CMOSRAM[CMOS.Address];
  }

  unsigned char result = SysVIAState.ora & SysVIAState.ddra;
  if (CMOS.Enabled) result=(SysVIAState.ora & ~SysVIAState.ddra);
  /* I don't know this lot properly - just put in things as we figure them out */
  if (MachineType != Model::Master128) if (!(IC32State & 8)) { if (KbdOP()) result|=128; }
  if ((MachineType == Model::Master128) && !CMOS.Enabled) {
    if (KbdOP()) result |= 128;
  }

#ifdef SPEECH_ENABLED
  if (!(IC32State & 2) && Model::Master128) {
    result = tms5220_status_r();
  }
#endif

  if ((!(IC32State & 4)) && (MachineType != Model::Master128) ) {
    result = 0xff;
  }

  // DebugTrace("SlowDataBusRead giving 0x%02x\n", result);

  return result;
}

/*--------------------------------------------------------------------------*/
/* Address is in the range 0-f - with the fe40 stripped out */
void SysVIAWrite(int Address, unsigned char Value)
{
  // DebugTrace("SysVIAWrite: Address=0x%02x Value=0x%02x\n", Address, Value);

  if (DebugEnabled)
  {
    DebugDisplayTraceF(DebugType::SysVIA, true,
                       "SysVia: Write address %X value %02X",
                       Address & 0xf, Value);
  }

  switch (Address) {
    case 0:
      // Clear bit 4 of IFR from ATOD Conversion
      SysVIAState.ifr&=~16;
      SysVIAState.orb = Value;
      IC32Write(Value);
      CMOS.Enabled = (Value & 64) != 0; // CMOS Chip select
      CMOS.Address=(((Value & 128)>>7)) ? SysVIAState.ora : CMOS.Address; // CMOS Address strobe
      if ((SysVIAState.ifr & 8) && ((SysVIAState.pcr & 0x20)==0)) {
        SysVIAState.ifr&=0xf7;
        UpdateIFRTopBit();
      }
      SysVIAState.ifr&=~16;
      UpdateIFRTopBit();
      break;

    case 1:
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
      SysVIAState.timer1c=SysVIAState.timer1l * 2 + 1;
      SysVIAState.ifr &=0xbf; /* clear timer 1 ifr */
      /* If PB7 toggling enabled, then lower PB7 now */
      if (SysVIAState.acr & 128) {
        SysVIAState.orb&=0x7f;
        SysVIAState.irb&=0x7f;
      }
      UpdateIFRTopBit();
      SysVIAState.timer1hasshot = false;
      break;

    case 7:
      SysVIAState.timer1l &= 0xff;
      SysVIAState.timer1l |= Value << 8;
      SysVIAState.ifr &=0xbf; /* clear timer 1 ifr (this is what Model-B does) */
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
      SRData=Value;
      break;

    case 11:
      SysVIAState.acr = Value;
      SRMode=(Value>>2)&7;
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
        SysVIAState.ier |= Value;
      else
        SysVIAState.ier &= ~Value;
      SysVIAState.ier&=0x7f;
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

  switch (Address) {
    case 0: /* IRB read */
	  // Clear bit 4 of IFR from ATOD Conversion
      SysVIAState.ifr&=~16;
      tmp=SysVIAState.orb & SysVIAState.ddrb;
      if (!JoystickButton[1])
        tmp |= 32;
      if (!JoystickButton[0])
        tmp |= 16;
      if (MachineType == Model::Master128)
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
      // DebugTrace("Read IFR got=0x%02x\n", SysVIAState.ifr);
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

  if (DebugEnabled)
  {
    DebugDisplayTraceF(DebugType::SysVIA, true,
                       "SysVia: Read address %X value %02X",
                       Address & 0xf, tmp & 0xff);
  }

  return tmp;
}

/*--------------------------------------------------------------------------*/
/* Value denotes the new value - i.e. 1 for a rising edge */
void SysVIATriggerCA1Int(int value) {
  /*value^=1; */
  // DebugTrace("SysVIATriggerCA1Int at %d\n", TotalCycles);
  /* Cause interrupt on appropriate edge */
  if (!((SysVIAState.pcr & 1) ^ value)) {
    SysVIAState.ifr|=2; /* CA1 */
    UpdateIFRTopBit();
  }
}

/*--------------------------------------------------------------------------*/
void SysVIA_poll_real(void) {
  static bool t1int=false;

  if (SysVIAState.timer1c<-2 && !t1int) {
    t1int=true;
    if (!SysVIAState.timer1hasshot || (SysVIAState.acr & 0x40)) {
      // DebugTrace("SysVia timer1 int at %d\n", TotalCycles);
      SysVIAState.ifr|=0x40; /* Timer 1 interrupt */
      UpdateIFRTopBit();
      if (SysVIAState.acr & 0x80) {
        SysVIAState.orb^=0x80; /* Toggle PB7 */
        SysVIAState.irb^=0x80; /* Toggle PB7 */
      }
      if ((SysVIAState.ier & 0x40) && CyclesToInt == NO_TIMER_INT_DUE) {
          CyclesToInt = 3 + SysVIAState.timer1c;
      }
      SysVIAState.timer1hasshot = true;
    }
  }

  if (SysVIAState.timer1c<-3) {
    SysVIAState.timer1c += (SysVIAState.timer1l * 2) + 4;
    t1int=false;
  }

  if (SysVIAState.timer2c<-2) {
    if (!SysVIAState.timer2hasshot) {
      // DebugTrace("SysVia timer2 int at %d\n", TotalCycles);
      SysVIAState.ifr|=0x20; /* Timer 2 interrupt */
      UpdateIFRTopBit();
      if ((SysVIAState.ier & 0x20) && CyclesToInt == NO_TIMER_INT_DUE) {
        CyclesToInt = 3 + SysVIAState.timer2c;
      }
      SysVIAState.timer2hasshot = true;
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
void SysVIAReset()
{
	VIAReset(&SysVIAState);

	// Make it no keys down and no dip switches set
	BeebReleaseAllKeys();

	SRData = 0;
	SRMode = 0;
	SRCount = 0;
	SREnabled = 0; // Disable Shift register shifting shiftily. (I am nuts) - Richard Gellman
}

/*-------------------------------------------------------------------------*/

static time_t CMOSConvertClock()
{
	struct tm Base;
	Base.tm_sec   = BCDToBin(CMOSRAM[0]);
	Base.tm_min   = BCDToBin(CMOSRAM[2]);
	Base.tm_hour  = BCDToBin(CMOSRAM[4]);
	Base.tm_mday  = BCDToBin(CMOSRAM[7]);
	Base.tm_mon   = BCDToBin(CMOSRAM[8]) - 1;
	Base.tm_year  = BCDToBin(CMOSRAM[9]);
	Base.tm_wday  = -1;
	Base.tm_yday  = -1;
	Base.tm_isdst = -1;

	time_t SysTime;
	time(&SysTime);
	struct tm *CurTime = localtime(&SysTime);
	Base.tm_year += (CurTime->tm_year / 100) * 100;

	return mktime(&Base);
}

/*-------------------------------------------------------------------------*/

void RTCInit()
{
	time_t SysTime;
	time(&SysTime);
	struct tm *CurTime = localtime(&SysTime);

	CMOSRAM[0] = BCD(static_cast<unsigned char>(CurTime->tm_sec));
	CMOSRAM[2] = BCD(static_cast<unsigned char>(CurTime->tm_min));
	CMOSRAM[4] = BCD(static_cast<unsigned char>(CurTime->tm_hour));
	CMOSRAM[6] = BCD(static_cast<unsigned char>(CurTime->tm_wday + 1));
	CMOSRAM[7] = BCD(static_cast<unsigned char>(CurTime->tm_mday));
	CMOSRAM[8] = BCD(static_cast<unsigned char>(CurTime->tm_mon + 1));
	CMOSRAM[9] = BCD(static_cast<unsigned char>(CurTime->tm_year % 100));

	RTCTimeOffset = SysTime - CMOSConvertClock();
}

/*-------------------------------------------------------------------------*/

static void RTCUpdate()
{
	time_t SysTime;
	time(&SysTime);
	SysTime -= RTCTimeOffset;
	struct tm *CurTime = localtime(&SysTime);

	CMOSRAM[0] = BCD(static_cast<unsigned char>(CurTime->tm_sec));
	CMOSRAM[2] = BCD(static_cast<unsigned char>(CurTime->tm_min));
	CMOSRAM[4] = BCD(static_cast<unsigned char>(CurTime->tm_hour));
	CMOSRAM[6] = BCD(static_cast<unsigned char>(CurTime->tm_wday + 1));
	CMOSRAM[7] = BCD(static_cast<unsigned char>(CurTime->tm_mday));
	CMOSRAM[8] = BCD(static_cast<unsigned char>(CurTime->tm_mon + 1));
	CMOSRAM[9] = BCD(static_cast<unsigned char>(CurTime->tm_year % 100));
}

/*-------------------------------------------------------------------------*/

void CMOSWrite(unsigned char Address, unsigned char Value)
{
	if (DebugEnabled)
	{
	  DebugDisplayTraceF(DebugType::CMOS, true,
	                     "CMOS: Write address %X value %02X",
	                     Address, Value);
	}

	// Many thanks to Tom Lees for supplying me with info on the 146818 registers
	// for these two functions.
	if (Address <= 0x9)
	{
		// Clock registers
		CMOSRAM[Address] = Value;
	}
	else if (Address == 0xa)
	{
		// Control register A
		CMOSRAM[Address] = Value & 0x7f; // Top bit not writable
	}
	else if (Address == 0xb)
	{
		// Control register B
		// Bit-7 SET - 0=clock running, 1=clock update halted
		if (Value & 0x80)
		{
			RTCUpdate();
		}
		else if ((CMOSRAM[Address] & 0x80) && !(Value & 0x80))
		{
			// New clock settings
			time_t SysTime;
			time(&SysTime);
			RTCTimeOffset = SysTime - CMOSConvertClock();
		}

		CMOSRAM[Address] = Value;
	}
	else if (Address == 0xc || Address == 0xd)
	{
		// Control register C and D - read only
	}
	else if (Address <= 0x3f)
	{
		// User RAM
		CMOSRAM[Address] = Value;
	}
}

/*-------------------------------------------------------------------------*/

unsigned char CMOSRead(unsigned char Address)
{
	// 0x0 to 0x9 - Clock
	// 0xa to 0xd - Regs
	// 0xe to 0x3f - RAM
	if (Address <= 0x9)
	{
		RTCUpdate();
	}

	unsigned char Value = CMOSRAM[Address];

	if (DebugEnabled)
	{
	  DebugDisplayTraceF(DebugType::CMOS, true,
	                     "CMOS: Read address %X value %02X",
	                     Address, Value);
	}

	return Value;
}

/*--------------------------------------------------------------------------*/
void DebugSysViaState()
{
	DebugViaState("SysVia", &SysVIAState);
}
