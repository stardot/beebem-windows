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

/*

Offset  Description                 Access  
+00     Status register             R/W
+01     Row register
+02     Data register
+03     Clear status register

Status register:
  Read
   Bits     Function
   0-3      Link settings
   4        FSYN (Latches high on Field sync)
   5        DEW (Data entry window)
   6        DOR (Latches INT on end of DEW)
   7        INT (latches high on end of DEW)
  
  Write
   Bits     Function
   0-1      Channel select
   2        Teletext Enable
   3        Enable Interrupts
   4        Enable AFC (and mystery links A)
   5        Mystery links B

*/

#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#include "teletext.h"
#include "debug.h"
#include "6502core.h"
#include "main.h"
#include "beebmem.h"
#include "log.h"

#define ENABLE_LOG 0

bool TeletextAdapterEnabled = false;
bool TeletextFiles;
bool TeletextLocalhost;
bool TeletextCustom;

unsigned char TeletextStatus = 0x0f;
bool TeletextInts = false;
bool TeletextEnable = false;
int TeletextChannel = 0;
int rowPtrOffset = 0x00;
int rowPtr = 0x00;
int colPtr = 0x00;

FILE *TeletextFile[4] = { nullptr, nullptr, nullptr, nullptr };
int TeletextFrameCount[4] = { 0, 0, 0, 0 };
const int TELETEXT_FRAME_SIZE = 860;
int TeletextCurrentFrame = 0;

unsigned char row[16][64] = {0};

char TeletextIP[4][20] = { "127.0.0.1", "127.0.0.1", "127.0.0.1", "127.0.0.1" };
u_short TeletextPort[4] = { 19761, 19762, 19763, 19764 };
char TeletextCustomIP[4][20];
u_short TeletextCustomPort[4];

static SOCKET TeletextSocket[4] = { INVALID_SOCKET, INVALID_SOCKET, INVALID_SOCKET, INVALID_SOCKET };
bool TeletextSocketConnected[4] = { false, false, false, false };

// Initiate connection on socket

static int TeletextConnect(int ch)
{
    TeletextSocketConnected[ch] = false;

    TeletextSocket[ch] = socket(AF_INET, SOCK_STREAM, 0);
    if (TeletextSocket[ch] == INVALID_SOCKET)
    {
        if (DebugEnabled)
        {
            DebugDisplayTraceF(DebugType::Teletext, true,
                               "Teletext: Unable to create socket %d", ch);
        }
        return 1;
    }

    if (DebugEnabled)
    {
        DebugDisplayTraceF(DebugType::Teletext, true,
                           "Teletext: socket %d created, connecting to server", ch);
    }

    u_long iMode = 1;
    ioctlsocket(TeletextSocket[ch], FIONBIO, &iMode); // non blocking

    struct sockaddr_in teletext_serv_addr;
    teletext_serv_addr.sin_family = AF_INET; // address family Internet
    teletext_serv_addr.sin_port = htons (TeletextPort[ch]); //Port to connect on
    teletext_serv_addr.sin_addr.s_addr = inet_addr (TeletextIP[ch]); //Target IP

    if (connect(TeletextSocket[ch], (SOCKADDR *)&teletext_serv_addr, sizeof(teletext_serv_addr)) == SOCKET_ERROR)
    {
        if (WSAGetLastError() != WSAEWOULDBLOCK) // WSAEWOULDBLOCK is expected
        {
            if (DebugEnabled)
            {
                DebugDisplayTraceF(DebugType::Teletext, true,
                                   "Teletext: Socket %d unable to connect to server %s:%d %d",
                                   ch, TeletextIP[ch], TeletextPort[ch],
                                   WSAGetLastError());
            }
            closesocket(TeletextSocket[ch]);
            TeletextSocket[ch] = INVALID_SOCKET;
            return 1;
        }
    }

    TeletextSocketConnected[ch] = true;
    return 0;
}

void TeletextInit(void)
{
    int i;
    TeletextStatus = 0x0f; /* low nibble comes from LK4-7 and mystery links which are left floating */
    TeletextInts = false;
    TeletextEnable = false;
    TeletextChannel = 0;
    rowPtr = 0x00;
    colPtr = 0x00;

    if (!TeletextAdapterEnabled)
        return;

    TeletextClose();

    for (i = 0; i < 4; i++)
    {
        if (TeletextCustom)
        {
            strcpy(TeletextIP[i], TeletextCustomIP[i]);
            TeletextPort[i] = TeletextCustomPort[i];
        }
    }

    if (TeletextLocalhost || TeletextCustom)
    {
        for (i = 0; i < 4; i++)
        {
            TeletextConnect(i);
        }
    }
    else
    {
        for (i = 0; i < 4; i++)
        {
            char pathname[256];
            sprintf(pathname, "%s/discims/txt%d.dat", mainWin->GetUserDataPath(), i);

            TeletextFile[i] = fopen(pathname, "rb");

            if (TeletextFile[i] != nullptr)
            {
                fseek(TeletextFile[i], 0L, SEEK_END);
                TeletextFrameCount[i] = ftell(TeletextFile[i]) / TELETEXT_FRAME_SIZE;
                fseek(TeletextFile[i], 0L, SEEK_SET);
            }
        }

        TeletextCurrentFrame = 0;
    }
}

