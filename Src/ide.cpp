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

/*

Offset  Description                 Access  
+00     data (low byte) on CS0      R/W  
+01     error on CS0                R  
+02     count on CS0                RW  
+03     sector on CS0               RW  
+04     cylinder (low) on CS0       RW  
+05     cylinder (high) on CS0      RW  
+06     head on CS0                 RW  
+07     status on CS0               R  

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
int IDEData;
FILE *IDEDisc[8];
int IDEDrive;
char IDEDriveEnabled = 0;
int IDEnumSectors = 64;
int IDEnumHeads = 4;

void IDEReset(void)
{
int i;
char buff[256];

    IDEStatus = 0x40;
    IDEData = 0;

    // NB: Only mount drives 0-3
    for (i = 0; i < 4; ++i)
    {
        sprintf(buff, "%s\\ide%d.dat", DiscPath, i);
        IDEDisc[i] = fopen(buff, "rb+");
    
        if (IDEDisc[i] == NULL)
        {
            IDEDisc[i] = fopen(buff, "wb");
            if (IDEDisc[i] != NULL)
                fclose(IDEDisc[i]);
            IDEDisc[i] = fopen(buff, "rb+");
        }
    }
}

void IDEWrite(int Address, int Value) 
{
    if (IDEDisc[IDEDrive] == NULL)
        return;

    IDERegs[Address] = Value;

    if (Address == 0x07)        // Command Register
    {
        if (Value == 0x20) DoIDESeek();         // Read command
        if (Value == 0x30) DoIDESeek();         // Write command
        if (Value == 0x91) DoIDEGeometry();     // Set Geometry command
//      if (Value == 0xEC) DoIDEIdentify();     // Identify command
    }

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
    if (IDEDisc[IDEDrive] == NULL)
        return data;

    switch (Address)
    {
    case 0x00 :         // Data register
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
    case 0x01:
        break;
    case 0x02:
        break;
    case 0x03:
        break;
    case 0x04:
        break;
    case 0x05:
        break;
    case 0x06:
        break;
    case 0x07:          // Status on CS0
        data = IDEStatus;
        break;
    }

    return data;
}


/*                  Heads<5  Heads>4
 * Head   Track     Drive    Drive
 *  0-3   0-8191      0        0
 *  0-3  8192-16383   1        0
 *  4-7   0-8191               1
 *  4-7  8192-16383            1
 *  8-11  0-8191               2
 *  8-11 8192-16383            2
 * 12-15  0-8191               3
 * 12-15 8192-16383            3
 * 16-19  0-8191      2        4
 * 16-19 8192-16383   3        4
 * 20-23  0-8191               5
 * 20-23 8192-16383            5
 * 24-27  0-8191               6
 * 24-27 8192-16383            6
 * 28-31  0-8191               7
 * 28-31 8192-16383            7
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
      MS = Track / 8192;                        // Master / Slave drive
      Track = Track & 8191;                     // Track 0 - 8191
      Head = IDERegs[6] & 0x03;                 // Head 0 - 3
      IDEDrive = (IDERegs[6] / 16) * 2 + MS;    // Drive 0 - 3
    } else {
      MS = Track / 16384;                       // Master / Slave drive
      Track = Track & 16383;                    // Track 0 - 16383
      Head = IDERegs[6] & 0x0F;                 // Head 0 - 15
      IDEDrive = (IDERegs[6] / 16) * 4 + MS;    // Drive 0 - 7
    }

//  pos = (Track * 4L * 64L + Head * 64L + Sector ) * 256L;     // Absolute position within file
    pos = (Track * IDEnumHeads * IDEnumSectors + Head * IDEnumSectors + Sector) * 256L;

    if (IDEDisc[IDEDrive] == NULL)
        return;

    fseek(IDEDisc[IDEDrive], pos, SEEK_SET);

    IDEStatus |= 0x08;                          // Data Ready
}


void DoIDEGeometry(void)
{
    IDEnumSectors = IDERegs[3];                 // Number of sectors
    IDEnumHeads = (IDERegs[6] & 0x0F) + 1;      // Number of heads
    IDEStatus |= 0x08;                          // Data Ready
}
