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
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include "iostream.h"

#include "6502core.h"
#include "disc8271.h"
#include "main.h"
#include "sysvia.h"
#include "uservia.h"
#include "video.h"
#include "atodconv.h"
#include "beebmem.h"
#include "disc1770.h"
#include "serial.h"
#include "tube.h"
#include "errno.h"
#include "uefstate.h"

//FILE *fdclog2;

int WritableRoms = 0;

/* Each Rom now has a Ram/Rom flag */
int RomWritable[16] = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};

int PagedRomReg;

static int RomModified=0; /* Rom changed - needs copying back */
static int SWRamModified=0; /* SW Ram changed - needs saving and restoring */
unsigned char WholeRam[65536];
unsigned char Roms[16][16384];
unsigned char ROMSEL;
/* Master 128 Specific Stuff */
unsigned char FSRam[12228]; // 12K Filing System RAM
unsigned char PrivateRAM[4096]; // 4K Private RAM (VDU Use mainly)
int CMOSRAM[64]; // 50 Bytes CMOS RAM
int CMOSDefault[64]={0,0,0,0,0,0xc9,0xff,0xfe,0x32,0,7,0xc1,0x1e,5,0,0x58,0xa2}; // Backup of CMOS Defaults
unsigned char ShadowRAM[32768]; // 20K Shadow RAM
unsigned char MOSROM[16384]; // 12K MOS Store for swapping FS ram in and out
unsigned char ACCCON; // ACCess CONtrol register
unsigned char UseShadow; // 1 to use shadow ram, 0 to use main ram
unsigned char MainRAM[32768]; // Main RAM temporary store when using shadow RAM
// ShadowRAM and MainRAM have to be 32K for reasons to do with addressing
struct CMOSType CMOS;
unsigned char Sh_Display,Sh_CPUX,Sh_CPUE,PRAM,FRAM;
/* End of Master 128 Specific Stuff, note initilised anyway regardless of Model Type in use */
char RomPath[512];
// FDD Extension board variables
int EFDCAddr; // 1770 FDC location
int EDCAddr; // Drive control location
bool NativeFDC; // TRUE for 8271, FALSE for DLL extension

/*----------------------------------------------------------------------------*/
/* Perform hardware address wrap around */
static unsigned int WrapAddr(int in) {
  unsigned int offsets[]={0x4000,0x6000,0x3000,0x5800};
  if (in<0x8000) return(in);
  in+=offsets[(IC32State & 0x30)>>4];
  in&=0x7fff;
  return(in);
}; /* WrapAddr */

/*----------------------------------------------------------------------------*/
/* This is for the use of the video routines.  It returns a pointer to
   a continuous area of 'n' bytes containing the contents of the
   'n' bytes of beeb memory starting at address 'a', with wrap around
   at 0x8000.  Potentially this routine may return a pointer into  a static
   buffer - so use the contents before recalling it
   'n' must be less than 1K in length.
   See 'BeebMemPtrWithWrapMo7' for use in Mode 7 - its a special case.
*/

char *BeebMemPtrWithWrap(int a, int n) {
  unsigned char NeedShadow=0; // 0 to read WholeRam, 1 to read ShadowRAM ; 2 to read MainRAM
  static char tmpBuf[1024];
  char *tmpBufPtr;
  int EndAddr=a+n-1;
  int toCopy;

  a=WrapAddr(a);
  EndAddr=WrapAddr(EndAddr);

  if (a<=EndAddr && Sh_Display==0) {
    return((char *)WholeRam+a);
  };
  if (a<=EndAddr && Sh_Display>0) {
    return((char *)ShadowRAM+a);
  };

  toCopy=0x8000-a;
  if (toCopy>n) toCopy=n;
  if (toCopy>0 && Sh_Display==0) memcpy(tmpBuf,WholeRam+a,toCopy);
  if (toCopy>0 && Sh_Display>0) memcpy(tmpBuf,ShadowRAM+a,toCopy);
  tmpBufPtr=tmpBuf+toCopy;
  toCopy=n-toCopy;
  if (toCopy>0 && Sh_Display==0) memcpy(tmpBufPtr,WholeRam+EndAddr-(toCopy-1),toCopy); /* Should that -1 be there ? */
  if (toCopy>0 && Sh_Display>0) memcpy(tmpBufPtr,ShadowRAM+EndAddr-(toCopy-1),toCopy); /* Should that -1 be there ? */
  // Tripling is for Shadow RAM handling
  return(tmpBuf);
}; /* BeebMemPtrWithWrap */

