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

int WritableRoms = 0;

/* Each Rom now has a Ram/Rom flag */
int RomWritable[16] = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};

int PagedRomReg;

static int RomModified=0; /* Rom changed - needs copying back */
static int SWRamModified=0; /* SW Ram changed - needs saving and restoring */
unsigned char WholeRam[65536];
/* Master 128 Specific Stuff */
unsigned char FSRam[12228]; // 12K Filing System RAM
unsigned char PrivateRAM[4096]; // 4K Private RAM (VDU Use mainly)
int CMOSRAM[64]; // = {0,0,0,0,1,0,1,1,1,153,0,132,0,0,104,0,0,0,0,12,255,0,0,7,0,30,5,0,0,0,0}; // 50 Bytes CMOS RAM
int CMOSDefault[64]={0,0,0,0,0,0xc9,0xff,0xfe,0x32,0,7,0xc1,0x1e,5,0,0x58,0xa2}; // Backup of CMOS Defaults
unsigned char ShadowRAM[32768]; // 20K Shadow RAM
unsigned char MOSROM[12228]; // 12K MOS Store for swapping FS ram in and out
unsigned char ACCCON; // ACCess CONtrol register
unsigned char UseShadow; // 1 to use shadow ram, 0 to use main ram
unsigned char MainRAM[32768]; // Main RAM temporary store when using shadow RAM
// ShadowRAM and MainRAM have to be 32K for reasons to do with addressing
struct CMOSType CMOS;
/* End of Master 128 Specific Stuff, note initilised anyway regardless of Model Type in use */
char RomPath[256];
// static unsigned char Roms[16][16384];

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
  if (MachineType==1) {
	  // Master Shadow RAM determination. not that simple ;)
	  // Bit D (0) of ACCON determines ram to use
	  // if D is 1 and UseShadow=1 then read WholeRam ; if D is 0 and UseShadow=1 then read MainRAM
	  // if D is 1 and UseShadow=0 then read ShadowRAM ; if D is 0 and UseShadow=0 then read WholeRAM
	  if ((ACCCON & 1) && UseShadow==0) NeedShadow=1;
	  if (!(ACCCON & 1) && UseShadow==1) NeedShadow=2;
  }

  a=WrapAddr(a);
  EndAddr=WrapAddr(EndAddr);

  if (a<=EndAddr && NeedShadow==0) {
    return((char *)WholeRam+a);
  };
  if (a<=EndAddr && NeedShadow==1) {
    return((char *)ShadowRAM+a);
  };
  if (a<=EndAddr && NeedShadow==2) {
    return((char *)MainRAM+a);
  };

  toCopy=0x8000-a;
  if (toCopy>n) toCopy=n;
  if (toCopy>0 && NeedShadow==0) memcpy(tmpBuf,WholeRam+a,toCopy);
  if (toCopy>0 && NeedShadow==1) memcpy(tmpBuf,ShadowRAM+a,toCopy);
  if (toCopy>0 && NeedShadow==2) memcpy(tmpBuf,MainRAM+a,toCopy);
  tmpBufPtr=tmpBuf+toCopy;
  toCopy=n-toCopy;
  if (toCopy>0 && NeedShadow==0) memcpy(tmpBufPtr,WholeRam+EndAddr-(toCopy-1),toCopy); /* Should that -1 be there ? */
  if (toCopy>0 && NeedShadow==1) memcpy(tmpBufPtr,ShadowRAM+EndAddr-(toCopy-1),toCopy); /* Should that -1 be there ? */
  if (toCopy>0 && NeedShadow==2) memcpy(tmpBufPtr,MainRAM+EndAddr-(toCopy-1),toCopy); /* Should that -1 be there ? */
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
  unsigned char NeedShadow=0; // 0 to read WholeRam, 1 to read ShadowRAM ; 2 to read MainRAM
  static char tmpBuf[1024];
  char *tmpBufPtr;
  int EndAddr=a+n-1;
  int toCopy;

  if (MachineType==1) {
	  // Master Shadow RAM determination. not that simple ;)
	  // Bit D (0) of ACCON determines ram to use
	  // if D is 1 and UseShadow=1 then read WholeRam ; if D is 0 and UseShadow=1 then read MainRAM
	  // if D is 1 and UseShadow=0 then read ShadowRAM ; if D is 0 and UseShadow=0 then read WholeRAM
	  if ((ACCCON & 1) && UseShadow==0) NeedShadow=1;
	  if (!(ACCCON & 1) && UseShadow==1) NeedShadow=2;
  }
  a=WrapAddrMo7(a);
  EndAddr=WrapAddrMo7(EndAddr);

  if (a<=EndAddr && NeedShadow==0) {
    return((char *)WholeRam+a);
  };
  if (a<=EndAddr && NeedShadow==1) {
    return((char *)ShadowRAM+a);
  };
  if (a<=EndAddr && NeedShadow==2) {
    return((char *)MainRAM+a);
  };

  toCopy=0x8000-a;
  if (toCopy>n && NeedShadow==0) return((char *)WholeRam+a);
  if (toCopy>n && NeedShadow==1) return((char *)ShadowRAM+a);
  if (toCopy>n && NeedShadow==2) return((char *)MainRAM+a);
  if (toCopy>0 && NeedShadow==0) memcpy(tmpBuf,WholeRam+a,toCopy);
  if (toCopy>0 && NeedShadow==1) memcpy(tmpBuf,ShadowRAM+a,toCopy);
  if (toCopy>0 && NeedShadow==2) memcpy(tmpBuf,MainRAM+a,toCopy);
  tmpBufPtr=tmpBuf+toCopy;
  toCopy=n-toCopy;
  if (toCopy>0 && NeedShadow==0) memcpy(tmpBufPtr,WholeRam+EndAddr-(toCopy-1),toCopy); /* Should that -1 be there ? */
  if (toCopy>0 && NeedShadow==1) memcpy(tmpBufPtr,ShadowRAM+EndAddr-(toCopy-1),toCopy); /* Should that -1 be there ? */
  if (toCopy>0 && NeedShadow==2) memcpy(tmpBufPtr,MainRAM+EndAddr-(toCopy-1),toCopy); /* Should that -1 be there ? */
  return(tmpBuf);
}; /* BeebMemPtrWithWrapMo7 */

