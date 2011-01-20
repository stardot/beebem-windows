/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 1998  Mike Wyatt

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

// BeebWin DirectX and display rendering support

#include <stdio.h>
#include <windows.h>
#include <initguid.h>
#include <ddraw.h>
#include "main.h"
#include "beebwin.h"
#include "beebemrc.h"
#include "6502core.h"
#include "ext1770.h"
#include "avi.h"

typedef HRESULT ( WINAPI* LPDIRECTDRAWCREATE )( GUID FAR *lpGUID, LPDIRECTDRAW FAR *lplpDD, IUnknown FAR *pUnkOuter );

extern HMODULE hFDCBoard;
extern EDCB ExtBoard;
extern AVIWriter *aviWriter;

/****************************************************************************/
void BeebWin::InitDX(void)
{
	if (m_DisplayRenderer == IDM_DISPDX9)
	{
		HRESULT hr = InitDX9();
		if (hr != D3D_OK)
		{
			char errstr[200];
			sprintf(errstr,"DirectX9 initialisation failed\nFailure code %X\nTrying DirectDraw",hr);
			MessageBox(m_hWnd,errstr,WindowTitle,MB_OK|MB_ICONERROR);

			m_DisplayRenderer = IDM_DISPDDRAW;
			UpdateDisplayRendererMenu();
		}
	}

	if (m_DisplayRenderer == IDM_DISPDDRAW)
	{
		HRESULT hr = InitDirectDraw();
		if (hr != DD_OK)
		{
			char errstr[200];
			sprintf(errstr,
					"DirectDraw initialisation failed\nFailure code %X\nSwitching to GDI",hr);
			MessageBox(m_hWnd,errstr,WindowTitle,MB_OK|MB_ICONERROR);
			PostMessage(m_hWnd, WM_COMMAND, IDM_DISPGDI, 0);
		}
	}

	m_CurrentDisplayRenderer = m_DisplayRenderer;
}
void BeebWin::ResetDX(void)
{
	m_DXResetPending = false;

	if (m_CurrentDisplayRenderer == IDM_DISPDX9)
	{
		ExitDX9();

		// Need to let message loop run before re-initialising DX otherwise
		// odd artifacts are seen when changing window size.
		PostMessage(m_hWnd, WM_REINITDX, 0, 0);
	}
	else if (m_CurrentDisplayRenderer == IDM_DISPDDRAW)
	{
		ResetSurfaces();
		ReinitDX();
	}
}
void BeebWin::ReinitDX(void)
{
	HRESULT hr = DD_OK;

	if (m_DisplayRenderer == IDM_DISPDX9)
	{
		hr = InitDX9();
	}
	else if (m_DisplayRenderer == IDM_DISPDDRAW)
	{
		hr = InitSurfaces();
	}

	if (hr != DD_OK)
	{
		char errstr[200];
		sprintf(errstr,"DirectX failure re-initialising\nFailure code %X\nSwitching to GDI\n",hr);
		MessageBox(m_hWnd,errstr,WindowTitle,MB_OK|MB_ICONERROR);
		PostMessage(m_hWnd, WM_COMMAND, IDM_DISPGDI, 0);
	}

	m_CurrentDisplayRenderer = m_DisplayRenderer;
}
void BeebWin::ExitDX(void)
{
	if (m_CurrentDisplayRenderer == IDM_DISPDX9)
	{
		ExitDX9();
	}
	else if (m_CurrentDisplayRenderer == IDM_DISPDDRAW)
	{
		ResetSurfaces();
		if (m_DD2)
		{
			m_DD2->Release();
			m_DD2 = NULL;
		}
		if (m_DD)
		{
			m_DD->Release();
			m_DD = NULL;
		}
	}
}

