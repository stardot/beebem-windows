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

#include <windows.h>

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <fstream>
#include <iostream>

#include "BeebMem.h"
#include "6502core.h"
#include "AtoDConv.h"
#include "Debug.h" // Rob added for INTON/OFF reporting only
#include "Disc1770.h"
#include "Disc8271.h"
#include "Econet.h" // Rob
#include "IC32Latch.h"
#include "Ide.h"
#include "Main.h"
#include "Music5000.h"
#include "Sasi.h"
#include "Scsi.h"
#include "Serial.h"
#include "SysVia.h"
#include "Teletext.h"
#include "Tube.h"
#include "UefState.h"
#include "UserVia.h"
#include "Video.h"
#include "Z80mem.h"
#include "Z80.h"
#include "pal.h"

unsigned char WholeRam[65536];

constexpr int MAX_ROM_SIZE = 16384;

unsigned char Roms[16][MAX_ROM_SIZE];

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

unsigned char PagedRomReg;
unsigned char ROMSEL;

// Computech (&B+) Specific Stuff Added by K.Lowe 18/08/03
bool MemSel = false; // Shadow/Main RAM Toggle
bool PrvEn = false;  // Private RAM Enable
bool ShEn = false;   // Shadow RAM Enable
bool Prvs1 = false;  // Private RAM 1K Area
bool Prvs4 = false;  // Private RAM 4K Area
bool Prvs8 = false;  // Private RAM 8K Area
static int HidAdd = 0;

unsigned char Hidden[256];
unsigned char Private[12288];
unsigned char ShadowRam[20480];

static const unsigned char HiddenDefault[31] = {
	// Addresses 0x0 thru 0x9: RTC Data: Sec, SecAlm, Min, MinAlm, Hr, HrAlm, Day, Date, Month, Year
	0, 0, 0, 0, 0, 0, 2, 1, 1, 0,
	// Addresses 0xA thru 0xD: Registers A, B, C & D
	0xe0,
	0x8e,
	0,
	0,
	// Addresses 0xE thru 0x12: ?
	0, 0, 0, 0, 0,
	0xed, // Address 0x13: 0xED LANGUAGE in bank E. FILE SYSTEM in bank D. ROMS.cfg should match this, but IBOS reset will correct if wrong
	0xff, // Address 0x14: 0xFF *INSERT status for ROMS &0F to &08. Default: &FF (All 8 ROMS enabled)
	0xff, // Address 0x15: 0xFF *INSERT status for ROMS &07 to &00. Default: &FF (All 8 ROMS enabled)
	0x78,
	0,
	0x17, // Address 0x18: 0x17 0-2: MODE / 3: SHADOW / 4: TV Interlace / 5-7: TV screen shift.
	0x20, // Address 0x19: 0x20 0-2: FDRIVE / 3-5: CAPS. Default was &23. Changed to &20
	0x19, // Address 0x1A: 0x19 0-7: Keyboard Delay
	5,    // Address 0x1B: 0x05 0-7: Keyboard Repeat
	0x0a, // Address 0x1C: 0x0A 0-7: Printer Ignore
	0x2d, // Address 0x1D: 0x2D 0: Tube / 2-4: BAUD / 5-7: Printer
	0xa1  // Address 0x1E: 0xA1 0: File system / 4: Boot / 5-7: Data. Default was &A0. Changed to &A1
};
// End of Computech (&B+) Specific Stuff

/* Master 128 Specific Stuff */
unsigned char FSRam[8192];       // 8K Filing System RAM
unsigned char PrivateRAM[4096];  // 4K Private RAM (VDU Use mainly)
unsigned char ShadowRAM[32768];  // 20K Shadow RAM
unsigned char ACCCON;            // ACCess CONtrol register

bool Sh_Display;
static bool PrivateRAMSelect;
static bool FSRAMSelect;
static bool Sh_CPUX, Sh_CPUE;
/* End of Master 128 Specific Stuff, note initilised anyway regardless of Model Type in use */

/* ROM file data */
char RomPath[_MAX_PATH];
char RomFile[_MAX_PATH];
ROMConfigFile RomConfig;

// SCSI and IDE hard drive file location
char HardDrivePath[_MAX_PATH]; // JGH

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

  Address += offsets[(IC32State & IC32_SCREEN_ADDRESS) >> 4];
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

