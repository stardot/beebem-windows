
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
/* Event handling under X

David Alan Gilbert 6/11/94 */

#ifndef DAGXCLEVENT_HEADER
#define DAGXCLEVENT_HEADER

#include "limits.h"

#include "X11/Xlib.h"
#include "X11/X.h"

class dagXEvent {
  XEvent e;
public:
  void waitForEvent();
  void waitAndDespatch();
  int checkForEvent(long event_mask=ULONG_MAX);
  int checkAndDespatch(long event_mask=ULONG_MAX);
  void despatch();
}; /* dagXEvent */

#endif
