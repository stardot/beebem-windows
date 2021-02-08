/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 1994  David Alan Gilbert
Copyright (C) 1994  Nigel Magnay
Copyright (C) 1997  Mike Wyatt
Copyright (C) 2001  Richard Gellman
Copyright (C) 2008  Rich Talbot-Watkins

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

#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <stdio.h>

#include "6502core.h"
#include "beebmem.h"
#include "beebwin.h"
#include "main.h"
#include "sysvia.h"
#include "video.h"
#include "uefstate.h"
#include "beebsound.h"
#include "debug.h"
#include "teletext.h"

#ifdef BEEB_DOTIME
#include <sys/times.h>
#ifdef SUNOS
#include <sys/param.h>
#endif
#ifdef HPUX
#include <unistd.h>
#endif
#endif

using namespace std;

/* Bit assignments in control reg:
   0 - Flash colour (0=first colour, 1=second)
   1 - Teletext select (0=on chip serialiser, 1=teletext)
 2,3 - Bytes per line (2,3=1,1 is 80, 1,0=40, 0,1=20, 0,0=10)
   4 - CRTC Clock chip select (0 = low frequency, 1= high frequency)
 5,6 - Cursor width in bytes (0,0 = 1 byte, 0,1=not defined, 1,0=2, 1,1=4)
   7 - Master cursor width (if set causes large cursor)
*/
EightUChars FastTable[256];
SixteenUChars FastTableDWidth[256]; /* For mode 4,5,6 */
bool FastTable_Valid = false;

typedef void (*LineRoutinePtr)();
LineRoutinePtr LineRoutine;

// Translates middle bits of VideoULA_ControlReg to number of colours
static const int NColsLookup[] = {
	16, 4, 2, 0 /* Not supported 16? */, 0, 16, 4, 2 // Based on AUG 379
};

unsigned char VideoULA_ControlReg=0x9c;
unsigned char VideoULA_Palette[16];

unsigned char CRTCControlReg=0;
unsigned char CRTC_HorizontalTotal=127;     /* R0 */
unsigned char CRTC_HorizontalDisplayed=80;  /* R1 */
unsigned char CRTC_HorizontalSyncPos=98;    /* R2 */
unsigned char CRTC_SyncWidth=0x28;          /* R3 - top 4 bits are Vertical (in scan lines) and bottom 4 are horizontal in characters */
unsigned char CRTC_VerticalTotal=38;        /* R4 */
unsigned char CRTC_VerticalTotalAdjust=0;   /* R5 */
unsigned char CRTC_VerticalDisplayed=32;    /* R6 */
unsigned char CRTC_VerticalSyncPos=34;      /* R7 */
unsigned char CRTC_InterlaceAndDelay=0;     /* R8 - 0,1 are interlace modes, 4,5 display blanking delay, 6,7 cursor blanking delay */
unsigned char CRTC_ScanLinesPerChar=7;      /* R9 */
unsigned char CRTC_CursorStart=0;           /* R10 */
unsigned char CRTC_CursorEnd=0;             /* R11 */
unsigned char CRTC_ScreenStartHigh=6;       /* R12 */
unsigned char CRTC_ScreenStartLow=0;        /* R13 */
unsigned char CRTC_CursorPosHigh=0;         /* R14 */
unsigned char CRTC_CursorPosLow=0;          /* R15 */
unsigned char CRTC_LightPenHigh=0;          /* R16 */
unsigned char CRTC_LightPenLow=0;           /* R17 */

unsigned int ActualScreenWidth=640;
long ScreenAdjust=0; // Mode 7 Defaults.
long VScreenAdjust=0;
unsigned char HSyncModifier=9;
bool TeletextEnabled = false;
char TeletextStyle = 1; // Defines whether teletext will skip intermediate lines in order to speed up
bool TeletextHalfMode = false; // set true to use half-mode (TeletextStyle=1 all the time)
int CurY=-1;
FILE *crtclog;

int ova,ovn; // mem ptr buffers

/* CharLine counts from the 'reference point' - i.e. the point at which we reset the address pointer - NOT
the point of the sync. If it is -ve its actually in the adjust time */
struct VideoStateT {
  int Addr;       /* Address of start of next visible character line in beeb memory  - raw */
  int StartAddr;  /* Address of start of first character line in beeb memory  - raw */
  int PixmapLine; /* Current line in the pixmap */
  int FirstPixmapLine; /* The first pixmap line where something is visible.  Used to eliminate the
                          blank vertical retrace lines at the top of the screen. */
  int PreviousFirstPixmapLine; /* The first pixmap line on the previous frame */
  int LastPixmapLine; /* The last pixmap line where something is visible.  Used to eliminate the
                          blank vertical retrace lines at the bottom of the screen. */
  int PreviousLastPixmapLine; /* The last pixmap line on the previous frame */
  bool IsTeletext; /* This frame is a teletext frame - do things differently */
  const unsigned char *DataPtr;  /* Pointer into host memory of video data */

  int CharLine;   /* 6845 counts in characters vertically - 0 at the top , incs by 1 - -1 means we are in the bit before the actual display starts */
  int InCharLineUp; /* Scanline within a character line - counts up*/
  int VSyncState; // Cannot =0 in MSVC $NRM; /* When >0 VSync is high */
  bool IsNewTVFrame; // Specifies the start of a new TV frame, following VSync (used so we only calibrate speed once per frame)
  bool InterlaceFrame;
  bool DoCA1Int;
};

static VideoStateT VideoState;

int VideoTriggerCount=9999; /* Number of cycles before next scanline service */

// First subscript is graphics flag (1 for graphics, 2 for separated graphics),
// next is character, then scanline
// character is (value & 127) - 32
// There are 20 rows, to account for "half pixels"
static unsigned int Mode7Font[3][96][20];

static bool Mode7FlashOn = true; // true if a flashing character in mode 7 is on
static bool Mode7DoubleHeightFlags[80]; // Pessimistic size for this flags - if true then corresponding character on NEXT line is top half
static bool CurrentLineBottom = false;
static bool NextLineBottom = false; // true if the next line of double height should be bottoms only

/* Flash every half second(?) i.e. 25 x 50Hz fields */
// No. On time is longer than off time. - according to my datasheet, its 0.75Hz with 3:1 ON:OFF ratio. - Richard Gellman
// cant see that myself.. i think it means on for 0.75 secs, off for 0.25 secs
#define MODE7FLASHFREQUENCY 25
#define MODE7ONFIELDS 37
#define MODE7OFFFIELDS 13

int CursorFieldCount = 32;
bool CursorOnState = true;
int Mode7FlashTrigger=MODE7ONFIELDS;

/* If 1 then refresh on every display, else refresh every n'th display */
int Video_RefreshFrequency=1;
/* The number of the current frame - starts at Video_RefreshFrequency - at 0 actually refresh */
static int FrameNum=0;

static void LowLevelDoScanLineNarrow();
static void LowLevelDoScanLineWide();
static void LowLevelDoScanLineNarrowNot4Bytes();
static void LowLevelDoScanLineWideNot4Bytes();
static void VideoAddCursor(void);
void AdjustVideo();
void VideoAddLEDs(void);

/*-------------------------------------------------------------------------------------------------------------*/

// Build enhanced mode 7 font

