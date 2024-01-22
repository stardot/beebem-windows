/**************************************************
 * (C) Nigel Barnes (original work from Mame
 * (C) Steve Inglis derivative work for Beebem
 *
 * BBC Micro PALPROM carrier boards original work done by Nigel.
 * License MIT 3 clause
 *
 ******************************************************************************/

#include "pal.h"
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "zlib/zlib.h"

largeRom ERom[4];

uint8_t cciword_device(int ROMSEL, int offset)
{
    switch (offset & 0x3fe0)
    {
        case 0x0060:
        case 0x3fc0: ERom[ROMSEL].m_bank = 0; break;
        case 0x0040:
        case 0x3fa0:
        case 0x3fe0: ERom[ROMSEL].m_bank = 1; break;
    }

    return ERom[ROMSEL].rom[(offset & 0x3fff) | (ERom[ROMSEL].m_bank << 14)];
}

uint8_t ccibase_device(int ROMSEL, int offset)
{
    switch (offset & 0x3fe0)
    {
        case 0x3f80: ERom[ROMSEL].m_bank = 0; break;
        case 0x3fa0: ERom[ROMSEL].m_bank = 1; break;
        case 0x3fc0: ERom[ROMSEL].m_bank = 2; break;
        case 0x3fe0: ERom[ROMSEL].m_bank = 3; break;
    }

    return ERom[ROMSEL].rom[(offset & 0x3fff) | (ERom[ROMSEL].m_bank << 14)];

}

uint8_t ccispell_device(int ROMSEL, int offset)
{
    if (offset == 0x3fe0)
    {
        ERom[ROMSEL].m_bank = 0;
    }
    else if (ERom[ROMSEL].m_bank == 0)
    {
        switch (offset & 0x3fe0)
        {
            case 0x3fc0: ERom[ROMSEL].m_bank = 1; break;
            case 0x3fa0: ERom[ROMSEL].m_bank = 2; break;
            case 0x3f80: ERom[ROMSEL].m_bank = 3; break;
            case 0x3f60: ERom[ROMSEL].m_bank = 4; break;
            case 0x3f40: ERom[ROMSEL].m_bank = 5; break;
            case 0x3f20: ERom[ROMSEL].m_bank = 6; break;
            case 0x3f00: ERom[ROMSEL].m_bank = 7; break;
        }
    }
    return ERom[ROMSEL].rom[(offset & 0x3fff) | (ERom[ROMSEL].m_bank << 14)];
}

uint8_t acotrilogy(int ROMSEL, int offset)
{
    switch (offset & 0x3ff8)
    {
        case 0x3ff8: ERom[ROMSEL].m_bank = offset & 0x03; break;
    }
    
    return ERom[ROMSEL].rom[(offset & 0x3fff) | (ERom[ROMSEL].m_bank << 14)];
}

uint8_t pres_abepdevice(int ROMSEL, int offset)
{
    switch (offset & 0x3ffc)
    {
        case 0x3ff8: ERom[ROMSEL].m_bank = 0; break;
        case 0x3ffc: ERom[ROMSEL].m_bank = 1; break;
    }

    return ERom[ROMSEL].rom[(offset & 0x3fff) | (ERom[ROMSEL].m_bank << 14)];
}

uint8_t pres_abedevice(int ROMSEL, int offset)
{
    switch (offset & 0x3ffc)
    {
        case 0x3ff8: ERom[ROMSEL].m_bank = 1; break;
        case 0x3ffc: ERom[ROMSEL].m_bank = 0; break;
    }

    return ERom[ROMSEL].rom[(offset & 0x3fff) | (ERom[ROMSEL].m_bank << 14)];
}

uint8_t watqst_device(int ROMSEL, int offset)
{
    switch (offset & 0x3fe0)
    {
        case 0x0820: ERom[ROMSEL].m_bank = 2; break;
        case 0x11e0: ERom[ROMSEL].m_bank = 1; break;
        case 0x12c0: ERom[ROMSEL].m_bank = 3; break;
        case 0x1340: ERom[ROMSEL].m_bank = 0; break;
    }

    if (offset & 0x2000)
    {
        return ERom[ROMSEL].rom[(offset & 0x1fff) | (ERom[ROMSEL].m_bank << 13)];
    } else {
        return ERom[ROMSEL].rom[(offset & 0x1fff)];
    }
}

uint8_t watwap_device(int ROMSEL, int offset)
{
    switch (offset & 0x3fe0)
    {
        case 0x1f00: ERom[ROMSEL].m_bank = 0; break;
        case 0x1f20: ERom[ROMSEL].m_bank = 1; break;
        case 0x1f40: ERom[ROMSEL].m_bank = 2; break;
        case 0x1f60: ERom[ROMSEL].m_bank = 3; break;
        case 0x1f80: ERom[ROMSEL].m_bank = 4; break;
        case 0x1fa0: ERom[ROMSEL].m_bank = 5; break;
        case 0x1fc0: ERom[ROMSEL].m_bank = 6; break;
        case 0x1fe0: ERom[ROMSEL].m_bank = 7; break;
    }

    if (offset & 0x2000)
    {
        return ERom[ROMSEL].rom[(offset & 0x1fff) | (ERom[ROMSEL].m_bank << 13)];
    } else {
        return ERom[ROMSEL].rom[offset & 0x1fff];
    }
}

