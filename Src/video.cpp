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
int FastTable_Valid=0;

typedef void (*LineRoutinePtr)(void);
LineRoutinePtr LineRoutine;

/* Translates middle bits of VideoULA_ControlReg to number of colours */
static int NColsLookup[]={16, 4, 2, 0 /* Not supported 16? */, 0, 16, 4, 2}; /* Based on AUG 379 */

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
int InitialOffset=0;
long ScreenAdjust=0; // Mode 7 Defaults.
long VScreenAdjust=0;
int VStart,HStart;
unsigned char HSyncModifier=9;
unsigned char TeletextEnabled=0;
char TeletextStyle=1; // Defines wether teletext will skip intermediate lines in order to speed up
int THalfMode=0; // 1 if to use half-mode (TeletextStyle=1 all the time)
int CurY=-1;
FILE *crtclog;

int ova,ovn; // mem ptr buffers

/* CharLine counts from the 'reference point' - i.e. the point at which we reset the address pointer - NOT
the point of the sync. If it is -ve its actually in the adjust time */
typedef struct {
  int Addr;       /* Address of start of next visible character line in beeb memory  - raw */
  int StartAddr;  /* Address of start of first character line in beeb memory  - raw */
  int PixmapLine; /* Current line in the pixmap */
  int FirstPixmapLine; /* The first pixmap line where something is visible.  Used to eliminate the 
                          blank virtical retrace lines at the top of the screen.  */
  int PreviousFirstPixmapLine; /* The first pixmap line on the previous frame */
  int LastPixmapLine; /* The last pixmap line where something is visible.  Used to eliminate the 
                          blank virtical retrace lines at the bottom of the screen.  */
  int PreviousLastPixmapLine; /* The last pixmap line on the previous frame */
  int IsTeletext; /* This frame is a teletext frame - do things differently */
  char *DataPtr;  /* Pointer into host memory of video data */

  int CharLine;   /* 6845 counts in characters vertically - 0 at the top , incs by 1 - -1 means we are in the bit before the actual display starts */
  int InCharLineUp; /* Scanline within a character line - counts up*/
  int VSyncState; // Cannot =0 in MSVC $NRM; /* When >0 VSync is high */
  int IsNewTVFrame; /* Specifies the start of a new TV frame, following VSync (used so we only calibrate speed once per frame) */
} VideoStateT;

static  VideoStateT VideoState;

int VideoTriggerCount=9999; /* Number of cycles before next scanline service */

/* first subscript is graphics flag (1 for graphics,2 for separated graphics), next is character, then scanline */
/* character is (valu &127)-32 */
static unsigned int EM7Font[3][96][20]; // 20 rows to account for "half pixels"

static int Mode7FlashOn=1; /* True if a flashing character in mode 7 is on */
static int Mode7DoubleHeightFlags[80]; /* Pessimistic size for this flags - if 1 then corresponding character on NEXT line is top half */
static int CurrentLineBottom=0;
static int NextLineBottom=0; // 1 if the next line of double height should be bottoms only

/* Flash every half second(?) i.e. 25 x 50Hz fields */
// No. On time is longer than off time. - according to my datasheet, its 0.75Hz with 3:1 ON:OFF ratio. - Richard Gellman
// cant see that myself.. i think it means on for 0.75 secs, off for 0.25 secs
#define MODE7FLASHFREQUENCY 25
#define MODE7ONFIELDS 37
#define MODE7OFFFIELDS 13
unsigned char CursorOnFields,CursorOffFields;
int CursorFieldCount=32;
unsigned char CursorOnState=1;
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
static void BuildMode7Font(void) {
  FILE *m7File;
  unsigned char m7cc,m7cy;
  unsigned int m7cb;
  unsigned int row1,row2,row3; // row builders for mode 7 graphics
  char TxtFnt[256];
  /* cout <<"Building mode 7 font data structures\n"; */
 // Build enhanced mode 7 font
  strcpy(TxtFnt,mainWin->GetAppPath());
  strcat(TxtFnt,"teletext.fnt");
  m7File=fopen(TxtFnt,"rb");
  if (m7File == NULL)
  {
    char errstr[200];
    sprintf(errstr, "Cannot open Teletext font file teletext.fnt");
    MessageBox(GETHWND,errstr,WindowTitle,MB_OK|MB_ICONERROR);
    exit(1);
  }
  for (m7cc=32;m7cc<=127;m7cc++) {
	  for (m7cy=0;m7cy<=17;m7cy++) {
		  m7cb=fgetc(m7File);
		  m7cb|=fgetc(m7File)<<8;
		  EM7Font[0][m7cc-32][m7cy+2]=m7cb<<2;
		  EM7Font[1][m7cc-32][m7cy+2]=m7cb<<2;
		  EM7Font[2][m7cc-32][m7cy+2]=m7cb<<2;
	  }
	  EM7Font[0][m7cc-32][0]=EM7Font[1][m7cc-32][0]=EM7Font[2][m7cc-32][0]=0;
	  EM7Font[0][m7cc-32][1]=EM7Font[1][m7cc-32][1]=EM7Font[2][m7cc-32][1]=0;
  }
  fclose(m7File);
  // Now fill in the graphics - this is built from an algorithm, but has certain lines/columns
  // blanked for separated graphics.
  for (m7cc=0;m7cc<96;m7cc++) {
	  // here's how it works: top two blocks: 1 & 2
	  // middle two blocks: 4 & 8
	  // bottom two blocks: 16 & 64
	  // its only a grpahics character if bit 5 (32) is clear.
	  if (!(m7cc & 32)) {
		  row1=0; row2=0; row3=0;
		  // left block has a value of 4032, right 63 and both 4095
		  if (m7cc & 1) row1|=4032;
		  if (m7cc & 2) row1|=63;
		  if (m7cc & 4) row2|=4032;
		  if (m7cc & 8) row2|=63;
		  if (m7cc & 16) row3|=4032;
		  if (m7cc & 64) row3|=63;
		  // now input these values into the array
		  // top row of blocks - continuous
		  EM7Font[1][m7cc][0]=EM7Font[1][m7cc][1]=EM7Font[1][m7cc][2]=row1;
		  EM7Font[1][m7cc][3]=EM7Font[1][m7cc][4]=EM7Font[1][m7cc][5]=row1;
		  // Separated
		  row1&=975; // insert gaps
		  EM7Font[2][m7cc][0]=EM7Font[2][m7cc][1]=EM7Font[2][m7cc][2]=row1;
		  EM7Font[2][m7cc][3]=row1; EM7Font[2][m7cc][4]=EM7Font[2][m7cc][5]=0;
		  // middle row of blocks - continuous
		  EM7Font[1][m7cc][6]=EM7Font[1][m7cc][7]=EM7Font[1][m7cc][8]=row2;
		  EM7Font[1][m7cc][9]=EM7Font[1][m7cc][10]=EM7Font[1][m7cc][11]=row2;
		  EM7Font[1][m7cc][12]=EM7Font[1][m7cc][13]=row2;
		  // Separated
		  row2&=975; // insert gaps
		  EM7Font[2][m7cc][6]=EM7Font[2][m7cc][7]=EM7Font[2][m7cc][8]=row2;
		  EM7Font[2][m7cc][9]=EM7Font[2][m7cc][10]=EM7Font[2][m7cc][11]=row2;
		  EM7Font[2][m7cc][12]=EM7Font[2][m7cc][13]=0;
		  // Bottom row - continuous
		  EM7Font[1][m7cc][14]=EM7Font[1][m7cc][15]=EM7Font[1][m7cc][16]=row3;
		  EM7Font[1][m7cc][17]=EM7Font[1][m7cc][18]=EM7Font[1][m7cc][19]=row3;
		  // Separated
		  row3&=975; // insert gaps
		  EM7Font[2][m7cc][14]=EM7Font[2][m7cc][15]=EM7Font[2][m7cc][16]=row3;
		  EM7Font[2][m7cc][17]=row3; EM7Font[2][m7cc][18]=EM7Font[2][m7cc][19]=0;
	  } // check for valid char to modify
  } // character loop.
}; /* BuildMode7Font */
/*-------------------------------------------------------------------------------------------------------------*/
static void DoFastTable16(void) {
  unsigned int beebpixvl,beebpixvr;
  unsigned int bplvopen,bplvtotal;
  unsigned char tmp;

  for(beebpixvl=0;beebpixvl<16;beebpixvl++) {
    bplvopen=((beebpixvl & 8)?128:0) |
             ((beebpixvl & 4)?32:0) |
             ((beebpixvl & 2)?8:0) |
             ((beebpixvl & 1)?2:0);
    for(beebpixvr=0;beebpixvr<16;beebpixvr++) {
      bplvtotal=bplvopen |
             ((beebpixvr & 8)?64:0) |
             ((beebpixvr & 4)?16:0) |
             ((beebpixvr & 2)?4:0) |
             ((beebpixvr & 1)?1:0);
      tmp=VideoULA_Palette[beebpixvl];
      if (tmp>7) {
        tmp&=7;
        if (VideoULA_ControlReg & 1) tmp^=7;
      };
      FastTable[bplvtotal].data[0]=FastTable[bplvtotal].data[1]=
        FastTable[bplvtotal].data[2]=FastTable[bplvtotal].data[3]=mainWin->cols[tmp];

      tmp=VideoULA_Palette[beebpixvr];
      if (tmp>7) {
        tmp&=7;
        if (VideoULA_ControlReg & 1) tmp^=7;
      };
      FastTable[bplvtotal].data[4]=FastTable[bplvtotal].data[5]=
        FastTable[bplvtotal].data[6]=FastTable[bplvtotal].data[7]=mainWin->cols[tmp];
    }; /* beebpixr */
  }; /* beebpixl */
}; /* DoFastTable16 */

