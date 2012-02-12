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

#include "beebsound.h"

#include <iostream>
#include <fstream>
#include <windows.h>
#include <process.h>
#include <math.h>

#include <errno.h>
#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include "main.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "6502core.h"
#include "port.h"
#include "beebmem.h"
#include "beebwin.h"
#include "uefstate.h"
#include "avi.h"
#include "main.h"
#ifdef SPEECH_ENABLED
#include "speech.h"
#endif

//#include <atlcomcli.h> // ATL not included in VS Express edition
#include <dsound.h>
#include <xaudio2.h>
#include <exception>
#include <vector>

extern AVIWriter *aviWriter;

//  #define DEBUGSOUNDTOFILE

#define PREFSAMPLERATE 44100
#define MAXBUFSIZE 32768

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
#define NUM_SOUND_SAMPLES (sizeof(SoundSamples)/sizeof(SoundSample))
static bool SoundSamplesLoaded = false;

SoundConfig::Option SoundConfig::Selection;
int UsePrimaryBuffer=0;
int SoundEnabled = 1;
int RelaySoundEnabled = 0;
int DiscDriveSoundEnabled = 0;
int SoundChipEnabled = 1;
int SoundSampleRate = PREFSAMPLERATE;
int SoundVolume = 3;
int SoundAutoTriggerTime;
int SoundBufferSize;
double CSC[4]={0,0,0,0},CSA[4]={0,0,0,0}; // ChangeSamps Adjusts
char SoundExponentialVolume = 1;

/* Number of places to shift the volume */
#define VOLMAG 3

int Speech[4];

FILE *sndlog=NULL;


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

static int ActiveChannel[4]={FALSE,FALSE,FALSE,FALSE}; /* Those channels with non-0 voolume */
// Set it to an array for more accurate sound generation
static unsigned int samplerate;
static double OurTime=0.0; /* Time in sample periods */

int SoundTrigger; /* Time to trigger a sound event */

static unsigned int GenIndex[4]; /* Used by the voice generators */
static int GenState[4];
static int bufptr=0;
int SoundDefault;
double SoundTuning=0.0; // Tunning offset

void PlayUpTil(double DestTime);
int GetVol(int vol);
BOOL bReRead=FALSE;
volatile BOOL bDoSound=TRUE;
int WriteOffset=0;
LARGE_INTEGER PFreq,LastPCount,CurrentPCount;
double CycleRatio;
struct AudioType TapeAudio; // Tape audio decoder stuff
bool TapeSoundEnabled;
int PartSamples=1;
bool Playing=0;

struct SoundStreamer
{
	virtual ~SoundStreamer()
	{
	}

	virtual std::size_t BufferSize() const = 0;

	virtual void Play() = 0;
	virtual void Pause() = 0;

	virtual void Stream( void const *pSamples ) = 0;
};

class DirectSoundStreamer : public SoundStreamer
{
public:
	struct Fail : std::exception
	{
		char const *what() const throw()
		{
			return "DirectSoundStreamer::Fail";
		}
	};

	struct PrimaryUnsupported : Fail
	{
		char const *what() const throw()
		{
			return "DirectSoundStreamer::PrimaryUnsupported";
		}
	};

	DirectSoundStreamer( std::size_t rate, bool primary );

	~DirectSoundStreamer()
	{
		if (m_pDirectSoundBuffer)
			m_pDirectSoundBuffer->Release();
		if (m_pDirectSound)
			m_pDirectSound->Release();
	}

	std::size_t BufferSize() const
	{
		return m_stream;
	}

	void Play()
	{
		m_pDirectSoundBuffer->Play( 0, 0, DSBPLAY_LOOPING );
	}

	void Pause()
	{
		m_pDirectSoundBuffer->Stop();
	}

	void Stream( void const *pSamples );

private:
	IDirectSound* m_pDirectSound;
	IDirectSoundBuffer* m_pDirectSoundBuffer;
	std::size_t m_begin;
	std::size_t m_physical;
	std::size_t m_rate;
	std::size_t m_stream;
	std::size_t m_virtual;

