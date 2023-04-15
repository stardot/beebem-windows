/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2023  Chris Needham

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

#include <windows.h>
#include <process.h>

#include "Thread.h"

Thread::Thread() :
	m_hThread(nullptr),
	m_hStartEvent(nullptr)
{
}

Thread::~Thread()
{
}

bool Thread::Start()
{
	m_hStartEvent = CreateEvent(nullptr, TRUE, FALSE, nullptr);

	if (m_hStartEvent == nullptr)
	{
		return false;
	}

	m_hThread = reinterpret_cast<HANDLE>(_beginthreadex(
		nullptr,        // security
		0,              // stack_size
		s_ThreadFunc,   // start_address
		this,           // arglist
		0,              // initflag
		nullptr         // thrdaddr
	));

	bool bSuccess = m_hThread != nullptr;

	if (bSuccess)
	{
		WaitForSingleObject(m_hStartEvent, INFINITE);
	}

	CloseHandle(m_hStartEvent);
	m_hStartEvent = nullptr;

	return bSuccess;
}

bool Thread::IsStarted() const
{
	return m_hThread != nullptr;
}

unsigned int __stdcall Thread::s_ThreadFunc(void *parameter)
{
	Thread* pThread = reinterpret_cast<Thread*>(parameter);

	// Notify the parent thread that this thread has started running.
	SetEvent(pThread->m_hStartEvent);

	return pThread->ThreadFunc();
}
