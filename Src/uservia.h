
/****************************************************************************/
/*              Beebem - (c) David Alan Gilbert 1994                        */
/*              ------------------------------------                        */
/* This program may be distributed freely within the following restrictions:*/
/*                                                                          */
/* 1) You may not charge for this program or for any part of it.            */
/* 2) This copyright message must be distributed with all copies.           */
/* 3) This program must be distributed complete with source code.  Binary   */
/*    only distribution is not permitted.                                   */
/* 4) The author offers no warrenties, or guarentees etc. - you use it at   */
/*    your own risk.  If it messes something up or destroys your computer   */
/*    thats YOUR problem.                                                   */
/* 5) You may use small sections of code from this program in your own      */
/*    applications - but you must acknowledge its use.  If you plan to use  */
/*    large sections then please ask the author.                            */
/*                                                                          */
/* If you do not agree with any of the above then please do not use this    */
/* program.                                                                 */
/* Please report any problems to the author at beebem@treblig.org           */
/****************************************************************************/
/* User VIA support file for the beeb emulator - David Alan Gilbert 11/12/94 */
/* Modified from the system via */

#ifndef USERVIA_HEADER
#define USERVIA_HEADER

#include "viastate.h"
#include "via.h"

extern VIAState UserVIAState;
extern unsigned char WTDelay1,WTDelay2;
extern int RTC_Enabled;

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
