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
/* Win32 port - Mike Wyatt 7/6/97 */
/* Conveted Win32 port to use DirectSound - Mike Wyatt 11/1/98 */
// 14/04/01 - Proved that I AM better than DirectSound, by fixing the code thereof ;P

#include "beebsound.h"

#include <iostream.h>
#include <windows.h>
#include <process.h>
#include <math.h>

#include <errno.h>
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <dsound.h>
#include "main.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "6502core.h"
#include "port.h"
#include "beebmem.h"
#include "beebwin.h"
#include "uefstate.h"

// #define DEBUGSOUNDTOFILE

#define PREFSAMPLERATE 22050
#define MAXBUFSIZE 32768

static unsigned char wb1[MAXBUFSIZE];
static unsigned char wb2[MAXBUFSIZE];
static unsigned char RelayOnBuf[2048];
static unsigned char RelayOffBuf[2048];

static HWAVEOUT hwo;
static WAVEHDR wh1, wh2;
static WAVEHDR *active_wh, *inactive_wh;
static unsigned char *active_wb, *inactive_wb;
static unsigned char *PROnBuf=RelayOnBuf;
static unsigned char *PROffBuf=RelayOffBuf;

static LPDIRECTSOUND DSound = NULL;
static LPDIRECTSOUNDBUFFER DSB1 = NULL;
int UsePrimaryBuffer=0;

int SoundEnabled = 1;
int DirectSoundEnabled = 0;
int RelaySoundEnabled = 0;
int SoundSampleRate = PREFSAMPLERATE;
int SoundVolume = 3;
int SoundAutoTriggerTime;
int SoundBufferSize,TSoundBufferSize;
double CSC[4]={0,0,0,0},CSA[4]={0,0,0,0}; // ChangeSamps Adjusts

/* Number of places to shift the volume */
#define VOLMAG 3
#define WRITE_ADJUST ((samplerate/50)*2)

static int RelayLen[3]={0,398,297}; // Relay samples
int UseHostClock=0;

//FILE *sndlog;


volatile struct {
  unsigned int ToneFreq[4];
  unsigned int ChangeSamps[4]; /* How often this channel should flip its otuput */
  unsigned int ToneVolume[4]; /* In units of /dev/dsp */
  struct {
    unsigned int FB:1; /* =0 for periodic, =1 for white */
    unsigned int Freq:2; /* 0=low, 1=medium, 2=high, 3=tone gen 1 freq */
    unsigned int Vol:4;
  } Noise;
  int LastToneFreqSet; /* the tone generator last set - for writing the 2nd byte */
} BeebState76489;
int RealVolumes[4]; // Holds the real volume values for state save use

bool ReloadingChip;
static int ActiveChannel[4]={FALSE,FALSE,FALSE,FALSE}; /* Those channels with non-0 voolume */
// Set it to an array for more accurate sound generation
static int devfd; /* Audio device id */
static unsigned int samplerate;
static double OurTime=0.0; /* Time in sample periods */

__int64 SoundTrigger; /* Time to trigger a sound event */
__int64 SoundTrigger2,STCycles; // BBC's Clock count for triggering host clock adjust
int ASoundTrigger; // CPU cycle equivalent to trigger recalculation;

static unsigned int GenIndex[4]; /* Used by the voice generators */
static int GenState[4];
static int bufptr=0;
int SoundDefault;
double SoundTuning=0.0; // Tunning offset

void PlayUpTil(double DestTime);
FILE *seglog;
BOOL bReRead=FALSE;
volatile BOOL bDoSound=TRUE;
int WriteOffset=0; int SampleAdjust=0;
char Relay=0; int RelayPos=0;
LARGE_INTEGER PFreq,LastPCount,CurrentPCount;
__int64 SoundCycles=0; double CycleRatio;
struct AudioType TapeAudio; // Tape audio decoder stuff
bool TapeSoundEnabled;
int PartSamples=1;
int SBSize=1;

