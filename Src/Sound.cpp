/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 1994  David Alan Gilbert
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

/* Win32 port - Mike Wyatt 7/6/97 */
/* Conveted Win32 port to use DirectSound - Mike Wyatt 11/1/98 */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "Sound.h"
#include "6502core.h"
#include "AviWriter.h"
#include "BeebWin.h"
#include "Main.h"
#include "SoundStreamer.h"
#include "Speech.h"
#include "UEFState.h"

//  #define DEBUGSOUNDTOFILE

const int DEFAULT_SAMPLE_RATE = 44100;
const int MAXBUFSIZE = 32768;

static unsigned char SoundBuf[MAXBUFSIZE];

unsigned char SpeechBuf[MAXBUFSIZE];

struct SoundSample
{
	const char *pFilename;
	unsigned char *pBuf;
	int len;
	int pos;
	bool playing;
	bool repeat;
};

static SoundSample SoundSamples[] = {
	{ "RelayOn.snd", NULL, 0, 0, false, false },
	{ "RelayOff.snd", NULL, 0, 0, false, false },
	{ "DriveMotor.snd", NULL, 0, 0, false, false },
	{ "HeadLoad.snd", NULL, 0, 0, false, false },
	{ "HeadUnload.snd", NULL, 0, 0, false, false },
	{ "HeadSeek.snd", NULL, 0, 0, false, false },
	{ "HeadStep.snd", NULL, 0, 0, false, false }
};

const int NUM_SOUND_SAMPLES = sizeof(SoundSamples) / sizeof(SoundSample);

static bool SoundSamplesLoaded = false;

SoundStreamerType SelectedSoundStreamer;
bool SoundEnabled = false;
bool RelaySoundEnabled = false;
bool DiscDriveSoundEnabled = false;
bool SoundChipEnabled = true;

unsigned int SoundSampleRate = DEFAULT_SAMPLE_RATE;
int SoundVolume = 100; //Percentage
static int SoundAutoTriggerTime;
static int SoundBufferSize = SoundSampleRate / 50;

static double CSC[4] = { 0, 0, 0, 0 };
static double CSA[4] = { 0, 0, 0, 0 }; // ChangeSamps Adjusts

bool SoundExponentialVolume = true;

/* Number of places to shift the volume */
#define VOLMAG 3

static int Speech[4];

// static FILE *sndlog = NULL;

