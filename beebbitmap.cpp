// BeebBitmap.cpp
// A simple class to hold on to a bitmap & Paint it
// WinG Edition

#include <windows.h>
#include <stdio.h>
#include <wing.h>

#include "BeebBitmap.h"

pal LogicalPalette =
{
  0x300,
  256
};

BeebBitmap::BeebBitmap()
{
	m_lpbmi = (BITMAPINFO *)&m_bmi;	
  	m_bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);		 	
	m_bmi.bmiHeader.biWidth = 320;
	m_bmi.bmiHeader.biHeight = -256;
	m_bmi.bmiHeader.biPlanes = 1;
	m_bmi.bmiHeader.biBitCount = 8; 	
	m_bmi.bmiHeader.biCompression = BI_RGB;
	m_bmi.bmiHeader.biSizeImage = 320*256;
	m_bmi.bmiHeader.biClrUsed = 0;
	m_bmi.bmiHeader.biClrImportant = 0;

	HDC	hScreenDC;
	int Counter;

	hScreenDC = GetDC(0);
	GetSystemPaletteEntries(hScreenDC,0,255,LogicalPalette.aEntries);
	ReleaseDC(0,hScreenDC);


	for(Counter = 0; Counter<8; Counter++)
	{
		LogicalPalette.aEntries[Counter+20].peRed =	  (Counter &1)?0xFF:0;
		LogicalPalette.aEntries[Counter+20].peGreen = (Counter &2)?0xFF:0;
		LogicalPalette.aEntries[Counter+20].peBlue =  (Counter &4)?0xFF:0;

	}	
	 for(Counter = 0;Counter < 256;Counter++)
          {
            // copy the system colors into the DIB header
            // WinG will do this in WinGRecommendDIBFormat,
            // but it may have failed above so do it here anyway
            
            m_bmi.aColors[Counter].rgbRed =
                    LogicalPalette.aEntries[Counter].peRed;
            m_bmi.aColors[Counter].rgbGreen =
                    LogicalPalette.aEntries[Counter].peGreen;
            m_bmi.aColors[Counter].rgbBlue =
                    LogicalPalette.aEntries[Counter].peBlue;
            m_bmi.aColors[Counter].rgbReserved = 0;

            LogicalPalette.aEntries[Counter].peFlags = 0;
		  
          }

	 m_hpalApp = CreatePalette((LOGPALETTE far *)&LogicalPalette);
		
	m_hDCBitmap = WinGCreateDC();

	void * ptr;

	m_hBitmap = WinGCreateBitmap(m_hDCBitmap,m_lpbmi,&ptr);
	
	m_bitmapbits = (char *)ptr;

	if(!SelectObject(m_hDCBitmap, m_hBitmap))
		MessageBox(NULL,"Error selecting the screen bitmap\n"
						"Make sure you are running in a 256 colour mode","Error!",MB_OK); 
}

void BeebBitmap::RealizePalette(HDC hDC)
{
	SelectPalette(hDC, m_hpalApp, FALSE);
	::RealizePalette(hDC);	
}

void BeebBitmap::Blit(HDC hDestDC, int srcy, int size)
{
	SelectObject(m_hDCBitmap, m_hBitmap);

	WinGBitBlt(hDestDC, 0, srcy, 320, size, m_hDCBitmap, 0, srcy);		
}