/****************************************************************************/
/* Writes sound data to a DirectSound buffer */
HRESULT WriteToSoundBuffer(PBYTE lpbSoundData)
{
	LPVOID lpvPtr1;
	DWORD dwBytes1; 
	LPVOID lpvPtr2;
	DWORD dwBytes2;
	DWORD CWC;
	HRESULT hr;
	int CDiff;
	if ((DSound==NULL) || (DSB1==NULL)) return DS_OK; // Don't write if DirectSound not up and running!
	// (As when in menu loop)
	// Correct from pointer desync
    if (bReRead) {
		DSB1->GetCurrentPosition(NULL,&CWC);
		WriteOffset=CWC+WRITE_ADJUST;
		if (WriteOffset>=TSoundBufferSize) WriteOffset-=TSoundBufferSize;
		bReRead=FALSE;
	} 
	// Obtain write pointer.
	hr = DSB1->Lock(WriteOffset, SoundBufferSize, &lpvPtr1, 
		&dwBytes1, &lpvPtr2, &dwBytes2, 0);
	if(hr == DSERR_BUFFERLOST)
	{
		hr = DSB1->Restore();
		if (hr == DS_OK)
			hr = DSB1->Lock(WriteOffset, SoundBufferSize,
				&lpvPtr1, &dwBytes1, &lpvPtr2, &dwBytes2, 0);
	}
	if(DS_OK == hr)
	{
		// Write to pointers.
		CopyMemory(lpvPtr1, lpbSoundData, dwBytes1);
		if(NULL != lpvPtr2)
		{
			CopyMemory(lpvPtr2, lpbSoundData+dwBytes1, dwBytes2);
		}
		// Release the data back to DirectSound.
		hr = DSB1->Unlock(lpvPtr1, dwBytes1, lpvPtr2, dwBytes2);
	}
	// Update pointers
	WriteOffset+=SoundBufferSize;
	if (WriteOffset>=TSoundBufferSize) WriteOffset-=TSoundBufferSize;
	// Check for pointer desync
	DSB1->GetCurrentPosition(NULL,&CWC);
	CDiff=WriteOffset-CWC;
	if (CDiff<0) CDiff=(WriteOffset+TSoundBufferSize)-CWC;
	if (abs(CDiff)>(signed)(WRITE_ADJUST*2)) bReRead=TRUE; 
	SampleAdjust--;
	if (SampleAdjust==0) {
		SampleAdjust=4;
		SoundBufferSize=(SBSize)?444:110; 
	} else SoundBufferSize=(SBSize)?440:111;
	return hr;
}

