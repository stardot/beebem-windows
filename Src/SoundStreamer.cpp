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

#include "SoundStreamer.h"
#include "DirectSoundStreamer.h"
#include "Main.h"
#include "Sound.h"
#include "XAudio2Streamer.h"

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

SoundStreamer *CreateSoundStreamer(int samplerate, int bits_per_sample, int channels)
{
	if (SelectedSoundStreamer == SoundStreamerType::XAudio2)
	{
		SoundStreamer *pSoundStreamer = new XAudio2Streamer();

		if (pSoundStreamer->Init(samplerate, bits_per_sample, channels))
		{
			return pSoundStreamer;
		}
		else
		{
			delete pSoundStreamer;
			pSoundStreamer= nullptr;

			SelectedSoundStreamer = SoundStreamerType::DirectSound;
		}
	}

	SoundStreamer *pSoundStreamer = new DirectSoundStreamer();

	if (pSoundStreamer->Init(samplerate, bits_per_sample, channels))
	{
		return pSoundStreamer;
	}
	else
	{
		delete pSoundStreamer;
		pSoundStreamer= nullptr;

		mainWin->Report(MessageType::Error, "Attempt to start sound system failed");

		return nullptr;
	}
}
