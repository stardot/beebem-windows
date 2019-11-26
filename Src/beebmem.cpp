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
#include <errno.h>
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
#include "scsi.h"
#include "sasi.h"
#include "ide.h"
#include "uefstate.h"
#include "z80mem.h"
#include "z80.h"
#include "econet.h"		//Rob
#include "debug.h"		//Rob added for INTON/OFF reporting only
#include "teletext.h"
#include "music5000.h"
#include "hog1mpaula.h"

using namespace std;

/* Each Rom now has a Ram/Rom flag */
bool RomWritable[16] = {
	true, true, true, true, true, true, true, true,
	true, true, true, true, true, true, true, true
};

/* Identifies what is in each bank */
BankType RomBankType[16] = {
	BankType::Empty, BankType::Empty, BankType::Empty, BankType::Empty,
	BankType::Empty, BankType::Empty, BankType::Empty, BankType::Empty,
	BankType::Empty, BankType::Empty, BankType::Empty, BankType::Empty,
	BankType::Empty, BankType::Empty, BankType::Empty, BankType::Empty
};

int PagedRomReg;

// Computech (&B+) Specific Stuff Added by K.Lowe 18/08/03
bool MemSel = false; // Shadow/Main RAM Toggle
bool PrvEn = false;  // Private RAM Enable
bool ShEn = false;   // Shadow RAM Enable
bool Prvs1 = false;  // Private RAM 1K Area
bool Prvs4 = false;  // Private RAM 4K Area
bool Prvs8 = false;  // Private RAM 8K Area
int HidAdd = 0;
// End of Computech (&B+) Specific Stuff

unsigned char WholeRam[65536];
unsigned char Roms[16][16384];

/* Computech (&B+) Specific Stuff Added by K.Lowe 18/08/03 */
unsigned char Hidden[256];
unsigned char Private[12288];
unsigned char ShadowRam[20480];
unsigned char HiddenDefault[31] = { 0,0,0,0,0,0,2,1,1,0,0xe0,0x8e,0,0,
0,0,0,0,0,
0xed,0xff,0xff,0x78,0,0x17,0x20,0x19,5,0x0a,0x2d,0xa1 };

//Addresses 0x0 thru 0xD:		RTC Data: Sec, SecAlm, Min, MinAlm, Hr, HrAlm, Day, Date, Month, Year, Registers A, B, C & D
//Addresses 0xE thru 0x12:	?
//Address 0x13: 0xED		LANGUAGE in bank E. FILE SYSTEM in bank D. ROMS.cfg should match this, but IBOS reset will correct if wrong
//Address 0x14: 0xFF		*INSERT status for ROMS &0F to &08. Default: &FF (All 8 ROMS enabled)
//Address 0x15: 0xFF		*INSERT status for ROMS &07 to &00. Default: &FF (All 8 ROMS enabled)
//Address 0x18: 0x17		0-2: MODE / 3: SHADOW / 4: TV Interlace / 5-7: TV screen shift.
//Address 0x19: 0x20		0-2: FDRIVE / 3-5: CAPS. Default was &23. Changed to &20
//Address 0x1A: 0x19		0-7: Keyboard Delay
//Address 0x1B: 0x05		0-7: Keyboard Repeat
//Address 0x1C: 0x0A		0-7: Printer Ignore
//Address 0x1D: 0x2D		0: Tube / 2-4: BAUD / 5-7: Printer
//Address 0x1E: 0xA1		0: File system / 4: Boot / 5-7: Data. Default was &A0. Changed to &A1/* End of Computech (&B+) Specific Stuff */

unsigned char ROMSEL;
/* Master 128 Specific Stuff */
unsigned char FSRam[8192];       // 8K Filing System RAM
unsigned char PrivateRAM[4096];  // 4K Private RAM (VDU Use mainly)
unsigned char CMOSRAM[64];       // 50 Bytes CMOS RAM
unsigned char CMOSDefault[64]={0,0,0,0,0,0xc9,0xff,0xfe,0x32,0,7,0xc1,0x1e,5,0,0x59,0xa2}; // Backup of CMOS Defaults
unsigned char ShadowRAM[32768];  // 20K Shadow RAM
unsigned char ACCCON;            // ACCess CONtrol register
struct CMOSType CMOS;
bool Sh_Display;
static bool PRAM, FRAM;
static bool Sh_CPUX, Sh_CPUE;
/* End of Master 128 Specific Stuff, note initilised anyway regardless of Model Type in use */