/*----------------------------------------------------------------------------*/
/* Perform hardware address wrap around - for mode 7*/
static unsigned int WrapAddrMo7(int in) {
  if (in<0x8000) return(in);
  in+=0x7c00;
  in&=0x7fff;
  return(in);
}; /* WrapAddrMo7 */

/*----------------------------------------------------------------------------*/
/* Special case of BeebMemPtrWithWrap for use in mode 7
*/

char *BeebMemPtrWithWrapMo7(int a, int n) {
  static char tmpBuf[1024];
  char *tmpBufPtr;
  int EndAddr=a+n-1;
  int toCopy;

  a=WrapAddrMo7(a);
  EndAddr=WrapAddrMo7(EndAddr);

  if (a<=EndAddr && Sh_Display==0) {
    return((char *)WholeRam+a);
  };
  if (a<=EndAddr && Sh_Display>0) {
    return((char *)ShadowRAM+a);
  };

  toCopy=0x8000-a;
  if (toCopy>n && Sh_Display==0) return((char *)WholeRam+a);
  if (toCopy>n && Sh_Display>0) return((char *)ShadowRAM+a);
  if (toCopy>0 && Sh_Display==0) memcpy(tmpBuf,WholeRam+a,toCopy);
  if (toCopy>0 && Sh_Display>0) memcpy(tmpBuf,ShadowRAM+a,toCopy);
  tmpBufPtr=tmpBuf+toCopy;
  toCopy=n-toCopy;
  if (toCopy>0 && Sh_Display==0) memcpy(tmpBufPtr,WholeRam+EndAddr-(toCopy-1),toCopy); /* Should that -1 be there ? */
  if (toCopy>0 && Sh_Display>0) memcpy(tmpBufPtr,ShadowRAM+EndAddr-(toCopy-1),toCopy); /* Should that -1 be there ? */
  return(tmpBuf);
}; /* BeebMemPtrWithWrapMo7 */

/*----------------------------------------------------------------------------*/
int BeebReadMem(int Address) {
  static int extracycleprompt=0;

 /* We now presume that the caller has checked to see if the address is below fc00
    and if so does a direct read */ 
  if (Address>=0xff00) return(WholeRam[Address]);
  /* OK - its IO space - lets check some system devices */
  /* VIA's first - games seem to do really heavy reaing of these */
  /* Can read from a via using either of the two 16 bytes blocks */
  if ((Address & ~0xf)==0xfe40 || (Address & ~0xf)==0xfe50) return(SysVIARead(Address & 0xf));
  if ((Address & ~0xf)==0xfe60 || (Address & ~0xf)==0xfe70) return(UserVIARead(Address & 0xf));
  if ((Address & ~7)==0xfe00) return(CRTCRead(Address & 0x7));
  if ((Address & ~3)==0xfe20) return(VideoULARead(Address & 0xf)); // Master uses fe24 to fe2f for FDC
  if ((Address & ~3)==0xfe30) return(PagedRomReg); /* report back ROMSEL - I'm sure the beeb allows ROMSEL read..
													correct me if im wrong. - Richard Gellman */
  if ((Address & ~3)==0xfe34 && MachineType==1) return(ACCCON);
  // In the Master at least, ROMSEL/ACCCON seem to be duplicated over a 4 byte block.
  if (((Address & ~0x1f)==0xfe80) && (MachineType==0) && (NativeFDC)) return(Disc8271_read(Address & 0x7));
  if ((MachineType==0) && (Address>=EFDCAddr) && (Address<(EFDCAddr+4)) && (!NativeFDC)) {
	  //MessageBox(GETHWND,"Read of 1770 Extension Board\n","BeebEm",MB_OK|MB_ICONERROR);
	  return(Read1770Register(Address-EFDCAddr));
  }
  if ((MachineType==0) && (Address==EDCAddr) && (!NativeFDC)) return(mainWin->GetDriveControl());
  if ((Address & ~7)==0xfe28 && MachineType==1) return(Read1770Register(Address & 0x7));
  if (Address==0xfe24 && MachineType==1) return(ReadFDCControlReg());
  if ((Address & ~0x1f)==0xfea0) return(0xfe); /* Disable econet */
  if ((Address & ~0x1f)==0xfec0 && MachineType==0) return(AtoDRead(Address & 0xf));
  if (Address>=0xfe18 && Address<=0xfe20 && MachineType==1) return(AtoDRead(Address - 0xfe18));
  if ((Address & ~0x1f)==0xfee0) return(ReadTubeFromHostSide(Address&7)); //Read From Tube
  // Tube seems to return FF on a master (?)
  if (Address==0xfe08) return(Read_ACIA_Status());
  if (Address==0xfe09) return(Read_ACIA_Rx_Data());
  if (Address==0xfe10) return(Read_SERPROC());
  if (Address==0xfc01) {
	  char infstr[200];
	  sprintf(infstr,"%0X %0X\n",EFDCAddr,EDCAddr);
	  MessageBox(GETHWND,infstr,"BeebEm",MB_OK|MB_ICONERROR); 
  }
  return(0);
} /* BeebReadMem */