static void BuildMode7Font()
{
	char FileName[256];
	strcpy(FileName, mainWin->GetAppPath());
	strcat(FileName, "teletext.fnt");

	FILE *TeletextFontFile = fopen(FileName, "rb");

	if (TeletextFontFile == nullptr)
	{
		char errstr[200];
		sprintf(errstr, "Cannot open Teletext font file teletext.fnt");
		MessageBox(GETHWND, errstr, WindowTitle, MB_OK | MB_ICONERROR);
		exit(1);
	}

	for (int Character = 32; Character <= 127; Character++)
	{
		// The first two lines of each character are blank.
		Mode7Font[0][Character - 32][0] = 0;
		Mode7Font[0][Character - 32][1] = 0;
		Mode7Font[1][Character - 32][0] = 0;
		Mode7Font[1][Character - 32][1] = 0;
		Mode7Font[2][Character - 32][0] = 0;
		Mode7Font[2][Character - 32][1] = 0;

		// Read 18 lines of 16 pixels each from the file.
		for (int y = 2; y < 20; y++)
		{
			unsigned int Bitmap =  fget16(TeletextFontFile);

			Mode7Font[0][Character - 32][y] = Bitmap << 2; // Text bank
			Mode7Font[1][Character - 32][y] = Bitmap << 2; // Contiguous graphics bank
			Mode7Font[2][Character - 32][y] = Bitmap << 2; // Separated graphics bank
		}
	}

	fclose(TeletextFontFile);

	// Now fill in the graphics - this is built from an algorithm, but has certain
	// lines/columns blanked for separated graphics.

	for (int Character = 0; Character < 96; Character++)
	{
		// Here's how it works:
		// - top two blocks: 1 & 2
		// - middle two blocks: 4 & 8
		// - bottom two blocks: 16 & 64
		// - its only a graphics character if bit 5 (32) is clear

		if ((Character & 32) == 0)
		{
			// Row builders for mode 7 sixel graphics
			int row1 = 0;
			int row2 = 0;
			int row3 = 0;

			// Left sixel has a value of 0xfc0, right 0x03f and both 0xfff
			if (Character & 0x01) row1 |= 0xfc0; // 1111 1100 0000
			if (Character & 0x02) row1 |= 0x03f; // 0000 0011 1111
			if (Character & 0x04) row2 |= 0xfc0;
			if (Character & 0x08) row2 |= 0x03f;
			if (Character & 0x10) row3 |= 0xfc0;
			if (Character & 0x40) row3 |= 0x03f;

			// Now input these values into the array

			// Top row of sixel - continuous
			Mode7Font[1][Character][0]  = row1;
			Mode7Font[1][Character][1]  = row1;
			Mode7Font[1][Character][2]  = row1;
			Mode7Font[1][Character][3]  = row1;
			Mode7Font[1][Character][4]  = row1;
			Mode7Font[1][Character][5]  = row1;

			// Middle row of sixel - continuous
			Mode7Font[1][Character][6]  = row2;
			Mode7Font[1][Character][7]  = row2;
			Mode7Font[1][Character][8]  = row2;
			Mode7Font[1][Character][9]  = row2;
			Mode7Font[1][Character][10] = row2;
			Mode7Font[1][Character][11] = row2;
			Mode7Font[1][Character][12] = row2;
			Mode7Font[1][Character][13] = row2;

			// Bottom row of sixel - continuous
			Mode7Font[1][Character][14] = row3;
			Mode7Font[1][Character][15] = row3;
			Mode7Font[1][Character][16] = row3;
			Mode7Font[1][Character][17] = row3;
			Mode7Font[1][Character][18] = row3;
			Mode7Font[1][Character][19] = row3;

			// Separated - insert gaps 0011 1100 1111
			row1 &= 0x3cf;
			row2 &= 0x3cf;
			row3 &= 0x3cf;

			// Top row of sixel - separated
			Mode7Font[2][Character][0] = row1;
			Mode7Font[2][Character][1] = row1;
			Mode7Font[2][Character][2] = row1;
			Mode7Font[2][Character][3] = row1;
			Mode7Font[2][Character][4] = 0;
			Mode7Font[2][Character][5] = 0;

			// Middle row of sixel - separated
			Mode7Font[2][Character][6]  = row2;
			Mode7Font[2][Character][7]  = row2;
			Mode7Font[2][Character][8]  = row2;
			Mode7Font[2][Character][9]  = row2;
			Mode7Font[2][Character][10] = row2;
			Mode7Font[2][Character][11] = row2;
			Mode7Font[2][Character][12] = 0;
			Mode7Font[2][Character][13] = 0;

			// Bottom row of sixel - separated
			Mode7Font[2][Character][14] = row3;
			Mode7Font[2][Character][15] = row3;
			Mode7Font[2][Character][16] = row3;
			Mode7Font[2][Character][17] = row3;
			Mode7Font[2][Character][18] = 0;
			Mode7Font[2][Character][19] = 0;
		}
	}
}

/*-------------------------------------------------------------------------------------------------------------*/
static void DoFastTable16() {
  for(unsigned int beebpixvl = 0; beebpixvl < 16; beebpixvl++) {
    unsigned int bplvopen = ((beebpixvl & 8) ? 128 : 0) |
                            ((beebpixvl & 4) ? 32 : 0) |
                            ((beebpixvl & 2) ? 8 : 0) |
                            ((beebpixvl & 1) ? 2 : 0);

    for (unsigned int beebpixvr = 0; beebpixvr < 16; beebpixvr++) {
      unsigned int bplvtotal = bplvopen |
                               ((beebpixvr & 8) ? 64 : 0) |
                               ((beebpixvr & 4) ? 16 : 0) |
                               ((beebpixvr & 2) ? 4 : 0) |
                               ((beebpixvr & 1) ? 1 : 0);

      unsigned char tmp = VideoULA_Palette[beebpixvl];

      if (tmp > 7) {
        tmp &= 7;
        if (VideoULA_ControlReg & 1) tmp ^= 7;
      }

      FastTable[bplvtotal].data[0] =
        FastTable[bplvtotal].data[1] =
        FastTable[bplvtotal].data[2] =
        FastTable[bplvtotal].data[3] = tmp;

      tmp = VideoULA_Palette[beebpixvr];

      if (tmp > 7) {
        tmp &= 7;
        if (VideoULA_ControlReg & 1) tmp ^= 7;
      }

      FastTable[bplvtotal].data[4] =
        FastTable[bplvtotal].data[5] =
        FastTable[bplvtotal].data[6] =
        FastTable[bplvtotal].data[7] = tmp;
    }
  }
}

/*-------------------------------------------------------------------------------------------------------------*/
static void DoFastTable16XStep8() {
  for (unsigned int beebpixvl = 0; beebpixvl < 16; beebpixvl++) {
    unsigned int bplvopen = ((beebpixvl & 8) ? 128 : 0) |
                            ((beebpixvl & 4) ? 32 : 0) |
                            ((beebpixvl & 2) ? 8 : 0) |
                            ((beebpixvl & 1) ? 2 : 0);

    for (unsigned int beebpixvr = 0;beebpixvr < 16; beebpixvr++) {
      unsigned int bplvtotal = bplvopen |
                               ((beebpixvr & 8) ? 64 : 0) |
                               ((beebpixvr & 4) ? 16 : 0) |
                               ((beebpixvr & 2) ? 4 : 0) |
                               ((beebpixvr & 1) ? 1 : 0);

      unsigned char tmp = VideoULA_Palette[beebpixvl];

      if (tmp > 7) {
        tmp &= 7;
        if (VideoULA_ControlReg & 1) tmp ^= 7;
      }

      FastTableDWidth[bplvtotal].data[0] =
        FastTableDWidth[bplvtotal].data[1] =
        FastTableDWidth[bplvtotal].data[2] =
        FastTableDWidth[bplvtotal].data[3] =
        FastTableDWidth[bplvtotal].data[4] =
        FastTableDWidth[bplvtotal].data[5] =
        FastTableDWidth[bplvtotal].data[6] =
        FastTableDWidth[bplvtotal].data[7] = tmp;

      tmp = VideoULA_Palette[beebpixvr];

      if (tmp > 7) {
        tmp &= 7;
        if (VideoULA_ControlReg & 1) tmp ^= 7;
      }

      FastTableDWidth[bplvtotal].data[8] =
        FastTableDWidth[bplvtotal].data[9] =
        FastTableDWidth[bplvtotal].data[10] =
        FastTableDWidth[bplvtotal].data[11] =
        FastTableDWidth[bplvtotal].data[12] =
        FastTableDWidth[bplvtotal].data[13] =
        FastTableDWidth[bplvtotal].data[14] =
        FastTableDWidth[bplvtotal].data[15] = tmp;
    }
  }
}

/*-------------------------------------------------------------------------------------------------------------*/
/* Some guess work and experimentation has determined that the left most pixel uses bits 7,5,3,1 for the       */
/* palette address, the next uses 6,4,2,0, the next uses 5,3,1,H (H=High), then 5,2,0,H                        */
static void DoFastTable4() {
  for (unsigned long beebpixv = 0; beebpixv < 256; beebpixv++) {
    unsigned long pentry = ((beebpixv & 128) ? 8 : 0) |
                           ((beebpixv & 32)  ? 4 : 0) |
                           ((beebpixv & 8)   ? 2 : 0) |
                           ((beebpixv & 2)   ? 1 : 0);

    unsigned char tmp = VideoULA_Palette[pentry];

    if (tmp>7) {
      tmp&=7;
      if (VideoULA_ControlReg & 1) tmp^=7;
    }

    FastTable[beebpixv].data[0] =
      FastTable[beebpixv].data[1] = tmp;

    pentry = ((beebpixv & 64) ? 8 : 0) |
             ((beebpixv & 16) ? 4 : 0) |
             ((beebpixv & 4)  ? 2 : 0) |
             ((beebpixv & 1)  ? 1 : 0);

    tmp=VideoULA_Palette[pentry];

    if (tmp>7) {
      tmp&=7;
      if (VideoULA_ControlReg & 1) tmp^=7;
    }

    FastTable[beebpixv].data[2] =
      FastTable[beebpixv].data[3] = tmp;

    pentry = ((beebpixv & 32) ? 8 : 0) |
             ((beebpixv & 8)  ? 4 : 0) |
             ((beebpixv & 2)  ? 2 : 0) |
             1;

    tmp = VideoULA_Palette[pentry];

    if (tmp>7) {
      tmp&=7;
      if (VideoULA_ControlReg & 1) tmp^=7;
    }

    FastTable[beebpixv].data[4] =
      FastTable[beebpixv].data[5] = tmp;

    pentry = ((beebpixv & 16) ? 8 : 0) |
             ((beebpixv & 4)  ? 4 : 0) |
             ((beebpixv & 1)  ? 2 : 0) |
             1;

    tmp = VideoULA_Palette[pentry];

    if (tmp>7) {
      tmp&=7;
      if (VideoULA_ControlReg & 1) tmp^=7;
    }

    FastTable[beebpixv].data[6] =
      FastTable[beebpixv].data[7] = tmp;
  }
}

