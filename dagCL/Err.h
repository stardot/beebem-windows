
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
/* A class for throwing in case of error - expected to be
a base class for some more specific error types
David Alan Gilbert 5/11/94 */

#ifndef DAGERR_HEADER
#define DAGERR_HEADER

#include <iostream.h>
#include <stdlib.h>

class dagErr {
  friend ostream& operator<<(ostream& s, dagErr& err);
  const char *Type,*Message;
public:
  dagErr(const char *iType, const char *iMessage) : Type(iType), Message(iMessage) { cerr << this; abort(); };
};

ostream& operator<<(ostream& s, dagErr& err);

/* Until we figure out what is going on */
#define throw

#endif

