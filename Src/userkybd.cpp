/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 1997  Laurie Whiffen

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

/* User defined keyboard funcitonality. */

#include <stdio.h>
#include <windows.h>
#include <string.h>
#include "main.h"
#include "beebemrc.h"
#include "userkybd.h"

#define	IDD_GETKEY  400
#define gkWidth		200
#define	gkHeight	90

BOOL	UserKeyboardDialog( HWND  hwndParent );
HBRUSH	InitButtonColour( HWND hWndCtrl, HDC hDC );
void	SetKeyColour( COLORREF aColour );
void	SetBBCKeyForVKEY( int Key, int shift );
void	ShowKeyDown( HWND hwnd, UINT ctrlID, HWND hWndCtrl );
void	SetRowCol( UINT ctrlID );
void	ShowKeyUp( void );
BOOL	CALLBACK UserKeyboard_DlgProc( HWND   hwnd,
									   UINT   nMessage,
									   WPARAM wParam,
									   LPARAM lParam );
LRESULT CALLBACK GetKeyWndProc( HWND hWnd,	
								UINT message,
								WPARAM uParam,
								LPARAM lParam);
void	OnDrawItem( UINT CtrlID, LPDRAWITEMSTRUCT lpDrawItemStruct );
void	DrawBorder( HDC hDC, RECT rect, BOOL Depressed );
void	DrawSides( HDC hDC, RECT rect, COLORREF TopLeft, COLORREF BottomRight );
void	DrawText( HDC hDC, RECT rect, HWND hWndctrl, COLORREF colour, BOOL Depressed );
COLORREF GetKeyColour( UINT ctrlID );
HWND	PromptForInput( HWND hwndParent, int doShiftedKey );
void	GetKeysUsed( LPSTR Keys );
LPSTR	KeyName( UINT Key );


// Colour used to highlight the selected key.
static COLORREF Highlight	= 0x00FF0080;	// Purple.
static COLORREF FunctionKey = 0x000000FF;	// Red
static COLORREF NormalKey	= 0x00000000;	// Black


COLORREF OldColour;		// Holds bkCOlor of non-highlighted key.
HWND	hWndBBCKey;		// Holds the BBCKey control handle which is now selected.
UINT    selectedCtrlID; // Holds ctrlId of selected key
HWND	hwndMain;		// Holds the BeebWin window Handle.
HWND	hwndGetkey;		// Holds the window handle of the Getkey prompt window.
HWND	hwndText;
HWND	hwndCtrl;       // OK button handle
HWND	hwndShift;      // Shift check box handle

int		BBCRow;			// Used to store the Row and Col values while we wait 
int		BBCCol;			// for a key press from the User.
BOOL	WaitingForKey = FALSE;	// True while waiting for a key to be pressed.
HBRUSH	hFunctionBrush;	// The brush that has to be passed back for coloured keys.
HFONT	hGetkeyFont;	// The Font used by the Getkey prompt window.
int		doingShifted;   // Selecting shifted or unshifted key press

// Initialised to defaultMapping
KeyMap UserKeymap;

/****************************************************************************/
BOOL UserKeyboardDialog( HWND      hwndParent )

{
    FARPROC pfnDlg;
    BOOL    fResult;

	// Set up Function key Brush.

	hFunctionBrush = CreateSolidBrush( FunctionKey );
    
	//  Invoke the dialog.

    pfnDlg = MakeProcInstance( (FARPROC)UserKeyboard_DlgProc, hInst );

    // Initialise locals used during this windows life.
	hwndMain = hwndParent;
	hwndGetkey = NULL;
	selectedCtrlID = 0;

	fResult = (DialogBox( hInst,
                         MAKEINTRESOURCE( IDD_USERKYBRD ),
                         hwndParent,
                         (DLGPROC)pfnDlg ) <= 0);

    //  Cleanup & exit.

    FreeProcInstance( pfnDlg );
	DeleteObject( hFunctionBrush );

    return fResult;

}   // UserKeyboardDialog
/****************************************************************************/

