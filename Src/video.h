
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
#ifndef VIDEO_HEADER
#define VIDEO_HEADER
#include <stdio.h>

extern int VideoTriggerCount;

extern unsigned char VideoULA_ControlReg;
extern unsigned char VideoULA_Palette[16];

extern unsigned char CRTCControlReg;
extern unsigned char CRTC_HorizontalTotal;     /* R0 */
extern unsigned char CRTC_HorizontalDisplayed; /* R1 */
extern unsigned char CRTC_HorizontalSyncPos;   /* R2 */
extern unsigned char CRTC_SyncWidth;           /* R3 - top 4 bits are Vertical (in scan lines) and bottom 4 are horizontal in characters */
extern unsigned char CRTC_VerticalTotal;       /* R4 */
extern unsigned char CRTC_VerticalTotalAdjust; /* R5 */
extern unsigned char CRTC_VerticalDisplayed;   /* R6 */
extern unsigned char CRTC_VerticalSyncPos;     /* R7 */
extern unsigned char CRTC_InterlaceAndDelay;   /* R8 - 0,1 are interlace modes, 4,5 display blanking delay, 6,7 cursor blanking delay */
extern unsigned char CRTC_ScanLinesPerChar;    /* R9 */
extern unsigned char CRTC_CursorStart;         /* R10 */
extern unsigned char CRTC_CursorEnd;           /* R11 */
extern unsigned char CRTC_ScreenStartHigh;     /* R12 */
extern unsigned char CRTC_ScreenStartLow;      /* R13 */
extern unsigned char CRTC_CursorPosHigh;       /* R14 */
extern unsigned char CRTC_CursorPosLow;        /* R15 */
extern unsigned char CRTC_LightPenHigh;        /* R16 */
extern unsigned char CRTC_LightPenLow;         /* R17 */
extern unsigned int ActualScreenWidth;
extern long ScreenAdjust;

void RedoMPTR(void);
void CRTCWrite(int Address, int Value);
int CRTCRead(int Address);
void VideoULAWrite(int Address, int Value);
int VideoULARead(int Address);
void VideoInit(void);
void video_dumpstate(void);
void VideoDoScanLine(void);
void VideoGetText(char *text, int line);

extern unsigned char TeletextEnabled;
extern unsigned char ShowCursorLine;

#define VideoPoll(ncycles) if ((VideoTriggerCount)<=TotalCycles) VideoDoScanLine();

// Allow enough lines for all modes.
// i.e. max(virtical total * scan lines per char) = 38 * 8  (mode 7 excluded)
#define MAX_VIDEO_SCAN_LINES 304

void SaveVideoUEF(FILE *SUEF);
void LoadVideoUEF(FILE *SUEF);
extern char TeletextStyle;
extern int THalfMode;
#endif
