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

#include <windows.h>
#include <ddraw.h>

#include <stdio.h>

#include "BeebWin.h"
#include "6502core.h"
#include "AviWriter.h"
#include "DebugTrace.h"
#include "Ext1770.h"
#include "Main.h"
#include "Messages.h"
#include "Resource.h"

// #define DEBUG_DX9

typedef HRESULT (WINAPI* LPDIRECTDRAWCREATE)(GUID FAR *lpGUID, LPDIRECTDRAW FAR *lplpDD, IUnknown FAR *pUnkOuter);

/****************************************************************************/

static void D3DMatrixIdentity(D3DMATRIX *pMatrix)
{
	pMatrix->_11 = 1.f;
	pMatrix->_12 = 0.f;
	pMatrix->_13 = 0.f;
	pMatrix->_14 = 0.f;

	pMatrix->_21 = 0.f;
	pMatrix->_22 = 1.f;
	pMatrix->_23 = 0.f;
	pMatrix->_24 = 0.f;

	pMatrix->_31 = 0.f;
	pMatrix->_32 = 0.f;
	pMatrix->_33 = 1.f;
	pMatrix->_34 = 0.f;

	pMatrix->_41 = 0.f;
	pMatrix->_42 = 0.f;
	pMatrix->_43 = 0.f;
	pMatrix->_44 = 1.f;
}

/****************************************************************************/

void BeebWin::InitDX()
{
	HRESULT hr = E_FAIL;

	if (m_DisplayRenderer == DisplayRendererType::DirectX9)
	{
		hr = InitD3DDevice();

		if (hr != D3D_OK)
		{
			Report(MessageType::Error, "DirectX9 initialisation failed\nFailure code %X\nTrying DirectDraw",
			       hr);

			PostMessage(m_hWnd, WM_COMMAND, IDM_DISPDDRAW, 0);
		}
	}
	else if (m_DisplayRenderer == DisplayRendererType::DirectDraw)
	{
		hr = InitDirectDraw();

		if (hr != D3D_OK)
		{
			Report(MessageType::Error, "DirectDraw initialisation failed\nFailure code %X\nSwitching to GDI",
			       hr);

			PostMessage(m_hWnd, WM_COMMAND, IDM_DISPGDI, 0);
		}
	}

	if (hr == D3D_OK)
	{
		m_CurrentDisplayRenderer = m_DisplayRenderer;
	}
}

/****************************************************************************/

void BeebWin::ResetDX()
{
	#ifdef DEBUG_DX9
	DebugTrace("BeebWin::ResetDX\n");
	#endif

	m_DXResetPending = false;

	if (m_CurrentDisplayRenderer == DisplayRendererType::DirectX9)
	{
		CloseD3DDevice();

		// Need to let message loop run before re-initialising DX otherwise
		// odd artifacts are seen when changing window size.
		PostMessage(m_hWnd, WM_REINITDX, 0, 0);
	}
	else if (m_CurrentDisplayRenderer == DisplayRendererType::DirectDraw)
	{
		CloseSurfaces();
		ReinitDX();
	}
}

/****************************************************************************/

void BeebWin::ReinitDX()
{
	#ifdef DEBUG_DX9
	DebugTrace("BeebWin::ReinitDX\n");
	#endif

	HRESULT hr = DD_OK;

	if (m_DisplayRenderer == DisplayRendererType::DirectX9)
	{
		hr = InitD3DDevice();
	}
	else if (m_DisplayRenderer == DisplayRendererType::DirectDraw)
	{
		hr = InitSurfaces();
	}

	if (hr != DD_OK)
	{
		Report(MessageType::Error, "DirectX failure re-initialising\nFailure code %X\nSwitching to GDI",
		       hr);

		PostMessage(m_hWnd, WM_COMMAND, IDM_DISPGDI, 0);
	}

	m_CurrentDisplayRenderer = m_DisplayRenderer;
}

/****************************************************************************/