	static std::size_t const m_count = 6;
};

DirectSoundStreamer::DirectSoundStreamer( std::size_t rate, bool primary ) : m_pDirectSound(), m_pDirectSoundBuffer(), m_begin(), m_physical( rate * 100 / 1000 ), m_rate( rate ), m_stream( rate * 20 / 1000 ), m_virtual( m_count * m_stream )
{
	// Create DirectSound
	if( FAILED( DirectSoundCreate( 0, &m_pDirectSound, 0 ) ) )
		throw Fail();
	HRESULT hResult( m_pDirectSound->SetCooperativeLevel( GETHWND, primary ? DSSCL_WRITEPRIMARY : DSSCL_PRIORITY ) );
	if( FAILED( hResult ) )
	{
		if( primary && hResult == DSERR_UNSUPPORTED )
			throw PrimaryUnsupported();
		throw Fail();
	}

	// Create DirectSoundBuffer
	WAVEFORMATEX wfx = {};
	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nChannels = 1;
	wfx.nSamplesPerSec = DWORD( m_rate );
	wfx.wBitsPerSample = 8;
	wfx.nBlockAlign = wfx.nChannels * wfx.wBitsPerSample / 8;
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

	DSBUFFERDESC dsbd = { sizeof( DSBUFFERDESC ) };
	dsbd.dwFlags = DSBCAPS_GETCURRENTPOSITION2;
	if( primary )
		dsbd.dwFlags |= DSBCAPS_PRIMARYBUFFER;
	else
	{
		if( m_physical < m_virtual )
			m_physical = m_virtual;
		dsbd.dwBufferBytes = DWORD( m_physical );
		dsbd.lpwfxFormat = &wfx;
	}
	dsbd.guid3DAlgorithm = DS3DALG_DEFAULT;
	if( FAILED( m_pDirectSound->CreateSoundBuffer( &dsbd, &m_pDirectSoundBuffer, 0 ) ) )
		throw Fail();

	if( primary )
	{
		// Setup primary buffer
		if( FAILED( m_pDirectSoundBuffer->SetFormat( &wfx ) ) )
			throw Fail();
		DSBCAPS dsbc = { sizeof( dsbc ) };
		if( FAILED( m_pDirectSoundBuffer->GetCaps( &dsbc ) ) )
			throw Fail();
		m_physical = std::size_t( dsbc.dwBufferBytes );
	}
	else
	{
		// Clear buffer
		void *p;
		DWORD size;
		if( FAILED( m_pDirectSoundBuffer->Lock( 0, 0, &p, &size, 0, 0, DSBLOCK_ENTIREBUFFER ) ) )
			throw Fail();
		using std::memset;
		memset( p, 128, std::size_t( size ) );
		if( FAILED( m_pDirectSoundBuffer->Unlock( p, size, 0, 0 ) ) )
			throw Fail();
	}

	// Start source voice
	Play();
}

void DirectSoundStreamer::Stream( void const *pSamples )
{
	// Verify buffer availability
	DWORD play, write;
	if( FAILED( m_pDirectSoundBuffer->GetCurrentPosition( &play, &write ) ) )
		return;
	if( write < play )
		write += DWORD( m_physical );
	std::size_t begin( m_begin );
	if( begin < play )
		begin += m_physical;
	if( begin < write )
		begin = std::size_t( write );
	std::size_t end( begin + m_stream );
	if( play + m_virtual < end )
		return;
	begin %= m_physical;

	// Copy samples to buffer
	void *ps[ 2 ];
	DWORD sizes[ 2 ];
	HRESULT hResult( m_pDirectSoundBuffer->Lock( DWORD( begin ), DWORD( m_stream ), &ps[ 0 ], &sizes[ 0 ], &ps[ 1 ], &sizes[ 1 ], 0 ) );
	if( FAILED( hResult ) )
	{
		if( hResult != DSERR_BUFFERLOST )
			return;
		m_pDirectSoundBuffer->Restore();
		if( FAILED( m_pDirectSoundBuffer->Lock( DWORD( begin ), DWORD( m_stream ), &ps[ 0 ], &sizes[ 0 ], &ps[ 1 ], &sizes[ 1 ], 0 ) ) )
			return;
	}
	using std::memcpy;
	memcpy( ps[ 0 ], pSamples, std::size_t( sizes[ 0 ] ) );
	if( ps[ 1 ] )
		memcpy( ps[ 1 ], static_cast< char const * >( pSamples ) + sizes[ 0 ], std::size_t( sizes[ 1 ] ) );
	m_pDirectSoundBuffer->Unlock( ps[ 0 ], sizes[ 0 ], ps[ 1 ], sizes[ 1 ] );

	// Select next buffer
	m_begin = end % m_physical;
}

