/* Create and (possibly map) an X window */
/* David Alan Gilbert 5/11/94 */
#ifndef DAGXWINDOW_HEADER
#define DAGXWINDOW_HEADER

#include "X11/Xlib.h"

#include "dagCL/AssocArray.h"
#include "dagXCL/DispServerConnection.h"

class dagXWindow {
  Window myHandle;
  Display *myDisplay;
  int Valid;
  dagXWindow* myParent; /* Parent for passing events upwards */
  dagXWindow* firstChild; /* head of my child list */
  dagXWindow* nextWindow; /* Next child on the list - i.e. peer */
public:
  static dagAssocArray<Window,dagXWindow> allWinList;

  dagXWindow(Display *display, Window parent, int x, int y,
                           unsigned int width, unsigned int height,
			   unsigned int border_width,
			   int depth,
			   unsigned int xClass,
			   Visual *visual,
			   unsigned long valuemask,
			   XSetWindowAttributes *attributes);

  dagXWindow(int x, int y,
             unsigned int width, unsigned int height,
	     unsigned int border_width,
	     int depth,
	     unsigned int xClass,
	     Visual *visual,
	     unsigned long valuemask,
	     XSetWindowAttributes *attributes);
     

  virtual ~dagXWindow(); /* Virtual so that each cless knows its size??? */

  dagXWindow* addSubWindow(int x, int y,
                           unsigned int width, unsigned int height,
			   unsigned int border_width,
			   int depth,
			   unsigned int xClass,
			   Visual *visual,
			   unsigned long valuemask,
			   XSetWindowAttributes *attributes);
  void Map();
  void SetName(char *newName);
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
  virtual void eventCreateNotify(
}; /* dagXWindow */

#endif