void BeebWin::ExitDX()
{
	if (m_CurrentDisplayRenderer == DisplayRendererType::DirectX9)
	{
		CloseD3DDevice();
	}
	else if (m_CurrentDisplayRenderer == DisplayRendererType::DirectDraw)
	{
		CloseSurfaces();

		if (m_DD2 != nullptr)
		{
			m_DD2->Release();
			m_DD2 = nullptr;
		}

		if (m_DD != nullptr)
		{
			m_DD->Release();
			m_DD = nullptr;
		}

		if (m_hInstDDraw != nullptr)
		{
			FreeLibrary(m_hInstDDraw);
			m_hInstDDraw = nullptr;
		}
	}
}

/****************************************************************************/

HRESULT BeebWin::InitDirectDraw()
{
	HRESULT ddrval = DDERR_GENERIC;

	m_hInstDDraw = nullptr;
	m_DD = nullptr;
	m_DD2 = nullptr;
	m_Clipper = nullptr;
	m_DDS2One = nullptr;
	m_DDSOne = nullptr;
	m_DDS2Primary = nullptr;
	m_DDSPrimary = nullptr;

	m_hInstDDraw = LoadLibrary("ddraw.dll");

	if (m_hInstDDraw != nullptr)
	{
		LPDIRECTDRAWCREATE pDDCreate = (LPDIRECTDRAWCREATE)GetProcAddress(m_hInstDDraw, "DirectDrawCreate");

		if (pDDCreate != nullptr)
		{
			ddrval = pDDCreate(nullptr, &m_DD, nullptr);

			if (ddrval == DD_OK)
			{
				ddrval = m_DD->QueryInterface(IID_IDirectDraw2, (LPVOID *)&m_DD2);
			}

			if (ddrval == DD_OK)
			{
				ddrval = InitSurfaces();
			}
		}
	}

	return ddrval;
}

/****************************************************************************/

HRESULT BeebWin::InitSurfaces()
{
	HRESULT ddrval;

	if (m_FullScreen)
	{
		ddrval = m_DD2->SetCooperativeLevel(m_hWnd, DDSCL_EXCLUSIVE | DDSCL_FULLSCREEN);
	}
	else
	{
		ddrval = m_DD2->SetCooperativeLevel(m_hWnd, DDSCL_NORMAL);
	}

	if (ddrval == DD_OK)
	{
		if (m_FullScreen)
		{
			ddrval = m_DD2->SetDisplayMode(m_XDXSize, m_YDXSize, 32, 0, 0);
		}
	}

	if (ddrval == DD_OK)
	{
		DDSURFACEDESC ddsd;
		ddsd.dwSize = sizeof(ddsd);
		ddsd.dwFlags = DDSD_CAPS;
		ddsd.ddsCaps.dwCaps = DDSCAPS_PRIMARYSURFACE;
		ddrval = m_DD2->CreateSurface(&ddsd, &m_DDSPrimary, nullptr);
	}

	if (ddrval == DD_OK)
		ddrval = m_DDSPrimary->QueryInterface(IID_IDirectDrawSurface2, (LPVOID *)&m_DDS2Primary);
	if (ddrval == DD_OK)
		ddrval = m_DD2->CreateClipper(0, &m_Clipper, NULL);
	if (ddrval == DD_OK)
		ddrval = m_Clipper->SetHWnd(0, m_hWnd);
	if (ddrval == DD_OK)
		ddrval = m_DDS2Primary->SetClipper(m_Clipper);
	if (ddrval == DD_OK)
	{
		DDSURFACEDESC ddsd;
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
		ddrval = m_DD2->CreateSurface(&ddsd, &m_DDSOne, nullptr);
	}

	if (ddrval == DD_OK)
	{
		ddrval = m_DDSOne->QueryInterface(IID_IDirectDrawSurface2, (LPVOID *)&m_DDS2One);
	}

	if (ddrval == DD_OK)
	{
		m_DXInit = true;
	}

	return ddrval;
}

/****************************************************************************/

