/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 1994  David Alan Gilbert
Copyright (C) 1997  Mike Wyatt
Copyright (C) 2001  Richard Gellman
Copyright (C) 2004  Ken Lowe
Copyright (C) 2004  Rob O'Donnell

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public 
License along with this program; if not, write to the Free 
Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
Boston, MA  02110-1301, USA.
****************************************************************/

// Beebemulator - memory subsystem - David Alan Gilbert 16/10/1994
// Econet emulation: Rob O'Donnell robert@irrelevant.com 28/12/2004
// IDE Interface: JGH jgh@mdfs.net 25/12/2011

#ifdef WIN32
#include <windows.h>
#endif

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <iostream>
#include <fstream>

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
#include "scsi.h"
#include "sasi.h"
#include "ide.h"
#include "uefstate.h"
#include "z80mem.h"
#include "z80.h"
#include "econet.h"		//Rob
#include "debug.h"		//Rob added for INTON/OFF reporting only
#include "teletext.h"

using namespace std;

/* Each Rom now has a Ram/Rom flag */
int RomWritable[16] = {1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1};

/* Identifies what is in each bank */
BankType RomBankType[16] = {
	BankEmpty,BankEmpty,BankEmpty,BankEmpty,BankEmpty,BankEmpty,BankEmpty,BankEmpty,
	BankEmpty,BankEmpty,BankEmpty,BankEmpty,BankEmpty,BankEmpty,BankEmpty,BankEmpty
};

int PagedRomReg;

/* Computech (&B+) Specific Stuff Added by K.Lowe 18/08/03 */
struct tm;
time_t long_time; // Define Clock for Computech Integra-B

int MemSel=0; /* Shadow/Main RAM Toggle */
int PrvEn=0;  /* Private RAM Enable */
int ShEn=0;   /* Shadow RAM Enable */
int Prvs1=0;  /* Private RAM 1K Area */
int Prvs4=0;  /* Private RAM 4K Area */
int Prvs8=0;  /* Private RAM 8K Area */
int HidAdd=0;
/* End of Computech (&B+) Specific Stuff */

unsigned char WholeRam[65536];
unsigned char Roms[16][16384];

/* Computech (&B+) Specific Stuff Added by K.Lowe 18/08/03 */
unsigned char Hidden[256];
unsigned char Private[12288];
unsigned char ShadowRam[20480];
unsigned char HiddenDefault[31] = {0,0,0,0,0,0,2,1,1,0,0xe0,0x8e,0,0,0,0,0,0,0,
						0xef,0xff,0xff,0x78,0,0x17,0x23,0x19,5,0x0a,0x2d,0xa0 }; 
/* End of Computech (&B+) Specific Stuff */

unsigned char ROMSEL;
/* Master 128 Specific Stuff */
unsigned char FSRam[8192];       // 8K Filing System RAM
unsigned char PrivateRAM[4096];  // 4K Private RAM (VDU Use mainly)
unsigned char CMOSRAM[64];       // 50 Bytes CMOS RAM
unsigned char CMOSDefault[64]={0,0,0,0,0,0xc9,0xff,0xfe,0x32,0,7,0xc1,0x1e,5,0,0x59,0xa2}; // Backup of CMOS Defaults
unsigned char ShadowRAM[32768];  // 20K Shadow RAM
unsigned char ACCCON;            // ACCess CONtrol register
struct CMOSType CMOS;
unsigned char Sh_Display,Sh_CPUX,Sh_CPUE,PRAM,FRAM;
/* End of Master 128 Specific Stuff, note initilised anyway regardless of Model Type in use */

/* ROM file data */
char RomPath[_MAX_PATH];
char RomFile[_MAX_PATH];
ROMConfigFile RomConfig;
char DiscPath[_MAX_PATH]; // JGH

// FDD Extension board variables
int EFDCAddr;   // 1770 FDC location
int EDCAddr;    // Drive control location
bool NativeFDC; // TRUE for 8271, FALSE for DLL extension

// Econet NMI enable signals. Decoded from address bus and latched by IC97
#define INTON	TRUE
#define INTOFF	FALSE

/*----------------------------------------------------------------------------*/
/* Perform hardware address wrap around */
static unsigned int WrapAddr(int in) {
  unsigned int offsets[]={0x4000,0x6000,0x3000,0x5800}; // page 419 of AUG is wrong
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
  static char tmpBuf[1024];
  char *tmpBufPtr;
  int EndAddr=a+n-1;
  int toCopy;

  a=WrapAddr(a);
  EndAddr=WrapAddr(EndAddr);

  // On Master the FSRam area is displayed if start addr below shadow area
  if (MachineType==3 && a<=EndAddr && Sh_Display>0 && a<0x3000) {
    if (0x3000-a < n) {
      toCopy=0x3000-a;
      if (toCopy>n) toCopy=n;
      if (toCopy>0) memcpy(tmpBuf,FSRam+0x2000-toCopy,toCopy);
      tmpBufPtr=tmpBuf+toCopy;
      toCopy=n-toCopy;
      if (toCopy>0) memcpy(tmpBufPtr,ShadowRAM+EndAddr-(toCopy-1),toCopy);
      return(tmpBuf);
	}
    else if (a<0x1000) {
      return((char *)FSRam); // Should probably be PrivateRAM?
    }
    else {
      return((char *)FSRam+a-0x1000);
    }
  }

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
  if (toCopy>0 && Sh_Display==0) memcpy(tmpBufPtr,WholeRam+EndAddr-(toCopy-1),toCopy);
  if (toCopy>0 && Sh_Display>0) memcpy(tmpBufPtr,ShadowRAM+EndAddr-(toCopy-1),toCopy);
  // Tripling is for Shadow RAM handling
  return(tmpBuf);
}; // BeebMemPtrWithWrap


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
  if (toCopy>0 && Sh_Display==0) memcpy(tmpBufPtr,WholeRam+EndAddr-(toCopy-1),toCopy);
  if (toCopy>0 && Sh_Display>0) memcpy(tmpBufPtr,ShadowRAM+EndAddr-(toCopy-1),toCopy);
  return(tmpBuf);
}; // BeebMemPtrWithWrapMo7