/*----------------------------------------------------------------------------*/
int BeebReadMem(int Address) {
  static int extracycleprompt=0;

 /* We now presume that the caller has checked to see if the address is below fc00
    and if so does a direct read */ 
 /* if (Address<0xfc00) return(WholeRam[Address]); */
  if (Address>=0xff00) return(WholeRam[Address]);
  Cycles++;
  extracycleprompt++;
  if (extracycleprompt & 8) Cycles++;
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
  if ((Address & ~0x1f)==0xfe80 && MachineType==0) return(Disc8271_read(Address & 0x7));
  if ((Address & ~7)==0xfe28 && MachineType==1) return(Read1770Register(Address & 0x7));
  if (Address==0xfe24 && MachineType==1) return(ReadFDCControlReg());
  if ((Address & ~0x1f)==0xfea0) return(0xfe); /* Disable econet */
  if ((Address & ~0x1f)==0xfec0 && MachineType==0) return(AtoDRead(Address & 0xf));
  if (Address>=0xfe18 && Address<=0xfe20 && MachineType==1) return(AtoDRead(Address - 0xfe18));
  if ((Address & ~0x1f)==0xfee0) return(0xfe+MachineType); /* Disable tube */
  // Tube seems to return FF on a master (?)
  return(0);
} /* BeebReadMem */