void BeebWin::CloseSurfaces()
{
	if (m_Clipper)
	{
		m_Clipper->Release();
		m_Clipper = nullptr;
	}

	if (m_DDS2One)
	{
		m_DDS2One->Release();
		m_DDS2One = nullptr;
	}

	if (m_DDSOne)
	{
		m_DDSOne->Release();
		m_DDSOne = nullptr;
	}

	if (m_DDS2Primary)
	{
		m_DDS2Primary->Release();
		m_DDS2Primary = nullptr;
	}

	if (m_DDSPrimary)
	{
		m_DDSPrimary->Release();
		m_DDSPrimary = nullptr;
	}

	m_DXInit = false;
}

/****************************************************************************/

bool BeebWin::InitDX9()
{
	#ifdef DEBUG_DX9
	DebugTrace("BeebWin::InitDX9\n");
	#endif

	// Create the D3D object.
	m_pD3D = Direct3DCreate9(D3D_SDK_VERSION);

	if (m_pD3D == nullptr)
	{
		return false;
	}

	#ifdef DEBUG_DX9

	D3DDISPLAYMODE d3dMode;
	UINT nModes = m_pD3D->GetAdapterModeCount(D3DADAPTER_DEFAULT, D3DFMT_X8R8G8B8);
	for (UINT Mode = 0; Mode < nModes; ++Mode)
	{
		m_pD3D->EnumAdapterModes(D3DADAPTER_DEFAULT, D3DFMT_X8R8G8B8,
		                         Mode, &d3dMode);
		DebugTrace("D3D Mode: %d x %d, refresh %d\n",
		           d3dMode.Width, d3dMode.Height, d3dMode.RefreshRate);
	}

	#endif

	return true;
}

/****************************************************************************/

void BeebWin::CloseDX9()
{
	#ifdef DEBUG_DX9
	DebugTrace("BeebWin::CloseDX9\n");
	#endif

	if (m_pD3D != nullptr)
	{
		m_pD3D->Release();
		m_pD3D = nullptr;
	}
}

/****************************************************************************/

