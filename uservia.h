
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
/* User VIA support file for the beeb emulator - David Alan Gilbert 11/12/94 */
/* Modified from the system via */

#ifndef USERVIA_HEADER
#define USERVIA_HEADER

#include "via.h"

extern VIAState UserVIAState;

void UserVIAWrite(int Address, int Value);
int UserVIARead(int Address);
void UserVIAReset(void);

void UserVIA_poll_real(void);

#define UserVIA_poll(ncycles) \
  UserVIAState.timer1c-=ncycles; \
  UserVIAState.timer2c-=ncycles; \
  if ((UserVIAState.timer1c<0) || (UserVIAState.timer2c<0)) UserVIA_poll_real();

void SaveUserVIAState(unsigned char *StateData);
void RestoreUserVIAState(unsigned char *StateData);

void uservia_dumpstate(void);
#endif
