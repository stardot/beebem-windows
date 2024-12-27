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

Offset           Read               Write
+00         Status Register     Control Latch
+01         Undefined result    Loads Row Counter
+02         Reads Next Byte     Writes Next Byte
+03                 Clears status flags

Status register:
   Bits     Function
   0-3      Link settings (connected to 1, 0, AFC, or 'spare' bit D5)
   4        Field Sync Flag (Set by a field sync pulse)
   5        DEW (High during the teletext portion of the TV field)
   6        DOR (Set by a failure to clear the status flags before start of DEW)
   7        INT (Set by trailing edge of DEW)

Control latch:
   Bits     Function
   0-1      Channel select
   2        Teletext Enable
   3        Enable Interrupts
   4        Enable AFC
   5        Spare latched bit
   6-7      Not latched
*/

#include <windows.h>

#include <string>

#include <stdio.h>
#include <stdlib.h>

#include "Teletext.h"
#include "6502core.h"
#include "BeebMem.h"
#include "Debug.h"
#include "Log.h"
#include "Main.h"
#include "Socket.h"

#define ENABLE_LOG 0

bool TeletextAdapterEnabled = false;
int TeletextAdapterTrigger;

TeletextSourceType TeletextSource;

std::string TeletextFileName[4];
std::string TeletextIP[4];
u_short TeletextPort[4];

enum TTXState {TTXFIELD, TTXFSYNC, TTXDEW};
static TTXState TeletextState = TTXFIELD;

static unsigned char TeletextStatus = 0x0f;
static bool TeletextInts = false;
static bool TeletextEnable = false;
static int TeletextChannel = 0;
static int rowPtr = 0x00;
static int colPtr = 0x00;

static FILE *TeletextFile[4] = { nullptr, nullptr, nullptr, nullptr };
static int TeletextFieldCount[4] = { 0, 0, 0, 0 };
constexpr int TELETEXT_FIELD_SIZE = 860;
static int TeletextCurrentField = 0;

static unsigned char row[16][64] = {0};

static SOCKET TeletextSocket[4] = { INVALID_SOCKET, INVALID_SOCKET, INVALID_SOCKET, INVALID_SOCKET };
static int TeletextConnectTimeout[4] = { 0, 0, 0, 0 };

static char TeletextSocketBuff[4][672*2] = {0}; // hold one frame of t42 data (2 fields)

/*--------------------------------------------------------------------------*/

static bool TeletextConnect(int ch);
static void CloseTeletextSocket(int Index);

bool ConnectSocket(SOCKET Socket, const sockaddr* Name, int Length)
{
    #ifdef WIN32

    if (connect(Socket, Name, Length) == 0)
    {
        return true;
    }
    else
    {
        return WSAGetLastError() == WSAEWOULDBLOCK; // WSAEWOULDBLOCK is expected
    }

    #else

    return connect(Socket, Name, Length) == EAGAIN;

    #endif
}

/*--------------------------------------------------------------------------*/

// Initiate connection on socket

static bool TeletextConnect(int ch)
{
    TeletextConnectTimeout[ch] = 0;

    TeletextSocket[ch] = socket(AF_INET, SOCK_STREAM, 0);

    if (TeletextSocket[ch] == INVALID_SOCKET)
    {
        if (DebugEnabled)
        {
            DebugDisplayTraceF(DebugType::Teletext, true,
                               "Teletext: Unable to create socket %d", ch);
        }

        return false;
    }

    if (DebugEnabled)
    {
        DebugDisplayTraceF(DebugType::Teletext, true,
                           "Teletext: socket %d created, connecting to server", ch);
    }

    SetSocketBlocking(TeletextSocket[ch], false); // non blocking

    struct sockaddr_in teletext_serv_addr;
    teletext_serv_addr.sin_family = AF_INET; // address family Internet
    teletext_serv_addr.sin_port = htons(TeletextPort[ch]); // Port to connect on
    teletext_serv_addr.sin_addr.s_addr = inet_addr(TeletextIP[ch].c_str()); // Target IP

    if (!ConnectSocket(TeletextSocket[ch], (SOCKADDR *)&teletext_serv_addr, sizeof(teletext_serv_addr)))
    {
        if (DebugEnabled)
        {
            DebugDisplayTraceF(DebugType::Teletext, true,
                                "Teletext: Socket %d unable to connect to server %s:%d %d",
                                ch, TeletextIP[ch].c_str(), TeletextPort[ch],
                                GetLastSocketError());
        }

        CloseTeletextSocket(ch);
        return false;
    }

    // How long should we wait (in emulated video fields) for a connection
    // to complete?
    TeletextConnectTimeout[ch] = 50; // allow a full second

    return true;
}

