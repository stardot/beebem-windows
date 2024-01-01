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

#include <fstream>
#include <iostream>

#include "Via.h"
#include "Debug.h"
#include "SysVia.h"
#include "UEFState.h"
#include "UserVia.h"

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
  ToReset->timer1hasshot=false;
  ToReset->timer2hasshot=false;
  ToReset->timer1adjust=0; //Added by Ken Lowe 24/08/03
  ToReset->timer2adjust=0;
  ToReset->ca2 = false;
  ToReset->cb2 = false;
}

/*-------------------------------------------------------------------------*/

void DebugViaState(const char *s, VIAState *v)
{
	DebugDisplayInfo("");

	DebugDisplayInfoF("%s: ora=%02X orb=%02X ira=%02X irb=%02X ddra=%02X ddrb=%02X", s,
		(int)v->ora, (int)v->orb,
		(int)v->ira, (int)v->irb,
		(int)v->ddra, (int)v->ddrb);

	DebugDisplayInfoF("%s: acr=%02X pcr=%02X ifr=%02X ier=%02X", s,
		(int)v->acr, (int)v->pcr,
		(int)v->ifr, (int)v->ier);

	DebugDisplayInfoF("%s: t1=%04X%s t2=%04X%s t1l=%04X t2l=%04X t1s=%d t2s=%d", s,
		(int)(v->timer1c < 0 ? ((v->timer1c - 1) / 2) & 0xffff : v->timer1c / 2),
		v->timer1c & 1 ? "+" : " ",
		(int)(v->timer2c < 0 ? ((v->timer2c - 1) / 2) & 0xffff : v->timer2c / 2),
		v->timer2c & 1 ? "+" : " ",
		(int)v->timer1l, (int)v->timer2l,
		(int)v->timer1hasshot, (int)v->timer2hasshot);

	DebugDisplayInfoF("%s: ca2=%d cb2=%d", s,
		(int)v->ca2, (int)v->cb2);
}

void SaveVIAUEF(FILE *SUEF)
{
	for (int n = 0; n < 2; n++)
	{
		const VIAState *VIAPtr = (n == 0) ? &SysVIAState : &UserVIAState;
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

void LoadViaUEF(FILE *SUEF)
{
	int VIAType = fget8(SUEF);
	VIAState *VIAPtr = VIAType ? &UserVIAState : &SysVIAState;
	VIAPtr->orb = fget8(SUEF);
	VIAPtr->irb = fget8(SUEF);
	VIAPtr->ora = fget8(SUEF);
	VIAPtr->ira = fget8(SUEF);
	VIAPtr->ddrb = fget8(SUEF);
	VIAPtr->ddra = fget8(SUEF);
	VIAPtr->timer1c = fget16(SUEF) * 2;
	VIAPtr->timer1l = fget16(SUEF);
	VIAPtr->timer2c = fget16(SUEF) * 2;
	VIAPtr->timer2l = fget16(SUEF);
	VIAPtr->acr = fget8(SUEF);
	VIAPtr->pcr = fget8(SUEF);
	VIAPtr->ifr = fget8(SUEF);
	VIAPtr->ier = fget8(SUEF);
	VIAPtr->timer1hasshot = fgetbool(SUEF);
	VIAPtr->timer2hasshot = fgetbool(SUEF);
	if (!VIAType) IC32State = fget8(SUEF);

	VIAPtr->ca2 = ((VIAPtr->pcr & PCR_CA2_CONTROL) == PCR_CA2_OUTPUT_HIGH);
	VIAPtr->cb2 = ((VIAPtr->pcr & PCR_CB2_CONTROL) == PCR_CB2_OUTPUT_HIGH);
}
