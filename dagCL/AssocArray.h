
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
/* A simple implementation of an associative array  - using a singly linked 
linked list David Alan Gilbert 06/11/94 */
#ifndef DAGASSOCARRAY_HEADER
#define DAGASSOCARRAY_HEADER

/*
  We keep a copy of the index and use a pointer to the contents - this is
  on the basic that the contents are liable to be bigger/more complex than
  the index - I suppose that may not be true....


  One unusual feature is that it does not add entries when they are not
  found - instead it throws a dagAssocArray_NotFound exception.

  Lookup's are done via [] and adding an item is done explicitly via
  the use of the 'add' method.
*/

#include <dagCL/Err.h>

class dagAssocArrayErr : private dagErr {
public:
  dagAssocArrayErr(const char *iMessage) : dagErr("Assoc Array", iMessage) {};
};

template<class Index, class Content> class dagAssocArray_Element {
  friend dagAssocArray<Index,Content>;
  Index index;
  Content* content;
  dagAssocArray_Element<Index,Content>* next;

  dagAssocArray_Element(Index i, Content* c,
    dagAssocArray_Element<Index,Content>* n) : index(i), content(c), next(n) {};
}; /* dagAssocArray_Element */

template<class Index, class Content> class dagAssocArray {
  int DestroyContents; /* If !=0 the contents will be destroyed when the
                          array is */
  dagAssocArray_Element<Index,Content>* first;

public:
  dagAssocArray(int CleanUp) :
     DestroyContents(CleanUp), first(NULL) { };

  ~dagAssocArray() {
    dagAssocArray_Element<Index,Content>* delPtr=first;
    dagAssocArray_Element<Index,Content>* next;

    while (delPtr) {
      if (DestroyContents) delete delPtr->content;
      next=delPtr->next;
      delete delPtr;
      delPtr=next;
    } /* Run along the list deleting entries (and possibly contents) */
  } /* Destructor */

  Content* operator[](const Index i) {
    dagAssocArray_Element<Index,Content>* searchPtr=first;

    /* Search for the next item */
    for(;searchPtr;searchPtr=searchPtr->next)
      if (i==searchPtr->index) return(searchPtr->content);

    throw dagAssocArrayErr("Not found");
  } /* lookup */

  /* Remove at the moment only allows you to remove an item based on the
     Content - no the index. Life would get difficult if Index and Content
     were of comparable types and function overloading could be a problem if
     i declared both remove(Index) and remove(Content *) ??? */
  void remove(Content *c) {
    dagAssocArray_Element<Index,Content>* searchPtr=first;
    dagAssocArray_Element<Index,Content>** ptrToMod=&first;

    /* Search for the item */
    for(;searchPtr;ptrToMod=&(searchPtr->next),searchPtr=searchPtr->next) {
      if (c==searchPtr->content) {
        if (DestroyContents) delete searchPtr->content;
        *ptrToMod=searchPtr->next;
        return;
      }
    }

    throw dagAssocArrayErr("Not Found");
  } /* remove */
        
  void add(const Index i, Content* c) {
    /* Create a new entry on the front of the list */
    first=new dagAssocArray_Element<Index,Content>(i,c,first);
  }
};
#endif