void SetKeyColour( COLORREF aColour )
{
	
	HDC hdc = GetDC( hWndBBCKey );
	OldColour = SetBkColor( hdc, aColour );
	ReleaseDC( hWndBBCKey, hdc );
	InvalidateRect(hWndBBCKey, NULL, TRUE);
	UpdateWindow(hWndBBCKey);


} // SetKeyColor

/****************************************************************************/

void ShowKeyUp( void )
{
	if ( WaitingForKey  )
	{
		WaitingForKey = FALSE;

		// Show the key as not depressed, ie normal.
		SetKeyColour( OldColour );
    }

} // ShowKeyUp.

/****************************************************************************/

void ShowKeyDown( HWND hwnd, UINT ctrlID, HWND hWndCtrl )
{
	// If we are already waiting for a Key then reset the bkcolor for that
	// key.
	if ( WaitingForKey )
	{
		selectedCtrlID = 0;
		SetKeyColour( OldColour );
	}
	
	// Set the place holders.
	SetRowCol( ctrlID );

	hWndBBCKey  = hWndCtrl;
	selectedCtrlID = ctrlID;
	WaitingForKey = TRUE;
	
	// Now ask the user to input he PC key to assign to the BBC key.
	if ( hwndGetkey != NULL )
		SendMessage( hwndGetkey, WM_CLOSE, 0, 0L );

	// Start the Getkey prompt window.
	hwndGetkey = PromptForInput( hwnd, 0 );

} // ShowKeyDown

/****************************************************************************/
void SetBBCKeyForVKEY( int Key, int shift )
{
	if (Key >= 0 && Key < 256)
	{
		UserKeymap[Key][shift].row = BBCRow;
		UserKeymap[Key][shift].col = BBCCol;
		UserKeymap[Key][shift].shift = doingShifted;

		//char info[256];
		//sprintf(info, "SetBBCKey: key=%d, shift=%d, row=%d, col=%d, bbcshift=%d\n",
		//		Key, shift, BBCRow, BBCCol, doingShifted);
		//OutputDebugString(info);
	}

} // SetBBCKeyForVKEY


/****************************************************************************/

BOOL CALLBACK UserKeyboard_DlgProc( HWND   hwnd,
									UINT   nMessage,
									WPARAM wParam,
									LPARAM lParam )
{
/*
 UINT nID = LOWORD(wParam);
  
// You extract the remaining two values in this way in the 16-bit framework:
  
HWND hWndCtrl = (HWND)LOWORD(lParam);    	//Control handle
int nCode = HIWORD(lParam);	              //Notification code
  
// You extract them this way in the 32-bit framework:
  
HWND hWndCtrl = (HWND)lParam;	           //Control handle
int nCode = HIWORD(wParam);	             //Notification code
*/
	switch( nMessage )
	{
	// 
	case WM_COMMAND:
		switch( wParam )
		{
		case IDOK:
		case IDCANCEL:
			EndDialog( hwnd, TRUE );
			break;
		default:
			ShowKeyDown( hwnd, (UINT)wParam, (HWND)lParam );
		};
		return TRUE;

	case WM_DRAWITEM:
		// Draw the key.
		OnDrawItem( (UINT)wParam, (LPDRAWITEMSTRUCT) lParam );
		return TRUE;

	default:
		return FALSE;
	};		

	return FALSE;
}   // UserKeyboard_DlgProc

/****************************************************************************/

void DrawSides( HDC hDC, RECT rect, COLORREF TopLeft, COLORREF BottomRight )
{
	HPEN tl, br;
	POINT	point;

	tl = HPEN(CreatePen( PS_SOLID, 1, TopLeft ));
	br = CreatePen( PS_SOLID, 1, BottomRight );

	tl = HPEN(SelectObject( hDC, tl )); // Shadow now contains the original.

	MoveToEx( hDC, rect.left, rect.bottom-1, &point );
	LineTo( hDC, rect.left, rect.top );
	LineTo( hDC, rect.right-1, rect.top );

	br = HPEN(SelectObject( hDC, br ));
	LineTo( hDC, rect.right-1, rect.bottom-1 );
	LineTo( hDC, rect.left, rect.bottom-1 );

	// Clean up.
	DeleteObject( br );
	DeleteObject( SelectObject( hDC, tl ));


} // DrawSides