/*----------------------------------------------------------------------------*/
static void DoRomChange(int NewBank) {
  /* Speed up hack - if we are switching to the same rom, then don't bother */
  if (MachineType==0) NewBank&=0xf; // strip top bit if Model B
  
  if (NewBank==PagedRomReg) return;
  // Master Specific stuff   
  if (MachineType==0) {
    if (RomModified) memcpy(Roms[PagedRomReg],WholeRam+0x8000,0x4000);
    RomModified=0;
    PagedRomReg=NewBank;
    memcpy(WholeRam+0x8000,Roms[PagedRomReg],0x4000);
  };

  if (MachineType==1) {
	// rewrote this section as well
	  // copy out the private ram if its switched in
	  if ((PagedRomReg & 128)>>7) memcpy(PrivateRAM,WholeRam+0x8000,0x1000);
	  // copy out the sideway ram if thats switched in
      if ((RomModified) && (!(PagedRomReg & 128))) memcpy(Roms[PagedRomReg],WholeRam+0x8000,0x4000);
	  // Switch banks
	  memcpy(WholeRam+0x8000,Roms[NewBank & 0xf],0x4000);
	  // switch ram back in if need be
	  if ((NewBank & 128)>>7)  memcpy(WholeRam+0x8000,PrivateRAM,0x1000);
	  PagedRomReg=NewBank;
  }

}; /* DoRomChange */
/*----------------------------------------------------------------------------*/
static void FiddleACCCON(unsigned char newValue) {
	// Master specific, should only execute in Master128 mode
	// ignore bits TST (6) IFJ (5) and ITU (4)
	newValue&=143;
	// Detect change in bit Y (3)
	if ((ACCCON & 8)!=(newValue & 8)) {
		if ((newValue & 8)==8) {
			// switch ram into mos, mos into store
			// memcpy(MOSROM,WholeRam+0xc000,0x2000); // Why am i wasting time copying
			// the MOS ROM out again? - Richard Gellman
			memcpy(WholeRam+0xc000,FSRam,0x2000);
		}
		else {
			// ram out, mos in
			memcpy(FSRam,WholeRam+0xc000,0x2000);
			memcpy(WholeRam+0xc000,MOSROM,0x2000);
		}
	}
    // Detect IRR Bit (7)
	// Shadow RAM is handled differntly, see the beebreadmem functions.
	if ((newValue & 128)==128) DoInterrupt();
	ACCCON=newValue & 127; // mask out the IRR bit so that interrupts dont occur repeatedly
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
	  if (MachineType==1) {
		  if (((Address<0x9000) && (PagedRomReg & 128)) || (RomWritable[PagedRomReg])) {
			  WholeRam[Address]=Value;
			  if (!(PagedRomReg & 128)) RomModified=1;
			  if (RomWritable[PagedRomReg]) SWRamModified=1;
		  }
	  }
    return;
  }

  if (Address<0xe000 && (ACCCON & 8)==8 && MachineType==1 && Address>=0xc000) WholeRam[Address]=Value;

  Cycles++;
  extracycleprompt++;
  if (extracycleprompt & 8) Cycles++;

  if ((Address>=0xfc00) && (Address<=0xfeff)) {
    /* Check for some hardware */
    if ((Address & ~0x3)==0xfe20) {
      VideoULAWrite(Address & 0xf, Value);
      return;
    }

    /* Can write to a via using either of the two 16 bytes blocks */
    if ((Address & ~0xf)==0xfe40 || (Address & ~0xf)==0xfe50) {
      SysVIAWrite((Address & 0xf),Value);
      return;
    }

    /* Can write to a via using either of the two 16 bytes blocks */
    if ((Address & ~0xf)==0xfe60 || (Address & ~0xf)==0xfe70) {
      UserVIAWrite((Address & 0xf),Value);
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

    /* Should get the serial system, now that the CRTC has been extracted */
//    if ((Address & ~0x1f)==0xfe00) {
//      cerr << "Write *0x" << hex << Address << "=0x" << Value << dec << "\n";
//    };

    if ((Address & ~0x1f)==0xfe80 && MachineType==0) {
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

    if ((Address & ~0x1f)==0xfec0) {
      AtoDWrite((Address & 0xf),Value);
      return;
    }

   // if (Address==0xfc01) exit(0); <-- ok what the hell is this?
   // return;
  }
}

/*----------------------------------------------------------------------------*/
static void ReadRom(char *name,int bank) {
	// Ok, im tired, its late (8pm, er late?) so this is the deal 
	// in Master mode, the files TERMINAL,ROM, DFS,ROM MOS,ROM and BASIC4.ROM are NEEDED
	// in Model B mode, the files OS12, BASIC.ROM and DNFS are NEEDED
	// but we check them later
	// in this proc, it won't return an error message if the rom is not present.
  FILE *InFile;
  char fullname[256];
  long romsize;
  strcpy(fullname,RomPath);
  strcat(fullname,"/beebfile/");
  strcat(fullname,name);
/*  if (InFile=fopen(fullname,"rb"),InFile==NULL) {
#ifdef WIN32
    char errstr[200];
	sprintf(errstr, "Cannot open ROM image file:\n  %s", fullname);
    MessageBox(GETHWND,errstr,"BBC Emulator",MB_OK|MB_ICONERROR);
	exit(1);
#else
    fprintf(stderr,"Could not open %s rom file\n",fullname);
    abort();
#endif
  } */
if ((InFile=fopen(fullname,"rb")) !=NULL) {
  /* Check ROM size */
  fseek(InFile, 0L, SEEK_END);
  romsize = ftell(InFile);
  if (romsize!=16384 && romsize!=8192) {
#ifdef WIN32
    char errstr[200];
	sprintf(errstr, "ROM image is wrong size:\n  %s", name);
    MessageBox(GETHWND,errstr,"BBC Emulator",MB_OK|MB_ICONERROR);
	exit(1);
#else
    fprintf(stderr,"Could not read %s\n",name);
    abort();
#endif
  }
  fseek(InFile, 0L, SEEK_SET);
  fread(Roms[bank],1,romsize,InFile);

  fclose(InFile);

  /* Write Protect the ROMS read in at startup */
  RomWritable[bank] = 0;
}

} /* ReadRom */

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
 
 /* Read all ROM files in the beebfile directory */
 // This section rewritten for V.1.32 to take account of roms.cfg file.
 strcpy(TmpPath,RomPath);
 strcat(TmpPath,"Roms.cfg");
 RomCfg=fopen(TmpPath,"rt");
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
		isrom=1;
		if (strncmp(RomName,"EMPTY",5)==0)  { RomWritable[romslot]=0; isrom=0; }
		if (strncmp(RomName,"RAM",3)==0) { RomWritable[romslot]=1; isrom=0; }
		for (sc = 0; fullname[sc]; sc++) if (fullname[sc] == '\\') fullname[sc] = '/';
		fullname[strlen(fullname)-1]=0;
		InFile=fopen(fullname,"rb");
		if	(InFile!=NULL) { fread(Roms[romslot],1,16384,InFile); fclose(InFile); RomWritable[romslot]=0; }
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
 memcpy(MOSROM,WholeRam+0xc00,16384);
}
/*----------------------------------------------------------------------------*/
void BeebMemInit(void) {
  /* Remove the non-win32 stuff here, soz, im not gonna do multi-platform master128 upgrades
  u want for linux? u do yourself! ;P - Richard Gellman */
  
  char TmpPath[256];
  unsigned char RomBlankingSlot;
  long CMOSLength;
  FILE *CMDF3;
  unsigned char CMA3;
  for (CMA3=0;CMA3<16;CMA3++) RomWritable[CMA3]=1;
  for (RomBlankingSlot=0xf;RomBlankingSlot<0x10;RomBlankingSlot--) memset(Roms[RomBlankingSlot],0,0x4000);
  
  BeebReadRoms();

  /* Put first ROM in */
  memcpy(WholeRam+0x8000,Roms[0xf],0x4000);
  PagedRomReg=0xf;
  RomModified=0;
  /* Copy part of MOS to MOS Store for Master 128 */
  memcpy(MOSROM,WholeRam+0xC000,0x2000);
  ACCCON=0; UseShadow=0; // Select all main memory
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
