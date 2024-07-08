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

#include <assert.h>

#include "RingBuffer.h"

/*****************************************************************************/

RingBuffer::RingBuffer(int Size) :
	m_pBuffer(new unsigned char[Size]),
	m_Size(Size)
{
	Reset();
}

/*****************************************************************************/

RingBuffer::~RingBuffer()
{
	delete [] m_pBuffer;
}

/*****************************************************************************/

void RingBuffer::Reset()
{
	m_Head = 0;
	m_Tail = 0;
	m_Length = 0;
}

/*****************************************************************************/

bool RingBuffer::HasData() const
{
	return m_Length > 0;
}

/*****************************************************************************/

unsigned char RingBuffer::GetData()
{
	assert(m_Length > 0);

	unsigned char Data = m_pBuffer[m_Head];
	m_Head = (m_Head + 1) % m_Size;
	m_Length--;

	return Data;
}

/*****************************************************************************/

bool RingBuffer::PutData(unsigned char Data)
{
	if (m_Length != m_Size)
	{
		m_pBuffer[m_Tail] = Data;
		m_Tail = (m_Tail + 1) % m_Size;
		m_Length++;
		return true;
	}

	return false;
}

/*****************************************************************************/

int RingBuffer::GetSpace() const
{
	return m_Size - m_Length;
}

/*****************************************************************************/

int RingBuffer::GetLength() const
{
	return m_Length;
}

/*****************************************************************************/
