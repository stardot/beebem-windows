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

#ifndef SOUND_STREAMER_HEADER
#define SOUND_STREAMER_HEADER

#include <vector>

class SoundStreamer
{
	public:
		SoundStreamer();
		virtual ~SoundStreamer();

		virtual bool Init(std::size_t samplerate,
		                  std::size_t bits_per_sample,
		                  std::size_t channels) = 0;

		virtual std::size_t BufferSize() const = 0;

		virtual void Play() = 0;
		virtual void Pause() = 0;

		virtual void Stream(const void *pSamples) = 0;

		static void PlayAll();
		static void PauseAll();

	private:
		static std::vector<SoundStreamer*> m_streamers;
};

SoundStreamer *CreateSoundStreamer(int samplerate, int bits_per_sample, int channels);

#endif
