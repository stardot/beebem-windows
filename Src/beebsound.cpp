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

#include "beebsound.h"

#include <iostream.h>

#ifdef SOUNDSUPPORT

#include <errno.h>
#ifndef WIN32
#include <unistd.h>
#else
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <dsound.h>
#include "main.h"
#endif
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifndef WIN32
#include <sys/time.h>
#include <sys/resource.h>
#endif

#include "6502core.h"
#include "port.h"

#ifndef WIN32
#include <fcntl.h>
#include <sys/soundcard.h>
#include <sys/ioctl.h>
#endif

/*#define DEBUGSOUND*/
/*#define DEBUGSOUNDTOFILE*/

#ifndef WIN32

#define PREFSAMPLERATE 40000
#define MAXBUFSIZE 256
static unsigned char Buffer[MAXBUFSIZE];

#else

#define PREFSAMPLERATE 22050
#define MAXBUFSIZE 8192
static unsigned char wb1[MAXBUFSIZE];
static unsigned char wb2[MAXBUFSIZE];

static HWAVEOUT hwo;
static WAVEHDR wh1, wh2;
static WAVEHDR *active_wh, *inactive_wh;
static unsigned char *active_wb, *inactive_wb;

static LPDIRECTSOUND DSound = NULL;
static LPDIRECTSOUNDBUFFER DSB1 = NULL;
static LPDIRECTSOUNDBUFFER DSB2 = NULL;

#endif

int SoundEnabled = 1;
int DirectSoundEnabled = 0;
int SoundSampleRate = PREFSAMPLERATE;
int SoundVolume = 3;
int SoundAutoTriggerTime;
int SoundBufferSize;

/* Number of places to shift the volume */
#define VOLMAG 3

