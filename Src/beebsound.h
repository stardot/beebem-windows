
/****************************************************************************/
/*              Beebem - (c) David Alan Gilbert 1994/1995                   */
/*              -----------------------------------------                   */
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
/* Sound emulation for the beeb - David Alan Gilbert 26/11/94 */

#ifndef SOUND_HEADER
#define SOUND_HEADER

#ifdef WIN32
/* Always compile sound code - it is switched on and off using SoundEnabled */
#define SOUNDSUPPORT
#endif

extern int SoundEnabled;    /* Sound on/off flag */
extern int DirectSoundEnabled;  /* DirectSound enabled for Win32 */
extern int SoundSampleRate; /* Sample rate, 11025, 22050 or 44100 Hz */
extern int SoundVolume;     /* Volume, 1(full),2,3 or 4(low) */

extern int SoundTrigger; /* Cycle based trigger on sound */

void SoundInit();
void SoundReset();

/* Called in sysvia.cc when a write to one of the 76489's registers occurs */
void Sound_RegWrite(int Value);

void SoundTrigger_Real(void);

#define Sound_Trigger(ncycles) if (SoundTrigger<=TotalCycles) SoundTrigger_Real();

#endif
