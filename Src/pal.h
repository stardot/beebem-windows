/***********************************************************
 * BBC Micro PALPROM carrier board
 * Work based on original work by Nigel Barnes (Mame)
 * (C) Nigel Barnes (Original Work)
 * (S) Steve Inglis (derivative work for beebem5)
 *
 * The original work and all the heavy lifting was done by Nigel
 *
 *
***************************************************************************/

#ifndef ROM_PAL_H
#define ROM_PAL_H

#include <stdint.h>
#include <stdio.h>

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
    watted
};

constexpr int MAX_EROMS = 16;
constexpr int MAX_PALROM_SIZE = 131072;

struct largeRom {
    unsigned char rom[MAX_PALROM_SIZE];
    PALRomType type  = PALRomType::none;
    uint8_t m_bank = 0;
};

uint8_t cciword_device(int romsel, int offset);
uint8_t ccibase_device(int romsel, int offset);
uint8_t ccispell_device(int romsel, int offset);
uint8_t acotrilogy(int romsel, int offset);
uint8_t pres_device(int romsel, int offset);

uint8_t ExtendedRom(int romsel, int offset);
uint32_t GetRomFileSize(FILE *handle);
void GuessRomType(int rom, uint32_t size);

#endif