/*-------------------------------------------------------------------------------------------------------------*/
static void DoFastTable16XStep8(void) {
  unsigned int beebpixvl,beebpixvr;
  unsigned int bplvopen,bplvtotal;
  unsigned char tmp;

  for(beebpixvl=0;beebpixvl<16;beebpixvl++) {
    bplvopen=((beebpixvl & 8)?128:0) |
             ((beebpixvl & 4)?32:0) |
             ((beebpixvl & 2)?8:0) |
             ((beebpixvl & 1)?2:0);
    for(beebpixvr=0;beebpixvr<16;beebpixvr++) {
      bplvtotal=bplvopen |
             ((beebpixvr & 8)?64:0) |
             ((beebpixvr & 4)?16:0) |
             ((beebpixvr & 2)?4:0) |
             ((beebpixvr & 1)?1:0);
      tmp=VideoULA_Palette[beebpixvl];
      if (tmp>7) {
        tmp&=7;
        if (VideoULA_ControlReg & 1) tmp^=7;
      };
      FastTableDWidth[bplvtotal].data[0]=FastTableDWidth[bplvtotal].data[1]=
        FastTableDWidth[bplvtotal].data[2]=FastTableDWidth[bplvtotal].data[3]=
      FastTableDWidth[bplvtotal].data[4]=FastTableDWidth[bplvtotal].data[5]=
        FastTableDWidth[bplvtotal].data[6]=FastTableDWidth[bplvtotal].data[7]=mainWin->cols[tmp];

      tmp=VideoULA_Palette[beebpixvr];
      if (tmp>7) {
        tmp&=7;
        if (VideoULA_ControlReg & 1) tmp^=7;
      };
      FastTableDWidth[bplvtotal].data[8]=FastTableDWidth[bplvtotal].data[9]=
        FastTableDWidth[bplvtotal].data[10]=FastTableDWidth[bplvtotal].data[11]=
      FastTableDWidth[bplvtotal].data[12]=FastTableDWidth[bplvtotal].data[13]=
        FastTableDWidth[bplvtotal].data[14]=FastTableDWidth[bplvtotal].data[15]=mainWin->cols[tmp];
    }; /* beebpixr */
  }; /* beebpixl */
}; /* DoFastTable16XStep8 */
/*-------------------------------------------------------------------------------------------------------------*/
/* Some guess work and experimentation has determined that the left most pixel uses bits 7,5,3,1 for the       */
/* palette address, the next uses 6,4,2,0, the next uses 5,3,1,H (H=High), then 5,2,0,H                        */
static void DoFastTable4(void) {
  unsigned char tmp;
  unsigned long beebpixv,pentry;

  for(beebpixv=0;beebpixv<256;beebpixv++) {
    pentry=((beebpixv & 128)?8:0)
           | ((beebpixv & 32)?4:0)
           | ((beebpixv & 8)?2:0)
           | ((beebpixv & 2)?1:0);
    tmp=VideoULA_Palette[pentry];
    if (tmp>7) {
      tmp&=7;
      if (VideoULA_ControlReg & 1) tmp^=7;
    };
    FastTable[beebpixv].data[0]=FastTable[beebpixv].data[1]=mainWin->cols[tmp];

    pentry=((beebpixv & 64)?8:0)
           | ((beebpixv & 16)?4:0)
           | ((beebpixv & 4)?2:0)
           | ((beebpixv & 1)?1:0);
    tmp=VideoULA_Palette[pentry];
    if (tmp>7) {
      tmp&=7;
      if (VideoULA_ControlReg & 1) tmp^=7;
    };
    FastTable[beebpixv].data[2]=FastTable[beebpixv].data[3]=mainWin->cols[tmp];

    pentry=((beebpixv & 32)?8:0)
           | ((beebpixv & 8)?4:0)
           | ((beebpixv & 2)?2:0)
           | 1;
    tmp=VideoULA_Palette[pentry];
    if (tmp>7) {
      tmp&=7;
      if (VideoULA_ControlReg & 1) tmp^=7;
    };
    FastTable[beebpixv].data[4]=FastTable[beebpixv].data[5]=mainWin->cols[tmp];
    pentry=((beebpixv & 16)?8:0)
           | ((beebpixv & 4)?4:0)
           | ((beebpixv & 1)?2:0)
           | 1;
    tmp=VideoULA_Palette[pentry];
    if (tmp>7) {
      tmp&=7;
      if (VideoULA_ControlReg & 1) tmp^=7;
    };
    FastTable[beebpixv].data[6]=FastTable[beebpixv].data[7]=mainWin->cols[tmp];
  }; /* beebpixv */
}; /* DoFastTable4 */