/*-------------------------------------------------------------------------------------------------------------*/
/* Some guess work and experimentation has determined that the left most pixel uses bits 7,5,3,1 for the       */
/* palette address, the next uses 6,4,2,0, the next uses 5,3,1,H (H=High), then 5,2,0,H                        */
static void DoFastTable4XStep4() {
  for (unsigned long beebpixv = 0; beebpixv < 256; beebpixv++) {
    unsigned long pentry = ((beebpixv & 128) ? 8 : 0) |
                           ((beebpixv & 32)  ? 4 : 0) |
                           ((beebpixv & 8)   ? 2 : 0) |
                           ((beebpixv & 2)   ? 1 : 0);

    unsigned char tmp = VideoULA_Palette[pentry];

    if (tmp>7) {
      tmp&=7;
      if (VideoULA_ControlReg & 1) tmp^=7;
    }

    FastTableDWidth[beebpixv].data[0] =
      FastTableDWidth[beebpixv].data[1] =
      FastTableDWidth[beebpixv].data[2] =
      FastTableDWidth[beebpixv].data[3] = tmp;

    pentry = ((beebpixv & 64) ? 8 : 0) |
             ((beebpixv & 16) ? 4 : 0) |
             ((beebpixv & 4)  ? 2 : 0) |
             ((beebpixv & 1)  ? 1 : 0);

    tmp = VideoULA_Palette[pentry];

    if (tmp > 7) {
      tmp &= 7;
      if (VideoULA_ControlReg & 1) tmp ^= 7;
    }

    FastTableDWidth[beebpixv].data[4] =
      FastTableDWidth[beebpixv].data[5] =
      FastTableDWidth[beebpixv].data[6] =
      FastTableDWidth[beebpixv].data[7] = tmp;

    pentry = ((beebpixv & 32) ? 8 : 0) |
             ((beebpixv & 8)  ? 4 : 0) |
             ((beebpixv & 2)  ? 2 : 0) |
             1;

    tmp = VideoULA_Palette[pentry];

    if (tmp > 7) {
      tmp &= 7;
      if (VideoULA_ControlReg & 1) tmp ^= 7;
    }

    FastTableDWidth[beebpixv].data[8] =
      FastTableDWidth[beebpixv].data[9] =
      FastTableDWidth[beebpixv].data[10] =
      FastTableDWidth[beebpixv].data[11] = tmp;

    pentry = ((beebpixv & 16) ? 8 :0 ) |
             ((beebpixv & 4)  ? 4 : 0) |
             ((beebpixv & 1)  ? 2 : 0) |
             1;

    tmp = VideoULA_Palette[pentry];

    if (tmp > 7) {
      tmp &= 7;
      if (VideoULA_ControlReg & 1) tmp ^= 7;
    }

    FastTableDWidth[beebpixv].data[12] =
      FastTableDWidth[beebpixv].data[13] =
      FastTableDWidth[beebpixv].data[14] =
      FastTableDWidth[beebpixv].data[15] = tmp;
  }
}

/*-------------------------------------------------------------------------------------------------------------*/
/* Some guess work and experimentation has determined that the left most pixel uses the same pattern as mode 1 */
/* all the way upto the 5th pixel which uses 31hh then 20hh and then 1hhh then 0hhhh                           */
static void DoFastTable2() {
  for (unsigned long beebpixv = 0; beebpixv < 256; beebpixv++) {
    unsigned long beebpixvt = beebpixv;

    for (int pix = 0; pix < 8; pix++) {
      unsigned long pentry = ((beebpixvt & 128) ? 8 : 0) |
                             ((beebpixvt & 32)  ? 4 : 0) |
                             ((beebpixvt & 8)   ? 2 : 0) |
                             ((beebpixvt & 2)   ? 1 : 0);

      beebpixvt <<= 1;
      beebpixvt |= 1;

      unsigned char tmp = VideoULA_Palette[pentry];

      if (tmp > 7) {
        tmp &= 7;
        if (VideoULA_ControlReg & 1) tmp ^= 7;
      }

      FastTable[beebpixv].data[pix] = tmp;
    }
  }
}

/*-------------------------------------------------------------------------------------------------------------*/
/* Some guess work and experimentation has determined that the left most pixel uses the same pattern as mode 1 */
/* all the way upto the 5th pixel which uses 31hh then 20hh and hten 1hhh then 0hhhh                           */
static void DoFastTable2XStep2() {
  for(unsigned long beebpixv = 0; beebpixv < 256; beebpixv++) {
    unsigned long beebpixvt = beebpixv;

    for (int pix = 0; pix < 8; pix++) {
      unsigned long pentry = ((beebpixvt & 128) ? 8 : 0) |
                             ((beebpixvt & 32)  ? 4 : 0) |
                             ((beebpixvt & 8)   ? 2 : 0) |
                             ((beebpixvt & 2)   ? 1 : 0);
      beebpixvt<<=1;
      beebpixvt|=1;

      unsigned char tmp = VideoULA_Palette[pentry];

      if (tmp > 7) {
        tmp &= 7;
        if (VideoULA_ControlReg & 1) tmp ^= 7;
      }

      FastTableDWidth[beebpixv].data[pix * 2] =
        FastTableDWidth[beebpixv].data[pix * 2 + 1] = tmp;
    }
  }
}

/*-------------------------------------------------------------------------------------------------------------*/

/* Rebuild fast table.
   The fast table accelerates the translation of beeb video memory
   values into X pixel values */

static void DoFastTable() {
  if ((CRTC_HorizontalDisplayed & 3) == 0) {
    LineRoutine = (VideoULA_ControlReg & 0x10) ? LowLevelDoScanLineNarrow : LowLevelDoScanLineWide;
  }
  else {
    LineRoutine = (VideoULA_ControlReg & 0x10) ? LowLevelDoScanLineNarrowNot4Bytes : LowLevelDoScanLineWideNot4Bytes;
  }

  // What happens next depends on the number of colours
  switch (NColsLookup[(VideoULA_ControlReg & 0x1c) >> 2]) {
    case 2:
      if (VideoULA_ControlReg & 0x10) {
        DoFastTable2();
      } else {
        DoFastTable2XStep2();
      }
      FastTable_Valid = true;
      break;

    case 4:
      if (VideoULA_ControlReg & 0x10) {
        DoFastTable4();
      } else {
        DoFastTable4XStep4();
      }
      FastTable_Valid = true;
      break;

    case 16:
      if (VideoULA_ControlReg & 0x10) {
        DoFastTable16();
      } else {
        DoFastTable16XStep8();
      }
      FastTable_Valid = true;
      break;

    default:
      break;
  }
}

/*-------------------------------------------------------------------------------------------------------------*/
#define BEEB_DOTIME_SAMPLESIZE 50

static void VideoStartOfFrame(void) {
#ifdef BEEB_DOTIME
  static int Have_GotTime=0;
  static struct tms previous,now;
  static int Time_FrameCount=0;

  double frametime;
  static CycleCountT OldCycles=0;

  if (!Have_GotTime) {
    times(&previous);
    Time_FrameCount=-1;
    Have_GotTime=1;
  }

  if (Time_FrameCount==(BEEB_DOTIME_SAMPLESIZE-1)) {
    times(&now);
    frametime=now.tms_utime-previous.tms_utime;
#ifndef SUNOS
#ifndef HPUX
                frametime/=(double)CLOCKS_PER_SEC;
#else
                frametime/=(double)sysconf(_SC_CLK_TCK);
#endif
#else
                frametime/=(double)HZ;
#endif
    frametime/=(double)BEEB_DOTIME_SAMPLESIZE;
    cerr << "Frametime: " << frametime << "s fps=" << (1/frametime) << "Total cycles=" << TotalCycles << "Cycles in last unit=" << (TotalCycles-OldCycles) << "\n";
    OldCycles=TotalCycles;
    previous=now;
    Time_FrameCount=0;
  } else Time_FrameCount++;

#endif

  /* FrameNum is determined by the window handler */
  if (VideoState.IsNewTVFrame)	// RTW - only calibrate timing once per frame
  {
    VideoState.IsNewTVFrame = false;
#ifdef WIN32
    FrameNum = mainWin->StartOfFrame();
#else
    /* If FrameNum hits 0 we actually refresh */
    if (FrameNum--==0) {
      FrameNum=Video_RefreshFrequency-1;
    }
#endif

    CursorFieldCount--;
    Mode7FlashTrigger--;
    VideoState.InterlaceFrame = !VideoState.InterlaceFrame;
  }

  // Cursor update for blink. I thought I'd put it here, as this is where the mode 7 flash field thingy is too
  // - Richard Gellman
  if (CursorFieldCount<0) {
    int CurStart = CRTC_CursorStart & 0x60;

    if (CurStart == 0) {
      // 0 is cursor displays, but does not blink
      CursorFieldCount = 0;
      CursorOnState = true;
    }
    else if (CurStart == 0x20) {
      // 32 is no cursor
      CursorFieldCount = 0;
      CursorOnState = false;
    }
    else if (CurStart == 0x40) {
      // 64 is 1/16 fast blink
      CursorFieldCount = 8;
      CursorOnState = !CursorOnState;
    }
    else if (CurStart == 0x60) {
      // 96 is 1/32 slow blink
      CursorFieldCount = 16;
      CursorOnState = !CursorOnState;
    }
  }

  // RTW - The meaning of CharLine has changed: -1 no longer means that we are in the vertical
  // total adjust period, and this is no longer handled as if it were at the beginning of a new CRTC cycle.
  // Hence, here we always set CharLine to 0.
  VideoState.CharLine=0;
  VideoState.InCharLineUp=0;

  VideoState.Addr = VideoState.StartAddr = CRTC_ScreenStartLow + (CRTC_ScreenStartHigh << 8);

  VideoState.IsTeletext = (VideoULA_ControlReg & 2) != 0;

  if (VideoState.IsTeletext) {
    // O aye. this is the mode 7 flash section is it? Modified for corrected flash settings - Richard Gellman
    if (Mode7FlashTrigger<0) {
      Mode7FlashTrigger = Mode7FlashOn ? MODE7OFFFIELDS : MODE7ONFIELDS;
      Mode7FlashOn = !Mode7FlashOn; // toggle flash state
    }
  }

  const int IL_Multiplier = (CRTC_InterlaceAndDelay & 1) ? 2 : 1;

  if (VideoState.InterlaceFrame) {
    IncTrigger((IL_Multiplier*(CRTC_HorizontalTotal+1)*((VideoULA_ControlReg & 16)?1:2)),VideoTriggerCount); /* Number of 2MHz cycles until another scanline needs doing */
  } else {
    IncTrigger(((CRTC_HorizontalTotal+1)*((VideoULA_ControlReg & 16)?1:2)),VideoTriggerCount); /* Number of 2MHz cycles until another scanline needs doing */
  }

  TeletextPoll();
}