/****************************************************************************/
/* DestTime is in samples */
void PlayUpTil(double DestTime) {
  int tmptotal,channel,bufinc,tapetotal;
  char Extras;

  /*cerr << "PlayUpTil: DestTime=" << DestTime << " OurTime=" << OurTime << "\n";*/
  while (DestTime>OurTime) {
	if ((!DirectSoundEnabled) && (!ActiveChannel[0]) && (!ActiveChannel[1]) && (!ActiveChannel[2]) && (!ActiveChannel[3])) {
      OurTime=DestTime;
    } else {
      for(bufinc=0;(bufptr<SoundBufferSize) && ((OurTime+bufinc)<DestTime);bufptr++,bufinc++) {
		int tt;
        tmptotal=0;
		Extras=4;
		// Begin of for loop
        for(channel=1;channel<=3;channel++) {
		 if (ActiveChannel[channel]) {
		  if (GenState[channel])
			tmptotal+=BeebState76489.ToneVolume[channel];
		  else
			tmptotal-=BeebState76489.ToneVolume[channel];
          GenIndex[channel]++;
          tt=(int)CSC[channel];
		  if (!PartSamples) tt=0;
          if (GenIndex[channel]>=(BeebState76489.ChangeSamps[channel]+tt)) {
		    CSC[channel]+=CSA[channel];
		    CSC[channel]-=tt;
            GenIndex[channel]=0;
            GenState[channel]^=1;
          };
		 }
        }; /* Channel loop */

        /* Now put in noise generator stuff */
	   if (ActiveChannel[0]) { 
        if (BeebState76489.Noise.FB) {
          /* White noise */
		  if (GenState[0])
			tmptotal+=BeebState76489.ToneVolume[0];
		  else
			tmptotal-=BeebState76489.ToneVolume[0];
          GenIndex[0]++;
          switch (BeebState76489.Noise.Freq) {
            case 0: /* Low */
              if (GenIndex[0]>=(samplerate/10000)) {
                GenIndex[0]=0;
                GenState[0]=rand() & 1;
              };
              break;

            case 1: /* Med */
              if (GenIndex[0]>=(samplerate/5000)) {
                GenIndex[0]=0;
                GenState[0]=rand() & 1;
              };
              break;

            case 2: /* High */
              if (GenIndex[0]>=(samplerate/2500)) {
                GenIndex[0]=0;
                GenState[0]=rand() & 1;
              };
              break;

            case 3: /* as channel 1 */
              if (GenIndex[0]>=BeebState76489.ChangeSamps[1]) {
                GenIndex[0]=0;
                GenState[0]=rand() & 1;
              };
              break;
          }; /* Freq type switch */
        } else {
          /* Periodic */
		  if (GenState[0])
			tmptotal+=BeebState76489.ToneVolume[0];
		  else
			tmptotal-=BeebState76489.ToneVolume[0];
          GenIndex[0]++;
          switch (BeebState76489.Noise.Freq) {
            case 0: /* Low */
              if (GenState[0]) {
                if (GenIndex[0]>=(samplerate/125)) {
                  GenIndex[0]=0;
                  GenState[0]=0;
                };
              } else {
                if (GenIndex[0]>=(samplerate/1250)) {
                  GenIndex[0]=0;
                  GenState[0]=1;
                };
              };
              break;

            case 1: /* Med */
              if (GenState[0]) {
                if (GenIndex[0]>=(samplerate/250)) {
                  GenIndex[0]=0;
                  GenState[0]=0;
                };
              } else {
                if (GenIndex[0]>=(samplerate/2500)) {
                  GenIndex[0]=0;
                  GenState[0]=1;
                };
              };
              break;

            case 2: /* High */
              if (GenState[0]) {
                if (GenIndex[0]>=(samplerate/500)) {
                  GenIndex[0]=0;
                  GenState[0]=0;
                };
              } else {
                if (GenIndex[0]>=(samplerate/5000)) {
                  GenIndex[0]=0;
                  GenState[0]=1;
                };
              };
              break;

            case 3: /* Tone gen 1 */
              if (GenState[0]) {
                if (GenIndex[0]>=(BeebState76489.ChangeSamps[1]*16)) {
                  GenIndex[0]=0;
                  GenState[0]=0;
                };
              } else {
                if (GenIndex[0]>=(BeebState76489.ChangeSamps[1])) {
                  GenIndex[0]=0;
                  GenState[0]=1;
                };
              };
              break;

          }; /* Freq type switch */
		}
        };

		// Mix in relay sound here
		if (Relay==1) tmptotal+=(RelayOffBuf[RelayPos++]-127)*10;
		if (Relay==2) tmptotal+=(RelayOnBuf[RelayPos++]-127)*10;
		if (TapeSoundEnabled) {
			// Mix in tape sound here
			tapetotal=0; 
			if ((TapeAudio.Enabled) && (TapeAudio.Signal==2)) {
				if (TapeAudio.Samples++>=36) TapeAudio.Samples=0;
				tapetotal=(int)(sin(((TapeAudio.Samples*20)*3.14)/180)*80);
				Extras++;
			}
			if ((TapeAudio.Enabled) && (TapeAudio.Signal==1)) {
				tapetotal=(int)(sin(((TapeAudio.Samples*(10*(1+TapeAudio.CurrentBit)))*3.14)/180)*(80+(40*(1-TapeAudio.CurrentBit))));
				// And if you can follow that equation, "ill give you the money meself" - Richard Gellman
				if (TapeAudio.Samples++>=36) {
					TapeAudio.Samples=0;
					TapeAudio.BytePos++;
					if (TapeAudio.BytePos<=10) TapeAudio.CurrentBit=(TapeAudio.Data & (1<<(10-TapeAudio.BytePos)))?1:0;
				}
				if (TapeAudio.BytePos>10) {
					TapeAudio.ByteCount--;
					if (!TapeAudio.ByteCount) TapeAudio.Signal=2; else { TapeAudio.BytePos=1; TapeAudio.CurrentBit=0; }
				}
				Extras++;
			}
			tmptotal+=tapetotal;
		}

		/* Make it a bit louder under Windows */
		if (Relay) Extras++;
		if (TapeAudio.Enabled) Extras++;
		if (Extras) tmptotal/=4; else tmptotal=0;
		inactive_wb[bufptr] = (tmptotal/SoundVolume)+128;
		//fputc(inactive_wb[bufptr],sndlog);
		// end of for loop
		if (RelayPos>=RelayLen[Relay]) Relay=0;
      }; /* buffer loop */

	  /* Only write data when buffer is full */
	  if (bufptr == SoundBufferSize)
	  {
#ifdef DEBUGSOUNDTOFILE
		FILE *fd = fopen("/audio.dbg", "a+b");
		if (fd != NULL)
		{
			fwrite(inactive_wb, 1, SoundBufferSize, fd);
			fclose(fd);
		}
		else
		{
			MessageBox(GETHWND,"Failed to open audio.dbg","BBC Emulator",MB_OK|MB_ICONERROR);
			exit(1);
		}
#else
		if (!DirectSoundEnabled)
		{
			inactive_wh->lpData = (char *)inactive_wb;
			inactive_wh->dwBufferLength = SoundBufferSize;
			inactive_wh->dwBytesRecorded = 0;
			inactive_wh->dwUser = 0;
			inactive_wh->dwFlags = 0;
			inactive_wh->dwLoops = 0;
			inactive_wh->lpNext = 0;
			inactive_wh->reserved = 0;

			MMRESULT mmresult = waveOutPrepareHeader(hwo, inactive_wh, sizeof(WAVEHDR));
			if (mmresult == MMSYSERR_NOERROR)
			{
				mmresult = waveOutWrite(hwo, inactive_wh, sizeof(WAVEHDR));
				if (mmresult != MMSYSERR_NOERROR)
				{
			  		MessageBox(GETHWND,"Sound write failed","BBC Emulator",MB_OK|MB_ICONERROR);
					SoundReset();
				}

				if (active_wh != NULL)
				{
					while (!(active_wh->dwFlags & WHDR_DONE))
						Sleep(0);

					mmresult = waveOutUnprepareHeader(hwo, active_wh, sizeof(WAVEHDR));
					if (mmresult != MMSYSERR_NOERROR)
					{
				  		MessageBox(GETHWND,"Sound unprepare failed","BBC Emulator",MB_OK|MB_ICONERROR);
						SoundReset();
					}
				}
			}
			else
			{
		  		MessageBox(GETHWND,"Sound prepare failed","BBC Emulator",MB_OK|MB_ICONERROR);
				SoundReset();
			}
		}
		else
		{
			HRESULT hr;
			hr = WriteToSoundBuffer(inactive_wb);
		} 
#endif

		/* Swap active and inactive buffers */
		active_wh = inactive_wh;
		active_wb = inactive_wb;
		if (active_wb == wb1)
		{
			inactive_wh = &wh2;
			inactive_wb = wb2;
		}
		else
		{
			inactive_wh = &wh1;
			inactive_wb = wb1;
		}
		bufptr=0;
	  }

	  OurTime+=bufinc;
   }; /* If no volume */
  }; /* While time */
}; /* PlayUpTil */

