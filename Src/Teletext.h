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

#ifndef TELETEXT_HEADER
#define TELETEXT_HEADER

extern bool TeletextAdapterEnabled;
extern int TeletextAdapterTrigger;

enum class TeletextSourceType : unsigned char {
    IP = 0,
    File
};

extern TeletextSourceType TeletextSource;

constexpr int TELETEXT_CHANNEL_COUNT = 4;

constexpr u_short TELETEXT_BASE_PORT = 19761;

extern std::string TeletextFileName[4];
extern std::string TeletextIP[4];
extern u_short TeletextPort[4];

void TeletextWrite(int Address, int Value);
unsigned char TeletextRead(int Address);
void TeletextAdapterUpdate();

#define TeletextPoll() { if (TeletextAdapterTrigger <= TotalCycles) TeletextAdapterUpdate(); }

void TeletextInit();
void TeletextClose();

#endif
