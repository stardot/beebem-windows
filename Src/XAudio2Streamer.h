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

#ifndef XAUDIO2_SOUND_STREAMER_HEADER
#define XAUDIO2_SOUND_STREAMER_HEADER

#include <xaudio2.h>
#include <vector>

#include "SoundStreamer.h"

class XAudio2Streamer : public SoundStreamer
{
	public:
		XAudio2Streamer();
		virtual ~XAudio2Streamer();

	public:
		bool Init(std::size_t rate,
		          std::size_t bits_per_sample,
		          std::size_t channels) override;

		std::size_t BufferSize() const override;

		void Play() override;

		void Pause() override;

		void Stream(const void *pSamples) override;

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