/****************************************************************************/
/* Convert time in cycles to time in samples                                */
static __int64 LastBeebCycle=0; /* Last parameter to this function */
static double LastOurTime=0; /* Last result of this function */

static double CyclesToSamples(__int64 beebtime) {
  double tmp;

  /* OK - beeb cycles are in 2MHz units, ours are in 1/samplerate */
  /* This is all done incrementally - find the number of ticks since the last call
     in both domains.  This does mean this should only be called once */
  /* Extract number of cycles since last call */
  if (beebtime<LastBeebCycle) {
    /* Wrap around in beebs time */
    tmp=((double)CycleCountWrap-(double)LastBeebCycle)+(double)beebtime;
  } else {
    tmp=(double)beebtime-(double)LastBeebCycle;
  };

/*fprintf(stderr,"Convert tmp=%f\n",tmp); */
  LastBeebCycle=beebtime;

  tmp*=(samplerate);
  tmp/=2000000.0; /* Few - glad thats a double! */

  LastOurTime+=tmp;
  return LastOurTime;
}; /* CyclesToSamples */

/****************************************************************************/
static void PlayTilNow(void) {
  double nowsamps=CyclesToSamples(SoundCycles);
  PlayUpTil(nowsamps);
}; /* PlayTilNow */

/****************************************************************************/
/* Creates a DirectSound buffer */
HRESULT CreateSecondarySoundBuffer(void)
{
	WAVEFORMATEX wf;
	DSBUFFERDESC dsbdesc;
	HRESULT hr;

	// Set up wave format structure.
	memset(&wf, 0, sizeof(WAVEFORMATEX));
	wf.wFormatTag = WAVE_FORMAT_PCM;
	wf.nChannels = 1;
	wf.nSamplesPerSec = samplerate;
	wf.wBitsPerSample = 8;
	wf.nBlockAlign = 1;
	wf.nAvgBytesPerSec = samplerate;
	wf.cbSize = 0;

	// Set up DSBUFFERDESC structure.
	memset(&dsbdesc, 0, sizeof(DSBUFFERDESC)); // Zero it out.
	dsbdesc.dwSize = sizeof(DSBUFFERDESC);

	// Need default controls (pan, volume, frequency).
	dsbdesc.dwFlags = DSBCAPS_GETCURRENTPOSITION2; // | DSBCAPS_STICKYFOCUS;
	dsbdesc.dwBufferBytes = 32768;
	TSoundBufferSize=32768;
	dsbdesc.lpwfxFormat = (LPWAVEFORMATEX)&wf;

	// Create buffer.
	hr = DSound->CreateSoundBuffer(&dsbdesc, &DSB1, NULL);

	return hr;
}

