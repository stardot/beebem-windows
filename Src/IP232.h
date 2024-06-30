/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2006  Jon Welch
Copyright (C) 2009  Rob O'Donnell

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

#ifndef IP232_HEADER
#define IP232_HEADER

extern bool IP232Handshake;
extern bool IP232Raw;
extern char IP232Address[256];
extern int IP232Port;

bool IP232Open();
void IP232Close();

bool IP232Poll();
bool IP232PollStatus();
void IP232Write(unsigned char Data);
void IP232SetRTS(bool RTS);
unsigned char IP232Read();
unsigned char IP232ReadStatus();

constexpr unsigned char IP232_DTR_LOW  = 0;
constexpr unsigned char IP232_DTR_HIGH = 1;

#endif
