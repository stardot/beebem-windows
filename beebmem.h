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

extern int WritableRoms;  /* Is writing to a ROM allowed */

extern int RomWritable[16]; /* Allow writing to ROMs on an individual basis */

extern unsigned char WholeRam[65536];
static unsigned char Roms[16][16384];
extern int PagedRomReg;
/* Master 128 Specific Stuff */
extern unsigned char FSRam[12228]; // 12K Filing System RAM (yes ok its only 8, i misread ;P)
extern unsigned char PrivateRAM[4096]; // 4K Private RAM (VDU Use mainly)
extern int CMOSRAM[64]; // 50 Bytes CMOS RAM
extern unsigned char ShadowRAM[32768]; // 20K Shadow RAM
extern unsigned char MOSROM[12228]; // 12K MOS Store for swapping FS ram in and out
extern unsigned char ACCCON; // ACCess CONtrol register
extern unsigned char UseShadow; // 1 to use shadow ram, 0 to use main ram
extern unsigned char MainRAM[32768]; // temporary store for main ram when using shadow ram
struct CMOSType {
	unsigned char Enabled;
	unsigned char ChipSelect;
	unsigned char Address;
    unsigned char StrobedData;
	unsigned char DataStrobe;
	unsigned char Op;
};
extern struct CMOSType CMOS;
extern char RomPath[256];
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
void BeebMemInit(void);

void SaveMemState(unsigned char *RomState,unsigned char *MemState,unsigned char *SWRamState);
void RestoreMemState(unsigned char *RomState,unsigned char *MemState,unsigned char *SWRamState);

/* Used to show the Rom titles from the options menu */
char *ReadRomTitle( int bank, char *Title, int BufSize );

void beebmem_dumpstate(void);
#endif
