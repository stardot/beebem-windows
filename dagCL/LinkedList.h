/****************************************************************************/
/*              Beebem - (c) David Alan Gilbert 1995                        */
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
/* A template for a linked list class                                       */
#ifndef DAGLINKEDLIST_HEADER
#define DAGLINKEDLIST_HEADER

#include <dagCL/Err.h>

template<class Content> class dagLinkedList_Element {
  friend dagLinkedList<Content>;
  friend dagLinkedList_Iter<Content>;

  Content* content;
  dagLinkedList_Element<Content>* next;

  dagLinkedList_Element(Content* c,
    dagLinkedList_Element<Content>* n) : content(c), next(n) {};
}; /* dagLinkedList_Element */

template<class Content> class dagLinkedList {
  int DestroyContents; /* If !=0 the contents will be destroyed when the
                          array is */
  dagLinkedList_Element<Content>* first;

public:
  dagLinkedList(int CleanUp) :
     DestroyContents(CleanUp), first(NULL) { };

  ~dagLinkedList() {
    dagLinkedList_Element<Content>* delPtr=first;
    dagLinkedList_Element<Content>* next;

    while (delPtr) {
      if (DestroyContents) delete delPtr->content;
      next=delPtr->next;
      delete delPtr;
      delPtr=next;
    } /* Run along the list deleting entries (and possibly contents) */
  } /* Destructor */

  void remove(Content *c) {
    dagLinkedList_Element<Content>* searchPtr=first;
    dagLinkedList_Element<Content>** ptrToMod=&first;

    /* Search for the item */
    for(;searchPtr;ptrToMod=&(searchPtr->next),searchPtr=searchPtr->next) {
      if (c==searchPtr->content) {
        if (DestroyContents) delete searchPtr->content;
        *ptrToMod=searchPtr->next;
        return;
      }
    }
  } /* remove */
        
  void add(Content* c) {
    /* Create a new entry on the front of the list */
    first=new dagLinkedList_Element<Index,Content>(i,c,first);
  }

  /* Returns true if the list is empty */
  int QueryEmpty() {
    return(first==NULL);
  };

  friend dagLinkedList_Iter<Content>;
};

/* An iterator for that linked list, based on the one in Stroustrop p.267 */
template <Content> class dagLinkedList_Iter {
  dagLinkedList_Element<Content>* ce; /* Current element */
  dagLinkedList<Content>* cl; /* The current list */

public:
    inline dagLinkedList_Iter(dagLinkedList<Content>* l) {
      cl=s;
      ce=s->first;
    };

    inline Content* operator() () {
        dagLinkedList_Element<Content>*  ret;

        ret=ce;
        if (ce!=NULL) ce=ce->next;

        return(ret->content);
    };
};

#endif