class XAudio2Streamer : public SoundStreamer
{
public:
	struct Fail : std::exception
	{
		char const *what() const throw()
		{
			return "XAudio2Streamer::Fail";
		}
	};

	XAudio2Streamer( std::size_t rate );

	~XAudio2Streamer()
	{
		if (m_pXAudio2)
			m_pXAudio2->Release();
	}

	std::size_t BufferSize() const
	{
		return m_size;
	}

	void Play()
	{
		m_pXAudio2SourceVoice->Start( 0 );
	}

	void Pause()
	{
		m_pXAudio2SourceVoice->Stop( 0 );
	}

	void Stream( void const *pSamples );

private:
	typedef unsigned char Sample;
	typedef std::vector< Sample > Samples;
	Samples m_samples;
	IXAudio2* m_pXAudio2;
	IXAudio2MasteringVoice *m_pXAudio2MasteringVoice;
	IXAudio2SourceVoice *m_pXAudio2SourceVoice;
	std::size_t m_index;
	std::size_t m_rate;
	std::size_t m_size;

	static std::size_t const m_count = 4;
};

XAudio2Streamer::XAudio2Streamer( std::size_t rate ) : m_samples(), m_pXAudio2(), m_pXAudio2MasteringVoice(), m_pXAudio2SourceVoice(), m_index(), m_rate( rate ), m_size( rate * 20 / 1000 )
{
	// Allocate memory for buffers
	m_samples.resize( m_count * m_size, 128 );

	// Create XAudio2
	if( FAILED( XAudio2Create( &m_pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR ) ) )
		throw Fail();

	// Create mastering voice
	if( FAILED( m_pXAudio2->CreateMasteringVoice( &m_pXAudio2MasteringVoice ) ) )
		throw Fail();

	// Create source voice
	WAVEFORMATEX wfx = {};
	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nChannels = 1;
	wfx.nSamplesPerSec = DWORD( m_rate );
	wfx.wBitsPerSample = 8;
	wfx.nBlockAlign = wfx.nChannels * wfx.wBitsPerSample / 8;
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
	if( FAILED( m_pXAudio2->CreateSourceVoice( &m_pXAudio2SourceVoice, &wfx, 0, XAUDIO2_DEFAULT_FREQ_RATIO ) ) )
		throw Fail();

	// Start source voice
	Play();
}

void XAudio2Streamer::Stream( void const *pSamples )
{
	// Verify buffer availability
	XAUDIO2_VOICE_STATE xa2vs;
	m_pXAudio2SourceVoice->GetState( &xa2vs );
	if( xa2vs.BuffersQueued == m_count )
		return;

	// Copy samples to buffer
	Sample *pBuffer( &m_samples[ m_index * m_size ] );
	using std::memcpy;
	memcpy( pBuffer, pSamples, m_size );

	// Submit buffer to voice
	XAUDIO2_BUFFER xa2b = {};
	xa2b.AudioBytes = UINT32( m_size );
	xa2b.pAudioData = pBuffer;
	if( FAILED( m_pXAudio2SourceVoice->SubmitSourceBuffer( &xa2b ) ) )
		return;

	// Select next buffer
	m_index = ( m_index + 1 ) % m_count;
}