HRESULT BeebWin::InitD3DDevice()
{
	#ifdef DEBUG_DX9
	DebugTrace("BeebWin::InitD3DDevice\n");
	#endif

	HRESULT hResult = D3D_OK;

	CUSTOMVERTEX* pVertices = nullptr;
	m_pd3dDevice = nullptr;
	m_pVB = nullptr;
	m_pTexture = nullptr;

	D3DMATRIX Ortho2D;
	D3DMATRIX Ident;

	// Create the D3D object on first use.
	if (m_pD3D == nullptr)
	{
		if (!InitDX9())
		{
			hResult = E_FAIL;
			goto Fail;
		}
	}

	// Find the monitor index based on the window BeebEm is currently on
	// as needed to pass to CreateDevice().
	HMONITOR hMonitor = MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST);

	UINT Adapter = D3DADAPTER_DEFAULT;
	UINT AdapterCount = m_pD3D->GetAdapterCount();

	for (UINT i = 0; i < AdapterCount; i++)
	{
		if (hMonitor == m_pD3D->GetAdapterMonitor(i))
		{
			Adapter = i;
			break;
		}
	}

	D3DCAPS9 DeviceCaps;
	m_pD3D->GetDeviceCaps(Adapter, D3DDEVTYPE_HAL, &DeviceCaps);

	D3DDISPLAYMODE DisplayMode;
    m_pD3D->GetAdapterDisplayMode(Adapter, &DisplayMode);

	// Set up the structure used to create the D3DDevice.
	D3DPRESENT_PARAMETERS d3dpp;
	ZeroMemory(&d3dpp, sizeof(d3dpp));

	d3dpp.Windowed = m_FullScreen ? FALSE : TRUE;

	if (d3dpp.Windowed)
	{
		d3dpp.BackBufferFormat = D3DFMT_UNKNOWN;
	}
	else
	{
		d3dpp.BackBufferWidth = m_XDXSize;
		d3dpp.BackBufferHeight = m_YDXSize;
		d3dpp.BackBufferFormat = D3DFMT_X8R8G8B8;
		d3dpp.FullScreen_RefreshRateInHz = DisplayMode.RefreshRate;
	}

	d3dpp.BackBufferCount = 1;
	d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	d3dpp.hDeviceWindow = m_hWnd;
	d3dpp.EnableAutoDepthStencil = FALSE;

	DWORD BehaviourFlags = 0;

	if (DeviceCaps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT)
	{
		BehaviourFlags |= D3DCREATE_HARDWARE_VERTEXPROCESSING;
	}
	else
	{
		BehaviourFlags |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;
	}

	// Create the D3DDevice
	hResult = m_pD3D->CreateDevice(Adapter,
	                               D3DDEVTYPE_HAL,
	                               m_hWnd, // hFocusWindow
	                               BehaviourFlags,
	                               &d3dpp,
	                               &m_pd3dDevice);

	if (hResult != D3D_OK)
	{
		#ifdef DEBUG_DX9
		DebugTrace("CreateDevice failed %08X\n", hResult);
		#endif

		goto Fail;
	}

	// Turn off D3D lighting
	m_pd3dDevice->SetRenderState(D3DRS_LIGHTING, FALSE);

	if (m_DXSmoothing && (!m_DXSmoothMode7Only || TeletextEnabled))
	{
		// Turn on bilinear interpolation so image is smoothed
		m_pd3dDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
		m_pd3dDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
	}

	// Just display a texture
	m_pd3dDevice->SetTextureStageState(0, D3DTSS_COLOROP, D3DTOP_SELECTARG1);
	m_pd3dDevice->SetTextureStageState(0, D3DTSS_COLORARG1, D3DTA_TEXTURE);

	DebugTrace("m_pD3D->CreateVertexBuffer\n");

	// Create the vertex buffer.
	hResult = m_pd3dDevice->CreateVertexBuffer(4 * sizeof(CUSTOMVERTEX),
		                                       0, // Usage
		                                       D3DFVF_CUSTOMVERTEX,
		                                       D3DPOOL_MANAGED,
		                                       &m_pVB,
		                                       nullptr);

	if (hResult != D3D_OK)
	{
		#ifdef DEBUG_DX9
		DebugTrace("CreateVertexBuffer failed %08X\n", hResult);
		#endif

		goto Fail;
	}

	// Fill the vertex buffer. We are setting the tu and tv texture
	// coordinates, which range from 0.0 to 1.0
	hResult = m_pVB->Lock(0, 0, (void**)&pVertices, 0);

	if (hResult != D3D_OK)
	{
		#ifdef DEBUG_DX9
		DebugTrace("VertexBuffer Lock failed %08X\n", hResult);
		#endif

		goto Fail;
	}

	pVertices[0].position = {0.0f, -511.0f, 0.0f};
	pVertices[0].color    = D3DCOLOR_RGBA(255, 255, 255, 0);
	pVertices[0].tu       = 0.0f;
	pVertices[0].tv       = 1.0f;

	pVertices[1].position = {0.0f, 0.0f, 0.0f};
	pVertices[1].color    = D3DCOLOR_RGBA(255, 255, 255, 0);
	pVertices[1].tu       = 0.0f;
	pVertices[1].tv       = 0.0f;

	pVertices[2].position = {799.0f, -511.0f, 0.0f};
	pVertices[2].color    = D3DCOLOR_RGBA(255, 255, 255, 0);
	pVertices[2].tu       = 1.0f;
	pVertices[2].tv       = 1.0f;

	pVertices[3].position = {799.0f, 0.0f, 0.0f};
	pVertices[3].color    = D3DCOLOR_RGBA(255, 255, 255, 0);
	pVertices[3].tu       = 1.0f;
	pVertices[3].tv       = 0.0f;

	m_pVB->Unlock();

	// Set up matrices
	//D3DXMatrixOrthoOffCenterLH(&Ortho2D, 0.0f, 800.0f, -512.0f, 0.0f, 0.0f, 1.0f);
	D3DMatrixIdentity(&Ortho2D);
	// const float l = 0.0f;
	const float r = 800.0f;
	const float b = -512.0f;
	const float t = 0.0f;
	const float zn = 0.0f;
	const float zf = 1.0f;
	Ortho2D._11 = 2.0f / (r - 1.0f);
	Ortho2D._22 = 2.0f / (t - b);
	Ortho2D._33 = 1.0f / (zf - zn);
	Ortho2D._41 = (1.0f + r) / (1.0f - r);
	Ortho2D._42 = (t + b) / (b - t);
	Ortho2D._43 = zn / (zn - zf);

	m_pd3dDevice->SetTransform(D3DTS_PROJECTION, &Ortho2D);

	D3DMatrixIdentity(&Ident);
	m_pd3dDevice->SetTransform(D3DTS_VIEW, &Ident);
	m_pd3dDevice->SetTransform(D3DTS_WORLD, &Ident);

	// Identity matrix will fill window with our texture
	D3DMatrixIdentity(&m_TextureMatrix);

	hResult = m_pd3dDevice->CreateTexture(800,
	                                      512,
	                                      1, // Levels
	                                      0, // Usage
	                                      D3DFMT_X8R8G8B8,
	                                      D3DPOOL_MANAGED,
	                                      &m_pTexture,
	                                      nullptr);

	if (hResult != D3D_OK)
	{
		#ifdef DEBUG_DX9
		DebugTrace("CreateTexture failed %08X\n", hResult);
		#endif

		goto Fail;
	}

	m_DXInit = true;

	return hResult;