/*--------------------------------------------------------------------------*/

static void CloseTeletextSocket(int Index)
{
    CloseSocket(TeletextSocket[Index]);
    TeletextSocket[Index] = INVALID_SOCKET;
    TeletextConnectTimeout[Index] = 0;
}

/*--------------------------------------------------------------------------*/

void TeletextInit()
{
    TeletextStatus = 0x0f; // Low nibble comes from LK4-7 and mystery links which are left floating
    TeletextInts = false;
    TeletextEnable = false;
    TeletextChannel = 0;
    rowPtr = 0x00;
    colPtr = 0x00;

    if (!TeletextAdapterEnabled)
    {
        ClearTrigger(TeletextAdapterTrigger);
        return;
    }

    TeletextClose();

    if (TeletextSource == TeletextSourceType::IP)
    {
        for (int i = 0; i < TELETEXT_CHANNEL_COUNT; i++)
        {
            TeletextConnect(i);
        }
    }
    else
    {
        for (int i = 0; i < TELETEXT_CHANNEL_COUNT; i++)
        {
            if (!TeletextFileName[i].empty())
            {
                TeletextFile[i] = fopen(TeletextFileName[i].c_str(), "rb");

                if (TeletextFile[i] != nullptr)
                {
                    fseek(TeletextFile[i], 0L, SEEK_END);
                    TeletextFieldCount[i] = (ftell(TeletextFile[i]) / TELETEXT_FIELD_SIZE) & ~1; // ignore trailing odd fields
                    fseek(TeletextFile[i], 0L, SEEK_SET);
                }
                else
                {
                    mainWin->Report(MessageType::Error,
                                    "Cannot open teletext data file:\n %s", TeletextFileName[i].c_str());
                }
            }
        }

        TeletextCurrentField = 0;
    }

    TeletextState = TTXFIELD; // within a field

    SetTrigger(128, TeletextAdapterTrigger); // wait for approximately 1 video line
}

/*--------------------------------------------------------------------------*/

void TeletextClose()
{
    // Close any connected teletext sockets or files
    for (int ch = 0; ch < TELETEXT_CHANNEL_COUNT; ch++)
    {
        if (TeletextSocket[ch] != INVALID_SOCKET)
        {
            if (DebugEnabled)
            {
                DebugDisplayTraceF(DebugType::Teletext, true,
                                   "Teletext: closing socket %d", ch);
            }

            CloseTeletextSocket(ch);
        }

        if (TeletextFile[ch] != nullptr)
        {
            fclose(TeletextFile[ch]);
            TeletextFile[ch] = nullptr;
        }
    }
}

/*--------------------------------------------------------------------------*/

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
            // if (Value * 0x20) set spare bit D5
            // if (Value * 0x10) enable AFC

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
            row[rowPtr][colPtr] = Value & 0xFF;
            colPtr = (colPtr + 1) & 0x3F;
            break;

        case 0x03:
            TeletextStatus &= ~0xD0; // Clear INT, DOR, and FSYN latches
            intStatus &= ~(1 << teletext); // Clear interrupt
            break;
    }
}

/*--------------------------------------------------------------------------*/