/*----------------------------------------------------------------------------*/
static void DoRomChange(int NewBank) {
  /* Speed up hack - if we are switching to the same rom, then don't bother */
  if (MachineType==0) NewBank&=0xf; // strip top bit if Model B
  ROMSEL=NewBank&0xf;
  
  if (NewBank==PagedRomReg) return;
  // Master Specific stuff   
  if (MachineType==0) {
    if (RomModified) memcpy(Roms[PagedRomReg],WholeRam+0x8000,0x4000);
    RomModified=0;
    PagedRomReg=NewBank;
    memcpy(WholeRam+0x8000,Roms[PagedRomReg],0x4000);
  };

  if (MachineType==1) {
	  PagedRomReg=NewBank;
	  PRAM=(PagedRomReg & 128);
  }

}; /* DoRomChange */
/*----------------------------------------------------------------------------*/
static void FiddleACCCON(unsigned char newValue) {
	// Master specific, should only execute in Master128 mode
	// ignore bits TST (6) IFJ (5) and ITU (4)
	newValue&=143;
	unsigned char oldshd;
	if ((newValue & 128)==128) DoInterrupt();
	ACCCON=newValue & 127; // mask out the IRR bit so that interrupts dont occur repeatedly
	//if (newValue & 128) intStatus|=128; else intStatus&=127;
	oldshd=Sh_Display;
	Sh_Display=ACCCON & 1;
	if (Sh_Display!=oldshd) RedoMPTR();
	Sh_CPUX=ACCCON & 4;
	Sh_CPUE=ACCCON & 2;
	FRAM=ACCCON & 8;
}
/*----------------------------------------------------------------------------*/
void BeebWriteMem(int Address, int Value) {
  static int extracycleprompt=0;
/*  fprintf(stderr,"Write %x to 0x%x\n",Value,Address); */

  /* Now we presume that the caller has validated the address as beingwithin
  main ram and hence the following line is not required */
  if (Address<0x8000) {
    WholeRam[Address]=Value;
    return;
  }

  /* heh, this is the fun part, with ram and ACCCON */
  // Rewritten for V.1.32 - Richard Gellman - 11/03/2001 - 2:58pm
  if (Address<0xc000) {
	  // First the BBC B
	  if ((MachineType==0) && (RomWritable[PagedRomReg])) {
		  WholeRam[Address]=Value;
		  RomModified=1;
		  if (RomWritable[PagedRomReg]) SWRamModified=1;
	  }
	  // Now the Master 128
	  // Master 128 memory handling now done in 6502core.cpp, V.1.4 - Richard Gellman
    return;
  }

/*Cycles++; <--- What on earth is all this for?
  extracycleprompt++;
  if (extracycleprompt & 8) Cycles++;*/

  if ((Address>=0xfc00) && (Address<=0xfeff)) {
    /* Check for some hardware */
    if ((Address & ~0x3)==0xfe20) {
      VideoULAWrite(Address & 0xf, Value);
      return;
    }

    /* Can write to a via using either of the two 16 bytes blocks */
    if ((Address & ~0xf)==0xfe40 || (Address & ~0xf)==0xfe50) {
      SysVIAWrite((Address & 0xf),Value);
	  Cycles++;
      return;
    }

    /* Can write to a via using either of the two 16 bytes blocks */
    if ((Address & ~0xf)==0xfe60 || (Address & ~0xf)==0xfe70) {
      UserVIAWrite((Address & 0xf),Value);
	  Cycles++;
      return;
    }

    if (Address>=0xfe30 && Address<0xfe34) {
      DoRomChange(Value);
      return;
    }

	if (Address>=0xfe34 && Address<0xfe38 && MachineType==1) {
		FiddleACCCON(Value);
		return;
	}
	// In the Master at least, ROMSEL/ACCCON seem to be duplicated over a 4 byte block.
    /*cerr << "Write *0x" << hex << Address << "=0x" << Value << dec << "\n"; */
    if ((Address & ~0x7)==0xfe00) {
      CRTCWrite(Address & 0x7, Value);
      return;
    }

    if (((Address & ~0x1f)==0xfe80) && (MachineType==0) && (NativeFDC)) {
      Disc8271_write((Address & 7),Value);
      return;
    }

	if ((Address & ~0x7)==0xfe28 && MachineType==1) {
		Write1770Register(Address & 7,Value);
		return;
	} 

	if (Address==0xfe24 && MachineType==1) {
		WriteFDCControlReg(Value);
		return;
	}

    if ((Address & ~0x1f)==0xfec0 && MachineType==0) {
      AtoDWrite((Address & 0xf),Value);
      return;
    }

    if ((Address & ~0x7)==0xfe18 && MachineType==1) {
      AtoDWrite((Address & 0xf),Value);
      return;
    }

	if (Address==0xfe08) Write_ACIA_Control(Value);
	if (Address==0xfe09) Write_ACIA_Tx_Data(Value);
	if (Address==0xfe10) Write_SERPROC(Value);
	if ((Address&~0x7)==0xfee0) WriteTubeFromHostSide(Address&7,Value);

	if ((MachineType==0) && (Address==EDCAddr) && (!NativeFDC)) {
		//fprintf(fdclog2,"FDC CONTROL write of %02X\n",Value);
		mainWin->SetDriveControl(Value);
	}
	if ((MachineType==0) && (Address>=EFDCAddr) && (Address<(EFDCAddr+4)) && (!NativeFDC)) {
		//MessageBox(GETHWND,"Write to 1770 Extension Board\n","BeebEm",MB_OK|MB_ICONERROR);
		Write1770Register(Address-EFDCAddr,Value);
	}

   // if (Address==0xfc01) exit(0); <-- ok what the hell is this?
    return;
  }
}