/****************************************************************************/
HRESULT BeebWin::InitDirectDraw(void)
{
	HRESULT ddrval = DDERR_GENERIC;
	HINSTANCE hInstDDraw;
	LPDIRECTDRAWCREATE pDDCreate = NULL;

	m_DD = NULL;
	m_DD2 = NULL;
	m_Clipper = NULL;
	m_DDS2One = NULL;
	m_DDSOne = NULL;
	m_DDS2Primary = NULL;
	m_DDSPrimary = NULL;

	hInstDDraw = LoadLibrary("ddraw.dll");
	if(hInstDDraw)
	{
		pDDCreate = (LPDIRECTDRAWCREATE)GetProcAddress(hInstDDraw, "DirectDrawCreate");
		if( pDDCreate )
		{
			ddrval = pDDCreate( NULL, &m_DD, NULL );
			if( ddrval == DD_OK )
			{
				ddrval = m_DD->QueryInterface(IID_IDirectDraw2, (LPVOID *)&m_DD2);
			}
			if( ddrval == DD_OK )
			{
				ddrval = InitSurfaces();
			}
		}
	}

	return ddrval;
}

/****************************************************************************/
HRESULT BeebWin::InitSurfaces(void)
{
	DDSURFACEDESC ddsd;
	HRESULT ddrval;

	if (m_isFullScreen)
		ddrval = m_DD2->SetCooperativeLevel( m_hWnd, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN);
	else
		ddrval = m_DD2->SetCooperativeLevel( m_hWnd, DDSCL_NORMAL );
	if( ddrval == DD_OK )
	{
		if (m_isFullScreen)
		{
			ddrval = m_DD2->SetDisplayMode(m_XDXSize, m_YDXSize, 32, 0, 0);
		}
	}
	if( ddrval == DD_OK )
	{
		ddsd.dwSize = sizeof( ddsd );
		ddsd.dwFlags = DDSD_CAPS;
		ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
		ddrval = m_DD2->CreateSurface( &ddsd, &m_DDSPrimary, NULL );
	}

	if( ddrval == DD_OK )
		ddrval = m_DDSPrimary->QueryInterface(IID_IDirectDrawSurface2, (LPVOID *)&m_DDS2Primary);
	if( ddrval == DD_OK )
		ddrval = m_DD2->CreateClipper( 0, &m_Clipper, NULL );
	if( ddrval == DD_OK )
		ddrval = m_Clipper->SetHWnd( 0, m_hWnd );
	if( ddrval == DD_OK )
		ddrval = m_DDS2Primary->SetClipper( m_Clipper );
	if( ddrval == DD_OK )
	{
		ZeroMemory(&ddsd, sizeof(ddsd));
		ddsd.dwSize = sizeof(ddsd);
		ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
		ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
		if (m_DXSmoothing && (!m_DXSmoothMode7Only || TeletextEnabled))
			ddsd.ddsCaps.dwCaps |= DDSCAPS_VIDEOMEMORY;
		else
			ddsd.ddsCaps.dwCaps |= DDSCAPS_SYSTEMMEMORY;
		ddsd.dwWidth = 800;
		ddsd.dwHeight = 512;
		ddrval = m_DD2->CreateSurface(&ddsd, &m_DDSOne, NULL);
	}
	if( ddrval == DD_OK )
	{
		ddrval = m_DDSOne->QueryInterface(IID_IDirectDrawSurface2, (LPVOID *)&m_DDS2One);
	}

	if (ddrval == DD_OK)
		m_DXInit = TRUE;

	return ddrval;
}

/****************************************************************************/
void BeebWin::ResetSurfaces(void)
{
	if (m_Clipper)
	{
		m_Clipper->Release();
		m_Clipper = NULL;
	}
	if (m_DDS2One)
	{
		m_DDS2One->Release();
		m_DDS2One = NULL;
	}
	if (m_DDSOne)
	{
		m_DDSOne->Release();
		m_DDSOne = NULL;
	}
	if (m_DDS2Primary)
	{
		m_DDS2Primary->Release();
		m_DDS2Primary = NULL;
	}
	if (m_DDSPrimary)
	{
		m_DDSPrimary->Release();
		m_DDSPrimary = NULL;
	}

	m_DXInit = FALSE;
}