/****************************************************************************/

void DrawBorder( HDC hDC, RECT rect, BOOL Depressed )
{
	// Draw outer border.
	if (Depressed)
		DrawSides( hDC, rect, 0x00000000, 0x00FFFFFF );
	else
		DrawSides( hDC, rect, 0x00FFFFFF, 0x00000000 );
	
	// Draw inner border.
	rect.top++;
	rect.left++;
	rect.right--;
	rect.bottom--;

	if (Depressed)
		DrawSides( hDC, rect, 0x00777777, 0x00FFFFFF );
	else
		DrawSides( hDC, rect, 0x00FFFFFF, 0x00777777 );

} // DrawBorder.


/****************************************************************************/

void DrawText( HDC hDC, RECT rect, HWND hWndctrl, COLORREF colour, BOOL Depressed )
{
	SIZE Size;
	CHAR text[10];

	GetWindowText( hWndctrl, text, 9 );

	if (GetTextExtentPoint32( hDC, text, (int)strlen(text), &Size ))
	{
		// Set text colour.
		SetTextColor( hDC, colour );

		// Output text.
		if ( Depressed )
			TextOut( hDC, 
				 	 (((rect.right - rect.left) - Size.cx ) / 2 ) + 1, 
					 (((rect.bottom-rect.top) - Size.cy) / 2) + 1, 
					 text, 
					 (int)strlen( text ));
		else
			TextOut( hDC, 
				 	 ((rect.right - rect.left) - Size.cx ) / 2, 
					 ((rect.bottom-rect.top) - Size.cy) / 2, 
					 text, 
					 (int)strlen( text ));
	}

} // DrawText

/****************************************************************************/

COLORREF GetKeyColour( UINT ctrlID )
{
	if (WaitingForKey && selectedCtrlID == ctrlID)
		return Highlight;

	switch ( ctrlID )
	{
	case IDK_F0:
	case IDK_F1:
	case IDK_F2:
	case IDK_F3:
	case IDK_F4:
	case IDK_F5:
	case IDK_F6:
	case IDK_F7:
	case IDK_F8:
	case IDK_F9:
		return FunctionKey;

	default:
		return NormalKey;
	}
} // GetKeyColour

/****************************************************************************/

void OnDrawItem( UINT CtrlID, LPDRAWITEMSTRUCT lpDrawItemStruct )
{
	// set the Pen and Backgorund Brush.
	HBRUSH aBrush = CreateSolidBrush( GetKeyColour( CtrlID ) );
	HPEN aPen = CreatePen( PS_NULL, 1, 0x00000000 );

	// Copy into the Device Context.
	aBrush = HBRUSH(SelectObject( lpDrawItemStruct->hDC, aBrush ));
	aPen = HPEN(SelectObject( lpDrawItemStruct->hDC, aPen ));

	// Draw the rectanlge.
	SetBkColor( lpDrawItemStruct->hDC,GetKeyColour( CtrlID ) ); 
	Rectangle(	lpDrawItemStruct->hDC, 
				lpDrawItemStruct->rcItem.left,
				lpDrawItemStruct->rcItem.top,
				lpDrawItemStruct->rcItem.right,
				lpDrawItemStruct->rcItem.bottom );

	// Draw border.
	DrawBorder( lpDrawItemStruct->hDC,
				lpDrawItemStruct->rcItem, 
				( lpDrawItemStruct->itemState == ( ODS_FOCUS | ODS_SELECTED )) );
		

	// Draw the text.
	DrawText( lpDrawItemStruct->hDC,
			  lpDrawItemStruct->rcItem,
			  lpDrawItemStruct->hwndItem,
			  0x00FFFFFF,
			  ( lpDrawItemStruct->itemState == ( ODS_FOCUS | ODS_SELECTED )) ); 


	// Clear up.
	DeleteObject( SelectObject( lpDrawItemStruct->hDC, aBrush ) );
	DeleteObject( SelectObject( lpDrawItemStruct->hDC, aPen ) );

} // OnDrawItem

