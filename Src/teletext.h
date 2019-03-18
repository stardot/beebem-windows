/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2006  Jon Welch
Copyright (C) 2019  Alistair Cree

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
/* Teletext Support for Beebem */

extern bool TeletextAdapterEnabled;
extern bool TeletextFiles;
extern bool TeletextLocalhost;
extern bool TeletextCustom;

extern char TeletextIP[4][20];
extern u_short TeletextPort[4];
extern char TeletextCustomIP[4][20];
extern u_short TeletextCustomPort[4];

void TeletextWrite(int Address, int Value);
int TeletextRead(int Address);
void TeletextPoll(void);
void TeletextLog(char *text, ...);
void TeletextInit(void);
void TeletextClose(void);
