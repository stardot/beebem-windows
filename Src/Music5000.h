/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2016  Mike Wyatt

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

//
// BeebEm Music 5000 emulation
//
// Mike Wyatt - Jan 2016
//

#ifndef _MUSIC5000_H
#define _MUSIC5000_H

extern bool Music5000Enabled;

#define Music5000Poll(cycles) { if (Music5000Enabled) Music5000Update(cycles); }

void Music5000Init();
void Music5000Reset();
void Music5000Write(int address, unsigned char value);
bool Music5000Read(int address, unsigned char *value);
void Music5000Update(UINT cycles);
void SaveMusic5000UEF(FILE *SUEF);
void LoadMusic5000UEF(FILE *SUEF, int Version);
void LoadMusic5000JIMPageRegUEF(FILE *SUEF);

#endif
