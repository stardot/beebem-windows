
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
/* Please report any problems to the author at beebem@treblig.org           */
/****************************************************************************/
/* Support file for 6522 via - 30/10/94 - David Alan Gilbert */

#ifndef VIA_HEADER
#define VIA_HEADER

typedef struct {
  unsigned char ora,orb;
  unsigned char ira,irb;
  unsigned char ddra,ddrb;
  unsigned char acr,pcr;
  unsigned char ifr,ier;
  int timer1c,timer2c; /* NOTE: Timers descrement at 2MHz and values are */
  int timer1l,timer2l; /*   fixed up on read/write - latches hold 1MHz values*/
  int timer1hasshot; /* True if we have already caused an interrupt for one shot mode */
  int timer2hasshot; /* True if we have already caused an interrupt for one shot mode */
} VIAState;

void VIAReset(VIAState *ToReset);
void SaveVIAState(VIAState *VIAData, unsigned char *StateData);
void RestoreVIAState(VIAState *VIAData, unsigned char *StateData);

void via_dumpstate(VIAState *ToDump);
#endif