// Perform hardware address wrap around for mode 7.
//
// The beeb uses the 14-bit address generated by the 6845 in one of two
// different ways. If the top bit of the address (value 0x2000) is set then
// the beeb treats it as a mode 7 address, otherwise it treats it as a mode 0-6
// address. Note that this is independent of the teletext select bit in the
// video ULA.
//
// In mode 7 the 6845 is programmed with a start address between 0x2000 and
// 0x23ff to display data from 0x3C00 to 0x3fff or with a start address
// between 0x2800 and 0x2bff to display data from 0x7C00 to 0x7fff.
//
// This code handles wrapping at 1K by ignoring the 0x400 bit.
//
// If the 6845 is programmed with a start address of 0x2400 it accesses
// memory from 0x3c00 to 0x3fff then 0x7c00 to 0x7fff giving a 2K linear
// buffer.

static int WrapAddrMode7(int Address) {
	if (MachineType == Model::B || MachineType == Model::IntegraB) {
		return (Address & 0x800) << 3 | 0x3c00 | (Address & 0x3ff);
	}
	else {
		return 0x7c00 | (Address & 0x3ff);
	}
}

/*----------------------------------------------------------------------------*/

// Special case of BeebMemPtrWithWrap for use in mode 7

const unsigned char *BeebMemPtrWithWrapMode7(int Address, int Length) {
	static unsigned char tmpBuf[1024];

	const unsigned char *Memory = Sh_Display ? ShadowRAM : WholeRam;

	for (int i = 0; i < Length; i++, Address++) {
		tmpBuf[i] = Memory[WrapAddrMode7(Address)];
	}

	return tmpBuf;
}