/*----------------------------------------------------------------------------*/
int BeebReadMem(int Address) {
	int Value = 0xff;

// BBC B Start
  if (MachineType==0) {
	  if (Address>=0x8000 && Address<0xc000) return(Roms[ROMSEL][Address-0x8000]);
	  if (Address<0xfc00) return(WholeRam[Address]);
	  if (Address>=0xff00) return(WholeRam[Address]);
  }
// BBC B End


//BBC B Integra B Start
  if (MachineType==1) { 
	    if (Address<0x3000) return(WholeRam[Address]);
		if ((Address>=0x8000) && (Address<0x8400) && (Prvs8==1) && (PrvEn==1)) return(Private[Address-0x8000]);
		if ((Address>=0x8000) && (Address<0x9000) && (Prvs4==1) && (PrvEn==1)) return(Private[Address-0x8000]);
		if ((Address>=0x9000) && (Address<0xb000) && (Prvs1==1) && (PrvEn==1)) return(Private[Address-0x8000]);
		if ((Address<0x8000) && (ShEn==1) && (MemSel==0)) return(ShadowRam[Address-0x3000]);
		if ((Address<0x8000) && (ShEn==0)) return(WholeRam[Address]);
		if ((Address<0x8000) && (ShEn==1) && (MemSel==1)) return(WholeRam[Address]);
		if (Address>=0x8000 && Address<0xc000) return(Roms[ROMSEL][Address-0x8000]);
		if (Address<0xfc00) return(WholeRam[Address]);
		if (Address>=0xff00) return(WholeRam[Address]);

		if (Address==0xfe3c) {
			time( &long_time );
			if (HidAdd==0) return(localtime(&long_time)->tm_sec);
			if (HidAdd==2) return(localtime(&long_time)->tm_min);
			if (HidAdd==4) return(localtime(&long_time)->tm_hour);
			if (HidAdd==6) return((localtime(&long_time)->tm_wday)+1);
			if (HidAdd==7) return(localtime(&long_time)->tm_mday);
			if (HidAdd==8) return((localtime(&long_time)->tm_mon)+1);
			if (HidAdd==9) return((localtime(&long_time)->tm_year)-10);
			if (HidAdd==0xa) return(0x0);
			return(Hidden[HidAdd]);
		}
	}
//BBC B Integra B End

//BBC B+ Start
  if (MachineType==2) { 
	    if (Address<0x3000) return(WholeRam[Address]);
		if ((Address<0x8000) && (Sh_Display==1) && (PrePC>=0xC000) && (PrePC<0xE000)) return(ShadowRAM[Address]);
		if ((Address<0x8000) && (Sh_Display==1) && (MemSel==1) && (PrePC>=0xA000) && (PrePC<0xB000)) return(ShadowRAM[Address]);
		if (Address<0x8000) return(WholeRam[Address]);
		if ((Address<0xB000) && (MemSel==1)) return(Private[Address-0x8000]);
		if (Address>=0x8000 && Address<0xc000) return(Roms[ROMSEL][Address-0x8000]);
		if (Address<0xfc00) return(WholeRam[Address]);
		if (Address>=0xff00) return(WholeRam[Address]);
  }
//BBC B+ End


// Master 128 Start
	if (MachineType==3) {
		switch ((Address&0xf000)>>12) {
		case 0:
		case 1:
		case 2:
			return(WholeRam[Address]); // Low memory - not paged.
			break;
		case 3:
		case 4:
		case 5:
		case 6:
		case 7:
			if ((!Sh_CPUX) && (!Sh_CPUE)) return(WholeRam[Address]);
			if (Sh_CPUX) return(ShadowRAM[Address]);
			if ((Sh_CPUE)  && (!Sh_CPUX)) {
				if ((PrePC>=0xc000) && (PrePC<0xe000)) return(ShadowRAM[Address]); else return(WholeRam[Address]);
			}
			break;
		case 8:
			if (PRAM>0) { 
				return(PrivateRAM[Address-0x8000]); 
			} else { 
				return(Roms[ROMSEL][Address-0x8000]);
			}
			break;
		case 9:
		case 0xa:
		case 0xb:
			return(Roms[ROMSEL][Address-0x8000]);
			break;
		case 0xc:
		case 0xd:
			if (FRAM) return(FSRam[Address-0xc000]); else return(WholeRam[Address]);
			break;
		case 0xe:
			return(WholeRam[Address]);
			break;
		case 0xf:
			if (Address<0xfc00 || Address>=0xff00) { return(WholeRam[Address]); }
			if ((ACCCON & 0x40) && Address>=0xfc00 && Address<0xff00) { 
				return WholeRam[Address];
			}
			break;
		default:
			return(0);
		}
	}
// Master 128 End

	if (Address>=0xff00)
		return(WholeRam[Address]);

	/* IO space */

	if (Address >= 0xfc00 && Address < 0xfe00) {
		SyncIO();
		AdjustForIORead();
	}

	/* VIAs first - games seem to do really heavy reading of these */
	/* Can read from a via using either of the two 16 bytes blocks */
	if ((Address & ~0xf)==0xfe40 || (Address & ~0xf)==0xfe50) {
		SyncIO();
		Value = SysVIARead(Address & 0xf);
		AdjustForIORead();
		return Value;
	}

	if ((Address & ~0xf)==0xfe60 || (Address & ~0xf)==0xfe70) {
		SyncIO();
		Value = UserVIARead(Address & 0xf);
		AdjustForIORead();
		return Value;
	}

	if ((Address & ~7)==0xfe00) {
		SyncIO();
		Value = CRTCRead(Address & 0x7);
		AdjustForIORead();
		return Value;
	}

	if (Address==0xfe08) {
		SyncIO();
		Value = Read_ACIA_Status();
		AdjustForIORead();
		return Value;
	}
	if (Address==0xfe09) {
		SyncIO();
		Value = Read_ACIA_Rx_Data();
		AdjustForIORead();
		return Value;
	}
	if (Address==0xfe10) {
		SyncIO();
		Value = Read_SERPROC();
		AdjustForIORead();
		return Value;
	}

	/* Rob: BBC AUG says FE20 is econet stn no. for bbc b. [It's in cmos for a master,]
	   This is wrong and doesn't work. Study of the circuit diagram for a model B (IC26) shows it's at 
	   FE18-FE1F. This now works.  AUG says read station also enables Econet NMI but again the circuit 
	   shows the read-station select line is also INTOFF. (and it's any access to FE18, not just a read.) 
	*/ 
	if (EconetEnabled &&
		((MachineType!=3 && (Address & ~3)==0xfe18) ||
		 (MachineType==3 && (Address & ~3)==0xfe38)) ) {
		if (DebugEnabled)
			DebugDisplayTrace(DEBUG_ECONET, true, "Econet: INTOFF");
		EconetNMIenabled = INTOFF; 
		return(Read_Econet_Station()); 
	}

	if (Address>=0xfe18 && Address<=0xfe20 && MachineType==3) {
		return(AtoDRead(Address - 0xfe18));
	}

	/* Rob: BBC AUG states video ULA at FE20-21 is write-only - why support reading?
	   The circuit diagram shows read of FE20-FE27 is INTON (maybe this is where AUG confusion came from)
	   INTON & INTOFF enable/disable NMI from the 68B54 via flip flop IC97, IC27 & IC91.  Guess what happens
	   if the ADLC is holding an NMI which was masked by INTOFF and you turn INTON ....!
	   (NMI from the FDC is always enabled)
	*/
	if (EconetEnabled &&
		((MachineType!=3 && (Address & ~3)==0xfe20) ||
		 (MachineType==3 && (Address & ~3)==0xfe3c)) ) {
		if (DebugEnabled) DebugDisplayTrace(DEBUG_ECONET, true, "Econet: INTON");
		if (!EconetNMIenabled) {  // was off
			EconetNMIenabled = INTON;  // turn on
			if (ADLC.status1 & 128) {			// irq pending?
				NMIStatus|=1<<nmi_econet; 
				if (DebugEnabled) DebugDisplayTrace(DEBUG_ECONET, true, "Econet: delayed NMI asserted");
			}
		}
	}

	if ((Address & ~3)==0xfe20) {
		return(VideoULARead(Address & 0xf));
	}

	// Master uses fe24 to fe2f for FDC
	if (Address==0xfe24 && MachineType==3) {
		return(ReadFDCControlReg());
	}
	if ((Address & ~7)==0xfe28 && MachineType==3) {
		return(Read1770Register(Address & 0x7));
	}

	if ((Address & ~3)==0xfe30) {
		return(PagedRomReg); /* report back ROMSEL - I'm sure the beeb allows ROMSEL read..
								correct me if im wrong. - Richard Gellman */
	}
	// In the Master at least, ROMSEL/ACCCON seem to be duplicated over a 4 byte block.
	if ((Address & ~3)==0xfe34 && MachineType==3) {
		return(ACCCON);
	}

	if (((Address & ~0x1f)==0xfe80) && (MachineType!=3) && (NativeFDC)) {
		return(Disc8271_read(Address & 0x7));
	}

	if ((Address & ~0x1f)==0xfea0) {
		if (EconetEnabled)
			return(ReadEconetRegister(Address & 3)); /* Read 68B54 ADLC */
		return(0xfe); // if not enabled
	}

	if ((Address & ~0x1f)==0xfec0 && MachineType!=3) {
		SyncIO();
		Value = AtoDRead(Address & 0xf);
		AdjustForIORead();
		return Value;
	}

	if ((Address & ~0x1f)==0xfee0)
	{
		if (TorchTube)
			return(ReadTorchTubeFromHostSide(Address&0x1f)); //Read From Torch Tube
		else
			return(ReadTubeFromHostSide(Address&7)); //Read From Tube
	}

	if ((Address & ~0x3)==0xfc10) {
		return(TeleTextRead(Address & 0x3));
	}
    
	if ((Address & ~0x3)==0xfc40) {
		if (SCSIDriveEnabled) return(SCSIRead(Address & 0x3));
	}

	if ((Address & ~0x7)==0xfc40) {
		if (IDEDriveEnabled)  return(IDERead(Address & 0x7));
	}

	if ((Address & ~0x1)==0xfc50) {
		return(mainWin->PasteKey(Address & 0x1));
	}

	if ((Address & ~0x3)==0xfdf0) {
		return(SASIRead(Address & 0x3));
	}

	if ((MachineType!=3) && (Address>=EFDCAddr) && (Address<(EFDCAddr+4)) && (!NativeFDC)) {
		//MessageBox(GETHWND,"Read of 1770 Extension Board\n","BeebEm",MB_OK|MB_ICONERROR);
		return(Read1770Register(Address-EFDCAddr));
	}
	if ((MachineType!=3) && (Address==EDCAddr) && (!NativeFDC)) {
		return(mainWin->GetDriveControl());
	}

	return(0xFF);
} /* BeebReadMem */

