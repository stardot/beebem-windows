
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

#include "dagXCL/Event.h"
#include "dagXCL/Window.h"

void dagXEvent::waitForEvent() {
  XNextEvent(_primaryDisplay->getXDisplay(),&e);
} /* waitForEvent */

/* Return true if we have an event */
int dagXEvent::checkForEvent(long event_mask) {
  return(XCheckMaskEvent(_primaryDisplay->getXDisplay(),
    event_mask,&e));
}

/* Get an event - if one arrives despatch it and return true */
int dagXEvent::checkAndDespatch(long event_mask) {
  if (XCheckMaskEvent(_primaryDisplay->getXDisplay(), event_mask, &e)) {
    despatch();
    return(1);
  };

  return(0);
}

void dagXEvent::waitAndDespatch() {
  waitForEvent();
  despatch();
}

void dagXEvent::despatch() {
  dagXWindow *eventWindow=dagXWindow::allWinList[e.xany.window];

  switch (e.type) {
    case KeyPress:
      eventWindow->eventKeyDown(e.xkey);
      break;

    case KeyRelease:
      eventWindow->eventKeyUp(e.xkey);
      break;

    case ButtonPress:
      eventWindow->eventButtonDown(e.xbutton);
      break;

    case ButtonRelease:
      eventWindow->eventButtonUp(e.xbutton);
      break;

    case MotionNotify:
      eventWindow->eventMotion(e.xmotion);
      break;

    case EnterNotify:
      eventWindow->eventPtrEnter(e.xcrossing);
      break;

    case LeaveNotify:
      eventWindow->eventPtrLeave(e.xcrossing);
      break;

    case FocusIn:
      eventWindow->eventFocusIn(e.xfocus);
      break;

    case FocusOut:
      eventWindow->eventFocusOut(e.xfocus);
      break;

    case KeymapNotify: /* NOTE: Not sure if this should distribute to windows */
      eventWindow->eventKeymapNotify(e.xkeymap);
      break;

    case Expose:
      eventWindow->eventExpose(e.xexpose);
      break;

    case GraphicsExpose:
      eventWindow->eventGraphicExpose(e.xgraphicsexpose);
      break;

    case NoExpose:
      eventWindow->eventNoExpose(e.xnoexpose);
      break;

    case VisibilityNotify:
      eventWindow->eventVisibilityNotify(e.xvisibility);
      break;

    case CreateNotify:
      eventWindow->eventCreateNotify(e.xcreatewindow);
      break;

    case DestroyNotify:
      eventWindow->eventDestroyNotify(e.xdestroywindow);
      break;

    case UnmapNotify:
      eventWindow->eventUnmapNotify(e.xunmap);
      break;

    case MapNotify:
      eventWindow->eventMapNotify(e.xmap);
      break;

    case MapRequest:
      eventWindow->eventMapRequest(e.xmaprequest);
      break;

    case ReparentNotify:
      eventWindow->eventReparentNotify(e.xreparent);
      break;

    case ConfigureNotify:
      eventWindow->eventConfigureNotify(e.xconfigure);
      break;

    case ConfigureRequest:
      eventWindow->eventConfigureRequest(e.xconfigurerequest);
      break;

    case GravityNotify:
      eventWindow->eventGravityNotify(e.xgravity);
      break;

    case ResizeRequest:
      eventWindow->eventResizeRequest(e.xresizerequest);
      break;

    case CirculateNotify:
      eventWindow->eventCirculateNotify(e.xcirculate);
      break;

    case CirculateRequest:
      eventWindow->eventCirculateRequest(e.xcirculaterequest);
      break;

    case PropertyNotify:
      eventWindow->eventPropertyNotify(e.xproperty);
      break;

    case SelectionClear:
      eventWindow->eventSelectionClear(e.xselectionclear);
      break;

    case SelectionRequest:
      eventWindow->eventSelectionRequest(e.xselectionrequest);
      break;

    case SelectionNotify:
      eventWindow->eventSelectionNotify(e.xselection);
      break;

    case ColormapNotify:
      eventWindow->eventColourmapNotify(e.xcolormap);
      break;

    case ClientMessage:
      eventWindow->eventClientMessage(e.xclient);
      break;

    case MappingNotify:
      eventWindow->eventMappingNotify(e.xmapping);
      break;

    default:
      cerr << "Unhandled event type=" << e.type;
      break;
   } /* Type switch */
} /* despatch */