/* ROM file data */
char RomPath[_MAX_PATH];
char RomFile[_MAX_PATH];
ROMConfigFile RomConfig;
char DiscPath[_MAX_PATH]; // JGH

// FDD Extension board variables
int EFDCAddr;   // 1770 FDC location
int EDCAddr;    // Drive control location
bool NativeFDC; // true for 8271, false for DLL extension

// Econet NMI enable signals. Decoded from address bus and latched by IC97
#define INTON	true
#define INTOFF	false

/*----------------------------------------------------------------------------*/
/* Perform hardware address wrap around */
static int WrapAddr(int Address) {
  static const int offsets[] = {0x4000, 0x6000, 0x3000, 0x5800}; // page 419 of AUG is wrong

  if (Address < 0x8000) {
    return Address;
  }

  Address += offsets[(IC32State & 0x30) >> 4];
  Address &= 0x7fff;

  return Address;
}

/*----------------------------------------------------------------------------*/
/* This is for the use of the video routines.  It returns a pointer to
   a continuous area of 'n' bytes containing the contents of the
   'n' bytes of beeb memory starting at address 'a', with wrap around
   at 0x8000.  Potentially this routine may return a pointer into  a static
   buffer - so use the contents before recalling it
   'n' must be less than 1K in length.
   See 'BeebMemPtrWithWrapMo7' for use in Mode 7 - it's a special case.
*/

const unsigned char *BeebMemPtrWithWrap(int Address, int Length) {
  static unsigned char tmpBuf[1024];
  unsigned char *tmpBufPtr;

  Address = WrapAddr(Address);
  int EndAddress = WrapAddr(Address + Length - 1);

  // On Master the FSRam area is displayed if start addr below shadow area
  if (MachineType == Model::Master128 && Address <= EndAddress && Sh_Display && Address < 0x3000) {
    if (0x3000 - Address < Length) {
      int toCopy = 0x3000 - Address;
      if (toCopy > Length) toCopy = Length;
      if (toCopy > 0) memcpy(tmpBuf, FSRam + 0x2000 - toCopy, toCopy);
      tmpBufPtr = tmpBuf + toCopy;
      toCopy = Length - toCopy;
      if (toCopy > 0) memcpy(tmpBufPtr, ShadowRAM + EndAddress - (toCopy - 1), toCopy);
      return tmpBuf;
    }
    else if (Address < 0x1000) {
      return FSRam; // Should probably be PrivateRAM?
    }
    else {
      return FSRam + Address - 0x1000;
    }
  }

  if (Address <= EndAddress) {
    if (Sh_Display) {
      return ShadowRAM + Address;
    }
    else {
      return WholeRam + Address;
    }
  }

  int toCopy = 0x8000 - Address;

  if (toCopy > Length) toCopy = Length;

  if (toCopy > 0) {
    if (Sh_Display) {
      memcpy(tmpBuf, ShadowRAM + Address, toCopy);
    }
    else {
      memcpy(tmpBuf, WholeRam + Address, toCopy);
    }
  }

  tmpBufPtr = tmpBuf + toCopy;
  toCopy = Length - toCopy;

  if (toCopy > 0) {
    if (Sh_Display) {
      memcpy(tmpBufPtr, ShadowRAM + EndAddress - (toCopy - 1), toCopy);
    }
    else {
      memcpy(tmpBufPtr, WholeRam + EndAddress - (toCopy - 1), toCopy);
    }
  }

  // Tripling is for Shadow RAM handling
  return tmpBuf;
}

/*----------------------------------------------------------------------------*/
/* Perform hardware address wrap around - for mode 7 */
static int WrapAddrMo7(int Address) {
  if (Address < 0x8000) {
    return Address;
  }

  Address += 0x7c00;
  Address &= 0x7fff;

  return Address;
}

/*----------------------------------------------------------------------------*/
/* Special case of BeebMemPtrWithWrap for use in mode 7 */

