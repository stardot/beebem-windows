// beebwin.cpp 
// NRM's port to win32

#include <windows.h>
#include "main.h"
#include "beebwin.h"

#include "port.h"


#include "6502core.h"
#include "disc8271.h"
#include "sysvia.h"

LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
int TranslateKey(int, int *, int *);

// Row,Col
static int transTable[256][2]={
	0,0,	0,0,	0,0,	0,0,   // 0
	0,0,	0,0,	0,0,	0,0,   // 4
	5,9,	6,0,	0,0,	0,0,   // 8 [BS][TAB]..
	0,0,	4,9,	0,0,	0,0,   // 12 .RET..
	0,0,	0,1,	0,0,	-2,-2,   // 16 .CTRL.BREAK
	4,0,	0,0,	0,0,	0,0,   // 20 CAPS...
	0,0,	0,0,	0,0,	7,0,   // 24 ...ESC
	0,0,	0,0,	0,0,	0,0,   // 28
	6,2,	0,0,	0,0,	6,9,   // 32 SPACE..[End]
	0,0,	1,9,	3,9,	7,9,   // 36 .[Left][Up][Right]
	2,9,	0,0,	0,0,	0,0,   // 40 [Down]...
	0,0,	0,0,	5,9,	0,0,   // 44 ..[DEL].
	2,7,	3,0,	3,1,	1,1,   // 48 0123	
	1,2,	1,3,	3,4,	2,4,   // 52 4567
	1,5,	2,6,	0,0,	0,0,   // 56 89
	0,0,	0,0,	0,0,	0,0,   // 60
	0,0,	4,1,	6,4,	5,2,   // 64.. ABC
	3,2,	2,2,	4,3,	5,3,   // 68  DEFG
	5,4,	2,5,	4,5,	4,6,   // 72  HIJK
	5,6,	6,5,	5,5,	3,6,   // 76  LMNO
	3,7,	1,0,	3,3,	5,1,   // 80  PQRS
	2,3,	3,5,	6,3,	2,1,   // 84  TUVW
	4,2,	4,4,	6,1,	0,0,   // 88  XYZ
	0,0,	0,0,	0,0,	0,0,   // 92
	0,0,	0,0,	0,0,	0,0,   // 96
	0,0,	0,0,	0,0,	0,0,   // 100
	0,0,	0,0,	0,0,	0,0,   // 104
	0,0,	0,0,	0,0,	0,0,   // 108
	7,1,	7,2,	7,3,	1,4,   // 112 F1 F2 F3 F4
	7,4,	7,5,	1,6,	7,6,   // 116 F5 F6 F7 F8
	7,7,	2,0,	2,0,	-2,-2, // 120 F9 F10 F11 F12
	0,0,	0,0,	0,0,	0,0,   // 124 
	0,0,	0,0,	0,0,	0,0,   // 128
	0,0,	0,0,	0,0,	0,0,   // 132
	0,0,	0,0,	0,0,	0,0,   // 136
	0,0,	0,0,	0,0,	0,0,   // 140
	0,0,	0,0,	0,0,	0,0,   // 144
	0,0,	0,0,	0,0,	0,0,   // 148
	0,0,	0,0,	0,0,	0,0,   // 152
	0,0,	0,0,	0,0,	0,0,   // 156
	0,0,	0,0,	0,0,	0,0,   // 160
	0,0,	0,0,	0,0,	0,0,   // 164
	0,0,	0,0,	0,0,	0,0,   // 168
	0,0,	0,0,	0,0,	0,0,   // 172
	0,0,	0,0,	0,0,	0,0,   // 176
	0,0,	0,0,	0,0,	0,0,   // 180
	0,0,	0,0,	5,7,	1,8,   // 184  ..;=
	6,6,	1,7,	6,7,	6,8,   // 188  ,-./
	4,8,	0,0,	0,0,	0,0,   // 192  @ ...
	0,0,	0,0,	0,0,	0,0,   // 196
	0,0,	0,0,	0,0,	0,0,   // 200
	0,0,	0,0,	0,0,	0,0,   // 204
	0,0,	0,0,	0,0,	0,0,   // 208
	0,0,	0,0,	0,0,	0,0,   // 212
	0,0,	0,0,	0,0,	3,8,   // 216 ...[
	7,8,	5,8,	2,8,	0,0,   // 220 .][#~].
	0,0,	0,0,	0,0,	0,0,   // 224
	0,0,	0,0,	0,0,	0,0,   // 228
	0,0,	0,0,	0,0,	0,0,   // 232
	0,0,	0,0,	0,0,	0,0, 
	0,0,	0,0,	0,0,	0,0, 
	0,0,	0,0,	0,0,	0,0, 
	0,0,	0,0,	0,0,	0,0, 
	0,0,	0,0,	0,0,	0,0 
};
					
BeebWin::BeebWin()
{	
	for(int i=0;i<8;i++)
		cols[i] = 20+i;		// Initialise BEEB Colours; make this =i
							// if you aren't using wing (hence no palettes)
	
	m_screen = beebdisplay.m_bitmapbits;   // Take a copy of the pointer to the screen

	InitClass();
	CreateBeebWindow();	
	m_hDC = GetDC(m_hWnd);
}

