
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
#ifndef DAGXWINDOW_HEADER
#define DAGXWINDOW_HEADER

#include "X11/Xlib.h"

#include "dagCL/AssocArray.h"
#include "dagXCL/DispServerConnection.h"
#include "dagXCL/Drawable.h"
#include "dagXCL/Event.h"
#include "dagXCL/Window.h"

/*                 N O T E
   Child windows bound to the parent using bindSubWindow are destroyed
   when the parent window is destroyed - they should not destroy their X
   window since the parent will do a hierarchical destroy.  This facility
   should only be used by windows allocated using new.
*/

class dagXWindow : public dagXDrawable {
  friend dagXEvent;
protected:
  Window myHandle;
  dagXWindow* myParent; /* Parent for passing events upwards */
  dagXWindow* firstChild; /* head of my child list */
  dagXWindow* nextWindow; /* Next child on the list - i.e. peer */
  Screen* xScreen; /* The screen we are on */
  dagXWindow(dagXDispServerConnection* con, Window parent, int x, int y,
                           unsigned int width, unsigned int height,
			   unsigned int border_width,
			   int depth,
			   unsigned int xClass,
			   Visual *visual,
			   unsigned long valuemask,
			   XSetWindowAttributes *attributes);


  void bindSubWindow(dagXWindow* parent); /* Auto destroy child at end */
  void Map();
  void SetName(char *newName);
  void Background(unsigned long background_pixel);
  void SetEventTypeMask(long event_mask);

  virtual void eventKeyDown(XKeyEvent xkey);
  virtual void eventKeyUp(XKeyEvent xkey);
  virtual void eventButtonDown(XButtonEvent xbutton);
  virtual void eventButtonUp(XButtonEvent xbutton);
  virtual void eventMotion(XMotionEvent xmotion);
  virtual void eventPtrEnter(XCrossingEvent xcrossing);
  virtual void eventPtrLeave(XCrossingEvent xcrossing);
  virtual void eventFocusIn(XFocusChangeEvent xfocus);
  virtual void eventFocusOut(XFocusChangeEvent xfocus);
  virtual void eventKeymapNotify(XKeymapEvent xkeymap);
  virtual void eventExpose(XExposeEvent xexpose);
  virtual void eventGraphicExpose(XGraphicsExposeEvent xgraphicsexpose);
  virtual void eventNoExpose(XNoExposeEvent xnoexpose);
  virtual void eventVisibilityNotify(XVisibilityEvent xvisibility);
  virtual void eventCreateNotify(XCreateWindowEvent xcreatewindow);
  virtual void eventDestroyNotify(XDestroyWindowEvent xdestroywindow);
  virtual void eventUnmapNotify(XUnmapEvent xunmap);
  virtual void eventMapNotify(XMapEvent xmap);
  virtual void eventMapRequest(XMapRequestEvent xmaprequest);
  virtual void eventReparentNotify(XReparentEvent xreparent);
  virtual void eventConfigureNotify(XConfigureEvent xconfigure);
  virtual void eventConfigureRequest(XConfigureRequestEvent xconfigurerequest);
  virtual void eventGravityNotify(XGravityEvent xgravity);
  virtual void eventResizeRequest(XResizeRequestEvent xresizerequest);
  virtual void eventCirculateNotify(XCirculateEvent xcirculate);
  virtual void eventCirculateRequest(XCirculateRequestEvent xcirculaterequest);
  virtual void eventPropertyNotify(XPropertyEvent xproperty);
  virtual void eventSelectionClear(XSelectionClearEvent xselectionclear);
  virtual void eventSelectionRequest(XSelectionRequestEvent xselectionrequest);
  virtual void eventSelectionNotify(XSelectionEvent xselection);
  virtual void eventColourmapNotify(XColormapEvent xcolormap);
  virtual void eventClientMessage(XClientMessageEvent xclient);
  virtual void eventMappingNotify(XMappingEvent xmapping);
public:
  static dagAssocArray<Window,dagXWindow> allWinList;
  virtual ~dagXWindow(); /* Virtual so that each cless knows its size??? */
  Drawable xDrawable(void) { return myHandle; };
	Screen* myScreen;
	Display* myDisplay;
}; /* dagXWindow */

#endif