/*-------------------------------------------------------------------------------------------------------------*/
/* Some guess work and experimentation has determined that the left most pixel uses bits 7,5,3,1 for the       */
/* palette address, the next uses 6,4,2,0, the next uses 5,3,1,H (H=High), then 5,2,0,H                        */
static void DoFastTable4XStep4(void) {
  unsigned char tmp;
  unsigned long beebpixv,pentry;

  for(beebpixv=0;beebpixv<256;beebpixv++) {
    pentry=((beebpixv & 128)?8:0)
           | ((beebpixv & 32)?4:0)
           | ((beebpixv & 8)?2:0)
           | ((beebpixv & 2)?1:0);
    tmp=VideoULA_Palette[pentry];
    if (tmp>7) {
      tmp&=7;
      if (VideoULA_ControlReg & 1) tmp^=7;
    };
    FastTableDWidth[beebpixv].data[0]=FastTableDWidth[beebpixv].data[1]=
    FastTableDWidth[beebpixv].data[2]=FastTableDWidth[beebpixv].data[3]=mainWin->cols[tmp];

    pentry=((beebpixv & 64)?8:0)
           | ((beebpixv & 16)?4:0)
           | ((beebpixv & 4)?2:0)
           | ((beebpixv & 1)?1:0);
    tmp=VideoULA_Palette[pentry];
    if (tmp>7) {
      tmp&=7;
      if (VideoULA_ControlReg & 1) tmp^=7;
    };
    FastTableDWidth[beebpixv].data[4]=FastTableDWidth[beebpixv].data[5]=
    FastTableDWidth[beebpixv].data[6]=FastTableDWidth[beebpixv].data[7]=mainWin->cols[tmp];

    pentry=((beebpixv & 32)?8:0)
           | ((beebpixv & 8)?4:0)
           | ((beebpixv & 2)?2:0)
           | 1;
    tmp=VideoULA_Palette[pentry];
    if (tmp>7) {
      tmp&=7;
      if (VideoULA_ControlReg & 1) tmp^=7;
    };
    FastTableDWidth[beebpixv].data[8]=FastTableDWidth[beebpixv].data[9]=
    FastTableDWidth[beebpixv].data[10]=FastTableDWidth[beebpixv].data[11]=mainWin->cols[tmp];
    pentry=((beebpixv & 16)?8:0)
           | ((beebpixv & 4)?4:0)
           | ((beebpixv & 1)?2:0)
           | 1;
    tmp=VideoULA_Palette[pentry];
    if (tmp>7) {
      tmp&=7;
      if (VideoULA_ControlReg & 1) tmp^=7;
    };
    FastTableDWidth[beebpixv].data[12]=FastTableDWidth[beebpixv].data[13]=
    FastTableDWidth[beebpixv].data[14]=FastTableDWidth[beebpixv].data[15]=mainWin->cols[tmp];
  }; /* beebpixv */
}; /* DoFastTable4XStep4 */

/*-------------------------------------------------------------------------------------------------------------*/
/* Some guess work and experimentation has determined that the left most pixel uses the same pattern as mode 1 */
/* all the way upto the 5th pixel which uses 31hh then 20hh and hten 1hhh then 0hhhh                           */
static void DoFastTable2(void) {
  unsigned char tmp;
  unsigned long beebpixv,beebpixvt,pentry;
  int pix;

  for(beebpixv=0;beebpixv<256;beebpixv++) {
    beebpixvt=beebpixv;
    for(pix=0;pix<8;pix++) {
      pentry=((beebpixvt & 128)?8:0)
             | ((beebpixvt & 32)?4:0)
             | ((beebpixvt & 8)?2:0)
             | ((beebpixvt & 2)?1:0);
      beebpixvt<<=1;
      beebpixvt|=1;
      tmp=VideoULA_Palette[pentry];
      if (tmp>7) {
        tmp&=7;
        if (VideoULA_ControlReg & 1) tmp^=7;
      };
      FastTable[beebpixv].data[pix]=mainWin->cols[tmp];
    }; /* pix */
  }; /* beebpixv */
}; /* DoFastTable2 */

/*-------------------------------------------------------------------------------------------------------------*/
/* Some guess work and experimentation has determined that the left most pixel uses the same pattern as mode 1 */
/* all the way upto the 5th pixel which uses 31hh then 20hh and hten 1hhh then 0hhhh                           */
static void DoFastTable2XStep2(void) {
  unsigned char tmp;
  unsigned long beebpixv,beebpixvt,pentry;
  int pix;

  for(beebpixv=0;beebpixv<256;beebpixv++) {
    beebpixvt=beebpixv;
    for(pix=0;pix<8;pix++) {
      pentry=((beebpixvt & 128)?8:0)
             | ((beebpixvt & 32)?4:0)
             | ((beebpixvt & 8)?2:0)
             | ((beebpixvt & 2)?1:0);
      beebpixvt<<=1;
      beebpixvt|=1;
      tmp=VideoULA_Palette[pentry];
      if (tmp>7) {
        tmp&=7;
        if (VideoULA_ControlReg & 1) tmp^=7;
      };
      FastTableDWidth[beebpixv].data[pix*2]=FastTableDWidth[beebpixv].data[pix*2+1]=mainWin->cols[tmp];
    }; /* pix */
  }; /* beebpixv */
}; /* DoFastTable2XStep2 */

/*-------------------------------------------------------------------------------------------------------------*/
/* Check validity of fast table, and if invalid rebuild.
   The fast table accelerates the translation of beeb video memory
   values into X pixel values */
