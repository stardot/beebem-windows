/****************************************************************************/
/*              Beebem - (c) David Alan Gilbert 1994/1995                   */
/*              -----------------------------------------                   */
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
/* Please report any problems to the author at gilbertd@cs.man.ac.uk        */
/****************************************************************************/
/* Beeb emulator - main file */
#include "windows.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>

#include <iostream.h>

//#include "dagXCL/DispServerConnection.h"
//#include "dagXCL/Event.h"

#include "6502core.h"
#include "beebmem.h"
#include "sysvia.h"
#include "uservia.h"
#include "beebbitmap.h"
#include "beebwin.h"
#include "disc8271.h"
#include "video.h"

DWORD execloop(DWORD);

#ifdef SUNOS
extern "C" {
extern int on_exit(void (*procp)(int,caddr_t arg),caddr_t arg);
}
#endif

int DumpAfterEach=0;
#include "via.h"
extern VIAState SysVIAState;
BeebWin *mainWin;
HINSTANCE hInst;

static char *Banner= "BeebEm";

static char *Version="0.04 26/01/95";

/*
int XErrHandler(Display *display, XErrorEvent *Err) {
  char errbuff[1024];
  XGetErrorText(display, Err->error_code, errbuff, 1024);
  cerr << "an X error on display " << XDisplayName(NULL) << "text: " << errbuff;
  exit(1);
}

int XIOErrHandler(Display *display) {
  cerr << "oh dear - a serious X error\n";
  exit(1);
}
*/
#ifndef SUNOS
void AtExitHandler(void) {
#else
void AtExitHandler(int status, caddr_t arg) {
#endif
  delete mainWin;
}; /* AtExitHandler */

void sighandler_exit(int a) {
/*  extern int Stats[256];
  int x,y;
  for(y=0;y<0x100;y+=0x10) {
    for(x=0;x<0x10;x++) {
      cerr.width(8);
      cerr << Stats[x+y] << " ";
    };
    cerr << "\n";
  }; */
  cerr << "Caught an INT!\n";
  exit(0);
};

void sighandler_toggledump(int a) {
/*  DumpAfterEach^=1;
  signal(SIGUSR1,sighandler_toggledump);*/
};

void sighandler_dumpstate(int a) {
  /*core_dumpstate();
  video_dumpstate();
  disc8271_dumpstate();
  uservia_dumpstate();
  sysvia_dumpstate();
  beebmem_dumpstate();
  signal(SIGUSR2,sighandler_dumpstate);*/
}; 

int CALLBACK WinMain(HINSTANCE hInstance, 
					HINSTANCE hPrevInstance,
					LPSTR lpszCmdLine,
					int nCmdShow)
{
  //cerr << Banner << "\n" << "Version: " << Version << "\n";
  MSG msg;
  HANDLE hAccelTable;
  
  hInst = hInstance;
  
  BeebMemInit();
  Init6502core();
  SysVIAReset();
  UserVIAReset();
  VideoInit();
  Disc8271_reset();
  DumpRegs();

  //if (argc>1) DumpAfterEach=1;
  /* Initialisation for X */
  //(new dagXDispServerConnection())->setPrimary();
  //XSynchronize(_primaryDisplay->getXDisplay(),1);

  //XSetErrorHandler(XErrHandler);
  //XSetIOErrorHandler(XIOErrHandler);
  mainWin=new BeebWin();

  //signal(SIGUSR1,sighandler_toggledump);
  //signal(SIGUSR2,sighandler_dumpstate);
  //signal(SIGINT,sighandler_exit);

//#ifndef SUNOS
//  atexit(AtExitHandler);
//#else
//  on_exit(AtExitHandler,NULL);
//#endif

  //while (1) {
   // static dagXEvent myEvent;

  //  Exec6502Instruction();
     
    /* Process all events on the queue before doing next instruction */
    //while (myEvent.checkAndDespatch())/* cerr << "Event!\n"*/;
  //}	;

  
  do
  {
  		if(PeekMessage(&msg, NULL, 0, 0, PM_NOREMOVE))
		{
	  		if(!GetMessage(&msg, // message structure
	        			NULL,   // handle of window receiving the message
	           			0,      // lowest message to examine
	           			0))
	        	return(0);	// Quit the app on WM_QUIT
  
	        if (!TranslateAccelerator (msg.hwnd, hAccelTable, &msg)) {
	                TranslateMessage(&msg);// Translates virtual key codes
	                DispatchMessage(&msg); // Dispatches message to window
	        }
		}

		Exec6502Instruction();
  }	while(1);
  
  
  /*
  DWORD	threadID;
  CreateThread(NULL,0,(LPTHREAD_START_ROUTINE)execloop,0,0,&threadID);

  while(GetMessage(&msg, // message structure
	       	NULL,   // handle of window receiving the message
	        0,      // lowest message to examine
	        0))
  {	        	
  
        if (!TranslateAccelerator (msg.hwnd, hAccelTable, &msg)) {
                TranslateMessage(&msg);// Translates virtual key codes
                DispatchMessage(&msg); // Dispatches message to window
        }
  };
*/

  return(0);  
} /* main */

DWORD execloop(DWORD unused)
{
	while(-1)
	{
		Exec6502Instruction();
	};
  return(0);
}
