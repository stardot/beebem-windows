/****************************************************************************/
/*                               Beebem                                     */
/*                               ------                                     */
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
/****************************************************************************/
/* Mike Wyatt and NRM's port to win32 - 7/6/97 */

#include "windows.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include <iostream.h>

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

extern VIAState SysVIAState;
int DumpAfterEach=0;

unsigned char MachineType;
BeebWin *mainWin = NULL;
HINSTANCE hInst;

int CALLBACK WinMain(HINSTANCE hInstance, 
					HINSTANCE hPrevInstance,
					LPSTR lpszCmdLine,
					int nCmdShow)
{
	MSG msg;
  
	hInst = hInstance;
	mainWin=new BeebWin();
	mainWin->Initialise();
    if (MachineType==0) {
	BeebMemInit();
	Init6502core();
	SysVIAReset();
	UserVIAReset();
	VideoInit();
	Disc8271_reset();
	SoundReset();
	AtoDReset(); 
	}
	if (MachineType==1) {
//	memset(WholeRam,0,0x5000);
//	memset(FSRam,0,0x2000);
//	memset(ShadowRAM,0,0x5000);
//	memset(PrivateRAM,0,0x1000);
	ACCCON=0;
	PagedRomReg=0xf;
	BeebMemInit();
	Init6502core();
	SysVIAReset();
	UserVIAReset();
	VideoInit();
	Disc8271_reset();
	Reset1770();
	SoundReset();
	AtoDReset();
	UseShadow=0;
	}


	do
	{
		if(PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE) || mainWin->IsFrozen())
		{
			if(!GetMessage(&msg,    // message structure
							NULL,   // handle of window receiving the message
							0,      // lowest message to examine
							0))
				break;              // Quit the app on WM_QUIT
  
			TranslateMessage(&msg);// Translates virtual key codes
			DispatchMessage(&msg); // Dispatches message to window
		}

		if (!mainWin->IsFrozen())
			Exec6502Instruction();
	} while(1);
  
	delete mainWin;
	
	return(0);  
} /* main */