BeebWin::~BeebWin()
{	
	ReleaseDC(m_hWnd, m_hDC);

}

char * BeebWin::imageData()
{
	return m_screen;
}

int BeebWin::bytesPerLine() const
{
	return 320;
}

void BeebWin::updateLines(int starty, int nlines)
{
	HDC	hDC;	
	
	//hDC = GetDC(m_hWnd);
			
	beebdisplay.Blit(m_hDC, starty, nlines);

	//ReleaseDC(m_hWnd,hDC);
}

EightUChars * BeebWin::GetLinePtr(int y)
{
    if(y > 255) y=255;
	return((EightUChars *)(m_screen + ( y * bytesPerLine() )) );
	
}

SixteenUChars * BeebWin::GetLinePtr16(int y)
{
    if(y > 255) y=255;
	return((SixteenUChars *)(m_screen + ( y * bytesPerLine() )) );
	
}

BOOL BeebWin::InitClass(void)
{
	    WNDCLASS  wc;

        // Fill in window class structure with parameters that describe the
        // main window.

        wc.style         = CS_HREDRAW | CS_VREDRAW;// Class style(s).
        wc.lpfnWndProc   = (WNDPROC)WndProc;       // Window Procedure
        wc.cbClsExtra    = 0;                      // No per-class extra data.
        wc.cbWndExtra    = 0;                      // No per-window extra data.
        wc.hInstance     = hInst;              	   // Owner of this class
        wc.hIcon         = NULL; //LoadIcon (hInstance, MAKEINTRESOURCE(IDI_APP)); // Icon name from .RC
        wc.hCursor       = LoadCursor(NULL, IDC_ARROW);// Cursor
        wc.hbrBackground = (HBRUSH)(COLOR_WINDOW+1);// Default color
        wc.lpszMenuName  = NULL; //MAKEINTRESOURCE(IDR_GENERIC); // Menu from .RC
        wc.lpszClassName = "BEEBWIN"; //szAppName;              // Name to register as

        // Register the window class and return success/failure code.
        return (RegisterClass(&wc));
	
}

void BeebWin::CreateBeebWindow(void)
{
	m_hWnd = CreateWindow(
                "BEEBWIN",           	// See RegisterClass() call.
                "BBC Emulator",         // Text for window title bar.
                WS_OVERLAPPED|                
                WS_CAPTION|
				WS_SYSMENU|
                WS_MINIMIZEBOX,	// Window style.                
                CW_USEDEFAULT, 0,		// Use default positioning
                326,
                261+1+GetSystemMetrics(SM_CYCAPTION) , 
                NULL,                	// Overlapped windows have no parent.
                NULL,                // Use the window class menu.
                hInst,           // This instance owns this window.
                NULL                 // We don't use any data in our WM_CREATE
        ); 

	ShowWindow(m_hWnd, SW_SHOW); // Show the window
    UpdateWindow(m_hWnd);         // Sends WM_PAINT message
}

LRESULT CALLBACK WndProc(
                HWND hWnd,         // window handle
                UINT message,      // type of message
                WPARAM uParam,     // additional information
                LPARAM lParam)     // additional information
{
        FARPROC lpProcAbout;  // pointer to the "About" function
        int wmId, wmEvent;
		HDC		hdc;
		UINT	f;

        switch (message) {
			int	row, col;

                case WM_COMMAND:  // message: command from application menu

                        wmId    = LOWORD(uParam);
                        wmEvent = HIWORD(uParam);

				break;                        
                
				case WM_PALETTECHANGED:
				if(!mainWin)
					break;
			    	if ((HWND)uParam == hWnd)
			    		break;

			    // fall through to WM_QUERYNEWPALETTE

			  	case WM_QUERYNEWPALETTE:
				if(!mainWin)
					break;

			    	hdc = GetDC(hWnd);
			    	mainWin->beebdisplay.RealizePalette(hdc);				    	
			    	ReleaseDC(hWnd,hdc);	
			    	return TRUE;		    				    
			    break;


                case WM_PAINT:
				if(mainWin != NULL) {
					PAINTSTRUCT	ps;
					HDC			hDC, hDCScreen;

					hDC = BeginPaint(hWnd, &ps);
					mainWin->beebdisplay.RealizePalette(hDC);	
								
					mainWin->beebdisplay.Blit(hDC, 0, 256);

					EndPaint(hWnd, &ps);
				}
				break;
                
                case WM_KEYDOWN:
;					if(TranslateKey(uParam, &row, &col)>=0)
						BeebKeyDown(row, col);
				break;

				case WM_KEYUP:
					if(TranslateKey(uParam,&row, &col)>=0)
						BeebKeyUp(row, col);
					else
						if(row==-2)
						{ // Must do a reset!
							Init6502core();
      						Disc8271_reset();
						}
				break;
                              
                case WM_DESTROY:  // message: window being destroyed
                        PostQuitMessage(0);
                        break;

                default:          // Passes it on if unproccessed
                        return (DefWindowProc(hWnd, message, uParam, lParam));
        }
        return (0);
}

int TranslateKey(int vkey, int *row, int *col)
{
	
	row[0] = transTable[vkey][0];
	col[0] = transTable[vkey][1];	

    return(row[0]);
}

