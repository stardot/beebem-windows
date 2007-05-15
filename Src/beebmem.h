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
/* Beebemulator - memory subsystem - David Alan Gilbert 16/10/94 */
#ifndef BEEBMEM_HEADER
#define BEEBMEM_HEADER

#include "stdio.h"

extern int RomWritable[16]; /* Allow writing to ROMs on an individual basis */

extern unsigned char WholeRam[65536];
extern unsigned char Roms[16][16384];
extern unsigned char ROMSEL;
extern int PagedRomReg;
/* Master 128 Specific Stuff */
extern unsigned char FSRam[8192]; // 8K Filing System RAM
extern unsigned char PrivateRAM[4096]; // 4K Private RAM (VDU Use mainly)
extern unsigned char CMOSRAM[64]; // 50 Bytes CMOS RAM
extern unsigned char ShadowRAM[32768]; // 20K Shadow RAM
extern unsigned char ACCCON; // ACCess CONtrol register
struct CMOSType {
	unsigned char Enabled;
	unsigned char ChipSelect;
	unsigned char Address;
    unsigned char StrobedData;
	unsigned char DataStrobe;
	unsigned char Op;
};
extern struct CMOSType CMOS;
extern unsigned char Sh_Display,Sh_CPUX,Sh_CPUE,PRAM,FRAM;
extern char RomPath[512];
extern char RomFile[512];
/* End of Master 128 Specific Stuff, note initilised anyway regardless of Model Type in use */
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
void BeebReadRoms(void);
void BeebMemInit(unsigned char LoadRoms,unsigned char SkipIntegraBConfig);

/* Used to show the Rom titles from the options menu */
char *ReadRomTitle( int bank, char *Title, int BufSize );

void beebmem_dumpstate(void);
void SaveMemUEF(FILE *SUEF);
extern int EFDCAddr; // 1770 FDC location
extern int EDCAddr; // Drive control location
extern bool NativeFDC; // see beebmem.cpp for description
void LoadRomRegsUEF(FILE *SUEF);
void LoadMainMemUEF(FILE *SUEF);
void LoadShadMemUEF(FILE *SUEF);
void LoadPrivMemUEF(FILE *SUEF);
void LoadFileMemUEF(FILE *SUEF);
void LoadSWRMMemUEF(FILE *SUEF);
void LoadIntegraBHiddenMemUEF(FILE *SUEF);
#endif
