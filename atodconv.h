/****************************************************************************/
/*                               Beebem                                     */
/*                               ------                                     */
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
/****************************************************************************/
/* Analogue to digital converter support file for the beeb emulator -
   Mike Wyatt 7/6/97 */

#ifndef ATODCONV_HEADER
#define ATODCONV_HEADER

extern int JoystickEnabled;
extern int JoystickX;  /* 16 bit number, 0 = right */
extern int JoystickY;  /* 16 bit number, 0 = down */

void AtoDWrite(int Address, int Value);
int AtoDRead(int Address);
void AtoDInit(void);
void AtoDReset(void);

extern int AtoDTrigger;  /* For next A to D conversion completion */

void AtoD_poll_real(void);
#define AtoD_poll(ncycles) if (AtoDTrigger<=TotalCycles) AtoD_poll_real();

#endif
