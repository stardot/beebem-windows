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
/* Please report any problems to the author at gilbertd@cs.man.ac.uk        */
/****************************************************************************/
/* Support file for 6522 via - 30/10/94 - David Alan Gilbert */

#include <iostream.h>
#include "via.h"

void VIAReset(VIAState *ToReset) {
  ToReset->ora=ToReset->orb=0xff; /* ??? */
  ToReset->ira=ToReset->irb=0xff; /* ??? */
  ToReset->ddra=ToReset->ddrb=0; /* All inputs */
  ToReset->acr=0; /* Timed ints on t1, t2, no pb7 hacking, no latching, no shifting */
  ToReset->pcr=0; /* Neg edge inputs for cb2,ca2 and CA1 and CB1 */
  ToReset->ifr=0; /* No interrupts presently interrupting */
  ToReset->ier=0x80; /* No interrupts enabled */
  ToReset->timer1l=ToReset->timer2l=0x7fff;/*0xffff; */
  ToReset->timer1c=ToReset->timer2c=0xffff;/*0x1ffff; */
  ToReset->timer1hasshot=0;
  ToReset->timer2hasshot=0;
} /* VIAReset */

/*-------------------------------------------------------------------------*/
void SaveVIAState(VIAState *VIAData, unsigned char *StateData) {
	StateData[0] = VIAData->orb;
	StateData[1] = VIAData->irb;
	StateData[2] = VIAData->ora;
	StateData[3] = VIAData->ira;
	StateData[4] = VIAData->ddrb;
	StateData[5] = VIAData->ddra;
	StateData[6] = (VIAData->timer1c / 2) & 255; /* Timers stored in 2MHz */
	StateData[7] = (VIAData->timer1c / 2) >> 8;  /* units for speed */
	StateData[8] = VIAData->timer1l & 255;
	StateData[9] = VIAData->timer1l >> 8;
	StateData[10] = (VIAData->timer2c / 2) & 255;
	StateData[11] = (VIAData->timer2c / 2) >> 8;
	StateData[12] = VIAData->timer2l & 255;
	StateData[13] = VIAData->timer2l >> 8;
	StateData[14] = 0;  /* Shift register - not currently used */
	StateData[15] = VIAData->acr;
	StateData[16] = VIAData->pcr;
	StateData[17] = VIAData->ifr;
	StateData[18] = VIAData->ier;
}

/*-------------------------------------------------------------------------*/
void RestoreVIAState(VIAState *VIAData, unsigned char *StateData) {
	VIAData->orb = StateData[0];
	VIAData->irb = StateData[1];
	VIAData->ora = StateData[2];
	VIAData->ira = StateData[3];
	VIAData->ddrb = StateData[4];
	VIAData->ddra = StateData[5];
	VIAData->timer1c = (StateData[6] + (StateData[7] << 8)) * 2;
	VIAData->timer1l = StateData[8] + (StateData[9] << 8);
	VIAData->timer2c = (StateData[10] + (StateData[11] << 8)) * 2;
	VIAData->timer2l = StateData[12] + (StateData[13] << 8);
	VIAData->acr = StateData[15];
	VIAData->pcr = StateData[16];
	VIAData->ifr = StateData[17];
	VIAData->ier = StateData[18];
	VIAData->timer1hasshot = 0;
	VIAData->timer2hasshot = 0;
}

/*-------------------------------------------------------------------------*/
void via_dumpstate(VIAState *ToDump) {
  cerr << "  ora=" << int(ToDump->ora) << "\n";
  cerr << "  orb="  << int(ToDump->orb) << "\n";
  cerr << "  ira="  << int(ToDump->ira) << "\n";
  cerr << "  irb="  << int(ToDump->irb) << "\n";
  cerr << "  ddra="  << int(ToDump->ddra) << "\n";
  cerr << "  ddrb="  << int(ToDump->ddrb) << "\n";
  cerr << "  acr="  << int(ToDump->acr) << "\n";
  cerr << "  pcr="  << int(ToDump->pcr) << "\n";
  cerr << "  ifr="  << int(ToDump->ifr) << "\n";
  cerr << "  ier="  << int(ToDump->ier) << "\n";
  cerr << "  timer1c="  << ToDump->timer1c << "\n";
  cerr << "  timer2c="  << ToDump->timer2c << "\n";
  cerr << "  timer1l="  << ToDump->timer1l << "\n";
  cerr << "  timer2l="  << ToDump->timer2l << "\n";
  cerr << "  timer1hasshot="  << ToDump->timer1hasshot << "\n";
  cerr << "  timer2hasshot="  << ToDump->timer2hasshot << "\n";
}; /* via_dumpstate */
