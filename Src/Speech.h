/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2005  John Welch

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

#ifndef SPEECH_HEADER
#define SPEECH_HEADER

#if ENABLE_SPEECH

bool SpeechInit();
void SpeechStart();
void SpeechStop();
void SpeechWrite(unsigned char Data);
void SpeechReadEnable();
unsigned char SpeechRead();
bool SpeechReady();
bool SpeechInterrupt();
void SpeechUpdate(unsigned char *buff, int length);
void SpeechPoll(int Cycles);

extern bool SpeechEnabled;
extern bool SpeechStarted;

#endif

#endif