unsigned char TeletextRead(int Address)
{
    if (!TeletextAdapterEnabled)
        return 0xff;

    unsigned char data = 0x00;

    switch (Address)
    {
    case 0x00:          // Status Register
        data = TeletextStatus;
        if (TeletextState == TTXDEW)
            data |= 0x20; // raise DEW bit
        break;

    case 0x01:          // Row Register
        break;

    case 0x02:          // Data Register
        #if ENABLE_LOG
        if (colPtr == 0x00)
            WriteLog("TeletextRead Reading Row %d, PC = 0x%04x\n", rowPtr, ProgramCounter);
        // WriteLog("TeletextRead Returning Row %d, Col %d, Data %d, PC = 0x%04x\n", rowPtr, colPtr, row[rowPtr][colPtr], ProgramCounter);
        #endif

        data = row[rowPtr][colPtr];
        colPtr = (colPtr + 1) & 0x3F;
        break;

    case 0x03:
        TeletextStatus &= ~0xD0;       // Clear INT, DOR, and FSYNC latches
        intStatus &= ~(1 << teletext);
        break;
    }

    #if ENABLE_LOG
    WriteLog("TeletextRead Address = 0x%02x, Value = 0x%02x, PC = 0x%04x\n", Address, data, ProgramCounter);
    #endif

    return data;
}

/*--------------------------------------------------------------------------*/