static struct {
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

static int RealVolumes[4]; // Holds the real volume values for state save use

static bool ActiveChannel[4] = {false, false, false, false}; // Those channels with non-0 volume
// Set it to an array for more accurate sound generation

static double OurTime=0.0; /* Time in sample periods */

int SoundTrigger; /* Time to trigger a sound event */

static unsigned int GenIndex[4]; /* Used by the voice generators */
static int GenState[4];
static int bufptr=0;
bool SoundDefault;
double SoundTuning=0.0; // Tuning offset

static int GetVol(int vol);

struct AudioType TapeAudio; // Tape audio decoder stuff
bool TapeSoundEnabled;
bool PartSamples = true;

SoundStreamer *pSoundStreamer = nullptr;

/****************************************************************************/

// Writes sound data to a sound buffer

static void WriteToSoundBuffer(BYTE *pSoundData)
{
	if (pSoundStreamer != nullptr)
	{
		pSoundStreamer->Stream(pSoundData);
	}

	if (aviWriter != nullptr)
	{
		HRESULT hResult = aviWriter->WriteSound(pSoundData, SoundBufferSize);

		if (FAILED(hResult) && hResult != E_UNEXPECTED)
		{
			mainWin->Report(MessageType::Error, "Failed to write sound to AVI file");

			mainWin->EndVideo();
		}
	}
}

/****************************************************************************/
/* DestTime is in samples */

void PlayUpTil(double DestTime)
{
	#if ENABLE_SPEECH

	int SpeechPtr = 0;

	if ((MachineType != Model::Master128 && MachineType != Model::MasterET) && SpeechEnabled)
	{
		SpeechPtr = 0;
		memset(SpeechBuf, 128, sizeof(SpeechBuf));
		int len = (int) (DestTime - OurTime + 1);
		if (len > MAXBUFSIZE)
			len = MAXBUFSIZE;
		SpeechUpdate(SpeechBuf, len);
	}

	#endif

	while (DestTime > OurTime) {
		int bufinc = 0;

		for (bufinc = 0; bufptr < SoundBufferSize && OurTime + bufinc < DestTime; bufptr++, bufinc++) {
			int tt;
			int tmptotal = 0;

			if (SoundChipEnabled) {
				// Channels 1 to 3 are tone generators
				for (int channel = 1; channel <= 3; channel++) {
					if (ActiveChannel[channel]) {
						if ((GenState[channel]) && (!Speech[channel]))
							tmptotal+=BeebState76489.ToneVolume[channel];
						if ((!GenState[channel]) && (!Speech[channel]))
							tmptotal-=BeebState76489.ToneVolume[channel];
						if (Speech[channel])
							tmptotal+=(BeebState76489.ToneVolume[channel]-GetVol(7));
						GenIndex[channel]++;
						tt=(int)CSC[channel];
						if (!PartSamples) tt=0;
						if (GenIndex[channel]>=(BeebState76489.ChangeSamps[channel]+tt)) {
							if (CSC[channel] >= 1.0)
								CSC[channel]-=1.0;
							CSC[channel]+=CSA[channel];
							GenIndex[channel]=0;
							GenState[channel]^=1;
						}
					}
				}

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
							if (GenIndex[0] >= SoundSampleRate / 10000) {
								GenIndex[0]=0;
								GenState[0]=rand() & 1;
							}
							break;

						case 1: /* Med */
							if (GenIndex[0] >= SoundSampleRate / 5000) {
								GenIndex[0]=0;
								GenState[0]=rand() & 1;
							}
							break;

						case 2: /* High */
							if (GenIndex[0] >= SoundSampleRate / 2500) {
								GenIndex[0]=0;
								GenState[0]=rand() & 1;
							}
							break;

						case 3: /* as channel 1 */
							if (GenIndex[0]>=BeebState76489.ChangeSamps[1]) {
								GenIndex[0]=0;
								GenState[0]=rand() & 1;
							}
							break;
						} /* Freq type switch */
					} else {
						/* Periodic */
						if (GenState[0])
							tmptotal+=BeebState76489.ToneVolume[0];
						else
							tmptotal-=BeebState76489.ToneVolume[0];
						GenIndex[0]++;
						switch (BeebState76489.Noise.Freq) {
						case 2: /* Low */
							if (GenState[0]) {
								if (GenIndex[0] >= SoundSampleRate / 125) {
									GenIndex[0]=0;
									GenState[0]=0;
								}
							} else {
								if (GenIndex[0] >= SoundSampleRate / 1250) {
									GenIndex[0]=0;
									GenState[0]=1;
								}
							}
							break;

						case 1: /* Med */
							if (GenState[0]) {
								if (GenIndex[0] >= SoundSampleRate / 250) {
									GenIndex[0]=0;
									GenState[0]=0;
								}
							} else {
								if (GenIndex[0] >= SoundSampleRate / 2500) {
									GenIndex[0]=0;
									GenState[0]=1;
								}
							}
							break;

						case 0: /* High */
							if (GenState[0]) {
								if (GenIndex[0] >= SoundSampleRate / 500) {
									GenIndex[0]=0;
									GenState[0]=0;
								}
							} else {
								if (GenIndex[0] >= SoundSampleRate / 5000) {
									GenIndex[0]=0;
									GenState[0]=1;
								}
							}
							break;

						case 3: /* Tone gen 1 */
							tt=(int)CSC[0];
							if (GenIndex[0]>=(BeebState76489.ChangeSamps[1]+tt)) {
								static int per=0;
								CSC[0]+=CSA[1]-tt;
								GenIndex[0]=0;
								GenState[0]=(per==0);
								if (++per==30) {
									per=0;
								}
							}
							break;
						} /* Freq type switch */
					}
				}
			}

			tmptotal /= 4;

			#if ENABLE_SPEECH