HRESULT CreatePrimarySoundBuffer(void)
{
	WAVEFORMATEX wf;
	DSBUFFERDESC dsbdesc;
	HRESULT hr;
	DSBCAPS dsbcaps;

	// Set up wave format structure.
	memset(&wf, 0, sizeof(WAVEFORMATEX));
	wf.wFormatTag = WAVE_FORMAT_PCM;
	wf.nChannels = 1;
	wf.nSamplesPerSec = samplerate;
	wf.wBitsPerSample = 8;
	wf.nBlockAlign = 1;
	wf.nAvgBytesPerSec = samplerate;
	wf.cbSize = 0;

	// Set up DSBUFFERDESC structure.
	memset(&dsbdesc, 0, sizeof(DSBUFFERDESC)); // Zero it out.
	dsbdesc.dwSize = sizeof(DSBUFFERDESC);
	dsbdesc.dwFlags = DSBCAPS_PRIMARYBUFFER | DSBCAPS_GETCURRENTPOSITION2 | DSBCAPS_STICKYFOCUS;
	dsbdesc.dwBufferBytes = 0;
	dsbdesc.lpwfxFormat = NULL;

	// Create buffer.
	hr = DSound->CreateSoundBuffer(&dsbdesc, &DSB1, NULL);
	DSB1->SetFormat(&wf);
	dsbcaps.dwSize=sizeof(DSBCAPS);
	DSB1->GetCaps(&dsbcaps);
	TSoundBufferSize=dsbcaps.dwBufferBytes;
	return hr;
}

/****************************************************************************/
static void InitAudioDev(int sampleratein) {
  samplerate=sampleratein;
  DirectSoundEnabled=1;
		HRESULT hr;
		int dsect=0;
		dsect=1;
 		hr = DirectSoundCreate(NULL, &DSound, NULL);
		if (hr != DS_OK) MessageBox(GETHWND,"Attempt to start DirectSound system failed","BeebEm",MB_ICONERROR|MB_OK);
		if(hr == DS_OK)
		{
			hr=DS_OK;
			if (UsePrimaryBuffer) {
				hr = DSound->SetCooperativeLevel(mainWin->GethWnd(), DSSCL_WRITEPRIMARY);
				if (hr == DSERR_UNSUPPORTED) {
					MessageBox(GETHWND,"Use of Primary DirectSound Buffer unsupported on this system. Using Secondary DirectSound Buffer instead",
						"BBC Emulator",MB_OK|MB_ICONERROR);
					UsePrimaryBuffer=0;
				}
			}
			if (!UsePrimaryBuffer) hr=DSound->SetCooperativeLevel(mainWin->GethWnd(),DSSCL_NORMAL);
		}
		if(hr == DS_OK)
		{
		dsect=2;
			if (UsePrimaryBuffer) hr = CreatePrimarySoundBuffer();
			else hr=CreateSecondarySoundBuffer();
		} else MessageBox(GETHWND,"Attempt to create DirectSound buffer failed","BeebEm",MB_ICONERROR|MB_OK);
		if (hr != DS_OK)
		{
			char  errstr[200];
			sprintf(errstr,"Direct Sound initialisation failed on part %i\nFailure code %X",dsect,hr);
			MessageBox(GETHWND,errstr,"BBC Emulator",MB_OK|MB_ICONERROR);
			SoundReset();
		}
		mainWin->SetPBuff();
		
		active_wb = NULL;
		inactive_wb = wb1;
		if (hr==DS_OK) DSB1->Play(0,0,DSBPLAY_LOOPING);
		if (hr==DS_OK) SoundEnabled=1;
}; /* InitAudioDev */

void LoadRelaySounds(void) {
	/* Loads in the relay sound effects into buffers */
	FILE *RelayFile;
	char RelayFileName[256];
	char *PRFN=RelayFileName;
	strcpy(PRFN,RomPath);
	strcat(PRFN,"RelayOn.SND");
	RelayFile=fopen(PRFN,"rb");
	if (RelayFile!=NULL) {
		fread(RelayOnBuf,1,2048,RelayFile);
		fclose(RelayFile);
	}
	else {
		char  errstr[200];
		sprintf(errstr,"Could not open Relay ON Sound");
		MessageBox(GETHWND,errstr,"BBC Emulator",MB_OK|MB_ICONERROR);
	}
	strcpy(PRFN,RomPath);
	strcat(PRFN,"RelayOff.SND");
	RelayFile=fopen(PRFN,"rb");
	if (RelayFile!=NULL) {
		fread(RelayOffBuf,1,2048,RelayFile);
		fclose(RelayFile);
	}
	else {
		char  errstr[200];
		sprintf(errstr,"Could not open Relay OFF Sound");
		MessageBox(GETHWND,errstr,"BBC Emulator",MB_OK|MB_ICONERROR);
	}
}

