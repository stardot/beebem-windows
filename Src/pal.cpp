/**************************************************
 * (C) Nigel Barnes (original work from Mame
 * (C) Steve Inglis derivative work for Beebem
 *
 * BBC Micro PALPROM carrier boards original work done by Nigel.
 * License MIT 3 clause
 *
 ******************************************************************************/

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "pal.h"
#include "zlib/zlib.h"

largeRom ERom[MAX_EROMS];

static uint8_t cciword_device(int ROMSEL, int offset)
{
    switch (offset & 0x3fe0)
    {
        case 0x0060:
        case 0x3fc0: ERom[ROMSEL].Bank = 0; break;
        case 0x0040:
        case 0x3fa0:
        case 0x3fe0: ERom[ROMSEL].Bank = 1; break;
    }

    return ERom[ROMSEL].Rom[(offset & 0x3fff) | (ERom[ROMSEL].Bank << 14)];
}

static uint8_t ccibase_device(int ROMSEL, int offset)
{
    switch (offset & 0x3fe0)
    {
        case 0x3f80: ERom[ROMSEL].Bank = 0; break;
        case 0x3fa0: ERom[ROMSEL].Bank = 1; break;
        case 0x3fc0: ERom[ROMSEL].Bank = 2; break;
        case 0x3fe0: ERom[ROMSEL].Bank = 3; break;
    }

    return ERom[ROMSEL].Rom[(offset & 0x3fff) | (ERom[ROMSEL].Bank << 14)];
}

static uint8_t ccispell_device(int ROMSEL, int offset)
{
    if (offset == 0x3fe0)
    {
        ERom[ROMSEL].Bank = 0;
    }
    else if (ERom[ROMSEL].Bank == 0)
    {
        switch (offset & 0x3fe0)
        {
            case 0x3fc0: ERom[ROMSEL].Bank = 1; break;
            case 0x3fa0: ERom[ROMSEL].Bank = 2; break;
            case 0x3f80: ERom[ROMSEL].Bank = 3; break;
            case 0x3f60: ERom[ROMSEL].Bank = 4; break;
            case 0x3f40: ERom[ROMSEL].Bank = 5; break;
            case 0x3f20: ERom[ROMSEL].Bank = 6; break;
            case 0x3f00: ERom[ROMSEL].Bank = 7; break;
        }
    }

    return ERom[ROMSEL].Rom[(offset & 0x3fff) | (ERom[ROMSEL].Bank << 14)];
}

static uint8_t acotrilogy(int ROMSEL, int offset)
{
    switch (offset & 0x3ff8)
    {
        case 0x3ff8: ERom[ROMSEL].Bank = offset & 0x03; break;
    }

    return ERom[ROMSEL].Rom[(offset & 0x3fff) | (ERom[ROMSEL].Bank << 14)];
}

static uint8_t pres_abepdevice(int ROMSEL, int offset)
{
    switch (offset & 0x3ffc)
    {
        case 0x3ff8: ERom[ROMSEL].Bank = 0; break;
        case 0x3ffc: ERom[ROMSEL].Bank = 1; break;
    }

    return ERom[ROMSEL].Rom[(offset & 0x3fff) | (ERom[ROMSEL].Bank << 14)];
}

static uint8_t pres_abedevice(int ROMSEL, int offset)
{
    switch (offset & 0x3ffc)
    {
        case 0x3ff8: ERom[ROMSEL].Bank = 1; break;
        case 0x3ffc: ERom[ROMSEL].Bank = 0; break;
    }

    return ERom[ROMSEL].Rom[(offset & 0x3fff) | (ERom[ROMSEL].Bank << 14)];
}

static uint8_t watqst_device(int ROMSEL, int offset)
{
    switch (offset & 0x3fe0)
    {
        case 0x0820: ERom[ROMSEL].Bank = 2; break;
        case 0x11e0: ERom[ROMSEL].Bank = 1; break;
        case 0x12c0: ERom[ROMSEL].Bank = 3; break;
        case 0x1340: ERom[ROMSEL].Bank = 0; break;
    }

    if (offset & 0x2000)
    {
        return ERom[ROMSEL].Rom[(offset & 0x1fff) | (ERom[ROMSEL].Bank << 13)];
    } else {
        return ERom[ROMSEL].Rom[(offset & 0x1fff)];
    }
}

