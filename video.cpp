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
/* Video handling -          David Alan Gilbert */

/* Version 2 - 24/12/94 - Designed to emulate start of frame interrupt
   correctly */

/* Mike Wyatt 7/6/97 - Added cursor display and Win32 port */

#include "iostream.h"
#include <stdlib.h>

#include "6502core.h"
#include "beebmem.h"
#include "beebwin.h"
#include "main.h"
#include "mode7font.h"
#include "sysvia.h"
#include "video.h"

#ifdef BEEB_DOTIME
#include <sys/times.h>
#ifdef SUNOS
#include <sys/param.h>
#endif
#ifdef HPUX
#include <unistd.h>
#endif
#endif

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
static int NColsLookup[]={16, 4, 2, 0 /* Not supported 16? */, 0 /* ???? */, 16, 4, 2}; /* Based on AUG 379 */

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

/* CharLine counts from the 'reference point' - i.e. the point at which we reset the address pointer - NOT
the point of the sync. If it is -ve its actually in the adjust time */
typedef struct {
  int Addr;       /* Address of start of next visible character line in beeb memory  - raw */
  int PixmapLine; /* Current line in the pixmap */
  int PreviousFinalPixmapLine; /* The last pixmap line on the previous frame */
  int IsTeletext; /* This frame is a teletext frame - do things differently */
  char *DataPtr;  /* Pointer into host memory of video data */

  int CharLine;   /* 6845 counts in characters vertically - 0 at the top , incs by 1 - -1 means we are in the bit before the actual display starts */
  int InCharLine; /* Scanline within a character line - counts down*/
  int InCharLineUp; /* Scanline within a character line - counts up*/
  int VSyncState; // Cannot =0 in MSVC $NRM; /* When >0 VSync is high */
} VideoStateT;

static  VideoStateT VideoState;

int VideoTriggerCount=9999; /* Number of cycles before next scanline service */

/* first subscript is graphics flag (1 for graphics,2 for separated graphics), next is character, then scanline */
/* character is (valu &127)-32 */
static unsigned char Mode7Font[3][96][9];

static int Mode7FlashOn=1; /* True if a flashing character in mode 7 is on */
static int Mode7DoubleHeightFlags[80]; /* Pessimistic size for this flags - if 1 then corresponding character on NEXT line is top half */

/* Flash every half second(???) i.e. 25 x 50Hz fields */
#define MODE7FLASHFREQUENCY 25
int Mode7FlashTrigger=MODE7FLASHFREQUENCY;

/* If 1 then refresh on every display, else refresh every n'th display */
int Video_RefreshFrequency=1;
/* The number of the current frame - starts at Video_RefreshFrequency - at 0 actually refresh */
static int FrameNum=0;

