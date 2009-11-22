/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 1994  David Alan Gilbert
Copyright (C) 1994  Nigel Magnay
Copyright (C) 1997  Mike Wyatt

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

/* Mike Wyatt and NRM's port to win32 - 7/6/97 */

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include <iostream>
#include <fstream>
#include <windows.h>

#include "6502core.h"
#include "beebmem.h"
#include "beebsound.h"
#include "sysvia.h"
#include "uservia.h"
#include "beebwin.h"
#include "disc8271.h"
#include "video.h"
#include "via.h"
#include "atodconv.h"
#include "disc1770.h"
#include "serial.h"
#include "tube.h"
#include "econet.h"	//Rob

#ifdef MULTITHREAD
#undef MULTITHREAD
#endif

extern VIAState SysVIAState;
int DumpAfterEach=0;

unsigned char MachineType;
BeebWin *mainWin = NULL;
HINSTANCE hInst;
HWND hCurrentDialog = NULL;
HACCEL hCurrentAccelTable = NULL;
DWORD iSerialThread,iStatThread; // Thread IDs
DWORD mEthernetPortReadTaskID;	 // ditto
DWORD mEthernetPortStatusTaskID;	 // ditto
FILE *tlog;

int CALLBACK WinMain(HINSTANCE hInstance, 
					HINSTANCE hPrevInstance,
					LPSTR lpszCmdLine,
					int nCmdShow)
{
	MSG msg;

	hInst = hInstance;

    tlog = NULL;

//  tlog = fopen("\\trace.log", "wt");

    mainWin=new BeebWin();
	mainWin->Initialise();

	// Create serial threads
	InitThreads();
	CreateThread(NULL,0,(LPTHREAD_START_ROUTINE) SerialThread,NULL,0,&iSerialThread);
	CreateThread(NULL,0,(LPTHREAD_START_ROUTINE) StatThread,NULL,0,&iStatThread);


	do
	{
		if(PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE) || mainWin->IsFrozen())
		{
			if(!GetMessage(&msg,    // message structure
							NULL,   // handle of window receiving the message
							0,      // lowest message to examine
							0))
				break;              // Quit the app on WM_QUIT

			if (hCurrentAccelTable != NULL)
			{
				TranslateAccelerator(hCurrentDialog, hCurrentAccelTable, &msg);
			}
			if (hCurrentDialog == NULL || !IsDialogMessage(hCurrentDialog, &msg)) {
				TranslateMessage(&msg);// Translates virtual key codes
				DispatchMessage(&msg); // Dispatches message to window
			}
		}

		if (!mainWin->IsFrozen()) {
			Exec6502Instruction();
		}
	} while(1);
  
	mainWin->KillDLLs();

   if (tlog) fclose(tlog);

    delete mainWin;
	Kill_Serial();
	return(0);  
} /* main */

char *mon[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun",
				"Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };


void WriteLog(char *fmt, ...)

{
va_list argptr;
SYSTEMTIME tim;
char buff[256];

    if (tlog)
    {
	    va_start(argptr, fmt);
	    vsprintf(buff, fmt, argptr);
	    va_end(argptr);

        GetLocalTime(&tim);
        fprintf(tlog, "[%02d-%3s-%02d %02d:%02d:%02d.%03d] ", 
	        tim.wDay, mon[tim.wMonth - 1], tim.wYear % 100, tim.wHour, tim.wMinute, tim.wSecond, tim.wMilliseconds);
        
        fprintf(tlog, "%s", buff);
    }

}
