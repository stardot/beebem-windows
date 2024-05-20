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

#include <windowsx.h>

#include "XAudio2Streamer.h"
#include "Main.h"

XAudio2Streamer::XAudio2Streamer() :
	m_samples(0),
	m_pXAudio2(nullptr),
	m_pXAudio2MasteringVoice(nullptr),
	m_pXAudio2SourceVoice(nullptr),
	m_index(0),
	m_rate(0),
	m_size(0),
	m_bytespersample(0),
	m_channels(0)
{
}

XAudio2Streamer::~XAudio2Streamer()
{
	if (m_pXAudio2)
	{
		m_pXAudio2->Release();
	}
}

bool XAudio2Streamer::Init(std::size_t rate,
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

	// Allocate memory for buffers
	m_samples.resize(m_count * m_size * m_bytespersample * m_channels,
	                 bits_per_sample == 8 ? 128 : 0);

	// Create XAudio2
	if (FAILED(XAudio2Create(&m_pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR)))
	{
		return false;
	}

	// Create mastering voice
	if (FAILED(m_pXAudio2->CreateMasteringVoice(&m_pXAudio2MasteringVoice)))
	{
		return false;
	}

	// Create source voice
	WAVEFORMATEX wfx;
	memset(&wfx, 0, sizeof(wfx));

	wfx.wFormatTag      = WAVE_FORMAT_PCM;
	wfx.nChannels       = (WORD)m_channels;
	wfx.nSamplesPerSec  = (DWORD)m_rate;
	wfx.wBitsPerSample  = (WORD)(m_bytespersample * 8);
	wfx.nBlockAlign     = wfx.nChannels * wfx.wBitsPerSample / 8;
	wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

	if (FAILED(m_pXAudio2->CreateSourceVoice(&m_pXAudio2SourceVoice, &wfx, 0, XAUDIO2_DEFAULT_FREQ_RATIO)))
	{
		return false;
	}

	// Start source voice
	Play();

	return true;
}

std::size_t XAudio2Streamer::BufferSize() const
{
	return m_size;
}

void XAudio2Streamer::Play()
{
	m_pXAudio2SourceVoice->Start(0);
}

void XAudio2Streamer::Pause()
{
	m_pXAudio2SourceVoice->Stop(0);
}

void XAudio2Streamer::Stream(const void *pSamples)
{
	// Verify buffer availability
	XAUDIO2_VOICE_STATE xa2vs;
	m_pXAudio2SourceVoice->GetState(&xa2vs);

	if (xa2vs.BuffersQueued == m_count)
		return;

	// Copy samples to buffer
	Sample *pBuffer = &m_samples[m_index * m_size * m_bytespersample * m_channels];

	memcpy(pBuffer, pSamples, m_size * m_bytespersample * m_channels);

	// Submit buffer to voice
	XAUDIO2_BUFFER Buffer = {};
	Buffer.AudioBytes = (UINT32)(m_size * m_bytespersample * m_channels);
	Buffer.pAudioData = (BYTE *)pBuffer;

	if (FAILED(m_pXAudio2SourceVoice->SubmitSourceBuffer(&Buffer)))
		return;

	// Select next buffer
	if (++m_index == m_count)
	{
		m_index = 0;
	}
}