void TeletextAdapterUpdate()
{
    if (!TeletextAdapterEnabled)
    {
        return;
    }

    switch (TeletextState)
    {
        default:
        case TTXFIELD: // transition to FSYNC state
            // (SAA5030 FS goes high around 40us after the true field sync point)
            TeletextState = TTXFSYNC;
            TeletextStatus |= 0x10; // latch FSYNC
            // DEW goes high approximately 290us after FSYNC on an even field, or 320us after FSYNC on an odd field
            IncTrigger((TeletextCurrentField&1)?640:580, TeletextAdapterTrigger); // wait appropriately depending on field
            break;

        case TTXFSYNC: // transition to DEW state
            TeletextStatus &= 0xBF;
            TeletextStatus |= ((TeletextStatus & 0x80) >> 1); // latch INT into DOR
            // the DEW signal remains high for 17 video lines (17 * 128 cycles)
            IncTrigger(2176, TeletextAdapterTrigger); // wait for approximately 17 video lines
            TeletextState = TTXDEW;

            if (TeletextSource == TeletextSourceType::IP)
            {
                char tmpBuff[672*20]; // big enough to hold 20 fields of data

                if (!(TeletextCurrentField&1)) // an even field
                {
                    for (int i = 0; i < TELETEXT_CHANNEL_COUNT; i++)
                    {
                        if (TeletextSocket[i] != INVALID_SOCKET)
                        {
                            int result;

                            // find out how much data is buffered on the socket
                            unsigned long n;
                            if (ioctlsocket(TeletextSocket[i], FIONREAD, &n) == SOCKET_ERROR)
                            {
                                int Error = GetLastSocketError();

                                if (DebugEnabled)
                                {
                                    DebugDisplayTraceF(DebugType::Teletext, true,
                                                       "Teletext: FIONREAD error %d. Closing socket %d",
                                                       Error, i);
                                }

                                CloseTeletextSocket(i);
                            }
                            else if (n > (672*50)) // over 50 fields of data are queued on the socket
                            {
                                // Skip forward 20 fields (2/5 of a second) to attempt
                                // to catch up with server. This occurs when emulation
                                // has been paused by moving the BeebEm window or
                                // interacting with menus. If these delays are allowed
                                // to accumulate we will eventually fill the buffer and
                                // drop the connection we should end up somewhere between
                                // 30 to 50 fields behind "live".

                                if (DebugEnabled)
                                {
                                    DebugDisplayTraceF(DebugType::Teletext, true,
                                                       "Teletext: Skipping forward 2/5 second on socket %d",
                                                       i);
                                }

                                result = recv(TeletextSocket[i], tmpBuff, 672*20, 0);

                                if (result != (672*20))
                                {
                                    // read failed, probably fatal.
                                    int Error = GetLastSocketError();

                                    if (DebugEnabled)
                                    {
                                        DebugDisplayTraceF(DebugType::Teletext, true,
                                                           "Teletext: discard recv error %d. Closing socket %d",
                                                           Error, i);
                                    }

                                    CloseTeletextSocket(i);
                                }
                                else
                                {
                                    TeletextConnectTimeout[i] = 15; // connection is ok, reset the timeout
                                }
                            }
                            else if (n > (672*2))
                            {
                                // attempt to read a full frame of data
                                result = recv(TeletextSocket[i], TeletextSocketBuff[i], 672*2, 0);

                                if (result == 0 || result == SOCKET_ERROR)
                                {
                                    int Error = GetLastSocketError();

                                    if (WouldBlock(Error))
                                    {
                                        if (DebugEnabled)
                                        {
                                            DebugDisplayTraceF(DebugType::Teletext, true,
                                                               "Teletext: WSAEWOULDBLOCK in socket %d. Skipping field",i);
                                        }

                                        if (TeletextConnectTimeout[i] > 0)
                                        {
                                            TeletextConnectTimeout[i]--;
                                            continue; // reuse the connect() timeout to catch hanging sockets where the server has gone away
                                        }
                                    }

                                    if (DebugEnabled)
                                    {
                                        DebugDisplayTraceF(DebugType::Teletext, true,
                                                           "Teletext: recv error %d. Closing socket %d",
                                                           Error, i);
                                    }

                                    CloseTeletextSocket(i);
                                }
                                else if (result != 672*2)
                                {
                                    // failed to read a complete field
                                    if (DebugEnabled)
                                    {
                                        DebugDisplayTraceF(DebugType::Teletext, true,
                                                           "Teletext: short recv detected, received %d bytes. Closing socket %d",
                                                           result, i);
                                    }

                                    CloseTeletextSocket(i);
                                }
                                else
                                {
                                    TeletextConnectTimeout[i] = 50; // connection is ok, reset the timeout
                                }
                            }
                            else
                            {
                                if (TeletextConnectTimeout[i] > 0)
                                {
                                    TeletextConnectTimeout[i]--;
                                    continue;
                                }

                                if (DebugEnabled)
                                {
                                    DebugDisplayTraceF(DebugType::Teletext, true,
                                                       "Teletext: no data. Closing socket %d",
                                                       i);
                                }

                                CloseTeletextSocket(i);
                            }
                        }
                    }
                }

                if (TeletextEnable)
                {
                    // Copy received packets from the TeletextSocketBuff to the teletext adapter's memory
                    for (int i = 0; i < 16; ++i)
                    {
                        int j = i + ((TeletextCurrentField&1)?16:0); // offset odd fields within frame buffer
                        if (TeletextSocketBuff[TeletextChannel][j * 42] != 0)
                        {
                            row[i][0] = 0x27;
                            memcpy(&(row[i][1]), TeletextSocketBuff[TeletextChannel] + j * 42, 42);
                        }
                    }
                }

                TeletextCurrentField ^= 1; // toggle field
            }
            else
            {
                char buff[16 * 43] = {0};

                if (TeletextFile[TeletextChannel] != nullptr)
                {
                    if (TeletextCurrentField >= TeletextFieldCount[TeletextChannel])
                    {
                        TeletextCurrentField = 0;
                    }

                    fseek(TeletextFile[TeletextChannel], TeletextCurrentField * TELETEXT_FIELD_SIZE + 3L * 43L, SEEK_SET);

                    fread(buff, 16 * 43, 1, TeletextFile[TeletextChannel]);
                }

                if (TeletextEnable)
                {
                    for (int i = 0; i < 16; ++i)
                    {
                        if (buff[i*43] != 0)
                        {
                            row[i][0] = 0x27;
                            memcpy(&(row[i][1]), buff + i * 43, 42);
                        }
                    }
                }

                TeletextCurrentField++;
            }

            rowPtr = 0x00;
            colPtr = 0x00;
            break;

        case TTXDEW: // transition to field state
            TeletextStatus |= 0x80; // latch INT
            // FSYNC raises every 20000us. Field duration is 40000 cycles minus 17 lines of DEW and the delay between start of FSYNC and DEW
            // even: 40000 - ((128 * 17) - 640) = 37244
            // odd:  40000 - ((128 * 17) - 580) = 37184
            // TeletextCurrentField has updated in TTXDEW!
            IncTrigger((!(TeletextCurrentField&1))?37184:37244, TeletextAdapterTrigger);
            TeletextState = TTXFIELD;

            if (TeletextInts)
                intStatus |= 1 << teletext; // raise the interrupt
            break;
    }
}