/****************************************************************************/
/* The 'freqval' variable is the value as sene by the 76489                 */
static void SetFreq(int Channel, int freqval) {
  unsigned int freq;
  int ChangeSamps; /* Number of samples after which to change */
  double t;

  if (freqval==0) {
    ChangeSamps=INT_MAX;
  } else {
    freq=4000000/(32*freqval);
    if (freq>samplerate) {
      ChangeSamps=INT_MAX; // Way to high frequency - shut off 
    } else {
      if (freq>(samplerate/2)) {
        // Hmm - a bit high - make it top out - change on every sample 
        // What we should be doing is moving to sine wave at 1/6 samplerate 
        ChangeSamps=2;
      } else {
        ChangeSamps=(int)( (( (double)samplerate/(double)freq)/2.0) +SoundTuning);
		t=( (( (double)samplerate/(double)freq)/2.0) +SoundTuning);
		CSA[Channel]=(double)(t-ChangeSamps);
		CSC[Channel]=0;
      };
    }; // freq<=samplerate 
  }; /* Freqval!=0 */
  BeebState76489.ChangeSamps[Channel]=ChangeSamps;
};

/****************************************************************************/
void AdjustSoundCycles(void) {
	if (!UseHostClock) return;
	if ((!DirectSoundEnabled) || (!SoundEnabled)) return; // it just lags the program if there's no sound
	LARGE_INTEGER TickDiff;
	__int64 CycleDiff;
	// Get performance counter
	QueryPerformanceCounter(&CurrentPCount);
	TickDiff.QuadPart=CurrentPCount.QuadPart-LastPCount.QuadPart;
	CycleDiff=(__int64)(TickDiff.QuadPart*CycleRatio);
	if (CycleDiff>(SoundAutoTriggerTime/2)) {
		LastPCount.QuadPart=CurrentPCount.QuadPart;
		SoundCycles+=CycleDiff;
	}
}

void SoundTrigger_Real(void) {
	AdjustSoundCycles();
    PlayTilNow();
	SoundTrigger=SoundCycles+SoundAutoTriggerTime;
}; /* SoundTrigger_Real */

void Sound_Trigger(int NCycles) {
	STCycles+=NCycles;
	if (SoundTrigger2<=STCycles) {
		AdjustSoundCycles();
		SoundTrigger2+=3000;
	}
	if (!UseHostClock) SoundCycles+=NCycles;
		if (SoundTrigger<=SoundCycles) SoundTrigger_Real();
}

void SoundChipReset(void) {
  BeebState76489.LastToneFreqSet=0;
  BeebState76489.ToneVolume[0]=0;
  BeebState76489.ToneVolume[1]=BeebState76489.ToneVolume[2]=BeebState76489.ToneVolume[3]=15<<VOLMAG;
  BeebState76489.ToneFreq[0]=BeebState76489.ToneFreq[1]=BeebState76489.ToneFreq[2]=1000;
  BeebState76489.ToneFreq[3]=1000;
  BeebState76489.Noise.FB=0;
  BeebState76489.Noise.Freq=0;
  ActiveChannel[0]=FALSE;
  ActiveChannel[1]=ActiveChannel[2]=ActiveChannel[3]=FALSE;
}

/****************************************************************************/
/* Called to enable sound output                                            */
void SoundInit() {
  int pfr;
  ClearTrigger(SoundTrigger);
  LastBeebCycle=SoundCycles;
  LastOurTime=(double)LastBeebCycle * (double)SoundSampleRate / 2000000.0;
  OurTime=LastOurTime;
  bufptr=0;
  InitAudioDev(SoundSampleRate);
  if (SoundSampleRate == 44100) SoundAutoTriggerTime = 5000; 
  if (SoundSampleRate == 22050) SoundAutoTriggerTime = 10000; 
  if (SoundSampleRate == 11025) SoundAutoTriggerTime = 20000; 
  SampleAdjust=4;
  SoundBufferSize=111;
  LoadRelaySounds();
  bReRead=TRUE;
  if (UseHostClock) {
	pfr=QueryPerformanceFrequency(&PFreq); // Get timer resolution
	QueryPerformanceCounter(&LastPCount);
	CurrentPCount.QuadPart=LastPCount.QuadPart;
	CycleRatio=2000000.0/PFreq.QuadPart;
  }
  if (!DirectSoundEnabled) {
	  // Non DX sound stuff here
	  if (SoundSampleRate == 44100)
		  SoundBufferSize = MAXBUFSIZE;
	  else if (SoundSampleRate == 22050)
		  SoundBufferSize = MAXBUFSIZE / 2;
	  else
		  SoundBufferSize = MAXBUFSIZE / 4;
  }
  //sndlog=fopen("/snd.log","wb");
}; /* SoundInit */

