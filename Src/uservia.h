/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 1994  David Alan Gilbert
Copyright (C) 1997  Mike Wyatt
Copyright (C) 2004  Ken Lowe

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

/* User VIA support file for the beeb emulator - David Alan Gilbert 11/12/94 */
/* Modified from the system via */

#ifndef USERVIA_HEADER
#define USERVIA_HEADER

#include "viastate.h"
#include "via.h"

extern VIAState UserVIAState;
extern unsigned char WTDelay1,WTDelay2;
extern int RTC_Enabled;
extern unsigned char SWRAMBoardEnabled;

void UserVIAWrite(int Address, int Value);
int UserVIARead(int Address);
void UserVIAReset(void);
void UserVIA_poll(unsigned int ncycles);

void uservia_dumpstate(void);

/* AMX mouse enabled */
extern int AMXMouseEnabled;

/* Map Left+Right button presses to a middle button press */
extern int AMXLRForMiddle;

/* Number of cycles between each mouse interrupt */
#define AMX_TRIGGER 1000
extern int AMXTrigger;

/* Checks if a movement interrupt should be generated */
void AMXMouseMovement();

/* Target and current positions for mouse */
extern int AMXTargetX;
extern int AMXTargetY;
extern int AMXCurrentX;
extern int AMXCurrentY;

/* Button states */
#define AMX_LEFT_BUTTON 1
#define AMX_MIDDLE_BUTTON 2
#define AMX_RIGHT_BUTTON 4
extern int AMXButtons;

/* Printer enabled */
extern int PrinterEnabled;
void PrinterEnable(char *FileName);
void PrinterDisable();

/* Trigger for checking for printer ready. */
#define PRINTER_TRIGGER 25
extern int PrinterTrigger;
void PrinterPoll();

/* User Port Breakout Box */

void GetBitKeysUsed( char *Keys );
extern int	BitKey;			// Used to store the bit key pressed while we wait 
void ShowBitKey(int key, int ctrlID);
char *BitKeyName( int Key );
void SetBitKey( int ctrlID );
int GetValue(int ctrlID);
void SetValue(int ctrlID, int State);
extern int BitKeys[8];
extern int LastBitButton;
extern bool mBreakOutWindow;

void ShowInputs(unsigned char data);
void ShowOutputs(unsigned char data);

void BreakOutOpenDialog(HINSTANCE hinst, HWND hwndMain);
void BreakOutCloseDialog();
HWND	PromptForBitKeyInput( HWND hwndParent, UINT ctrlID );
BOOL	CALLBACK BreakOutDlgProc( HWND   hwnd,
									   UINT   nMessage,
									   WPARAM wParam,
									   LPARAM lParam );
LRESULT CALLBACK GetBitKeyWndProc( HWND hWnd,	
								UINT message,
								WPARAM uParam,
								LPARAM lParam);


#endif
