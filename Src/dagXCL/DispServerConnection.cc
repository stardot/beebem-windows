
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
/* Please report any problems to the author at gilbertd@cs.man.ac.uk        */
/****************************************************************************/
/* A class which on creation opens a connection to an X server, and neatly */
/* closses it in its destructor */
/* David Alan Gilbert 5/11/94 */
#include "X11/Xlib.h"

#include "iostream.h"

#include "dagXCL/XErr.h"
#include "dagXCL/DispServerConnection.h"

dagXDispServerConnection* dagXDispServerConnection::primary=NULL;
dagXDispServerConnection* _primaryDisplay=NULL;

dagXDispServerConnection::dagXDispServerConnection(char *display_name) {
  /* cerr << "in constructor of dagXDispServerConnection\n"; */

  if (xDisplay=XOpenDisplay(display_name),xDisplay==NULL)
    throw dagXErr("Could not open X display");
  xScreen=XScreenOfDisplay(xDisplay,DefaultScreen(xDisplay));
}

dagXDispServerConnection::~dagXDispServerConnection() {
  XCloseDisplay(xDisplay);
  xDisplay=NULL;
}

void dagXDispServerConnection::setPrimary() {
  /* cerr << "in setPrimary of dagXDispServerConnection\n"; */
  primary=this;
  _primaryDisplay=this;
}

dagXDispServerConnection *dagXDispServerConnection::getPrimary() {
  return(primary);
}

/* Return the default root of the primary connection */
Window dagXDispServerConnection::root() {
  return(DefaultRootWindow(xDisplay));
}

Display *dagXDispServerConnection::getXDisplay() {
  return(xDisplay);
}

GC dagXDispServerConnection::defaultGC() {
  return(DefaultGC(xDisplay,XScreenNumberOfScreen(xScreen)));
}; /* defaultGC */