			// Mix in speech sound
			if (SpeechEnabled && (MachineType != Model::Master128 && MachineType != Model::MasterET) {
				tmptotal += (SpeechBuf[SpeechPtr++] - 128) * 2;
			}

			#endif

			// Mix in sound samples here
			for (int i = 0; i < NUM_SOUND_SAMPLES; ++i) {
				if (SoundSamples[i].playing) {
					tmptotal+=(SoundSamples[i].pBuf[SoundSamples[i].pos]-128)*2;
					SoundSamples[i].pos += 44100 / SoundSampleRate;
					if (SoundSamples[i].pos >= SoundSamples[i].len) {
						if (SoundSamples[i].repeat)
							SoundSamples[i].pos = 0;
						else
							SoundSamples[i].playing = false;
					}
				}
			}

			if (TapeSoundEnabled) {
				// Mix in tape sound here
				int tapetotal = 0;

				if ((TapeAudio.Enabled) && (TapeAudio.Signal==2)) {
					if (TapeAudio.Samples++>=36) TapeAudio.Samples=0;
					tapetotal=(int)(sin(((TapeAudio.Samples*20)*3.14)/180)*20);
				}
				if ((TapeAudio.Enabled) && (TapeAudio.Signal==1)) {
					tapetotal=(int)(sin(((TapeAudio.Samples*(10*(1+TapeAudio.CurrentBit)))*3.14)/180)*(20+(10*(1-TapeAudio.CurrentBit))));
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
				}
				tmptotal+=tapetotal;
			}

			// Reduce amplitude to reduce clipping
			tmptotal/=2;

			// Apply volume
			tmptotal = (tmptotal * SoundVolume)/100;

			// Range check
			if (tmptotal > 127)
				tmptotal = 127;
			if (tmptotal < -127)
				tmptotal = -127;

			SoundBuf[bufptr] = (unsigned char)(tmptotal + 128);
		} /* buffer loop */

		/* Only write data when buffer is full */
		if (bufptr == SoundBufferSize)
		{
#ifdef DEBUGSOUNDTOFILE
			FILE *fd = fopen("/audio.dbg", "a+b");
			if (fd != NULL)
			{
				fwrite(SoundBuf, 1, SoundBufferSize, fd);
				fclose(fd);
			}
			else
			{
				mainWin->ReportError("Failed to open audio.dbg");
				exit(1);
			}
#else
			WriteToSoundBuffer(SoundBuf);
#endif
			// buffer swapping no longer needed
			bufptr=0;
		}

		OurTime += bufinc;
	}
}

/****************************************************************************/
/* Convert time in cycles to time in samples                                */
static int LastBeebCycle=0; /* Last parameter to this function */
static double LastOurTime=0; /* Last result of this function */

static double CyclesToSamples(int BeebCycles) {
  double tmp;

  /* OK - beeb cycles are in 2MHz units, ours are in 1/samplerate */
  /* This is all done incrementally - find the number of ticks since the last call
     in both domains.  This does mean this should only be called once */
  /* Extract number of cycles since last call */
  if (BeebCycles < LastBeebCycle) {
    /* Wrap around in beebs time */
    tmp = ((double)CycleCountWrap-(double)LastBeebCycle)+(double)BeebCycles;
  } else {
    tmp = (double)BeebCycles-(double)LastBeebCycle;
  }

  tmp /= mainWin->m_RealTimeTarget ? mainWin->m_RealTimeTarget : 1;
  /*fprintf(stderr,"Convert tmp=%f\n",tmp); */

  LastBeebCycle = BeebCycles;

  tmp *= SoundSampleRate;
  tmp /= 2000000.0; /* Few - glad thats a double! */

  LastOurTime += tmp;
  return LastOurTime;
}

/****************************************************************************/

static void InitAudioDev()
{
	if (pSoundStreamer != nullptr)
	{
		delete pSoundStreamer;
	}

	pSoundStreamer = CreateSoundStreamer(SoundSampleRate, 8, 1);

	SoundEnabled = pSoundStreamer != nullptr;
}

void LoadSoundSamples()
{
	char FileName[256];

	if (!SoundSamplesLoaded)
	{
		for (int i = 0; i < NUM_SOUND_SAMPLES; ++i)
		{
			strcpy(FileName, mainWin->GetAppPath());
			strcat(FileName, SoundSamples[i].pFilename);
			FILE *fd = fopen(FileName, "rb");
			if (fd != NULL) {
				fseek(fd, 0, SEEK_END);
				SoundSamples[i].len = ftell(fd);
				SoundSamples[i].pBuf = (unsigned char *)malloc(SoundSamples[i].len);
				fseek(fd, 0, SEEK_SET);
				fread(SoundSamples[i].pBuf, 1, SoundSamples[i].len, fd);
				fclose(fd);
			}
			else
			{
				mainWin->Report(MessageType::Error,
				                "Could not open sound sample file:\n  %s",
				                FileName);
			}
		}

		SoundSamplesLoaded = true;
	}
}

