/* IDE Support for Beebem */
/* Written by Jon Welch */

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
#include "6502core.h"
#include "main.h"
#include "beebemrc.h"
#include "ide.h"

int IDERegs[8];
int IDEStatus;
int IDEData;
FILE *IDEDisc[4];
int IDEDrive;

void IDEReset(void)
{
int i;
char buff[256];

    IDEStatus = 0x40;
    IDEData = 0;

    for (i = 0; i < 4; ++i)
    {
        sprintf(buff, "%s\\discims\\ide%d.dat", mainWin->GetAppPath(), i);

        IDEDisc[i] = fopen(buff, "rb+");
    
        if (IDEDisc[i] == NULL)
        {
            IDEDisc[i] = fopen(buff, "wb");
            fclose(IDEDisc[i]);
            IDEDisc[i] = fopen(buff, "rb+");
        }
    }
}

void IDEWrite(int Address, int Value) 
{

    IDERegs[Address] = Value;

    if (Address == 0x07)        // Control Register
    {
        if (Value == 0x20) DoIDESeek();         // Read command
        if (Value == 0x30) DoIDESeek();         // Write command
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


/*
 * Drive  Head Track
 *   0      0    1
 *   1      0   8193
 *   2     16    1
 *   3     16   8193
 */

void DoIDESeek(void)

{
int Sector;
int Track;
int Head;
long pos;
int MS;

    IDEData = IDERegs[2] * 256;                     // Number of sectors to read/write, * 256 = bytes
    Sector = IDERegs[3] - 1;                        // Sector number 0 - 63
    Track = (IDERegs[4] + IDERegs[5] * 256) - 1;    // Track number
    MS = Track / 8192;                              // Master / Slave drive
    Track = Track & 8191;                           // Track 0 - 8192
    Head = IDERegs[6] & 0x03;                       // Head 0 - 3
    IDEDrive = (IDERegs[6] / 16) * 2 + MS;          // Drive 0 - 3

    pos = (Track * 4L * 64L + Head * 64L + Sector ) * 256L;     // Absolute position within file

    fseek(IDEDisc[IDEDrive], pos, SEEK_SET);

    IDEStatus |= 0x08;                              // Data Ready

}