void DebugMemoryState()
{
	char sram[60];
	char* psram = sram;
	int i = 0;
	int m = 0;
	sram[0] = 0;
	DebugDisplayInfo("Memory state:");
	for(i = 0; i < 16; i++)
	{
		if(RomWritable[i])
		{
			psram += sprintf(psram,"%d, ",i);
			m += 16;
		}
	}
	if(strlen(sram) > 0)
	{
		sram[strlen(sram)-2] = '\0';
		DebugDisplayInfoF("%dK sideways RAM using bank %s",m,sram);
	}

	DebugDisplayInfoF("ROMSEL: %d", ROMSEL);
	switch(MachineType)
	{
		case 0:	// BBC B
			break;
		case 1: // Integra B
			DebugDisplayInfoF("Shadow RAM: %s, %s",ShEn == 1 ? "enabled" : "disabled", MemSel == 0 && ShEn == 1 ? "selected" : "not selected");
			DebugDisplayInfoF("Private areas: %s; 1K %s, 4K %s, 8K %s",PrvEn == 1 ? "enabled" : "disabled", Prvs1 == 1 ? "on" : "off", Prvs4 == 1 ? "on" : "off", Prvs8 == 1 ? "on" : "off");
			DebugDisplayInfoF("Hidden area address: 0x%01X", HidAdd);
			break;
		case 2: // B+
			DebugDisplayInfoF("Shadow RAM: %s, %s", Sh_Display == 1 ? "enabled" : "disabled", Sh_Display == 1 && ((PrePC>=0xC000 && PrePC<0xE000) || (MemSel==1 && PrePC>=0xA000 && PrePC <0xB000)) ? "selected" : "not selected");
			DebugDisplayInfoF("Private RAM: %s", MemSel==1 ? "enabled" : "disabled");
			break;
		case 3: // Master
			DebugDisplayInfoF("ACCCON: IRR:%s TST:%s IFJ:%s ITU:%s Y:%s X:%s E:%s D:%s",
				(intStatus & 0x80) != 0 ? "on" : "off",
				(ACCCON & 0x40) != 0 ? "on" : "off",
				(ACCCON & 0x20) != 0 ? "on" : "off",
				(ACCCON & 0x10) != 0 ? "on" : "off",
				(ACCCON & 0x08) != 0 ? "on" : "off",
				(ACCCON & 0x04) != 0 ? "on" : "off",
				(ACCCON & 0x02) != 0 ? "on" : "off",
				(ACCCON & 0x01) != 0 ? "on" : "off");
			break;
	}
}