void TeletextClose()
{
    /* close any connected teletext sockets or files */
    for (int ch = 0; ch < 4; ch++)
    {
        if (TeletextSocket[ch] != INVALID_SOCKET)
        {
            if (DebugEnabled)
            {
                DebugDisplayTraceF(DebugType::Teletext, true,
                                   "Teletext: closing socket %d", ch);
            }

            closesocket(TeletextSocket[ch]);
            TeletextSocket[ch] = INVALID_SOCKET;
            TeletextSocketConnected[ch] = false;
        }

        if (TeletextFile[ch])
        {
            fclose(TeletextFile[ch]);
            TeletextFile[ch] = nullptr;
        }
    }
}

void TeletextWrite(int Address, int Value) 
{
    if (!TeletextAdapterEnabled)
        return;

    #if ENABLE_LOG
    WriteLog("TeletextWrite Address = 0x%02x, Value = 0x%02x, PC = 0x%04x\n", Address, Value, ProgramCounter);
    #endif

    switch (Address)
    {
        case 0x00:
            // Status register
            // if (Value * 0x20) mystery links
            // if (Value * 0x10) enable AFC and mystery links

            TeletextInts = (Value & 0x08) == 0x08;
            if (TeletextInts && (TeletextStatus & 0x80))
                intStatus |= (1 << teletext); // Interrupt if INT and interrupts enabled
            else
                intStatus &= ~(1 << teletext); // Clear interrupt

            TeletextEnable = (Value & 0x04) == 0x04;

            if ((Value & 0x03) != TeletextChannel)
            {
                TeletextChannel = Value & 0x03;
            }
            break;

        case 0x01:
            rowPtr = Value;
            colPtr = 0x00;
            break;

        case 0x02:
            row[rowPtr][colPtr++] = Value & 0xFF;
            break;

        case 0x03:
            TeletextStatus &= ~0xD0; // Clear INT, DOR, and FSYN latches
            intStatus &= ~(1 << teletext); // Clear interrupt
            break;
    }
}

unsigned char TeletextRead(int Address)
{
    if (!TeletextAdapterEnabled)
        return 0xff;

    unsigned char data = 0x00;

    switch (Address)
    {
    case 0x00:          // Status Register
        data = TeletextStatus;
        break;
    case 0x01:          // Row Register
        break;
    case 0x02:          // Data Register
        #if ENABLE_LOG
        if (colPtr == 0x00)
            WriteLog("TeletextRead Reading Row %d, PC = 0x%04x\n", rowPtr, ProgramCounter);
        // WriteLog("TeletextRead Returning Row %d, Col %d, Data %d, PC = 0x%04x\n", rowPtr, colPtr, row[rowPtr][colPtr], ProgramCounter);
        #endif

        data = row[rowPtr][colPtr++];
        break;

    case 0x03:
        TeletextStatus &= ~0xD0;       // Clear INT, DOR, and FSYN latches
        intStatus &= ~(1 << teletext);
        break;
    }

    #if ENABLE_LOG
    WriteLog("TeletextRead Address = 0x%02x, Value = 0x%02x, PC = 0x%04x\n", Address, data, ProgramCounter);
    #endif

    return data;
}

void TeletextPoll(void)
{
    if (!TeletextAdapterEnabled)
        return;

    int i;

    if (TeletextLocalhost || TeletextCustom)
    {
        char socketBuff[4][672] = {0};

        for (i = 0; i < 4; i++)
        {
            if (TeletextSocket[i] != INVALID_SOCKET)
            {
                int result = recv(TeletextSocket[i], socketBuff[i], 672, 0);

                if (result == SOCKET_ERROR)
                {
                    int err = WSAGetLastError();
                    if (err == WSAEWOULDBLOCK)
                        break; // not fatal, ignore

                    if (DebugEnabled)
                    {
                        DebugDisplayTraceF(DebugType::Teletext, true,
                                           "Teletext: recv error %d. Closing socket %d",
                                           err, i);
                    }

                    closesocket(TeletextSocket[i]);
                    TeletextSocketConnected[i] = false;
                    TeletextSocket[i] = INVALID_SOCKET;
                }
            }
        }

        TeletextStatus &= 0x0F;
        TeletextStatus |= 0xD0; // data ready so latch INT, DOR, and FSYN

        if (TeletextEnable)
        {
            for (i = 0; i < 16; ++i)
            {
                if (socketBuff[TeletextChannel][i * 42] != 0)
                {
                    row[i][0] = 0x27;
                    memcpy(&(row[i][1]), socketBuff[TeletextChannel] + i * 42, 42);
                }
            }
        }
    }
    else
    {
        char buff[16 * 43] = {0};

        if (TeletextFile[TeletextChannel] != nullptr)
        {
            if (TeletextCurrentFrame >= TeletextFrameCount[TeletextChannel])
            {
                TeletextCurrentFrame = 0;
            }

            fseek(TeletextFile[TeletextChannel], TeletextCurrentFrame * TELETEXT_FRAME_SIZE + 3L * 43L, SEEK_SET);
            fread(buff, 16 * 43, 1, TeletextFile[TeletextChannel]);

            TeletextStatus &= 0x0F;
            TeletextStatus |= 0xD0;       // data ready so latch INT, DOR, and FSYN
        }

        if (TeletextEnable)
        {
            for (i = 0; i < 16; ++i)
            {
                if (buff[i*43] != 0)
                {
                    row[i][0] = 0x67;
                    memcpy(&(row[i][1]), buff + i * 43, 42);
                }
                else
                {
                    row[i][0] = 0x00;
                }
            }
        }

        TeletextCurrentFrame++;
    }

    rowPtr = 0x00;
    colPtr = 0x00;

    if (TeletextInts)
        intStatus |= 1 << teletext;
}
