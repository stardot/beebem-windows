
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
/* First attempts to hack shm images on - I have >>NO<< documentation
   on how to do this - so I'm trying to hack this based on the mpeg
   player source! */

#include "iostream.h"

#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>

#ifdef SUNOS
extern "C" {
/* SunOS seems to be missing these */
extern int shmget(key_t key, int size, int shmflg);
extern char *shmat(int shmid, char*shmaddr, int shmflg);
extern int shmdt(char *shmaddr);
extern int shmctl (int shmid, int cmd, struct shmid_ds *buf);
};
#endif

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/extensions/XShm.h>

#include "dagXCL/DispServerConnection.h"
#include "dagXCL/Drawable.h"
#include "dagXCL/XErr.h"
#include "dagXCL/shmImage.h"

dagShmImage::dagShmImage(int Format, int depth,
                          unsigned int width, unsigned int height) {
  ximage=XShmCreateImage(_primaryDisplay->getXDisplay(), 
                         None, depth,
                         Format, NULL, &shminfo, width, height);
  if (ximage==NULL) {
    throw dagXErr("Failed creating shared image");
  };

  shminfo.shmid=shmget(IPC_PRIVATE, (ximage->bytes_per_line * ximage->height),
     IPC_CREAT|0777);

  if (shminfo.shmid<0) {
    throw dagErr("Shared memory","shmget failed in shmImage");
  };

  shminfo.shmaddr=(char *)shmat(shminfo.shmid,0,0);
  if (shminfo.shmaddr == ((char *)-1)) {
    XDestroyImage(ximage);
    ximage=NULL;
    throw dagErr("Shared memory","Failed to attach shared memory in shmImage");
  };

  ximage->data=shminfo.shmaddr;
  XShmAttach(_primaryDisplay->getXDisplay(),&shminfo);
  XSync(_primaryDisplay->getXDisplay(),0);

  cerr << "Warning (from shmImage) - should really think about byte ordering etc.\n";

}; /* dagShmImage::dagShmImage */

dagShmImage::~dagShmImage() {
  XShmDetach(_primaryDisplay->getXDisplay(),&shminfo);
  XDestroyImage(ximage);
  shmdt(shminfo.shmaddr);
  shmctl(shminfo.shmid, IPC_RMID,0);
  ximage=NULL;
}; /* dagShmImage::~dagShmImage */

void dagShmImage::put(dagXDrawable *destination, GC gc, int srcx,int srcy,int destx,int desty,
      int srcwidth,int srcheight, Bool sendevent) {
  XShmPutImage(_primaryDisplay->getXDisplay()/*destination->myDisplay*/,destination->xDrawable(),
               gc,ximage,srcx,srcy,destx,desty,srcwidth,srcheight,sendevent);
}; /* dagShmImage::put */

char *dagShmImage::imageData() {
  return(ximage->data);
}; /* dagShmImage::imageData */