/****************************************************************************/

HWND PromptForInput( HWND hwndParent, int doShiftedKey )
{
	int Error;
	HWND hwnd;
	WNDCLASS  wc;
	CHAR	szClass[12] = "BEEBGETKEY";
	CHAR	*szTitle1 = "Press key for unshifted press...";
	CHAR	*szTitle2 = "Press key for shifted press...";

	doingShifted = doShiftedKey;

	// Fill in window class structure with parameters that describe the
	// main window, if it doesn't already exist.
	if (!GetClassInfo( hInst, szClass, &wc ))
	{

		wc.style		 = CS_HREDRAW | CS_VREDRAW;// Class style(s).
		wc.lpfnWndProc	 = (WNDPROC)GetKeyWndProc;	   // Window Procedure
		wc.cbClsExtra	 = 0;					   // No per-class extra data.
		wc.cbWndExtra	 = 0;					   // No per-window extra data.
		wc.hInstance	 = hInst;				   // Owner of this class
		wc.hIcon		 = LoadIcon(hInst, MAKEINTRESOURCE(IDI_BEEBEM));
		wc.hCursor		 = NULL;
		wc.hbrBackground = (HBRUSH)(COLOR_BTNFACE+1);// Default color
		wc.lpszMenuName  = NULL; // Menu from .RC
		wc.lpszClassName = "BEEBGETKEY"; //szAppName;				// Name to register as

		// Register the window class and return success/failure code.
		(RegisterClass(&wc));
	}

	hwnd = CreateWindow(	szClass,	// pointer to registered class name
							doShiftedKey ? szTitle2 : szTitle1,	// pointer to window name
							WS_OVERLAPPED|				  
							WS_CAPTION| DS_MODALFRAME | DS_SYSMODAL,
//				WS_SYSMENU|
//				WS_MINIMIZEBOX, // Window style.			    
							// WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | WS_CAPTION,
							//WS_SYSMENU, // Window style.			    
							80, 40,
							gkWidth,	// window width
							gkHeight,	// window height
							hwndParent,	// handle to parent or owner window
							NULL,//HMENU(IDD_GETKEY),	// handle to menu or child-window identifier
							hInst,	// handle to application instance
							NULL // pointer to window-creation data

							);
	if ( hwnd == NULL )
		Error = GetLastError();
	else
		ShowWindow( hwnd, SW_SHOW );
	
	return hwnd;
}

/****************************************************************************/

