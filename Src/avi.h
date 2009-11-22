/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2005  Mike Wyatt

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

//
// BeebEm AVI Video Capture
//
// Mike Wyatt - Oct 2005
//

#include <vfw.h>
#include "beebwin.h"

class AVIWriter  
{
public:
	AVIWriter();
	virtual ~AVIWriter();

	// Open file
	HRESULT Initialise(const CHAR *psFileName, 
					   const WAVEFORMATEX *WaveFormat,
					   const bmiData *BitmapFormat,
					   int fps,
					   HWND hWnd);
	void Close(void);

	HRESULT WriteSound(BYTE *pBuffer,
					   ULONG nBytesToWrite);
	HRESULT WriteVideo(BYTE *pBuffer);

private:
	PAVIFILE m_pAVIFile;

	WAVEFORMATEX m_WaveFormat;
	PAVISTREAM m_pAudioStream;
	PAVISTREAM m_pCompressedAudioStream;
	LONG m_nSampleSize;

	bmiData m_BitmapFormat;
	PAVISTREAM m_pVideoStream;
	PAVISTREAM m_pCompressedVideoStream;
	LONG m_nFrame;
};