/****************************************************************************/
HRESULT BeebWin::InitDX9(void)
{
	CUSTOMVERTEX* pVertices = NULL;
	HRESULT hr = D3D_OK;

	m_pD3D = NULL;
	m_pd3dDevice = NULL;
	m_pVB = NULL;
	m_pTexture = NULL;

	// Create the D3D object.
	m_pD3D = Direct3DCreate9( D3D_SDK_VERSION );
	if( NULL == m_pD3D )
	{
		hr = E_FAIL;
	}

#if 0
	if (hr == D3D_OK)
	{
		D3DDISPLAYMODE d3dMode;
		UINT nModes = m_pD3D->GetAdapterModeCount(D3DADAPTER_DEFAULT, D3DFMT_X8R8G8B8);
		for (UINT mode = 0; mode < nModes; ++mode)
		{
			hr = m_pD3D->EnumAdapterModes(D3DADAPTER_DEFAULT, D3DFMT_X8R8G8B8,
										  mode, &d3dMode);
			char str[200];
			sprintf(str, "D3D Mode: %d x %d, refresh %d\n",
					d3dMode.Width, d3dMode.Height, d3dMode.RefreshRate);
			OutputDebugString(str);
		}
	}
#endif

	if (hr == D3D_OK)
	{
		// Set up the structure used to create the D3DDevice.
		D3DPRESENT_PARAMETERS d3dpp;
		ZeroMemory( &d3dpp, sizeof(d3dpp) );

		d3dpp.Windowed = m_isFullScreen ? FALSE : TRUE;
		if (d3dpp.Windowed)
		{
			d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
		}
		else
		{
			d3dpp.BackBufferWidth = m_XDXSize;
			d3dpp.BackBufferHeight = m_YDXSize;
			d3dpp.BackBufferFormat = D3DFMT_X8R8G8B8;
		}
		d3dpp.BackBufferCount = 1;
		d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
		d3dpp.hDeviceWindow = m_hWnd;
		d3dpp.EnableAutoDepthStencil = FALSE;

		// Create the D3DDevice
		hr = m_pD3D->CreateDevice(
			D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, m_hWnd,
			D3DCREATE_SOFTWARE_VERTEXPROCESSING,
			&d3dpp, &m_pd3dDevice );
	}

	if (hr == D3D_OK)
	{
		// Turn off D3D lighting
		m_pd3dDevice->SetRenderState( D3DRS_LIGHTING, FALSE );

		if (m_DXSmoothing && (!m_DXSmoothMode7Only || TeletextEnabled))
		{
			// Turn on bilinear interpolation so image is smoothed
			m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
			m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
		}

		// Just display a texture
		m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLOROP,	 D3DTOP_SELECTARG1 );
		m_pd3dDevice->SetTextureStageState( 0, D3DTSS_COLORARG1, D3DTA_TEXTURE );

		// Create the vertex buffer.
		hr = m_pd3dDevice->CreateVertexBuffer(
			4 * sizeof(CUSTOMVERTEX),
			0, D3DFVF_CUSTOMVERTEX,
			D3DPOOL_MANAGED, &m_pVB, NULL );
	}

	if (hr == D3D_OK)
	{
		// Fill the vertex buffer. We are setting the tu and tv texture
		// coordinates, which range from 0.0 to 1.0
		hr = m_pVB->Lock( 0, 0, (void**)&pVertices, 0 );
	}

	if (hr == D3D_OK)
	{
		pVertices[0].position = D3DXVECTOR3( 0.0f, -511.0f, 0.0f );
		pVertices[0].color	  = 0x00ffffff;
		pVertices[0].tu		  = 0.0f;
		pVertices[0].tv		  = 1.0f;

		pVertices[1].position = D3DXVECTOR3( 0.0f, 0.0f, 0.0f );
		pVertices[1].color	  = 0x00ffffff;
		pVertices[1].tu		  = 0.0f;
		pVertices[1].tv		  = 0.0f;

		pVertices[2].position = D3DXVECTOR3( 799.0f, -511.0f, 0.0f );
		pVertices[2].color	  = 0x00ffffff;
		pVertices[2].tu		  = 1.0f;
		pVertices[2].tv		  = 1.0f;

		pVertices[3].position = D3DXVECTOR3( 799.0f, 0.0f, 0.0f );
		pVertices[3].color	  = 0x00ffffff;
		pVertices[3].tu		  = 1.0f;
		pVertices[3].tv		  = 0.0f;
	
		m_pVB->Unlock();
	}

	if (hr == D3D_OK)
	{
		// Set up matrices
		D3DXMATRIX Ortho2D;
		//D3DXMatrixOrthoOffCenterLH(&Ortho2D, 0.0f, 800.0f, -512.0f, 0.0f, 0.0f, 1.0f);
		D3DXMatrixIdentity(&Ortho2D);
		float l = 0.0f;
		float r = 800.0f;
		float b = -512.0f;
		float t = 0.0f;
		float zn = 0.0f;
		float zf = 1.0f;
		Ortho2D._11 = 2.0f/(r-1.0f);
		Ortho2D._22 = 2.0f/(t-b);
		Ortho2D._33 = 1.0f/(zf-zn);
		Ortho2D._41 = (1.0f+r)/(1.0f-r);
		Ortho2D._42 = (t+b)/(b-t);
		Ortho2D._43 = zn/(zn-zf);

		m_pd3dDevice->SetTransform(D3DTS_PROJECTION, &Ortho2D);

		D3DXMATRIX Ident;
		D3DXMatrixIdentity(&Ident);
		m_pd3dDevice->SetTransform(D3DTS_VIEW, &Ident);
		m_pd3dDevice->SetTransform(D3DTS_WORLD, &Ident);

		// Identity matrix will fill window with our texture
		D3DXMatrixIdentity(&m_TextureMatrix);
	}		

	if (hr == D3D_OK)
	{
		//hr = D3DXCreateTexture(m_pd3dDevice, 800, 512, 0, 0,
		//		D3DFMT_X8R8G8B8, D3DPOOL_MANAGED, &m_pTexture);
		hr = m_pd3dDevice->CreateTexture(800, 512, 1, 0,
				D3DFMT_X8R8G8B8, D3DPOOL_MANAGED, &m_pTexture, NULL);
	}

	if (hr == D3D_OK)
	{
		m_DXInit = TRUE;
	}
	else
	{
		ExitDX9();
	}

	return hr;
}