/*--------------------------------------------------------------------------*/
/* Scanline processing for modes with fast 6845 clock - i.e. narrow pixels  */
static void LowLevelDoScanLineNarrow() {
  int BytesToGo=CRTC_HorizontalDisplayed;
  EightUChars *vidPtr=mainWin->GetLinePtr(VideoState.PixmapLine);

  /* If the step is 4 then each byte corresponds to one entry in the fasttable
     and thus we can copy it really easily (and fast!) */
  const unsigned char *CurrentPtr = VideoState.DataPtr + VideoState.InCharLineUp;

  /* This should help the compiler - it doesn't need to test for end of loop
     except every 4 entries */
  BytesToGo/=4;
  for(;BytesToGo;CurrentPtr+=32,BytesToGo--) {
    *(vidPtr++)=FastTable[*CurrentPtr];
    *(vidPtr++)=FastTable[*(CurrentPtr+8)];
    *(vidPtr++)=FastTable[*(CurrentPtr+16)];
    *(vidPtr++)=FastTable[*(CurrentPtr+24)];
  }
}

/*--------------------------------------------------------------------------*/
/* Scanline processing for modes with fast 6845 clock - i.e. narrow pixels  */
/* This version handles screen modes where there is not a multiple of 4     */
/* bytes per scanline.                                                      */
static void LowLevelDoScanLineNarrowNot4Bytes() {
  int BytesToGo=CRTC_HorizontalDisplayed;
  EightUChars *vidPtr=mainWin->GetLinePtr(VideoState.PixmapLine);

  /* If the step is 4 then each byte corresponds to one entry in the fasttable
     and thus we can copy it really easily (and fast!) */
  const unsigned char *CurrentPtr = VideoState.DataPtr + VideoState.InCharLineUp;

  for(;BytesToGo;CurrentPtr+=8,BytesToGo--)
    (vidPtr++)->eightbyte=FastTable[*CurrentPtr].eightbyte;
}

/*-----------------------------------------------------------------------------*/
/* Scanline processing for the low clock rate modes                            */
static void LowLevelDoScanLineWide() {
  int BytesToGo=CRTC_HorizontalDisplayed;
  SixteenUChars *vidPtr=mainWin->GetLinePtr16(VideoState.PixmapLine);

  /* If the step is 4 then each byte corresponds to one entry in the fasttable
     and thus we can copy it really easily (and fast!) */
  const unsigned char *CurrentPtr = VideoState.DataPtr + VideoState.InCharLineUp;

  /* This should help the compiler - it doesn't need to test for end of loop
     except every 4 entries */
  BytesToGo/=4;
  for(;BytesToGo;CurrentPtr+=32,BytesToGo--) {
    *(vidPtr++)=FastTableDWidth[*CurrentPtr];
    *(vidPtr++)=FastTableDWidth[*(CurrentPtr+8)];
    *(vidPtr++)=FastTableDWidth[*(CurrentPtr+16)];
    *(vidPtr++)=FastTableDWidth[*(CurrentPtr+24)];
  }
}

/*-----------------------------------------------------------------------------*/
/* Scanline processing for the low clock rate modes                            */
/* This version handles cases where the screen width is not divisible by 4     */
static void LowLevelDoScanLineWideNot4Bytes() {
  int BytesToGo=CRTC_HorizontalDisplayed;
  SixteenUChars *vidPtr=mainWin->GetLinePtr16(VideoState.PixmapLine);

  const unsigned char *CurrentPtr = VideoState.DataPtr + VideoState.InCharLineUp;

  for(;BytesToGo;CurrentPtr+=8,BytesToGo--)
    *(vidPtr++)=FastTableDWidth[*CurrentPtr];
}