LRESULT CALLBACK GetKeyWndProc( HWND hWnd,		   // window handle
								UINT message,	   // type of message
								WPARAM uParam,	   // additional information
								LPARAM lParam)	   // additional information
{
#define IDI_TEXT 100
	BOOL checkShifted = FALSE;
	int shift;

	switch( message )
	{
	case WM_CREATE:
		// Add the parameters required for this window. ie a Stic text control
		// and a cancel button.

		CHAR szUsedKeys[80];

		// Change Window Font.
		PostMessage( hWnd, WM_SETFONT, (WPARAM)GetStockObject( ANSI_VAR_FONT ), 
					 MAKELPARAM(FALSE, 0) );

		// Create the static text for keys used
		hwndCtrl = CreateWindow( "STATIC", "Assigned to PC key(s): ", WS_CHILD | SS_SIMPLE | WS_VISIBLE,
								 4, 4, 
								 gkWidth-10, 16, hWnd, HMENU(IDI_TEXT), 
								 ((LPCREATESTRUCT)lParam)->hInstance, NULL );
		PostMessage( hwndCtrl, WM_SETFONT, (WPARAM)GetStockObject( ANSI_VAR_FONT ), 
					 MAKELPARAM(FALSE, 0) );

		// The keys used list.
		GetKeysUsed( szUsedKeys );
		CharToOem( szUsedKeys, szUsedKeys );
		hwndCtrl = CreateWindow( "STATIC", szUsedKeys, WS_CHILD | SS_SIMPLE | WS_VISIBLE,
								 8, 20, 
								 gkWidth-10, 16, hWnd, HMENU(IDI_TEXT), 
								 ((LPCREATESTRUCT)lParam)->hInstance, NULL );
		PostMessage( hwndCtrl, WM_SETFONT, (WPARAM)GetStockObject( ANSI_VAR_FONT ), 
					 MAKELPARAM(FALSE, 0) );

		// Create the OK button.
		hwndCtrl = CreateWindow( "BUTTON", "&Ok", WS_CHILD | BS_DEFPUSHBUTTON | WS_VISIBLE, 
								 gkWidth - 80, gkHeight - 50, 
								 60, 18,
								 hWnd, HMENU(IDOK), 
								 ((LPCREATESTRUCT)lParam)->hInstance, NULL );

		PostMessage( hwndCtrl, WM_SETFONT, (WPARAM)GetStockObject( ANSI_VAR_FONT ), 
					 MAKELPARAM(FALSE, 0) );

		// Create the shift checkbox.
		hwndShift = CreateWindow( "BUTTON", "&Shift", WS_CHILD | BS_AUTOCHECKBOX | WS_VISIBLE, 
								 20, gkHeight - 50, 
								 70, 18,
								 hWnd, NULL, 
								 ((LPCREATESTRUCT)lParam)->hInstance, NULL );

		PostMessage( hwndShift, WM_SETFONT, (WPARAM)GetStockObject( ANSI_VAR_FONT ), 
					 MAKELPARAM(FALSE, 0) );
	break;

	case WM_COMMAND:
		if ((HWND)lParam == hwndCtrl)
		{
			// Respond to the OK button click
			PostMessage( hWnd, WM_CLOSE, 0, 0L );
			checkShifted = TRUE;
		}
		else // shift checkbox
		{
			// Do not let checkbox have focus
			SetFocus( hWnd );
		}
		break;

	case WM_SYSKEYUP:
	case WM_KEYUP:
		if (uParam != 255)
		{
			// Assign the BBC key to the PC key.
			shift = (SendMessage(hwndShift, BM_GETCHECK, 0, 0) == BST_CHECKED) ? 1 : 0;
			SetBBCKeyForVKEY( (int) uParam, shift);
		
			// Close the window.
			PostMessage( hWnd, WM_CLOSE, 0, 0L );

			checkShifted = TRUE;
		}
		break;

	default:
		// Let the default procedure ehandle everything else.
		return DefWindowProc( hWnd, message, uParam, lParam );
	}

	if (checkShifted)
	{
		selectedCtrlID = 0;

		// Do shifted key
		if (!doingShifted)
			hwndGetkey = PromptForInput( GetParent(hWnd), 1 );
	}

	return FALSE; // Return zero because we have processed this message.
}

/****************************************************************************/

void GetKeysUsed( LPSTR Keys )
{
	int i;
	int s;

	Keys[0] = '\0';

	// First see if this key is defined.
	if ((BBCRow == 0 ) && (BBCCol == 0 ))
		strcpy( Keys, "Not Assigned" );
	else
	{
		for( i=1; i<256; i++ )
		{
			for ( s=0; s<2; ++s)
			{
				if ((UserKeymap[i][s].row == BBCRow) &&
					(UserKeymap[i][s].col == BBCCol) &&
					(UserKeymap[i][s].shift == doingShifted))
				{
					// We have found a key that matches.
					if (strlen( Keys ) != 0 )
						strcat( Keys,  ", " );
					if (s == 1)
						strcat( Keys, "Sh-" );
					strcat( Keys, KeyName(i) );
				}
			}
		}
		if ( strlen( Keys ) == 0 )
			strcpy( Keys, "Not Assigned" );
	}

}

/****************************************************************************/