namespace
{
	SoundStreamer *pSoundStreamer = 0;
}

/****************************************************************************/
/* Writes sound data to a sound buffer */
HRESULT WriteToSoundBuffer(PBYTE lpbSoundData)
{
	if( pSoundStreamer )
		pSoundStreamer->Stream( lpbSoundData );
	if( aviWriter )
	{
		HRESULT hResult( aviWriter->WriteSound( lpbSoundData, SoundBufferSize ) );
		if( FAILED( hResult ) && hResult != E_UNEXPECTED )
		{
			MessageBox( GETHWND, "Failed to write sound to AVI file", "BeebEm", MB_OK | MB_ICONERROR );
			delete aviWriter;
			aviWriter = 0;
		}
	}
	return S_OK;
}


/****************************************************************************/
/* DestTime is in samples */
void PlayUpTil(double DestTime) {
	int tmptotal,channel,bufinc,tapetotal;
	char Extras;
	int SpeechPtr = 0;
	int i;

#ifdef SPEECH_ENABLED
	if (MachineType != 3 && SpeechEnabled)
	{
		SpeechPtr = 0;
		memset(SpeechBuf, 128, sizeof(SpeechBuf));
		int len = (int) (DestTime - OurTime + 1);
		if (len > MAXBUFSIZE)
			len = MAXBUFSIZE;
		tms5220_update(SpeechBuf, len);
	}
#endif

	while (DestTime>OurTime) {

		for(bufinc=0;(bufptr<SoundBufferSize) && ((OurTime+bufinc)<DestTime);bufptr++,bufinc++) {
			int tt;
			tmptotal=0;
			Extras=4;

			if (SoundChipEnabled) {
				// Begin of for loop
				for(channel=1;channel<=3;channel++) {
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
				} /* Channel loop */

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
							}
							break;

						case 1: /* Med */
							if (GenIndex[0]>=(samplerate/5000)) {
								GenIndex[0]=0;
								GenState[0]=rand() & 1;
							}
							break;

						case 2: /* High */
							if (GenIndex[0]>=(samplerate/2500)) {
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
								if (GenIndex[0]>=(samplerate/125)) {
									GenIndex[0]=0;
									GenState[0]=0;
								}
							} else {
								if (GenIndex[0]>=(samplerate/1250)) {
									GenIndex[0]=0;
									GenState[0]=1;
								}
							}
							break;

						case 1: /* Med */
							if (GenState[0]) {
								if (GenIndex[0]>=(samplerate/250)) {
									GenIndex[0]=0;
									GenState[0]=0;
								}
							} else {
								if (GenIndex[0]>=(samplerate/2500)) {
									GenIndex[0]=0;
									GenState[0]=1;
								}
							}
							break;

						case 0: /* High */
							if (GenState[0]) {
								if (GenIndex[0]>=(samplerate/500)) {
									GenIndex[0]=0;
									GenState[0]=0;
								}
							} else {
								if (GenIndex[0]>=(samplerate/5000)) {
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

#ifdef SPEECH_ENABLED
			// Mix in speech sound
			if (SpeechEnabled) if (MachineType != 3) tmptotal += (SpeechBuf[SpeechPtr++]-128)*10;
#endif

			// Mix in sound samples here
			for (i = 0; i < NUM_SOUND_SAMPLES; ++i) {
				if (SoundSamples[i].playing) {
					tmptotal+=(SoundSamples[i].pBuf[SoundSamples[i].pos]-128)*10;
					Extras++;
					SoundSamples[i].pos += (44100 / samplerate);
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
			if (Extras)
				tmptotal/=Extras;
			else
				tmptotal=0;

			SoundBuf[bufptr] = (tmptotal/SoundVolume)+128;
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
				MessageBox(GETHWND,"Failed to open audio.dbg",WindowTitle,MB_OK|MB_ICONERROR);
				exit(1);
			}
#else
			//if (sndlog!=NULL)
				//fprintf(sndlog,"Sound write at %lf Samples\n",DestTime);
				//fwrite(SoundBuf,1,SoundBufferSize,sndlog);
			HRESULT hr;
			hr = WriteToSoundBuffer(SoundBuf);
#endif
			// buffer swapping no longer needed
			bufptr=0;
		}

		OurTime+=bufinc;
	} /* While time */ 
}; /* PlayUpTil */

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
  if (BeebCycles<LastBeebCycle) {
    /* Wrap around in beebs time */
    tmp=((double)CycleCountWrap-(double)LastBeebCycle)+(double)BeebCycles;
  } else {
    tmp=(double)BeebCycles-(double)LastBeebCycle;
  };
  tmp/=(mainWin->m_RealTimeTarget)?mainWin->m_RealTimeTarget:1;
/*fprintf(stderr,"Convert tmp=%f\n",tmp); */
  LastBeebCycle=BeebCycles;

  tmp*=(samplerate);
  tmp/=2000000.0; /* Few - glad thats a double! */

  LastOurTime+=tmp;
  return LastOurTime;
}; /* CyclesToSamples */

/****************************************************************************/
static void InitAudioDev(int sampleratein) {

	delete pSoundStreamer;
	samplerate = sampleratein;
	if( SoundConfig::Selection == SoundConfig::XAudio2 )
	{
		try
		{
			pSoundStreamer = new XAudio2Streamer( samplerate );
			return;
		}
		catch( ... )
		{
		}
		SoundConfig::Selection = SoundConfig::DirectSound;
	}
	if( UsePrimaryBuffer )
	{
		try
		{
			pSoundStreamer = new DirectSoundStreamer( samplerate, true );
			return;
		}
		catch( DirectSoundStreamer::PrimaryUnsupported const & )
		{
			MessageBox(GETHWND, "Use of primary DirectSound buffer unsupported on this system. Using secondary DirectSound buffer instead", WindowTitle, MB_OK | MB_ICONERROR);
			UsePrimaryBuffer = 0;
			mainWin->SetPBuff();
		}
		catch( ... )
		{
		}
	}
	if( ! UsePrimaryBuffer )
	{
		try
		{
			pSoundStreamer = new DirectSoundStreamer( samplerate, false );
			return;
		}
		catch( ... )
		{
		}
	}
	SoundEnabled = 0;
	MessageBox( GETHWND, "Attempt to start sound system failed", "BeebEm", MB_OK | MB_ICONERROR );

}// InitAudioDev

void LoadSoundSamples(void) {
	FILE *fd;
	char FileName[256];
	int i;

	if (!SoundSamplesLoaded) {
		for (i = 0; i < NUM_SOUND_SAMPLES; ++i) {
			strcpy(FileName, mainWin->GetAppPath());
			strcat(FileName, SoundSamples[i].pFilename);
			fd = fopen(FileName, "rb");
			if (fd != NULL) {
				fseek(fd, 0, SEEK_END);
				SoundSamples[i].len = ftell(fd);
				SoundSamples[i].pBuf = (unsigned char *)malloc(SoundSamples[i].len);
				fseek(fd, 0, SEEK_SET);
				fread(SoundSamples[i].pBuf, 1, SoundSamples[i].len, fd);
				fclose(fd);
			}
			else {
				char errstr[200];
				sprintf(errstr,"Could not open sound sample file:\n  %s", FileName);
				MessageBox(GETHWND,errstr,WindowTitle,MB_OK|MB_ICONERROR);
			}
		}
		SoundSamplesLoaded = true;
	}
}

/****************************************************************************/
/* The 'freqval' variable is the value as sene by the 76489                 */
static void SetFreq(int Channel, int freqval) {
  unsigned int freq;
  int ChangeSamps; /* Number of samples after which to change */
  //fprintf(sndlog,"Channel %d - Value %d\n",Channel,freqval);
  double t;

  if (freqval==0) freqval=1;
  if (freqval<5) Speech[Channel]=1; else Speech[Channel]=0;
  freq=4000000/(32*freqval);

  t=( (( (double)samplerate/(double)freq)/2.0) +SoundTuning);
  ChangeSamps=(int)t;
  CSA[Channel]=(double)(t-ChangeSamps);
  CSC[Channel]=CSA[Channel];  // we look ahead, so should already include the fraction on the first update
  if (Channel==1) {
    CSC[0]=CSC[1];
  }

  BeebState76489.ChangeSamps[Channel]=ChangeSamps;
};

/****************************************************************************/
static void SoundTrigger_Real(void) {
  double nowsamps;
  nowsamps=CyclesToSamples(TotalCycles);
  PlayUpTil(nowsamps);
  SoundTrigger=TotalCycles+SoundAutoTriggerTime;
}; /* SoundTrigger_Real */

void Sound_Trigger(int NCycles) {
	if (SoundTrigger<=TotalCycles) SoundTrigger_Real(); 
	// fprintf(sndlog,"SoundTrigger_Real was called from Sound_Trigger\n"); }
}

void SoundChipReset(void) {
  BeebState76489.LastToneFreqSet=0;
  BeebState76489.ToneVolume[0]=0;
  BeebState76489.ToneVolume[1]=BeebState76489.ToneVolume[2]=BeebState76489.ToneVolume[3]=GetVol(15);
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
  ClearTrigger(SoundTrigger);
  LastBeebCycle=TotalCycles;
  LastOurTime=(double)LastBeebCycle * (double)SoundSampleRate / 2000000.0;
  OurTime=LastOurTime;
  bufptr=0;
  InitAudioDev(SoundSampleRate);
  if (SoundSampleRate == 44100) SoundAutoTriggerTime = 5000; 
  if (SoundSampleRate == 22050) SoundAutoTriggerTime = 10000; 
  if (SoundSampleRate == 11025) SoundAutoTriggerTime = 20000; 
  SoundBufferSize=pSoundStreamer?pSoundStreamer->BufferSize():SoundSampleRate/50;
  LoadSoundSamples();
  bReRead=TRUE;
  SoundTrigger=TotalCycles+SoundAutoTriggerTime;
}; /* SoundInit */

void SwitchOnSound(void) {
  SetFreq(3,1000);
  ActiveChannel[3]=TRUE;
  BeebState76489.ToneVolume[3]=GetVol(15);
}

void SetSound(char State) {
	if( pSoundStreamer )
	{
		switch( State )
		{
		case MUTED:
			pSoundStreamer->Pause();
			break;
		case UNMUTED:
			pSoundStreamer->Play();
			break;
		}
	}
}


/****************************************************************************/
/* Called to disable sound output                                           */
void SoundReset(void) {

	delete pSoundStreamer;
	pSoundStreamer = 0;

	ClearTrigger(SoundTrigger);

}//	SoundReset

/****************************************************************************/
/* Called in sysvia.cc when a write is made to the 76489 sound chip         */
void Sound_RegWrite(int value) {
  int trigger = 0;
  unsigned char VolChange;

  if (!SoundEnabled)
    return;
  VolChange=4;

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
        BeebState76489.ToneFreq[2]=(BeebState76489.ToneFreq[2] & 0x3f0) | (value & 0xf);
        SetFreq(3,BeebState76489.ToneFreq[2]);
        BeebState76489.LastToneFreqSet=2;
	//	trigger = 1;
        break;

      case 1: /* Tone 3 vol */
        RealVolumes[3]=value&15;
		if ((BeebState76489.ToneVolume[3]==0) && ((value &15)!=15)) ActiveChannel[3]=TRUE;
        if ((BeebState76489.ToneVolume[3]!=0) && ((value &15)==15)) ActiveChannel[3]=FALSE;
        BeebState76489.ToneVolume[3]=GetVol(15-(value & 15));
        BeebState76489.LastToneFreqSet=2;
		trigger = 1;
		VolChange=3;
        break;

      case 2: /* Tone 2 freq */
        BeebState76489.ToneFreq[1]=(BeebState76489.ToneFreq[1] & 0x3f0) | (value & 0xf);
        BeebState76489.LastToneFreqSet=1;
        SetFreq(2,BeebState76489.ToneFreq[1]);
	//	trigger = 1;
        break;

      case 3: /* Tone 2 vol */
        RealVolumes[2]=value&15;
        if ((BeebState76489.ToneVolume[2]==0) && ((value &15)!=15)) ActiveChannel[2]=TRUE;
        if ((BeebState76489.ToneVolume[2]!=0) && ((value &15)==15)) ActiveChannel[2]=FALSE;
        BeebState76489.ToneVolume[2]=GetVol(15-(value & 15));
        BeebState76489.LastToneFreqSet=1;
		trigger = 1;
		VolChange=2;
        break;

      case 4: /* Tone 1 freq (Possibly also noise!) */
        BeebState76489.ToneFreq[0]=(BeebState76489.ToneFreq[0] & 0x3f0) | (value & 0xf);
        BeebState76489.LastToneFreqSet=0;
        SetFreq(1,BeebState76489.ToneFreq[0]);
	//	trigger = 1;
        break;

      case 5: /* Tone 1 vol */
        RealVolumes[1]=value&15;
        if ((BeebState76489.ToneVolume[1]==0) && ((value &15)!=15)) ActiveChannel[1]=TRUE;
        if ((BeebState76489.ToneVolume[1]!=0) && ((value &15)==15)) ActiveChannel[1]=FALSE;
        BeebState76489.ToneVolume[1]=GetVol(15-(value & 15));
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
        BeebState76489.ToneVolume[0]=GetVol(15-(value & 15));
        trigger = 1;
		VolChange=0;
        break;
    };
   //if (VolChange<4) fprintf(sndlog,"Channel %d - Volume %d at %lu Cycles\n",VolChange,value &15,SoundCycles);
  };
  if (trigger)
    SoundTrigger_Real();
}; /* Sound_RegWrite */