/*-------------------------------------------------------------------------------------------------------------*/
/* Do all the pixel rows for one row of teletext characters                                                    */
static void DoMode7Row(void) {
  const unsigned char *CurrentPtr = VideoState.DataPtr;
  int CurrentChar;
  int XStep;
  unsigned char byte;

  unsigned int Foreground = 7;
  /* The foreground colour changes after the current character; only relevant for hold graphics */
  unsigned int ForegroundPending=Foreground;
  unsigned int ActualForeground;
  unsigned int Background = 0;
  bool Flash = false; // i.e. steady
  bool DoubleHeight = false; // Normal
  bool Graphics;
  bool NextGraphics = false; // i.e. alpha
  bool Separated = false; // i.e. continuous graphics
  bool HoldGraph;
  bool NextHoldGraph = false; // i.e. don't hold graphics
  unsigned char HoldGraphChar;
  unsigned char NextHoldGraphChar = 32; // the character to "hold" during control codes
  bool HoldSeparated;
  bool NextHoldSeparated = false; // Separated graphics mode in force when grapics held
  unsigned int CurrentCol[20]={0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff
  ,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff};
  int CurrentLen[20]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
  int CurrentStartX[20]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
  int CurrentScanLine;
  int CurrentX=0;
  int CurrentPixel;
  int FontTypeIndex=0; /* 0=alpha, 1=contiguous graphics, 2=separated graphics */

  if (CRTC_HorizontalDisplayed>80) return; /* Not possible on beeb - and would break the double height lookup array */

  XStep=1;

  for(CurrentChar=0;CurrentChar<CRTC_HorizontalDisplayed;CurrentChar++) {
    HoldGraph=NextHoldGraph;
    HoldGraphChar=NextHoldGraphChar;
    HoldSeparated=NextHoldSeparated;
    Graphics=NextGraphics;
    byte=CurrentPtr[CurrentChar];
    if (byte<32) byte+=128; // fix for naughty programs that use 7-bit control codes - Richard Gellman
    if ((byte & 32) && Graphics) {
      NextHoldGraphChar=byte;
      NextHoldSeparated=Separated;
    }
    if ((byte>=128) && (byte<=159)) {
      if (!HoldGraph && byte != 158) NextHoldGraphChar = 32; // SAA5050 teletext rendering bug
      switch (byte) {
        case 129: // Alphanumeric red
        case 130: // Alphanumeric green
        case 131: // Alphanumeric yellow
        case 132: // Alphanumeric blue
        case 133: // Alphanumeric magenta
        case 134: // Alphanumeric cyan
        case 135: // Alphanumeric white
          ForegroundPending = byte - 128;
          NextGraphics = false;
          NextHoldGraphChar=32;
          break;

        case 136: // Flash
          Flash = true;
          break;

        case 137: // Steady
          Flash = false;
          break;

        case 140: // Normal height
          DoubleHeight = false;
          break;

        case 141: // Double height
          if (!CurrentLineBottom) NextLineBottom = true;
          DoubleHeight = true;
          break;

        case 145: // Graphics red
        case 146: // Graphics green
        case 147: // Graphics yellow
        case 148: // Graphics blue
        case 149: // Graphics magenta
        case 150: // Graphics cyan
        case 151: // Graphics white
          ForegroundPending = byte - 144;
          NextGraphics = true;
          break;

        case 152: // Conceal display - not sure about this
          Foreground=Background;
          ForegroundPending=Background;
          break;

        case 153: // Contiguous graphics
          Separated = false;
          break;

        case 154: // Separated graphics
          Separated = true;
          break;

        case 156: // Black background
          Background = 0;
          break;

        case 157: // New background
          Background = Foreground;
          break;

        case 158: // Hold graphics
          NextHoldGraph = true;
          HoldGraph = true;
          break;

        case 159: // Release graphics
          NextHoldGraph = false;
          break;
      }

      // This next line hides any non double height characters on the bottom line
      // Fudge so that the special character is just displayed in background
      if (HoldGraph && Graphics) {
        byte=HoldGraphChar;
        FontTypeIndex=HoldSeparated?2:1;
      } else {
        byte=32;
        FontTypeIndex=Graphics?(Separated?2:1):0;
      }
    } /* test for special character */
    else {
      FontTypeIndex = Graphics ? (Separated ? 2 : 1) : 0;
    }

    if (CurrentLineBottom && ((byte & 127) > 31) && !DoubleHeight) byte = 32;
    TeletextStyle = (CRTC_ScanLinesPerChar <= 9 || TeletextHalfMode) ? 2 : 1;
    /* Top bit never reaches character generator */
    byte&=127;
    /* Our font table goes from character 32 up */
    if (byte < 32) byte = 0; else byte -= 32;

    /* Conceal flashed text if necessary */
    ActualForeground = (Flash && !Mode7FlashOn) ? Background : Foreground;

    if (!DoubleHeight) {
      // Loop through each scanline
      for (CurrentScanLine = 0 + (TeletextStyle - 1); CurrentScanLine < 20; CurrentScanLine += TeletextStyle) {
        unsigned int tmp = Mode7Font[FontTypeIndex][byte][CurrentScanLine];

        if (tmp == 0 || tmp == 255) {
          unsigned int col = tmp == 0 ? Background : ActualForeground;

          if (col == CurrentCol[CurrentScanLine]) {
            // Same colour, so increment run length
            CurrentLen[CurrentScanLine] += 12 * XStep;
          }
          else {
            if (CurrentLen[CurrentScanLine]) {
              mainWin->doHorizLine(
                CurrentCol[CurrentScanLine],             // Colour
                VideoState.PixmapLine + CurrentScanLine, // y
                CurrentStartX[CurrentScanLine],          // sx
                CurrentLen[CurrentScanLine]              // width
              );
            }

            CurrentCol[CurrentScanLine] = col;
            CurrentStartX[CurrentScanLine] = CurrentX;
            CurrentLen[CurrentScanLine] = 12 * XStep;
          }
        } else {
          // Loop through 12 pixels horizontally
          for (CurrentPixel = 0x800; CurrentPixel != 0; CurrentPixel >>= 1) {
            /* Background or foreground ? */
            unsigned int col = (tmp & CurrentPixel) ? ActualForeground : Background;

            /* Do we need to draw ? */
            if (col == CurrentCol[CurrentScanLine]) {
              // Same colour, so increment run length
              CurrentLen[CurrentScanLine] += XStep;
            }
            else {
              if (CurrentLen[CurrentScanLine]) {
                mainWin->doHorizLine(
                  CurrentCol[CurrentScanLine],             // Colour
                  VideoState.PixmapLine + CurrentScanLine, // y
                  CurrentStartX[CurrentScanLine],          // sx
                  CurrentLen[CurrentScanLine]              // width
                );
              }

              CurrentCol[CurrentScanLine] = col;
              CurrentStartX[CurrentScanLine] = CurrentX;
              CurrentLen[CurrentScanLine] = XStep;
            }

            CurrentX += XStep;
          }

          CurrentX -= 12 * XStep;
        }
      } /* Scanline for */

      CurrentX+=12*XStep;
      Mode7DoubleHeightFlags[CurrentChar] = true; // Not double height - so if the next line is double height it will be top half
    }
    else {
      /* Double height! */

      // Loop through 12 pixels horizontally
      for (CurrentPixel = 0x800; CurrentPixel != 0; CurrentPixel >>= 1) {
        // Loop through each scanline
        for (CurrentScanLine = 0 + (TeletextStyle - 1); CurrentScanLine < 20; CurrentScanLine += TeletextStyle) {
          const int ActualScanLine = CurrentLineBottom ? 10 + (CurrentScanLine / 2) : (CurrentScanLine / 2);

          /* Background or foreground ? */
          unsigned int col = (Mode7Font[FontTypeIndex][byte][ActualScanLine] & CurrentPixel) ? ActualForeground : Background;

          /* Do we need to draw ? */
          if (col == CurrentCol[CurrentScanLine]) {
            // Same colour, so increment run length
            CurrentLen[CurrentScanLine] += XStep;
          }
          else {
            if (CurrentLen[CurrentScanLine]) {
              mainWin->doHorizLine(
                CurrentCol[CurrentScanLine],             // Colour
                VideoState.PixmapLine + CurrentScanLine, // y
                CurrentStartX[CurrentScanLine],          // sx
                CurrentLen[CurrentScanLine]              // width
              );
            }

            CurrentCol[CurrentScanLine] = col;
            CurrentStartX[CurrentScanLine] = CurrentX;
            CurrentLen[CurrentScanLine] = XStep;
          }
        }

        CurrentX += XStep;
      }

      Mode7DoubleHeightFlags[CurrentChar] = !Mode7DoubleHeightFlags[CurrentChar]; // Not double height - so if the next line is double height it will be top half
    }
    Foreground=ForegroundPending;
  } /* character loop */

  /* Finish off right bits of scan line */
  for (CurrentScanLine = 0 + (TeletextStyle - 1); CurrentScanLine < 20; CurrentScanLine += TeletextStyle) {
    if (CurrentLen[CurrentScanLine])
      mainWin->doHorizLine(
        CurrentCol[CurrentScanLine],             // Colour
        VideoState.PixmapLine + CurrentScanLine, // y
        CurrentStartX[CurrentScanLine],          // sx
        CurrentLen[CurrentScanLine]              // width
      );
  }

  CurrentLineBottom = NextLineBottom;
  NextLineBottom = false;
}

/*-------------------------------------------------------------------------------------------------------------*/
/* Actually does the work of decoding beeb memory and plotting the line to X */
static void LowLevelDoScanLine() {
  if (!FastTable_Valid) {
    // Update acceleration tables
    DoFastTable();
  }

  if (FastTable_Valid) {
    LineRoutine();
  }
}

void RedoMPTR(void) {
	if (VideoState.IsTeletext) {
		VideoState.DataPtr = BeebMemPtrWithWrapMode7(ova, ovn);
	}
	else {
		VideoState.DataPtr = BeebMemPtrWithWrap(ova, ovn);
	}

	// FastTable_Valid = false;
}

