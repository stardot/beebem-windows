
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
/* Class representing an X image communicating via shm - DAG */
#ifndef DAGSHMIMAGE_HEADER
#define DAGSHMIMAGE_HEADER

#include <sys/ipc.h>
#include <sys/shm.h>

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/extensions/XShm.h>

#include "dagXCL/Drawable.h"

class dagShmImage {
	XShmSegmentInfo shminfo;
public:
	XImage *ximage;
	dagShmImage(int Format, int depth,
	            unsigned int width, unsigned int height);
	~dagShmImage();
	void put(dagXDrawable *destination, GC gc, int srcx,int srcy,int destx,int desty,
	    int srcwidth,int srcheight, Bool sendevent);
	char *imageData();
}; /* dagShmImage */
#endif