/*----------------------------------------------------------------------------*/
// ReadRom was replaced with BeebReadRoms.
/*----------------------------------------------------------------------------*/
char *ReadRomTitle( int bank, char *Title, int BufSize )
{
	int i;

	// Copy the ROM Title to the Buffer
	for( i=0; (( i<(BufSize-1)) && ( Roms[bank][i+9]>30)); i++ )
		Title[i] = Roms[bank][i+9];

	// Add terminating NULL.
	Title[i] = '\0';

	return Title;
}
/*----------------------------------------------------------------------------*/
void BeebReadRoms(void) {
 FILE *InFile,*RomCfg;
 char TmpPath[256];
 char fullname[256];
 int romslot = 0xf;
 char RomNameBuf[80];
 char *RomName=RomNameBuf;
 unsigned char sc,isrom;
 unsigned char Shortener=1; // Amount to shorten filename by
 
 /* Read all ROM files in the beebfile directory */
 // This section rewritten for V.1.32 to take account of roms.cfg file.
 strcpy(TmpPath,RomPath);
 strcat(TmpPath,"Roms.cfg");
 RomCfg=fopen(TmpPath,"rt");
 if (RomCfg==NULL) {
	 // Open failed, if its because of an unfound file,
	 // try to copy example.cfg on to it, and re-open
//	 if (errno==ENOENT) {
		 strcpy(TmpPath,RomPath);
		 strcat(TmpPath,"Example.cfg");
		 InFile=fopen(TmpPath,"rt");
		 if (InFile!=NULL) {
			 strcpy(TmpPath,RomPath);
			 strcat(TmpPath,"Roms.cfg");
			 RomCfg=fopen(TmpPath,"wt");
			 //Begin copying the file over
			 for (romslot=0;romslot<34;romslot++) {
				 fgets(RomName,80,InFile);
				 fputs(RomName,RomCfg);
			 }
			 fclose(RomCfg);
			 fclose(InFile);
		 }
//	 }
	 strcpy(TmpPath,RomPath);
	 strcat(TmpPath,"Roms.cfg");
	 RomCfg=fopen(TmpPath,"rt"); // Retry the opem
 }

 if (RomCfg!=NULL) {
	 // CFG file open, proceed to read the roms.
	 // if machinetype=1 (i.e. master 128) we need to skip 17 lines in the file
	 if (MachineType==1) for (romslot=0;romslot<17;romslot++) fgets(RomName,80,RomCfg);
	 // read OS ROM
	 fgets(RomName,80,RomCfg);
	 strcpy(fullname,RomName);
	 if ((RomName[0]!='\\') && (RomName[1]!=':')) {
		 strcpy(fullname,RomPath);
		 strcat(fullname,"/beebfile/");
		 strcat(fullname,RomName);
	 }
	 // for some reason, we have to change \ to /  to make C work...
	 for (sc = 0; fullname[sc]; sc++) if (fullname[sc] == '\\') fullname[sc] = '/';
	 fullname[strlen(fullname)-1]=0;
	 InFile=fopen(fullname,"rb");
	 if (InFile!=NULL) { fread(WholeRam+0xc000,1,16384,InFile); fclose(InFile); }
  	 else {
		 char errstr[200];
		 sprintf(errstr, "Cannot open specified OS ROM:\n %s",fullname);
		 MessageBox(GETHWND,errstr,"BBC Emulator",MB_OK|MB_ICONERROR);
	 }
	 // read paged ROMs
	 for (romslot=15;romslot>=0;romslot--) {
		fgets(RomName,80,RomCfg);
		strcpy(fullname,RomName);
		if ((RomName[0]!='\\') && (RomName[1]!=':')) {
 			strcpy(fullname,RomPath);
			strcat(fullname,"/beebfile/");
			strcat(fullname,RomName);
		}
		isrom=1; RomWritable[romslot]=0; Shortener=1;
		if (strncmp(RomName,"EMPTY",5)==0)  { RomWritable[romslot]=0; isrom=0; }
		if (strncmp(RomName,"RAM",3)==0) { RomWritable[romslot]=1; isrom=0; }
		if (strncmp(RomName+(strlen(RomName)-5),":RAM",4)==0) {
			// Writable ROM (don't ask, Mark De Weger should be happy now ;) Hi Mark! )
			RomWritable[romslot]=1; // Make it writable
			isrom=1; // Make it a ROM still
			Shortener=5; // Shorten filename
		}
		for (sc = 0; fullname[sc]; sc++) if (fullname[sc] == '\\') fullname[sc] = '/';
		fullname[strlen(fullname)-Shortener]=0;
		InFile=fopen(fullname,"rb");
		if	(InFile!=NULL) { fread(Roms[romslot],1,16384,InFile); fclose(InFile); }
		else {
			if (isrom==1) {
				char errstr[200];
				sprintf(errstr, "Cannot open specified ROM:\n %s",fullname);
				MessageBox(GETHWND,errstr,"BBC Emulator",MB_OK|MB_ICONERROR);
			}
		}
	 }
	 fclose(RomCfg);
 }
 else {
    char errstr[200];
	sprintf(errstr, "Cannot open ROM Configuration file ROMS.CFG");
    MessageBox(GETHWND,errstr,"BBC Emulator",MB_OK|MB_ICONERROR);
	exit(1);
	}
 // Copy MOS to MOS Store
 //memcpy(MOSROM,WholeRam+0xc000,16384);
}
/*----------------------------------------------------------------------------*/
void BeebMemInit(unsigned char LoadRoms) {
  /* Remove the non-win32 stuff here, soz, im not gonna do multi-platform master128 upgrades
  u want for linux? u do yourself! ;P - Richard Gellman */
  
  char TmpPath[256];
  unsigned char RomBlankingSlot;
  long CMOSLength;
  FILE *CMDF3;
  unsigned char CMA3;
  if (LoadRoms) {
	  for (CMA3=0;CMA3<16;CMA3++) RomWritable[CMA3]=1;
	  for (RomBlankingSlot=0xf;RomBlankingSlot<0x10;RomBlankingSlot--) memset(Roms[RomBlankingSlot],0,0x4000);
	  // This shouldn't be required for sideways RAM.
	  BeebReadRoms(); // Only load roms on start
  }

  /* Put first ROM in */
  memcpy(WholeRam+0x8000,Roms[0xf],0x4000);
  PagedRomReg=0xf;
  RomModified=0;
  // Initialise Master stuff
  if (MachineType==1) {
	  ACCCON=0; UseShadow=0; // Select all main memory
//	  memcpy(WholeRam+0xc000,MOSROM,0x2000); // Make sure the old MOS ROM is switched in
  }
  // This CMOS stuff can be done anyway
  // Ah, bug with cmos.ram you say?	
  strcpy(TmpPath,RomPath); strcat(TmpPath,"/beebstate/cmos.ram");
  if ((CMDF3 = fopen(TmpPath,"rb"))!=NULL) {
	  fseek(CMDF3,0,SEEK_END);
	  CMOSLength=ftell(CMDF3);
	  fseek(CMDF3,0,SEEK_SET);
	  if (CMOSLength==50) for(CMA3=0xe;CMA3<64;CMA3++) CMOSRAM[CMA3]=fgetc(CMDF3);
	  fclose(CMDF3);
  }
  else for(CMA3=0xe;CMA3<64;CMA3++) CMOSRAM[CMA3]=CMOSDefault[CMA3-0xe];
  //fdclog2=fopen("/fdcc.log","wb");
} /* BeebMemInit */

