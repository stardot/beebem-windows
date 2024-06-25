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
 * (C) Nigel Barnes (original work from Mame
 * (C) Steve Inglis derivative work for BeebEm
 *
 * BBC Micro PALPROM carrier boards original work done by Nigel.
 * License MIT 3 clause
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "PALRom.h"
#include "zlib/zlib.h"

PALRomState PALRom[MAX_PAL_ROMS];

static uint8_t cciword_device(int ROMSEL, int offset)
{
    switch (offset & 0x3fe0)
    {
        case 0x0060:
        case 0x3fc0: PALRom[ROMSEL].Bank = 0; break;
        case 0x0040:
        case 0x3fa0:
        case 0x3fe0: PALRom[ROMSEL].Bank = 1; break;
    }

    return PALRom[ROMSEL].Rom[(offset & 0x3fff) | (PALRom[ROMSEL].Bank << 14)];
}

static uint8_t ccibase_device(int ROMSEL, int offset)
{
    switch (offset & 0x3fe0)
    {
        case 0x3f80: PALRom[ROMSEL].Bank = 0; break;
        case 0x3fa0: PALRom[ROMSEL].Bank = 1; break;
        case 0x3fc0: PALRom[ROMSEL].Bank = 2; break;
        case 0x3fe0: PALRom[ROMSEL].Bank = 3; break;
    }

    return PALRom[ROMSEL].Rom[(offset & 0x3fff) | (PALRom[ROMSEL].Bank << 14)];
}

static uint8_t ccispell_device(int ROMSEL, int offset)
{
    if (offset == 0x3fe0)
    {
        PALRom[ROMSEL].Bank = 0;
    }
    else if (PALRom[ROMSEL].Bank == 0)
    {
        switch (offset & 0x3fe0)
        {
            case 0x3fc0: PALRom[ROMSEL].Bank = 1; break;
            case 0x3fa0: PALRom[ROMSEL].Bank = 2; break;
            case 0x3f80: PALRom[ROMSEL].Bank = 3; break;
            case 0x3f60: PALRom[ROMSEL].Bank = 4; break;
            case 0x3f40: PALRom[ROMSEL].Bank = 5; break;
            case 0x3f20: PALRom[ROMSEL].Bank = 6; break;
            case 0x3f00: PALRom[ROMSEL].Bank = 7; break;
        }
    }

    return PALRom[ROMSEL].Rom[(offset & 0x3fff) | (PALRom[ROMSEL].Bank << 14)];
}

static uint8_t acotrilogy(int ROMSEL, int offset)
{
    switch (offset & 0x3ff8)
    {
        case 0x3ff8: PALRom[ROMSEL].Bank = offset & 0x03; break;
    }

    return PALRom[ROMSEL].Rom[(offset & 0x3fff) | (PALRom[ROMSEL].Bank << 14)];
}

static uint8_t pres_abepdevice(int ROMSEL, int offset)
{
    switch (offset & 0x3ffc)
    {
        case 0x3ff8: PALRom[ROMSEL].Bank = 0; break;
        case 0x3ffc: PALRom[ROMSEL].Bank = 1; break;
    }

    return PALRom[ROMSEL].Rom[(offset & 0x3fff) | (PALRom[ROMSEL].Bank << 14)];
}

static uint8_t pres_abedevice(int ROMSEL, int offset)
{
    switch (offset & 0x3ffc)
    {
        case 0x3ff8: PALRom[ROMSEL].Bank = 1; break;
        case 0x3ffc: PALRom[ROMSEL].Bank = 0; break;
    }

    return PALRom[ROMSEL].Rom[(offset & 0x3fff) | (PALRom[ROMSEL].Bank << 14)];
}

static uint8_t watqst_device(int ROMSEL, int offset)
{
    switch (offset & 0x3fe0)
    {
        case 0x0820: PALRom[ROMSEL].Bank = 2; break;
        case 0x11e0: PALRom[ROMSEL].Bank = 1; break;
        case 0x12c0: PALRom[ROMSEL].Bank = 3; break;
        case 0x1340: PALRom[ROMSEL].Bank = 0; break;
    }

    if (offset & 0x2000)
    {
        return PALRom[ROMSEL].Rom[(offset & 0x1fff) | (PALRom[ROMSEL].Bank << 13)];
    }
    else
    {
        return PALRom[ROMSEL].Rom[(offset & 0x1fff)];
    }
}

static uint8_t watwap_device(int ROMSEL, int offset)
{
    switch (offset & 0x3fe0)
    {
        case 0x1f00: PALRom[ROMSEL].Bank = 0; break;
        case 0x1f20: PALRom[ROMSEL].Bank = 1; break;
        case 0x1f40: PALRom[ROMSEL].Bank = 2; break;
        case 0x1f60: PALRom[ROMSEL].Bank = 3; break;
        case 0x1f80: PALRom[ROMSEL].Bank = 4; break;
        case 0x1fa0: PALRom[ROMSEL].Bank = 5; break;
        case 0x1fc0: PALRom[ROMSEL].Bank = 6; break;
        case 0x1fe0: PALRom[ROMSEL].Bank = 7; break;
    }

    if (offset & 0x2000)
    {
        return PALRom[ROMSEL].Rom[(offset & 0x1fff) | (PALRom[ROMSEL].Bank << 13)];
    }
    else
    {
        return PALRom[ROMSEL].Rom[offset & 0x1fff];
    }
}

static uint8_t wapted_device(int ROMSEL, int offset)
{
    switch (offset & 0x3fe0)
    {
        case 0x1f80: PALRom[ROMSEL].Bank = 0; break;
        case 0x1fa0: PALRom[ROMSEL].Bank = 1; break;
        case 0x1fc0: PALRom[ROMSEL].Bank = 2; break;
        case 0x1fe0: PALRom[ROMSEL].Bank = 3; break;
    }

    if (offset & 0x2000)
    {
        return PALRom[ROMSEL].Rom[(offset & 0x1fff) | (PALRom[ROMSEL].Bank << 13)];
    }
    else
    {
        return PALRom[ROMSEL].Rom[offset & 0x1fff];
    }
}

uint8_t PALRomRead(int ROMSEL, int offset)
{
    switch (PALRom[ROMSEL].Type)
    {
        case PALRomType::none:
        default:
            return PALRom[ROMSEL].Rom[offset];

        case PALRomType::cciword:
            return cciword_device(ROMSEL, offset);

        case PALRomType::ccispell:
            return ccispell_device(ROMSEL, offset);

        case PALRomType::ccibase:
            return ccibase_device(ROMSEL, offset);

        case PALRomType::acotrilogy:
            return acotrilogy(ROMSEL, offset);

        case PALRomType::presabe:
            return pres_abedevice(ROMSEL, offset);

        case PALRomType::presabep:
            return pres_abepdevice(ROMSEL, offset);

        case PALRomType::watqst:
            return watqst_device(ROMSEL, offset);

        case PALRomType::watwap:
            return watwap_device(ROMSEL, offset);

        case PALRomType::watted:
            return wapted_device(ROMSEL, offset);
    }
}

PALRomType GuessRomType(uint8_t *Rom, uint32_t Size)
{
    char RomName[11];
    memcpy(RomName, &Rom[9], 10);
    RomName[10] = '\0';
    int Crc = (int)crc32(0, Rom, Size);

    if (strstr(RomName, "ViewSpell") && Size == PALROM_64K && Crc == 0xE56B0E01)
    {
        return PALRomType::acotrilogy;
    }
    else if (strstr(RomName, "MegaROM") && Size == PALROM_128K && Crc == 0xFAAC60BB)
    {
        return PALRomType::ccispell;
    }
    else if (strstr(RomName, "INTER-BASE") && Size == PALROM_64K && Crc == 0x4332ED95)
    {
        return PALRomType::ccibase;
    }
    else if (strstr(RomName, "INTER-WORD") && Size == PALROM_32K && Crc == 0xC93E3C33)
    {
        return PALRomType::cciword;
    }
    else if (strstr(RomName, "AMX") && Size == PALROM_32K && Crc == 0xA72E2E59)
    {
        // outstanding needs updated crc
        return PALRomType::cciword;
    }
    else if (strstr(RomName, "SPELLMASTE") && Size == PALROM_128K && Crc == 0xC26533EC)
    {
        return PALRomType::ccispell;
    }
    else if (strstr(RomName, "Advanced") && Size == PALROM_32K && Crc == 0xE0B93F43)
    {
        return PALRomType::presabe;
    }
    else if (strstr(RomName, "The BASIC") && Size == PALROM_32K && Crc == 0x7E3A119A)
    {
        return PALRomType::presabep;
    }
    else if (strstr(RomName, "QUEST") && Size == PALROM_32K && (Crc == 0x7880EB9A || Crc == 0x839A9B34))
    {
        return PALRomType::watqst;
    }
    else if (strstr(RomName, "WAPPING ED") && Size == PALROM_64K && Crc == 0x26431B44)
    {
        return PALRomType::watwap;
    }
    else if (strstr(RomName, "TED 32K\0 1") && Size == PALROM_32K && Crc == 0x2D4C6458)
    {
        return PALRomType::watted;
    }
    else if (strstr(RomName, "CONQUEST") && Size == PALROM_32K && Crc == 0xF9634ECE)
    {
        return PALRomType::watqst;
    }

    return PALRomType::none;
}