/*----------------------------------------------------------------------------*/
unsigned char BeebReadMem(int Address) {
	unsigned char Value = 0xff;

	if (MachineType == Model::B) {
		if (Address >= 0x8000 && Address < 0xc000) {
			if (ERom[ROMSEL].type == PALRomType::none) {
				return Roms[ROMSEL][Address - 0x8000];
			} else {
				return ExtendedRom(ROMSEL, Address - 0x8000);
			}
		}
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
		if (Address >= 0x8000 && Address < 0xc000) {
			if (ERom[ROMSEL].type == PALRomType::none) {
				return Roms[ROMSEL][Address - 0x8000];
			}
			else {
				return ExtendedRom(ROMSEL, Address - 0x8000);
			}
		}
		if (Address < 0xfc00) return WholeRam[Address];
		if (Address >= 0xff00) return WholeRam[Address];

		if (Address==0xfe3c) {
			time_t long_time; // Clock for Computech Integra-B
			time(&long_time);
			struct tm* time = localtime(&long_time);

			if (HidAdd==0) return (unsigned char)time->tm_sec;
			if (HidAdd==2) return (unsigned char)time->tm_min;
			if (HidAdd==4) return (unsigned char)time->tm_hour;
			if (HidAdd==6) return (unsigned char)(time->tm_wday + 1);
			if (HidAdd==7) return (unsigned char)(time->tm_mday);
			if (HidAdd==8) return (unsigned char)(time->tm_mon + 1);
			if (HidAdd==9) return (unsigned char)(time->tm_year % 100);
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
		if (Address >= 0x8000 && Address < 0xc000) {
			if (ERom[ROMSEL].type == PALRomType::none) {
				return Roms[ROMSEL][Address - 0x8000];
			}
			else {
				return ExtendedRom(ROMSEL, Address - 0x8000);
			}

		}
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
			if (PrivateRAMSelect) {
				return(PrivateRAM[Address-0x8000]);
			} else {
				if (ERom[ROMSEL].type == PALRomType::none) {
					return Roms[ROMSEL][Address - 0x8000];
				}
				else {
					return ExtendedRom(ROMSEL, Address - 0x8000);
				}
			}
			break;
		case 9:
		case 0xa:
		case 0xb:
			if (ERom[ROMSEL].type == PALRomType::none) {
				return Roms[ROMSEL][Address - 0x8000];
			}
			else {
				return ExtendedRom(ROMSEL, Address - 0x8000);
			}

			break;
		case 0xc:
		case 0xd:
			if (FSRAMSelect)
			{
				return FSRam[Address - 0xc000];
			}
			else
			{
				return WholeRam[Address];
			}
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
		Value = SerialACIAReadStatus();
		AdjustForIORead();
		return Value;
	}

	if (Address==0xfe09) {
		SyncIO();
		Value = SerialACIAReadRxData();
		AdjustForIORead();
		return Value;
	}

	if (Address==0xfe10) {
		SyncIO();
		Value = SerialULARead();
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
		 (MachineType == Model::Master128 && (Address & ~3) == 0xfe38)))
	{
		if (DebugEnabled)
			DebugDisplayTrace(DebugType::Econet, true, "Econet: INTOFF");
		EconetNMIEnabled = INTOFF;
		return EconetReadStationID();
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
	     (MachineType == Model::Master128 && (Address & ~3) == 0xfe3c)))
	{
		if (DebugEnabled) DebugDisplayTrace(DebugType::Econet, true, "Econet: INTON");

		if (!EconetNMIEnabled) // was off
		{
			EconetNMIEnabled = INTON;  // turn on

			if (EconetInterruptRequest()) // irq pending?
			{
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
		return(PagedRomReg); // report back ROMSEL - I'm sure the beeb allows ROMSEL read..
		                     // correct me if im wrong. - Richard Gellman
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
			return EconetRead(Address & 3); /* Read 68B54 ADLC */
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
		if (TubeType == Tube::TorchZ80)
			return ReadTorchTubeFromHostSide(Address & 0x1f); // Read From Torch Tube
		else
			return ReadTubeFromHostSide(Address & 7); // Read From Tube
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

	// DB: M5000 will only return its fcff select register or
	// Jim mapped registers if it has been selected by writing
	// it's id value to fcff - other Jim devices should similarly
	// only return fcff..fcfc/fdxx when they have been selected
	unsigned char ret;
	if (Music5000Read(Address, &ret))
		return ret;

	if ((Address & ~0x3)==0xfdf0) {
		return(SASIRead(Address & 0x3));
	}

	if (MachineType != Model::Master128 && Address >= EFDCAddr && Address < (EFDCAddr+4) && !NativeFDC) {
		// mainWin->Report(MessageType::Error, "Read of 1770 Extension Board");
		return Read1770Register(Address - EFDCAddr);
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
	int m = 0;
	sram[0] = 0;

	DebugDisplayInfo("Memory state:");

	for (int i = 0; i < 16; i++)
	{
		if (RomWritable[i])
		{
			psram += sprintf(psram,"%d, ",i);
			m += 16;
		}
	}

	if (strlen(sram) > 0)
	{
		sram[strlen(sram) - 2] = '\0';
		DebugDisplayInfoF("%dK sideways RAM using bank %s", m, sram);
	}

	DebugDisplayInfoF("ROMSEL: %d", ROMSEL);

	switch (MachineType)
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

static void DoRomChange(unsigned char NewBank)
{
  ROMSEL = NewBank & 0xf;

  if (MachineType != Model::Master128) {
    NewBank&=0xf; // strip top bit if Model B
    PagedRomReg=NewBank;
    return;
  }

  // Master Specific stuff
  if (MachineType == Model::Master128) {
    PagedRomReg = NewBank;
    PrivateRAMSelect = (PagedRomReg & 0x80) != 0;
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
	FSRAMSelect = (ACCCON & 8) != 0;
}

/*----------------------------------------------------------------------------*/
static void RomWriteThrough(int Address, unsigned char Value) {
	int bank = 0;

	// SW RAM board - bank to write to is selected by User VIA
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
	if (bank < MAX_EROMS) ERom[bank].type = PALRomType::none;
}

/*----------------------------------------------------------------------------*/
void BeebWriteMem(int Address, unsigned char Value)
{
	if (MachineType == Model::B) {
		if (Address < 0x8000) {
			WholeRam[Address] = Value;
			return;
		}

		if (Address < 0xc000 && Address >= 0x8000) {
			if (!SWRAMBoardEnabled && RomWritable[ROMSEL]) Roms[ROMSEL][Address -0x8000] =Value;
			else RomWriteThrough(Address, Value);
			if (ROMSEL < MAX_EROMS) ERom[ROMSEL].type = PALRomType::none;
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
			// else RomWriteThrough(Address, Value); // Not supported on Integra-B
			if (ROMSEL < MAX_EROMS) ERom[ROMSEL].type = PALRomType::none;
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
			// TODO
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
			if (ROMSEL < MAX_EROMS) ERom[ROMSEL].type = PALRomType::none;
			return;
		}

		if (Address >= 0xfe30 && Address < 0xfe34) {
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
				if (PrivateRAMSelect)
				{
					PrivateRAM[Address - 0x8000] = Value;
				}
				else {
					if (RomWritable[ROMSEL]) Roms[ROMSEL][Address-0x8000]=Value;
					//else RomWriteThrough(Address, Value); //Not supported on Master
					if (ERom[ROMSEL].type != PALRomType::none) ERom[ROMSEL].type = PALRomType::none;
				}
				break;
			case 9:
			case 0xa:
			case 0xb:
				if (RomWritable[ROMSEL]) Roms[ROMSEL][Address-0x8000]=Value;
				//else RomWriteThrough(Address, Value); //Not supported on Master
				if (ROMSEL < MAX_EROMS) ERom[ROMSEL].type = PALRomType::none;
				break;
			case 0xc:
			case 0xd:
				if (FSRAMSelect)
				{
					FSRam[Address - 0xc000] = Value;
				}
				break;
			}
			return;
		}
	}

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
		SerialACIAWriteControl(Value);
		return;
	}

	if (Address==0xfe09) {
		SyncIO();
		AdjustForIOWrite();
		SerialACIAWriteTxData(Value);
		return;
	}

	if (Address==0xfe10) {
		SyncIO();
		AdjustForIOWrite();
		SerialULAWrite(Value);
		return;
	}

	//Rob: econet NMI mask
	if (EconetEnabled &&
		((MachineType != Model::Master128 && (Address & ~3) == 0xfe18) ||
		 (MachineType == Model::Master128 && (Address & ~3) == 0xfe38)) ) {
		if (DebugEnabled)
			DebugDisplayTrace(DebugType::Econet, true, "Econet: INTOFF(w)");
		EconetNMIEnabled = INTOFF;
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
		EconetWrite((Address & 3), Value);
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
		if (TubeType == Tube::TorchZ80)
			WriteTorchTubeFromHostSide(Address & 0xf, Value);
		else
			WriteTubeFromHostSide(Address & 7, Value);
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

static bool IsValidROMString(const unsigned char* s)
{
	int Count = 0;
	int Total = 0;

	for (; *s != '\0'; s++)
	{
		Count += isprint(*s) ? 1 : 0;
		Total++;
	}

	if (Total == 0)
	{
		return true;
	}
	else
	{
		return ((double)Count / (double)Total) > 0.5;
	}
}

bool ReadRomInfo(int bank, RomInfo* info)
{
	if((RomFlags)Roms[bank][6] == 0)
		return false;

	info->Slot = bank;

	// LanguageAddr and ServiceAddr are really 6502 instructions. Most ROMs obey the JMP convention as
	// described in the AUG, however BASIC fills the first 6 bytes with CMP 1; BEQ 1F; RTS; NOP
	// which seems to simply be a check for whether it was entered properly.
	info->ServiceAddr = info->LanguageAddr = info->RelocationAddr = 0;
	if(Roms[bank][0] == 0x4C)
		info->LanguageAddr = (Roms[bank][2] << 8) | Roms[bank][1];
	if(Roms[bank][3] == 0x4C)
		info->ServiceAddr = (Roms[bank][5] << 8) | Roms[bank][4];

	// TODO: Flags0-3 specify instruction type, see master reference manual part 1 p257.
	info->Flags = (RomFlags)Roms[bank][6];

	info->VersionStr[0] = '\0';
	info->Version = Roms[bank][8];

	strncpy(info->Title, (char*)&Roms[bank][9], MAX_ROMINFO_LENGTH);
	info->Title[MAX_ROMINFO_LENGTH] = '\0';

	if (!IsValidROMString((unsigned char*)info->Title))
	{
		return false;
	}

	if (strlen(info->Title) + 9 != Roms[bank][7])
	{
		strncpy(info->VersionStr, (char*)&Roms[bank][strlen(info->Title) + 10], MAX_ROMINFO_LENGTH);
		info->VersionStr[MAX_ROMINFO_LENGTH] = '\0';
	}

	strncpy(info->Copyright, (char*)(Roms[bank] + Roms[bank][7] + 1), MAX_ROMINFO_LENGTH);
	info->Copyright[MAX_ROMINFO_LENGTH] = '\0';

	if (info->Flags & RomRelocate)
	{
		int addr = Roms[bank][7] + (int)strlen(info->Copyright) + 2;
		info->RelocationAddr = (Roms[bank][addr + 3] << 24) | (Roms[bank][addr + 2] << 16) | (Roms[bank][addr + 1] << 8) | Roms[bank][addr];
	}

	if (!(info->Flags & RomService))
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

char *ReadRomTitle(int bank, char *Title, int BufSize)
{
	int i;

	// Copy the ROM Title to the Buffer
	for (i = 0; i < (BufSize - 1) && Roms[bank][i + 9] > 30; i++)
	{
		Title[i] = Roms[bank][i + 9];
	}

	// Add terminating NULL.
	Title[i] = '\0';

	return Title;
}

/*----------------------------------------------------------------------------*/
bool ReadROMFile(const char *filename, ROMConfigFile ROMConfig)
{
	bool success = true;
	char line[MAX_PATH];

	FILE *fd = fopen(filename, "r");
	if (!fd)
	{
		mainWin->Report(MessageType::Error,
		                "Cannot open ROM configuration file:\n  %s", filename);

		return false;
	}

	for (int model = 0; model < 4 && success; ++model)
	{
		for (int bank = 0; bank < 17 && success; ++bank)
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
		mainWin->Report(MessageType::Error,
		                "Invalid ROM configuration file:\n  %s", filename);

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

	// Clear ROMs
	for (bank = 0; bank < 16; bank++)
	{
		RomWritable[bank] = false;
		RomBankType[bank] = BankType::Empty;
		memset(Roms[bank], 0, sizeof(Roms[bank]));
		memset(ERom[bank].rom, 0, sizeof(ERom[bank].rom));
		ERom[bank].type = PALRomType::none;
		ERom[bank].m_bank = 0;
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
		fread(WholeRam + 0xc000, 1, MAX_ROM_SIZE, InFile);
		fclose(InFile);
		// Try to read OS ROM memory map:
		if((extension = strrchr(fullname, '.')) != NULL)
			*extension = 0;
		strncat(fullname, ".map", _MAX_PATH);
		DebugLoadMemoryMap(fullname, 16);
	}
	else {
		mainWin->Report(MessageType::Error,
		                "Cannot open specified OS ROM:\n %s", fullname);
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

			InFile = fopen(fullname, "rb");

			if (InFile != nullptr)
			{
				fseek(InFile, 0, SEEK_END);
				long Size = ftell(InFile);
				fseek(InFile, 0, SEEK_SET);

				if (Size <= MAX_PALROM_SIZE)
				{
					// Read ROM:
					fread(Roms[bank], 1, MAX_ROM_SIZE, InFile);

					// Read PAL ROM:
					fseek(InFile, 0L, SEEK_SET);
					fread(ERom[bank].rom, 1, Size, InFile);
					GuessRomType(bank, Size);

					fclose(InFile);

					// Try to read ROM memory map:
					if((extension = strrchr(fullname, '.')) != NULL)
						*extension = 0;
					strncat(fullname, ".map", _MAX_PATH);
					DebugLoadMemoryMap(fullname, bank);
				}
				else
				{
					mainWin->Report(MessageType::Error,
					                "ROM file too large:\n %s", fullname);
				}
			}
			else {
				mainWin->Report(MessageType::Error,
				                "Cannot open specified ROM:\n %s", fullname);
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
  FSRAMSelect = false;
  PrivateRAMSelect = false;
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
  memcpy(WholeRam + 0x8000, Roms[0xf], MAX_ROM_SIZE);
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
			fwrite(Roms[bank], 1, MAX_ROM_SIZE, SUEF);
			break;
		case BankType::Rom:
			fput16(0x0475,SUEF); // ROM bank
			fput32(16386,SUEF);
			fputc(bank,SUEF);
			fputc(static_cast<int>(BankType::Rom),SUEF);
			fwrite(Roms[bank], 1, MAX_ROM_SIZE, SUEF);
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
	PagedRomReg = fget8(SUEF);
	ROMSEL = PagedRomReg & 0xf;
	ACCCON = fget8(SUEF);
	switch (MachineType) {
	case Model::IntegraB:
		MemSel = (PagedRomReg & 0x80) != 0;
		PrvEn = (PagedRomReg & 0x40) != 0;
		PagedRomReg &= 0xf;
		ShEn = (ACCCON & 0x80) != 0;
		Prvs8 = (ACCCON & 0x40) != 0;
		Prvs4 = (ACCCON & 0x20) != 0;
		Prvs1 = (ACCCON & 0x10) != 0;
		HidAdd = fgetbool(SUEF);
		break;

	case Model::BPlus:
		MemSel = (PagedRomReg & 0x80) != 0;
		PagedRomReg &= 0xf;
		Sh_Display = (ACCCON & 0x80) != 0;
		break;

	case Model::Master128:
		PrivateRAMSelect = (PagedRomReg & 0x80) != 0;
		Sh_Display = (ACCCON & 1) != 0;
		Sh_CPUX = (ACCCON & 4) != 0;
		Sh_CPUE = (ACCCON & 2) != 0;
		FSRAMSelect = (ACCCON & 8) != 0;
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
	fread(Roms[Rom], 1, MAX_ROM_SIZE, SUEF);
}

void LoadSWRomMemUEF(FILE *SUEF) {
	int Rom;
	Rom=fgetc(SUEF);
	RomBankType[Rom] = static_cast<BankType>(fgetc(SUEF));
	switch (RomBankType[Rom])
	{
	case BankType::Rom:
		RomWritable[Rom] = false;
		fread(Roms[Rom], 1, MAX_ROM_SIZE, SUEF);
		break;
	case BankType::Empty:
		memset(Roms[Rom], 0, MAX_ROM_SIZE);
		break;
	}
}