/*-------------------------------------------------------------------------------------------------------------*/
void VideoDoScanLine(void) {
  int l;
  /* cerr << "CharLine=" << VideoState.CharLine << " InCharLineUp=" << VideoState.InCharLineUp << "\n"; */
  if (VideoState.IsTeletext) {
    if (VideoState.DoCA1Int) {
      SysVIATriggerCA1Int(0);
      VideoState.DoCA1Int = false;
    }

    /* Clear the next 20 scan lines */
    if (!FrameNum) {
      if (VScreenAdjust>0 && VideoState.PixmapLine==0)
        for (l=-VScreenAdjust; l<0; ++l)
          mainWin->doHorizLine(0, l, -36, 800);
      for (l=0; l<20 && VideoState.PixmapLine+l<512; ++l)
        mainWin->doHorizLine(0, VideoState.PixmapLine+l, -36, 800);
    }

    // RTW - Mode 7 emulation is rather broken, as we should be plotting it line-by-line instead
    // of character block at a time.
    // For now though, I leave it as it is, and plot an entire block when InCharLineUp is 0.
    // The infrastructure now exists though to make DoMode7Row plot just a single scanline (from InCharLineUp 0..9).
    if (VideoState.CharLine<CRTC_VerticalDisplayed && VideoState.InCharLineUp==0) {
      ova=VideoState.Addr; ovn=CRTC_HorizontalDisplayed;
      VideoState.DataPtr = BeebMemPtrWithWrapMode7(VideoState.Addr, CRTC_HorizontalDisplayed);
      VideoState.Addr+=CRTC_HorizontalDisplayed;
      if (!FrameNum) DoMode7Row();
      VideoState.PixmapLine+=20;
    }

    /* Move onto next physical scanline as far as timing is concerned */
    VideoState.InCharLineUp++;

    // RTW - Mode 7 hardwired for now. Assume 10 scanlines per character regardless (actually 9.5 but god knows how that works)
    if (VideoState.CharLine<=CRTC_VerticalTotal && VideoState.InCharLineUp>9) {
      VideoState.CharLine++;
      VideoState.InCharLineUp=0;
    }

    // RTW - check if we have reached the end of the PAL frame.
    // This whole thing is a bit hardwired and kludged. Should really be emulating VSync position 'properly' like Modes 0-6.
    if (VideoState.CharLine > CRTC_VerticalTotal && VideoState.InCharLineUp >= CRTC_VerticalTotalAdjust) {
      // Changed so that whole screen is still visible after *TV255
      VScreenAdjust=-100+(((CRTC_VerticalTotal+1)-(CRTC_VerticalSyncPos-1))*(20/TeletextStyle));
      AdjustVideo();
      if (!FrameNum) {
        VideoAddCursor();
        VideoAddLEDs();
        // Clear rest of screen below virtical total
        for (l=VideoState.PixmapLine; l<500/TeletextStyle; ++l)
          mainWin->doHorizLine(0, l, -36, 800);
        mainWin->updateLines(0,(500/TeletextStyle));
      }
      VideoState.IsNewTVFrame = true;
      VideoStartOfFrame();
      VideoState.PreviousLastPixmapLine=VideoState.PixmapLine;
      VideoState.PixmapLine=0;
      SysVIATriggerCA1Int(1);
      VideoState.DoCA1Int = true;
    } else {
      // RTW- set timer till the next scanline update (this is now nice and simple)
      IncTrigger((CRTC_HorizontalTotal+1)*((VideoULA_ControlReg & 16)?1:2),VideoTriggerCount);
    }
  } else {
    /* Non teletext. */

    // Handle VSync
    // RTW - this was moved to the top so that we can correctly set R7=0,
    // i.e. we can catch it before the line counters are incremented
    if (VideoState.VSyncState) {
      if (!(--VideoState.VSyncState)) {
        SysVIATriggerCA1Int(0);
      }
    }

    if (VideoState.VSyncState == 0 && VideoState.CharLine == CRTC_VerticalSyncPos && VideoState.InCharLineUp == 0) {
      // Nothing displayed?
      if (VideoState.FirstPixmapLine<0)
        VideoState.FirstPixmapLine=0;

      VideoState.PreviousFirstPixmapLine=VideoState.FirstPixmapLine;
      VideoState.FirstPixmapLine=-1;
      VideoState.PreviousLastPixmapLine=VideoState.LastPixmapLine;
      VideoState.LastPixmapLine=0;
      VideoState.PixmapLine=0;
      VideoState.IsNewTVFrame = true;

      SysVIATriggerCA1Int(1);
      VideoState.VSyncState=(CRTC_SyncWidth>>4);
    }

    /* Clear the scan line */
    if (!FrameNum)
      memset(mainWin->GetLinePtr(VideoState.PixmapLine),0,800);

    // RTW - changed so we are even able to plot vertical total adjust region if CRTC_VerticalDisplayed is high enough
    if (VideoState.CharLine<CRTC_VerticalDisplayed) {
      // Visible char line, record first line
      if (VideoState.FirstPixmapLine==-1)
        VideoState.FirstPixmapLine=VideoState.PixmapLine;
      // Always record the last line
      VideoState.LastPixmapLine=VideoState.PixmapLine;

      /* If first row of character then get the data pointer from memory */
      if (VideoState.InCharLineUp==0) {
        ova=VideoState.Addr*8; ovn=CRTC_HorizontalDisplayed*8;
        VideoState.DataPtr=BeebMemPtrWithWrap(VideoState.Addr*8,CRTC_HorizontalDisplayed*8);
        VideoState.Addr+=CRTC_HorizontalDisplayed;
      }

      if ((VideoState.InCharLineUp<8) && ((CRTC_InterlaceAndDelay & 0x30)!=48)) {
        if (!FrameNum)
          LowLevelDoScanLine();
      }
    }

    // See if we are at the cursor line
    if (CurY == -1 && VideoState.Addr > (CRTC_CursorPosLow + (CRTC_CursorPosHigh << 8))) {
      CurY = VideoState.PixmapLine;
    }

    // Screen line increment and wraparound
    if (++VideoState.PixmapLine == MAX_VIDEO_SCAN_LINES)
      VideoState.PixmapLine = 0;

    /* Move onto next physical scanline as far as timing is concerned */
    VideoState.InCharLineUp+=1;

    // RTW - check whether we have reached a new character row.
    // if CharLine>CRTC_VerticalTotal, we are in the vertical total adjust region so we don't wrap to a new row.
    if (VideoState.CharLine<=CRTC_VerticalTotal && VideoState.InCharLineUp>CRTC_ScanLinesPerChar) {
      VideoState.CharLine++;
      VideoState.InCharLineUp=0;
    }

    // RTW - neater way of detecting the end of the PAL frame, which doesn't require making a special case
    // of the vertical total adjust period.
    if (VideoState.CharLine>CRTC_VerticalTotal && VideoState.InCharLineUp>=CRTC_VerticalTotalAdjust) {
      VScreenAdjust=0;
      if (!FrameNum && VideoState.IsNewTVFrame) {
        VideoAddCursor();
        VideoAddLEDs();
        CurY=-1;
        int n = VideoState.PreviousLastPixmapLine-VideoState.PreviousFirstPixmapLine+1;
        if (n < 0) {
          n += MAX_VIDEO_SCAN_LINES;
        }

        int startLine = 32;
        if (n > 248 && VideoState.PreviousFirstPixmapLine >= 40) {
          // RTW -
          // This is a little hack which ensures that a fullscreen mode with *TV255 will always
          // fit unclipped in the window in Modes 0-6
          startLine = 40;
        }

        mainWin->updateLines(startLine, 256);
      }
      VideoStartOfFrame();
      AdjustVideo();
    } else {
      IncTrigger((CRTC_HorizontalTotal+1)*((VideoULA_ControlReg & 16)?1:2),VideoTriggerCount);
    }
  } /* Teletext if */
}

/*-------------------------------------------------------------------------------------------------------------*/
void AdjustVideo() {
  ActualScreenWidth = CRTC_HorizontalDisplayed * HSyncModifier;

  if (ActualScreenWidth > 800) {
    ActualScreenWidth = 800;
  }
  else if (ActualScreenWidth < 640) {
    ActualScreenWidth = 640;
  }

  int InitialOffset = 0 - (((CRTC_HorizontalTotal + 1) / 2) - (HSyncModifier == 8 ? 40 : 20));
  int HStart = InitialOffset +
               (CRTC_HorizontalTotal + 1 - (CRTC_HorizontalSyncPos + (CRTC_SyncWidth & 0x0f))) +
               ((HSyncModifier == 8) ? 2 : 1);
  if (TeletextEnabled) HStart += 2;
  if (HStart < 0) HStart = 0;
  ScreenAdjust = HStart * HSyncModifier + (VScreenAdjust > 0 ? VScreenAdjust * 800 : 0);
}

/*-------------------------------------------------------------------------------------------------------------*/
void VideoInit(void) {
  VideoStartOfFrame();
  ova=0x3000; ovn=640;
  VideoState.DataPtr=BeebMemPtrWithWrap(0x3000,640);
  SetTrigger(99,VideoTriggerCount); /* Give time for OS to set mode up before doing anything silly */
  FastTable_Valid = false;
  BuildMode7Font();

#ifndef WIN32
  char *environptr=getenv("BeebVideoRefreshFreq");
  if (environptr!=NULL) Video_RefreshFrequency=atoi(environptr);
  if (Video_RefreshFrequency<1) Video_RefreshFrequency=1;
#endif

  FrameNum=Video_RefreshFrequency;
  VideoState.PixmapLine=0;
  VideoState.FirstPixmapLine=-1;
  VideoState.PreviousFirstPixmapLine=0;
  VideoState.LastPixmapLine=0;
  VideoState.PreviousLastPixmapLine=256;
  VideoState.IsNewTVFrame = false;
  CurY=-1;
  //AdjustVideo();
//  crtclog=fopen("/crtc.log","wb");
}

/*-------------------------------------------------------------------------------------------------------------*/
void CRTCWrite(int Address, unsigned char Value) {
  if (Address & 1) {
    // if (CRTCControlReg<14) { fputc(CRTCControlReg,crtclog); fputc(Value,crtclog); }
    // if (CRTCControlReg<14) {
    //     fprintf(crtclog,"%d (%02X) Written to register %d from %04X\n",Value,Value,CRTCControlReg,ProgramCounter);
    // }

    if (DebugEnabled && CRTCControlReg<14) {
        char info[200];
        sprintf(info, "CRTC: Write register %X value %02X", (int)CRTCControlReg, Value);
        DebugDisplayTrace(DebugType::Video, true, info);
    }

    switch (CRTCControlReg) {
      case 0:
        CRTC_HorizontalTotal = Value;
        AdjustVideo();
        break;

      case 1:
        CRTC_HorizontalDisplayed = Value;
        if (CRTC_HorizontalDisplayed > 127)
            CRTC_HorizontalDisplayed = 127;
        FastTable_Valid = false;
        AdjustVideo();
        break;

      case 2:
        CRTC_HorizontalSyncPos = Value;
        AdjustVideo();
        break;

      case 3:
        CRTC_SyncWidth = Value;
        AdjustVideo();
        // fprintf(crtclog,"V Sync width: %d\n",(Value>>4)&15);
        break;

      case 4:
        CRTC_VerticalTotal = Value;
        AdjustVideo();
        break;

      case 5:
        CRTC_VerticalTotalAdjust = Value & 0x1f;  // 5 bit register
        // fprintf(crtclog,"Vertical Total Adjust: %d\n",Value);
        AdjustVideo();
        break;

      case 6:
        CRTC_VerticalDisplayed = Value;
        AdjustVideo();
        break;

      case 7:
        CRTC_VerticalSyncPos = Value & 0x7f;
        AdjustVideo();
        break;

      case 8:
        CRTC_InterlaceAndDelay = Value;
        break;

      case 9:
        CRTC_ScanLinesPerChar = Value;
        break;

      case 10:
        CRTC_CursorStart = Value;
        break;

      case 11:
        CRTC_CursorEnd=Value;
        break;

      case 12:
        CRTC_ScreenStartHigh = Value;
        // fprintf(crtclog,"Screen now at &%02x%02x\n",Value,CRTC_ScreenStartLow);
        break;

      case 13:
        CRTC_ScreenStartLow = Value;
        // fprintf(crtclog,"Screen now at &%02x%02x\n",CRTC_ScreenStartHigh,Value);
        break;

      case 14:
        CRTC_CursorPosHigh = Value & 0x3f; /* Cursor high only has 6 bits */
        break;

      case 15:
        CRTC_CursorPosLow = Value & 0xff;
        break;

      default: /* In case the user wrote a duff control register value */
        break;
    }
    /* cerr << "CRTCWrite RegNum=" << int(CRTCControlReg) << " Value=" << Value << "\n"; */
  } else {
    CRTCControlReg = Value & 0x1f;
  }
}