static void DoFastTable(void) {
  /* if it's already OK then quit */
  if (FastTable_Valid) return;

  if (!(CRTC_HorizontalDisplayed & 3)) {
    LineRoutine=(VideoULA_ControlReg & 0x10)?LowLevelDoScanLineNarrow:LowLevelDoScanLineWide;
  } else {
    LineRoutine=(VideoULA_ControlReg & 0x10)?LowLevelDoScanLineNarrowNot4Bytes:LowLevelDoScanLineWideNot4Bytes;
  };

  /* What happens next dpends on the number of colours */
  switch (NColsLookup[(VideoULA_ControlReg & 0x1c) >> 2]) {
    case 2:
      if (VideoULA_ControlReg & 0x10) {
        DoFastTable2();
      } else {
        DoFastTable2XStep2();
      };
      FastTable_Valid=1;     
      break;

    case 4:
      if (VideoULA_ControlReg & 0x10) {
        DoFastTable4();
      } else {
        DoFastTable4XStep4();
      };
      FastTable_Valid=1;     
      break;

    case 16:
      if (VideoULA_ControlReg & 0x10) {
        DoFastTable16();
      } else {
        DoFastTable16XStep8();
      };
      FastTable_Valid=1;
      break;

    default:
      break;
  }; /* Colours/pixel switch */
}; /* DoFastTable */

/*-------------------------------------------------------------------------------------------------------------*/
#define BEEB_DOTIME_SAMPLESIZE 50

static void VideoStartOfFrame(void) {
  static int InterlaceFrame=0;
  int CurStart;
  int IL_Multiplier;
#ifdef BEEB_DOTIME
  static int Have_GotTime=0;
  static struct tms previous,now;
  static int Time_FrameCount=0;

  double frametime;
  static CycleCountT OldCycles=0;
  
  int CurStart; 


  if (!Have_GotTime) {
    times(&previous);
    Time_FrameCount=-1;
    Have_GotTime=1;
  };

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
  if (mainWin && VideoState.IsNewTVFrame)	// RTW - only calibrate timing once per frame
  {
    VideoState.IsNewTVFrame = 0;
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
    InterlaceFrame^=1;
  }

  // Cursor update for blink. I thought I'd put it here, as this is where the mode 7 flash field thingy is too
  // - Richard Gellman
  if (CursorFieldCount<0) {
    CurStart = CRTC_CursorStart & 0x60;
    // 0 is cursor displays, but does not blink
    // 32 is no cursor
    // 64 is 1/16 fast blink
    // 96 is 1/32 slow blink
    if (CurStart==0) { CursorFieldCount=CursorOnFields; CursorOnState=1; }
    if (CurStart==32) { CursorFieldCount=CursorOffFields; CursorOnState=0; }
    if (CurStart==64) { CursorFieldCount=8; CursorOnState^=1; }
    if (CurStart==96) { CursorFieldCount=16; CursorOnState^=1; }
  }

  // RTW - The meaning of CharLine has changed: -1 no longer means that we are in the vertical
  // total adjust period, and this is no longer handled as if it were at the beginning of a new CRTC cycle.
  // Hence, here we always set CharLine to 0.
  VideoState.CharLine=0;
  VideoState.InCharLineUp=0;
  
  VideoState.IsTeletext=(VideoULA_ControlReg &2)>0;
  if (!VideoState.IsTeletext) {
    VideoState.Addr=VideoState.StartAddr=CRTC_ScreenStartLow+(CRTC_ScreenStartHigh<<8);
  } else {
    int tmphigh=CRTC_ScreenStartHigh;
    /* undo wrangling of start address - I don't understand why this should be - see p.372 of AUG for this info */
    tmphigh^=0x20;
    tmphigh+=0x74;
    tmphigh&=255;
    VideoState.Addr=VideoState.StartAddr=CRTC_ScreenStartLow+(tmphigh<<8);

	// O aye. this is the mode 7 flash section is it? Modified for corrected flash settings - Richard Gellman
    if (Mode7FlashTrigger<0) {
		Mode7FlashTrigger=(Mode7FlashOn)?MODE7OFFFIELDS:MODE7ONFIELDS;
      Mode7FlashOn^=1; /* toggle flash state */
    };
  };

  IL_Multiplier=(CRTC_InterlaceAndDelay&1)?2:1;
  if (InterlaceFrame) {
    IncTrigger((IL_Multiplier*(CRTC_HorizontalTotal+1)*((VideoULA_ControlReg & 16)?1:2)),VideoTriggerCount); /* Number of 2MHz cycles until another scanline needs doing */
  } else {
    IncTrigger(((CRTC_HorizontalTotal+1)*((VideoULA_ControlReg & 16)?1:2)),VideoTriggerCount); /* Number of 2MHz cycles until another scanline needs doing */
  };

  TeleTextPoll();

}; /* VideoStartOfFrame */


/*--------------------------------------------------------------------------*/
/* Scanline processing for modes with fast 6845 clock - i.e. narrow pixels  */
static void LowLevelDoScanLineNarrow() {
  unsigned char *CurrentPtr;
  int BytesToGo=CRTC_HorizontalDisplayed;
  EightUChars *vidPtr=mainWin->GetLinePtr(VideoState.PixmapLine);

  /* If the step is 4 then each byte corresponds to one entry in the fasttable
     and thus we can copy it really easily (and fast!) */
  CurrentPtr=(unsigned char *)VideoState.DataPtr+VideoState.InCharLineUp;

  /* This should help the compiler - it doesn't need to test for end of loop
     except every 4 entries */
  BytesToGo/=4;
  for(;BytesToGo;CurrentPtr+=32,BytesToGo--) {
    *(vidPtr++)=FastTable[*CurrentPtr];
    *(vidPtr++)=FastTable[*(CurrentPtr+8)];
    *(vidPtr++)=FastTable[*(CurrentPtr+16)];
    *(vidPtr++)=FastTable[*(CurrentPtr+24)];
  };
}; /* LowLevelDoScanLineNarrow() */

/*--------------------------------------------------------------------------*/
/* Scanline processing for modes with fast 6845 clock - i.e. narrow pixels  */
/* This version handles screen modes where there is not a multiple of 4     */
/* bytes per scanline.                                                      */
static void LowLevelDoScanLineNarrowNot4Bytes() {
  unsigned char *CurrentPtr;
  int BytesToGo=CRTC_HorizontalDisplayed;
  EightUChars *vidPtr=mainWin->GetLinePtr(VideoState.PixmapLine);

  /* If the step is 4 then each byte corresponds to one entry in the fasttable
     and thus we can copy it really easily (and fast!) */
    CurrentPtr=(unsigned char *)VideoState.DataPtr+VideoState.InCharLineUp;

  for(;BytesToGo;CurrentPtr+=8,BytesToGo--)
    (vidPtr++)->eightbyte=FastTable[*CurrentPtr].eightbyte;
}; /* LowLevelDoScanLineNarrowNot4Bytes() */