static void LowLevelDoScanLineNarrow();
static void LowLevelDoScanLineWide();
static void LowLevelDoScanLineNarrowNot4Bytes();
static void LowLevelDoScanLineWideNot4Bytes();
static void VideoAddCursor(void);
/*-------------------------------------------------------------------------------------------------------------*/
static void BuildMode7Font(void) {
  int presentchar,presentline,presentpixel,presgraph;
  char *tempptr;
  unsigned char  tempvalue;
  /* cout <<"Building mode 7 font data structures\n"; */

  for(presentchar=0;presentchar<96;presentchar++) {
    for(presentline=0;presentline<9;presentline++) {
      /* We build the value of the pixel up */
      tempvalue=0;
      /* Pointer to left pixel in raw font */
      tempptr=mode7fontRaw[(presentchar/16)*9+presentline]+8*(presentchar % 16);

      /* Now each pixel in that character line */
      for(presentpixel=0;presentpixel<8;presentpixel++) {
        if (tempptr[presentpixel]!=' ') tempvalue|=1<<(7-presentpixel);
      }; /* presentpixel */

      /* Store the data into the non graphic as well as the graphic areas - then we will overwrite the graphics one */
      Mode7Font[2][presentchar][presentline]=Mode7Font[1][presentchar][presentline]=Mode7Font[0][presentchar][presentline]=tempvalue;
    }; /* presentline */
  }; /* presentchar */

  /* fill in all the graphics characters */
  for(presgraph=0;presgraph<64;presgraph++) {
    presentchar=(presgraph & 31) + ((presgraph & 32)?64:0);
    Mode7Font[1][presentchar][0]=Mode7Font[1][presentchar][1]=Mode7Font[1][presentchar][2]=((presgraph &1)?0xf0:0) | ((presgraph &2)?0xf:0);
    Mode7Font[1][presentchar][3]=Mode7Font[1][presentchar][4]=Mode7Font[1][presentchar][5]=((presgraph &4)?0xf0:0) | ((presgraph &8)?0xf:0);
    Mode7Font[1][presentchar][6]=Mode7Font[1][presentchar][7]=Mode7Font[1][presentchar][8]=((presgraph &16)?0xf0:0) | ((presgraph &32)?0xf:0);
    /* The following lines do separated graphics */
    Mode7Font[2][presentchar][2]=Mode7Font[2][presentchar][5]=Mode7Font[2][presentchar][8]=0;
    Mode7Font[2][presentchar][0]=Mode7Font[2][presentchar][1]=((presgraph &1)?0xe0:0) | ((presgraph &2)?0xe:0);
    Mode7Font[2][presentchar][3]=Mode7Font[2][presentchar][4]=((presgraph &4)?0xe0:0) | ((presgraph &8)?0xe:0);
    Mode7Font[2][presentchar][6]=Mode7Font[2][presentchar][7]=((presgraph &16)?0xe0:0) | ((presgraph &32)?0xe:0);
  } /*  presgraph */;
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
  int tmp;
  static int InterlaceFrame=0;
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

#ifdef WIN32
  /* FrameNum is determined by the window handler */
  if (mainWin)
    FrameNum = mainWin->StartOfFrame();
#else
  /* If FrameNum hits 0 we actually refresh */
  if (FrameNum--==0) {
    FrameNum=Video_RefreshFrequency-1;
  };
#endif

  if (CRTC_VerticalTotalAdjust==0) {
    VideoState.CharLine=0;
    VideoState.InCharLine=CRTC_ScanLinesPerChar;
    VideoState.InCharLineUp=0;
  } else {
    VideoState.CharLine=-1;
    VideoState.InCharLine=CRTC_VerticalTotalAdjust;
    VideoState.InCharLineUp=0;
  };
  VideoState.IsTeletext=(VideoULA_ControlReg &2)>0;
  if (!VideoState.IsTeletext) {
    VideoState.Addr=CRTC_ScreenStartLow+(CRTC_ScreenStartHigh<<8);
  } else {
    int tmphigh=CRTC_ScreenStartHigh;
    /* undo wrangling of start address - I don't understand why this should be - see p.372 of AUG for this info */
    tmphigh^=0x20;
    tmphigh+=0x74;
    tmphigh&=255;
    VideoState.Addr=CRTC_ScreenStartLow+(tmphigh<<8);

    Mode7FlashTrigger--;
    if (Mode7FlashTrigger<0) {
      Mode7FlashTrigger=MODE7FLASHFREQUENCY;
      Mode7FlashOn^=1; /* toggle flash state */
    };
    for(tmp=0;tmp<80;tmp++) {
      Mode7DoubleHeightFlags[tmp]=1; /* corresponding character on NEXT line is top half */
    };
  };

  InterlaceFrame^=1;
  if (InterlaceFrame) {
    IncTrigger((2*(CRTC_HorizontalTotal+1)*((VideoULA_ControlReg & 16)?1:2)),VideoTriggerCount); /* Number of 2MHz cycles until another scanline needs doing */
  } else {
    IncTrigger(((CRTC_HorizontalTotal+1)*((VideoULA_ControlReg & 16)?1:2)),VideoTriggerCount); /* Number of 2MHz cycles until another scanline needs doing */
  };
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
  int XStep=640/(CRTC_HorizontalDisplayed*8);
  unsigned char byte,tmp;

  unsigned int Foreground=mainWin->cols[7];
  unsigned int ActualForeground;
  unsigned int Background=mainWin->cols[0];
  int Flash=0; /* i.e. steady */
  int DoubleHeight=0; /* Normal */
  int Graphics=0; /* I.e. alpha */
  int Separated=0; /* i.e. continuous graphics */
  int HoldGraph=0; /* I.e. don't hold graphics - I don't know what hold graphics is anyway! */

  unsigned int CurrentCol[9]={0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff,0xffffff};
  int CurrentLen[9]={0,0,0,0,0,0,0,0,0};
  int CurrentStartX[9]={0,0,0,0,0,0,0,0,0};
  int CurrentScanLine;
  int CurrentX=0;
  int CurrentPixel;
  int col;
  int FontTypeIndex=0; /* 0=alpha, 1=continuous graphics, 2=separated graphics */

  if (CRTC_HorizontalDisplayed>80) return; /* Not possible on beeb - and would break the double height lookup array */

  for(CurrentChar=0;CurrentChar<CRTC_HorizontalDisplayed;CurrentChar++) {
    byte=CurrentPtr[CurrentChar];
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
      /* Fudge so that the special character is just displayed in background */
      byte=32;
      FontTypeIndex=Graphics?(Separated?2:1):0;
    }; /* test for special character */

    /* Top bit never reaches character generator */
    byte&=127;
    /* Our font table goes from character 32 up */
    if (byte<32) byte=0; else byte-=32;

    /* Conceal flashed text if necessary */
    ActualForeground=(Flash && !Mode7FlashOn)?Background:Foreground;
    if (!DoubleHeight) {
      for(CurrentScanLine=0;CurrentScanLine<9;CurrentScanLine++) {
        tmp=Mode7Font[FontTypeIndex][byte][CurrentScanLine];
        if ((tmp==0) || (tmp==255)) {
          col=(tmp==0)?Background:ActualForeground;
          if (col==CurrentCol[CurrentScanLine]) CurrentLen[CurrentScanLine]+=8*XStep; else {
            if (CurrentLen[CurrentScanLine])
              mainWin->doHorizLine(CurrentCol[CurrentScanLine],VideoState.PixmapLine+CurrentScanLine,CurrentStartX[CurrentScanLine],CurrentLen[CurrentScanLine]);
            CurrentCol[CurrentScanLine]=col;
            CurrentStartX[CurrentScanLine]=CurrentX;
            CurrentLen[CurrentScanLine]=8*XStep;
          }; /* same colour */
        } else {
          for(CurrentPixel=0x80;CurrentPixel;CurrentPixel=CurrentPixel>>1) {
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
          CurrentX-=8*XStep;
        }; /* tmp!=0 */
      }; /* Scanline for */
      CurrentX+=8*XStep;
      Mode7DoubleHeightFlags[CurrentChar]=1; /* Not double height - so if the next line is double height it will be top half */
    } else {
      int ActualScanLine;
      /* Double height! */
      for(CurrentPixel=0x80;CurrentPixel;CurrentPixel=CurrentPixel>>1) {
        for(CurrentScanLine=0;CurrentScanLine<9;CurrentScanLine++) {
          if (Mode7DoubleHeightFlags[CurrentChar]) ActualScanLine=CurrentScanLine >> 1; else ActualScanLine=4+((CurrentScanLine+1)>>1);
          /* Background or foreground ? */
          col=(Mode7Font[FontTypeIndex][byte][ActualScanLine] & CurrentPixel)?ActualForeground:Background;

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
  for(CurrentScanLine=0;CurrentScanLine<9;CurrentScanLine++) {
    if (CurrentLen[CurrentScanLine])
      mainWin->doHorizLine(CurrentCol[CurrentScanLine],VideoState.PixmapLine+CurrentScanLine,CurrentStartX[CurrentScanLine],CurrentLen[CurrentScanLine]);
  };
}; /* DoMode7Row */
/*-------------------------------------------------------------------------------------------------------------*/
/* Actually does the work of decoding beeb memory and plotting the line to X */
static void LowLevelDoScanLine() {
  /* Update acceleration tables */
  DoFastTable();
  if (FastTable_Valid) LineRoutine();
}; /* LowLevelDoScanLine */

/*-------------------------------------------------------------------------------------------------------------*/
void VideoDoScanLine(void) {
  /* cerr << "CharLine=" << VideoState.CharLine << " InCharLine=" << VideoState.InCharLine << "\n"; */
  if (VideoState.IsTeletext) {
    static int DoCA1Int=0;

    if (DoCA1Int) {
      SysVIATriggerCA1Int(0);
      DoCA1Int=0;
    };

    if ((VideoState.CharLine!=-1) && (VideoState.CharLine<CRTC_VerticalDisplayed)) {
      VideoState.DataPtr=BeebMemPtrWithWrapMo7(VideoState.Addr,CRTC_HorizontalDisplayed);
      VideoState.Addr+=CRTC_HorizontalDisplayed;
      if (!FrameNum) DoMode7Row();
      VideoState.PixmapLine+=9;
    };


      /* Move onto next physical scanline as far as timing is concerned */
    if (VideoState.CharLine==-1) {
      VideoState.InCharLine-=1;
      VideoState.InCharLineUp+=1;
    } else 
      VideoState.InCharLine=-1;

      if (VideoState.InCharLine<0) {
        VideoState.CharLine++;
        VideoState.InCharLine=CRTC_ScanLinesPerChar;
        VideoState.InCharLineUp=0;
      };

    if (VideoState.CharLine>CRTC_VerticalTotal) {
      if (!FrameNum) {
        VideoAddCursor();
        mainWin->updateLines(0,256);

        /* Fill unscanned lines under picture.  Cursor will displayed on one of these
           lines when its on the last line of the screen so they are cleared after they
           are displayed, ready for the next screen. */
        if (VideoState.PixmapLine<256) {
          memset(mainWin->imageData()+VideoState.PixmapLine*640,
                 mainWin->cols[0], (256-VideoState.PixmapLine)*640);
        }
      }
      VideoStartOfFrame();
      VideoState.PreviousFinalPixmapLine=VideoState.PixmapLine;
      VideoState.PixmapLine=0;
      SysVIATriggerCA1Int(1);
      DoCA1Int=1;
    } else {
      if (VideoState.CharLine!=-1)  {
        IncTrigger((CRTC_HorizontalTotal+1)*((VideoULA_ControlReg & 16)?1:2)*((VideoState.CharLine==1)?9:10),VideoTriggerCount);
      } else {
        IncTrigger((CRTC_HorizontalTotal+1)*((VideoULA_ControlReg & 16)?1:2),VideoTriggerCount);
      };
    };
  } else {
    /* Non teletext */
    if ((VideoState.CharLine!=-1) && (VideoState.CharLine<CRTC_VerticalDisplayed)) {
      /* If first row of character then get the data pointer from memory */
      if (VideoState.InCharLine==CRTC_ScanLinesPerChar) {
        VideoState.DataPtr=BeebMemPtrWithWrap(VideoState.Addr*8,CRTC_HorizontalDisplayed*8);
        VideoState.Addr+=CRTC_HorizontalDisplayed;
      };

      if ((VideoState.InCharLine)>(CRTC_ScanLinesPerChar-8)) {
        if (!FrameNum) LowLevelDoScanLine();
        VideoState.PixmapLine+=1;
      } else {
        if (!FrameNum) mainWin->doHorizLine(mainWin->cols[0],VideoState.PixmapLine++,0,640);
      };
    }; /* !=-1 and is in displayed bit */


    /* Move onto next physical scanline as far as timing is concerned */
    VideoState.InCharLine-=1;
    VideoState.InCharLineUp+=1;
    if (VideoState.VSyncState) {
      if (!(--VideoState.VSyncState)) {
        SysVIATriggerCA1Int(0);
      };
    };

    if (VideoState.InCharLine<0) {
      VideoState.CharLine++;
      /* Suspect the -1 in sync pos is a fudge factor - careful!  - DAG - now out */
      if ((VideoState.VSyncState==0) && (VideoState.CharLine==(CRTC_VerticalSyncPos))) {
        /* Fill unscanned lines under picture */
        if (VideoState.PixmapLine<VideoState.PreviousFinalPixmapLine) {
          int CurrentLine;
          for(CurrentLine=VideoState.PixmapLine;(CurrentLine<256) && (CurrentLine<VideoState.PreviousFinalPixmapLine);
              CurrentLine++) {
            mainWin->doHorizLine(mainWin->cols[0],CurrentLine,0,640);
          };
        };
        VideoState.PreviousFinalPixmapLine=VideoState.PixmapLine;
        VideoState.PixmapLine=0;
        SysVIATriggerCA1Int(1);
        VideoState.VSyncState=2;
      };

      VideoState.InCharLine=CRTC_ScanLinesPerChar;
      VideoState.InCharLineUp=0;
    };

    if (VideoState.CharLine>CRTC_VerticalTotal) {
      if (!FrameNum) {
        VideoAddCursor();
        mainWin->updateLines(0,256);
      }
      VideoStartOfFrame();
    } else
      IncTrigger((CRTC_HorizontalTotal+1)*((VideoULA_ControlReg & 16)?1:2),VideoTriggerCount);
  }; /* Teletext if */
}; /* VideoDoScanLine */

/*-------------------------------------------------------------------------------------------------------------*/
void VideoInit(void) {
  char *environptr;
  VideoStartOfFrame();
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
  VideoState.PreviousFinalPixmapLine=255;
}; /* VideoInit */

/*-------------------------------------------------------------------------------------------------------------*/
void CRTCWrite(int Address, int Value) {
  Value&=0xff;
  if (Address & 1) {
    switch (CRTCControlReg) {
      case 0:
        CRTC_HorizontalTotal=Value;
        break;

      case 1:
        CRTC_HorizontalDisplayed=Value;
        FastTable_Valid=0;
        break;

      case 2:
        CRTC_HorizontalSyncPos=Value;
        break;

      case 3:
        CRTC_SyncWidth=Value;
        break;

      case 4:
        CRTC_VerticalTotal=Value;
        break;

      case 5:
        CRTC_VerticalTotalAdjust=Value;
        break;

      case 6:
        CRTC_VerticalDisplayed=Value;
        break;

      case 7:
        CRTC_VerticalSyncPos=Value;
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
        break;

      case 13:
        CRTC_ScreenStartLow=Value;
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
  if (Address & 1) {
    VideoULA_Palette[(Value & 0xf0)>>4]=(Value & 0xf) ^ 7;
    FastTable_Valid=0;
    /* cerr << "Palette reg " << ((Value & 0xf0)>>4) << " now has value " << ((Value & 0xf) ^ 7) << "\n"; */
  } else {
    VideoULA_ControlReg=Value;
    FastTable_Valid=0; /* Could be more selective and only do it if no.of.cols bit changes */
    /* cerr << "VidULA Ctrl reg write " << hex << Value << "\n"; */
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
	int CurX,CurY;
	int CurSize;
	int CurStart, CurEnd;

	/* Check if cursor has been hidden */
	if ((VideoULA_ControlReg & 0xe0) == 0 || (CRTC_CursorStart & 0x40) == 0)
		return;

	/* Use clock bit and cursor buts to work out size */
	if (VideoULA_ControlReg & 0x80)
		CurSize = CurSizes[(VideoULA_ControlReg & 0x70)>>4] * 8;
	else
		CurSize = 2 * 8; /* Mode 7 */

	if (VideoState.IsTeletext)
	{
		ScrAddr=CRTC_ScreenStartLow+(((CRTC_ScreenStartHigh ^ 0x20) + 0x74 & 0xff)<<8);
		CurAddr=CRTC_CursorPosLow+(((CRTC_CursorPosHigh ^ 0x20) + 0x74 & 0xff)<<8);

		CurStart = (CRTC_CursorStart & 0x1f) / 2;
		CurEnd = CRTC_CursorEnd / 2;
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
	CurY = RelAddr / CRTC_HorizontalDisplayed;

	/* Convert to pixel positions */
	CurX = CurX*640/CRTC_HorizontalDisplayed;
	CurY = CurY * (VideoState.IsTeletext ? CRTC_ScanLinesPerChar/2 : CRTC_ScanLinesPerChar + 1);

	/* Limit cursor size */
	if (CurEnd > 9)
		CurEnd = 9;

	if (CurX + CurSize >= 640)
		CurSize = 640 - CurX;

	if (CurSize > 0)
	{
		for (int y = CurStart; y <= CurEnd && CurY + y < 256; ++y)
		{
			if (CurY + y >= 0)
				mainWin->doHorizLine(7, CurY + y, CurX, CurSize);
		}
	}
}

/*-------------------------------------------------------------------------*/
void SaveVideoState(unsigned char *StateData) {
	/* 6845 state */
	StateData[0] = CRTCControlReg;
	StateData[1] = CRTC_HorizontalTotal;
	StateData[2] = CRTC_HorizontalDisplayed;
	StateData[3] = CRTC_HorizontalSyncPos;
	StateData[4] = CRTC_SyncWidth;
	StateData[5] = CRTC_VerticalTotal;
	StateData[6] = CRTC_VerticalTotalAdjust;
	StateData[7] = CRTC_VerticalDisplayed;
	StateData[8] = CRTC_VerticalSyncPos;
	StateData[9] = CRTC_InterlaceAndDelay;
	StateData[10] = CRTC_ScanLinesPerChar;
	StateData[11] = CRTC_CursorStart;
	StateData[12] = CRTC_CursorEnd;
	StateData[13] = CRTC_ScreenStartHigh;
	StateData[14] = CRTC_ScreenStartLow;
	StateData[15] = CRTC_CursorPosHigh;
	StateData[16] = CRTC_CursorPosLow;
	StateData[17] = CRTC_LightPenHigh;
	StateData[18] = CRTC_LightPenLow;

	/* Video ULA state */
	StateData[32] = VideoULA_ControlReg;
	for (int col = 0; col < 16; ++col)
		StateData[33+col] = VideoULA_Palette[col] ^ 7; /* Use real ULA values */
}

/*-------------------------------------------------------------------------*/
void RestoreVideoState(unsigned char *StateData) {
	/* 6845 state */
	CRTCControlReg = StateData[0];
	CRTC_HorizontalTotal = StateData[1];
	CRTC_HorizontalDisplayed = StateData[2];
	CRTC_HorizontalSyncPos = StateData[3];
	CRTC_SyncWidth = StateData[4];
	CRTC_VerticalTotal = StateData[5];
	CRTC_VerticalTotalAdjust = StateData[6];
	CRTC_VerticalDisplayed = StateData[7];
	CRTC_VerticalSyncPos = StateData[8];
	CRTC_InterlaceAndDelay = StateData[9];
	CRTC_ScanLinesPerChar = StateData[10];
	CRTC_CursorStart = StateData[11];
	CRTC_CursorEnd = StateData[12];
	CRTC_ScreenStartHigh = StateData[13];
	CRTC_ScreenStartLow = StateData[14];
	CRTC_CursorPosHigh = StateData[15];
	CRTC_CursorPosLow = StateData[16];
	CRTC_LightPenHigh = StateData[17];
	CRTC_LightPenLow = StateData[18];

	/* Video ULA state */
	VideoULA_ControlReg = StateData[32];
	for (int col = 0; col < 16; ++col)
		VideoULA_Palette[col] = StateData[33+col] ^ 7; /* Convert ULA values to colours */

	/* Reset the other video state variables */
	VideoInit();
}

/*-------------------------------------------------------------------------------------------------------------*/
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

