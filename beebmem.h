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
/* Beebemulator - memory subsystem - David Alan Gilbert 16/10/94 */
#ifndef BEEBMEM_HEADER
#define BEEBMEM_HEADER

extern int WritableRoms;  /* Is writing to a ROM allowed */

extern unsigned char WholeRam[65536];
/* NOTE: Only to be used if 'a' doesn't change the address */
#define BEEBREADMEM_FAST(a) ((a<0xfc00)?WholeRam[a]:BeebReadMem(a))
/* as BEEBREADMEM_FAST but then increments a */
#define BEEBREADMEM_FASTINC(a) ((a<0xfc00)?WholeRam[a++]:BeebReadMem(a++))

int BeebReadMem(int Address);
void BeebWriteMem(int Address, int Value);
#define BEEBWRITEMEM_FAST(Address, Value) if (Address<0x8000) WholeRam[Address]=Value; else BeebWriteMem(Address,Value);
#define BEEBWRITEMEM_DIRECT(Address, Value) WholeRam[Address]=Value;
char *BeebMemPtrWithWrap(int a, int n);
char *BeebMemPtrWithWrapMo7(int a, int n);
void BeebMemInit(void);

void SaveMemState(unsigned char *MemState);
void RestoreMemState(unsigned char *MemState);

void beebmem_dumpstate(void);
#endif
