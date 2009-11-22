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

#include <windows.h>
#include <mmreg.h>
#include "avi.h"

AVIWriter::AVIWriter()
	: m_pAVIFile(NULL),
	  m_pAudioStream(NULL),
	  m_pCompressedAudioStream(NULL),
	  m_pVideoStream(NULL),
	  m_pCompressedVideoStream(NULL),
	  m_nSampleSize(0),
	  m_nFrame(0)
{
	AVIFileInit();
}

HRESULT AVIWriter::Initialise(const CHAR *pszFileName, 
							  const WAVEFORMATEX *WaveFormat,
							  const bmiData *BitmapFormat,
							  int fps,
							  HWND hWnd)
{
	AVISTREAMINFO StreamInfo;
	AVICOMPRESSOPTIONS opts;
	HRESULT hr = S_OK;

	if (WaveFormat)
		m_WaveFormat = *WaveFormat;

	m_BitmapFormat = *BitmapFormat;

	DeleteFile(pszFileName);
	hr = AVIFileOpen(&m_pAVIFile,
					 pszFileName,
					 OF_WRITE | OF_CREATE,
					 NULL);
	if (SUCCEEDED(hr) && WaveFormat)
	{
		memset(&StreamInfo, 0, sizeof(AVISTREAMINFO));
		StreamInfo.fccType = streamtypeAUDIO;
		StreamInfo.dwQuality = (DWORD)-1;
		StreamInfo.dwScale = 1;
		StreamInfo.dwRate = m_WaveFormat.nSamplesPerSec;
		StreamInfo.dwSampleSize = m_WaveFormat.nBlockAlign;
		strcpy(&StreamInfo.szName[0], "BeebEm Audio Capture");

		m_nSampleSize = StreamInfo.dwSampleSize;
		
		hr = AVIFileCreateStream(m_pAVIFile, &m_pAudioStream, &StreamInfo);
	}

	//if (SUCCEEDED(hr) && WaveFormat)
	//{
	//	memset(&opts, 0, sizeof(AVICOMPRESSOPTIONS));
	//	opts.fccType = streamtypeAUDIO;
	//	opts.fccHandler = 0;
	//	opts.dwKeyFrameEvery = 0;
	//	opts.dwQuality = 0;
	//	opts.dwBytesPerSecond = 0;
	//	opts.dwFlags = 0;
	//	opts.lpFormat = &m_WaveFormat;
	//	opts.cbFormat = sizeof(WAVEFORMATEX);
	//	opts.lpParms = 0;
	//	opts.cbParms = 0;
	//	opts.dwInterleaveEvery = 0;
	//
	//	hr = AVIMakeCompressedStream(&m_pCompressedAudioStream, m_pAudioStream, &opts, NULL);
	//}

	if (SUCCEEDED(hr) && WaveFormat)
	{
		hr = AVIStreamSetFormat(m_pAudioStream,
								0,
								(void*)&m_WaveFormat,
								sizeof(WAVEFORMATEX) + m_WaveFormat.cbSize);
	}

	if (SUCCEEDED(hr))
	{
		memset(&StreamInfo, 0, sizeof(AVISTREAMINFO));
		StreamInfo.fccType = streamtypeVIDEO;
		StreamInfo.fccHandler = 0;
		StreamInfo.dwScale = 1;
		StreamInfo.dwRate = fps;
		StreamInfo.dwSampleSize = 0;
		StreamInfo.dwSuggestedBufferSize = m_BitmapFormat.bmiHeader.biSizeImage;
		StreamInfo.dwQuality = (DWORD)-1;
		StreamInfo.rcFrame.left = 0;
		StreamInfo.rcFrame.top = 0;
		StreamInfo.rcFrame.right = m_BitmapFormat.bmiHeader.biWidth;
		StreamInfo.rcFrame.bottom = m_BitmapFormat.bmiHeader.biHeight;
		strcpy(&StreamInfo.szName[0], "BeebEm Video Capture");
		
		hr = AVIFileCreateStream(m_pAVIFile, &m_pVideoStream, &StreamInfo);
	}

	if (SUCCEEDED(hr))
	{
		memset(&opts, 0, sizeof(AVICOMPRESSOPTIONS));
		opts.fccType = streamtypeVIDEO;
		opts.fccHandler = mmioFOURCC('m', 'r', 'l', 'e'); // Microsoft RLE
		//opts.fccHandler = mmioFOURCC('m', 's', 'v', 'c'); // Microsoft Video 1
		opts.dwKeyFrameEvery = 100;
		opts.dwQuality = 0;
		opts.dwBytesPerSecond = 0;
		opts.dwFlags = AVICOMPRESSF_KEYFRAMES;
		opts.lpFormat = 0;
		opts.cbFormat = 0;
		opts.lpParms = 0;
		opts.cbParms = 0;
		opts.dwInterleaveEvery = 0;

		//AVICOMPRESSOPTIONS *aopts[1];
		//aopts[0]=&opts;
		//AVISaveOptions(hWnd, 0, 1, &m_pVideoStream, aopts);

		hr = AVIMakeCompressedStream(&m_pCompressedVideoStream, m_pVideoStream, &opts, NULL);

		//AVISaveOptionsFree(1,aopts);
	}

	if (SUCCEEDED(hr))
	{
   		hr = AVIStreamSetFormat(m_pCompressedVideoStream,
								0,
								(void*)&m_BitmapFormat,
								m_BitmapFormat.bmiHeader.biSize +
									m_BitmapFormat.bmiHeader.biClrUsed * sizeof(RGBQUAD));
	}

	if (FAILED(hr))
		Close();

	return hr;
}

