
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
#include <windows.h>
#endif

#define MUTED 0
#define UNMUTED 1

#include <stdio.h>

extern int SoundDefault; // Default sound state (enabled/disabled via sound menu)
extern int SoundEnabled;    /* Sound on/off flag - will be off if DirectSound init fails */
extern int DirectSoundEnabled;  /* DirectSound enabled for Win32 */
extern int RelaySoundEnabled; // Relay Click noise enable
extern int SoundSampleRate; /* Sample rate, 11025, 22050 or 44100 Hz */
extern int SoundVolume;     /* Volume, 1(full),2,3 or 4(low) */

extern __int64 SoundTrigger; /* Cycle based trigger on sound */
extern double SoundTuning;
extern __int64 SoundCycles;

void SoundInit();
void SoundReset();

/* Called in sysvia.cc when a write to one of the 76489's registers occurs */
void Sound_RegWrite(int Value);
void DumpSound(void);
void SoundTrigger_Real(void);
void ClickRelay(unsigned char RState);

void Sound_Trigger(int NCycles);

extern volatile BOOL bDoSound;
extern void AdjustSoundCycles(void);

void SetSound(char State);

struct AudioType {
	char Signal; // Signal type: data, gap, or tone.
	char BytePos; // Position in data byte
	bool Enabled; // Enable state of audio deooder
	int Data; // The actual data itself
	int Samples; // Samples counted in current pattern till changepoint
	char CurrentBit; // Current bit in data being processed
	char ByteCount; // Byte repeat counter
};

extern struct AudioType TapeAudio;
extern bool TapeSoundEnabled;
extern int SoundChipEnabled;
void SoundChipReset(void);
void SwitchOnSound(void);
extern int UseHostClock;
extern int UsePrimaryBuffer;
void LoadSoundUEF(FILE *SUEF);
void SaveSoundUEF(FILE *SUEF);
extern int PartSamples;
extern int SBSize;
void MuteSound(void);
#endif
