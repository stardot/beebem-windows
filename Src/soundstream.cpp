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

#include "soundstream.h"
#include "main.h"

std::list<SoundStreamer*> SoundStreamer::m_streamers;

SoundStreamer::SoundStreamer()
{
	m_streamers.push_back(this);
}

SoundStreamer::~SoundStreamer()
{
	m_streamers.remove(this);
}

void SoundStreamer::PlayAll()
{
	std::list<SoundStreamer*>::iterator li;
	for (li = m_streamers.begin(); li != m_streamers.end(); li++)
		(*li)->Play();
}

void SoundStreamer::PauseAll()
{
	std::list<SoundStreamer*>::iterator li;
	for (li = m_streamers.begin(); li != m_streamers.end(); li++)
		(*li)->Pause();
}


DirectSoundStreamer::DirectSoundStreamer( std::size_t rate,
										  std::size_t bits_per_sample,
										  std::size_t channels )
	: m_pDirectSound(),
	  m_pDirectSoundBuffer(),
	  m_begin(),
	  m_rate( rate ),
	  m_size( rate * 20 / 1000 ),
	  m_physical( 0 ),
	  m_bytespersample( bits_per_sample / 8 ),
	  m_channels( channels )
{
	if (bits_per_sample != 8 && bits_per_sample != 16)
		throw Fail();

	if (channels != 1 && channels != 2)
		throw Fail();

	m_physical = m_count * m_size * m_bytespersample * m_channels;

	// Create DirectSound
	if( FAILED( DirectSoundCreate( 0, &m_pDirectSound, 0 ) ) )
		throw Fail();
	HRESULT hResult( m_pDirectSound->SetCooperativeLevel( GETHWND, DSSCL_PRIORITY ) );
	if( FAILED( hResult ) )
	{
		throw Fail();
	}

	// Create DirectSoundBuffer
	WAVEFORMATEX wfx = {};
	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nChannels = (WORD)m_channels;
	wfx.nSamplesPerSec = DWORD( m_rate );
	wfx.wBitsPerSample = (WORD)(m_bytespersample * 8);
	wfx.nBlockAlign = wfx.nChannels * wfx.wBitsPerSample / 8;
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

	DSBUFFERDESC dsbd = { sizeof( DSBUFFERDESC ) };
	dsbd.dwFlags = DSBCAPS_GETCURRENTPOSITION2;
	dsbd.dwBufferBytes = DWORD( m_physical );
	dsbd.lpwfxFormat = &wfx;
	dsbd.guid3DAlgorithm = DS3DALG_DEFAULT;
	if( FAILED( m_pDirectSound->CreateSoundBuffer( &dsbd, &m_pDirectSoundBuffer, 0 ) ) )
		throw Fail();

	// Clear buffer
	void *p;
	DWORD size;
	if( FAILED( m_pDirectSoundBuffer->Lock( 0, 0, &p, &size, 0, 0, DSBLOCK_ENTIREBUFFER ) ) )
		throw Fail();
	using std::memset;
	memset( p, bits_per_sample == 8 ? 128 : 0, std::size_t( size ) );
	if( FAILED( m_pDirectSoundBuffer->Unlock( p, size, 0, 0 ) ) )
		throw Fail();

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
	std::size_t end( begin + m_size * m_bytespersample * m_channels );
	if( play + m_physical < end )
		return;
	begin %= m_physical;

	// Copy samples to buffer
	void *ps[2];
	DWORD sizes[2];
	HRESULT hResult( m_pDirectSoundBuffer->Lock( DWORD( begin ),
												 DWORD( m_size * m_bytespersample * m_channels ),
												 &ps[0], &sizes[0], &ps[1], &sizes[1], 0 ) );
	if( FAILED( hResult ) )
	{
		if( hResult != DSERR_BUFFERLOST )
			return;
		m_pDirectSoundBuffer->Restore();
		if( FAILED( m_pDirectSoundBuffer->Lock( DWORD( begin ),
												DWORD( m_size * m_bytespersample * m_channels ),
												&ps[0], &sizes[0], &ps[1], &sizes[1], 0 ) ) )
			return;
	}

	using std::memcpy;
	memcpy( ps[0], pSamples, std::size_t( sizes[0] ) );
	if( ps[1] )
		memcpy( ps[1], static_cast< char const * >( pSamples ) + sizes[0], std::size_t( sizes[1] ) );
	m_pDirectSoundBuffer->Unlock( ps[0], sizes[0], ps[1], sizes[1] );

	// Select next buffer
	m_begin = end % m_physical;
}

XAudio2Streamer::XAudio2Streamer( std::size_t rate,
								  std::size_t bits_per_sample,
								  std::size_t channels )
	: m_samples(),
	  m_pXAudio2(),
	  m_pXAudio2MasteringVoice(),
	  m_pXAudio2SourceVoice(),
	  m_index(),
	  m_rate( rate ),
	  m_size( rate * 20 / 1000 ),
	  m_bytespersample( bits_per_sample / 8 ),
	  m_channels( channels )
{
	if (bits_per_sample != 8 && bits_per_sample != 16)
		throw Fail();

	if (channels != 1 && channels != 2)
		throw Fail();

	// Allocate memory for buffers
	m_samples.resize( m_count * m_size * m_bytespersample * m_channels,
					  bits_per_sample == 8 ? 128 : 0 );

	// Create XAudio2
	if( FAILED( XAudio2Create( &m_pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR ) ) )
		throw Fail();

	// Create mastering voice
	if( FAILED( m_pXAudio2->CreateMasteringVoice( &m_pXAudio2MasteringVoice ) ) )
		throw Fail();

	// Create source voice
	WAVEFORMATEX wfx = {};
	wfx.wFormatTag = WAVE_FORMAT_PCM;
	wfx.nChannels = (WORD)m_channels;
	wfx.nSamplesPerSec = DWORD( m_rate );
	wfx.wBitsPerSample = (WORD)(m_bytespersample * 8);
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
	Sample *pBuffer = &m_samples[ m_index * m_size * m_bytespersample * m_channels ];
	using std::memcpy;
	memcpy( pBuffer, pSamples, m_size * m_bytespersample * m_channels );

	// Submit buffer to voice
	XAUDIO2_BUFFER xa2b = {};
	xa2b.AudioBytes = UINT32( m_size * m_bytespersample * m_channels );
	xa2b.pAudioData = (const BYTE *)pBuffer;
	if( FAILED( m_pXAudio2SourceVoice->SubmitSourceBuffer( &xa2b ) ) )
		return;

	// Select next buffer
	m_index = ( m_index + 1 ) % m_count;
}

SoundStreamer *CreateSoundStreamer(int samplerate, int bits_per_sample, int channels)
{
	SoundStreamer *pSoundStreamer = 0;

	if( SoundConfig::Selection == SoundConfig::XAudio2 )
	{
		try
		{
			pSoundStreamer = new XAudio2Streamer( samplerate, bits_per_sample, channels );
			return pSoundStreamer;
		}
		catch( ... )
		{
		}
		SoundConfig::Selection = SoundConfig::DirectSound;
	}
	try
	{
		pSoundStreamer = new DirectSoundStreamer( samplerate, bits_per_sample, channels );
		return pSoundStreamer;
	}
	catch( ... )
	{
	}

	MessageBox( GETHWND, "Attempt to start sound system failed", "BeebEm", MB_OK | MB_ICONERROR );
	return NULL;
}