void DumpSound(void) {
	
}

void ClickRelay(unsigned char RState) {
	if (RelaySoundEnabled) {
		if (RState) {
			SoundSamples[SAMPLE_RELAY_ON].pos = 0;
			SoundSamples[SAMPLE_RELAY_ON].playing = true;
		}
		else {
			SoundSamples[SAMPLE_RELAY_OFF].pos = 0;
			SoundSamples[SAMPLE_RELAY_OFF].playing = true;
		}
	}
}

void PlaySoundSample(int sample, bool repeat) {
	SoundSamples[sample].pos = 0;
	SoundSamples[sample].playing = true;
	SoundSamples[sample].repeat = repeat;
}

void StopSoundSample(int sample) {
	SoundSamples[sample].playing = false;
}

int GetVol(int vol) {
	if (SoundExponentialVolume)	{
//		static int expVol[] = { 0,  2,  4,  6,  9, 12, 15, 19, 24, 30, 38, 48, 60, 76,  95, 120 };
		static int expVol[] = { 0, 11, 14, 17, 20, 24, 28, 33, 39, 46, 54, 63, 74, 87, 102, 120 };
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
		if (Data!=15) ActiveChannel[4-Chan]=TRUE; else ActiveChannel[4-Chan]=FALSE;
	}
	RegVal=224|(fgetc(SUEF)&7);
	Sound_RegWrite(RegVal);
	Data=fgetc(SUEF);
	RegVal=240|(Data&15);
	Sound_RegWrite(RegVal);
	BeebState76489.ToneVolume[0]=GetVol(15-Data);
	if (Data!=15) ActiveChannel[0]=TRUE; else ActiveChannel[0]=FALSE;
	BeebState76489.LastToneFreqSet=fgetc(SUEF);
	GenIndex[0]=fget16(SUEF);
	GenIndex[1]=fget16(SUEF);
	GenIndex[2]=fget16(SUEF);
	GenIndex[3]=fget16(SUEF);
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
