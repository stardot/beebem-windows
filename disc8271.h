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
/*--------------------------------------------------------------------------*/
/* 8271 disc emulation - David Alan Gilbert 4/12/94 */
#ifndef DISC8271_HEADER
#define DISC8271_HEADER

extern int Disc8271Trigger; /* Cycle based time Disc8271Trigger */

void LoadSimpleDSDiscImage(char *FileName, int DriveNum,int Tracks);
void LoadSimpleDiscImage(char *FileName, int DriveNum,int HeadNum, int Tracks);

/*--------------------------------------------------------------------------*/
/* Address is in the range 0-7 - with the fe80 etc stripped out */
int Disc8271_read(int Address);

/*--------------------------------------------------------------------------*/
/* Address is in the range 0-7 - with the fe80 etc stripped out */
void Disc8271_write(int Address, int Value);

/*--------------------------------------------------------------------------*/
void Disc8271_poll_real(void);

#define Disc8271_poll(ncycles) if (Disc8271Trigger<=TotalCycles) Disc8271_poll_real();

/*--------------------------------------------------------------------------*/
void Disc8271_reset(void);

/*--------------------------------------------------------------------------*/
void disc8271_dumpstate(void);
#endif