/****************************************************************************/
void BeebWin::ExitDX9(void)
{
	if (m_pTexture != NULL)
	{
		m_pTexture->Release();
		m_pTexture = NULL;
	}

	if( m_pVB != NULL )
	{
		m_pVB->Release();
		m_pVB = NULL;
	}

	if( m_pd3dDevice != NULL )
	{
		m_pd3dDevice->Release();
		m_pd3dDevice = NULL;
	}

	if( m_pD3D != NULL )
	{
		m_pD3D->Release();
		m_pD3D = NULL;
	}

	m_DXInit = FALSE;
}

/****************************************************************************/
void BeebWin::RenderDX9(void)
{
	HRESULT hr;

	// Clear the backbuffer
	hr = m_pd3dDevice->Clear( 0, NULL, D3DCLEAR_TARGET,
						 D3DCOLOR_XRGB(0,0,0), 1.0f, 0 );

	// Begin the scene
	hr = m_pd3dDevice->BeginScene();
	if(hr == D3D_OK)
	{
		hr = m_pd3dDevice->SetStreamSource( 0, m_pVB, 0, sizeof(CUSTOMVERTEX) );
		if (hr == D3D_OK)
			hr = m_pd3dDevice->SetFVF( D3DFVF_CUSTOMVERTEX );
		if (hr == D3D_OK)
			hr = m_pd3dDevice->SetTexture(0, m_pTexture);
		if (hr == D3D_OK)
			hr = m_pd3dDevice->SetTransform(D3DTS_WORLD, &m_TextureMatrix);
		if (hr == D3D_OK)
			hr = m_pd3dDevice->DrawPrimitive(D3DPT_TRIANGLESTRIP, 0, 4);

		// End the scene
		if (hr == D3D_OK)
			hr = m_pd3dDevice->EndScene();
	}

	// Present the backbuffer contents to the display
	if (hr == D3D_OK)
		hr = m_pd3dDevice->Present( NULL, NULL, NULL, NULL );

	if (hr == D3DERR_DEVICELOST)
	{
		// Can be generated when switching in/out of fullscreen - need to reset
		ResetDX();
		hr = D3D_OK;
	}

	if (hr != D3D_OK)
	{
		char errstr[200];
		sprintf(errstr,"DirectX9 renderer failed\nFailure code %X\nSwitching to GDI\n",hr);
		MessageBox(m_hWnd,errstr,WindowTitle,MB_OK|MB_ICONERROR);
		PostMessage(m_hWnd, WM_COMMAND, IDM_DISPGDI, 0);
	}
}

