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

#ifndef SOUNDSTREAM_HEADER
#define SOUNDSTREAM_HEADER

#include <windows.h>
#include <windowsx.h>
#include <mmsystem.h>
#include <vfw.h>
#include <dsound.h>
#include <xaudio2.h>
#include <iostream>
#include <exception>
#include <vector>
#include <list>

namespace SoundConfig
{
	extern enum Option
	{
		XAudio2,
		DirectSound
	}
	Selection;
}

struct SoundStreamer
{
	SoundStreamer();
	virtual ~SoundStreamer();

	virtual std::size_t BufferSize() const = 0;

	virtual void Play() = 0;
	virtual void Pause() = 0;

	virtual void Stream( void const *pSamples ) = 0;

	static void PlayAll();
	static void PauseAll();

private:
	static std::list<SoundStreamer*> m_streamers;
};

SoundStreamer *CreateSoundStreamer(int samplerate, int bits_per_sample, int channels);


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

	DirectSoundStreamer( std::size_t rate,
						 std::size_t bits_per_sample,
						 std::size_t channels );

	~DirectSoundStreamer()
	{
		if (m_pDirectSoundBuffer)
			m_pDirectSoundBuffer->Release();
		if (m_pDirectSound)
			m_pDirectSound->Release();
	}

	std::size_t BufferSize() const
	{
		return m_size;
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
	std::size_t m_rate;
	std::size_t m_size;
	std::size_t m_physical;
	std::size_t m_bytespersample;
	std::size_t m_channels;

	static std::size_t const m_count = 6;
};

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

	XAudio2Streamer( std::size_t rate,
					 std::size_t bits_per_sample,
					 std::size_t channels );

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
	typedef std::vector<Sample> Samples;
	Samples m_samples;
	IXAudio2* m_pXAudio2;
	IXAudio2MasteringVoice *m_pXAudio2MasteringVoice;
	IXAudio2SourceVoice *m_pXAudio2SourceVoice;
	std::size_t m_index;
	std::size_t m_rate;
	std::size_t m_size;
	std::size_t m_bytespersample;
	std::size_t m_channels;

	static std::size_t const m_count = 4;
};

#endif