Fail:
	CloseD3DDevice();

	return hResult;
}

/****************************************************************************/

void BeebWin::CloseD3DDevice()
{
	#ifdef DEBUG_DX9
	DebugTrace("BeebWin::CloseD3DDevice\n");
	#endif

	if (m_pTexture != nullptr)
	{
		m_pTexture->Release();
		m_pTexture = nullptr;
	}

	if (m_pVB != nullptr)
	{
		m_pVB->Release();
		m_pVB = nullptr;
	}

	if (m_pd3dDevice != nullptr)
	{
		m_pd3dDevice->Release();
		m_pd3dDevice = nullptr;
	}

	m_DXInit = false;
}

/****************************************************************************/

void BeebWin::RenderDX9()
{
	// Clear the backbuffer
	HRESULT hr = m_pd3dDevice->Clear(0, nullptr, D3DCLEAR_TARGET,
	                                 D3DCOLOR_XRGB(0,0,0), 1.0f, 0);

	// Begin the scene
	hr = m_pd3dDevice->BeginScene();

	if(hr == D3D_OK)
	{
		hr = m_pd3dDevice->SetStreamSource(0, m_pVB, 0, sizeof(CUSTOMVERTEX));
		if (hr == D3D_OK)
			hr = m_pd3dDevice->SetFVF(D3DFVF_CUSTOMVERTEX);
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
	{
		hr = m_pd3dDevice->Present(NULL, NULL, NULL, NULL);
	}

	if (hr == D3DERR_DEVICELOST)
	{
		#ifdef DEBUG_DX9
		DebugTrace("BeebWin::RenderDX9 - D3DERR_DEVICELOST\n");
		#endif

		// Can be generated when switching in/out of fullscreen - need to reset
		m_DXDeviceLost = true;
		PostMessage(m_hWnd, WM_DIRECTX9_DEVICE_LOST, 0, 0);
	}
}

/****************************************************************************/

// WM_DIRECTX9_DEVICE_LOST message handler.

void BeebWin::OnDeviceLost()
{
	#ifdef DEBUG_DX9
	DebugTrace("BeebWin::OnDeviceLost\n");
	#endif

	if (m_pd3dDevice != nullptr)
	{
		HRESULT hResult = m_pd3dDevice->TestCooperativeLevel();

		if (hResult == D3DERR_DEVICELOST)
		{
			#ifdef DEBUG_DX9
			DebugTrace("D3DERR_DEVICELOST\n");
			#endif

			// The device has been lost but cannot be reset at this time.
			// Therefore, rendering is not possible.
			::Sleep(100);
			PostMessage(m_hWnd, WM_DIRECTX9_DEVICE_LOST, 0, 0);
		}
		else if (hResult == D3DERR_DEVICENOTRESET)
		{
			#ifdef DEBUG_DX9
			DebugTrace("D3DERR_DEVICENOTRESET\n");
			#endif

			// The device has been lost but can be reset at this time.
			m_DXDeviceLost = false;
			ResetDX();
			ResetTiming();
		}
		else if (hResult == D3DERR_DRIVERINTERNALERROR)
		{
			#ifdef DEBUG_DX9
			DebugTrace("D3DERR_DRIVERINTERNALERROR\n");
			#endif

			PostMessage(m_hWnd, WM_COMMAND, IDM_DISPGDI, 0);
		}
	}
}

/****************************************************************************/

void BeebWin::updateLines(HDC hDC, int StartY, int NLines)
{
	static bool LastTeletextEnabled = false;
	static bool First = true;

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
	if (LastTeletextEnabled != TeletextEnabled || First)
	{
		if (m_DisplayRenderer != DisplayRendererType::GDI && m_DXSmoothing && m_DXSmoothMode7Only)
		{
			UpdateSmoothing();
		}

		LastTeletextEnabled = TeletextEnabled;
		First = false;
	}

	// Use last stored params?
	if (StartY == 0 && NLines == 0)
	{
		StartY = m_LastStartY;
		NLines = m_LastNLines;
	}
	else
	{
		m_LastStartY = StartY;
		m_LastNLines = NLines;
	}

	++m_ScreenRefreshCount;
	int TeletextLines = 500 / TeletextStyle;

	// Do motion blur
	if (m_MotionBlur != 0)
	{
		char j;

		if (m_MotionBlur == 2)
			j = 32;
		else if (m_MotionBlur == 4)
			j = 16;
		else // blur 8 frames
			j = 8;

		for (int i = 0; i < 800*512; ++i)
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

	if (m_DisplayRenderer == DisplayRendererType::GDI)
	{
		RECT destRect;
		GetClientRect(m_hWnd, &destRect);
		int win_nlines = destRect.bottom;
		int TextStart = win_nlines - 20;

		int xAdj = 0;
		int yAdj = 0;

		if (m_FullScreen && m_MaintainAspectRatio)
		{
			// Aspect ratio adjustment
			xAdj = (int)(m_XRatioCrop * (float)m_XWinSize);
			yAdj = (int)(m_YRatioCrop * (float)m_YWinSize);
		}

		StretchBlt(hDC, xAdj, yAdj, m_XWinSize - xAdj * 2, win_nlines - yAdj * 2,
		           m_hDCBitmap, 0, StartY,
		           TeletextEnabled ? 552 : ActualScreenWidth,
		           TeletextEnabled ? TeletextLines : NLines,
		           SRCCOPY);

		DisplayFDCBoardInfo(hDC, 0, TextStart);
	}
	else
	{
		if (!m_DXInit)
			return;

		if (IsWindowMinimized())
			return;

		if (m_DisplayRenderer == DisplayRendererType::DirectX9)
		{
			if (m_DXDeviceLost) return;

			IDirect3DSurface9 *pSurface;
			HRESULT ddrval = m_pTexture->GetSurfaceLevel(0, &pSurface);

			if (ddrval == D3D_OK)
			{
				HDC hdc;
				ddrval = pSurface->GetDC(&hdc);

				if (ddrval == D3D_OK)
				{
					BitBlt(hdc, 0, 0, 800, NLines, m_hDCBitmap, 0, StartY, SRCCOPY);
					DisplayClientAreaText(hdc);
					pSurface->ReleaseDC(hdc);

					// Scale beeb screen to fill the D3D texture
					int width  = TeletextEnabled ? 552 : ActualScreenWidth;
					int height = TeletextEnabled ? TeletextLines : NLines;
					// D3DXMatrixScaling(&m_TextureMatrix,
					//                   800.0f / (float)width, 512.0f / (float)height, 1.0f);
					D3DMatrixIdentity(&m_TextureMatrix);
					m_TextureMatrix._11 = 800.0f/(float)width;
					m_TextureMatrix._22 = 512.0f/(float)height;

					if (m_FullScreen && m_MaintainAspectRatio)
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
				Report(MessageType::Error, "DirectX failure while updating screen\nFailure code %X\nSwitching to GDI",
				       ddrval);

				PostMessage(m_hWnd, WM_COMMAND, IDM_DISPGDI, 0);
			}
		}
		else
		{
			// Blit the beeb bitmap onto the secondary buffer
			HDC hdc;
			HRESULT ddrval = m_DDS2One->GetDC(&hdc);

			if (ddrval == DDERR_SURFACELOST)
			{
				ddrval = m_DDS2One->Restore();

				if (ddrval == DD_OK)
					ddrval = m_DDS2One->GetDC(&hdc);
			}

			if (ddrval == DD_OK)
			{
				BitBlt(hdc, 0, 0, 800, NLines, m_hDCBitmap, 0, StartY, SRCCOPY);
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

				if (m_FullScreen && m_MaintainAspectRatio)
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
				srcRect.left   = 0;
				srcRect.top    = 0;
				srcRect.right  = TeletextEnabled ? 552 : ActualScreenWidth;
				srcRect.bottom = TeletextEnabled ? TeletextLines : NLines;

				ddrval = m_DDS2Primary->Blt(&destRect, m_DDS2One, &srcRect, DDBLT_ASYNC, NULL);
				if (ddrval == DDERR_SURFACELOST)
				{
					ddrval = m_DDS2Primary->Restore();
					if (ddrval == DD_OK)
						ddrval = m_DDS2Primary->Blt(&destRect, m_DDS2One, &srcRect, DDBLT_ASYNC, NULL);
				}
			}

			if (ddrval != DD_OK && ddrval != DDERR_WASSTILLDRAWING)
			{
				// Ignore DX errors for now - swapping between full screen and windowed DX
				// apps causes an error while transitioning between display modes.
				// It appears to correct itself after a second or two though.
				// Report(MessageType::Error,
				//        "DirectX failure while updating screen\nFailure code %X", ddrval);
			}
		}
	}

	if (aviWriter != nullptr)
	{
		StretchBlt(m_AviDC, 0, 0, m_Avibmi.bmiHeader.biWidth, m_Avibmi.bmiHeader.biHeight,
		           m_hDCBitmap, 0, StartY,
		           TeletextEnabled ? 552 : ActualScreenWidth,
		           TeletextEnabled ? TeletextLines : NLines,
		           SRCCOPY);

		HRESULT hr = aviWriter->WriteVideo((BYTE*)m_AviScreen);

		if (FAILED(hr) && hr != E_UNEXPECTED)
		{
			Report(MessageType::Error, "Failed to write video to AVI file");

			EndVideo();
		}
	}

	if (m_CaptureBitmapPending)
	{
		CaptureBitmap(0,
		              StartY,
		              TeletextEnabled ? 552 : ActualScreenWidth,
		              TeletextEnabled ? TeletextLines : NLines,
		              TeletextEnabled);

		m_CaptureBitmapPending = false;
	}
}

/****************************************************************************/

bool BeebWin::IsWindowMinimized() const
{
	WINDOWPLACEMENT wndpl;
	wndpl.length = sizeof(WINDOWPLACEMENT);

	if (GetWindowPlacement(m_hWnd, &wndpl))
	{
		if (wndpl.showCmd == SW_SHOWMINIMIZED)
			return true;
	}

	return false;
}

/****************************************************************************/

void BeebWin::DisplayClientAreaText(HDC hdc)
{
	int TextStart=240;
	if (TeletextEnabled)
		TextStart=(480/TeletextStyle)-((TeletextStyle==2)?12:0);

	DisplayFDCBoardInfo(hdc, 0, TextStart);

	if (m_ShowSpeedAndFPS && m_FullScreen)
	{
		char fps[50];
		if (!m_EmuPaused)
			sprintf(fps, "%2.2f %2d", m_RelativeSpeed, (int)m_FramesPerSecond);
		else
			sprintf(fps, "Paused");
		SetBkMode(hdc,TRANSPARENT);
		SetTextColor(hdc,0x808080);
		TextOut(hdc, TeletextEnabled ? 490 : 580, TextStart, fps, (int)strlen(fps));
	}
}

/****************************************************************************/

void BeebWin::DisplayFDCBoardInfo(HDC hDC, int x, int y)
{
	if (DisplayCycles > 0 && HasFDCBoard())
	{
		const char* BoardName = GetFDCBoardName();

		SetBkMode(hDC, TRANSPARENT);
		SetTextColor(hDC, 0x808080);
		TextOut(hDC, x, y, BoardName, (int)strlen(BoardName));
	}
}

/****************************************************************************/

static const char* pszReleaseCaptureMessage = "(Press Ctrl+Alt to release mouse)";

bool BeebWin::ShouldDisplayTiming() const
{
	return m_ShowSpeedAndFPS && (m_DisplayRenderer == DisplayRendererType::GDI || !m_FullScreen);
}

void BeebWin::DisplayTiming()
{
	if (ShouldDisplayTiming())
	{
		if (m_MouseCaptured)
		{
			sprintf(m_szTitle, "%s  Speed: %2.2f  fps: %2d  %s",
			        WindowTitle, m_RelativeSpeed, (int)m_FramesPerSecond, pszReleaseCaptureMessage);
		}
		else
		{
			sprintf(m_szTitle, "%s  Speed: %2.2f  fps: %2d",
			        WindowTitle, m_RelativeSpeed, (int)m_FramesPerSecond);
		}

		SetWindowText(m_hWnd, m_szTitle);
	}
}

void BeebWin::UpdateWindowTitle()
{
	if (ShouldDisplayTiming())
	{
		DisplayTiming();
	}
	else
	{
		if (m_MouseCaptured)
		{
			sprintf(m_szTitle, "%s  %s",
			        WindowTitle, pszReleaseCaptureMessage);
		}
		else
		{
			strcpy(m_szTitle, WindowTitle);
		}

		SetWindowText(m_hWnd, m_szTitle);
	}
}

/****************************************************************************/

void BeebWin::UpdateSmoothing()
{
	if (m_DisplayRenderer == DisplayRendererType::DirectX9)
	{
		if (m_DXSmoothing && (!m_DXSmoothMode7Only || TeletextEnabled))
		{
			// Turn on bilinear interpolation so image is smoothed
			m_pd3dDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_LINEAR);
			m_pd3dDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_LINEAR);
		}
		else
		{
			// Turn off bilinear interpolation
			m_pd3dDevice->SetSamplerState(0, D3DSAMP_MINFILTER, D3DTEXF_POINT);
			m_pd3dDevice->SetSamplerState(0, D3DSAMP_MAGFILTER, D3DTEXF_POINT);
		}
	}
	else
	{
		if (m_DDS2One)
		{
			m_DDS2One->Release();
			m_DDS2One = nullptr;
		}

		if (m_DDSOne)
		{
			m_DDSOne->Release();
			m_DDSOne = nullptr;
		}

		DDSURFACEDESC ddsd;
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

		HRESULT ddrval = m_DD2->CreateSurface(&ddsd, &m_DDSOne, NULL);

		if (ddrval == DD_OK)
		{
			ddrval = m_DDSOne->QueryInterface(IID_IDirectDrawSurface2, (LPVOID *)&m_DDS2One);
		}
	}
}

/****************************************************************************/

void BeebWin::SetMotionBlur(int MotionBlur)
{
	m_MotionBlur = MotionBlur;

	UpdateMotionBlurMenu();
}

void BeebWin::UpdateMotionBlurMenu()
{
	static const struct { UINT ID; int MotionBlur; } MenuItems[] =
	{
		{ IDM_BLUR_OFF, 0 },
		{ IDM_BLUR_2,   2 },
		{ IDM_BLUR_4,   4 },
		{ IDM_BLUR_8,   8 }
	};

	UINT SelectedMenuItemID = 0;

	for (int i = 0; i < _countof(MenuItems); i++)
	{
		if (m_MotionBlur == MenuItems[i].MotionBlur)
		{
			SelectedMenuItemID = MenuItems[i].ID;
			break;
		}
	}

	CheckMenuRadioItem(IDM_BLUR_OFF, IDM_BLUR_8, SelectedMenuItemID);
}

/****************************************************************************/
