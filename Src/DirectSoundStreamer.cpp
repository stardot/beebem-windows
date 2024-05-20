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

#include "DirectSoundStreamer.h"
#include "BeebWin.h"
#include "Main.h"

DirectSoundStreamer::DirectSoundStreamer() :
	m_pDirectSound(nullptr),
	m_pDirectSoundBuffer(nullptr),
	m_begin(0),
	m_physical(0),
	m_bytespersample(0),
	m_channels(0)
{
}

DirectSoundStreamer::~DirectSoundStreamer()
{
	if (m_pDirectSoundBuffer != nullptr)
	{
		m_pDirectSoundBuffer->Release();
	}

	if (m_pDirectSound != nullptr)
	{
		m_pDirectSound->Release();
	}
}

bool DirectSoundStreamer::Init(std::size_t rate,
                               std::size_t bits_per_sample,
                               std::size_t channels)
{
	if (bits_per_sample != 8 && bits_per_sample != 16)
	{
		return false;
	}

	if (channels != 1 && channels != 2)
	{
		return false;
	}

	m_rate = rate;
	m_size = rate * 20 / 1000;
	m_bytespersample = bits_per_sample / 8;
	m_channels = channels;
	m_physical = m_count * m_size * m_bytespersample * m_channels;

	// Create DirectSound
	if (FAILED(DirectSoundCreate(0, &m_pDirectSound, 0)))
	{
		return false;
	}

	HRESULT hResult = m_pDirectSound->SetCooperativeLevel(mainWin->GethWnd(), DSSCL_PRIORITY);

	if (FAILED(hResult))
	{
		return false;
	}

	// Create DirectSoundBuffer
	WAVEFORMATEX wfx;
	memset(&wfx, 0, sizeof(wfx));

	wfx.wFormatTag      = WAVE_FORMAT_PCM;
	wfx.nChannels       = (WORD)m_channels;
	wfx.nSamplesPerSec  = (DWORD)m_rate;
	wfx.wBitsPerSample  = (WORD)(m_bytespersample * 8);
	wfx.nBlockAlign     = wfx.nChannels * wfx.wBitsPerSample / 8;
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

	DSBUFFERDESC BufferDesc;
	memset(&BufferDesc, 0, sizeof(BufferDesc));

	BufferDesc.dwSize          = sizeof(DSBUFFERDESC);
	BufferDesc.dwFlags         = DSBCAPS_GETCURRENTPOSITION2;
	BufferDesc.dwBufferBytes   = (DWORD)m_physical;
	BufferDesc.lpwfxFormat     = &wfx;
	BufferDesc.guid3DAlgorithm = DS3DALG_DEFAULT;

	if (FAILED(m_pDirectSound->CreateSoundBuffer(&BufferDesc, &m_pDirectSoundBuffer, 0)))
	{
		return false;
	}

	// Clear buffer
	void *pAudioBuffer;
	DWORD size;

	if (FAILED(m_pDirectSoundBuffer->Lock(0, 0, &pAudioBuffer, &size, 0, 0, DSBLOCK_ENTIREBUFFER)))
	{
		return false;
	}

	memset(pAudioBuffer, bits_per_sample == 8 ? 128 : 0, size);

	if (FAILED(m_pDirectSoundBuffer->Unlock(pAudioBuffer, size, 0, 0)))
	{
		return false;
	}

	// Start source voice
	Play();

	return true;
}

std::size_t DirectSoundStreamer::BufferSize() const
{
	return m_size;
}

void DirectSoundStreamer::Play()
{
	m_pDirectSoundBuffer->Play(0, 0, DSBPLAY_LOOPING);
}

void DirectSoundStreamer::Pause()
{
	m_pDirectSoundBuffer->Stop();
}

void DirectSoundStreamer::Stream(const void *pSamples)
{
	// Verify buffer availability
	DWORD play, write;

	if (FAILED(m_pDirectSoundBuffer->GetCurrentPosition(&play, &write)))
		return;

	if (write < play)
		write += (DWORD)m_physical;

	std::size_t begin = m_begin;

	if (begin < play)
		begin += m_physical;

	if (begin < write)
		begin = write;

	std::size_t end = begin + m_size * m_bytespersample * m_channels;

	if (play + m_physical < end)
		return;

	begin %= m_physical;

	// Copy samples to buffer
	void *ps[2];
	DWORD sizes[2];

	HRESULT hResult = m_pDirectSoundBuffer->Lock(
		(DWORD)begin,
		(DWORD)(m_size * m_bytespersample * m_channels),
		&ps[0],
		&sizes[0],
		&ps[1],
		&sizes[1],
		0
	);

	if (FAILED(hResult))
	{
		if (hResult != DSERR_BUFFERLOST)
			return;

		m_pDirectSoundBuffer->Restore();

		hResult = m_pDirectSoundBuffer->Lock(
			(DWORD)begin,
			(DWORD)(m_size * m_bytespersample * m_channels),
			&ps[0],
			&sizes[0],
			&ps[1],
			&sizes[1],
			0
		);

		if (FAILED(hResult))
			return;
	}

	memcpy(ps[0], pSamples,  sizes[0]);

	if (ps[1])
		memcpy( ps[1], static_cast< char const * >( pSamples ) + sizes[0], std::size_t( sizes[1] ) );

	m_pDirectSoundBuffer->Unlock( ps[0], sizes[0], ps[1], sizes[1] );

	// Select next buffer
	m_begin = end % m_physical;
}