/*-----------------------------------------------------------------------------*/
/* Scanline processing for the low clock rate modes                            */
static void LowLevelDoScanLineWide() {
  unsigned char *CurrentPtr;
  int BytesToGo=CRTC_HorizontalDisplayed;
  SixteenUChars *vidPtr=mainWin->GetLinePtr16(VideoState.PixmapLine);

  /* If the step is 4 then each byte corresponds to one entry in the fasttable
     and thus we can copy it really easily (and fast!) */
    CurrentPtr=(unsigned char *)VideoState.DataPtr+VideoState.InCharLineUp;

  /* This should help the compiler - it doesn't need to test for end of loop
     except every 4 entries */
  BytesToGo/=4;
  for(;BytesToGo;CurrentPtr+=32,BytesToGo--) {
    *(vidPtr++)=FastTableDWidth[*CurrentPtr];
    *(vidPtr++)=FastTableDWidth[*(CurrentPtr+8)];
    *(vidPtr++)=FastTableDWidth[*(CurrentPtr+16)];
    *(vidPtr++)=FastTableDWidth[*(CurrentPtr+24)];
  };
}; /* LowLevelDoScanLineWide */

/*-----------------------------------------------------------------------------*/
/* Scanline processing for the low clock rate modes                            */
/* This version handles cases where the screen width is not divisible by 4     */
static void LowLevelDoScanLineWideNot4Bytes() {
  unsigned char *CurrentPtr;
  int BytesToGo=CRTC_HorizontalDisplayed;
  SixteenUChars *vidPtr=mainWin->GetLinePtr16(VideoState.PixmapLine);

  CurrentPtr=(unsigned char *)VideoState.DataPtr+VideoState.InCharLineUp;

  for(;BytesToGo;CurrentPtr+=8,BytesToGo--)
    *(vidPtr++)=FastTableDWidth[*CurrentPtr];
}; /* LowLevelDoScanLineWideNot4Bytes */

/*-------------------------------------------------------------------------------------------------------------*/
/* Do all the pixel rows for one row of teletext characters                                                    */
static void DoMode7Row(void) {
  char *CurrentPtr=VideoState.DataPtr;
  int CurrentChar;
  int XStep;
  unsigned char byte;
  unsigned int tmp;

  unsigned int Foreground=mainWin->cols[7];
  unsigned int ActualForeground;
  unsigned int Background=mainWin->cols[0];
  int Flash=0; /* i.e. steady */
  int DoubleHeight=0; /* Normal */
  int Graphics=0; /* I.e. alpha */
  int Separated=0; /* i.e. continuous graphics */
  int HoldGraph=0; /* I.e. don't hold graphics - I don't know what hold graphics is anyway! */
  // That's ok. Nobody else does either, and nor do I. - Richard Gellman.
  int HoldGraphChar=32; // AHA! we know what it is now, this is the character to "hold" during control codes
  unsigned int CurrentCol[20]={0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff
  ,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff};
  int CurrentLen[20]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
  int CurrentStartX[20]={0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0};
  int CurrentScanLine;
  int CurrentX=0;
  int CurrentPixel;
  unsigned int col;
  int FontTypeIndex=0; /* 0=alpha, 1=contiguous graphics, 2=separated graphics */

  if (CRTC_HorizontalDisplayed>80) return; /* Not possible on beeb - and would break the double height lookup array */

  XStep=1;

  for(CurrentChar=0;CurrentChar<CRTC_HorizontalDisplayed;CurrentChar++) {
    byte=CurrentPtr[CurrentChar]; 
    if (byte<32) byte+=128; // fix for naughty programs that use 7-bit control codes - Richard Gellman
    if ((byte & 32) && (Graphics)) HoldGraphChar=byte;
    if ((byte>=128) && (byte<=159)) {
		switch (byte) {
	    case 129:
        case 130:
        case 131:
        case 132:
        case 133:
        case 134:
        case 135:
          Foreground=mainWin->cols[byte-128];
          Graphics=0;
          break;

        case 136:
          Flash=1;
          break;

        case 137:
          Flash=0;
          break;

        case 140:
          DoubleHeight=0;
          break;

        case 141:
		  if (!CurrentLineBottom) NextLineBottom=1;
          DoubleHeight=1;
          break;

        case 145:
        case 146:
        case 147:
        case 148:
        case 149:
        case 150:
        case 151:
          Foreground=mainWin->cols[byte-144];
          Graphics=1;
          break;

        case 152: /* Conceal display - not sure about this */
          Foreground=Background;
          break;

        case 153:
          Separated=0;
          break;

        case 154:
          Separated=1;
          break;

        case 156:
          Background=mainWin->cols[0];
          break;

        case 157:
          Background=Foreground;
          break;

        case 158:
          HoldGraph=1;
          break;

        case 159:
          HoldGraph=0;
          break;
      }; /* Special character switch */
	  // This next line hides any non double height characters on the bottom line
      /* Fudge so that the special character is just displayed in background */
      if ((HoldGraph==1) && (Graphics==1)) byte=HoldGraphChar; else byte=32;
      FontTypeIndex=Graphics?(Separated?2:1):0;
    }; /* test for special character */
	if ((CurrentLineBottom) && ((byte&127)>31) && (!DoubleHeight)) byte=32;
	if ((CRTC_ScanLinesPerChar<=9) || (THalfMode)) TeletextStyle=2; else TeletextStyle=1;
    /* Top bit never reaches character generator */
    byte&=127;
    /* Our font table goes from character 32 up */
    if (byte<32) byte=0; else byte-=32; 

    /* Conceal flashed text if necessary */
    ActualForeground=(Flash && !Mode7FlashOn)?Background:Foreground;
    if (!DoubleHeight) {
      for(CurrentScanLine=0+(TeletextStyle-1);CurrentScanLine<20;CurrentScanLine+=TeletextStyle) {
        tmp=EM7Font[FontTypeIndex][byte][CurrentScanLine];
		//tmp=1365;
        if ((tmp==0) || (tmp==255)) {
          col=(tmp==0)?Background:ActualForeground;
          if (col==CurrentCol[CurrentScanLine]) CurrentLen[CurrentScanLine]+=12*XStep; else {
            if (CurrentLen[CurrentScanLine])
              mainWin->doHorizLine(CurrentCol[CurrentScanLine],VideoState.PixmapLine+CurrentScanLine,CurrentStartX[CurrentScanLine],CurrentLen[CurrentScanLine]);
            CurrentCol[CurrentScanLine]=col;
            CurrentStartX[CurrentScanLine]=CurrentX;
            CurrentLen[CurrentScanLine]=12*XStep;

          }; /* same colour */
        } else {
          for(CurrentPixel=0x800;CurrentPixel;CurrentPixel=CurrentPixel>>1) {
            /* Background or foreground ? */
            col=(tmp & CurrentPixel)?ActualForeground:Background;

            /* Do we need to draw ? */
            if (col==CurrentCol[CurrentScanLine]) CurrentLen[CurrentScanLine]+=XStep; else {
              if (CurrentLen[CurrentScanLine]) 
                mainWin->doHorizLine(CurrentCol[CurrentScanLine],VideoState.PixmapLine+CurrentScanLine,CurrentStartX[CurrentScanLine],CurrentLen[CurrentScanLine]);
              CurrentCol[CurrentScanLine]=col;
              CurrentStartX[CurrentScanLine]=CurrentX;
              CurrentLen[CurrentScanLine]=XStep;
            }; /* Fore/back ground */
            CurrentX+=XStep;
          }; /* Pixel within byte */
          CurrentX-=12*XStep;
        }; /* tmp!=0 */
      }; /* Scanline for */
      CurrentX+=12*XStep;
      Mode7DoubleHeightFlags[CurrentChar]=1; /* Not double height - so if the next line is double height it will be top half */
    } else {
      int ActualScanLine;
      /* Double height! */
      for(CurrentPixel=0x800;CurrentPixel;CurrentPixel=CurrentPixel>>1) {
        for(CurrentScanLine=0+(TeletextStyle-1);CurrentScanLine<20;CurrentScanLine+=TeletextStyle) {
          if (!CurrentLineBottom) ActualScanLine=CurrentScanLine >> 1; else ActualScanLine=10+(CurrentScanLine>>1);
          /* Background or foreground ? */
          col=(EM7Font[FontTypeIndex][byte][ActualScanLine] & CurrentPixel)?ActualForeground:Background;

          /* Do we need to draw ? */
          if (col==CurrentCol[CurrentScanLine]) CurrentLen[CurrentScanLine]+=XStep; else {
            if (CurrentLen[CurrentScanLine])  {
              mainWin->doHorizLine(CurrentCol[CurrentScanLine],VideoState.PixmapLine+CurrentScanLine,CurrentStartX[CurrentScanLine],CurrentLen[CurrentScanLine]);
            };
            CurrentCol[CurrentScanLine]=col;
            CurrentStartX[CurrentScanLine]=CurrentX;
            CurrentLen[CurrentScanLine]=XStep;
          }; /* Fore/back ground */
        }; /* Scanline for */
        CurrentX+=XStep;
      }; /* Pixel within byte */
      Mode7DoubleHeightFlags[CurrentChar]^=1; /* Not double height - so if the next line is double height it will be top half */
	};
  }; /* character loop */

  /* Finish off right bits of scan line */
  for(CurrentScanLine=0+(TeletextStyle-1);CurrentScanLine<20;CurrentScanLine+=TeletextStyle) {
    if (CurrentLen[CurrentScanLine])
      mainWin->doHorizLine(CurrentCol[CurrentScanLine],VideoState.PixmapLine+CurrentScanLine,CurrentStartX[CurrentScanLine],CurrentLen[CurrentScanLine]);
  };
  CurrentLineBottom=NextLineBottom;
  NextLineBottom=0;
}; /* DoMode7Row */
/*-------------------------------------------------------------------------------------------------------------*/
/* Actually does the work of decoding beeb memory and plotting the line to X */
static void LowLevelDoScanLine() {
  /* Update acceleration tables */
  DoFastTable();
  if (FastTable_Valid) LineRoutine();
}; /* LowLevelDoScanLine */

