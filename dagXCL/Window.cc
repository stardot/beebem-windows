
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
/* Create and (possibly map) an X window */
/* David Alan Gilbert 5/11/94 */
#include "X11/Xlib.h"
#include "X11/Xutil.h"

#include "dagCL/AssocArray.h"
#include "dagXCL/DispServerConnection.h"
#include "dagXCL/XErr.h"
#include "dagXCL/Window.h"

dagXWindow::dagXWindow(dagXDispServerConnection* con, Window parent, int x, int y,
                   unsigned int width, unsigned int height,
		   unsigned int border_width,
		   int depth,
		   unsigned int xClass,
		   Visual *visual,
		   unsigned long valuemask,
		   XSetWindowAttributes *attributes) :
    firstChild(NULL), nextWindow(NULL), myParent(NULL) {
  myHandle=XCreateWindow(con->getXDisplay(),parent,x,y,width,height,
   border_width,depth,xClass,visual,valuemask,attributes);
  myDisplay=con->getXDisplay();
  myScreen=con->getXScreen();
  allWinList.add(myHandle,this);
}

void dagXWindow::bindSubWindow(dagXWindow* subWindow) {
  /* Add the new window into the parent's list */
  subWindow->nextWindow=firstChild;
  firstChild=subWindow;

  subWindow->myParent=this;
} /* bindSubWindow */

void dagXWindow::Map() {
  XMapWindow(myDisplay, myHandle);
}

void dagXWindow::SetName(char *newName) {
  /* Based on code from rxvt in its change_window_name routine */
  XTextProperty name;

  if (XStringListToTextProperty(&newName,1,&name) ==0)
    throw dagErr("No Memory","Can't allocate window name");

  XSetWMName(myDisplay,myHandle,&name);
  XFree(name.value);
} /* SetName */

void dagXWindow::SetEventTypeMask(long event_mask) {
  XSelectInput(myDisplay,myHandle, event_mask);
} /* SetEventTypeMask */

void dagXWindow::Background(unsigned long background_pixel) {
  XSetWindowBackground(myDisplay,myHandle,1);
} /* background */

dagXWindow::~dagXWindow() {
  dagXWindow* toDestroy=firstChild;
  dagXWindow* next;
  XDestroyWindow(myDisplay,myHandle);

  /* got to destroy all the children without actually calling X stuff! */
  while (toDestroy) {
    next=toDestroy->nextWindow;

    delete toDestroy;

    toDestroy=next;
  } /* Child destroy loop */

  /* Remove the window from the x-handle to object translation list */
  allWinList.remove(this);
}

void dagXWindow::eventKeyDown(XKeyEvent xkey) {
  if (myParent) myParent->eventKeyDown(xkey);
}; /* KeyDown */

void dagXWindow::eventKeyUp(XKeyEvent xkey) {
  if (myParent) myParent->eventKeyUp(xkey);
}; /* KeyUp */

void dagXWindow::eventButtonDown(XButtonEvent xbutton) {
  if (myParent) myParent->eventButtonDown(xbutton);
}; /* ButtonDown */

void dagXWindow::eventButtonUp(XButtonEvent xbutton) {
  if (myParent) myParent->eventButtonUp(xbutton);
}; /* ButtonUp */

void dagXWindow::eventMotion(XMotionEvent xmotion) {
  if (myParent) myParent->eventMotion(xmotion);
}; /* Motion */

void dagXWindow::eventPtrEnter(XCrossingEvent xcrossing) {
  if (myParent) myParent->eventPtrEnter(xcrossing);
}; /* PtrEnter */

void dagXWindow::eventPtrLeave(XCrossingEvent xcrossing) {
  if (myParent) myParent->eventPtrLeave(xcrossing);
}; /* PtrLeave */

void dagXWindow::eventFocusIn(XFocusChangeEvent xfocus) {
  if (myParent) myParent->eventFocusIn(xfocus);
}; /* FocusIn */

void dagXWindow::eventFocusOut(XFocusChangeEvent xfocus) {
  if (myParent) myParent->eventFocusOut(xfocus);
}; /* FocusOut */

void dagXWindow::eventKeymapNotify(XKeymapEvent xkeymap) {
  if (myParent) myParent->eventKeymapNotify(xkeymap);
}; /* KeymapNotify */

void dagXWindow::eventExpose(XExposeEvent xexpose) {
  if (myParent) myParent->eventExpose(xexpose);
}; /* Expose */