/*-------------------------------------------------------------------------------------------------------------*/

unsigned char CRTCRead(int Address)
{
  if (Address & 1) {
    switch (CRTCControlReg) {
      case 12:
        return CRTC_ScreenStartHigh;
      case 13:
        return CRTC_ScreenStartLow;
      case 14:
        return CRTC_CursorPosHigh;
      case 15:
        return CRTC_CursorPosLow;
      case 16:
        return CRTC_LightPenHigh;
      case 17:
        return CRTC_LightPenLow;
      default:
        break;
    }
  }
  else {
    // Rockwell part has bits 5,6,7 used - bit 6 is set when LPEN is received, bit 5 when in vertical retrace
    return 0;
  }

  return 0; // Keeep MSVC happy $NRM
}

/*-------------------------------------------------------------------------------------------------------------*/
void VideoULAWrite(int Address, unsigned char Value) {
  if (Address & 1) {
    VideoULA_Palette[(Value & 0xf0)>>4]=(Value & 0xf) ^ 7;
    FastTable_Valid = false;
    /* cerr << "Palette reg " << ((Value & 0xf0)>>4) << " now has value " << ((Value & 0xf) ^ 7) << "\n"; */
    //fprintf(crtclog,"Pallette written to at line %d\n",VideoState.PixmapLine);
  } else {
    unsigned char oldValue = VideoULA_ControlReg;
    VideoULA_ControlReg=Value;
    FastTable_Valid = false; /* Could be more selective and only do it if no.of.cols bit changes */
    /* cerr << "VidULA Ctrl reg write " << hex << Value << "\n"; */
    // Adjust HSyncModifier
    if (VideoULA_ControlReg & 16) HSyncModifier=8; else HSyncModifier=16;
    if (VideoULA_ControlReg & 2) HSyncModifier=12;
    // number of pixels per CRTC character (on our screen)
    TeletextEnabled = (Value & 2) != 0;
    if ((Value&2)^(oldValue&2)) { ScreenAdjust=0; }
    AdjustVideo();
  }
}

/*-------------------------------------------------------------------------------------------------------------*/

unsigned char VideoULARead(int /* Address */)
{
	return 0xfe; // Read not defined from Video ULA
}

/*-------------------------------------------------------------------------------------------------------------*/
static void VideoAddCursor() {
	static const int CurSizes[] = { 2,1,0,0,4,2,0,4 };
	int ScrAddr,CurAddr,RelAddr;
	int CurX;
	int CurSize;
	int CurStart, CurEnd;

	/* Check if cursor has been hidden */
	if ((VideoULA_ControlReg & 0xe0) == 0 || (CRTC_CursorStart & 0x60) == 0x20)
		return;

	/* Use clock bit and cursor bits to work out size */
	if (VideoULA_ControlReg & 0x80)
		CurSize = CurSizes[(VideoULA_ControlReg & 0x70)>>4] * 8;
	else
		CurSize = 2 * 8; /* Mode 7 */

	if (VideoState.IsTeletext)
	{
		ScrAddr=CRTC_ScreenStartLow+(((CRTC_ScreenStartHigh ^ 0x20) + 0x74 & 0xff)<<8);
		CurAddr=CRTC_CursorPosLow+(((CRTC_CursorPosHigh ^ 0x20) + 0x74 & 0xff)<<8);

		CurStart = (CRTC_CursorStart & 0x1f) / 2;
		CurEnd = CRTC_CursorEnd;
		CurSize-=4;
	}
	else
	{
		ScrAddr=CRTC_ScreenStartLow+(CRTC_ScreenStartHigh<<8);
		CurAddr=CRTC_CursorPosLow+(CRTC_CursorPosHigh<<8);

		CurStart = CRTC_CursorStart & 0x1f;
		CurEnd = CRTC_CursorEnd;
	}

	RelAddr=CurAddr-ScrAddr;
	if (RelAddr < 0 || CRTC_HorizontalDisplayed == 0)
		return;

	/* Work out char positions */
	CurX = RelAddr % CRTC_HorizontalDisplayed;

	/* Convert to pixel positions */
	if (VideoState.IsTeletext) {
		CurX = CurX * 12;
		CurY = (RelAddr / CRTC_HorizontalDisplayed) * 20 + 9;
	}
	else {
		CurX = CurX * HSyncModifier;
	}

	/* Limit cursor size */ // This should be 11, not 9 - Richard Gellman
	if (CurEnd > 11)
		CurEnd = 11;

	if (CurX + CurSize >= 640)
		CurSize = 640 - CurX;

	// Cursor delay
	CurX+=((CRTC_InterlaceAndDelay&192)>>6)*HSyncModifier;
	if (VideoState.IsTeletext) CurX-=2*HSyncModifier;
	if (CurSize > 0)
	{
		for (int y = CurStart; y <= CurEnd && y <= CRTC_ScanLinesPerChar && CurY + y < 500; ++y)
		{
			if (CurY + y >= 0) {
				if (CursorOnState) {
					mainWin->doInvHorizLine(7, CurY + y, CurX, CurSize);
				}
			}
		}
	}
}

// A low to high transition on the 6845 Light Pen Strobe (LPSTB) input
// latches the current Refresh Address in the light pen register.

// Perhaps tie to mouse pointer?

void VideoLightPenStrobe()
{
	CRTC_LightPenHigh = (VideoState.Addr & 0x3f00) >> 8;
	CRTC_LightPenLow  = VideoState.Addr & 0xff;
}

void VideoAddLEDs(void) {
	// now add some keyboard leds
	if (LEDs.ShowKB) {
		if (MachineType == Model::Master128) {
			mainWin->doLED(4, true);
		}
		else {
			mainWin->doLED(4, LEDs.Motor);
		}
		mainWin->doLED(14,LEDs.CapsLock);
		mainWin->doLED(24,LEDs.ShiftLock);
	}

	if (LEDs.ShowDisc) {
		int adj = TeletextEnabled ? 86 : 0;
		mainWin->doLED(578-adj,LEDs.HDisc[0]);
		mainWin->doLED(588-adj,LEDs.HDisc[1]);
		mainWin->doLED(598-adj,LEDs.HDisc[2]);
		mainWin->doLED(608-adj,LEDs.HDisc[3]);
		mainWin->doLED(618-adj,LEDs.Disc0);
		mainWin->doLED(628-adj,LEDs.Disc1);
	}
}

/*-------------------------------------------------------------------------*/
void SaveVideoUEF(FILE *SUEF) {
	fput16(0x0468,SUEF);
	fput32(112, SUEF);
	// Save CRTC state
	fputc(CRTC_HorizontalTotal, SUEF);
	fputc(CRTC_HorizontalDisplayed, SUEF);
	fputc(CRTC_HorizontalSyncPos, SUEF);
	fputc(CRTC_SyncWidth, SUEF);
	fputc(CRTC_VerticalTotal, SUEF);
	fputc(CRTC_VerticalTotalAdjust, SUEF);
	fputc(CRTC_VerticalDisplayed, SUEF);
	fputc(CRTC_VerticalSyncPos, SUEF);
	fputc(CRTC_InterlaceAndDelay, SUEF);
	fputc(CRTC_ScanLinesPerChar, SUEF);
	fputc(CRTC_CursorStart, SUEF);
	fputc(CRTC_CursorEnd, SUEF);
	fputc(CRTC_ScreenStartHigh, SUEF);
	fputc(CRTC_ScreenStartLow, SUEF);
	fputc(CRTC_CursorPosHigh, SUEF);
	fputc(CRTC_CursorPosLow, SUEF);
	fputc(CRTC_LightPenHigh, SUEF);
	fputc(CRTC_LightPenLow, SUEF);
	// VIDPROC
	fputc(VideoULA_ControlReg, SUEF);
	for (int col = 0; col < 16; ++col) {
		fputc(VideoULA_Palette[col] ^ 7,SUEF); // Use real ULA values
	}
	fput16(ActualScreenWidth, SUEF);
	fput32(ScreenAdjust, SUEF);
	fputc(CRTCControlReg, SUEF);
	fputc(TeletextStyle, SUEF);

	fput32(VideoState.Addr, SUEF);
	fput32(VideoState.StartAddr, SUEF);
	fput32(VideoState.PixmapLine, SUEF);
	fput32(VideoState.FirstPixmapLine, SUEF);
	fput32(VideoState.PreviousFirstPixmapLine, SUEF);
	fput32(VideoState.LastPixmapLine, SUEF);
	fput32(VideoState.PreviousLastPixmapLine, SUEF);
	fput32(VideoState.CharLine, SUEF);
	fput32(VideoState.InCharLineUp, SUEF);
	fput32(VideoState.VSyncState, SUEF);
	fputc(VideoState.IsNewTVFrame, SUEF);
	fputc(VideoState.InterlaceFrame, SUEF);
	fputc(VideoState.DoCA1Int, SUEF);
	fput32(ova, SUEF);
	fput32(ovn, SUEF);
	fput32(CursorFieldCount, SUEF);
	fputc(CursorOnState, SUEF);
	fput32(CurY, SUEF);
	fput32(Mode7FlashTrigger, SUEF);
	fputc(Mode7FlashOn, SUEF);
	fput32(VideoTriggerCount - TotalCycles, SUEF);
}

