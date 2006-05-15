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
/* Support file for 6522 via - 30/10/94 - David Alan Gilbert */

#ifndef VIA_HEADER
#define VIA_HEADER
#include <iostream>
#include <fstream>
#include "via.h"
#include <stdio.h>
#include "sysvia.h"
#include "uservia.h"
#include "uefstate.h"
#include "viastate.h"
#include "debug.h"

using namespace std;

void VIAReset(VIAState *ToReset) {
  ToReset->ora=ToReset->orb=0xff;
  ToReset->ira=ToReset->irb=0xff;
  ToReset->ddra=ToReset->ddrb=0; /* All inputs */
  ToReset->acr=0; /* Timed ints on t1, t2, no pb7 hacking, no latching, no shifting */
  ToReset->pcr=0; /* Neg edge inputs for cb2,ca2 and CA1 and CB1 */
  ToReset->ifr=0; /* No interrupts presently interrupting */
  ToReset->ier=0x80; /* No interrupts enabled */
  ToReset->timer1l=ToReset->timer2l=0xffff;/*0xffff; */
  ToReset->timer1c=ToReset->timer2c=0xffff;/*0x1ffff; */
  ToReset->timer1hasshot=0;
  ToReset->timer2hasshot=0;
  ToReset->timer1adjust=0; //Added by Ken Lowe 24/08/03
  ToReset->timer2adjust=0;
} /* VIAReset */

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

void DebugViaState(const char *s, VIAState *v)
{
	char info[200];

	DebugDisplayInfo("");

	sprintf(info, "%s: ora=%02X orb=%02X ira=%02X irb=%02X ddra=%02X ddrb=%02X", s,
		(int)v->ora, (int)v->orb,
		(int)v->ira, (int)v->irb,
		(int)v->ddra, (int)v->ddrb);
	DebugDisplayInfo(info);

	sprintf(info, "%s: acr=%02X pcr=%02X ifr=%02X ier=%02X", s,
		(int)v->acr, (int)v->pcr,
		(int)v->ifr, (int)v->ier);
	DebugDisplayInfo(info);

	sprintf(info, "%s: t1=%04X t2=%04X t1l=%04X t2l=%04X t1s=%d t2s=%d", s,
		(int)v->timer1c / 2, (int)v->timer2c / 2,
		(int)v->timer1l, (int)v->timer2l,
		(int)v->timer1hasshot, (int)v->timer2hasshot);
	DebugDisplayInfo(info);
}

void SaveVIAUEF(FILE *SUEF) {
	VIAState *VIAPtr;
	for (int n=0;n<2;n++) {
		if (!n) VIAPtr=&SysVIAState; else VIAPtr=&UserVIAState;
		fput16(0x0467,SUEF);
		fput32(22,SUEF);
		fputc(n,SUEF);
		fputc(VIAPtr->orb,SUEF);
		fputc(VIAPtr->irb,SUEF);
		fputc(VIAPtr->ora,SUEF);
		fputc(VIAPtr->ira,SUEF);
		fputc(VIAPtr->ddrb,SUEF);
		fputc(VIAPtr->ddra,SUEF);
		fput16(VIAPtr->timer1c / 2,SUEF);
		fput16(VIAPtr->timer1l,SUEF);
		fput16(VIAPtr->timer2c / 2,SUEF);
		fput16(VIAPtr->timer2l,SUEF);
		fputc(VIAPtr->acr,SUEF);
		fputc(VIAPtr->pcr,SUEF);
		fputc(VIAPtr->ifr,SUEF);
		fputc(VIAPtr->ier,SUEF);
		fputc(VIAPtr->timer1hasshot,SUEF);
		fputc(VIAPtr->timer2hasshot,SUEF);
		if (!n) fputc(IC32State,SUEF); else fputc(0,SUEF);
	}
}

void LoadViaUEF(FILE *SUEF) {
	VIAState *VIAPtr;
	int VIAType;
	VIAType=fgetc(SUEF); if (VIAType) VIAPtr=&UserVIAState; else VIAPtr=&SysVIAState;
	VIAPtr->orb=fgetc(SUEF);
	VIAPtr->irb=fgetc(SUEF);
	VIAPtr->ora=fgetc(SUEF);
	VIAPtr->ira=fgetc(SUEF);
	VIAPtr->ddrb=fgetc(SUEF);
	VIAPtr->ddra=fgetc(SUEF);
	VIAPtr->timer1c=fget16(SUEF) * 2;
	VIAPtr->timer1l=fget16(SUEF);
	VIAPtr->timer2c=fget16(SUEF) * 2;
	VIAPtr->timer2l=fget16(SUEF);
	VIAPtr->acr=fgetc(SUEF);
	VIAPtr->pcr=fgetc(SUEF);
	VIAPtr->ifr=fgetc(SUEF);
	VIAPtr->ier=fgetc(SUEF);
	VIAPtr->timer1hasshot=fgetc(SUEF);
	VIAPtr->timer2hasshot=fgetc(SUEF);
	if (!VIAType) IC32State=fgetc(SUEF);
}
#endif