/*-------------------------------------------------------------------------*/
void SaveMemState(unsigned char *RomState,
				  unsigned char *MemState,
				  unsigned char *SWRamState)
{
	memcpy(MemState, WholeRam, 32768);

	/* Save SW RAM state if it is selected and it has been modified */
	if (SWRamModified && RomWritable[PagedRomReg])
	{
		RomState[0] = 1;
		memcpy(SWRamState, WholeRam+0x8000, 16384);
	}
}


void SaveMemUEF(FILE *SUEF) {
	unsigned char RAMCount;
	fput16(0x0461,SUEF); // Memory Control State
	fput32(2,SUEF);
	fputc(PagedRomReg,SUEF);
	fputc(ACCCON,SUEF);
	fput16(0x0462,SUEF); // Main Memory
	fput32(32768,SUEF);
	fwrite(WholeRam,1,32768,SUEF);
	if (MachineType==1) {
		fput16(0x0463,SUEF); // Shadow RAM
		fput32(32770,SUEF);
		fput16(0,SUEF);
		fwrite(ShadowRAM,1,32768,SUEF);
		fput16(0x0464,SUEF); // Private RAM
		fput32(4096,SUEF);
		fwrite(PrivateRAM,1,4096,SUEF);
		fput16(0x0465,SUEF); // Filing System RAM
		fput32(8192,SUEF);
		fwrite(FSRam,1,8192,SUEF);
	}
	for (RAMCount=0;RAMCount<16;RAMCount++) {
		if (RomWritable[RAMCount]) {
			fput16(0x0466,SUEF); // ROM bank
			fput32(16385,SUEF);
			fputc(RAMCount,SUEF);
			fwrite(Roms[RAMCount],1,16384,SUEF);
		}
	}
}