struct {
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

static int ActiveChannels; /* Those channels with non-0 voolume */
static int devfd; /* Audio device id */
static int samplerate;
static double OurTime=0.0; /* Time in sample periods */

int SoundTrigger; /* Time to trigger a sound event */

static unsigned int GenIndex[4]; /* Used by the voice generators */
static int GenState[4];
static int bufptr=0;

/****************************************************************************/
#ifdef WIN32
/* Writes sound data to a DirectSound buffer */
HRESULT WriteToSoundBuffer(LPDIRECTSOUNDBUFFER lpDsb,
			PBYTE lpbSoundData, DWORD dwSoundBytes)
{
	LPVOID lpvPtr1;
	DWORD dwBytes1; 
	LPVOID lpvPtr2;
	DWORD dwBytes2;
	HRESULT hr;

	// Obtain write pointer.
	hr = lpDsb->Lock(0, dwSoundBytes, &lpvPtr1, 
		&dwBytes1, &lpvPtr2, &dwBytes2, DSBLOCK_FROMWRITECURSOR);
	if(hr == DSERR_BUFFERLOST)
	{
		hr = lpDsb->Restore();
		if (hr == DS_OK)
			hr = lpDsb->Lock(0, dwSoundBytes,
				&lpvPtr1, &dwBytes1, &lpvPtr2, &dwBytes2, DSBLOCK_FROMWRITECURSOR);
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
		hr = lpDsb->Unlock(lpvPtr1, dwBytes1, lpvPtr2, dwBytes2);
	}
	return hr;
}
#endif

/****************************************************************************/
/* DestTime is in samples */
static void PlayUpTil(double DestTime) {
  int tmptotal,channel,bufinc;

  /*cerr << "PlayUpTil: DestTime=" << DestTime << " OurTime=" << OurTime << "\n";*/

  while (DestTime>OurTime) {
#ifdef WIN32
    if (bufptr == 0 && ActiveChannels == 0) {
#else
	if ((BeebState76489.ToneVolume[0]==0) && 
      (BeebState76489.ToneVolume[1]==0) &&
      (BeebState76489.ToneVolume[2]==0) &&
      (BeebState76489.ToneVolume[3]==0) && 0) {
#endif
      OurTime=DestTime;
    } else {
      for(bufinc=0;(bufptr<SoundBufferSize) && ((OurTime+bufinc)<DestTime);bufptr++,bufinc++) {
        tmptotal=0;
        for(channel=1;channel<=3;channel++) {
#ifdef WIN32
		  if (GenState[channel])
			tmptotal+=BeebState76489.ToneVolume[channel];
		  else
			tmptotal-=BeebState76489.ToneVolume[channel];
#else
          tmptotal+=GenState[channel]?BeebState76489.ToneVolume[channel]:0;
#endif
          GenIndex[channel]++;
        
          if (GenIndex[channel]>=BeebState76489.ChangeSamps[channel]) {
            GenIndex[channel]=0;
            GenState[channel]^=1;
          };
        }; /* Channel loop */

        /* Now put in noise generator stuff */
        if (BeebState76489.Noise.FB) {
          /* White noise */
#ifdef WIN32
		  if (GenState[0])
			tmptotal+=BeebState76489.ToneVolume[0];
		  else
			tmptotal-=BeebState76489.ToneVolume[0];
#else
          tmptotal+=GenState[0]?BeebState76489.ToneVolume[0]:0;
#endif
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
#ifdef WIN32
		  if (GenState[0])
			tmptotal+=BeebState76489.ToneVolume[0];
		  else
			tmptotal-=BeebState76489.ToneVolume[0];
#else
          tmptotal+=GenState[0]?BeebState76489.ToneVolume[0]:0;
#endif
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
        };

#ifndef WIN32
        tmptotal/=(4*SoundVolume);
        Buffer[bufptr]=tmptotal;
#else
		/* Make it a bit louder under Windows */
		if (ActiveChannels>2)
			tmptotal/=ActiveChannels;
		else
	        tmptotal/=2;
		inactive_wb[bufptr] = tmptotal/SoundVolume + 128;
#endif
      }; /* buffer loop */

#ifndef WIN32
      if (write(devfd,Buffer,bufptr)==-1) {
        cerr << "Write on audio device failed\n";
      };

 /*   fprintf(stderr,"PlayUpTil: After write: bufptr=%d OurTime=%f\n",bufptr,OurTime);*/
      OurTime+=bufinc;
      bufptr=0;

#else
	  /* Only write data when buffer is full */
	  if (bufptr == SoundBufferSize)
	  {
#ifdef DEBUGSOUNDTOFILE
		FILE *fd = fopen("audio.dbg", "a+b");
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
			if (active_wb == wb1)
			{
				hr = WriteToSoundBuffer(DSB2, inactive_wb, SoundBufferSize);
				if (hr == DS_OK)
				{
					hr = DSB2->Play(0,0,0);
					if(hr == DSERR_BUFFERLOST)
					{
						hr = DSB2->Restore();
						if (hr == DS_OK)
							hr = DSB2->Play(0,0,0);
					}
				}
			}
			else
			{
				hr = WriteToSoundBuffer(DSB1, inactive_wb, SoundBufferSize);
				if (hr == DS_OK)
				{
					hr = DSB1->Play(0,0,0);
					if(hr == DSERR_BUFFERLOST)
					{
						hr = DSB1->Restore();
						if (hr == DS_OK)
							hr = DSB1->Play(0,0,0);
					}
				}
			}
			if (hr != DS_OK)
			{
				char  errstr[200];
				sprintf(errstr,"Direct Sound write failed\nFailure code %X",hr);
				MessageBox(GETHWND,errstr,"BBC Emulator",MB_OK|MB_ICONERROR);
				SoundReset();
			}
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
#endif
    }; /* If no volume */
  }; /* While time */

}; /* PlayUpTil */

/****************************************************************************/
/* Convert time in cycles to time in samples                                */
static unsigned int LastBeebCycle=0; /* Last parameter to this function */
static double LastOurTime=0; /* Last result of this function */

static double CyclesToSamples(unsigned int beebtime) {
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

  tmp*=samplerate;
  tmp/=2000000.0; /* Few - glad thats a double! */

  LastOurTime+=tmp;

  return LastOurTime;
}; /* CyclesToSamples */

/****************************************************************************/
static void PlayTilNow(void) {
  double nowsamps=CyclesToSamples(TotalCycles);

  PlayUpTil(nowsamps);
}; /* PlayTilNow */

/****************************************************************************/
#ifdef WIN32
/* Creates a DirectSound buffer */
HRESULT CreateSoundBuffer(
		LPDIRECTSOUND lpDirectSound, LPDIRECTSOUNDBUFFER *lplpDsb)
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
	wf.nBlockAlign = wf.wBitsPerSample * wf.nChannels / 8;
	wf.nAvgBytesPerSec = wf.nSamplesPerSec * wf.nBlockAlign;
	wf.cbSize = 0;

	// Set up DSBUFFERDESC structure.
	memset(&dsbdesc, 0, sizeof(DSBUFFERDESC)); // Zero it out.
	dsbdesc.dwSize = sizeof(DSBUFFERDESC);

	// Need default controls (pan, volume, frequency).
	dsbdesc.dwFlags = DSBCAPS_CTRLDEFAULT;
	dsbdesc.dwBufferBytes = SoundBufferSize;
	dsbdesc.lpwfxFormat = (LPWAVEFORMATEX)&wf;

	// Create buffer.
	hr = DSound->CreateSoundBuffer(&dsbdesc, lplpDsb, NULL);

	return hr;
}
#endif

/****************************************************************************/
static void InitAudioDev(int sampleratein) {
  int parm;

  samplerate=sampleratein;

#ifndef WIN32
#ifdef DEBUGSOUNDTOFILE
  if (devfd=open("audiodebug",O_WRONLY|O_CREAT/*|O_NONBLOCK*/),devfd<=0) {
    perror("open audio debug");
    cerr << "Couldn't open audiodebug\n";
    exit(1);
  };
#else
  if (devfd=open("/dev/dsp",O_WRONLY/*|O_NONBLOCK*/),devfd<=0) {
    cerr << "Couldn't open /dev/dsp\n";
    exit(1);
  };

  /* The following code is based on Philip VanBaren's 'freq5' */
  parm=0x00080008; 
  if (ioctl(devfd,SNDCTL_DSP_SETFRAGMENT,&parm)<0) {
    cerr << "Couldn't set sound fragment size\n";
    exit(1);
  };

  if (ioctl(devfd,SOUND_PCM_RESET,0)<0) {
    cerr << "Couldn't reset sound dev\n";
    exit(1);
  };

  parm=8;
  if (ioctl(devfd,SOUND_PCM_WRITE_BITS,&parm)<0) {
    cerr << "Couldn't set sound sample depth\n";
    exit(1);
  };

  parm=1;
  if (ioctl(devfd,SOUND_PCM_WRITE_CHANNELS,&parm)<0) {
    cerr << "Couldn't set mono\n";
    exit(1);
  };

  ioctl(devfd,SOUND_PCM_SYNC,0);

  if (ioctl(devfd,SOUND_PCM_WRITE_RATE,&samplerate)<0) {
    cerr << "Couldn't set sample rate to %d\n",samplerate;
    exit(1);
  };
#endif
#else
	if (!DirectSoundEnabled)
	{
		MMRESULT mmresult;
		WAVEFORMATEX wfx;

		wfx.wFormatTag = WAVE_FORMAT_PCM;
		wfx.nChannels = 1;
		wfx.nSamplesPerSec = samplerate;
		wfx.wBitsPerSample = 8;
		wfx.nBlockAlign = wfx.wBitsPerSample * wfx.nChannels / 8;
		wfx.nAvgBytesPerSec = wfx.nBlockAlign * wfx.nSamplesPerSec;
		wfx.cbSize = 0;

		mmresult = waveOutOpen(&hwo,WAVE_MAPPER,&wfx,0,0,CALLBACK_NULL);
		if (mmresult != MMSYSERR_NOERROR)
		{
	  		MessageBox(GETHWND,"Could not open a wave sound device","BBC Emulator",MB_OK|MB_ICONERROR);
			SoundReset();
		}

		active_wh = NULL;
		active_wb = NULL;
		inactive_wh = &wh1;
		inactive_wb = wb1;
	}
	else
	{
		HRESULT hr;
		hr = DirectSoundCreate(NULL, &DSound, NULL);
		if(hr == DS_OK)
		{
			hr = DSound->SetCooperativeLevel(mainWin->GethWnd(), DSSCL_NORMAL);
		}
		if(hr == DS_OK)
		{
			hr = CreateSoundBuffer(DSound, &DSB1);
		}
		if(hr == DS_OK)
		{
			hr = CreateSoundBuffer(DSound, &DSB2);
		}
		if (hr != DS_OK)
		{
			char  errstr[200];
			sprintf(errstr,"Direct Sound initialisation failed\nFailure code %X",hr);
			MessageBox(GETHWND,errstr,"BBC Emulator",MB_OK|MB_ICONERROR);
			SoundReset();
		}  

		active_wb = NULL;
		inactive_wb = wb1;
	}
#endif
}; /* InitAudioDev */

/****************************************************************************/
/* The 'freqval' variable is the value as sene by the 76489                 */
static void SetFreq(int Channel, int freqval) {
  int freq;
  int ChangeSamps; /* Number of samples after which to change */

  if (freqval==0) {
    ChangeSamps=INT_MAX;
  } else {
    freq=4000000/(32*freqval);
  
    if (freq>samplerate) {
      ChangeSamps=INT_MAX; /* Way to high frequency - shut off */
    } else {
      if (freq>(samplerate/2)) {
        /* Hmm - a bit high - make it top out - change on every sample */
        /* What we should be doing is moving to sine wave at 1/6 samplerate */
        ChangeSamps=2;
      } else {
        ChangeSamps=(int)((((double)samplerate/(double)freq)/2.0)+0.5);
      };
    }; /* freq<=samplerate */
  }; /* Freqval!=0 */

  BeebState76489.ChangeSamps[Channel]=ChangeSamps;
};

/****************************************************************************/
void SoundTrigger_Real(void) {
  if (SoundEnabled) {
    PlayTilNow();
    SetTrigger(SoundAutoTriggerTime,SoundTrigger);
  }
}; /* SoundTrigger_Real */

/****************************************************************************/
/* Called to enable sound output                                            */
void SoundInit() {
  /* I don't have any info on what this lot should be */
  BeebState76489.LastToneFreqSet=0;
  BeebState76489.ToneVolume[0]=BeebState76489.ToneVolume[1]=BeebState76489.ToneVolume[2]=BeebState76489.ToneVolume[3]=0;
  BeebState76489.ToneFreq[0]=BeebState76489.ToneFreq[1]=BeebState76489.ToneFreq[2]=0x2ff;
  BeebState76489.Noise.FB=0;
  BeebState76489.Noise.Freq=0;
  ClearTrigger(SoundTrigger);
  ActiveChannels=0;
  SoundEnabled=1;
  LastBeebCycle=TotalCycles;
  LastOurTime=(double)LastBeebCycle * (double)SoundSampleRate / 2000000.0;
  OurTime=LastOurTime;
  bufptr=0;
  if (SoundSampleRate == 44100)
    SoundBufferSize = MAXBUFSIZE;
  else if (SoundSampleRate == 22050)
    SoundBufferSize = MAXBUFSIZE / 2;
  else
    SoundBufferSize = MAXBUFSIZE / 4;
  SoundAutoTriggerTime = ((200000*SoundBufferSize)/SoundSampleRate)*10; /* Need to avoid overflow */
  InitAudioDev(SoundSampleRate);
}; /* SoundInit */

/****************************************************************************/
/* Called to disable sound output                                           */
void SoundReset(void) {
#ifdef WIN32
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
			DSB1->Release();
			DSB1 = NULL;
		}
		if (DSB2 != NULL)
		{
			DSB2->Release();
			DSB2 = NULL;
		}
		if (DSound != NULL)
		{
			DSound->Release();
			DSound = NULL;
		}
	}
#else
  close(devfd);
#endif
  ClearTrigger(SoundTrigger);
  SoundEnabled = 0;
} /* SoundReset */