const unsigned char *BeebMemPtrWithWrapMo7(int Address, int Length) {
  static unsigned char tmpBuf[1024];

  Address = WrapAddrMo7(Address);
  int EndAddress = WrapAddrMo7(Address + Length - 1);

  if (Address <= EndAddress) {
    if (Sh_Display) {
      return ShadowRAM + Address;
    }
    else {
      return WholeRam + Address;
    }
  }

  int toCopy = 0x8000 - Address;

  if (toCopy > Length) {
    if (Sh_Display) {
      return ShadowRAM + Address;
    }
    else {
      return WholeRam + Address;
    }
  }

  if (toCopy > 0) {
    if (Sh_Display) {
      memcpy(tmpBuf, ShadowRAM + Address, toCopy);
    }
    else {
      memcpy(tmpBuf, WholeRam + Address, toCopy);
    }
  }

  unsigned char *tmpBufPtr = tmpBuf + toCopy;
  toCopy = Length - toCopy;

  if (toCopy > 0) {
    if (Sh_Display) {
      memcpy(tmpBufPtr, ShadowRAM + EndAddress - (toCopy - 1), toCopy);
    }
    else {
      memcpy(tmpBufPtr, WholeRam + EndAddress - (toCopy - 1), toCopy);
    }
  }

  return tmpBuf;
}

