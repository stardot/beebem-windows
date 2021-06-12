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
	  m_videoCompressor(0),
	  m_videoBuffer(0),
	  m_lastVideoFrame(0),
	  m_videoBufferSize(0),
	  m_nSampleSize(0),
	  m_nFrame(0)
{
	AVIFileInit();
}

HRESULT AVIWriter::Initialise(const CHAR *pszFileName, 
                              const WAVEFORMATEX *WaveFormat,
                              const bmiData *BitmapFormat,
                              int fps)
{
	AVISTREAMINFO StreamInfo;
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

	bmiData outputData = m_BitmapFormat;
	outputData.bmiHeader.biCompression = BI_RLE8;

	if (SUCCEEDED(hr))
	{
		hr = AVIStreamSetFormat(m_pVideoStream,
								0,
								(void*)&outputData,
								outputData.bmiHeader.biSize +
									outputData.bmiHeader.biClrUsed * sizeof(RGBQUAD));
	}

	m_BitmapOutputFormat = BitmapFormat->bmiHeader;
	m_BitmapOutputFormat.biCompression = BI_RLE8;

	m_videoCompressor = ICOpen(
		mmioFOURCC('V', 'I', 'D', 'C'),
		mmioFOURCC('m', 'r', 'l', 'e'),
		ICMODE_FASTCOMPRESS);

	if (m_videoCompressor)
	{
		m_videoBufferSize = ICCompressGetSize(m_videoCompressor, &BitmapFormat->bmiHeader, &m_BitmapOutputFormat);
		m_videoBuffer = malloc(m_videoBufferSize);
		m_lastVideoFrame = malloc(BitmapFormat->bmiHeader.biSizeImage);
		if (!m_videoBuffer || !m_lastVideoFrame)
			hr = E_OUTOFMEMORY;
	}
	else
	{
		hr = E_FAIL;
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

	if (NULL != m_videoBuffer)
	{
		free(m_videoBuffer);
		m_videoBuffer = NULL;
	}

	if (NULL != m_lastVideoFrame)
	{
		free(m_lastVideoFrame);
		m_lastVideoFrame = NULL;
	}

	if (NULL != m_videoCompressor)
	{
		ICClose(m_videoCompressor);
		m_videoCompressor = NULL;
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
	if (NULL == m_videoCompressor || NULL == m_videoBuffer || NULL == m_lastVideoFrame || NULL == m_pAVIFile)
	{
		return E_UNEXPECTED;
	}

	bool keyFrame = m_nFrame % 25 == 0;

	DWORD flags = 0;
	DWORD result = ICCompress(
		m_videoCompressor,
		keyFrame ? ICCOMPRESS_KEYFRAME : 0,
		&m_BitmapOutputFormat,
		m_videoBuffer,
		&m_BitmapFormat.bmiHeader,
		pBuffer,
		0,
		&flags,
		m_nFrame,
		m_videoBufferSize,
		0,
		keyFrame ? 0 : &m_BitmapFormat.bmiHeader,
		keyFrame ? 0 : m_lastVideoFrame);

	// Save the frame for next time
	memcpy(m_lastVideoFrame, pBuffer, m_BitmapFormat.bmiHeader.biSizeImage);

	if (result)
		return E_FAIL;

	HRESULT hr = S_OK;
	LONG nBytesWritten = 0;
	LONG nSamplesWritten = 0;

	hr = AVIStreamWrite(m_pVideoStream,
						m_nFrame,        // append at the end of the stream
						1,               // how many samples to write
						m_videoBuffer,   // where the data is
						m_BitmapOutputFormat.biSizeImage, // how much data do we have
						keyFrame ? AVIIF_KEYFRAME : 0,    // is this a key frame?
						&nSamplesWritten,// how many samples were written
						&nBytesWritten); // how many bytes were written
	if (SUCCEEDED(hr))
		m_nFrame++;

	return hr;
}