void RedoMPTR(void) {
	if (VideoState.IsTeletext) VideoState.DataPtr=BeebMemPtrWithWrapMo7(ova,ovn);
	if (!VideoState.IsTeletext) VideoState.DataPtr=BeebMemPtrWithWrap(ova,ovn);
	//FastTable_Valid=0;
}

/*-------------------------------------------------------------------------------------------------------------*/
void VideoDoScanLine(void) {
  int l;
  /* cerr << "CharLine=" << VideoState.CharLine << " InCharLineUp=" << VideoState.InCharLineUp << "\n"; */
  if (VideoState.IsTeletext) {
    static int DoCA1Int=0;
    if (DoCA1Int) {
      SysVIATriggerCA1Int(0);
      DoCA1Int=0;
    }; 

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
      VideoState.DataPtr=BeebMemPtrWithWrapMo7(VideoState.Addr,CRTC_HorizontalDisplayed);
      VideoState.Addr+=CRTC_HorizontalDisplayed;
      if (!FrameNum) DoMode7Row();
      VideoState.PixmapLine+=20;
    };

    /* Move onto next physical scanline as far as timing is concerned */
    VideoState.InCharLineUp+=1;
	// RTW - Mode 7 hardwired for now. Assume 10 scanlines per character regardless (actually 9.5 but god knows how that works)
	if (VideoState.CharLine<=CRTC_VerticalTotal && VideoState.InCharLineUp>9) {
      VideoState.CharLine++;
      VideoState.InCharLineUp=0;
    }

	// RTW - check if we have reached the end of the PAL frame.
	// This whole thing is a bit hardwired and kludged. Should really be emulating VSync position 'properly' like Modes 0-6. 
	if (VideoState.CharLine>CRTC_VerticalTotal && VideoState.InCharLineUp>=CRTC_VerticalTotalAdjust) {
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
	  VideoState.IsNewTVFrame = 1;
      VideoStartOfFrame();
      VideoState.PreviousLastPixmapLine=VideoState.PixmapLine;
      VideoState.PixmapLine=0;
      SysVIATriggerCA1Int(1);
      DoCA1Int=1;
    } else {
	  // RTW- set timer till the next scanline update (this is now nice and simple)
      IncTrigger((CRTC_HorizontalTotal+1)*((VideoULA_ControlReg & 16)?1:2),VideoTriggerCount);
    };
  } else {
    /* Non teletext. */

	// Handle VSync
	// RTW - this was moved to the top so that we can correctly set R7=0,
	// i.e. we can catch it before the line counters are incremented
    if (VideoState.VSyncState) {
      if (!(--VideoState.VSyncState)) {
        SysVIATriggerCA1Int(0);
      };
    }

	if ((VideoState.VSyncState==0) && (VideoState.CharLine==CRTC_VerticalSyncPos) && (VideoState.InCharLineUp == 0)) {
      // Nothing displayed?
      if (VideoState.FirstPixmapLine<0)
        VideoState.FirstPixmapLine=0;

      VideoState.PreviousFirstPixmapLine=VideoState.FirstPixmapLine;
      VideoState.FirstPixmapLine=-1;
      VideoState.PreviousLastPixmapLine=VideoState.LastPixmapLine;
      VideoState.LastPixmapLine=0;
      VideoState.PixmapLine=0;
      VideoState.IsNewTVFrame = 1;

      SysVIATriggerCA1Int(1);
      VideoState.VSyncState=(CRTC_SyncWidth>>4);
    };

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
      };
  
      if ((VideoState.InCharLineUp<8) && ((CRTC_InterlaceAndDelay & 0x30)!=48)) {
        if (!FrameNum)
          LowLevelDoScanLine();
      }
    }

	// See if we are at the cursor line
	if (CurY == -1 && VideoState.Addr > (CRTC_CursorPosLow+(CRTC_CursorPosHigh<<8)))
		CurY = VideoState.PixmapLine;

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
    };

	// RTW - neater way of detecting the end of the PAL frame, which doesn't require making a special case
	// of the vertical total adjust period.
	if (VideoState.CharLine>CRTC_VerticalTotal && VideoState.InCharLineUp>=CRTC_VerticalTotalAdjust) {
      VScreenAdjust=0;
      if (!FrameNum && VideoState.IsNewTVFrame) {
        VideoAddCursor();
        VideoAddLEDs();
		CurY=-1;
        int n = VideoState.PreviousLastPixmapLine-VideoState.PreviousFirstPixmapLine+1;
		if (n < 0)
		  n += MAX_VIDEO_SCAN_LINES;

		int startLine = 32;
		if (n > 248 && VideoState.PreviousFirstPixmapLine >= 40)
		{
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
  }; /* Teletext if */
}; /* VideoDoScanLine */

/*-------------------------------------------------------------------------------------------------------------*/
void AdjustVideo() {
	ActualScreenWidth=CRTC_HorizontalDisplayed*HSyncModifier;
	if (ActualScreenWidth>800) ActualScreenWidth=800;
	if (ActualScreenWidth<640) ActualScreenWidth=640;

	InitialOffset=0-(((CRTC_HorizontalTotal+1)/2)-((HSyncModifier==8)?40:20));
	HStart=InitialOffset+((CRTC_HorizontalTotal+1)-(CRTC_HorizontalSyncPos+(CRTC_SyncWidth&15)));
	HStart+=(HSyncModifier==8)?2:1;
	if (TeletextEnabled) HStart+=2;
	if (HStart<0) HStart=0;
	ScreenAdjust=(HStart*HSyncModifier)+((VScreenAdjust>0)?(VScreenAdjust*800):0);
}
/*-------------------------------------------------------------------------------------------------------------*/
void VideoInit(void) {
//  char *environptr;
  VideoStartOfFrame();
  ova=0x3000; ovn=640;
  VideoState.DataPtr=BeebMemPtrWithWrap(0x3000,640);
  SetTrigger(99,VideoTriggerCount); /* Give time for OS to set mode up before doing anything silly */
  FastTable_Valid=0;
  BuildMode7Font();

#ifndef WIN32
  environptr=getenv("BeebVideoRefreshFreq");
  if (environptr!=NULL) Video_RefreshFrequency=atoi(environptr);
  if (Video_RefreshFrequency<1) Video_RefreshFrequency=1;
#endif

  FrameNum=Video_RefreshFrequency;
  VideoState.PixmapLine=0;
  VideoState.FirstPixmapLine=-1;
  VideoState.PreviousFirstPixmapLine=0;
  VideoState.LastPixmapLine=0;
  VideoState.PreviousLastPixmapLine=256;
  VideoState.IsNewTVFrame=0;
  CurY=-1;
  //AdjustVideo();
//  crtclog=fopen("/crtc.log","wb");
}; /* VideoInit */


/*-------------------------------------------------------------------------------------------------------------*/
void CRTCWrite(int Address, int Value) {
  Value&=0xff;
  if (Address & 1) {
//	if (CRTCControlReg<14) { fputc(CRTCControlReg,crtclog); fputc(Value,crtclog); }
//	if (CRTCControlReg<14) {
//		fprintf(crtclog,"%d (%02X) Written to register %d from %04X\n",Value,Value,CRTCControlReg,ProgramCounter);
//	}

	if (DebugEnabled && CRTCControlReg<14) {
		char info[200];
		sprintf(info, "CRTC: Write register %X value %02X", (int)CRTCControlReg, Value);
		DebugDisplayTrace(DEBUG_VIDEO, true, info);
	}

	switch (CRTCControlReg) {
      case 0:
        CRTC_HorizontalTotal=Value;
		InitialOffset=0-(((CRTC_HorizontalTotal+1)/2)-((HSyncModifier==8)?40:20));
		AdjustVideo();
        break;

      case 1:
        CRTC_HorizontalDisplayed=Value;
		if (CRTC_HorizontalDisplayed > 127)
			CRTC_HorizontalDisplayed = 127;
        FastTable_Valid=0;
		AdjustVideo();
		break;

      case 2:
		CRTC_HorizontalSyncPos=Value;
		AdjustVideo();
        break;

      case 3:
        CRTC_SyncWidth=Value;
		AdjustVideo();
//		fprintf(crtclog,"V Sync width: %d\n",(Value>>4)&15);
        break;

      case 4:
        CRTC_VerticalTotal=Value;
		AdjustVideo();
        break;

      case 5:
        CRTC_VerticalTotalAdjust=Value & 0x1f;  // 5 bit register
//		fprintf(crtclog,"Vertical Total Adjust: %d\n",Value);
		AdjustVideo();
        break;

      case 6:
        CRTC_VerticalDisplayed=Value;
		AdjustVideo();
		break;

      case 7:
        CRTC_VerticalSyncPos=(Value & 0x7f);
        AdjustVideo();
		break;

      case 8:
        CRTC_InterlaceAndDelay=Value;
        break;

      case 9:
        CRTC_ScanLinesPerChar=Value;
        break;

      case 10:
        CRTC_CursorStart=Value;
        break;

      case 11:
        CRTC_CursorEnd=Value;
        break;

      case 12:
        CRTC_ScreenStartHigh=Value;
//		fprintf(crtclog,"Screen now at &%02x%02x\n",Value,CRTC_ScreenStartLow);
        break;

      case 13:
        CRTC_ScreenStartLow=Value;
//		fprintf(crtclog,"Screen now at &%02x%02x\n",CRTC_ScreenStartHigh,Value);
        break;

      case 14:
        CRTC_CursorPosHigh=Value & 0x3f; /* Cursor high only has 6 bits */
        break;

      case 15:
        CRTC_CursorPosLow=Value & 0xff;
        break;

      default: /* In case the user wrote a duff control register value */
        break;
    }; /* CRTCWrite switch */
    /* cerr << "CRTCWrite RegNum=" << int(CRTCControlReg) << " Value=" << Value << "\n"; */
  } else {
    CRTCControlReg=Value & 0x1f;
  };
}; /* CRTCWrite */

/*-------------------------------------------------------------------------------------------------------------*/
int CRTCRead(int Address) {
  if (Address & 1) {
    switch (CRTCControlReg) {
      case 14:
        return(CRTC_CursorPosHigh);
      case 15:
        return(CRTC_CursorPosLow);
      case 16:
        return(CRTC_LightPenHigh); /* Perhaps tie to mouse pointer ? */
      case 17:
        return(CRTC_LightPenLow);
      default:
        break;
    }; /* CRTC Read switch */
  } else {
    return(0); /* Rockwell part has bits 5,6,7 used - bit 6 is set when LPEN is received, bit 5 when in vertical retrace */
  }
return(0);	// Keeep MSVC happy $NRM
}; /* CRTCRead */

/*-------------------------------------------------------------------------------------------------------------*/
void VideoULAWrite(int Address, int Value) {
  int oldValue;
  if (Address & 1) {
    VideoULA_Palette[(Value & 0xf0)>>4]=(Value & 0xf) ^ 7;
    FastTable_Valid=0;
    /* cerr << "Palette reg " << ((Value & 0xf0)>>4) << " now has value " << ((Value & 0xf) ^ 7) << "\n"; */
	//fprintf(crtclog,"Pallette written to at line %d\n",VideoState.PixmapLine);
  } else {
	oldValue=VideoULA_ControlReg;
    VideoULA_ControlReg=Value;
    FastTable_Valid=0; /* Could be more selective and only do it if no.of.cols bit changes */
    /* cerr << "VidULA Ctrl reg write " << hex << Value << "\n"; */
	// Adjust HSyncModifier
	if (VideoULA_ControlReg & 16) HSyncModifier=8; else HSyncModifier=16;
	if (VideoULA_ControlReg & 2) HSyncModifier=12;
	// number of pixels per CRTC character (on our screen)
	if (Value & 2) TeletextEnabled=1; else TeletextEnabled=0;
	if ((Value&2)^(oldValue&2)) { ScreenAdjust=0; }
	AdjustVideo();
  };
}; /* VidULAWrite */

/*-------------------------------------------------------------------------------------------------------------*/
int VideoULARead(int Address) {
  return(Address); /* Read not defined from Video ULA */
}; /* VidULARead */

/*-------------------------------------------------------------------------------------------------------------*/
static void VideoAddCursor(void) {
	static int CurSizes[] = { 2,1,0,0,4,2,0,4 };
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
		CurEnd = CRTC_CursorEnd ;
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
				if (CursorOnState)
				 mainWin->doInvHorizLine(mainWin->cols[7], CurY + y, CurX, CurSize);
			}
		}
	}
}

