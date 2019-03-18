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
#include <process.h>
#include <windows.h>
#include "teletext.h"
#include "debug.h"
#include "6502core.h"
#include "main.h"
#include "beebmem.h"
#include "log.h"

bool TeletextAdapterEnabled = false;
bool TeletextFiles;
bool TeletextLocalhost;
bool TeletextCustom;

int TeletextStatus = 0x0f;
bool TeletextInts = false;
bool TeletextEnable = false;
int txtChnl = 0;
int rowPtrOffset = 0x00;
int rowPtr = 0x00;
int colPtr = 0x00;

FILE *TeletextFile[4] = { NULL, NULL, NULL, NULL };
long txtFrames[4] = { 0, 0, 0, 0 };
long txtCurFrame = 0;

unsigned char row[16][64] = {0};

char TeletextIP[4][20] = { "127.0.0.1", "127.0.0.1", "127.0.0.1", "127.0.0.1" };
u_short TeletextPort[4] = { 19761, 19762, 19763, 19764 };
char TeletextCustomIP[4][20];
u_short TeletextCustomPort[4];

extern WSADATA WsaDat;
static SOCKET TeletextSocket[4] = {INVALID_SOCKET, INVALID_SOCKET, INVALID_SOCKET, INVALID_SOCKET};
bool TeletextSocketConnected[4] = {false, false, false, false};
const int TeletextConnectThreadCh[4] = {0,1,2,3}; // dumb way to get fixed channel numbers into TeletextConnect threads
static HANDLE hTeletextConnectThread[4] = { nullptr, nullptr, nullptr, nullptr };
static unsigned int __stdcall TeletextConnect(void *chparam);

void TeletextLog(char *text, ...)
{
FILE *f;
va_list argptr;

    return;

    va_start(argptr, text);

    f = fopen("teletext.log", "at");
    if (f)
    {
        vfprintf(f, text, argptr);
        fclose(f);
    }

    va_end(argptr);
}

static unsigned int __stdcall TeletextConnect(void *chparam)
{
    /* initiate connection on socket */
    char info[200];
    int ch = *((int *)chparam);
    TeletextSocketConnected[ch] = false;
    
    struct sockaddr_in teletext_serv_addr;
    u_long iMode;
    TeletextSocket[ch] = socket(AF_INET, SOCK_STREAM, 0);
    if (TeletextSocket[ch] == INVALID_SOCKET)
    {
        if (DebugEnabled)
        {
            sprintf(info, "Teletext: Unable to create socket %d", ch);
            DebugDisplayTrace(DebugType::Teletext, true, info);
        }
        return 1;
    }
    if (DebugEnabled)
    {
        sprintf(info, "Teletext: socket %d created", ch);
        DebugDisplayTrace(DebugType::Teletext, true, info);
    }
    
    teletext_serv_addr.sin_family = AF_INET; // address family Internet
    teletext_serv_addr.sin_port = htons (TeletextPort[ch]); //Port to connect on
    teletext_serv_addr.sin_addr.s_addr = inet_addr (TeletextIP[ch]); //Target IP
    
    if (connect(TeletextSocket[ch], (SOCKADDR *)&teletext_serv_addr, sizeof(teletext_serv_addr)) == SOCKET_ERROR)
    {
        if (DebugEnabled) {
            sprintf(info, "Teletext: Socket %d unable to connect to server %s:%d %d",ch,TeletextIP[ch], TeletextPort[ch], WSAGetLastError());
            DebugDisplayTrace(DebugType::Teletext, true, info);
        }
        closesocket(TeletextSocket[ch]);
        TeletextSocket[ch] = INVALID_SOCKET;
        return 1;
    }

    if (DebugEnabled)
    {
        sprintf(info, "Teletext: socket %d connected to server",ch);
        DebugDisplayTrace(DebugType::Teletext, true, info);
    }
    
    iMode = 1;
    ioctlsocket(TeletextSocket[ch], FIONBIO, &iMode); // non blocking
    TeletextSocketConnected[ch] = true;
    return 0;
}

void TeletextInit(void)
{
    int i;
    char buff[256];
    TeletextStatus = 0x0f; /* low nibble comes from LK4-7 and mystery links which are left floating */
    TeletextInts = false;
    TeletextEnable = false;
    txtChnl = 0;
    rowPtr = 0x00;
    colPtr = 0x00;
    
    if (!TeletextAdapterEnabled)
        return;
    
    TeletextClose();
    for (i=0; i<4; i++){
        if (TeletextCustom)
        {
            strcpy(TeletextIP[i], TeletextCustomIP[i]);
            TeletextPort[i] = TeletextCustomPort[i];
        }
    }
    if (TeletextLocalhost || TeletextCustom)
    {
        if (WSAStartup(MAKEWORD(1, 1), &WsaDat) != 0) {
            WriteLog("Teletext: WSA initialisation failed");
            if (DebugEnabled) 
                DebugDisplayTrace(DebugType::Teletext, true, "Teletext: WSA initialisation failed");
            
            return;
        }
        
        for (i=0; i<4; i++){
            /* start thread to handle connecting each socket to avoid holding up main thread */
            hTeletextConnectThread[i] = (HANDLE)_beginthreadex(nullptr, 0, TeletextConnect, (void *)(&TeletextConnectThreadCh[i]), 0, nullptr);
        }
    }
    else
    {
        for (i=0; i<4; i++){
            sprintf(buff, "%s/discims/txt%d.dat", mainWin->GetUserDataPath(), i);
            
            TeletextFile[i] = fopen(buff, "rb");
            
            if (TeletextFile[i])
            {
                fseek(TeletextFile[i], 0L, SEEK_END);
                txtFrames[i] = ftell(TeletextFile[i]) / 860L;
                fseek(TeletextFile[i], 0L, SEEK_SET);
            }
        }
        
        txtCurFrame = 0;
    }
}