/*----------------------------------------------------------------------------*/
static void DoRomChange(int NewBank) {
  ROMSEL=NewBank&0xf;

  if (MachineType!=3) {
    NewBank&=0xf; // strip top bit if Model B
    PagedRomReg=NewBank;
	return;
  };

  // Master Specific stuff   
  if (MachineType==3) {
	  PagedRomReg=NewBank;
	  PRAM=(PagedRomReg & 128);
  }

}; /* DoRomChange */
/*----------------------------------------------------------------------------*/
static void FiddleACCCON(unsigned char newValue) {
	// Master specific, should only execute in Master128 mode
	// ignore bits TST (6) IFJ (5) and ITU (4)
//	newValue&=143;
	unsigned char oldshd;
//	if ((newValue & 128)==128) DoInterrupt();
	ACCCON=newValue & 127; // mask out the IRR bit so that interrupts dont occur repeatedly
	if (newValue & 128) intStatus|=128; else intStatus&=127;
	oldshd=Sh_Display;
	Sh_Display=ACCCON & 1;
	if (Sh_Display!=oldshd) RedoMPTR();
	Sh_CPUX=ACCCON & 4;
	Sh_CPUE=ACCCON & 2;
	FRAM=ACCCON & 8;
}
/*----------------------------------------------------------------------------*/
static void RomWriteThrough(int Address, unsigned char Value) {
	int bank = 0;

	// SW RAM board - bank to write to is selected by User Via
	if (SWRAMBoardEnabled)
	{
		bank = (UserVIAState.orb & UserVIAState.ddrb) & 0xf;
		if (!RomWritable[bank])
			bank = 16;
	}
	else
	{
		// Find first writable bank
		while (bank < 16 && !RomWritable[bank])
			++bank;
	}

	if (bank < 16)
		Roms[bank][Address-0x8000]=Value;
}