void LoadVideoUEF(FILE *SUEF, int Version) {
	CRTC_HorizontalTotal = fget8(SUEF);
	CRTC_HorizontalDisplayed = fget8(SUEF);
	CRTC_HorizontalSyncPos = fget8(SUEF);
	CRTC_SyncWidth = fget8(SUEF);
	CRTC_VerticalTotal = fget8(SUEF);
	CRTC_VerticalTotalAdjust = fget8(SUEF);
	CRTC_VerticalDisplayed = fget8(SUEF);
	CRTC_VerticalSyncPos = fget8(SUEF);
	CRTC_InterlaceAndDelay = fget8(SUEF);
	CRTC_ScanLinesPerChar = fget8(SUEF);
	CRTC_CursorStart = fget8(SUEF);
	CRTC_CursorEnd = fget8(SUEF);
	CRTC_ScreenStartHigh = fget8(SUEF);
	CRTC_ScreenStartLow = fget8(SUEF);
	CRTC_CursorPosHigh = fget8(SUEF);
	CRTC_CursorPosLow = fget8(SUEF);
	CRTC_LightPenHigh = fget8(SUEF);
	CRTC_LightPenLow = fget8(SUEF);
	// VIDPROC
	VideoULA_ControlReg = fget8(SUEF);
	for (int col = 0; col < 16; ++col) {
		VideoULA_Palette[col] = fget8(SUEF) ^ 7; // Use real ULA values
	}
	ActualScreenWidth=fget16(SUEF);
	ScreenAdjust=fget32(SUEF);
	CRTCControlReg = fget8(SUEF);
	TeletextStyle = fget8(SUEF);

	VideoInit();

	TeletextEnabled = (VideoULA_ControlReg & 2) != 0;
	if (VideoULA_ControlReg & 16) HSyncModifier=8; else HSyncModifier=16;
	if (VideoULA_ControlReg & 2) HSyncModifier=12;

	if (Version >= 13) {
		VideoState.Addr = fget32(SUEF);
		VideoState.StartAddr = fget32(SUEF);
		VideoState.PixmapLine = fget32(SUEF);
		VideoState.FirstPixmapLine = fget32(SUEF);
		VideoState.PreviousFirstPixmapLine = fget32(SUEF);
		VideoState.LastPixmapLine = fget32(SUEF);
		VideoState.PreviousLastPixmapLine = fget32(SUEF);
		// VideoState.IsTeletext - computed in VideoStartOfFrame(), called from VideoInit()
		// VideoState.DataPtr - computed in VideoStartOfFrame(), called from VideoInit()
		VideoState.CharLine = fget32(SUEF);
		VideoState.InCharLineUp = fget32(SUEF);
		VideoState.VSyncState = fget32(SUEF);
		VideoState.IsNewTVFrame = fgetbool(SUEF);
		VideoState.InterlaceFrame = fgetbool(SUEF);
		VideoState.DoCA1Int = fgetbool(SUEF);
		ova = fget32(SUEF);
		ovn = fget32(SUEF);
		CursorFieldCount = fget32(SUEF);
		CursorOnState = fgetbool(SUEF);
		CurY = fget32(SUEF);
		Mode7FlashTrigger = fget32(SUEF);
		Mode7FlashOn = fgetbool(SUEF);
		VideoTriggerCount = TotalCycles + fget32(SUEF);
	}
}

/*-------------------------------------------------------------------------*/
void video_dumpstate(void) {
  int tmp;
  cerr << "video:\n";
  cerr << "  VideoULA_ControlReg=" << int(VideoULA_ControlReg) << "\n";
  cerr << "  VideoULA_Palette=";
  for(tmp=0;tmp<16;tmp++)
    cerr << int(VideoULA_Palette[tmp]) << " ";
  cerr << "\n  CRTC Control=" << int(CRTCControlReg) << "\n";
  cerr << "  CRTC_HorizontalTotal=" << int(CRTC_HorizontalTotal) << "\n";
  cerr << "  CRTC_HorizontalDisplayed=" << int(CRTC_HorizontalDisplayed)<< "\n";
  cerr << "  CRTC_HorizontalSyncPos=" << int(CRTC_HorizontalSyncPos)<< "\n";
  cerr << "  CRTC_SyncWidth=" << int(CRTC_SyncWidth)<< "\n";
  cerr << "  CRTC_VerticalTotal=" << int(CRTC_VerticalTotal)<< "\n";
  cerr << "  CRTC_VerticalTotalAdjust=" << int(CRTC_VerticalTotalAdjust)<< "\n";
  cerr << "  CRTC_VerticalDisplayed=" << int(CRTC_VerticalDisplayed)<< "\n";
  cerr << "  CRTC_VerticalSyncPos=" << int(CRTC_VerticalSyncPos)<< "\n";
  cerr << "  CRTC_InterlaceAndDelay=" << int(CRTC_InterlaceAndDelay)<< "\n";
  cerr << "  CRTC_ScanLinesPerChar=" << int(CRTC_ScanLinesPerChar)<< "\n";
  cerr << "  CRTC_CursorStart=" << int(CRTC_CursorStart)<< "\n";
  cerr << "  CRTC_CursorEnd=" << int(CRTC_CursorEnd)<< "\n";
  cerr << "  CRTC_ScreenStartHigh=" << int(CRTC_ScreenStartHigh)<< "\n";
  cerr << "  CRTC_ScreenStartLow=" << int(CRTC_ScreenStartLow)<< "\n";
  cerr << "  CRTC_CursorPosHigh=" << int(CRTC_CursorPosHigh)<< "\n";
  cerr << "  CRTC_CursorPosLow=" << int(CRTC_CursorPosLow)<< "\n";
  cerr << "  CRTC_LightPenHigh=" << int(CRTC_LightPenHigh)<< "\n";
  cerr << "  CRTC_LightPenLow=" << int(CRTC_LightPenLow)<< "\n";
}

void DebugVideoState()
{
	DebugDisplayInfo("");

	DebugDisplayInfoF("CRTC: HTot=%02X HDis=%02X HSyn=%02X SWid=%02X VTot=%02X VAdj=%02X VDis=%02X VSyn=%02X",
		(int)CRTC_HorizontalTotal,
		(int)CRTC_HorizontalDisplayed,
		(int)CRTC_HorizontalSyncPos,
		(int)CRTC_SyncWidth,
		(int)CRTC_VerticalTotal,
		(int)CRTC_VerticalTotalAdjust,
		(int)CRTC_VerticalDisplayed,
		(int)CRTC_VerticalSyncPos);

	DebugDisplayInfoF("CRTC: IntD=%02X SLCh=%02X CurS=%02X CurE=%02X ScrS=%02X%02X CurP=%02X%02X VidULA=%02X",
		(int)CRTC_InterlaceAndDelay,
		(int)CRTC_ScanLinesPerChar,
		(int)CRTC_CursorStart,
		(int)CRTC_CursorEnd,
		(int)CRTC_ScreenStartHigh,
		(int)CRTC_ScreenStartLow,
		(int)CRTC_CursorPosHigh,
		(int)CRTC_CursorPosLow,
		(int)VideoULA_ControlReg);
}

/*-------------------------------------------------------------------------*/
void VideoGetText(char *text, int line)
{
	unsigned char c;
	int x;

	text[0] = 0;
	text[1] = 0;

	if (!VideoState.IsTeletext || line >= CRTC_VerticalDisplayed)
		return;

	const unsigned char *dataPtr = BeebMemPtrWithWrapMode7(
		VideoState.StartAddr + line * CRTC_HorizontalDisplayed,
		CRTC_HorizontalDisplayed);

	for (x = 0; x < CRTC_HorizontalDisplayed; ++x)
	{
		c = dataPtr[x];
		if (isprint(c))
		{
			if (c == '_')
				c = '#';
			else if (c == '#')
				c = '£';
			else if (c == '`')
				c = '_';
			text[x] = c;
		}
		else
		{
			text[x] = ' ';
		}
	}
	text[x] = '\0';
}
