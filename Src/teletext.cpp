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

/* Teletext Support for Beebem */

/*

Offset  Description                 Access  
+00     data						R/W  
+01     read status                 R  
+02     write select                W  
+03     write irq enable            W  


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

bool TeleTextAdapterEnabled = false;
int TeleTextStatus = 0xef;
bool TeleTextInts = false;
int rowPtrOffset = 0x00;
int rowPtr = 0x00;
int colPtr = 0x00;

FILE *txtFile = NULL;
long txtFrames = 0;
long txtCurFrame = 0;
int txtChnl = -1;

unsigned char row[16][43];

// TODO: proper configuration instead of hardcoded values
char TeletextIP[4][256] = { "127.0.0.1", "127.0.0.1", "127.0.0.1", "127.0.0.1" };
u_short TeletextPort[4] = { 9991, 9992, 9993, 9994 };

extern WSADATA WsaDat;
static SOCKET TeletextSocket[4] = {INVALID_SOCKET, INVALID_SOCKET, INVALID_SOCKET, INVALID_SOCKET};

void TeleTextLog(char *text, ...)
{
FILE *f;
va_list argptr;

    return;

	va_start(argptr, text);

    f = fopen("\\teletext.log", "at");
    if (f)
    {
        vfprintf(f, text, argptr);
        fclose(f);
    }

	va_end(argptr);
}

typedef struct threadData {
    int ch;
} THREADDATA, *PTHREADDATA;

DWORD WINAPI TeleTextConnect(void* data)
{
    struct sockaddr_in teletext_serv_addr;
    PTHREADDATA pDataArray;
    pDataArray = (PTHREADDATA)data;
    int ch = pDataArray->ch;
    u_long iMode;
    char info[200];
    closesocket(TeletextSocket[ch]);
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
            sprintf(info, "Teletext: Socket %d unable to connect to server %s %d",ch,TeletextIP[ch], WSAGetLastError());
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
    
    return 0;
}

void TeleTextInit(void)
{
    int i;
    TeleTextStatus = 0xef;
    
    if (!TeleTextAdapterEnabled)
        return;
    
    WSACleanup();
    if (WSAStartup(MAKEWORD(1, 1), &WsaDat) != 0) {
        WriteLog("Teletext: WSA initialisation failed");
        if (DebugEnabled) 
            DebugDisplayTrace(DebugType::Teletext, true, "Teletext: WSA initialisation failed");
        
        return;
    }
    
    PTHREADDATA pDataArray[4];
    for (i=0; i<4; i++){
        pDataArray[i] = (PTHREADDATA) HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, sizeof(THREADDATA));
        pDataArray[i]->ch = i;
        CreateThread(NULL,0,TeleTextConnect,pDataArray[i],0,NULL);
    }
    /*
    rowPtr = 0x00;
    colPtr = 0x00;
    
    if (txtFile) fclose(txtFile);

    if (!TeleTextAdapterEnabled)
        return;

    sprintf(buff, "%s/discims/txt%d.dat", mainWin->GetUserDataPath(), txtChnl);
    
    txtFile = fopen(buff, "rb");

    if (txtFile)
    {
        fseek(txtFile, 0L, SEEK_END);
        txtFrames = ftell(txtFile) / 860L;
        fseek(txtFile, 0L, SEEK_SET);
    }

    txtCurFrame = 0;

    TeleTextLog("TeleTextInit Frames = %ld\n", txtFrames);
    */
}

void TeleTextClose(int ch)
{
	if (TeletextSocket[ch] != INVALID_SOCKET) {
		if (DebugEnabled)
        {
            char info[200];
            sprintf(info, "Teletext: closing socket %d", ch);
            DebugDisplayTrace(DebugType::Teletext, true, info);
        }
		closesocket(TeletextSocket[ch]);
		TeletextSocket[ch] = INVALID_SOCKET;
	}
}

void TeleTextWrite(int Address, int Value) 
{
    if (!TeleTextAdapterEnabled)
        return;

    TeleTextLog("TeleTextWrite Address = 0x%02x, Value = 0x%02x, PC = 0x%04x\n", Address, Value, ProgramCounter);
	
    switch (Address)
    {
		case 0x00:
            if ( (Value & 0x0c) == 0x0c)
            {
                TeleTextInts = true;
            }
            if ( (Value & 0x0c) == 0x00) TeleTextInts = false;
            if ( (Value & 0x10) == 0x00) TeleTextStatus &= ~0x10;       // Clear data available

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
            TeleTextStatus &= ~0x10;       // Clear data available
			break;
    }
}

int TeleTextRead(int Address)
{
    if (!TeleTextAdapterEnabled)
        return 0xff;

int data = 0x00;

    switch (Address)
    {
    case 0x00 :         // Data Register
        data = TeleTextStatus;
   		intStatus&=~(1<<teletext);
        break;
    case 0x01:			// Status Register
        break;
    case 0x02:

        if (colPtr == 0x00)
            TeleTextLog("TeleTextRead Reading Row %d, PC = 0x%04x\n", 
                rowPtr, ProgramCounter);

        if (colPtr >= 43)
        {
            TeleTextLog("TeleTextRead Reading Past End Of Row %d, PC = 0x%04x\n", rowPtr, ProgramCounter);
            colPtr = 0;
        }

//        TeleTextLog("TeleTextRead Returning Row %d, Col %d, Data %d, PC = 0x%04x\n", 
//            rowPtr, colPtr, row[rowPtr][colPtr], ProgramCounter);
        
        data = row[rowPtr][colPtr++];

        break;

    case 0x03:
        break;
    }

    TeleTextLog("TeleTextRead Address = 0x%02x, Value = 0x%02x, PC = 0x%04x\n", Address, data, ProgramCounter);
    
    return data;
}

void TeleTextPoll(void)

{
    if (!TeleTextAdapterEnabled)
        return;

int i;
//char buff[16 * 43];
char socketBuff[4][672];
int ret;

    TeleTextStatus |= 0x10;       // teletext data available

    for (i=0;i<4;i++)
    {
        if (TeletextSocket[i] != INVALID_SOCKET)
        {
            ret = recv(TeletextSocket[i], socketBuff[i], 672, 0);
            // todo: something sensible with ret
        }
    }
    
    
    if (TeleTextInts == true)
    {
        intStatus|=1<<teletext;
        for (i = 0; i < 16; ++i)
        {
            if (socketBuff[txtChnl][i*42] != 0)
            {
                row[i][0] = 0x67;
                memcpy(&(row[i][1]), socketBuff[txtChnl] + i * 42, 42);
            }
            else
            {
                row[i][0] = 0x00;
            }
        }
    }
    
    /*
    if (txtFile)
    {

        if (TeleTextInts == true)
        {


            intStatus|=1<<teletext;

//            TeleTextStatus = 0xef;
            rowPtr = 0x00;
            colPtr = 0x00;

            TeleTextLog("TeleTextPoll Reading Frame %ld, PC = 0x%04x\n", txtCurFrame, ProgramCounter);

            fseek(txtFile, txtCurFrame * 860L + 3L * 43L, SEEK_SET);
            fread(buff, 16 * 43, 1, txtFile);
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
        
            txtCurFrame++;
            if (txtCurFrame >= txtFrames) txtCurFrame = 0;
        }
    }
    */

}