/****************************************************************************/
/* Called in sysvia.cc when a write is made to the 76489 sound chip         */
void Sound_RegWrite(int value) {
  int trigger = 0;

  if (!SoundEnabled)
    return;

#ifdef DEBUGSOUND
  cerr << "Sound_RegWrite - Value=" << value << "\n";
#endif

  if (!(value & 0x80)) {
    unsigned val=BeebState76489.ToneFreq[BeebState76489.LastToneFreqSet] & 15;

    /* Its changing the top half of the frequency */
    val |= (value & 0x3f)<<4;

    /* And update */
    BeebState76489.ToneFreq[BeebState76489.LastToneFreqSet]=val;
    SetFreq(BeebState76489.LastToneFreqSet+1,BeebState76489.ToneFreq[BeebState76489.LastToneFreqSet]);
#ifdef DEBUGSOUND
    cerr << "Sound_RegWrite: Freq of tone " << (BeebState76489.LastToneFreqSet+1) << " now " << val << "\n";
#endif
    trigger = 1;
  } else {
    /* Another register */
    switch ((value>>4) & 0x7) {
      case 0: /* Tone 3 freq */
        BeebState76489.ToneFreq[2]=(BeebState76489.ToneFreq[2] & 0x2f0) | (value & 0xf);
#ifdef DEBUGSOUND
	cerr << "Sound_RegWrite: Freq of tone 3 now " << BeebState76489.ToneFreq[2] << "\n";
#endif
        SetFreq(3,BeebState76489.ToneFreq[2]);
        BeebState76489.LastToneFreqSet=2;
        break;

      case 1: /* Tone 3 vol */
        if ((BeebState76489.ToneVolume[3]==0) && ((value &15)!=15)) ActiveChannels++;
        if ((BeebState76489.ToneVolume[3]!=0) && ((value &15)==15)) ActiveChannels--;
        BeebState76489.ToneVolume[3]=(15-(value & 15))<<VOLMAG;
#ifdef DEBUGSOUND
	cerr << "Sound_RegWrite: Vol of tone 3 now " << BeebState76489.ToneVolume[2] << "\n";
#endif
        BeebState76489.LastToneFreqSet=2;
        trigger = 1;
        break;

      case 2: /* Tone 2 freq */
        BeebState76489.ToneFreq[1]=(BeebState76489.ToneFreq[1] & 0x2f0) | (value & 0xf);
#ifdef DEBUGSOUND
	cerr << "Sound_RegWrite: Freq of tone 2 now " << BeebState76489.ToneFreq[1] << "\n";
#endif
        BeebState76489.LastToneFreqSet=1;
        SetFreq(2,BeebState76489.ToneFreq[1]);
        break;

      case 3: /* Tone 2 vol */
        if ((BeebState76489.ToneVolume[2]==0) && ((value &15)!=15)) ActiveChannels++;
        if ((BeebState76489.ToneVolume[2]!=0) && ((value &15)==15)) ActiveChannels--;
        BeebState76489.ToneVolume[2]=(15-(value & 15))<<VOLMAG;
#ifdef DEBUGSOUND
	cerr << "Sound_RegWrite: Vol of tone 2 now " << BeebState76489.ToneVolume[2] << "\n";
#endif
        BeebState76489.LastToneFreqSet=1;
        trigger = 1;
        break;

      case 4: /* Tone 1 freq (Possibly also noise!) */
        BeebState76489.ToneFreq[0]=(BeebState76489.ToneFreq[0] & 0x2f0) | (value & 0xf);
#ifdef DEBUGSOUND
	cerr << "Sound_RegWrite: Freq of tone 1 now " << BeebState76489.ToneFreq[0] << "\n";
#endif
        BeebState76489.LastToneFreqSet=0;
        SetFreq(1,BeebState76489.ToneFreq[0]);
        break;

      case 5: /* Tone 1 vol */
        if ((BeebState76489.ToneVolume[1]==0) && ((value &15)!=15)) ActiveChannels++;
        if ((BeebState76489.ToneVolume[1]!=0) && ((value &15)==15)) ActiveChannels--;
        BeebState76489.ToneVolume[1]=(15-(value & 15))<<VOLMAG;
#ifdef DEBUGSOUND
	cerr << "Sound_RegWrite: Vol of tone 1 now " << BeebState76489.ToneVolume[1] << "\n";
#endif
        BeebState76489.LastToneFreqSet=0;
        trigger = 1;
        break;

      case 6: /* Noise control */
        BeebState76489.Noise.Freq=value &3;
        BeebState76489.Noise.FB=(value>>2)&1;

#ifdef DEBUGSOUND
	cerr << "Sound_RegWrite: Noise now " << (BeebState76489.Noise.FB?"Periodic":"White")  << " with Frequency setting " << BeebState76489.Noise.Freq << "\n";
#endif
        trigger = 1;
        break;

      case 7: /* Noise volume */
        if ((BeebState76489.ToneVolume[0]==0) && ((value &15)!=15)) ActiveChannels++;
        if ((BeebState76489.ToneVolume[0]!=0) && ((value &15)==15)) ActiveChannels--;
        BeebState76489.ToneVolume[0]=(15-(value & 15))<<VOLMAG;
#ifdef DEBUGSOUND
	cerr << "Sound_RegWrite: Vol of noise now " << BeebState76489.ToneVolume[0] << "\n";
#endif
        trigger = 1;
        break;
    };
  };
  if (trigger)
    SoundTrigger_Real();
}; /* Sound_RegWrite */

#else
void ADummyRoutine(int a) {
  cerr << "Just so the compiler doesn't get confused by an empty file!\n";
}
#endif
