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
// Dossytronics 1M Paula / blitter sound demo board for Hoglet
// 1MHz fpga board
//
// Dominic Beesley - 2019
//
// This is a rough functional approximation of the the real board

#ifndef _HOG1MPAULA_H
#define _HOG1MPAULA_H


extern bool Hog1MPaulaEnabled;

#define Hog1MPaulaPoll(cycles) { if (Hog1MPaulaEnabled) Hog1MPaulaUpdate(cycles); }

void Hog1MPaulaInit();
void Hog1MPaulaReset();
void Hog1MPaulaWrite(UINT16 address, UINT8 value);
bool Hog1MPaulaRead(UINT16 address, UINT8 *value);
void Hog1MPaulaUpdate(UINT cycles);



#endif