LPSTR KeyName( UINT Key )
{
	static CHAR Character[2]; // Used to return single characters.

	switch( Key )
	{
	case   8: return "Backspace";
	case   9: return "Tab";
	case  13: return "Enter";
	case  17: return "Ctrl";
	case  18: return "Alt";
	case  19: return "Break";
	case  20: return "Caps";
	case  27: return "Esc";
	case  32: return "Spacebar";
	case  33: return "PgUp";
	case  34: return "PgDn";
	case  35: return "End";
	case  36: return "Home";
	case  37: return "Left";
	case  38: return "Up";
	case  39: return "Right";
	case  40: return "Down";
	case  45: return "Insert";
	case  46: return "Del";
	case  93: return "Menu";
	case  96: return "Pad0";
	case  97: return "Pad1";
	case  98: return "Pad2";
	case  99: return "Pad3";
	case 100: return "Pad4";
	case 101: return "Pad5";
	case 102: return "Pad6";
	case 103: return "Pad7";
	case 104: return "Pad8";
	case 105: return "Pad9";
	case 106: return "Pad*";
	case 107: return "Pad+";
	case 109: return "Pad-";
	case 110: return "Pad.";
	case 111: return "Pad/";
	case 112: return "F1";
	case 113: return "F2";
	case 114: return "F3";
	case 115: return "F4";
	case 116: return "F5";
	case 117: return "F6";
	case 118: return "F7";
	case 119: return "F8";
	case 120: return "F9";
	case 121: return "F10";
	case 122: return "F11";
	case 123: return "F12";
	case 144: return "NumLock";
	case 145: return "SclLock";
	case 186: return ";";
	case 187: return "=";
	case 188: return ",";
	case 189: return "-";
	case 190: return ".";
	case 191: return "/";
	case 192: return "\'";
	case 219: return "[";
	case 220: return "\\";
	case 221: return "]";
	case 222: return "#";
	case 223: return "`";

	default:
		Character[0] = (CHAR) LOBYTE( Key );
		Character[1] = '\0';
		return Character;
	}

}

/****************************************************************************/