AVIWriter::~AVIWriter()
{
	Close();
	AVIFileExit();
}

void AVIWriter::Close()
{
	if (NULL != m_pCompressedAudioStream)
	{
		AVIStreamRelease(m_pCompressedAudioStream);
		m_pCompressedAudioStream = NULL;
	}

	if (NULL != m_pAudioStream)
	{
		AVIStreamRelease(m_pAudioStream);
		m_pAudioStream = NULL;
	}

	if (NULL != m_pCompressedVideoStream)
	{
		AVIStreamRelease(m_pCompressedVideoStream);
		m_pCompressedVideoStream = NULL;
	}

	if (NULL != m_pVideoStream)
	{
		AVIStreamRelease(m_pVideoStream);
		m_pVideoStream = NULL;
	}

	if (NULL != m_pAVIFile)
	{
		AVIFileRelease(m_pAVIFile);
		m_pAVIFile = NULL;
	}
}

HRESULT AVIWriter::WriteSound(BYTE *pBuffer,
							  ULONG nBytesToWrite)
{
	if (NULL == m_pAudioStream || NULL == m_pAVIFile)
	{
		return E_UNEXPECTED;
	}

	LONG nSamples2Write = nBytesToWrite / m_nSampleSize;

	HRESULT hr = S_OK;
	ULONG nTotalBytesWritten = 0;

	while ((hr == S_OK) && (nTotalBytesWritten < nBytesToWrite))
	{
		LONG nBytesWritten = 0;
		LONG nSamplesWritten = 0;

		hr = AVIStreamWrite(m_pAudioStream,
							-1,				 // append at the end of the stream
							nSamples2Write,	 // how many samples to write
							pBuffer,		 // where the data is
							nBytesToWrite,	 // how much data do we have
							AVIIF_KEYFRAME,	 // self-sufficient data 
							&nSamplesWritten,// how many samples were written
							&nBytesWritten); // how many bytes were written

		nTotalBytesWritten += nBytesWritten;
	}

	return hr;
}

HRESULT AVIWriter::WriteVideo(BYTE *pBuffer)
{
	if (NULL == m_pCompressedVideoStream || NULL == m_pAVIFile)
	{
		return E_UNEXPECTED;
	}

	HRESULT hr = S_OK;
	LONG nBytesWritten = 0;
	LONG nSamplesWritten = 0;

	hr = AVIStreamWrite(m_pCompressedVideoStream,
						m_nFrame,        // append at the end of the stream
						1,               // how many samples to write
						pBuffer,         // where the data is
						m_BitmapFormat.bmiHeader.biSizeImage, // how much data do we have
						AVIIF_KEYFRAME,	 // self-sufficient data 
						&nSamplesWritten,// how many samples were written
						&nBytesWritten); // how many bytes were written
	if (SUCCEEDED(hr))
		m_nFrame++;

	return hr;
}
