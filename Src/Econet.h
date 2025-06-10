/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2004  Rob O'Donnell
Copyright (C) 2005  Mike Wyatt

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

// Econet for BeebEm
// Written by Rob O'Donnell. robert@irrelevant.com
// Mike Wyatt - further development, Dec 2005

#ifndef ECONET_HEADER
#define ECONET_HEADER

bool EconetReset();
unsigned char EconetRead(unsigned char Register);
void EconetWrite(unsigned char Register, unsigned char Value);
unsigned char EconetReadStationID();
bool EconetInterruptRequest();
bool EconetPoll();

extern bool EconetEnabled;
extern bool EconetNMIEnabled;
extern bool EconetStateChanged;
extern int EconetTrigger;
extern int EconetFlagFillTimeoutTrigger;
extern int EconetFlagFillTimeout;

extern unsigned char EconetStationID;

extern char EconetCfgPath[MAX_PATH];
extern char AUNMapPath[MAX_PATH];


#endif