/****************************************************************************/

// The 'freqval' variable is the value as seen by the 76489

static void SetFreq(int Channel, int freqval) {
  //fprintf(sndlog,"Channel %d - Value %d\n",Channel,freqval);

	if (freqval == 0) {
		freqval = 1024;
	}

	if (freqval < 5) {
		Speech[Channel] = 1;
	}
	else {
		Speech[Channel] = 0;
	}

	unsigned int freq = 4000000 / (32 * freqval);

	double t = (((double)SoundSampleRate / (double)freq) / 2.0) + SoundTuning;
	int ChangeSamps = (int)t; // Number of samples after which to change

	CSA[Channel] = (double)(t - ChangeSamps);
	CSC[Channel] = CSA[Channel]; // We look ahead, so should already include the fraction on the first update

	if (Channel == 1) {
		CSC[0] = CSC[1];
	}

	BeebState76489.ChangeSamps[Channel] = ChangeSamps;
}

/****************************************************************************/

static void UpdateSound()
{
  double CurrentTimeInSamples = CyclesToSamples(TotalCycles);
  PlayUpTil(CurrentTimeInSamples);

  SoundTrigger = TotalCycles + SoundAutoTriggerTime;
}

void SoundPoll()
{
	if (SoundTrigger <= TotalCycles)
	{
		UpdateSound();
	}
}

void SoundChipReset() {
  BeebState76489.LastToneFreqSet=0;
  BeebState76489.ToneVolume[0]=0;
  BeebState76489.ToneVolume[1]=BeebState76489.ToneVolume[2]=BeebState76489.ToneVolume[3]=GetVol(15);
  BeebState76489.ToneFreq[0]=BeebState76489.ToneFreq[1]=BeebState76489.ToneFreq[2]=1000;
  BeebState76489.ToneFreq[3]=1000;
  BeebState76489.Noise.FB=0;
  BeebState76489.Noise.Freq=0;
  ActiveChannel[0] = false;
  ActiveChannel[1] = false;
  ActiveChannel[2] = false;
  ActiveChannel[3] = false;
}

/****************************************************************************/
/* Called to enable sound output                                            */
void SoundInit() {
  ClearTrigger(SoundTrigger);
  LastBeebCycle=TotalCycles;
  LastOurTime=(double)LastBeebCycle * (double)SoundSampleRate / 2000000.0;
  OurTime=LastOurTime;
  bufptr=0;
  InitAudioDev();
  if (SoundSampleRate == 44100) SoundAutoTriggerTime = 5000;
  if (SoundSampleRate == 22050) SoundAutoTriggerTime = 10000;
  if (SoundSampleRate == 11025) SoundAutoTriggerTime = 20000;
  SoundBufferSize = (int)(pSoundStreamer ? pSoundStreamer->BufferSize() : SoundSampleRate / 50);
  LoadSoundSamples();
  SoundTrigger = TotalCycles + SoundAutoTriggerTime;
}

void SwitchOnSound() {
  SetFreq(3,1000);
  ActiveChannel[3] = true;
  BeebState76489.ToneVolume[3]=GetVol(15);
}

void SetSound(SoundState state)
{
	switch (state)
	{
	case SoundState::Muted:
		SoundStreamer::PauseAll();
		break;
	case SoundState::Unmuted:
		SoundStreamer::PlayAll();
		break;
	}
}

/****************************************************************************/
/* Called to disable sound output                                           */
void SoundReset()
{
	if (pSoundStreamer != nullptr)
	{
		delete pSoundStreamer;
		pSoundStreamer = nullptr;
	}

	ClearTrigger(SoundTrigger);

	SoundEnabled = false;
}

