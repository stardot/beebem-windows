/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2024 Chris Needham

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

#ifndef TELETEXT_DIALOG_HEADER
#define TELETEXT_DIALOG_HEADER

#include <string>

#include "Dialog.h"
#include "Teletext.h"

class TeletextDialog : public Dialog
{
	public:
		TeletextDialog(
			HINSTANCE hInstance,
			HWND hwndParent,
			TeletextSourceType Source,
			const std::string& DiscsPath,
			const std::string* FileName,
			const std::string* IPAddress,
			const u_short* IPPort
		);

	public:
		TeletextSourceType GetSource() const;
		const std::string& GetFileName(int Index) const;
		const std::string& GetIPAddress(int Index) const;
		u_short GetPort(int Index) const;

	private:
		virtual INT_PTR DlgProc(
			UINT   nMessage,
			WPARAM wParam,
			LPARAM lParam
		);

		INT_PTR OnCommand(int Notification, int nCommandID);

		void SelectFile(int Channel);

		BOOL GetChannelIPControls(int Channel);
		void SetChannelIPControls(int Channel);

		void EnableFileControls(bool bEnable);
		void EnableIPControls(bool bEnable);

	private:
		TeletextSourceType m_TeletextSource;
		std::string m_DiscsPath;
		std::string m_FileName[4];
		std::string m_IPAddress[4];
		u_short m_IPPort[4];
};

#endif