void TeletextClose()
{
    /* close any connected teletext sockets or files */
    int ch;
    for (ch=0; ch<4; ch++)
    {
        if (TeletextSocket[ch] != INVALID_SOCKET && TeletextSocketConnected[ch] == true) {
            if (DebugEnabled)
            {
                char info[200];
                sprintf(info, "Teletext: closing socket %d", ch);
                DebugDisplayTrace(DebugType::Teletext, true, info);
            }
            closesocket(TeletextSocket[ch]);
            TeletextSocket[ch] = INVALID_SOCKET;
            TeletextSocketConnected[ch] = false;
        }
        if (TeletextFile[ch])
        {
            fclose(TeletextFile[ch]);
        }
    }
    WSACleanup();
}

void TeletextWrite(int Address, int Value) 
{
    if (!TeletextAdapterEnabled)
        return;

    TeletextLog("TeletextWrite Address = 0x%02x, Value = 0x%02x, PC = 0x%04x\n", Address, Value, ProgramCounter);
    
    switch (Address)
    {
        case 0x00:
            // Status register
            // if (Value * 0x20) mystery links
            // if (Value * 0x10) enable AFC and mystery links
            
            TeletextInts = (Value & 0x08) == 0x08;
            if (TeletextInts && (TeletextStatus & 0x80))
                intStatus|=(1<<teletext);   // interupt if INT and interrupts enabled
            else
                intStatus&=~(1<<teletext);  // Clear interrupt
            
            TeletextEnable = (Value & 0x04) == 0x04;
            
            if ( (Value & 0x03) != txtChnl)
            {
                txtChnl = Value & 0x03;
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
            TeletextStatus &= ~0xD0;       // Clear INT, DOR, and FSYN latches
            intStatus&=~(1<<teletext);     // Clear interrupt
            break;
    }
}

int TeletextRead(int Address)
{
    if (!TeletextAdapterEnabled)
        return 0xff;

    int data = 0x00;

    switch (Address)
    {
    case 0x00:          // Status Register
        data = TeletextStatus;
        break;
    case 0x01:          // Row Register
        break;
    case 0x02:          // Data Register
        if (colPtr == 0x00)
            TeletextLog("TeletextRead Reading Row %d, PC = 0x%04x\n", rowPtr, ProgramCounter);
        // TeletextLog("TeletextRead Returning Row %d, Col %d, Data %d, PC = 0x%04x\n", rowPtr, colPtr, row[rowPtr][colPtr], ProgramCounter);
        
        data = row[rowPtr][colPtr++];
        break;

    case 0x03:
        TeletextStatus &= ~0xD0;       // Clear INT, DOR, and FSYN latches
        intStatus&=~(1<<teletext);
        break;
    }

    TeletextLog("TeletextRead Address = 0x%02x, Value = 0x%02x, PC = 0x%04x\n", Address, data, ProgramCounter);
    
    return data;
}

void TeletextPoll(void)
{
    if (!TeletextAdapterEnabled)
        return;

    int i;
    char info[200];
    
    if (TeletextLocalhost || TeletextCustom)
    {
        int ret[4];
        char socketBuff[4][672] = {0};
        
        for (i=0;i<4;i++)
        {
            if (TeletextSocket[i] != INVALID_SOCKET && TeletextSocketConnected[i] == true)
            {
                ret[i] = recv(TeletextSocket[i], socketBuff[i], 672, 0);
                if (ret[i] < 0){
                    if (WSAGetLastError() == WSAEWOULDBLOCK)
                        break; // not fatal, ignore
                    if (DebugEnabled)
                    {
                        sprintf(info, "Teletext: recv error %d. Closing socket %d", WSAGetLastError(), i);
                        DebugDisplayTrace(DebugType::Teletext, true, info);
                    }
                    closesocket(TeletextSocket[i]);
                    TeletextSocketConnected[i] = false;
                    TeletextSocket[i] = INVALID_SOCKET;
                }
            }
        }
        
        TeletextStatus &= 0x0F;
        TeletextStatus |= 0xD0;       // data ready so latch INT, DOR, and FSYN
        
        if (TeletextEnable == true)
        {
            for (i = 0; i < 16; ++i)
            {
                if (socketBuff[txtChnl][i*42] != 0)
                {
                    row[i][0] = 0x27;
                    memcpy(&(row[i][1]), socketBuff[txtChnl] + i * 42, 42);
                }
            }
        }
    }
    else
    {
        char buff[16 * 43] = {0};
        
        if (TeletextFile[txtChnl])
        {
            if (txtCurFrame >= txtFrames[txtChnl]) txtCurFrame = 0;
            
            fseek(TeletextFile[txtChnl], txtCurFrame * 860L + 3L * 43L, SEEK_SET);
            fread(buff, 16 * 43, 1, TeletextFile[txtChnl]);
            
            TeletextStatus &= 0x0F;
            TeletextStatus |= 0xD0;       // data ready so latch INT, DOR, and FSYN
        }
        
        if (TeletextEnable == true)
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
        
        txtCurFrame++;
    }
    
    rowPtr = 0x00;
    colPtr = 0x00;
    
    if (TeletextInts == true)
        intStatus|=1<<teletext;
}