/****************************************************************************/
/* Called in sysvia.cpp when a write is made to the 76489 sound chip        */
void Sound_RegWrite(int value)
{
	unsigned int reg, tone, channel; // may not be tone, why not index volume and tone with the same index?

	if (value & 0x80)
	{
		reg = (value >> 4) & 7;
		BeebState76489.LastToneFreqSet = (2 - (reg >> 1)) & 3; // use 3 for noise (0,1->2, 2,3->1, 4,5->0, 6,7->3)
		tone = (BeebState76489.ToneFreq[BeebState76489.LastToneFreqSet] & ~15) | (value & 15);
	}
	else
	{
		reg = ((2 - BeebState76489.LastToneFreqSet) & 3) << 1; // (0->4, 1->2, 2->0, 3->6)
		tone = (BeebState76489.ToneFreq[BeebState76489.LastToneFreqSet] & 15) | ((value & 0x3F) << 4);
	}

	channel = (1 + BeebState76489.LastToneFreqSet) & 3; // (0->1, 1->2, 2->3, 3->0)

	switch (reg)
	{
	case 0: // Tone 3 freq
	case 2: // Tone 2 freq
	case 4: // Tone 1 freq
		BeebState76489.ToneFreq[BeebState76489.LastToneFreqSet] = tone;
		SetFreq(channel, tone);
		break;

	case 6: // Noise control
		BeebState76489.Noise.Freq = value & 3;
		BeebState76489.Noise.FB = (value >> 2) & 1;
		break;

	case 1: // Tone 3 vol
	case 3: // Tone 2 vol
	case 5: // Tone 1 vol
	case 7: // Tone 0 vol
		RealVolumes[channel] = value & 15;
		if ((BeebState76489.ToneVolume[channel] == 0) && ((value & 15) != 15)) ActiveChannel[channel] = true;
		if ((BeebState76489.ToneVolume[channel] != 0) && ((value & 15) == 15)) ActiveChannel[channel] = false;
		BeebState76489.ToneVolume[channel] = GetVol(15 - (value & 15));
		break;
	}

	UpdateSound();
}

void ClickRelay(bool RelayState) {
	if (RelaySoundEnabled) {
		if (RelayState) {
			PlaySoundSample(SAMPLE_RELAY_ON, false);
		}
		else {
			PlaySoundSample(SAMPLE_RELAY_OFF, false);
		}
	}
}

void PlaySoundSample(int sample, bool repeat) {
	if (SoundSamples[sample].pBuf != nullptr) {
		SoundSamples[sample].pos = 0;
		SoundSamples[sample].playing = true;
		SoundSamples[sample].repeat = repeat;
	}
}

void StopSoundSample(int sample) {
	SoundSamples[sample].playing = false;
}

static int GetVol(int vol) {
	if (SoundExponentialVolume) {
//		static int expVol[] = { 0,  2,  4,  6,  9, 12, 15, 19, 24, 30, 38, 48, 60, 76,  95, 120 };
		static const int expVol[] = { 0, 11, 14, 17, 20, 24, 28, 33, 39, 46, 54, 63, 74, 87, 102, 120 };
		if (vol >= 0 && vol <= 15)
			return expVol[vol];
		else
			return 0;
	}
	else {
		return vol << VOLMAG;
	}
}

void LoadSoundUEF(FILE *SUEF) {
	// Sound block
	unsigned char Chan;
	int Data;
	int RegVal; // This will be filled in by the data processor
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
		BeebState76489.ToneVolume[4-Chan]=GetVol(15-Data);
		ActiveChannel[4 - Chan] = Data != 15;
	}
	RegVal=224|(fgetc(SUEF)&7);
	Sound_RegWrite(RegVal);
	Data=fgetc(SUEF);
	RegVal=240|(Data&15);
	Sound_RegWrite(RegVal);
	BeebState76489.ToneVolume[0]=GetVol(15-Data);
	ActiveChannel[0] = Data != 15;
	BeebState76489.LastToneFreqSet=fgetc(SUEF);
	GenIndex[0]=fget16(SUEF);
	GenIndex[1]=fget16(SUEF);
	GenIndex[2]=fget16(SUEF);
	GenIndex[3]=fget16(SUEF);
}

void SaveSoundUEF(FILE *SUEF)
{
	// Sound Block
	fput16(BeebState76489.ToneFreq[2],SUEF);
	fput16(BeebState76489.ToneFreq[1],SUEF);
	fput16(BeebState76489.ToneFreq[0],SUEF);
	fputc(RealVolumes[3],SUEF);
	fputc(RealVolumes[2],SUEF);
	fputc(RealVolumes[1],SUEF);
	unsigned char Noise = (unsigned char)(
		BeebState76489.Noise.Freq | (BeebState76489.Noise.FB << 2)
	);
	fputc(Noise,SUEF);
	fputc(RealVolumes[0],SUEF);
	fputc(BeebState76489.LastToneFreqSet,SUEF);
	fput16(GenIndex[0],SUEF);
	fput16(GenIndex[1],SUEF);
	fput16(GenIndex[2],SUEF);
	fput16(GenIndex[3],SUEF);
}