/****************************************************************************/
void BeebWin::updateLines(HDC hDC, int starty, int nlines)
{
	static unsigned char LastTeletextEnabled = 255;
	WINDOWPLACEMENT wndpl;
	HRESULT ddrval;
	HDC hdc;
	int TTLines=0;
	int TextStart=240;
	int i,j;

	// Not initialised yet?
	if (m_screen == NULL)
		return;
	
	// Check for text view
	if (m_TextViewEnabled)
	{
		TextView();
		return;
	}

	// Changed to/from teletext mode?
	if (LastTeletextEnabled != TeletextEnabled)
	{
		if (m_DXSmoothing && m_DXSmoothMode7Only)
			UpdateSmoothing();
		LastTeletextEnabled = TeletextEnabled;
	}

	// Use last stored params?
	if (starty == 0 && nlines == 0)
	{
		starty = m_LastStartY;
		nlines = m_LastNLines;
	}
	else
	{
		m_LastStartY = starty;
		m_LastNLines = nlines;
	}

	++m_ScreenRefreshCount;
	TTLines=500/TeletextStyle;

	// Do motion blur
	if (m_MotionBlur != IDM_BLUR_OFF)
	{
		if (m_MotionBlur == IDM_BLUR_2)
			j = 32;
		else if (m_MotionBlur == IDM_BLUR_4)
			j = 16;
		else // blur 8 frames
			j = 8;

		for (i = 0; i < 800*512; ++i)
		{
			if (m_screen[i] != 0)
			{
				m_screen_blur[i] = m_screen[i];
			}
			else if (m_screen_blur[i] != 0)
			{
				m_screen_blur[i] += j;
				if (m_screen_blur[i] > 63)
					m_screen_blur[i] = 0;
			}
		}
		memcpy(m_screen, m_screen_blur, 800*512);
	}

	if (m_DisplayRenderer == IDM_DISPGDI)
	{
		RECT destRect;
		GetClientRect(m_hWnd, &destRect);
		int win_nlines = destRect.bottom;
		TextStart = win_nlines - 20;

		int xAdj = 0;
		int yAdj = 0;
		if (m_isFullScreen && m_MaintainAspectRatio)
		{
			// Aspect ratio adjustment
			xAdj = (int)(m_XRatioCrop * (float)m_XWinSize);
			yAdj = (int)(m_YRatioCrop * (float)m_YWinSize);
		}

		StretchBlt(hDC, xAdj, yAdj, m_XWinSize - xAdj * 2, win_nlines - yAdj * 2,
				   m_hDCBitmap, 0, starty,
				   (TeletextEnabled)?552:ActualScreenWidth, (TeletextEnabled==1)?TTLines:nlines,
				   SRCCOPY);

		if ((DisplayCycles>0) && (hFDCBoard!=NULL))
		{
			SetBkMode(hDC,TRANSPARENT);
			SetTextColor(hDC,0x808080);
			TextOut(hDC,0,TextStart,ExtBoard.BoardName,(int)strlen(ExtBoard.BoardName));
		}
	}
	else
	{
		if (m_DXInit == FALSE)
			return;

		wndpl.length = sizeof(WINDOWPLACEMENT);
		if (GetWindowPlacement(m_hWnd, &wndpl))
		{
			if (wndpl.showCmd == SW_SHOWMINIMIZED)
				return;
		}

		if (m_DisplayRenderer == IDM_DISPDX9)
		{
			IDirect3DSurface9 *pSurface;
			ddrval = m_pTexture->GetSurfaceLevel(0, &pSurface);
			if (ddrval == D3D_OK)
			{
				ddrval = pSurface->GetDC(&hdc);
				if (ddrval == D3D_OK)
				{
					BitBlt(hdc, 0, 0, 800, nlines, m_hDCBitmap, 0, starty, SRCCOPY);
					DisplayClientAreaText(hdc);
					pSurface->ReleaseDC(hdc);

					// Scale beeb screen to fill the D3D texture
					int width = (TeletextEnabled)?552:ActualScreenWidth;
					int height = (TeletextEnabled)?TTLines:nlines;
					//D3DXMatrixScaling(&m_TextureMatrix,
					//				  800.0f/(float)width, 512.0f/(float)height, 1.0f);
					D3DXMatrixIdentity(&m_TextureMatrix);
					m_TextureMatrix._11 = 800.0f/(float)width;
					m_TextureMatrix._22 = 512.0f/(float)height;

					if (m_isFullScreen && m_MaintainAspectRatio)
					{
						// Aspect ratio adjustment
						if (m_XRatioAdj > 0.0f)
						{
							m_TextureMatrix._11 *= m_XRatioAdj;
							m_TextureMatrix._41 = m_XRatioCrop * 800.0f;
						}
						else if (m_YRatioAdj > 0.0f)
						{
							m_TextureMatrix._22 *= m_YRatioAdj;
							m_TextureMatrix._42 = m_YRatioCrop * -512.0f;
						}
					}
				}
				pSurface->Release();
				RenderDX9();
			}
			if (ddrval != D3D_OK)
			{
				char errstr[200];
				sprintf(errstr,
					"DirectX failure while updating screen\nFailure code %XSwitching to GDI\n",
					ddrval);
				MessageBox(m_hWnd,errstr,WindowTitle,MB_OK|MB_ICONERROR);
				PostMessage(m_hWnd, WM_COMMAND, IDM_DISPGDI, 0);
			}
		}
		else
		{
			// Blit the beeb bitmap onto the secondary buffer
			ddrval = m_DDS2One->GetDC(&hdc);
			if (ddrval == DDERR_SURFACELOST)
			{
				ddrval = m_DDS2One->Restore();
				if (ddrval == DD_OK)
					ddrval = m_DDS2One->GetDC(&hdc);
			}
			if (ddrval == DD_OK)
			{
				BitBlt(hdc, 0, 0, 800, nlines, m_hDCBitmap, 0, starty, SRCCOPY);
				DisplayClientAreaText(hdc);
				m_DDS2One->ReleaseDC(hdc);

				// Work out where on screen to blit image
				RECT destRect;
				RECT srcRect;
				POINT pt;
				GetClientRect( m_hWnd, &destRect );
				pt.x = pt.y = 0;
				ClientToScreen( m_hWnd, &pt );
				OffsetRect(&destRect, pt.x, pt.y);

				if (m_isFullScreen && m_MaintainAspectRatio)
				{
					// Aspect ratio adjustment
					int xAdj = (int)(m_XRatioCrop * (float)(destRect.right - destRect.left));
					int yAdj = (int)(m_YRatioCrop * (float)(destRect.bottom - destRect.top));
					destRect.left += xAdj;
					destRect.right -= xAdj;
					destRect.top += yAdj;
					destRect.bottom -= yAdj;
				}

				// Blit the whole of the secondary buffer onto the screen
				srcRect.left = 0;
				srcRect.top = 0;
				srcRect.right = (TeletextEnabled)?552:ActualScreenWidth;
				srcRect.bottom = (TeletextEnabled)?TTLines:nlines;
			
				ddrval = m_DDS2Primary->Blt( &destRect, m_DDS2One, &srcRect, DDBLT_ASYNC, NULL);
				if (ddrval == DDERR_SURFACELOST)
				{
					ddrval = m_DDS2Primary->Restore();
					if (ddrval == DD_OK)
						ddrval = m_DDS2Primary->Blt( &destRect, m_DDS2One, &srcRect, DDBLT_ASYNC, NULL );
				}
			}
		
			if (ddrval != DD_OK && ddrval != DDERR_WASSTILLDRAWING)
			{
				char errstr[200];
				sprintf(errstr,"DirectX failure while updating screen\nFailure code %X",ddrval);
				// Ignore DX errors for now - swapping between full screen and windowed DX 
				// apps causes an error while transitioning between display modes.  It
				// appears to correct itself after a second or two though.
				// MessageBox(m_hWnd,errstr,WindowTitle,MB_OK|MB_ICONERROR);
			}
		}
	}

	if (aviWriter)
	{
		StretchBlt(m_AviDC, 0, 0, m_Avibmi.bmiHeader.biWidth, m_Avibmi.bmiHeader.biHeight,
				   m_hDCBitmap, 0, starty, (TeletextEnabled)?552:ActualScreenWidth,
				   (TeletextEnabled==1)?TTLines:nlines, SRCCOPY);

		HRESULT hr = aviWriter->WriteVideo((BYTE*)m_AviScreen);
		if (hr != E_UNEXPECTED && FAILED(hr))
		{
			MessageBox(m_hWnd, "Failed to write video to AVI file",
					   WindowTitle, MB_OK|MB_ICONERROR);
			delete aviWriter;
			aviWriter = NULL;
		}
	}

	if (m_CaptureBitmapPending)
	{
		CaptureBitmap(0, starty, (TeletextEnabled)?552:ActualScreenWidth,
					  (TeletextEnabled==1)?TTLines:nlines);
		m_CaptureBitmapPending = false;
	}
}

