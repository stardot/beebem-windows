/****************************************************************************/
/*              Beebem - (c) David Alan Gilbert 1994/5                      */
/*              --------------------------------------                      */
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

/* This system is designed to maintain a graph of values which are related -
  for example the width, height and position of windows and subwindows in
  X */
#ifndef DAGDEPVAL_HEADER
#define DAGDEPVAL_HEADER

#include "dagCL/Err.h"
#include "dagCL/LinkedList.h"

typedef enum depValOperator {
  depVal_Add,
  depVal_Sub,
  depVal_Mul,
  depVal_Div
};

/****************************************************************************/
/* Errors relating to depVal                                                */
class dagDepValErr : private dagErr {
public:
    dagDepValErr(const char *iMessage) : dagErr("DepVal",iMessage) {};
};

/****************************************************************************/
/* Temporary home of notification class */
/* The idea is that anything derived from this class has a 'changed'
   method which can be called to tell it a dependant variable has been
   modified and it may like to do something about it.  Also it can tell if
   something is being destroyed */
class dagNotify {
  /* 'ChangedItem' has just changed - shall I do anything ? */
  virtual void changed(dagNotify* ChangedItem) {};

  /* DestroyedItem is about to get destroyed - do I use ? */
  virtual void destroy(dagNotify* DestroyedItem) {};

};

/****************************************************************************/
/* Base class of all my dependant values                                    */
class DepVal : public dagNotify {
friend class DepVal_Handle;

Private:
  int BeingDestroyed; /* Stops recursion in cycles during destruction of instances */
  dagLinkedList<dagNotify> DepOnMe(0); /* A list of all things dependant on me */

Protected:
  virtual void NotifyChange() {
    dagLinkedList<dagNotify> chanit(DepOnMe);
    dagNotify *pres;

    /* Tell everyone we've changed */
    while (pres=chanit())
      pres->changed(this);
  };

Public:
  /* Make something else dependant on my value being changed */
  void AddDep(dagNotify* ToNotify) {
    DepOnMe.add(ToNotify);
  };

  void DelDep(dagNotify* ToNotNotify) {
    DepOnMe.remove(ToNotNotify);
  };
  
  DepVal() {
    BeingDestroyed=0;
  };

  /* Create but make something dependant on it (e.g. a window) */
  DepVal(dagNotify* ToNotify) {
    DepVal();
    DepOnMe.add(ToNotify);
  };

  ~DepVal() {
      BeingDestroyed=1;

      dagLinkedList<dagNotify> chanit(DepOnMe);
      dagNotify *pres;

      /* Tell everyone we've changed */
      while (pres=chanit())
        pres->destroy(this);
      };

      BeingDestroyed=0;
  };

  virtual long Value() =0;

  /* Alternative to Value */
  inline operator long() { return Value(); };
};

/****************************************************************************/
/* A handle class for DepVal (and its descendants                           */
/* Allows the representation of the DepVal to change                        
class DepVal_Handle {
  DepVal* rep;
public:
  DepVal* operator->() { return rep; }

  DepVal_Handle(DepVal* pp) : rep(pp) { };
};

/****************************************************************************/
/* A Dependant value which holds numbers                                    */
class DepVal_Num {
  long myVal;

public:
  DepVal_Num(long Val) { myVal=Val };

  long Value() { return myVal };

  /* Set a new value */
  long DepVal_Num::operator=(const long newval) {
    myVal=val;

    NotifyChange();
  };
};

#endif

