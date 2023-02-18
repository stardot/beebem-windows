/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2023 Chris Needham

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

#ifndef RING_BUFFER_HEADER
#define RING_BUFFER_HEADER

class RingBuffer
{
	public:
		RingBuffer();
		RingBuffer(const RingBuffer&) = delete;
		RingBuffer& operator=(const RingBuffer&) = delete;

	public:
		void Reset();
		bool HasData() const;
		unsigned char GetData();
		bool PutData(unsigned char Data);
		int GetSpace() const;
		int GetLength() const;

	private:
		static const int BUFFER_SIZE = 10240;
		unsigned char m_Buffer[BUFFER_SIZE];
		int m_Head;
		int m_Tail;
		int m_Length;
};

#endif