void SwitchOnSound(void) {
  SetFreq(3,1000);
  ActiveChannel[3]=TRUE;
  BeebState76489.ToneVolume[3]=15<<VOLMAG;
}

void SetSound(char State) {
	if (!SoundDefault) return;
	if ((State==MUTED) && (SoundEnabled)) SoundReset();
	if ((State==UNMUTED) && (SoundDefault)) SoundInit();
}


/****************************************************************************/
/* Called to disable sound output                                           */
void SoundReset(void) {
	if (!DirectSoundEnabled)
	{
		waveOutReset(hwo);
		waveOutUnprepareHeader(hwo, &wh1, sizeof(WAVEHDR));
		waveOutUnprepareHeader(hwo, &wh2, sizeof(WAVEHDR));
		waveOutClose(hwo);
	}
	else
	{
		if (DSB1 != NULL)
		{
			DSB1->Stop();
			DSB1->Release();
			DSB1 = NULL;
		}
		if (DSound != NULL)
		{
			DSound->Release();
			DSound = NULL;
		}
	}
  ClearTrigger(SoundTrigger);
  SoundEnabled = 0;
} /* SoundReset */

/****************************************************************************/
/* Called in sysvia.cc when a write is made to the 76489 sound chip         */
void Sound_RegWrite(int value) {
  int trigger = 0;
  unsigned char VolChange;

  if (!SoundEnabled)
    return;

  if (!(value & 0x80)) {
    unsigned val=BeebState76489.ToneFreq[BeebState76489.LastToneFreqSet] & 15;

    /* Its changing the top half of the frequency */
    val |= (value & 0x3f)<<4;

    /* And update */
    BeebState76489.ToneFreq[BeebState76489.LastToneFreqSet]=val;
    SetFreq(BeebState76489.LastToneFreqSet+1,BeebState76489.ToneFreq[BeebState76489.LastToneFreqSet]);
    trigger = 1;
  } else {
    /* Another register */
	VolChange=0xff;
    switch ((value>>4) & 0x7) {
      case 0: /* Tone 3 freq */
        BeebState76489.ToneFreq[2]=(BeebState76489.ToneFreq[2] & 0x2f0) | (value & 0xf);
        SetFreq(3,BeebState76489.ToneFreq[2]);
        BeebState76489.LastToneFreqSet=2;
        break;

      case 1: /* Tone 3 vol */
        RealVolumes[3]=value&15;
		if ((BeebState76489.ToneVolume[3]==0) && ((value &15)!=15)) ActiveChannel[3]=TRUE;
        if ((BeebState76489.ToneVolume[3]!=0) && ((value &15)==15)) ActiveChannel[3]=FALSE;
        BeebState76489.ToneVolume[3]=(15-(value & 15))<<VOLMAG;
        BeebState76489.LastToneFreqSet=2;
		trigger = 1;
		VolChange=3;
        break;

      case 2: /* Tone 2 freq */
        BeebState76489.ToneFreq[1]=(BeebState76489.ToneFreq[1] & 0x2f0) | (value & 0xf);
        BeebState76489.LastToneFreqSet=1;
        SetFreq(2,BeebState76489.ToneFreq[1]);
        break;

      case 3: /* Tone 2 vol */
        RealVolumes[2]=value&15;
        if ((BeebState76489.ToneVolume[2]==0) && ((value &15)!=15)) ActiveChannel[2]=TRUE;
        if ((BeebState76489.ToneVolume[2]!=0) && ((value &15)==15)) ActiveChannel[2]=FALSE;
        BeebState76489.ToneVolume[2]=(15-(value & 15))<<VOLMAG;
        BeebState76489.LastToneFreqSet=1;
		trigger = 1;
		VolChange=2;
        break;

      case 4: /* Tone 1 freq (Possibly also noise!) */
        BeebState76489.ToneFreq[0]=(BeebState76489.ToneFreq[0] & 0x2f0) | (value & 0xf);
        BeebState76489.LastToneFreqSet=0;
        SetFreq(1,BeebState76489.ToneFreq[0]);
        break;

      case 5: /* Tone 1 vol */
        RealVolumes[1]=value&15;
        if ((BeebState76489.ToneVolume[1]==0) && ((value &15)!=15)) ActiveChannel[1]=TRUE;
        if ((BeebState76489.ToneVolume[1]!=0) && ((value &15)==15)) ActiveChannel[1]=FALSE;
        BeebState76489.ToneVolume[1]=(15-(value & 15))<<VOLMAG;
        BeebState76489.LastToneFreqSet=0;
		trigger = 1;
		VolChange=1;
        break;

      case 6: /* Noise control */
        BeebState76489.Noise.Freq=value &3;
        BeebState76489.Noise.FB=(value>>2)&1;

        trigger = 1;
        break;

      case 7: /* Noise volume */
        if ((BeebState76489.ToneVolume[0]==0) && ((value &15)!=15)) ActiveChannel[0]=TRUE;
        if ((BeebState76489.ToneVolume[0]!=0) && ((value &15)==15)) ActiveChannel[0]=FALSE;
		RealVolumes[0]=value&15;
        BeebState76489.ToneVolume[0]=(15-(value & 15))<<VOLMAG;
        trigger = 1;
		VolChange=0;
        break;
    };
  };
  if ((trigger) && (!ReloadingChip))
    SoundTrigger_Real();
}; /* Sound_RegWrite */