/*----------------------------------------------------------------------------*/
void BeebWriteMem(int Address, unsigned char Value) {
  	unsigned char oldshd;
/*  fprintf(stderr,"Write %x to 0x%x\n",Value,Address); */

// BBC B Start
    if (MachineType==0) {
	 if (Address<0x8000) {
		 WholeRam[Address]=Value;
		 return;
	 }

	 if ((Address<0xc000) && (Address>=0x8000)) {
		 if (RomWritable[ROMSEL]) Roms[ROMSEL][Address-0x8000]=Value;
		 else RomWriteThrough(Address, Value);
		 return;
	 }
	}
// BBC B End


// BBC B Integra B Start
    if (MachineType==1) {
	 if (Address<0x3000) {
		 WholeRam[Address]=Value;
		 return;
	 }

	 if (Address<0x8000) {
		 if ((ShEn==1) && (MemSel==0)) {
			 ShadowRam[Address-0x3000]=Value;
			 return;
		 } else {
			 WholeRam[Address]=Value;
			 return;
		 }
	 }

	 if ((Address>=0x8000) && (Address<0x8400) && (Prvs8==1) && (PrvEn==1)) {
		 Private[Address-0x8000]=Value;
		 return;
	 }

	 if ((Address>=0x8000) && (Address<0x9000) && (Prvs4==1) && (PrvEn==1)) {
		 Private[Address-0x8000]=Value;
		 return;
	 }

	 if ((Address>=0x9000) && (Address<0xb000) && (Prvs1==1) && (PrvEn==1)) {
		 Private[Address-0x8000]=Value;
		 return;
	 }

	 if ((Address<0xc000) && (Address>=0x8000)) {
		if (RomWritable[ROMSEL]) Roms[ROMSEL][Address-0x8000]=Value;
		else RomWriteThrough(Address, Value);
		return;
	 }

	 if (Address==0xfe30) {
		 DoRomChange(Value);
		 MemSel = ((Value & 0x80)/0x80);
		 PrvEn = ((Value & 0x40)/0x40);
		 return;
	 }
	 
	 if (Address==0xfe34) {
		 ShEn=((Value &0x80)/0x80);
		 Prvs1=((Value &0x10)/0x10);
		 Prvs4=((Value &0x20)/0x20);
		 Prvs8=((Value &0x40)/0x40);
		 return;
	 }

	 if (Address==0xfe3c) {
		 if (HidAdd==0) {
			 Hidden[HidAdd]=Value;
			 return;
		 }
		 
		 if (HidAdd==2) {
			 Hidden[HidAdd]=Value;
			 return;
		 }

		 if (HidAdd==4) {
			 Hidden[HidAdd]=Value;
			 return;
		 }

		 if (HidAdd==6) {
			 Hidden[HidAdd]=Value;
			 return;
		 }

		 if (HidAdd==7) {
			 Hidden[HidAdd]=Value;
			 return;
		 }

		 if (HidAdd==8) {
			 Hidden[HidAdd]=Value;
			 return;
		 }

		 if (HidAdd==9) {
			 Hidden[HidAdd]=Value;
			 return;
		 }

		 Hidden[HidAdd]=Value;
		 return;
	 }

	 if (Address==0xfe38) {
		 HidAdd=Value;
		 return;
	 }
  }
// BBC B Integra B End


// BBC B+ Start
    if (MachineType==2) {
	 if (Address<0x3000) {
		 WholeRam[Address]=Value;
		 return;
	 }

	 if (Address<0x8000) {
		 if ((Sh_Display==1) && (PrePC>=0xC000) && (PrePC<0xE000)) {
			 ShadowRAM[Address]=Value;
			 return;
		 } else

		 if ((Sh_Display==1) && (MemSel==1) && (PrePC>=0xA000) && (PrePC<0xB000)) {
			 ShadowRAM[Address]=Value;
			 return;
		 } else {
			 WholeRam[Address]=Value;
			 return;
		 }
	 }

	 if ((Address<0xb000) && (MemSel==1)) {
		 Private[Address-0x8000]=Value;
		 return;
	 }


	 if ((Address<0xc000) && (Address>=0x8000)) {
		if (RomWritable[ROMSEL]) Roms[ROMSEL][Address-0x8000]=Value;
		else RomWriteThrough(Address, Value);
		return;
	 }

	 if (Address>=0xfe30 && Address<0xfe34) {
		 DoRomChange(Value);
		 MemSel = ((Value & 0x80)/0x80);
		 return;
	 }
	 
	 if (Address>=0xfe34 && Address<0xfe38) {
		 oldshd=Sh_Display;
		 Sh_Display=((Value &0x80)/0x80);
		 if (Sh_Display!=oldshd) RedoMPTR();
		 return;
	 }
	}
// BBC B+ End

// Master 128 Start
	if (MachineType==3) {
		if (Address < 0xfc00) {
			switch ((Address&0xf000)>>12) {
			case 0:
			case 1:
			case 2:
				WholeRam[Address]=Value; // Low memory - not paged.
				break;
			case 3:
			case 4:
			case 5:
			case 6:
			case 7:
				if ((!Sh_CPUX) && (!Sh_CPUE)) WholeRam[Address]=Value;
				if (Sh_CPUX) ShadowRAM[Address]=Value;
				if ((Sh_CPUE) && (!Sh_CPUX)) { 
					if ((PrePC>=0xc000) && (PrePC<0xe000)) ShadowRAM[Address]=Value; else WholeRam[Address]=Value;
				} 
				break;
			case 8:
				if (PRAM) { PrivateRAM[Address-0x8000]=Value; }
				else {
					if (RomWritable[ROMSEL]) Roms[ROMSEL][Address-0x8000]=Value;
					else RomWriteThrough(Address, Value);
				}
				break;
			case 9:
			case 0xa:
			case 0xb:
				if (RomWritable[ROMSEL]) Roms[ROMSEL][Address-0x8000]=Value;
				else RomWriteThrough(Address, Value);
				break;
			case 0xc:
			case 0xd:
				if (FRAM) FSRam[Address-0xc000]=Value;
				break;
			}
			return;
		}
	}
// Master 128 End

	/* IO space */

	if (Address >= 0xfc00 && Address < 0xfe00) {
		SyncIO();
		AdjustForIOWrite();
	}

	/* Can write to a via using either of the two 16 bytes blocks */
	if ((Address & ~0xf)==0xfe40 || (Address & ~0xf)==0xfe50) {
		SyncIO();
		AdjustForIOWrite();
		SysVIAWrite((Address & 0xf),Value);
		return;
	}

	/* Can write to a via using either of the two 16 bytes blocks */
	if ((Address & ~0xf)==0xfe60 || (Address & ~0xf)==0xfe70) {
		SyncIO();
		AdjustForIOWrite();
		UserVIAWrite((Address & 0xf),Value);
		return;
	}

	if ((Address & ~0x7)==0xfe00) {
		SyncIO();
		AdjustForIOWrite();
		CRTCWrite(Address & 0x7, Value);
		return;
	}

	if (Address==0xfe08) {
		SyncIO();
		AdjustForIOWrite();
		Write_ACIA_Control(Value);
		return;
	}
	if (Address==0xfe09) {
		SyncIO();
		AdjustForIOWrite();
		Write_ACIA_Tx_Data(Value);
		return;
	}
	if (Address==0xfe10) {
		SyncIO();
		AdjustForIOWrite();
		Write_SERPROC(Value);
		return;
	}

	//Rob: econet NMI mask
	if (EconetEnabled &&
		((MachineType!=3 && (Address & ~3)==0xfe18) ||
		 (MachineType==3 && (Address & ~3)==0xfe38)) ) {
		if (DebugEnabled)
			DebugDisplayTrace(DEBUG_ECONET, true, "Econet: INTOFF(w)");
		EconetNMIenabled = INTOFF; 
	}

	if ((Address & ~0x7)==0xfe18 && MachineType==3) {
		AtoDWrite((Address & 0x7),Value);
		return;
	}

	if ((Address & ~0x3)==0xfe20) {
		VideoULAWrite(Address & 0xf, Value);
		return;
	}

	if (Address==0xfe24 && MachineType==3) {
		WriteFDCControlReg(Value);
		return;
	}

	if ((Address & ~0x7)==0xfe28 && MachineType==3) {
		Write1770Register(Address & 7,Value);
		return;
	} 

	if (Address>=0xfe30 && Address<0xfe34) {
		DoRomChange(Value);
		return;
	}

	// In the Master at least, ROMSEL/ACCCON seem to be duplicated over a 4 byte block.
	/*cerr << "Write *0x" << hex << Address << "=0x" << Value << dec << "\n"; */
	if (Address>=0xfe34 && Address<0xfe38 && MachineType==3) {
		FiddleACCCON(Value);
		return;
	}

	if (((Address & ~0x1f)==0xfe80) && (MachineType!=3) && (NativeFDC)) {
		Disc8271_write((Address & 7),Value);
		return;
	}

	//Rob: add econet
	if (Address>=0xfeA0 && Address<0xfebf && (EconetEnabled) ) {
		WriteEconetRegister((Address & 3), Value);
		return;
	}

	if ((Address & ~0x1f)==0xfec0 && MachineType!=3) {
		SyncIO();
		AdjustForIOWrite();
		AtoDWrite((Address & 0xf),Value);
		return;
	}

	if ((Address&~0xf)==0xfee0)
	{
		if (TorchTube) 
			WriteTorchTubeFromHostSide(Address&0xf,Value);
		else
			WriteTubeFromHostSide(Address&7,Value);
	}

	if ((Address & ~0x3)==0xfc10) {
		TeleTextWrite((Address & 0x3),Value);
		return;
	}
    
	if ((Address & ~0x3)==0xfc40) {
		if (SCSIDriveEnabled) {
			SCSIWrite((Address & 0x3),Value);
			return;
		}
	}

	if ((Address & ~0x7)==0xfc40) {
		if (IDEDriveEnabled) {
			IDEWrite((Address & 0x7),Value);
			return;
		}
	}

	if ((Address & ~0x3)==0xfdf0) {
		SASIWrite((Address & 0x3),Value);
		return;
	}

	if ((MachineType!=3) && (Address==EDCAddr) && (!NativeFDC)) {
		mainWin->SetDriveControl(Value);
	}
	if ((MachineType!=3) && (Address>=EFDCAddr) && (Address<(EFDCAddr+4)) && (!NativeFDC)) {
		Write1770Register(Address-EFDCAddr,Value);
	}

    return;
}