/****************************************************************************/
void BeebWin::DisplayClientAreaText(HDC hdc)
{
	int TextStart=240;
	if (TeletextEnabled)
		TextStart=(480/TeletextStyle)-((TeletextStyle==2)?12:0);

	if ((DisplayCycles>0) && (hFDCBoard!=NULL))
	{
		SetBkMode(hdc,TRANSPARENT);
		SetTextColor(hdc,0x808080);
		TextOut(hdc,0,TextStart,ExtBoard.BoardName,(int)strlen(ExtBoard.BoardName));
	}

	if (m_ShowSpeedAndFPS && m_isFullScreen)
	{
		char fps[50];
		sprintf(fps, "%2.2f %2d", m_RelativeSpeed, (int)m_FramesPerSecond);
		SetBkMode(hdc,TRANSPARENT);
		SetTextColor(hdc,0x808080);
		TextOut(hdc,(TeletextEnabled)?490:580,TextStart,fps,(int)strlen(fps));
	}
}

/****************************************************************************/
void BeebWin::DisplayTiming(void)
{
	if (m_ShowSpeedAndFPS && (m_DisplayRenderer == IDM_DISPGDI || !m_isFullScreen))
	{
		sprintf(m_szTitle, "%s  Speed: %2.2f  fps: %2d",
				WindowTitle, m_RelativeSpeed, (int)m_FramesPerSecond);
		SetWindowText(m_hWnd, m_szTitle);
	}
}