void VideoAddLEDs(void) {
	// now add some keyboard leds
	if (LEDs.ShowKB) {
		if (MachineType==3) mainWin->doLED(4,TRUE); else mainWin->doLED(4,LEDs.Motor);
		mainWin->doLED(14,LEDs.CapsLock);
		mainWin->doLED(24,LEDs.ShiftLock);
	}
	if (LEDs.ShowDisc)  {
		int adj = (TeletextEnabled ? 86 : 0);
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
	fput32(47,SUEF);
	// save the registers now
	fputc(CRTC_HorizontalTotal,SUEF);
	fputc(CRTC_HorizontalDisplayed,SUEF);
	fputc(CRTC_HorizontalSyncPos,SUEF);
	fputc(CRTC_SyncWidth,SUEF);
	fputc(CRTC_VerticalTotal,SUEF);
	fputc(CRTC_VerticalTotalAdjust,SUEF);
	fputc(CRTC_VerticalDisplayed,SUEF);
	fputc(CRTC_VerticalSyncPos,SUEF);
	fputc(CRTC_InterlaceAndDelay,SUEF);
	fputc(CRTC_ScanLinesPerChar,SUEF);
	fputc(CRTC_CursorStart,SUEF);
	fputc(CRTC_CursorEnd,SUEF);
	fputc(CRTC_ScreenStartHigh,SUEF);
	fputc(CRTC_ScreenStartLow,SUEF);
	fputc(CRTC_CursorPosHigh,SUEF);
	fputc(CRTC_CursorPosLow,SUEF);
	fputc(CRTC_LightPenHigh,SUEF);
	fputc(CRTC_LightPenLow,SUEF);
	// VIDPROC
	fputc(VideoULA_ControlReg,SUEF);
	for (int col = 0; col < 16; ++col)
		fputc(VideoULA_Palette[col] ^ 7,SUEF); /* Use real ULA values */
	fput16(ActualScreenWidth,SUEF);
	fput32(ScreenAdjust,SUEF);
	fputc(CRTCControlReg,SUEF);
	fputc(TeletextStyle,SUEF);
	fput32(0,SUEF); // Pad out
}

void LoadVideoUEF(FILE *SUEF) {
	CRTC_HorizontalTotal=fgetc(SUEF);
	CRTC_HorizontalDisplayed=fgetc(SUEF);
	CRTC_HorizontalSyncPos=fgetc(SUEF);
	CRTC_SyncWidth=fgetc(SUEF);
	CRTC_VerticalTotal=fgetc(SUEF);
	CRTC_VerticalTotalAdjust=fgetc(SUEF);
	CRTC_VerticalDisplayed=fgetc(SUEF);
	CRTC_VerticalSyncPos=fgetc(SUEF);
	CRTC_InterlaceAndDelay=fgetc(SUEF);
	CRTC_ScanLinesPerChar=fgetc(SUEF);
	CRTC_CursorStart=fgetc(SUEF);
	CRTC_CursorEnd=fgetc(SUEF);
	CRTC_ScreenStartHigh=fgetc(SUEF);
	CRTC_ScreenStartLow=fgetc(SUEF);
	CRTC_CursorPosHigh=fgetc(SUEF);
	CRTC_CursorPosLow=fgetc(SUEF);
	CRTC_LightPenHigh=fgetc(SUEF);
	CRTC_LightPenLow=fgetc(SUEF);
	// VIDPROC
	VideoULA_ControlReg=fgetc(SUEF);
	for (int col = 0; col < 16; ++col)
		VideoULA_Palette[col]=fgetc(SUEF)^7; /* Use real ULA values */
	ActualScreenWidth=fget16(SUEF);
	ScreenAdjust=fget32(SUEF);
	CRTCControlReg=fgetc(SUEF);
	TeletextStyle=fgetc(SUEF);
	VideoInit();
	if (VideoULA_ControlReg & 2) TeletextEnabled=1; else TeletextEnabled=0;
	if (VideoULA_ControlReg & 16) HSyncModifier=8; else HSyncModifier=16;
	if (VideoULA_ControlReg & 2) HSyncModifier=12;
	//SetTrigger(99,VideoTriggerCount);
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
}; /* video_dumpstate */

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

	char *dataPtr = BeebMemPtrWithWrapMo7(
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