static uint8_t watwap_device(int ROMSEL, int offset)
{
    switch (offset & 0x3fe0)
    {
        case 0x1f00: ERom[ROMSEL].Bank = 0; break;
        case 0x1f20: ERom[ROMSEL].Bank = 1; break;
        case 0x1f40: ERom[ROMSEL].Bank = 2; break;
        case 0x1f60: ERom[ROMSEL].Bank = 3; break;
        case 0x1f80: ERom[ROMSEL].Bank = 4; break;
        case 0x1fa0: ERom[ROMSEL].Bank = 5; break;
        case 0x1fc0: ERom[ROMSEL].Bank = 6; break;
        case 0x1fe0: ERom[ROMSEL].Bank = 7; break;
    }

    if (offset & 0x2000)
    {
        return ERom[ROMSEL].Rom[(offset & 0x1fff) | (ERom[ROMSEL].Bank << 13)];
    } else {
        return ERom[ROMSEL].Rom[offset & 0x1fff];
    }
}

static uint8_t wapted_device(int ROMSEL, int offset)
{
    switch (offset & 0x3fe0)
    {
        case 0x1f80: ERom[ROMSEL].Bank = 0; break;
        case 0x1fa0: ERom[ROMSEL].Bank = 1; break;
        case 0x1fc0: ERom[ROMSEL].Bank = 2; break;
        case 0x1fe0: ERom[ROMSEL].Bank = 3; break;
    }

    if (offset & 0x2000)
    {
        return ERom[ROMSEL].Rom[(offset & 0x1fff) | (ERom[ROMSEL].Bank << 13)];
    } else {
        return ERom[ROMSEL].Rom[offset & 0x1fff];
    }
}

uint8_t ExtendedRom(int ROMSEL, int offset)
{
    switch (ERom[ROMSEL].Type)
    {
        case PALRomType::none:
        default:
            return ERom[ROMSEL].Rom[offset];

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

void GuessRomType(int rom, uint32_t size)
{
    char rom_name[11];
    memcpy(rom_name, &ERom[rom].Rom[9], 10);
    rom_name[10] = '\0';
    int crc = crc32(0, ERom[rom].Rom, size);

    if (strstr(rom_name, "ViewSpell") && size == 65536 && crc == 0xE56B0E01)
    {
        ERom[rom].Type = PALRomType::acotrilogy;
    }
    else if (strstr(rom_name, "MegaROM") && size == 131072 && crc == 0xFAAC60BB)
    {
        ERom[rom].Type = PALRomType::ccispell;
    }
    else if (strstr(rom_name, "INTER-BASE") && size == 65536 && crc == 0x4332ED95)
    {
        ERom[rom].Type = PALRomType::ccibase;
    }
    else if (strstr(rom_name, "INTER-WORD") && size == 32768 && crc == 0xC93E3C33)
    {
        ERom[rom].Type = PALRomType::cciword;
    }
    else if (strstr(rom_name, "AMX") && size == 32768 && crc == 0xA72E2E59)
    {
        // outstanding needs updated crc
        ERom[rom].Type = PALRomType::cciword;
    }
    else if (strstr(rom_name, "SPELLMASTE") && size == 131072 && crc == 0xC26533EC)
    {
        ERom[rom].Type = PALRomType::ccispell;
    }
    else if (strstr(rom_name, "Advanced") && size == 32768 && crc == 0xE0B93F43)
    {
        ERom[rom].Type = PALRomType::presabe;
    }
    else if (strstr(rom_name, "The BASIC") && size == 32768 && crc == 0x7E3A119A)
    {
        ERom[rom].Type = PALRomType::presabep;
    }
    else if (strstr(rom_name, "QUEST") && size == 32768 && (crc == 0x7880EB9A || crc == 0x839A9B34))
    {
        ERom[rom].Type = PALRomType::watqst;
    }
    else if (strstr(rom_name, "WAPPING ED") && size == 65536 && crc == 0x26431B44)
    {
        ERom[rom].Type = PALRomType::watwap;
    }
    else if (strstr(rom_name, "TED 32K\0 1") && size == 32768 && crc == 0x2D4C6458)
    {
        ERom[rom].Type = PALRomType::watted;
    }
    else if (strstr(rom_name, "CONQUEST") && size == 32768 && crc == 0xF9634ECE)
    {
        ERom[rom].Type = PALRomType::watqst;
    }
}
