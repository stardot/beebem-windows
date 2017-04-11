/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2006  Jon Welch

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

// IDE Support for BeebEm
//       2006, Jon Welch  : Initial code
// 26/12/2010, J.G.Harston:
//             Disk images at DiscsPath, not AppPath
// 28/12/2016, J.G.Harston:
//             Checks for valid drive moved to allow command/status
//             to be accessible. SetGeometry sets IDEStatus correctly.
//             Seek returns correct IDEStatus for invalid/absent drive.

/*
IDE Registers
Offset  Description             Access  
+00     data (low byte)           RW  
+01     error if status b0 set    R   
+02     count                     RW  
+03     sector                    RW  
+04     cylinder (low)            RW  
+05     cylinder (high)           RW  
+06     head and device           RW  
+07     status/command            RW  
*/

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <shlobj.h>
#include "6502core.h"
#include "main.h"
#include "beebemrc.h"
#include "ide.h"
#include "beebwin.h"
#include "beebmem.h"

int IDERegs[8];
int IDEStatus;
int IDEError;
int IDEData;
FILE *IDEDisc[8]={NULL,NULL,NULL,NULL,NULL,NULL,NULL,NULL};
int IDEDrive;
char IDEDriveEnabled = 0;
int IDEnumSectors = 64;
int IDEnumHeads = 4;
int IDEDriveMax = 4;

void IDEReset(void)
{
int i;
char buff[256];

    IDEStatus = 0x50;
    IDEError = 0;
    IDEData = 0;

    // NB: Only mount drives 0-IDEDriveMax
    for (i = 0; i < IDEDriveMax; ++i) {

        sprintf(buff, "%s\\ide%d.dat", DiscPath, i);
        IDEDisc[i] = fopen(buff, "rb+");
    
        if (IDEDisc[i] == NULL) {
            IDEDisc[i] = fopen(buff, "wb");
            if (IDEDisc[i] != NULL)
                fclose(IDEDisc[i]);
            IDEDisc[i] = fopen(buff, "rb+");
        }
    }
}

void IDEWrite(int Address, int Value) 
{
    IDERegs[Address] = Value;
    IDEError = 0;

    if (Address == 0x07)        // Command Register
    {
        if (Value == 0x20) DoIDESeek();         // Read command
        if (Value == 0x30) DoIDESeek();         // Write command
        if (Value == 0x91) DoIDEGeometry();     // Set Geometry command
//      if (Value == 0xEC) DoIDEIdentify();     // Identify command
        return;
    }

    if (IDEDrive >= IDEDriveMax)    return;     // This check must be after command register
    if (IDEDisc[IDEDrive] == NULL)  return;     //  to allow another drive to be selected

    if (Address == 0x00)                        // Write Data
    {
        if (IDEData > 0)                        // If in data write cycle,
        {
            fputc(Value, IDEDisc[IDEDrive]);    // Write data byte to file
            IDEData--;
            if (IDEData == 0)                   // If written all data,
            {
                IDEStatus &= ~0x08;             // reset Data Ready
            }
        }
    }
}

int IDERead(int Address)
{
    int data = 0xff;

    switch (Address)
    {
    case 0x00:          // Data register
        if (IDEDrive >= IDEDriveMax) {          // This check must be here to allow
            IDEData = 0;                        //  status registers to be read
        } else {
            if (IDEDisc[IDEDrive] == NULL) {
                IDEData = 0;
            }
        }

        if (IDEData > 0)                        // If in data read cycle,
        {
            data = fgetc(IDEDisc[IDEDrive]);    // read data byte from file.
            IDEData--;
            if (IDEData == 0)                   // If read all data,
            {
                IDEStatus &= ~0x08;             // reset Data Ready
            }
        }
        break;
    case 0x01:          // Error
        data = IDEError;
        break;
    case 0x07:          // Status
        data = IDEStatus;
        break;
    default:            // Other registers
        data = IDERegs[Address];
        break;
    }
    return data;
}


/*                    Heads<5  Heads>4
 *    Track     Head   Drive    Drive
 *     0-8191    0+n     0        0
 *  8192-16383   0+n     1        0
 * 16384-32767   0+n              1
 * 32768-49151   0+n              2
 * 49152-65535   0+n              3
 *     0-8191   16+n     2        4
 *  8192-16383  16+n     3        4
 * 16384-32767  16+n              5
 * 32768-49151  16+n              6
 * 49152-65535  16+n              7
 */

void DoIDESeek(void)
{
int Sector;
int Track;
int Head;
long pos;
int MS;

    IDEData = IDERegs[2] * 256;                 // Number of sectors to read/write, * 256 = bytes
    Sector = IDERegs[3] - 1;                    // Sector number 0 - 63
    Track = (IDERegs[4] + IDERegs[5] * 256);    // Track number
    if (IDEnumHeads < 5) {
      MS = Track / 8192;                        // Drive bit 0 (0/1 or 2/3)
      Track = Track & 8191;                     // Track 0 - 8191
      Head = IDERegs[6] & 0x03;                 // Head 0 - 3
      IDEDrive = (IDERegs[6] & 16) / 8 + MS;    // Drive 0 - 3
    } else {
      MS = Track / 16384;                       // Drive bit 0-1 (0-3 or 4-7)
      Track = Track & 16383;                    // Track 0 - 16383
      Head = IDERegs[6] & 0x0F;                 // Head 0 - 15
      IDEDrive = (IDERegs[6] & 16) / 4 + MS;    // Drive 0 - 7
    }

//  pos = (Track * 4L * 64L + Head * 64L + Sector ) * 256L;     // Absolute position within file
    pos = (Track * IDEnumHeads * IDEnumSectors + Head * IDEnumSectors + Sector) * 256L;

    if (IDEDrive >= IDEDriveMax) {              // Drive out of range
        IDEStatus = 0x01;                       // Not busy, error occured 
        IDEError = 0x10;                        // Sector not found (no media present)
        return;
    }

    if (IDEDisc[IDEDrive] == NULL) {            // No drive image present
        IDEStatus = 0x01;                       // Not busy, error occured 
        IDEError = 0x10;                        // Sector not found (no media present)
        return;
    }

    fseek(IDEDisc[IDEDrive], pos, SEEK_SET);
    IDEStatus |= 0x08;                          // Data Ready
}


void DoIDEGeometry(void)
{
    IDEnumSectors = IDERegs[3];                 // Number of sectors
    IDEnumHeads = (IDERegs[6] & 0x0F) + 1;      // Number of heads
    IDEStatus = 0x50;                           // Not busy, Ready
}
