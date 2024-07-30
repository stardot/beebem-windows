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

bool BuildMode7Font(const char *filename);
void RedoMPTR(void);
void CRTCWrite(int Address, unsigned char Value);
unsigned char CRTCRead(int Address);
void VideoULAWrite(int Address, unsigned char Value);
unsigned char VideoULARead(int Address);
void VideoInit(void);
void VideoDoScanLine(void);
void VideoGetText(char *text, int line);
void VideoLightPenStrobe();

extern bool TeletextEnabled;

#define VideoPoll(ncycles) if ((VideoTriggerCount)<=TotalCycles) VideoDoScanLine();

// Allow enough lines for all modes.
// i.e. max(vertical total * scan lines per char) = 39 * 8  (mode 7 excluded)
#define MAX_VIDEO_SCAN_LINES 312

void SaveVideoUEF(FILE *SUEF);
void LoadVideoUEF(FILE *SUEF, int Version);
extern char TeletextStyle;
extern bool TeletextHalfMode;

#endif
