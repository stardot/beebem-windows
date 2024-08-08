/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2024 Steve Inglis

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

/*
 * BBC Micro PALPROM carrier board
 * Work based on original work by Nigel Barnes (Mame)
 * (C) Nigel Barnes (Original Work)
 * (S) Steve Inglis (derivative work for BeebEm)
 *
 * The original work and all the heavy lifting was done by Nigel
 */

#ifndef PALROM_HEADER
#define PALROM_HEADER

#include <stdint.h>

enum class PALRomType {
    none,
    cciword,
    ccibase,
    ccispell,
    acotrilogy,
    presabe,
    presabep,
    watqst,
    watwap,
    watted,
    wwplusii
};

constexpr int PALROM_32K = 32768;
constexpr int PALROM_64K = 65536;
constexpr int PALROM_128K = 131072;
constexpr int MAX_PALROM_SIZE = PALROM_128K;

struct PALRomState {
    unsigned char Rom[MAX_PALROM_SIZE];
    PALRomType Type  = PALRomType::none;
    uint8_t Bank = 0;
};

constexpr int MAX_PAL_ROMS = 16;

extern PALRomState PALRom[MAX_PAL_ROMS];

uint8_t PALRomRead(int romsel, int offset);
PALRomType GuessRomType(uint8_t *Rom, uint32_t size);

#endif