void DumpSound(void) {
	
}

void ClickRelay(unsigned char RState) {
  if (RelaySoundEnabled) {
	RelayPos=0;
	Relay=RState+1;
  }
}

void LoadSoundUEF(FILE *SUEF) {
	// Sound block
	unsigned char Chan;
	int Data;
	int RegVal; // This will be filled in by the data processor
	ReloadingChip=TRUE;
	for (Chan=1;Chan<4;Chan++) {
		Data=fget16(SUEF);
		// Send the data direct to Sound_RegWrite()
		RegVal=(((Chan-1)*2)<<4)|128;
		RegVal|=(Data&15);
		Sound_RegWrite(RegVal);
		RegVal=(Data&1008)>>4;
		Sound_RegWrite(RegVal);
	}
	for (Chan=1;Chan<4;Chan++) {
		Data=fgetc(SUEF);
		RegVal=((((Chan-1)*2)+1)<<4)|128;
		RegVal|=Data&15;
		Sound_RegWrite(RegVal);
		BeebState76489.ToneVolume[4-Chan]=(15-Data)<<VOLMAG;
		if (Data!=15) ActiveChannel[4-Chan]=TRUE; else ActiveChannel[4-Chan]=FALSE;
	}
	RegVal=224|(fgetc(SUEF)&7);
	Sound_RegWrite(RegVal);
	Data=fgetc(SUEF);
	RegVal=240|(Data&15);
	Sound_RegWrite(RegVal);
	BeebState76489.ToneVolume[0]=(15-Data)<<VOLMAG;
	if (Data!=15) ActiveChannel[0]=TRUE; else ActiveChannel[0]=FALSE;
	BeebState76489.LastToneFreqSet=fgetc(SUEF);
	GenIndex[0]=fget16(SUEF);
	GenIndex[1]=fget16(SUEF);
	GenIndex[2]=fget16(SUEF);
	GenIndex[3]=fget16(SUEF);
	ReloadingChip=FALSE;
}

void SaveSoundUEF(FILE *SUEF) {
	unsigned char Noise;
	fput16(0x046B,SUEF);
	fput32(20,SUEF);
	// Sound Block
	fput16(BeebState76489.ToneFreq[2],SUEF);
	fput16(BeebState76489.ToneFreq[1],SUEF);
	fput16(BeebState76489.ToneFreq[0],SUEF);
	fputc(RealVolumes[3],SUEF);
	fputc(RealVolumes[2],SUEF);
	fputc(RealVolumes[1],SUEF);
    Noise=BeebState76489.Noise.Freq |
          (BeebState76489.Noise.FB<<2);
	fputc(Noise,SUEF);
	fputc(RealVolumes[0],SUEF);
	fputc(BeebState76489.LastToneFreqSet,SUEF);
	fput16(GenIndex[0],SUEF);
	fput16(GenIndex[1],SUEF);
	fput16(GenIndex[2],SUEF);
	fput16(GenIndex[3],SUEF);
}