bool ReadRomInfo(int bank, RomInfo* info)
{
	if((RomFlags)Roms[bank][6] == 0)
		return false;
	// LanguageAddr and ServiceAddr are really 6502 instructions. Most ROMs obey the JMP convention as
	// described in the AUG, however BASIC fills the first 6 bytes with CMP 1; BEQ 1F; RTS; NOP
	// which seems to simply be a check for whether it was entered properly.
	info->Slot = bank;
	info->ServiceAddr = info->LanguageAddr = info->RelocationAddr = 0;
	if(Roms[bank][0] == 0x4C)
		info->LanguageAddr = (Roms[bank][2] << 8) | Roms[bank][1];
	if(Roms[bank][3] == 0x4C)
		info->ServiceAddr = (Roms[bank][5] << 8) | Roms[bank][4];
	// TODO: Flags0-3 specify instruction type, see master reference manual part 1 p257.
	info->Flags = (RomFlags)Roms[bank][6];
	info->VersionStr[0] = '\0';
	info->Version = Roms[bank][8];
	strncpy(info->Title, (char*)&Roms[bank][9], 256);
	if(strlen(info->Title) + 9 != Roms[bank][7])
		strncpy(info->VersionStr, (char*)&Roms[bank][strlen(info->Title) + 10], 256);
	strncpy(info->Copyright, (char*)(Roms[bank] + Roms[bank][7] + 1), 256);
	if(info->Flags & RomRelocate)
	{
		int addr = Roms[bank][7] + (int)strlen(info->Copyright) + 2;
		info->RelocationAddr = (Roms[bank][addr + 3] << 24) | (Roms[bank][addr + 2] << 16) | (Roms[bank][addr + 1] << 8) | Roms[bank][addr];
	}
	if(!(info->Flags & RomService))
	{
		// BASIC:
		info->LanguageAddr = 0x8000;
		info->ServiceAddr = 0;
	}

	info->WorkspaceAddr = (BeebReadMem(0xDF0 + bank) * 0x100);

	// Some ROMs (e.g. DNFS) use bit 1 of this as an activity flag.
	// on Master 128 this is apparently not done and the address points into
	// the FSRAM.
	if(MachineType < 3)
		info->WorkspaceAddr &= 0x7FFF;
	return true;
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
bool ReadROMFile(const char *filename, ROMConfigFile ROMConfig)
{
	bool success = true;
	FILE *fd;
	int model;
	int bank;
	char line[MAX_PATH];

	fd = fopen(filename, "r");
	if (!fd)
	{
		char errstr[200];
		sprintf(errstr, "Cannot open ROM configuration file:\n  %s", filename);
		MessageBox(GETHWND,errstr,WindowTitle,MB_OK|MB_ICONERROR);
		return false;
	}

	for (model = 0; model < 4 && success; ++model)
	{
		for (bank = 0; bank < 17 && success; ++bank)
		{
			if (fgets(line, MAX_PATH, fd) == NULL)
			{
				success = false;
			}
			else
			{
				if (strchr(line, 13)) *strchr(line, 13) = 0;
				if (strchr(line, 10)) *strchr(line, 10) = 0;
				strcpy(ROMConfig[model][bank], line);
			}
		}
	}

	if (success && fgets(line, MAX_PATH, fd) != NULL)
	{
		// Too many lines
		success = false;
	}

	fclose(fd);

	if (!success)
	{
		char errstr[200];
		sprintf(errstr, "Invalid ROM configuration file:\n  %s", filename);
		MessageBox(GETHWND,errstr,WindowTitle,MB_OK|MB_ICONERROR);
		memset(ROMConfig, 0, sizeof(ROMConfigFile));
	}

	return success;
}

/*----------------------------------------------------------------------------*/
void BeebReadRoms(void) {
	FILE *InFile;
	char fullname[_MAX_PATH];
	int bank;
	char *RomName;
	char *extension;
	int i;

	// Clear ROMs
	for (bank = 0; bank < 16; bank++)
	{
		RomWritable[bank] = 0;
		RomBankType[bank] = BankEmpty;
		memset(Roms[bank], 0, 0x4000);
	}

	// Read OS ROM
	RomName = RomConfig[MachineType][0];
	strcpy(fullname,RomName);
	if ((RomName[0]!='\\') && (RomName[1]!=':')) {
		strcpy(fullname,RomPath);
		strcat(fullname,"BeebFile/");
		strcat(fullname,RomName);
	}

	// for some reason, we have to change \ to /  to make C work...
	for (i = 0; fullname[i]; i++)
	{
		if (fullname[i] == '\\')
			fullname[i] = '/';
	}
	InFile=fopen(fullname,"rb");
	if (InFile!=NULL)
	{
		fread(WholeRam+0xc000,1,16384,InFile);
		fclose(InFile);
		// Try to read OS ROM memory map:
		if((extension = strrchr(fullname, '.')) != NULL)
			*extension = 0;
		strncat(fullname, ".map", _MAX_PATH);
		DebugLoadMemoryMap(fullname, 16);
	}
	else {
		char errstr[200];
		sprintf(errstr, "Cannot open specified OS ROM:\n %s",fullname);
		MessageBox(GETHWND,errstr,WindowTitle,MB_OK|MB_ICONERROR);
	}

	// read paged ROMs
	for (bank=15;bank>=0;bank--)
	{
		RomName = RomConfig[MachineType][16-bank];
		strcpy(fullname,RomName);
		if ((RomName[0]!='\\') && (RomName[1]!=':')) {
			strcpy(fullname,RomPath);
			strcat(fullname,"BeebFile/");
			strcat(fullname,RomName);
		}

		if (strcmp(RomName,BANK_EMPTY)==0)
		{
			RomBankType[bank] = BankEmpty;
			RomWritable[bank] = 0;
		}
		else if (strcmp(RomName,BANK_RAM)==0)
		{
			RomBankType[bank] = BankRam;
			RomWritable[bank] = 1;
		}
		else
		{
			if (strncmp(RomName+(strlen(RomName)-4),ROM_WRITABLE,4)==0)
			{
				// Writable ROM
				RomBankType[bank] = BankRam;
				RomWritable[bank] = 1;
				fullname[strlen(fullname)-4]=0;
			}
			else
			{
				RomBankType[bank] = BankRom;
				RomWritable[bank] = 0;
			}

			for (i = 0; fullname[i]; i++)
			{
				if (fullname[i] == '\\')
					fullname[i] = '/';
			}
			InFile=fopen(fullname,"rb");
			if	(InFile!=NULL)
			{
				// Read ROM:
				fread(Roms[bank],1,16384,InFile);
				fclose(InFile);
				// Try to read ROM memory map:
				if((extension = strrchr(fullname, '.')) != NULL)
					*extension = 0;
				strncat(fullname, ".map", _MAX_PATH);
				DebugLoadMemoryMap(fullname, bank);
			}
			else {
				char errstr[200];
				sprintf(errstr, "Cannot open specified ROM:\n %s",fullname);
				MessageBox(GETHWND,errstr,WindowTitle,MB_OK|MB_ICONERROR);
			}
		}
	}
}
/*----------------------------------------------------------------------------*/
void BeebMemInit(unsigned char LoadRoms,unsigned char SkipIntegraBConfig) {
  // Reset everything
  memset(WholeRam,0,0x8000);
  memset(FSRam,0,0x2000);
  memset(ShadowRAM,0,0x8000);
  memset(PrivateRAM,0,0x1000);
  ACCCON=0;
  PRAM=FRAM=Sh_Display=Sh_CPUE=Sh_CPUX=0;
  memset(Private,0,0x3000);
  Private[0x3b2]=4; // Default OSMODE to 4
  memset(ShadowRam,0,0x5000);
  MemSel=PrvEn=ShEn=Prvs1=Prvs4=Prvs8=HidAdd=0;
  if (!SkipIntegraBConfig)
  {
	  memset(Hidden,0,256);
	  memcpy(Hidden, HiddenDefault, 31);
  }

  if (LoadRoms) {
	  // This shouldn't be required for sideways RAM.
	  DebugInitMemoryMaps();
	  BeebReadRoms(); // Only load roms on start
  }

  /* Put first ROM in */
  memcpy(WholeRam+0x8000,Roms[0xf],0x4000);
  PagedRomReg=0xf;

} /* BeebMemInit */

/*-------------------------------------------------------------------------*/
void SaveMemUEF(FILE *SUEF) {
	int bank;
	switch (MachineType) {
	case 0:
	case 3:
		fput16(0x0461,SUEF); // Memory Control State
		fput32(2,SUEF);
		fputc(PagedRomReg,SUEF);
		fputc(ACCCON,SUEF);
		break;

	case 1:
		fput16(0x0461,SUEF); // Memory Control State
		fput32(3,SUEF);
		fputc(PagedRomReg|(MemSel<<7)|(PrvEn<<6),SUEF);
		fputc((ShEn<<7)|(Prvs8<<6)|(Prvs4<<5)|(Prvs1<<4),SUEF);
		fputc(HidAdd,SUEF);
		break;

	case 2:
		fput16(0x0461,SUEF); // Memory Control State
		fput32(2,SUEF);
		fputc(PagedRomReg|(MemSel<<7),SUEF);
		fputc((Sh_Display<<7),SUEF);
		break;
	}

	fput16(0x0462,SUEF); // Main Memory
	fput32(32768,SUEF);
	fwrite(WholeRam,1,32768,SUEF);

	switch (MachineType) {
	case 1:
		fput16(0x0463,SUEF); // Shadow RAM
		fput32(20480,SUEF);
		fwrite(ShadowRam,1,20480,SUEF);
		fput16(0x0464,SUEF); // Private RAM
		fput32(12288,SUEF);
		fwrite(Private,1,12288,SUEF);
		fput16(0x046D,SUEF); // IntegraB Hidden RAM
		fput32(256,SUEF);
		fwrite(Hidden,1,256,SUEF);
		break;

	case 2:
		fput16(0x0463,SUEF); // Shadow RAM
		fput32(32768,SUEF);
		fwrite(ShadowRAM,1,32768,SUEF);
		fput16(0x0464,SUEF); // Private RAM
		fput32(12288,SUEF);
		fwrite(Private,1,12288,SUEF);
		break;

	case 3:
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
		break;
	}
	for (bank=0;bank<16;bank++) {
		switch (RomBankType[bank])
		{
		case BankRam:
			fput16(0x0466,SUEF); // RAM bank
			fput32(16385,SUEF);
			fputc(bank,SUEF);
			fwrite(Roms[bank],1,16384,SUEF);
			break;
		case BankRom:
			fput16(0x0475,SUEF); // ROM bank
			fput32(16386,SUEF);
			fputc(bank,SUEF);
			fputc(BankRom,SUEF);
			fwrite(Roms[bank],1,16384,SUEF);
			break;
		case BankEmpty:
			fput16(0x0475,SUEF); // ROM bank
			fput32(2,SUEF);
			fputc(bank,SUEF);
			fputc(BankEmpty,SUEF);
			break;
		}
	}
}

void LoadRomRegsUEF(FILE *SUEF) {
	PagedRomReg=fgetc(SUEF);
	ROMSEL=PagedRomReg & 0xf;
	ACCCON=fgetc(SUEF);
	switch (MachineType) {
	case 1:
		MemSel=(PagedRomReg >> 7) & 1;
		PrvEn=(PagedRomReg >> 6) & 1;
		PagedRomReg&=0xf;
		ShEn=(ACCCON>>7) & 1;
		Prvs8=(ACCCON>>6) & 1;
		Prvs4=(ACCCON>>5) & 1;
		Prvs1=(ACCCON>>4) & 1;
		HidAdd=fgetc(SUEF);
		break;

	case 2:
		MemSel=(PagedRomReg >> 7) & 1;
		PagedRomReg&=0xf;
		Sh_Display=(ACCCON>>7) & 1;
		break;

	case 3:
		PRAM=PagedRomReg & 128;
		Sh_Display=ACCCON & 1;
		Sh_CPUX=ACCCON & 4;
		Sh_CPUE=ACCCON & 2;
		FRAM=ACCCON & 8;
		break;
	}
}

void LoadMainMemUEF(FILE *SUEF) {
	fread(WholeRam,1,32768,SUEF);
}

void LoadShadMemUEF(FILE *SUEF) {
	int SAddr;
	switch (MachineType) {
	case 1:
		fread(ShadowRam,1,20480,SUEF);
		break;
	case 2:
		fread(ShadowRAM,1,32768,SUEF);
		break;
	case 3:
		SAddr=fget16(SUEF);
		fread(ShadowRAM+SAddr,1,32768,SUEF);
		break;
	}
}

void LoadPrivMemUEF(FILE *SUEF) {
	switch (MachineType) {
	case 1:
		fread(Private,1,12288,SUEF);
		break;
	case 2:
		fread(Private,1,12288,SUEF);
		break;
	case 3:
		fread(PrivateRAM,1,4096,SUEF);
		break;
	}
}

void LoadFileMemUEF(FILE *SUEF) {
	fread(FSRam,1,8192,SUEF);
}

void LoadIntegraBHiddenMemUEF(FILE *SUEF) {
	fread(Hidden,1,256,SUEF);
}

void LoadSWRamMemUEF(FILE *SUEF) {
	int Rom;
	Rom=fgetc(SUEF);
	RomWritable[Rom]=1;
	RomBankType[Rom]=BankRam;
	fread(Roms[Rom],1,16384,SUEF);
}
void LoadSWRomMemUEF(FILE *SUEF) {
	int Rom;
	Rom=fgetc(SUEF);
	RomBankType[Rom]=(BankType)fgetc(SUEF);
	switch (RomBankType[Rom])
	{
	case BankRom:
		RomWritable[Rom]=0;
		fread(Roms[Rom],1,16384,SUEF);
		break;
	case BankEmpty:
		memset(Roms[Rom], 0, 0x4000);
		break;
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