void LoadRomRegsUEF(FILE *SUEF) {
	PagedRomReg=fgetc(SUEF);
	ACCCON=fgetc(SUEF);
}

void LoadMainMemUEF(FILE *SUEF) {
	fread(WholeRam,1,32768,SUEF);
}

void LoadShadMemUEF(FILE *SUEF) {
	int SAddr;
	SAddr=fget16(SUEF);
	fread(ShadowRAM+SAddr,1,32768,SUEF);
}

void LoadPrivMemUEF(FILE *SUEF) {
	fread(PrivateRAM,1,4096,SUEF);
}

void LoadFileMemUEF(FILE *SUEF) {
	fread(FSRam,1,8192,SUEF);
}

void LoadSWRMMemUEF(FILE *SUEF) {
	int Rom;
	Rom=fgetc(SUEF);
	RomWritable[Rom]=1;
	fread(Roms[Rom],1,16384,SUEF);
}

/*-------------------------------------------------------------------------*/
void RestoreMemState(unsigned char *RomState,
					 unsigned char *MemState,
					 unsigned char *SWRamState)
{
	memcpy(WholeRam, MemState, 32768);

	/* Restore SW RAM state if it is in use */
	if (RomState[0] == 1)
	{
		RomModified = 1;
		SWRamModified = 1;
		PagedRomReg = 0;      /* Use rom slot 0 */
		RomWritable[0] = 1;
		memcpy(WholeRam+0x8000, SWRamState, 16384);
	}
}

/*-------------------------------------------------------------------------*/
/* dump the contents of mainram into 2 16 K files */
void beebmem_dumpstate(void) {
  FILE *bottom,*top;

  bottom=fopen("memdump_bottom","wb");
  top=fopen("memdump_top","wb");
  if ((bottom==NULL) || (top==NULL)) {
    cerr << "Couldn't open memory dump files\n";
    return;
  };

  fwrite(WholeRam,1,16384,bottom);
  fwrite(WholeRam+16384,1,16384,top);
  fclose(bottom);
  fclose(top);
}; /* beebmem_dumpstate */
