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

#include <stdio.h>
#include <stdarg.h>

#include <windows.h>

#include "6502core.h"
#include "beebwin.h"
#include "log.h"
#include "serial.h"

Model MachineType;
BeebWin *mainWin = NULL;
HINSTANCE hInst;
HWND hCurrentDialog = NULL;
HACCEL hCurrentAccelTable = NULL;

int CALLBACK WinMain(HINSTANCE hInstance, HINSTANCE /* hPrevInstance */,
                     LPSTR /* lpszCmdLine */, int /* nCmdShow */)
{
	MSG msg;

	hInst = hInstance;

	OpenLog();

	mainWin = new BeebWin();
	if (!mainWin->Initialise())
	{
		return 1;
	}

	// Create serial threads
	SerialInit();

	do
	{
		if (PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE) || mainWin->IsFrozen())
		{
			if (!GetMessage(&msg,   // message structure
			                NULL,   // handle of window receiving the message
			                0,      // lowest message to examine
			                0))
				break; // Quit the app on WM_QUIT

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

	CloseLog();

	Kill_Serial();

	delete mainWin;

	return 0;
}