uint8_t wapted_device(int ROMSEL, int offset)
{
    switch (offset & 0x3fe0)
    {
        case 0x1f80: ERom[ROMSEL].m_bank = 0; break;
        case 0x1fa0: ERom[ROMSEL].m_bank = 1; break;
        case 0x1fc0: ERom[ROMSEL].m_bank = 2; break;
        case 0x1fe0: ERom[ROMSEL].m_bank = 3; break;
    }

    if (offset & 0x2000)
    {
        return ERom[ROMSEL].rom[(offset & 0x1fff) | (ERom[ROMSEL].m_bank << 13)];
    } else {
        return ERom[ROMSEL].rom[offset & 0x1fff];
    }

}




uint8_t ExtendedRom(int ROMSEL, int offset)
{
    if (ERom[ROMSEL].type == PALRomType::none)
    {
        return ERom[ROMSEL].rom[offset];
    }
    if (ERom[ROMSEL].type == PALRomType::cciword)
    {
        return (cciword_device(ROMSEL, offset));
    }
    if (ERom[ROMSEL].type == PALRomType::ccispell)
    {
        return (ccispell_device(ROMSEL, offset));
    }
    if (ERom[ROMSEL].type == PALRomType::ccibase)
    {
        return (ccibase_device(ROMSEL, offset));
    }
    if (ERom[ROMSEL].type == PALRomType::acotrilogy)
    {
        return (acotrilogy(ROMSEL, offset));
    }
    if (ERom[ROMSEL].type == PALRomType::presabe)
    {
        return (pres_abedevice(ROMSEL, offset));
    }
    if (ERom[ROMSEL].type == PALRomType::presabep)
    {
        return (pres_abepdevice(ROMSEL, offset));
    }
    if (ERom[ROMSEL].type == PALRomType::watqst)
    {
        return (watqst_device(ROMSEL, offset));
    }
   if (ERom[ROMSEL].type == PALRomType::watwap)
    {
        return (watwap_device(ROMSEL, offset));
    }
   if (ERom[ROMSEL].type == PALRomType::watted)
    {
        return (wapted_device(ROMSEL, offset));
    }

  return ERom[ROMSEL].rom[offset];
}

uint32_t GetRomFileSize(FILE *handle)
{
    int rom_size = 0;

    if (handle != nullptr)
    {
        fseek(handle, 0L, SEEK_END);
        rom_size = ftell(handle);
        fseek(handle, 0L, SEEK_SET);
    } 
  // fallthrough
  return rom_size;
}

void GuessRomType(int rom, uint32_t size)
{
    char rom_name[11];
    memcpy(rom_name, &ERom[rom].rom[9],10);
    rom_name[10] = '\0';
    //int crc = crc32b(rom);
    int crc = crc32(0, ERom[rom].rom, size);
    fprintf(stderr, "CRC was %08X\n",crc);

    if (rom < 4)
    {
    if (size <= 16384) ERom[rom].type = PALRomType::none;
    if (strstr(rom_name, "ViewSpell") && crc == 0xE56B0E01)
            {
                if (size == 65536) ERom[rom].type = PALRomType::acotrilogy;
            }
    }

    if (strstr(rom_name, "MegaROM") && crc == 0xFAAC60BB)
    {
        if (size == 131072)
        {
            ERom[rom].type = PALRomType::ccispell;
        }
    }
    if (strstr(rom_name, "INTER-BASE") && crc == 0x4332ED95)
    {
        if (size == 65536)
        {
            ERom[rom].type = PALRomType::ccibase;
        }
    }
    
    if (strstr(rom_name, "INTER-WORD") && crc == 0xC93E3C33)
    {
        if (size == 32768)
        {
            ERom[rom].type = PALRomType::cciword;
        }
    }

    if (strstr(rom_name, "AMX") && crc == 0xA72E2E59)
    {
        // outstanding needs updated crc
        if (size == 32768)
        {
            ERom[rom].type = PALRomType::cciword;
        }
    }

    if (strstr(rom_name, "SPELLMASTE") && crc == 0xC26533EC)
    {
        if (size == 131072)
        {
            ERom[rom].type = PALRomType::ccispell;
        }
    }

    if (strstr(rom_name, "Advanced") && crc == 0xE0B93F43)
    {
        if (size == 32768)
        {
            ERom[rom].type = PALRomType::presabe;
        }

    }

    if (strstr(rom_name, "The BASIC") && crc == 0x7E3A119A)
    {
        if (size == 32768)
        {
            ERom[rom].type = PALRomType::presabep;
        }
        
    }

    if (strstr(rom_name, "QUEST") && (crc == 0x7880EB9A || crc == 0x839A9B34))
    {
        if (size == 32768)
        {
            ERom[rom].type = PALRomType::watqst;
        }
    }

    if (strstr(rom_name, "WAPPING ED") && crc == 0x26431B44)
    {
        if (size == 65536)
        {
            ERom[rom].type = PALRomType::watwap;
        }
    }

    if (strstr(rom_name, "TED 32K\0 1") && crc == 0x2D4C6458)
    {
        if (size == 32768)
        {
            ERom[rom].type = PALRomType::watted;
        }
    }

    if (strstr(rom_name, "CONQUEST") && crc == 0xF9634ECE)
    {
        if (size == 32768)
        {
            ERom[rom].type = PALRomType::watqst;
        }
    }



}

uint32_t crc32b(int ROMSEL) {
   int i, j;
   unsigned int byte, crc, mask;

   i = 0;
   crc = 0xFFFFFFFF;
   while (ERom[ROMSEL].rom[i] != 0) {
      byte = ERom[ROMSEL].rom[i];            // Get next byte.
      crc = crc ^ byte;
      for (j = 7; j >= 0; j--) {    // Do eight times.
          mask = (crc & 1);
          mask = mask - 1;
         crc = (crc >> 1) ^ (0xEDB88320 & mask);
      }
      i = i + 1;
   }
   return ~crc;
}