/*----------------------------------------------------------------------------*/
int BeebReadMem(int Address) {
	int Value = 0xff;

	if (MachineType == Model::B) {
		if (Address >= 0x8000 && Address < 0xc000) return Roms[ROMSEL][Address - 0x8000];
		if (Address < 0xfc00) return WholeRam[Address];
		if (Address >= 0xff00) return WholeRam[Address];
	}
	else if (MachineType == Model::IntegraB) {
		if (Address < 0x3000) return WholeRam[Address];
		if (Address >= 0x8000 && Address < 0x8400 && Prvs8 && PrvEn) return Private[Address-0x8000];
		if (Address >= 0x8000 && Address < 0x9000 && Prvs4 && PrvEn) return Private[Address-0x8000];
		if (Address >= 0x9000 && Address < 0xb000 && Prvs1 && PrvEn) return Private[Address-0x8000];
		if (Address < 0x8000) {
			if (ShEn) {
				if (MemSel) {
					return WholeRam[Address];
				}
				else {
					return ShadowRam[Address - 0x3000];
				}
			}
			else {
				return WholeRam[Address];
			}
		}
		if (Address >= 0x8000 && Address < 0xc000) return Roms[ROMSEL][Address-0x8000];
		if (Address < 0xfc00) return WholeRam[Address];
		if (Address >= 0xff00) return WholeRam[Address];

		if (Address==0xfe3c) {
			time_t long_time; // Clock for Computech Integra-B
			time( &long_time );
			if (HidAdd==0) return(localtime(&long_time)->tm_sec);
			if (HidAdd==2) return(localtime(&long_time)->tm_min);
			if (HidAdd==4) return(localtime(&long_time)->tm_hour);
			if (HidAdd==6) return((localtime(&long_time)->tm_wday)+1);
			if (HidAdd==7) return(localtime(&long_time)->tm_mday);
			if (HidAdd==8) return((localtime(&long_time)->tm_mon)+1);
			if (HidAdd==9) return((localtime(&long_time)->tm_year)%100);
			if (HidAdd==0xa) return(0x0);
			return(Hidden[HidAdd]);
		}
	}
	else if (MachineType == Model::BPlus) {
		if (Address < 0x3000) return WholeRam[Address];
		if (Address < 0x8000 && Sh_Display && PrePC >= 0xc000 && PrePC < 0xe000) return ShadowRAM[Address];
		if (Address < 0x8000 && Sh_Display && MemSel && PrePC >= 0xa000 && PrePC < 0xb000) return ShadowRAM[Address];
		if (Address < 0x8000) return WholeRam[Address];
		if (Address < 0xB000 && MemSel) return Private[Address-0x8000];
		if (Address >= 0x8000 && Address < 0xc000) return Roms[ROMSEL][Address-0x8000];
		if (Address < 0xfc00) return WholeRam[Address];
		if (Address >= 0xff00) return WholeRam[Address];
	}
	else if (MachineType == Model::Master128) {
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
			if (!Sh_CPUX && !Sh_CPUE) return WholeRam[Address];
			if (Sh_CPUX) return ShadowRAM[Address];
			if (Sh_CPUE && !Sh_CPUX) {
				if (PrePC >= 0xc000 && PrePC < 0xe000) {
					return ShadowRAM[Address];
				}
				else {
					return WholeRam[Address];
				}
			}
			break;
		case 8:
			if (PRAM) {
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
		((MachineType != Model::Master128 && (Address & ~3) == 0xfe18) ||
		 (MachineType == Model::Master128 && (Address & ~3) == 0xfe38)) ) {
		if (DebugEnabled)
			DebugDisplayTrace(DebugType::Econet, true, "Econet: INTOFF");
		EconetNMIenabled = INTOFF; 
		return(Read_Econet_Station()); 
	}

	if (Address >= 0xfe18 && Address <= 0xfe20 && MachineType == Model::Master128) {
		return(AtoDRead(Address - 0xfe18));
	}

	/* Rob: BBC AUG states video ULA at FE20-21 is write-only - why support reading?
	   The circuit diagram shows read of FE20-FE27 is INTON (maybe this is where AUG confusion came from)
	   INTON & INTOFF enable/disable NMI from the 68B54 via flip flop IC97, IC27 & IC91.  Guess what happens
	   if the ADLC is holding an NMI which was masked by INTOFF and you turn INTON ....!
	   (NMI from the FDC is always enabled)
	*/
	if (EconetEnabled &&
		((MachineType != Model::Master128 && (Address & ~3) == 0xfe20) ||
		 (MachineType == Model::Master128 && (Address & ~3) == 0xfe3c)) ) {
		if (DebugEnabled) DebugDisplayTrace(DebugType::Econet, true, "Econet: INTON");
		if (!EconetNMIenabled) {  // was off
			EconetNMIenabled = INTON;  // turn on
			if (ADLC.status1 & 128) {			// irq pending?
				NMIStatus |= 1 << nmi_econet;
				if (DebugEnabled) DebugDisplayTrace(DebugType::Econet, true, "Econet: delayed NMI asserted");
			}
		}
	}

	if ((Address & ~3)==0xfe20) {
		return(VideoULARead(Address & 0xf));
	}

	// Master uses fe24 to fe2f for FDC
	if (Address == 0xfe24 && MachineType == Model::Master128) {
		return(ReadFDCControlReg());
	}

	if ((Address & ~7) == 0xfe28 && MachineType == Model::Master128) {
		return(Read1770Register(Address & 0x7));
	}

	if ((Address & ~3)==0xfe30) {
		return(PagedRomReg); /* report back ROMSEL - I'm sure the beeb allows ROMSEL read..
								correct me if im wrong. - Richard Gellman */
	}
	// In the Master at least, ROMSEL/ACCCON seem to be duplicated over a 4 byte block.
	if ((Address & ~3) == 0xfe34 && MachineType == Model::Master128) {
		return(ACCCON);
	}

	if ((Address & ~0x1f) == 0xfe80 && MachineType != Model::Master128 && NativeFDC) {
		return Disc8271Read(Address & 0x7);
	}

	if ((Address & ~0x1f)==0xfea0) {
		if (EconetEnabled)
			return(ReadEconetRegister(Address & 3)); /* Read 68B54 ADLC */
		return(0xfe); // if not enabled
	}

	if ((Address & ~0x1f) == 0xfec0 && MachineType != Model::Master128) {
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
		return(TeletextRead(Address & 0x3));
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

	//DB: M5000 will only return its fcff select register or 
	//jim mapped registers if it has been selected by writing
	//it's id value to fcff - other jim devices should similarly
	//only return fcff..fcfc/fdxx when they have been selected
	UINT8 ret;
	if (Music5000Read(Address, &ret))
		return ret;
	if (Hog1MPaulaRead(Address, &ret))
		return ret;


	if ((Address & ~0x3)==0xfdf0) {
		return(SASIRead(Address & 0x3));
	}

	if (MachineType != Model::Master128 && Address >= EFDCAddr && Address < (EFDCAddr+4) && !NativeFDC) {
		//MessageBox(GETHWND,"Read of 1770 Extension Board\n","BeebEm",MB_OK|MB_ICONERROR);
		return(Read1770Register(Address-EFDCAddr));
	}
	if (MachineType != Model::Master128 && Address == EDCAddr && !NativeFDC) {
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
		case Model::B:
			break;
		case Model::IntegraB:
			DebugDisplayInfoF("Shadow RAM: %s, %s",ShEn ? "enabled" : "disabled", !MemSel && ShEn ? "selected" : "not selected");
			DebugDisplayInfoF("Private areas: %s; 1K %s, 4K %s, 8K %s",PrvEn ? "enabled" : "disabled", Prvs1 ? "on" : "off", Prvs4 ? "on" : "off", Prvs8 ? "on" : "off");
			DebugDisplayInfoF("Hidden area address: 0x%01X", HidAdd);
			break;
		case Model::BPlus:
			DebugDisplayInfoF("Shadow RAM: %s, %s", Sh_Display ? "enabled" : "disabled", Sh_Display && ((PrePC >= 0xC000 && PrePC < 0xE000) || (MemSel && PrePC >= 0xA000 && PrePC < 0xB000)) ? "selected" : "not selected");
			DebugDisplayInfoF("Private RAM: %s", MemSel ? "enabled" : "disabled");
			break;
		case Model::Master128:
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

  if (MachineType != Model::Master128) {
    NewBank&=0xf; // strip top bit if Model B
    PagedRomReg=NewBank;
    return;
  }

  // Master Specific stuff   
  if (MachineType == Model::Master128) {
    PagedRomReg = NewBank;
    PRAM = (PagedRomReg & 128) != 0;
  }
}

/*----------------------------------------------------------------------------*/
static void FiddleACCCON(unsigned char newValue) {
	// Master specific, should only execute in Master128 mode
	// ignore bits TST (6) IFJ (5) and ITU (4)
//	newValue&=143;
//	if ((newValue & 128)==128) DoInterrupt();
	ACCCON=newValue & 127; // mask out the IRR bit so that interrupts dont occur repeatedly
	if (newValue & 128) intStatus|=128; else intStatus&=127;
	bool oldshd = Sh_Display;
	Sh_Display = (ACCCON & 1) != 0;
	if (Sh_Display != oldshd) RedoMPTR();
	Sh_CPUX = (ACCCON & 4) != 0;
	Sh_CPUE = (ACCCON & 2) != 0;
	FRAM = (ACCCON & 8) != 0;
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
/*  fprintf(stderr,"Write %x to 0x%x\n",Value,Address); */

	if (MachineType == Model::B) {
		if (Address < 0x8000) {
			WholeRam[Address] = Value;
			return;
		}

		if (Address < 0xc000 && Address >= 0x8000) {
			if (RomWritable[ROMSEL]) Roms[ROMSEL][Address -0x8000] =Value;
			else RomWriteThrough(Address, Value);
			return;
		}
	}
	else if (MachineType == Model::IntegraB) {
		if (Address < 0x3000) {
			WholeRam[Address] = Value;
			return;
		}

		if (Address < 0x8000) {
			if (ShEn && !MemSel) {
				ShadowRam[Address - 0x3000] = Value;
				return;
			} else {
				WholeRam[Address]=Value;
				return;
			}
		}

		if (Address >= 0x8000 && Address < 0x8400 && Prvs8 && PrvEn) {
			Private[Address - 0x8000] = Value;
			return;
		}

		if (Address >= 0x8000 && Address < 0x9000 && Prvs4 && PrvEn) {
			Private[Address - 0x8000] = Value;
			return;
		}

		if (Address >= 0x9000 && Address < 0xb000 && Prvs1 && PrvEn) {
			Private[Address - 0x8000] = Value;
			return;
		}

		if (Address < 0xc000 && Address >= 0x8000) {
			if (RomWritable[ROMSEL]) Roms[ROMSEL][Address - 0x8000] = Value;
			else RomWriteThrough(Address, Value);
			return;
		}

		if (Address == 0xfe30) {
			DoRomChange(Value);
			MemSel = (Value & 0x80) != 0;
			PrvEn  = (Value & 0x40) != 0;
			return;
		}

		if (Address == 0xfe34) {
			ShEn  = (Value & 0x80) != 0;
			Prvs1 = (Value & 0x10) != 0;
			Prvs4 = (Value & 0x20) != 0;
			Prvs8 = (Value & 0x40) != 0;
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
	else if (MachineType == Model::BPlus) {
		if (Address<0x3000) {
			WholeRam[Address]=Value;
			return;
		}

		if (Address<0x8000) {
			if (Sh_Display && PrePC >= 0xC000 && PrePC < 0xE000) {
				ShadowRAM[Address]=Value;
				return;
			}
			else if (Sh_Display && MemSel && PrePC >= 0xA000 && PrePC < 0xB000) {
				ShadowRAM[Address]=Value;
				return;
			} else {
				WholeRam[Address]=Value;
				return;
			}
		}

		if (Address < 0xb000 && MemSel) {
			Private[Address - 0x8000] = Value;
			return;
		 }

		if ((Address < 0xc000) && (Address >= 0x8000)) {
			if (RomWritable[ROMSEL]) Roms[ROMSEL][Address-0x8000]=Value;
			//else RomWriteThrough(Address, Value); //Not supported on B+
			return;
		}

		 if (Address>=0xfe30 && Address<0xfe34) {
			DoRomChange(Value);
			MemSel = (Value & 0x80) != 0;
			return;
		}

		if (Address>=0xfe34 && Address<0xfe38) {
			bool oldshd = Sh_Display;
			Sh_Display = (Value & 0x80) != 0;
			if (Sh_Display!=oldshd) RedoMPTR();
			return;
		}
	}
	else if (MachineType == Model::Master128) {
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
				if (!Sh_CPUX && !Sh_CPUE) WholeRam[Address] = Value;
				if (Sh_CPUX) ShadowRAM[Address] = Value;
				if (Sh_CPUE && !Sh_CPUX) {
					if ((PrePC>=0xc000) && (PrePC<0xe000)) ShadowRAM[Address]=Value; else WholeRam[Address]=Value;
				} 
				break;
			case 8:
				if (PRAM) { PrivateRAM[Address-0x8000]=Value; }
				else {
					if (RomWritable[ROMSEL]) Roms[ROMSEL][Address-0x8000]=Value;
					//else RomWriteThrough(Address, Value); //Not supported on Master
				}
				break;
			case 9:
			case 0xa:
			case 0xb:
				if (RomWritable[ROMSEL]) Roms[ROMSEL][Address-0x8000]=Value;
				//else RomWriteThrough(Address, Value); //Not supported on Master
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
		((MachineType != Model::Master128 && (Address & ~3) == 0xfe18) ||
		 (MachineType == Model::Master128 && (Address & ~3) == 0xfe38)) ) {
		if (DebugEnabled)
			DebugDisplayTrace(DebugType::Econet, true, "Econet: INTOFF(w)");
		EconetNMIenabled = INTOFF; 
	}

	if ((Address & ~0x7) == 0xfe18 && MachineType == Model::Master128) {
		AtoDWrite((Address & 0x7),Value);
		return;
	}

	if ((Address & ~0x3)==0xfe20) {
		VideoULAWrite(Address & 0xf, Value);
		return;
	}

	if (Address == 0xfe24 && MachineType == Model::Master128) {
		WriteFDCControlReg(Value);
		return;
	}

	if ((Address & ~0x7) == 0xfe28 && MachineType == Model::Master128) {
		Write1770Register(Address & 7,Value);
		return;
	} 

	if (Address>=0xfe30 && Address<0xfe34) {
		DoRomChange(Value);
		return;
	}

	// In the Master at least, ROMSEL/ACCCON seem to be duplicated over a 4 byte block.
	/*cerr << "Write *0x" << hex << Address << "=0x" << Value << dec << "\n"; */
	if (Address >= 0xfe34 && Address < 0xfe38 && MachineType == Model::Master128) {
		FiddleACCCON(Value);
		return;
	}

	if (((Address & ~0x1f) == 0xfe80) && (MachineType != Model::Master128) && NativeFDC) {
		Disc8271Write((Address & 7), Value);
		return;
	}

	//Rob: add econet
	if (Address >= 0xfea0 && Address < 0xfebf && EconetEnabled) {
		WriteEconetRegister((Address & 3), Value);
		return;
	}

	if ((Address & ~0x1f) == 0xfec0 && MachineType != Model::Master128) {
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
		TeletextWrite((Address & 0x3),Value);
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

	Music5000Write(Address, Value);
	
	Hog1MPaulaWrite(Address, Value);


	if ((Address & ~0x3)==0xfdf0) {
		SASIWrite((Address & 0x3),Value);
		return;
	}

	if (MachineType != Model::Master128 && Address == EDCAddr && !NativeFDC) {
		mainWin->SetDriveControl(Value);
	}
	if (MachineType != Model::Master128 && Address >= EFDCAddr && Address < (EFDCAddr + 4) && !NativeFDC) {
		Write1770Register(Address-EFDCAddr,Value);
	}
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

	// Some ROMs use bit 7 of this as an enabled flag. DNFS uses bit 7 for the DFS
	// half and bit 6 for the NFS half.
	// On Master 128 this is done with bit 7 and bit 6 with %00 (pointing to low
	// memory) and %11 (pointing to FSRAM) being active and %01 (pointing to
	// screen memory) and %10 (pointing to SROM) being inactive.
	if (MachineType != Model::Master128)
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
		RomWritable[bank] = false;
		RomBankType[bank] = BankType::Empty;
		memset(Roms[bank], 0, 0x4000);
	}

	// Read OS ROM
	RomName = RomConfig[static_cast<int>(MachineType)][0];
	strcpy(fullname,RomName);
	if ((RomName[0]!='\\') && (RomName[1]!=':')) {
		strcpy(fullname,RomPath);
		strcat(fullname,"BeebFile/");
		strcat(fullname,RomName);
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
		RomName = RomConfig[static_cast<int>(MachineType)][16-bank];
		strcpy(fullname,RomName);
		if ((RomName[0]!='\\') && (RomName[1]!=':')) {
			strcpy(fullname,RomPath);
			strcat(fullname,"BeebFile/");
			strcat(fullname,RomName);
		}

		if (strcmp(RomName,BANK_EMPTY)==0)
		{
			RomBankType[bank] = BankType::Empty;
			RomWritable[bank] = false;
		}
		else if (strcmp(RomName,BANK_RAM)==0)
		{
			RomBankType[bank] = BankType::Ram;
			RomWritable[bank] = true;
		}
		else
		{
			if (strncmp(RomName+(strlen(RomName)-4),ROM_WRITABLE,4)==0)
			{
				// Writable ROM
				RomBankType[bank] = BankType::Ram;
				RomWritable[bank] = true;
				fullname[strlen(fullname)-4]=0;
			}
			else
			{
				RomBankType[bank] = BankType::Rom;
				RomWritable[bank] = false;
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
void RTCReset(void) {
	Hidden[0xb] &= 0x87; /* clear bits in register B */
	Hidden[0xc] = 0;
}
/*----------------------------------------------------------------------------*/
void BeebMemInit(bool LoadRoms, bool SkipIntegraBConfig) {
  // Reset everything
  memset(WholeRam,0,0x8000);
  memset(FSRam,0,0x2000);
  memset(ShadowRAM,0,0x8000);
  memset(PrivateRAM,0,0x1000);
  ACCCON = 0;
  Sh_Display = false;
  FRAM = false;
  PRAM = false;
  Sh_CPUE = false;
  Sh_CPUX = false;
  memset(Private,0,0x3000);
  Private[0x3b2] = 0x04; // Default OSMODE to 4
  Private[0x3b5] = 0x14; // Default Century to 2000
  Private[0x3b8] = 0xFF; // Default
  Private[0x3b9] = 0xFF; // Default
  Private[0x3ba] = 0x90; // Default
  Private[0x3ff] = 0x0F; // Default RAM in bank locations 4, 5, 6 & 7
  memset(ShadowRam,0,0x5000);
  MemSel = PrvEn = ShEn = Prvs1 = Prvs4 = Prvs8 = false;
  HidAdd = 0;

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
}

/*-------------------------------------------------------------------------*/
void SaveMemUEF(FILE *SUEF) {
	int bank;
	switch (MachineType) {
	case Model::B:
	case Model::Master128:
		fput16(0x0461,SUEF); // Memory Control State
		fput32(2,SUEF);
		fputc(PagedRomReg,SUEF);
		fputc(ACCCON,SUEF);
		break;

	case Model::IntegraB:
		fput16(0x0461,SUEF); // Memory Control State
		fput32(3,SUEF);
		fputc(PagedRomReg | (static_cast<int>(MemSel) << 7) | (static_cast<int>(PrvEn) << 6), SUEF);
		fputc((static_cast<int>(ShEn) << 7) | (static_cast<int>(Prvs8) << 6) |
		      (static_cast<int>(Prvs4) << 5) | (static_cast<int>(Prvs1) << 4), SUEF);
		fputc(HidAdd,SUEF);
		break;

	case Model::BPlus:
		fput16(0x0461,SUEF); // Memory Control State
		fput32(2,SUEF);
		fputc(PagedRomReg | (static_cast<int>(MemSel) << 7), SUEF);
		fputc((static_cast<int>(Sh_Display) << 7), SUEF);
		break;
	}

	fput16(0x0462,SUEF); // Main Memory
	fput32(32768,SUEF);
	fwrite(WholeRam,1,32768,SUEF);

	switch (MachineType) {
	case Model::IntegraB:
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

	case Model::BPlus:
		fput16(0x0463,SUEF); // Shadow RAM
		fput32(32768,SUEF);
		fwrite(ShadowRAM,1,32768,SUEF);
		fput16(0x0464,SUEF); // Private RAM
		fput32(12288,SUEF);
		fwrite(Private,1,12288,SUEF);
		break;

	case Model::Master128:
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
		case BankType::Ram:
			fput16(0x0466,SUEF); // RAM bank
			fput32(16385,SUEF);
			fputc(bank,SUEF);
			fwrite(Roms[bank],1,16384,SUEF);
			break;
		case BankType::Rom:
			fput16(0x0475,SUEF); // ROM bank
			fput32(16386,SUEF);
			fputc(bank,SUEF);
			fputc(static_cast<int>(BankType::Rom),SUEF);
			fwrite(Roms[bank],1,16384,SUEF);
			break;
		case BankType::Empty:
			fput16(0x0475,SUEF); // ROM bank
			fput32(2,SUEF);
			fputc(bank,SUEF);
			fputc(static_cast<int>(BankType::Empty),SUEF);
			break;
		}
	}

}

void LoadRomRegsUEF(FILE *SUEF) {
	PagedRomReg=fgetc(SUEF);
	ROMSEL=PagedRomReg & 0xf;
	ACCCON=fgetc(SUEF);
	switch (MachineType) {
	case Model::IntegraB:
		MemSel = (PagedRomReg & 0x80) != 0;
		PrvEn = (PagedRomReg & 0x40) != 0;
		PagedRomReg &= 0xf;
		ShEn = (ACCCON & 0x80) != 0;
		Prvs8 = (ACCCON & 0x40) != 0;
		Prvs4 = (ACCCON & 0x20) != 0;
		Prvs1 = (ACCCON & 0x10) != 0;
		HidAdd = fgetc(SUEF) != 0;
		break;

	case Model::BPlus:
		MemSel = (PagedRomReg & 0x80) != 0;
		PagedRomReg &= 0xf;
		Sh_Display = (ACCCON & 0x80) != 0;
		break;

	case Model::Master128:
		PRAM = (PagedRomReg & 0x80) != 0;
		Sh_Display = (ACCCON & 1) != 0;
		Sh_CPUX = (ACCCON & 4) != 0;
		Sh_CPUE = (ACCCON & 2) != 0;
		FRAM = (ACCCON & 8) != 0;
		break;
	}
}

void LoadMainMemUEF(FILE *SUEF) {
	fread(WholeRam,1,32768,SUEF);
}

void LoadShadMemUEF(FILE *SUEF) {
	int SAddr;
	switch (MachineType) {
	case Model::IntegraB:
		fread(ShadowRam,1,20480,SUEF);
		break;
	case Model::BPlus:
		fread(ShadowRAM,1,32768,SUEF);
		break;
	case Model::Master128:
		SAddr=fget16(SUEF);
		fread(ShadowRAM+SAddr,1,32768,SUEF);
		break;
	}
}

void LoadPrivMemUEF(FILE *SUEF) {
	switch (MachineType) {
	case Model::IntegraB:
		fread(Private,1,12288,SUEF);
		break;
	case Model::BPlus:
		fread(Private,1,12288,SUEF);
		break;
	case Model::Master128:
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
	RomWritable[Rom] = true;
	RomBankType[Rom] = BankType::Ram;
	fread(Roms[Rom],1,16384,SUEF);
}
void LoadSWRomMemUEF(FILE *SUEF) {
	int Rom;
	Rom=fgetc(SUEF);
	RomBankType[Rom] = static_cast<BankType>(fgetc(SUEF));
	switch (RomBankType[Rom])
	{
	case BankType::Rom:
		RomWritable[Rom] = false;
		fread(Roms[Rom],1,16384,SUEF);
		break;
	case BankType::Empty:
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
  }

  fwrite(WholeRam,1,16384,bottom);
  fwrite(WholeRam+16384,1,16384,top);
  fclose(bottom);
  fclose(top);
}