/****************************************************************************/
void BeebWin::UpdateSmoothing(void)
{
	DDSURFACEDESC ddsd;
	HRESULT ddrval;

	if (m_DisplayRenderer == IDM_DISPDX9)
	{
		if (m_DXSmoothing && (!m_DXSmoothMode7Only || TeletextEnabled))
		{
			// Turn on bilinear interpolation so image is smoothed
			m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR );
			m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR );
		}
		else
		{
			// Turn off bilinear interpolation
			m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MINFILTER, D3DTEXF_POINT );
			m_pd3dDevice->SetSamplerState( 0, D3DSAMP_MAGFILTER, D3DTEXF_POINT );
		}
	}
	else
	{
		if (m_DDS2One)
		{
			m_DDS2One->Release();
			m_DDS2One = NULL;
		}
		if (m_DDSOne)
		{
			m_DDSOne->Release();
			m_DDSOne = NULL;
		}
		ZeroMemory(&ddsd, sizeof(ddsd));
		ddsd.dwSize = sizeof(ddsd);
		ddsd.dwFlags = DDSD_CAPS | DDSD_HEIGHT | DDSD_WIDTH;
		ddsd.ddsCaps.dwCaps = DDSCAPS_OFFSCREENPLAIN;
		if (m_DXSmoothing && (!m_DXSmoothMode7Only || TeletextEnabled))
			ddsd.ddsCaps.dwCaps |= DDSCAPS_VIDEOMEMORY;
		else
			ddsd.ddsCaps.dwCaps |= DDSCAPS_SYSTEMMEMORY;
		ddsd.dwWidth = 800;
		ddsd.dwHeight = 512;
		ddrval = m_DD2->CreateSurface(&ddsd, &m_DDSOne, NULL);
		if( ddrval == DD_OK )
		{
			ddrval = m_DDSOne->QueryInterface(IID_IDirectDrawSurface2, (LPVOID *)&m_DDS2One);
		}
	}
}