void SetRowCol( UINT ctrlID )
{
	switch( ctrlID )
	{
	// Character keys.
	case IDK_A: BBCRow = 4; BBCCol = 1; break;
	case IDK_B: BBCRow = 6; BBCCol = 4; break;
	case IDK_C: BBCRow = 5; BBCCol = 2; break;
	case IDK_D: BBCRow = 3; BBCCol = 2; break;
	case IDK_E: BBCRow = 2; BBCCol = 2; break;
	case IDK_F: BBCRow = 4; BBCCol = 3; break;
	case IDK_G: BBCRow = 5; BBCCol = 3; break;
	case IDK_H: BBCRow = 5; BBCCol = 4; break;
	case IDK_I: BBCRow = 2; BBCCol = 5; break;
	case IDK_J: BBCRow = 4; BBCCol = 5; break;
	case IDK_K: BBCRow = 4; BBCCol = 6; break;
	case IDK_L: BBCRow = 5; BBCCol = 6; break;
	case IDK_M: BBCRow = 6; BBCCol = 5; break;
	case IDK_N: BBCRow = 5; BBCCol = 5; break;
	case IDK_O: BBCRow = 3; BBCCol = 6; break;
	case IDK_P: BBCRow = 3; BBCCol = 7; break;
	case IDK_Q: BBCRow = 1; BBCCol = 0; break;
	case IDK_R: BBCRow = 3; BBCCol = 3; break;
	case IDK_S: BBCRow = 5; BBCCol = 1; break;
	case IDK_T: BBCRow = 2; BBCCol = 3; break;
	case IDK_U: BBCRow = 3; BBCCol = 5; break;
	case IDK_V: BBCRow = 6; BBCCol = 3; break;
	case IDK_W: BBCRow = 2; BBCCol = 1; break;
	case IDK_X: BBCRow = 4; BBCCol = 2; break;
	case IDK_Y: BBCRow = 4; BBCCol = 4; break;
	case IDK_Z: BBCRow = 6; BBCCol = 1; break;

		
	// Number keys.
	case IDK_0: BBCRow = 2; BBCCol = 7; break;
	case IDK_1: BBCRow = 3; BBCCol = 0; break;
	case IDK_2: BBCRow = 3; BBCCol = 1; break;
	case IDK_3: BBCRow = 1; BBCCol = 1; break;
	case IDK_4: BBCRow = 1; BBCCol = 2; break;
	case IDK_5: BBCRow = 1; BBCCol = 3; break;
	case IDK_6: BBCRow = 3; BBCCol = 4; break;
	case IDK_7: BBCRow = 2; BBCCol = 4; break;
	case IDK_8: BBCRow = 1; BBCCol = 5; break;
	case IDK_9: BBCRow = 2; BBCCol = 6; break;

	// Function keys.
	case IDK_F0: BBCRow = 2; BBCCol = 0; break;
	case IDK_F1: BBCRow = 7; BBCCol = 1; break;
	case IDK_F2: BBCRow = 7; BBCCol = 2; break;
	case IDK_F3: BBCRow = 7; BBCCol = 3; break;
	case IDK_F4: BBCRow = 1; BBCCol = 4; break;
	case IDK_F5: BBCRow = 7; BBCCol = 4; break;
	case IDK_F6: BBCRow = 7; BBCCol = 5; break;
	case IDK_F7: BBCRow = 1; BBCCol = 6; break;
	case IDK_F8: BBCRow = 7; BBCCol = 6; break;
	case IDK_F9: BBCRow = 7; BBCCol = 7; break;

	// Special keys.
	case IDK_LEFT:	BBCRow = 1; BBCCol = 9; break;
	case IDK_RIGHT: BBCRow = 7; BBCCol = 9; break;
	case IDK_UP:	BBCRow = 3; BBCCol = 9; break;
	case IDK_DOWN:	BBCRow = 2; BBCCol = 9; break;
	case IDK_BREAK: BBCRow = -2; BBCCol = -2; break;
	case IDK_COPY:	BBCRow = 6; BBCCol = 9; break;
	case IDK_DEL:	BBCRow = 5; BBCCol = 9; break;
	case IDK_CAPS:	BBCRow = 4; BBCCol = 0; break;
	case IDK_TAB:	BBCRow = 6; BBCCol = 0; break;
	case IDK_CTRL:	BBCRow = 0; BBCCol = 1; break;
	case IDK_SPACE: BBCRow = 6; BBCCol = 2; break;
	case IDK_RETURN:BBCRow = 4; BBCCol = 9; break;
	case IDK_ESC:	BBCRow = 7; BBCCol = 0; break;
	case IDK_SHIFT_L: BBCRow = 0; BBCCol = 0; break;
	case IDK_SHIFT_R: BBCRow = 0; BBCCol = 0; break;
	case IDK_SHIFT_LOCK: BBCRow = 5; BBCCol = 0; break;

	//Special Character keys.
	case IDK_SEMI_COLON:	BBCRow = 5; BBCCol = 7; break;
	case IDK_EQUALS:		BBCRow = 1; BBCCol = 7; break;
	case IDK_COMMA:			BBCRow = 6; BBCCol = 6; break;
	case IDK_CARET:			BBCRow = 1; BBCCol = 8; break;
	case IDK_DOT:			BBCRow = 6; BBCCol = 7; break;
	case IDK_FWDSLASH:		BBCRow = 6; BBCCol = 8; break;
	case IDK_STAR:			BBCRow = 4; BBCCol = 8; break;
	case IDK_OPEN_SQUARE:	BBCRow = 3; BBCCol = 8; break;
	case IDK_BACKSLASH:		BBCRow = 7; BBCCol = 8; break;
	case IDK_CLOSE_SQUARE:	BBCRow = 5; BBCCol = 8; break;
	case IDK_AT:			BBCRow = 4; BBCCol = 7; break;
	case IDK_UNDERSCORE:	BBCRow = 2; BBCCol = 8; break;
	

	default:
		BBCRow = 0; BBCCol = 0;
	}
}
