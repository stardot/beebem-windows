// BeebBitmap.cpp
// A simple class to hold on to a bitmap & Paint it

#include <windows.h>
#include <stdio.h>

#include "BeebBitmap.h"

BeebBitmap::BeebBitmap()
{
  	__int64 *	m_pAligned;

	m_pAligned = new __int64[640*32];	// Used to make sure of alignment
	m_bitmapbits = (char *) m_pAligned;

	m_hDCBitmap = CreateCompatibleDC(NULL);

	m_hBitmap = CreateBitmap(640,256,1,8,NULL);//m_bitmapbits);
	
	if(!SelectObject(m_hDCBitmap, m_hBitmap))
		MessageBox(NULL,"Error selecting the screen bitmap\n"
						"Make sure you are running in a 256 colour mode","Error!",MB_OK); 
	
	m_lpbmi = (BITMAPINFO *)&m_bmi;	
  	m_bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);		 	
	m_bmi.bmiHeader.biWidth = 640;
	m_bmi.bmiHeader.biHeight = -256;
	m_bmi.bmiHeader.biPlanes = 1;
	m_bmi.bmiHeader.biBitCount = 8; 	
	m_bmi.bmiHeader.biCompression = BI_RGB;
	m_bmi.bmiHeader.biSizeImage = 640*256;
	m_bmi.bmiHeader.biClrUsed = 8;
	m_bmi.bmiHeader.biClrImportant = 8;

	__int16 *pInts = (__int16 *)&m_bmi.aColors[0];

	for(int i=0; i<9; i++)
		pInts[i] = i;

}

void BeebBitmap::RealizePalette(HDC hDC)
{

}

void BeebBitmap::Blit(HDC hDestDC, int srcy, int size)
{
	SetBitmapBits(m_hBitmap, 640*256, m_bitmapbits);
		
	//SetDIBits(m_hDCBitmap, m_hBitmap, srcy, size, m_bitmapbits, (BITMAPINFO *)&m_bmi, DIB_RGB_COLORS);
	//SetDIBits(m_hDCBitmap, m_hBitmap, srcy, size, m_bitmapbits, m_lpbmi, DIB_RGB_COLORS);
	SelectObject(m_hDCBitmap, m_hBitmap);
				
	BitBlt(hDestDC, 0, srcy, 640, size, m_hDCBitmap, 0, srcy, SRCCOPY);		
}