void dagXWindow::eventGraphicExpose(XGraphicsExposeEvent xgraphicsexpose) {
  if (myParent) myParent->eventGraphicExpose(xgraphicsexpose);
}; /* GraphicExpose */

void dagXWindow::eventNoExpose(XNoExposeEvent xnoexpose) {
  if (myParent) myParent->eventNoExpose(xnoexpose);
}; /* NoExpose */

void dagXWindow::eventVisibilityNotify(XVisibilityEvent xvisibility) {
  if (myParent) myParent->eventVisibilityNotify(xvisibility);
}; /* VisibilityNotify */

void dagXWindow::eventCreateNotify(XCreateWindowEvent xcreatewindow) {
  if (myParent) myParent->eventCreateNotify(xcreatewindow);
}; /* CreateNotify */

void dagXWindow::eventDestroyNotify(XDestroyWindowEvent xdestroywindow) {
  if (myParent) myParent->eventDestroyNotify(xdestroywindow);
}; /* DestroyNotify */

void dagXWindow::eventUnmapNotify(XUnmapEvent xunmap) {
  if (myParent) myParent->eventUnmapNotify(xunmap);
}; /* UnmapNotify */

void dagXWindow::eventMapNotify(XMapEvent xmap) {
  if (myParent) myParent->eventMapNotify(xmap);
}; /* MapNotify */

void dagXWindow::eventMapRequest(XMapRequestEvent xmaprequest) {
  if (myParent) myParent->eventMapRequest(xmaprequest);
}; /* MapRequest */

void dagXWindow::eventReparentNotify(XReparentEvent xreparent) {
  if (myParent) myParent->eventReparentNotify(xreparent);
}; /* ReparentNotify */

void dagXWindow::eventConfigureNotify(XConfigureEvent xconfigure) {
  if (myParent) myParent->eventConfigureNotify(xconfigure);
}; /* ConfigureNotify */

void dagXWindow::eventConfigureRequest(XConfigureRequestEvent xconfigurerequest) {
  if (myParent) myParent->eventConfigureRequest(xconfigurerequest);
}; /* ConfigureRequest */

void dagXWindow::eventGravityNotify(XGravityEvent xgravity) {
  if (myParent) myParent->eventGravityNotify(xgravity);
}; /* GravityNotify */

void dagXWindow::eventResizeRequest(XResizeRequestEvent xresizerequest) {
  if (myParent) myParent->eventResizeRequest(xresizerequest);
}; /* ResizeRequest */

void dagXWindow::eventCirculateNotify(XCirculateEvent xcirculate) {
  if (myParent) myParent->eventCirculateNotify(xcirculate);
}; /* CirculateNotify */

void dagXWindow::eventCirculateRequest(XCirculateRequestEvent xcirculaterequest) {
  if (myParent) myParent->eventCirculateRequest(xcirculaterequest);
}; /* CirculateRequest */

void dagXWindow::eventPropertyNotify(XPropertyEvent xproperty) {
  if (myParent) myParent->eventPropertyNotify(xproperty);
}; /* PropertyNotify */

void dagXWindow::eventSelectionClear(XSelectionClearEvent xselectionclear) {
  if (myParent) myParent->eventSelectionClear(xselectionclear);
}; /* SelectionClear */

void dagXWindow::eventSelectionRequest(XSelectionRequestEvent xselectionrequest) {
  if (myParent) myParent->eventSelectionRequest(xselectionrequest);
}; /* SelectionRequest */

void dagXWindow::eventSelectionNotify(XSelectionEvent xselection) {
  if (myParent) myParent->eventSelectionNotify(xselection);
}; /* SelectionNotify */

void dagXWindow::eventColourmapNotify(XColormapEvent xcolormap) {
  if (myParent) myParent->eventColourmapNotify(xcolormap);
}; /* ColourmapNotify */

void dagXWindow::eventClientMessage(XClientMessageEvent xclient) {
  if (myParent) myParent->eventClientMessage(xclient);
}; /* ClientMessage */

void dagXWindow::eventMappingNotify(XMappingEvent xmapping) {
  if (myParent) myParent->eventMappingNotify(xmapping);
}; /* MappingNotify */

/* Declaration of static members */
dagAssocArray<Window, dagXWindow> dagXWindow::allWinList(0 /* No cleanup */);
