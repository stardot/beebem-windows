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
