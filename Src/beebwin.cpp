/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 1994  Nigel Magnay
Copyright (C) 1997  Mike Wyatt
Copyright (C) 1998  Robert Schmidt
Copyright (C) 2001  Richard Gellman
Copyright (C) 2004  Ken Lowe
Copyright (C) 2004  Rob O'Donnell
Copyright (C) 2005  Jon Welch

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

// 07/06/1997: Mike Wyatt and NRM's port to Win32
// 11/01/1998: Conveted to use DirectX, Mike Wyatt
// 28/12/2004: Econet added Rob O'Donnell. robert@irrelevant.com.
// 26/12/2011: Added IDE Drive to Hardware options, JGH

#include <stdio.h>
#include <windows.h>
#include <initguid.h>
#include <shlobj.h>
#include <shlwapi.h>
#include <gdiplus.h>
#include "main.h"
#include "beebwin.h"
#include "port.h"
#include "6502core.h"
#include "disc8271.h"
#include "disc1770.h"
#include "sysvia.h"
#include "uservia.h"
#include "video.h"
#include "beebsound.h"
#include "beebmem.h"
#include "beebemrc.h"
#include "atodconv.h"
#include "userkybd.h"
#include "serial.h"
#include "econet.h"	 // Rob O'Donnell Christmas 2004.
#include "tube.h"
#include "ext1770.h"
#include "uefstate.h"
#include "debug.h"
#include "scsi.h"
#include "sasi.h"
#include "ide.h"
#include "z80mem.h"
#include "z80.h"
#include "userkybd.h"
#ifdef SPEECH_ENABLED
#include "speech.h"
#endif
#include "teletext.h"
#include "avi.h"
#include "csw.h"
#include "serialdevices.h"
#include "Arm.h"
#include "version.h"

using namespace Gdiplus;

#define WIN_STYLE (WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX|WS_MAXIMIZEBOX|WS_SIZEBOX)

// some LED based macros
#define LED_COL_BASE 64

static char *CFG_REG_KEY = "Software\\BeebEm";

#ifdef M512COPRO_ENABLED
void i86_main(void);
#endif

FILE *CMDF2;
unsigned char CMA2;
CArm *arm = NULL;

unsigned char HideMenuEnabled;
unsigned char DisableMenu = 0;
bool MenuOn = true;

struct LEDType LEDs;
char DiscLedColour=0; // 0 for red, 1 for green.

AVIWriter *aviWriter = NULL;

// FDC Board extension DLL variables
HMODULE hFDCBoard;

EDCB ExtBoard={0,0,NULL};
bool DiscLoaded[2]={FALSE,FALSE}; // Set to TRUE when a disc image has been loaded.
char CDiscName[2][256]; // Filename of disc current in drive 0 and 1;
char CDiscType[2]; // Current disc types
char FDCDLL[256]={0};

const char *WindowTitle = "BeebEm - BBC Model B / Master 128 Emulator";
static const char *AboutText =
	"BeebEm - Emulating:\n\nBBC Micro Model B\nBBC Micro Model B + IntegraB\n"
	"BBC Micro Model B Plus (128)\nAcorn Master 128\n\n"
	"Acorn 65C02 Second Processor\n"
	"Torch Z80 Second Processor\nAcorn Z80 Second Processor\n"
#ifdef M512COPRO_ENABLED
	"Master 512 Second Processor\n"
#endif
	"ARM Second Processor\n\n"
	"Version " VERSION_STRING ", Feb 2012";

/* Prototypes */
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

// Keyboard mappings
static KeyMap defaultMapping;
static KeyMap logicalMapping;

/* Currently selected translation table */
static KeyMap *transTable = &defaultMapping;

/****************************************************************************/
BeebWin::BeebWin()
{   
	m_DXInit = FALSE;
	m_hWnd = NULL;
	m_LastStartY = 0;
	m_LastNLines = 256;
	m_LastTickCount = 0;
	m_KeyMapAS = 0;
	m_KeyMapFunc = 0;
	m_ShiftPressed = 0;
	m_ShiftBooted = false;
	for (int k = 0; k < 256; ++k)
	{
		m_vkeyPressed[k][0][0] = -1;
		m_vkeyPressed[k][1][0] = -1;
		m_vkeyPressed[k][0][1] = -1;
		m_vkeyPressed[k][1][1] = -1;
	}
	m_DisableKeysWindows=0;
	m_DisableKeysBreak=0;
	m_DisableKeysEscape=0;
	m_DisableKeysShortcut=0;
	memset(&defaultMapping, 0, sizeof(KeyMap));
	memset(&logicalMapping, 0, sizeof(KeyMap));
	memset(&UserKeymap, 0, sizeof(KeyMap));
	memset(m_UserKeyMapPath, 0, sizeof(m_UserKeyMapPath));
	m_hBitmap = m_hOldObj = m_hDCBitmap = NULL;
	m_screen = m_screen_blur = NULL;
	m_ScreenRefreshCount = 0;
	m_RelativeSpeed = 1;
	m_FramesPerSecond = 50;
	strcpy(m_szTitle, WindowTitle);
	m_AviDC = NULL;
	m_AviDIB = NULL;
	m_CaptureBitmapPending = false;
	m_SpVoice = NULL;
	m_hTextView = NULL;
	m_frozen = FALSE;
	IgnoreIllegalInstructions = 1;
	aviWriter = NULL;
	for(int i=0;i<256;i++)
		cols[i] = i;
	m_WriteProtectDisc[0] = !IsDiscWritable(0);
	m_WriteProtectDisc[1] = !IsDiscWritable(1);
	UEFTapeName[0]=0;
	m_AutoSavePrefsCMOS = false;
	m_AutoSavePrefsFolders = false;
	m_AutoSavePrefsAll = false;
	m_AutoSavePrefsChanged = false;
	memset(m_KbdCmd, 0, sizeof(m_KbdCmd));
	memset(m_DebugScript, 0, sizeof(m_DebugScript));
	m_KbdCmdPos = -1;
	m_KbdCmdPress = false;
	m_KbdCmdDelay = 40;
	m_KbdCmdLastCycles = 0;
	m_NoAutoBoot = false;
	m_clipboardlen = 0;
	m_clipboardptr = 0;
	m_printerbufferlen = 0;
	m_translateCRLF = true;
	m_CurrentDisplayRenderer = 0;
	m_DXSmoothing = true;
	m_DXSmoothMode7Only = false;
	m_DXResetPending = false;
	m_JoystickCaptured = false;
	m_customip[0] = 0;
	m_customport = 0;
	m_isFullScreen = false;
	m_MaintainAspectRatio = true;
	m_startFullScreen = false;
	m_XDXSize = 640;
	m_YDXSize = 480;
	m_XScrSize = GetSystemMetrics(SM_CXSCREEN);
	m_YScrSize = GetSystemMetrics(SM_CYSCREEN);
	m_XWinBorder = GetSystemMetrics(SM_CXSIZEFRAME) * 2;
	m_YWinBorder = GetSystemMetrics(SM_CYSIZEFRAME) * 2 +
		GetSystemMetrics(SM_CYMENUSIZE) +
		GetSystemMetrics(SM_CYCAPTION) + 1;

	/* Get the applications path - used for non-user files */
	char app_path[_MAX_PATH];
	char app_drive[_MAX_DRIVE];
	char app_dir[_MAX_DIR];
	GetModuleFileName(NULL, app_path, _MAX_PATH);
	_splitpath(app_path, app_drive, app_dir, NULL, NULL);
	_makepath(m_AppPath, app_drive, app_dir, NULL, NULL);

	// Read user data path from registry
	if (!RegGetStringValue(HKEY_CURRENT_USER, CFG_REG_KEY, "UserDataFolder",
						   m_UserDataPath, _MAX_PATH))
	{
		// Default user data path to a sub-directory in My Docs
		if (SHGetFolderPath(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, m_UserDataPath) == NOERROR)
		{
			strcat(m_UserDataPath, "\\BeebEm\\");
		}
	}

	// Read disc images path from registry
	if (!RegGetStringValue(HKEY_CURRENT_USER, CFG_REG_KEY, "DiscsPath",
						   m_DiscPath, _MAX_PATH))
	{
		// Default disc images path to a sub-directory of UserData path
		strcpy(m_DiscPath, m_UserDataPath);
		strcat(m_DiscPath, "DiscIms\\");
	}

	// Set default files, may be overridden by command line parameters.
	strcpy(m_PrefsFile, "Preferences.cfg");
	strcpy(RomFile, "Roms.cfg");
}

/****************************************************************************/
void BeebWin::Initialise()
{   
	// Parse command line
	ParseCommandLine();
	FindCommandLineFile(m_CommandLineFileName1);
	FindCommandLineFile(m_CommandLineFileName2);
	CheckForLocalPrefs(m_CommandLineFileName1, false);

	// Check that user data directory exists
	if (CheckUserDataPath() == false)
		exit(-1);

	LoadPreferences();

	// Override full screen?
	if (m_startFullScreen)
	{
		m_isFullScreen = true;
	}

	if (FAILED(CoInitialize(NULL)))
		MessageBox(m_hWnd,"Failed to initialise COM\n",WindowTitle,MB_OK|MB_ICONERROR);

	// Init Windows controls
	INITCOMMONCONTROLSEX cc;
	cc.dwSize = sizeof(cc);
	cc.dwICC = ICC_LISTVIEW_CLASSES;
	InitCommonControlsEx(&cc);

	GdiplusStartupInput gdiplusStartupInput;
	GdiplusStartup(&m_gdiplusToken, &gdiplusStartupInput, NULL);

	InitClass();
	CreateBeebWindow(); 
	CreateBitmap();

	m_hMenu = GetMenu(m_hWnd);
	m_hDC = GetDC(m_hWnd);

	ReadROMFile(RomFile, RomConfig);
	ApplyPrefs();

	if(strcmp(m_DebugScript,"\0") != 0)
	{
		DebugOpenDialog(hInst, m_hWnd);
		DebugRunScript(m_DebugScript);
	}

	// Boot file if passed on command line
	HandleCommandLineFile(1, m_CommandLineFileName2);
	HandleCommandLineFile(0, m_CommandLineFileName1);

	// Schedule first key press if keyboard command supplied
	if (m_KbdCmd[0] != 0)
		SetTimer(m_hWnd, 1, 1000, NULL);
}

/****************************************************************************/
void BeebWin::ApplyPrefs()
{
	// Set up paths
	strcpy(EconetCfgPath, m_UserDataPath);
	strcpy(RomPath, m_UserDataPath);
	strcpy(DiscPath, m_DiscPath);

	// Load key maps
	char keymap[_MAX_PATH];
	strcpy(keymap, "Logical.kmap");
	GetDataPath(m_UserDataPath, keymap);
	ReadKeyMap(keymap, &logicalMapping);
	strcpy(keymap, "Default.kmap");
	GetDataPath(m_UserDataPath, keymap);
	ReadKeyMap(keymap, &defaultMapping);

	InitMenu();
	ShowMenu(TRUE);

	ExitDX();
	if (m_DisplayRenderer != IDM_DISPGDI)
		InitDX();

	InitTextToSpeech();
	InitTextView();

	/* Initialise printer */
	if (PrinterEnabled)
		PrinterEnable(m_PrinterDevice);
	else
		PrinterDisable();

	/* Joysticks can only be initialised after the window is created (needs hwnd) */
	if (m_MenuIdSticks == IDM_JOYSTICK)
		InitJoystick();

	LoadFDC(NULL, true);
	RTCInit();

	SoundReset();
	if (SoundDefault) SoundInit();
	SetSoundMenu();
#ifdef SPEECH_ENABLED
	if (SpeechDefault)
		tms5220_start();
#endif

	// Serial init
	if (IP232custom)
	{
		strcpy(IPAddress,IP232customip);
		PortNo = IP232customport;
	}
	else
	{
		strcpy(IPAddress,"127.0.0.1");
		PortNo = 25232;
	}
	if (SerialPortEnabled) {
		if (TouchScreenEnabled) 
			TouchScreenOpen();

		if (EthernetPortEnabled && (IP232localhost || IP232custom))
		{
			if (IP232Open() == false)
			{
				MessageBox(GETHWND,"Serial IP232 could not connect to specified address",
						   WindowTitle,MB_OK|MB_ICONERROR);
				bSerialStateChanged=TRUE;
				SerialPortEnabled=FALSE;
				UpdateSerialMenu(m_hMenu);
			}
		}
	}

	ResetBeebSystem(MachineType,TubeEnabled,1); 

	// Rom write flags
	for (int slot = 0; slot < 16; ++slot)
	{
		if (!RomWritePrefs[slot])
			RomWritable[slot] = 0;
	}
	SetRomMenu();
}

/****************************************************************************/
BeebWin::~BeebWin()
{   
	if (m_DisplayRenderer != IDM_DISPGDI)
		ExitDX();

	ReleaseDC(m_hWnd, m_hDC);

	if (m_hOldObj != NULL)
		SelectObject(m_hDCBitmap, m_hOldObj);
	if (m_hBitmap != NULL)
		DeleteObject(m_hBitmap);
	if (m_hDCBitmap != NULL)
		DeleteDC(m_hDCBitmap);

	GdiplusShutdown(m_gdiplusToken);

	CoUninitialize();
}

/****************************************************************************/
void BeebWin::Shutdown()
{   
	if (aviWriter)
		delete aviWriter;

	if (m_AutoSavePrefsCMOS || m_AutoSavePrefsFolders ||
		m_AutoSavePrefsAll || m_AutoSavePrefsChanged)
		SavePreferences(m_AutoSavePrefsAll);

	if (SoundEnabled)
		SoundReset();

	if (m_SpVoice)
	{
		m_SpVoice->Release();
		m_SpVoice = NULL;
	}

	if (mEthernetHandle > 0) 
	{
		IP232Close();
		mEthernetHandle = 0;
	}
}

/****************************************************************************/
void BeebWin::ResetBeebSystem(unsigned char NewModelType,unsigned char TubeStatus,unsigned char LoadRoms) 
{
	SwitchOnCycles=0; // Reset delay
	SoundReset();
	if (SoundDefault)
		SoundInit();
	SwitchOnSound();
	EnableTube=TubeStatus;
	MachineType=NewModelType;
	BeebMemInit(LoadRoms,m_ShiftBooted);
	Init6502core();
	if (EnableTube) Init65C02core();
#ifdef M512COPRO_ENABLED
	if (Tube186Enabled) i86_main();
#endif
    Enable_Z80 = 0;
    if (TorchTube || AcornZ80)
    {
       R1Status = 0;
       ResetTube();
       init_z80();
       Enable_Z80 = 1;
    }
	Enable_Arm = 0;
	if (ArmTube)
	{
		R1Status = 0;
		ResetTube();
		if (arm) delete arm;
		arm = new CArm;
		Enable_Arm = 1;
	}

	SysVIAReset();
	UserVIAReset();
	VideoInit();
	SetDiscWriteProtects();
	Disc8271_reset();
	if (EconetEnabled) EconetReset();	//Rob:
	Reset1770();
	AtoDInit();
	SetRomMenu();
	FreeDiscImage(0);
	// Keep the disc images loaded
	FreeDiscImage(1);
	Close1770Disc(0);
	Close1770Disc(1);
	if (SCSIDriveEnabled) SCSIReset();
	if (SCSIDriveEnabled) SASIReset();
	if (IDEDriveEnabled)  IDEReset();
	TeleTextInit();
	if (MachineType==3) InvertTR00=FALSE;
	if (MachineType!=3) {
		LoadFDC(NULL, false);
	}
	if ((MachineType!=3) && (NativeFDC)) {
		// 8271 disc
		if ((DiscLoaded[0]) && (CDiscType[0]==0)) LoadSimpleDiscImage(CDiscName[0],0,0,80);
		if ((DiscLoaded[0]) && (CDiscType[0]==1)) LoadSimpleDSDiscImage(CDiscName[0],0,80);
		if ((DiscLoaded[1]) && (CDiscType[1]==0)) LoadSimpleDiscImage(CDiscName[1],1,0,80);
		if ((DiscLoaded[1]) && (CDiscType[1]==1)) LoadSimpleDSDiscImage(CDiscName[1],1,80);
	}
	if (((MachineType!=3) && (!NativeFDC)) || (MachineType==3)) {
		// 1770 Disc
		if (DiscLoaded[0]) Load1770DiscImage(CDiscName[0],0,CDiscType[0],m_hMenu);
		if (DiscLoaded[1]) Load1770DiscImage(CDiscName[1],1,CDiscType[1],m_hMenu);
	}
}

/****************************************************************************/
void BeebWin::CreateBitmap()
{
	if (m_hBitmap != NULL)
		DeleteObject(m_hBitmap);
	if (m_hDCBitmap != NULL)
		DeleteDC(m_hDCBitmap);
	if (m_screen_blur != NULL)
		free(m_screen_blur);

	m_hDCBitmap = CreateCompatibleDC(NULL);

	m_bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	m_bmi.bmiHeader.biWidth = 800;
	m_bmi.bmiHeader.biHeight = -512;
	m_bmi.bmiHeader.biPlanes = 1;
	m_bmi.bmiHeader.biBitCount = 8;
	m_bmi.bmiHeader.biXPelsPerMeter = 0;
	m_bmi.bmiHeader.biYPelsPerMeter = 0;
	m_bmi.bmiHeader.biCompression = BI_RGB;
	m_bmi.bmiHeader.biSizeImage = 800*512;
	m_bmi.bmiHeader.biClrUsed = 68;
	m_bmi.bmiHeader.biClrImportant = 68;

#ifdef USE_PALETTE
	__int16 *pInts = (__int16 *)&m_bmi.bmiColors[0];
    
	for(int i=0; i<12; i++)
		pInts[i] = i;
    
	m_hBitmap = CreateDIBSection(m_hDCBitmap, (BITMAPINFO *)&m_bmi, DIB_PAL_COLORS,
							(void**)&m_screen, NULL,0);
#else
	for (int i = 0; i < 64; ++i)
	{
		float r,g,b;
		r = (float) (i & 1);
		g = (float) ((i & 2) >> 1);
		b = (float) ((i & 4) >> 2);

		if (palette_type != RGB)
		{
			r = g = b = (float) (0.299 * r + 0.587 * g + 0.114 * b);

			switch (palette_type)
			{
			case AMBER:
				r *= (float) 1.0;
				g *= (float) 0.8;
				b *= (float) 0.1;
				break;
			case GREEN:
				r *= (float) 0.2;
				g *= (float) 0.9;
				b *= (float) 0.1;
				break;
			}
		}

		m_bmi.bmiColors[i].rgbRed   = (BYTE) (r * m_BlurIntensities[i >> 3] / 100.0 * 255);
		m_bmi.bmiColors[i].rgbGreen = (BYTE) (g * m_BlurIntensities[i >> 3] / 100.0 * 255);
		m_bmi.bmiColors[i].rgbBlue  = (BYTE) (b * m_BlurIntensities[i >> 3] / 100.0 * 255);
		m_bmi.bmiColors[i].rgbReserved = 0;
	}

	// Red Leds - left is dark, right is lit.
	m_bmi.bmiColors[LED_COL_BASE].rgbRed=80;		m_bmi.bmiColors[LED_COL_BASE+1].rgbRed=255;
	m_bmi.bmiColors[LED_COL_BASE].rgbGreen=0;		m_bmi.bmiColors[LED_COL_BASE+1].rgbGreen=0;
	m_bmi.bmiColors[LED_COL_BASE].rgbBlue=0;		m_bmi.bmiColors[LED_COL_BASE+1].rgbBlue=0;
	m_bmi.bmiColors[LED_COL_BASE].rgbReserved=0;	m_bmi.bmiColors[LED_COL_BASE+1].rgbReserved=0;
	// Green Leds - left is dark, right is lit.
	m_bmi.bmiColors[LED_COL_BASE+2].rgbRed=0;		m_bmi.bmiColors[LED_COL_BASE+3].rgbRed=0;
	m_bmi.bmiColors[LED_COL_BASE+2].rgbGreen=80;	m_bmi.bmiColors[LED_COL_BASE+3].rgbGreen=255;
	m_bmi.bmiColors[LED_COL_BASE+2].rgbBlue=0;		m_bmi.bmiColors[LED_COL_BASE+3].rgbBlue=0;
	m_bmi.bmiColors[LED_COL_BASE+2].rgbReserved=0;	m_bmi.bmiColors[LED_COL_BASE+3].rgbReserved=0;

	m_hBitmap = CreateDIBSection(m_hDCBitmap, (BITMAPINFO *)&m_bmi, DIB_RGB_COLORS,
							(void**)&m_screen, NULL,0);
#endif

	m_screen_blur = (char *)calloc(m_bmi.bmiHeader.biSizeImage,1);

	m_hOldObj = SelectObject(m_hDCBitmap, m_hBitmap);
	if(m_hOldObj == NULL)
		MessageBox(m_hWnd,"Cannot select the screen bitmap\n"
					"Try running in a 256 colour mode",WindowTitle,MB_OK|MB_ICONERROR);
}

/****************************************************************************/
BOOL BeebWin::InitClass(void)
{
	WNDCLASS  wc;

	// Fill in window class structure with parameters that describe the
	// main window.

	wc.style		 = CS_HREDRAW | CS_VREDRAW;// Class style(s).
	wc.lpfnWndProc	 = (WNDPROC)WndProc;	   // Window Procedure
	wc.cbClsExtra	 = 0;					   // No per-class extra data.
	wc.cbWndExtra	 = 0;					   // No per-window extra data.
	wc.hInstance	 = hInst;				   // Owner of this class
	wc.hIcon		 = LoadIcon(hInst, MAKEINTRESOURCE(IDI_BEEBEM));
	wc.hCursor		 = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.lpszMenuName  = MAKEINTRESOURCE(IDR_MENU); // Menu from .RC
	wc.lpszClassName = "BEEBWIN"; //szAppName;				// Name to register as

	// Register the window class and return success/failure code.
	return (RegisterClass(&wc));
}

/****************************************************************************/
void BeebWin::CreateBeebWindow(void)
{
	DWORD style;
	int x,y;
	int show = SW_SHOW;

	x = m_XWinPos;
	y = m_YWinPos;
	if (x == -1 || y == -1)
	{
		x = CW_USEDEFAULT;
		y = 0;
		m_XWinPos = 0;
		m_YWinPos = 0;
	}

	if (m_DisplayRenderer == IDM_DISPGDI && m_isFullScreen)
		show = SW_MAXIMIZE;

	if (m_DisplayRenderer != IDM_DISPGDI && m_isFullScreen)
	{
		style = WS_POPUP;
	}
	else
	{
		style = WIN_STYLE;
	}

	m_hWnd = CreateWindow(
				"BEEBWIN",				// See RegisterClass() call.
				m_szTitle, 		// Text for window title bar.
				style,
				x, y,
				m_XWinSize + m_XWinBorder,
				m_YWinSize + m_YWinBorder,
				NULL,					// Overlapped windows have no parent.
				NULL,				 // Use the window class menu.
				hInst,			 // This instance owns this window.
				NULL				 // We don't use any data in our WM_CREATE
		); 

	ShowWindow(m_hWnd, show); // Show the window
	UpdateWindow(m_hWnd);		  // Sends WM_PAINT message

	SetWindowAttributes(false);
}

void BeebWin::ShowMenu(bool on) {
 if (DisableMenu)
  on=FALSE;

 if (on!=MenuOn) {
  if (on)
    SetMenu(m_hWnd, m_hMenu);
  else
    SetMenu(m_hWnd, NULL);
 }
  MenuOn=on;
}

void BeebWin::TrackPopupMenu(int x, int y) {
  ::TrackPopupMenu(m_hMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
                   x, y,
                   0,
                   m_hWnd,
                   NULL);
}

/****************************************************************************/
void BeebWin::InitMenu(void)
{
	char menu_string[256];
	HMENU hMenu = m_hMenu;

	// File -> Video Options
	CheckMenuItem(hMenu, IDM_VIDEORES1, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_VIDEORES2, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_VIDEORES3, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_VIDEOSKIP0, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_VIDEOSKIP1, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_VIDEOSKIP2, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_VIDEOSKIP3, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_VIDEOSKIP4, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_VIDEOSKIP5, MF_UNCHECKED);
	CheckMenuItem(hMenu, m_MenuIdAviResolution, MF_CHECKED);
	CheckMenuItem(hMenu, m_MenuIdAviSkip, MF_CHECKED);

	// File -> Disc Options
	CheckMenuItem(hMenu, IDM_WPDISC0, m_WriteProtectDisc[0] ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_WPDISC1, m_WriteProtectDisc[1] ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_WPONLOAD, m_WriteProtectOnLoad ? MF_CHECKED : MF_UNCHECKED);

	// File -> Capture Options
	CheckMenuItem(hMenu, IDM_CAPTURERES1, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_CAPTURERES2, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_CAPTURERES3, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_CAPTUREBMP, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_CAPTUREJPEG, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_CAPTUREGIF, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_CAPTUREPNG, MF_UNCHECKED);
	CheckMenuItem(hMenu, m_MenuIdCaptureResolution, MF_CHECKED);
	CheckMenuItem(hMenu, m_MenuIdCaptureFormat, MF_CHECKED);

	// Edit
	CheckMenuItem(hMenu, IDM_EDIT_CRLF, m_translateCRLF ? MF_CHECKED : MF_UNCHECKED);

	// Comms -> Tape Speed
	SetTapeSpeedMenu();

	// Comms
	CheckMenuItem(m_hMenu,ID_UNLOCKTAPE,(UnlockTape)?MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_PRINTERONOFF, PrinterEnabled ? MF_CHECKED : MF_UNCHECKED);

	// Comms -> Printer
	CheckMenuItem(hMenu, IDM_PRINTER_FILE, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_PRINTER_LPT1, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_PRINTER_LPT2, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_PRINTER_LPT3, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_PRINTER_LPT4, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_PRINTER_COM1, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_PRINTER_COM2, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_PRINTER_COM3, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_PRINTER_COM4, MF_UNCHECKED);
	CheckMenuItem(hMenu, m_MenuIdPrinterPort, MF_CHECKED);
	strcpy(menu_string, "File: ");
	strcat(menu_string, m_PrinterFileName);
	ModifyMenu(hMenu, IDM_PRINTER_FILE, MF_BYCOMMAND, IDM_PRINTER_FILE, menu_string);

	// Comms -> RS423
	CheckMenuItem(hMenu, ID_SERIAL, SerialPortEnabled ? MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_COM1, (SerialPort==1)? MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_COM2, (SerialPort==2)? MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_COM3, (SerialPort==3)? MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_COM4, (SerialPort==4)? MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_TOUCHSCREEN, TouchScreenEnabled ? MF_CHECKED:MF_UNCHECKED);
//	CheckMenuItem(hMenu, ID_IP232, EthernetPortEnabled ? MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_IP232LOCALHOST, IP232localhost ? MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_IP232CUSTOM, IP232custom ? MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_IP232MODE, IP232mode ? MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_IP232RAW, IP232raw ? MF_CHECKED:MF_UNCHECKED);

	// View
	UpdateDisplayRendererMenu();
	CheckMenuItem(hMenu, IDM_DXSMOOTHING, m_DXSmoothing ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_DXSMOOTHMODE7ONLY, m_DXSmoothMode7Only ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_SPEEDANDFPS, m_ShowSpeedAndFPS ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_FULLSCREEN, m_isFullScreen ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_MAINTAINASPECTRATIO, m_MaintainAspectRatio ? MF_CHECKED : MF_UNCHECKED);
	UpdateMonitorMenu();
	CheckMenuItem(hMenu, ID_HIDEMENU, HideMenuEnabled ? MF_CHECKED:MF_UNCHECKED);
	UpdateLEDMenu(hMenu);
	CheckMenuItem(hMenu, IDM_TEXTVIEW, m_TextViewEnabled ? MF_CHECKED:MF_UNCHECKED);

	// View -> Win size
	CheckMenuItem(hMenu, IDM_320X256, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_640X512, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_800X600, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_1024X768, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_1024X512, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_1280X1024, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_1440X1080, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_1600X1200, MF_UNCHECKED);
	//CheckMenuItem(hMenu, m_MenuIdWinSize, MF_CHECKED);

	// View -> DD mode
	CheckMenuItem(hMenu, ID_VIEW_DD_640X480, MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_VIEW_DD_720X576, MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_VIEW_DD_800X600, MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_VIEW_DD_1024X768, MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_VIEW_DD_1280X720, MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_VIEW_DD_1280X1024, MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_VIEW_DD_1280X768, MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_VIEW_DD_1280X960, MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_VIEW_DD_1440X900, MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_VIEW_DD_1600X1200, MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_VIEW_DD_1920X1080, MF_UNCHECKED);
	CheckMenuItem(hMenu, m_DDFullScreenMode, MF_CHECKED);

	// View -> Motion blur
	CheckMenuItem(hMenu, IDM_BLUR_OFF, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_BLUR_2, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_BLUR_4, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_BLUR_8, MF_UNCHECKED);
	CheckMenuItem(hMenu, m_MotionBlur, MF_CHECKED);

	// Speed
	CheckMenuItem(hMenu, IDM_REALTIME, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_FIXEDSPEED100, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_FIXEDSPEED50, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_FIXEDSPEED10, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_FIXEDSPEED5, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_FIXEDSPEED2, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_FIXEDSPEED1_5, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_FIXEDSPEED1_25, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_FIXEDSPEED1_1, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_FIXEDSPEED0_9, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_FIXEDSPEED0_5, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_FIXEDSPEED0_75, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_FIXEDSPEED0_25, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_FIXEDSPEED0_1, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_50FPS, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_25FPS, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_10FPS, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_5FPS, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_1FPS, MF_UNCHECKED);
	CheckMenuItem(hMenu, m_MenuIdTiming, MF_CHECKED);

	// Sound
	UpdateSoundStreamerMenu();
	SetSoundMenu();
#ifdef SPEECH_ENABLED
	CheckMenuItem(hMenu, IDM_SPEECH, SpeechDefault ? MF_CHECKED:MF_UNCHECKED);
#endif
	CheckMenuItem(m_hMenu,IDM_SOUNDCHIP,(SoundChipEnabled)?MF_CHECKED:MF_UNCHECKED);
	UpdateSFXMenu();
	CheckMenuItem(m_hMenu,ID_TAPESOUND,(TapeSoundEnabled)?MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_44100KHZ, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_22050KHZ, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_11025KHZ, MF_UNCHECKED);
	CheckMenuItem(hMenu, m_MenuIdSampleRate, MF_CHECKED);
	CheckMenuItem(hMenu, IDM_HIGHVOLUME, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_MEDIUMVOLUME, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_LOWVOLUME, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_FULLVOLUME, MF_UNCHECKED);
	CheckMenuItem(hMenu, m_MenuIdVolume, MF_CHECKED);
	SetPBuff();
	CheckMenuItem(m_hMenu,ID_PSAMPLES,(PartSamples)?MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(m_hMenu, IDM_EXPVOLUME, SoundExponentialVolume ? MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_TEXTTOSPEECH, m_TextToSpeechEnabled ? MF_CHECKED:MF_UNCHECKED);

	// AMX
	CheckMenuItem(hMenu, IDM_AMXONOFF, AMXMouseEnabled ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_AMX_LRFORMIDDLE, AMXLRForMiddle ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_AMX_320X256, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_AMX_640X256, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_AMX_160X256, MF_UNCHECKED);
	CheckMenuItem(hMenu, m_MenuIdAMXSize, MF_CHECKED);
	CheckMenuItem(hMenu, IDM_AMX_ADJUSTP50, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_AMX_ADJUSTP30, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_AMX_ADJUSTP10, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_AMX_ADJUSTM10, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_AMX_ADJUSTM30, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_AMX_ADJUSTM50, MF_UNCHECKED);
	if (m_MenuIdAMXAdjust != 0)
		CheckMenuItem(hMenu, m_MenuIdAMXAdjust, MF_CHECKED);

	// Hardware -> Model
	UpdateModelType();

	// Hardware
	CheckMenuItem(m_hMenu, IDM_TUBE, (TubeEnabled)?MF_CHECKED:MF_UNCHECKED);
#ifdef M512COPRO_ENABLED
	CheckMenuItem(m_hMenu, IDM_TUBE186, (Tube186Enabled)?MF_CHECKED:MF_UNCHECKED);
#endif
	CheckMenuItem(m_hMenu, IDM_TORCH, (TorchTube)?MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(m_hMenu, IDM_ACORNZ80, (AcornZ80)?MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(m_hMenu, IDM_ARM, (ArmTube)?MF_CHECKED:MF_UNCHECKED);
	SetRomMenu();
	CheckMenuItem(hMenu, IDM_SWRAMBOARD, SWRAMBoardEnabled ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_IGNOREILLEGALOPS, IgnoreIllegalInstructions ? MF_CHECKED : MF_UNCHECKED);
	UpdateOptiMenu();
	UpdateEconetMenu(hMenu);
	CheckMenuItem(hMenu, ID_TELETEXT, TeleTextAdapterEnabled ? MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_FLOPPYDRIVE, Disc8271Enabled ? MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_HARDDRIVE, SCSIDriveEnabled ? MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_IDEDRIVE, IDEDriveEnabled ? MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_UPRM, RTC_Enabled ? MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_RTCY2KADJUST, RTCY2KAdjust ? MF_CHECKED:MF_UNCHECKED);

	// Options
	CheckMenuItem(hMenu, IDM_JOYSTICK, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_AMOUSESTICK, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_DMOUSESTICK, MF_UNCHECKED);
	if (m_MenuIdSticks != 0)
		CheckMenuItem(hMenu, m_MenuIdSticks, MF_CHECKED);
	CheckMenuItem(hMenu, IDM_FREEZEINACTIVE, m_FreezeWhenInactive ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_HIDECURSOR, m_HideCursor ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_DEFAULTKYBDMAPPING, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_LOGICALKYBDMAPPING, MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_USERKYBDMAPPING, MF_UNCHECKED);
	CheckMenuItem(hMenu, m_MenuIdKeyMapping, MF_CHECKED);
	CheckMenuItem(hMenu, IDM_MAPAS, m_KeyMapAS ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_MAPFUNCS, m_KeyMapFunc ? MF_CHECKED : MF_UNCHECKED);
	UpdateDisableKeysMenu();
	CheckMenuItem(hMenu, IDM_AUTOSAVE_PREFS_CMOS, m_AutoSavePrefsCMOS ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_AUTOSAVE_PREFS_FOLDERS, m_AutoSavePrefsFolders ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_AUTOSAVE_PREFS_ALL, m_AutoSavePrefsAll ? MF_CHECKED : MF_UNCHECKED);
}

void BeebWin::UpdateDisplayRendererMenu() {
	CheckMenuItem(m_hMenu, IDM_DISPGDI,
				  m_DisplayRenderer == IDM_DISPGDI ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(m_hMenu, IDM_DISPDDRAW,
				  m_DisplayRenderer == IDM_DISPDDRAW ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(m_hMenu, IDM_DISPDX9,
				  m_DisplayRenderer == IDM_DISPDX9 ? MF_CHECKED : MF_UNCHECKED);
}

void BeebWin::UpdateSoundStreamerMenu() {
	CheckMenuItem(m_hMenu, IDM_XAUDIO2, SoundConfig::Selection == SoundConfig::XAudio2 ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(m_hMenu, IDM_DIRECTSOUND, SoundConfig::Selection == SoundConfig::DirectSound ? MF_CHECKED : MF_UNCHECKED);
	if( SoundConfig::Selection == SoundConfig::XAudio2 )
	{
		UsePrimaryBuffer = 0;
		SetPBuff();
	}
	EnableMenuItem(m_hMenu, ID_PBUFF, SoundConfig::Selection == SoundConfig::DirectSound ? MF_ENABLED : MF_GRAYED);
}

void BeebWin::UpdateMonitorMenu() {
	HMENU hMenu = m_hMenu;
	CheckMenuItem(hMenu, ID_MONITOR_RGB, (palette_type == RGB) ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_MONITOR_BW , (palette_type == BW) ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_MONITOR_GREEN , (palette_type == GREEN) ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_MONITOR_AMBER , (palette_type == AMBER) ? MF_CHECKED : MF_UNCHECKED);
}

void BeebWin::UpdateModelType() {
	HMENU hMenu= m_hMenu;
	CheckMenuItem(hMenu, ID_MODELB, (MachineType == 0) ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_MODELBINT, (MachineType == 1) ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_MODELBP, (MachineType == 2) ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_MASTER128, (MachineType == 3) ? MF_CHECKED : MF_UNCHECKED);
}

void BeebWin::UpdateSFXMenu() {
	HMENU hMenu = m_hMenu;
	CheckMenuItem(hMenu,ID_SFX_RELAY,RelaySoundEnabled?MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(hMenu,ID_SFX_DISCDRIVES,DiscDriveSoundEnabled?MF_CHECKED:MF_UNCHECKED);
}

void BeebWin::UpdateDisableKeysMenu() {
	CheckMenuItem(m_hMenu, IDM_DISABLEKEYSWINDOWS, m_DisableKeysWindows?MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(m_hMenu, IDM_DISABLEKEYSBREAK, m_DisableKeysBreak?MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(m_hMenu, IDM_DISABLEKEYSESCAPE, m_DisableKeysEscape?MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(m_hMenu, IDM_DISABLEKEYSSHORTCUT, m_DisableKeysShortcut?MF_CHECKED:MF_UNCHECKED);
}

/****************************************************************************/
void BeebWin::SetRomMenu(void)
{
	HMENU hMenu = m_hMenu;

	// Set the ROM Titles in the ROM/RAM menu.
	CHAR Title[19];
	
	int	 i;

	for( i=0; i<16; i++ )
	{
		Title[0] = '&';
		_itoa( i, &Title[1], 16 );
		Title[2] = ' ';
		
		// Get the Rom Title.
		ReadRomTitle( i, &Title[3], sizeof( Title )-4);
	
		if ( Title[3]== '\0' )
		{
			if (RomBankType[i]==BankRam)
				strcpy( &Title[3], "RAM" );
			else
				strcpy( &Title[3], "Empty" );
		}

		ModifyMenu( hMenu,	// handle of menu 
					IDM_ALLOWWRITES_ROM0 + i,
					MF_BYCOMMAND,	// menu item to modify
				//	MF_STRING,	// menu item flags 
					IDM_ALLOWWRITES_ROM0 + i,	// menu item identifier or pop-up menu handle
					Title		// menu item content 
					);

		/* Disable ROM and uncheck the Rom/RAM which are NOT writable */
		EnableMenuItem(hMenu, IDM_ALLOWWRITES_ROM0 + i,
					   (RomBankType[i]==BankRam) ? MF_ENABLED : MF_GRAYED);
		CheckMenuItem(hMenu, IDM_ALLOWWRITES_ROM0 + i,
					  (RomBankType[i]==BankRam) ? (RomWritable[i] ? MF_CHECKED : MF_UNCHECKED) : MF_UNCHECKED);
	}
}

/****************************************************************************/
void BeebWin::InitJoystick(void)
{
	MMRESULT mmresult = JOYERR_NOERROR;

	if (!m_JoystickCaptured)
	{
		/* Get joystick updates 10 times a second */
		mmresult = joySetCapture(m_hWnd, JOYSTICKID1, 100, FALSE);
		if (mmresult == JOYERR_NOERROR)
			mmresult = joyGetDevCaps(JOYSTICKID1, &m_JoystickCaps, sizeof(JOYCAPS));
		if (mmresult == JOYERR_NOERROR)
			m_JoystickCaptured = true;
	}

	if (mmresult == JOYERR_NOERROR)
	{
		AtoDEnable();
	}
	else if (mmresult == JOYERR_UNPLUGGED)
	{
		MessageBox(m_hWnd, "Joystick is not plugged in",
					WindowTitle, MB_OK|MB_ICONERROR);
	}
	else
	{
		MessageBox(m_hWnd, "Failed to initialise the joystick",
					WindowTitle, MB_OK|MB_ICONERROR);
	}
}

/****************************************************************************/
void BeebWin::ScaleJoystick(unsigned int x, unsigned int y)
{
	if (m_MenuIdSticks == IDM_JOYSTICK)
	{
		/* Scale and reverse the readings */
		JoystickX = (int)((double)(m_JoystickCaps.wXmax - x) * 65535.0 /
						  (double)(m_JoystickCaps.wXmax - m_JoystickCaps.wXmin));
		JoystickY = (int)((double)(m_JoystickCaps.wYmax - y) * 65535.0 /
						  (double)(m_JoystickCaps.wYmax - m_JoystickCaps.wYmin));
	}
}

/****************************************************************************/
void BeebWin::ResetJoystick(void)
{
	// joySetCapture() fails after a joyReleaseCapture() call (not sure why)
	// so leave joystick captured.
	// joyReleaseCapture(JOYSTICKID1);
	AtoDDisable();
}

/****************************************************************************/
void BeebWin::SetMousestickButton(int button)
{
	if ( (m_MenuIdSticks == IDM_AMOUSESTICK) || ((m_MenuIdSticks == IDM_DMOUSESTICK)) )
		JoystickButton = button;
}

/****************************************************************************/
void BeebWin::ScaleMousestick(unsigned int x, unsigned int y)
{
	static int lastx = 32768;
	static int lasty = 32768;
	int dx, dy;

	if (m_MenuIdSticks == IDM_AMOUSESTICK)				// Analogue mouse stick
	{
		JoystickX = (m_XWinSize - x) * 65535 / m_XWinSize;
		JoystickY = (m_YWinSize - y) * 65535 / m_YWinSize;
	}
	else if (m_MenuIdSticks == IDM_DMOUSESTICK)		// Digital mouse stick
	{
		dx = x - lastx;
		dy = y - lasty;
		
		if (dx > 4) JoystickX = 0;
		if (dx < -4) JoystickX = 65535;

		if (dy > 4) JoystickY = 0;
		if (dy < -4) JoystickY = 65535;

		lastx = x;
		lasty = y;
	}

	if (m_HideCursor)
		SetCursor(NULL);
}

/****************************************************************************/
void BeebWin::SetAMXPosition(unsigned int x, unsigned int y)
{
	if (AMXMouseEnabled)
	{
		// Scale the window coords to the beeb screen coords
		AMXTargetX = x * m_AMXXSize * (100 + m_AMXAdjust) / 100 / m_XWinSize;
		AMXTargetY = y * m_AMXYSize * (100 + m_AMXAdjust) / 100 / m_YWinSize;

		AMXMouseMovement();
	}
}

/****************************************************************************/
LRESULT CALLBACK WndProc(
				HWND hWnd,		   // window handle
				UINT message,	   // type of message
				WPARAM uParam,	   // additional information
				LPARAM lParam)	   // additional information
{
	int wmId, wmEvent;
	HDC hdc;
	int row, col;

	switch (message)
	{
		case WM_COMMAND:  // message: command from application menu
			wmId	= LOWORD(uParam);
			wmEvent = HIWORD(uParam);
			if (mainWin)
				mainWin->HandleCommand(wmId);
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
			mainWin->RealizePalette(hdc);
			ReleaseDC(hWnd,hdc);    
			return TRUE;							    
			break;

		case WM_PAINT:
			if(mainWin != NULL)
			{
				PAINTSTRUCT ps;
				HDC 		hDC;

				hDC = BeginPaint(hWnd, &ps);
				mainWin->RealizePalette(hDC);
				mainWin->updateLines(hDC, 0, 0);
				EndPaint(hWnd, &ps);

				if (mainWin->m_DXResetPending)
					mainWin->ResetDX();
			}
			break;

		case WM_SIZE:
			mainWin->WinSizeChange(uParam, LOWORD(lParam), HIWORD(lParam));
			break;

		case WM_MOVE:
			mainWin->WinPosChange(LOWORD(lParam), HIWORD(lParam));
			break;

		case WM_SYSKEYDOWN:
			//{char txt[100];sprintf(txt,"SysKeyD: %d, 0x%X, 0x%X\n", uParam, uParam, lParam);OutputDebugString(txt);}
			if (mainWin->m_TextToSpeechEnabled &&
				((uParam >= VK_NUMPAD0 && uParam <= VK_NUMPAD9) ||
				 uParam == VK_DECIMAL ||
				 uParam == VK_HOME || uParam == VK_END ||
				 uParam == VK_PRIOR || uParam == VK_NEXT ||
				 uParam == VK_UP || uParam == VK_DOWN ||
				 uParam == VK_LEFT || uParam == VK_RIGHT ||
				 uParam == VK_INSERT || uParam == VK_DELETE ||
				 uParam == VK_CLEAR))
			{
				mainWin->TextToSpeechKey(uParam);
			}
			else if (uParam == VK_OEM_8 /* Alt ` */)
			{
				mainWin->TextViewSyncWithBeebCursor();
			}
			else if (uParam == VK_RETURN && (lParam & 0x20000000))
			{
				mainWin->HandleCommand(IDM_FULLSCREEN);
				break;
			}
			else if (uParam == VK_F4 && (lParam & 0x20000000))
			{
				mainWin->HandleCommand(IDM_EXIT);
				break;
			}

			if (uParam != VK_F10 && uParam != VK_CONTROL)
				break;
		case WM_KEYDOWN:
			//{char txt[100];sprintf(txt,"KeyD: %d, 0x%X, 0x%X\n", uParam, uParam, lParam);OutputDebugString(txt);}
			if (mainWin->m_TextToSpeechEnabled &&
				((uParam >= VK_NUMPAD0 && uParam <= VK_NUMPAD9) ||
				 uParam == VK_DECIMAL ||
				 uParam == VK_HOME || uParam == VK_END ||
				 uParam == VK_PRIOR || uParam == VK_NEXT ||
				 uParam == VK_UP || uParam == VK_DOWN ||
				 uParam == VK_LEFT || uParam == VK_RIGHT ||
				 uParam == VK_INSERT || uParam == VK_DELETE ||
				 uParam == VK_CLEAR))
			{
				mainWin->TextToSpeechKey(uParam);
			}
			else if ((uParam == VK_DIVIDE || uParam == VK_MULTIPLY ||
					  uParam == VK_ADD || uParam == VK_SUBTRACT) &&
					 !mainWin->m_DisableKeysShortcut)
			{
				// Ignore shortcut key, handled on key up
			}
			else
			{
				int i;
				int mask;
				bool bit;

				mask = 0x01;
				bit = false;

				if (mBreakOutWindow == true)
				{
					for (i = 0; i < 8; ++i)
					{
						if (BitKeys[i] == uParam)
						{
							if ((UserVIAState.ddrb & mask) == 0x00)
							{
								UserVIAState.irb &= ~mask;
								ShowInputs( (UserVIAState.orb & UserVIAState.ddrb) | (UserVIAState.irb & (~UserVIAState.ddrb)) );
								bit = true;
							}
						}
						mask <<= 1;
					}
				}
				if (bit == false)
				{
					// Reset shift state if it was set by Run Disc
					if (mainWin->m_ShiftBooted)
					{
						mainWin->m_ShiftBooted = false;
						BeebKeyUp(0, 0);
					}
					
					mainWin->TranslateKey((int)uParam, false, row, col);
				}
			}
			break;

		case WM_SYSKEYUP:
			//{char txt[100];sprintf(txt,"SysKeyU: %d, 0x%X, 0x%X\n", uParam, uParam, lParam);OutputDebugString(txt);}
			if ((uParam == 0x35 || uParam == VK_NUMPAD5) && (lParam & 0x20000000))
			{
				mainWin->CaptureBitmapPending(true);
				break;
			}

			if (uParam == 0x31 && (lParam & 0x20000000) && !mainWin->m_DisableKeysShortcut)
			{
				mainWin->QuickSave();
				// Let user know state has been saved
				FlashWindow(GETHWND, TRUE);
				MessageBeep(MB_ICONEXCLAMATION);
				break;
			}
			if (uParam == 0x32 && (lParam & 0x20000000) && !mainWin->m_DisableKeysShortcut)
			{
				mainWin->QuickLoad();
				// Let user know state has been loaded
				FlashWindow(GETHWND, TRUE);
				MessageBeep(MB_ICONEXCLAMATION);
				break;
			}

			if (uParam == VK_OEM_PLUS && (lParam & 0x20000000))
			{
				mainWin->AdjustSpeed(true);
				break;
			}
			if (uParam == VK_OEM_MINUS && (lParam & 0x20000000))
			{
				mainWin->AdjustSpeed(false);
				break;
			}

			if (uParam != VK_F10 && uParam != VK_CONTROL)
				break;
		case WM_KEYUP:
			//{char txt[100];sprintf(txt,"KeyU: %d, 0x%X, 0x%X\n", uParam, uParam, lParam);OutputDebugString(txt);}
			if (uParam == VK_DIVIDE && !mainWin->m_DisableKeysShortcut)
			{
				mainWin->QuickSave();
				// Let user know state has been saved
				FlashWindow(GETHWND, TRUE);
				MessageBeep(MB_ICONEXCLAMATION);
			}
			else if (uParam == VK_MULTIPLY && !mainWin->m_DisableKeysShortcut)
			{
				mainWin->QuickLoad();
				// Let user know state has been loaded
				FlashWindow(GETHWND, TRUE);
				MessageBeep(MB_ICONEXCLAMATION);
			}
			else if (uParam == VK_ADD && !mainWin->m_DisableKeysShortcut)
			{
				mainWin->AdjustSpeed(true);
			}
			else if (uParam == VK_SUBTRACT && !mainWin->m_DisableKeysShortcut)
			{
				mainWin->AdjustSpeed(false);
			}
			else 
			{
				int i;
				int mask;
				bool bit;

				mask = 0x01;
				bit = false;

				if (mBreakOutWindow == true)
				{
					for (i = 0; i < 8; ++i)
					{
						if (BitKeys[i] == uParam)
						{
							if ((UserVIAState.ddrb & mask) == 0x00)
							{
								UserVIAState.irb |= mask;
								ShowInputs( (UserVIAState.orb & UserVIAState.ddrb) | (UserVIAState.irb & (~UserVIAState.ddrb)) );
								bit = true;
							}
						}
						mask <<= 1;
					}
				}

				if (bit == false)
				{
					if(mainWin->TranslateKey((int)uParam, true, row, col) < 0)
					{
						if(row==-2)
						{ // Must do a reset!
							Init6502core();
							if (EnableTube) Init65C02core();
#ifdef M512COPRO_ENABLED
							if (Tube186Enabled) i86_main();
#endif
							Enable_Z80 = 0;
							if (TorchTube || AcornZ80)
							{
								R1Status = 0;
								ResetTube();
								init_z80();
								Enable_Z80 = 1;
							}
							Enable_Arm = 0;
							if (ArmTube)
							{
								R1Status = 0;
								ResetTube();
								if (arm) delete arm;
								arm = new CArm;
								Enable_Arm = 1;
							}
							Disc8271_reset();
							Reset1770();
							if (EconetEnabled) EconetReset();//Rob
							if (SCSIDriveEnabled) SCSIReset();
							if (SCSIDriveEnabled) SASIReset();
							if (IDEDriveEnabled)  IDEReset();
							TeleTextInit();
							//SoundChipReset();
						}
						else if(row==-3)
						{
							if (col==-3) SoundTuning+=0.1; // Page Up
							if (col==-4) SoundTuning-=0.1; // Page Down
						}
					}
				}
			}
			break;					  

		case WM_ACTIVATE:
			if (mainWin)
			{
				mainWin->Activate(uParam != WA_INACTIVE);
				if(uParam != WA_INACTIVE)
				{
					// Bring debug window to foreground BEHIND main window.
					if(hwndDebug)
					{
						SetWindowPos(hwndDebug, GETHWND,0,0,0,0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOACTIVATE);
						SetWindowPos(GETHWND, HWND_TOP,0,0,0,0, SWP_NOMOVE | SWP_NOSIZE);
					}
				}
			}
			break;

		case WM_SETFOCUS:
			if (mainWin)
				mainWin->Focus(TRUE);
			break;

		case WM_KILLFOCUS:
			BeebReleaseAllKeys();
			if (mainWin)
				mainWin->Focus(FALSE);
			break;					  

		case MM_JOY1MOVE:
			if (mainWin)
				mainWin->ScaleJoystick(LOWORD(lParam), HIWORD(lParam));
			break;

		case MM_JOY1BUTTONDOWN:
		case MM_JOY1BUTTONUP:
			JoystickButton = ((UINT)uParam & (JOY_BUTTON1 | JOY_BUTTON2)) ? 1 : 0;
			break; 

		case WM_MOUSEMOVE:
			if (mainWin)
			{
				mainWin->ScaleMousestick(LOWORD(lParam), HIWORD(lParam));
				mainWin->SetAMXPosition(LOWORD(lParam), HIWORD(lParam));
				// Experiment: show menu in full screen when cursor moved to top of window
				if (HideMenuEnabled)
				{
					if (HIWORD(lParam) <= 2)
						mainWin->ShowMenu(true);
					else
						mainWin->ShowMenu(false);
				}
			}
			break;

		case WM_LBUTTONDOWN:
			if (mainWin)
				mainWin->SetMousestickButton(((UINT)uParam & MK_LBUTTON) ? TRUE : FALSE);
			AMXButtons |= AMX_LEFT_BUTTON;
			break;
		case WM_LBUTTONUP:
			if (mainWin)
				mainWin->SetMousestickButton(((UINT)uParam & MK_LBUTTON) ? TRUE : FALSE);
			AMXButtons &= ~AMX_LEFT_BUTTON;
			break;

		case WM_MBUTTONDOWN:
			AMXButtons |= AMX_MIDDLE_BUTTON;
			break;
		case WM_MBUTTONUP:
			AMXButtons &= ~AMX_MIDDLE_BUTTON;
			break;

		case WM_RBUTTONDOWN:
			AMXButtons |= AMX_RIGHT_BUTTON;
			break;
		case WM_RBUTTONUP:
			AMXButtons &= ~AMX_RIGHT_BUTTON;
			break;

		case WM_DESTROY:  // message: window being destroyed
			if (mainWin)
				mainWin->Shutdown();
			PostQuitMessage(0);
			break;

		case WM_ENTERMENULOOP: // entering menu, must mute directsound
			SetSound(MUTED);
			break;

		case WM_EXITMENULOOP:
			SetSound(UNMUTED);
			if (mainWin)
				mainWin->ResetTiming();
			break;

		case WM_ENTERSIZEMOVE: // Window being moved
			SetSound(MUTED);
			break;
		case WM_EXITSIZEMOVE:
			SetSound(UNMUTED);
			break;

		case WM_REINITDX:
			mainWin->ReinitDX();
			break;

		case WM_TIMER:
			if(uParam == 1)
				mainWin->HandleTimer();
			break;

		default:		  // Passes it on if unproccessed
			return (DefWindowProc(hWnd, message, uParam, lParam));
		}
	return (0);
}

/****************************************************************************/
int BeebWin::TranslateKey(int vkey, int keyUp, int &row, int &col)
{
	if (vkey < 0 || vkey > 255)
		return -9;

	// Key track of shift state
	if ((*transTable)[vkey][0].row == 0 && (*transTable)[vkey][0].col == 0)
	{
		if (keyUp)
			m_ShiftPressed = 0;
		else
			m_ShiftPressed = 1;
	}

	if (keyUp)
	{
		// Key released, lookup beeb row + col that this vkey 
		// mapped to when it was pressed.  Need to release
		// both shifted and non-shifted presses.
		row = m_vkeyPressed[vkey][0][0];
		col = m_vkeyPressed[vkey][1][0];
		m_vkeyPressed[vkey][0][0] = -1;
		m_vkeyPressed[vkey][1][0] = -1;
		if (row >= 0)
			BeebKeyUp(row, col);

		row = m_vkeyPressed[vkey][0][1];
		col = m_vkeyPressed[vkey][1][1];
		m_vkeyPressed[vkey][0][1] = -1;
		m_vkeyPressed[vkey][1][1] = -1;
		if (row >= 0)
			BeebKeyUp(row, col);
	}
	else // New key press - convert to beeb row + col
	{
		int needShift = m_ShiftPressed;

		row = (*transTable)[vkey][m_ShiftPressed].row;
		col = (*transTable)[vkey][m_ShiftPressed].col;
		needShift = (*transTable)[vkey][m_ShiftPressed].shift;

		if (m_KeyMapAS)
		{
			// Map A & S to CAPS & CTRL - good for some games
			if (vkey == 65)
			{
				row = 4;
				col = 0;
			}
			else if (vkey == 83)
			{
				row = 0;
				col = 1;
			}
		}

		if (m_KeyMapFunc)
		{
			// Map F1-F10 to f0-f9
			if (vkey >= 113 && vkey <= 121)
			{
				row = (*transTable)[vkey - 1][0].row;
				col = (*transTable)[vkey - 1][0].col;
			}
			else if (vkey == 112)
			{
				row = 2;
				col = 0;
			}
		}

		if (m_DisableKeysShortcut && (row == -3 || row == -4))
			row = -10;
		if (m_DisableKeysEscape && row == 7 && col == 0)
			row = -10;
		if (m_DisableKeysBreak && row == -2 && col == -2)
			row = -10;

		if (row >= 0)
		{
			// Make sure shift state is correct
			if (needShift)
				BeebKeyDown(0, 0);
			else
				BeebKeyUp(0, 0);

			BeebKeyDown(row, col);

			// Record beeb row + col for key release
			m_vkeyPressed[vkey][0][m_ShiftPressed ? 1 : 0] = row;
			m_vkeyPressed[vkey][1][m_ShiftPressed ? 1 : 0] = col;
		}
		else
		{
			// Special key!  Record so key up returns correct codes
			m_vkeyPressed[vkey][0][1] = row;
			m_vkeyPressed[vkey][1][1] = col;
		}
	}

	return(row);
}

/****************************************************************************/
int BeebWin::StartOfFrame(void)
{
	int FrameNum = 1;

	if (UpdateTiming())
		FrameNum = 0;

	// Force video frame rate to match AVI capture rate to avoid 
	// video and sound getting out of sync
	if (aviWriter != NULL)
	{
		m_AviFrameSkipCount++;
		if (m_AviFrameSkipCount > m_AviFrameSkip)
		{
			m_AviFrameSkipCount = 0;
			FrameNum = 0;
		}
		else
		{
			FrameNum = 1;
		}

		// Ensure that frames captured each second (50 frames) matches
		// the AVI capture FPS rate
		m_AviFrameCount++;
		if (m_AviFrameCount >= 50)
		{
			m_AviFrameCount = 0;
			m_AviFrameSkipCount = 0;
		}
	}

	return FrameNum;
}

void BeebWin::doLED(int sx,bool on) {
	int tsy; char colbase;
	colbase=(DiscLedColour*2)+LED_COL_BASE; // colour will be 0 for red, 1 for green.
	if (sx<100) colbase=LED_COL_BASE; // Red leds for keyboard always
	if (TeletextEnabled)
		tsy=496;
	else
		tsy=m_LastStartY+m_LastNLines-2;
	doUHorizLine(mainWin->cols[((on)?1:0)+colbase],tsy,sx,8);
	doUHorizLine(mainWin->cols[((on)?1:0)+colbase],tsy,sx,8);
};

/****************************************************************************/
void BeebWin::ResetTiming(void)
{
	m_LastTickCount = GetTickCount();
	m_LastStatsTickCount = m_LastTickCount;
	m_LastTotalCycles = TotalCycles;
	m_LastStatsTotalCycles = TotalCycles;
	m_TickBase = m_LastTickCount;
	m_CycleBase = TotalCycles;
	m_MinFrameCount = 0;
	m_LastFPSCount = m_LastTickCount;
	m_ScreenRefreshCount = 0;
}

/****************************************************************************/
BOOL BeebWin::UpdateTiming(void)
{
	DWORD TickCount;
	DWORD Ticks;
	DWORD SpareTicks;
	int Cycles;
	int CyclesPerSec;
	bool UpdateScreen = FALSE;

	TickCount = GetTickCount();

	/* Don't do anything if this is the first call or there has
	   been a long pause due to menu commands, or when something
	   wraps. */
	if (m_LastTickCount == 0 ||
		TickCount < m_LastTickCount ||
		(TickCount - m_LastTickCount) > 1000 ||
		TotalCycles < m_LastTotalCycles)
	{
		ResetTiming();
		return TRUE;
	}

	/* Update stats every second */
	if (TickCount >= m_LastStatsTickCount + 1000)
	{
		m_FramesPerSecond = m_ScreenRefreshCount;
		m_ScreenRefreshCount = 0;
		m_RelativeSpeed = ((TotalCycles - m_LastStatsTotalCycles) / 2000.0) /
								(TickCount - m_LastStatsTickCount);
		m_LastStatsTotalCycles = TotalCycles;
		m_LastStatsTickCount += 1000;
		DisplayTiming();
	}

	// Now we work out if BeebEm is running too fast or not
	if (m_RealTimeTarget > 0.0)
	{
		Ticks = TickCount - m_TickBase;
		Cycles = (int)((double)(TotalCycles - m_CycleBase) / m_RealTimeTarget);

		if (Ticks <= (DWORD)(Cycles / 2000))
		{
			// Need to slow down, show frame (max 50fps though) 
			// and sleep a bit
			if (TickCount >= m_LastFPSCount + 20)
			{
				UpdateScreen = TRUE;
				m_LastFPSCount += 20;
			}
			else
			{
				UpdateScreen = FALSE;
			}

			SpareTicks = (DWORD)(Cycles / 2000) - Ticks;
			Sleep(SpareTicks);
			m_MinFrameCount = 0;
		}
		else
		{
			// Need to speed up, skip a frame
			UpdateScreen = FALSE;

			// Make sure we show at least one in 100 frames
			++m_MinFrameCount;
			if (m_MinFrameCount >= 100)
			{
				UpdateScreen = TRUE;
				m_MinFrameCount = 0;
			}
		}

		/* Move counter bases forward */
		CyclesPerSec = (int) (2000000.0 * m_RealTimeTarget);
		while ((TickCount - m_TickBase) > 1000 && (TotalCycles - m_CycleBase) > CyclesPerSec)
		{
			m_TickBase += 1000;
			m_CycleBase += CyclesPerSec;
		}
	}
	else
	{
		/* Fast as possible with a certain frame rate */
		if (TickCount >= m_LastFPSCount + (1000 / m_FPSTarget))
		{
			UpdateScreen = TRUE;
			m_LastFPSCount += 1000 / m_FPSTarget;
		}
		else
		{
			UpdateScreen = FALSE;
		}
	}

	m_LastTickCount = TickCount;
	m_LastTotalCycles = TotalCycles;

	return UpdateScreen;
}

/****************************************************************************/
void BeebWin::TranslateDDSize(void)
{
	switch (m_DDFullScreenMode)
	{
	default:
	case ID_VIEW_DD_640X480:
		m_XDXSize = 640;
		m_YDXSize = 480;
		break;
	case ID_VIEW_DD_720X576:
		m_XDXSize = 720;
		m_YDXSize = 576;
		break;
	case ID_VIEW_DD_800X600:
		m_XDXSize = 800;
		m_YDXSize = 600;
		break;
	case ID_VIEW_DD_1024X768:
		m_XDXSize = 1024;
		m_YDXSize = 768;
		break;
	case ID_VIEW_DD_1280X720:
		m_XDXSize = 1280;
		m_YDXSize = 720;
		break;
	case ID_VIEW_DD_1280X768:
		m_XDXSize = 1280;
		m_YDXSize = 768;
		break;
	case ID_VIEW_DD_1280X960:
		m_XDXSize = 1280;
		m_YDXSize = 960;
		break;
	case ID_VIEW_DD_1280X1024:
		m_XDXSize = 1280;
		m_YDXSize = 1024;
		break;
	case ID_VIEW_DD_1440X900:
		m_XDXSize = 1440;
		m_YDXSize = 900;
		break;
	case ID_VIEW_DD_1600X1200:
		m_XDXSize = 1600;
		m_YDXSize = 1200;
		break;
	case ID_VIEW_DD_1920X1080:
		m_XDXSize = 1920;
		m_YDXSize = 1080;
		break;
	}
}
	
/****************************************************************************/
void BeebWin::TranslateWindowSize(void)
{
	switch (m_MenuIdWinSize)
	{
	case IDM_320X256:
		m_XWinSize = 320;
		m_YWinSize = 256;
		break;
	default:
	case IDM_640X512:
		m_XWinSize = 640;
		m_YWinSize = 512;
		break;
	case IDM_800X600:
		m_XWinSize = 800;
		m_YWinSize = 600;
		break;
	case IDM_1024X512:
		m_XWinSize = 1024;
		m_YWinSize = 512;
		break;
	case IDM_1024X768:
		m_XWinSize = 1024;
		m_YWinSize = 768;
		break;
	case IDM_1280X1024:
		m_XWinSize = 1280;
		m_YWinSize = 1024;
		break;
	case IDM_1440X1080:
		m_XWinSize = 1440;
		m_YWinSize = 1080;
		break;
	case IDM_1600X1200:
		m_XWinSize = 1600;
		m_YWinSize = 1200;
		break;
	case IDM_CUSTOMWINSIZE:
		break;
	}

	m_XLastWinSize = m_XWinSize;
	m_YLastWinSize = m_YWinSize;

	m_MenuIdWinSize = IDM_CUSTOMWINSIZE;
}

/****************************************************************************/
void BeebWin::TranslateSampleRate(void)
{
	switch (m_MenuIdSampleRate)
	{
	case IDM_44100KHZ:
		SoundSampleRate = 44100;
		break;

	default:
	case IDM_22050KHZ:
		SoundSampleRate = 22050;
		break;

	case IDM_11025KHZ:
		SoundSampleRate = 11025;
		break;
	}
}

/****************************************************************************/
void BeebWin::TranslateVolume(void)
{
	switch (m_MenuIdVolume)
	{
	case IDM_FULLVOLUME:
		SoundVolume = 1;
		break;

	case IDM_HIGHVOLUME:
		SoundVolume = 2;
		break;

	default:
	case IDM_MEDIUMVOLUME:
		SoundVolume = 3;
		break;

	case IDM_LOWVOLUME:
		SoundVolume = 4;
		break;
	}
}

/****************************************************************************/
void BeebWin::TranslateTiming(void)
{
	m_FPSTarget = 0;
	m_RealTimeTarget = 1.0;

	if (m_MenuIdTiming == IDM_3QSPEED)
		m_MenuIdTiming = IDM_FIXEDSPEED0_75;
	if (m_MenuIdTiming == IDM_HALFSPEED)
		m_MenuIdTiming = IDM_FIXEDSPEED0_5;

	switch (m_MenuIdTiming)
	{
	default:
	case IDM_REALTIME:
		m_RealTimeTarget = 1.0;
		m_FPSTarget = 0;
		break;

	case IDM_FIXEDSPEED100:
		m_RealTimeTarget = 100.0;
		m_FPSTarget = 0;
		break;

	case IDM_FIXEDSPEED50:
		m_RealTimeTarget = 50.0;
		m_FPSTarget = 0;
		break;

	case IDM_FIXEDSPEED10:
		m_RealTimeTarget = 10.0;
		m_FPSTarget = 0;
		break;

	case IDM_FIXEDSPEED5:
		m_RealTimeTarget = 5.0;
		m_FPSTarget = 0;
		break;

	case IDM_FIXEDSPEED2:
		m_RealTimeTarget = 2.0;
		m_FPSTarget = 0;
		break;

	case IDM_FIXEDSPEED1_5:
		m_RealTimeTarget = 1.5;
		m_FPSTarget = 0;
		break;

	case IDM_FIXEDSPEED1_25:
		m_RealTimeTarget = 1.25;
		m_FPSTarget = 0;
		break;

	case IDM_FIXEDSPEED1_1:
		m_RealTimeTarget = 1.1;
		m_FPSTarget = 0;
		break;

	case IDM_FIXEDSPEED0_9:
		m_RealTimeTarget = 0.9;
		m_FPSTarget = 0;
		break;

	case IDM_FIXEDSPEED0_5:
		m_RealTimeTarget = 0.5;
		m_FPSTarget = 0;
		break;

	case IDM_FIXEDSPEED0_75:
		m_RealTimeTarget = 0.75;
		m_FPSTarget = 0;
		break;

	case IDM_FIXEDSPEED0_25:
		m_RealTimeTarget = 0.25;
		m_FPSTarget = 0;
		break;

	case IDM_FIXEDSPEED0_1:
		m_RealTimeTarget = 0.1;
		m_FPSTarget = 0;
		break;

	case IDM_50FPS:
		m_FPSTarget = 50;
		m_RealTimeTarget = 0;
		break;

	case IDM_25FPS:
		m_FPSTarget = 25;
		m_RealTimeTarget = 0;
		break;

	case IDM_10FPS:
		m_FPSTarget = 10;
		m_RealTimeTarget = 0;
		break;

	case IDM_5FPS:
		m_FPSTarget = 5;
		m_RealTimeTarget = 0;
		break;

	case IDM_1FPS:
		m_FPSTarget = 1;
		m_RealTimeTarget = 0;
		break;
	}

	ResetTiming();
}

void BeebWin::AdjustSpeed(bool up)
{
	static int speeds[] = {
				IDM_FIXEDSPEED100,
				IDM_FIXEDSPEED50,
				IDM_FIXEDSPEED10,
				IDM_FIXEDSPEED5,
				IDM_FIXEDSPEED2,
				IDM_FIXEDSPEED1_5,
				IDM_FIXEDSPEED1_25,
				IDM_FIXEDSPEED1_1,
				IDM_REALTIME,
				IDM_FIXEDSPEED0_9,
				IDM_FIXEDSPEED0_75,
				IDM_FIXEDSPEED0_5,
				IDM_FIXEDSPEED0_25,
				IDM_FIXEDSPEED0_1,
				0};
	int s = 0;
	int t = m_MenuIdTiming;

	while (speeds[s] != 0 && speeds[s] != m_MenuIdTiming)
		s++;

	if (speeds[s] == 0)
	{
		t = IDM_REALTIME;
	}
	else if (up)
	{
		if (s > 0)
			t = speeds[s-1];
	}
	else
	{
		if (speeds[s+1] != 0)
			t = speeds[s+1];
	}

	if (t != m_MenuIdTiming)
	{
		CheckMenuItem(m_hMenu, m_MenuIdTiming, MF_UNCHECKED);
		m_MenuIdTiming = t;
		CheckMenuItem(m_hMenu, m_MenuIdTiming, MF_CHECKED);
		TranslateTiming();
	}
}

/****************************************************************************/
void BeebWin::TranslateKeyMapping(void)
{
	switch (m_MenuIdKeyMapping)
	{
	default:
	case IDM_DEFAULTKYBDMAPPING:
		transTable = &defaultMapping;
		break;

	case IDM_LOGICALKYBDMAPPING:
		transTable = &logicalMapping;
		break;

	case IDM_USERKYBDMAPPING:
		transTable = &UserKeymap;
		break;
	}
}

/****************************************************************************/
void BeebWin::SetTapeSpeedMenu(void) {
	CheckMenuItem(m_hMenu,ID_TAPE_FAST,(TapeClockSpeed==750)?MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(m_hMenu,ID_TAPE_MFAST,(TapeClockSpeed==1600)?MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(m_hMenu,ID_TAPE_MSLOW,(TapeClockSpeed==3200)?MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(m_hMenu,ID_TAPE_NORMAL,(TapeClockSpeed==5600)?MF_CHECKED:MF_UNCHECKED);
}

/****************************************************************************/
#define ASPECT_RATIO_X 5
#define ASPECT_RATIO_Y 4
void BeebWin::CalcAspectRatioAdjustment(int DisplayWidth, int DisplayHeight)
{
	m_XRatioAdj = 0.0f;
	m_YRatioAdj = 0.0f;
	m_XRatioCrop = 0.0f;
	m_YRatioCrop = 0.0f;

	if (m_isFullScreen)
	{
		int w = DisplayWidth * ASPECT_RATIO_Y;
		int h = DisplayHeight * ASPECT_RATIO_X;
		if (w > h)
		{
			m_XRatioAdj = (float)DisplayHeight / (float)DisplayWidth * (float)ASPECT_RATIO_X/(float)ASPECT_RATIO_Y;
			m_XRatioCrop = (1.0f - m_XRatioAdj) / 2.0f;
		}
		else if (w < h)
		{
			m_YRatioAdj = (float)DisplayWidth / (float)DisplayHeight * (float)ASPECT_RATIO_Y/(float)ASPECT_RATIO_X;
			m_YRatioCrop = (1.0f - m_YRatioAdj) / 2.0f;
		}
	}
}

/****************************************************************************/
void BeebWin::SetWindowAttributes(bool wasFullScreen)
{
	RECT wndrect;
	long style;

	if (m_isFullScreen)
	{
		if (!wasFullScreen)
		{
			GetWindowRect(m_hWnd, &wndrect);
			m_XWinPos = wndrect.left;
			m_YWinPos = wndrect.top;
		}

		if (m_DisplayRenderer != IDM_DISPGDI)
		{
			m_XWinSize = m_XDXSize;
			m_YWinSize = m_YDXSize;
			CalcAspectRatioAdjustment(m_XScrSize, m_YScrSize);

			style = GetWindowLong(m_hWnd, GWL_STYLE);
			style &= ~(WIN_STYLE);
			style |= WS_POPUP;
			SetWindowLong(m_hWnd, GWL_STYLE, style);

			if (m_DXInit == TRUE)
			{
				ResetDX();
			}
		}
		else
		{
			CalcAspectRatioAdjustment(m_XWinSize, m_YWinSize);

			style = GetWindowLong(m_hWnd, GWL_STYLE);
			style &= ~WS_POPUP;
			style |= WIN_STYLE;
			SetWindowLong(m_hWnd, GWL_STYLE, style);

			ShowWindow(m_hWnd, SW_MAXIMIZE);
		}

		// Experiment: hide menu in full screen
		if (HideMenuEnabled)
			ShowMenu(false);
	}
	else
	{
		CalcAspectRatioAdjustment(0, 0);

		if (wasFullScreen)
			ShowWindow(m_hWnd, SW_RESTORE);

		// Note: Window size gets lost in DDraw mode when DD is reset
		int xs = m_XLastWinSize;
		int ys = m_YLastWinSize;
			
		if (m_DisplayRenderer != IDM_DISPGDI && m_DXInit == TRUE)
		{
			ResetDX();
		}

		m_XWinSize = xs;
		m_YWinSize = ys;

		style = GetWindowLong(m_hWnd, GWL_STYLE);
		style &= ~WS_POPUP;
		style |= WIN_STYLE;
		SetWindowLong(m_hWnd, GWL_STYLE, style);

		SetWindowPos(m_hWnd, HWND_TOP, m_XWinPos, m_YWinPos,
					 m_XWinSize + m_XWinBorder,
					 m_YWinSize + m_YWinBorder,
					 !wasFullScreen ? SWP_NOMOVE : 0);

		// Experiment: hide menu in full screen
		if (HideMenuEnabled)
			ShowMenu(true);
	}

	// Clear unused areas of screen
	RECT rc;
	GetClientRect(m_hWnd, &rc);
	InvalidateRect(m_hWnd, &rc, TRUE);
}

/*****************************************************************************/
void BeebWin::WinSizeChange(int size, int width, int height)
{
	if (m_DisplayRenderer == IDM_DISPGDI && size == SIZE_RESTORED && m_isFullScreen)
	{
		m_isFullScreen = false;
		CheckMenuItem(m_hMenu, IDM_FULLSCREEN, MF_UNCHECKED);
	}

	if (!m_isFullScreen || m_DisplayRenderer == IDM_DISPGDI)
	{
		m_XWinSize = width;
		m_YWinSize = height;
		CalcAspectRatioAdjustment(m_XWinSize, m_YWinSize);

		if (size != SIZE_MINIMIZED && m_DisplayRenderer != IDM_DISPGDI && m_DXInit == TRUE)
		{
			m_DXResetPending = true;
		}
	}

	if (!m_isFullScreen)
	{
		m_XLastWinSize = m_XWinSize;
		m_YLastWinSize = m_YWinSize;
	}
}

/****************************************************************************/
void BeebWin::WinPosChange(int x, int y)
{
#if 0
	char str[200];
	sprintf(str, "WM_MOVE %d, %d (%d, %d)\n", x, y, m_XWinPos, m_YWinPos);
	OutputDebugString(str);
#endif
}

/****************************************************************************/
void BeebWin::TranslateAMX(void)
{
	switch (m_MenuIdAMXSize)
	{
	case IDM_AMX_160X256:
		m_AMXXSize = 160;
		m_AMXYSize = 256;
		break;
	default:
	case IDM_AMX_320X256:
		m_AMXXSize = 320;
		m_AMXYSize = 256;
		break;
	case IDM_AMX_640X256:
		m_AMXXSize = 640;
		m_AMXYSize = 256;
		break;
	}

	switch (m_MenuIdAMXAdjust)
	{
	case 0:
		m_AMXAdjust = 0;
		break;
	case IDM_AMX_ADJUSTP50:
		m_AMXAdjust = 50;
		break;
	default:
	case IDM_AMX_ADJUSTP30:
		m_AMXAdjust = 30;
		break;
	case IDM_AMX_ADJUSTP10:
		m_AMXAdjust = 10;
		break;
	case IDM_AMX_ADJUSTM10:
		m_AMXAdjust = -10;
		break;
	case IDM_AMX_ADJUSTM30:
		m_AMXAdjust = -30;
		break;
	case IDM_AMX_ADJUSTM50:
		m_AMXAdjust = -50;
		break;
	}
}

void BeebWin::UpdateSerialMenu(HMENU hMenu) {
	CheckMenuItem(hMenu, ID_SERIAL, (SerialPortEnabled)?MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_TOUCHSCREEN, (TouchScreenEnabled)?MF_CHECKED:MF_UNCHECKED);
//	CheckMenuItem(hMenu, ID_IP232, (EthernetPortEnabled)?MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_IP232LOCALHOST, (IP232localhost)?MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_IP232CUSTOM, (IP232custom)?MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_IP232MODE, (IP232mode)?MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_IP232RAW, (IP232raw)?MF_CHECKED:MF_UNCHECKED);

	CheckMenuItem(hMenu, ID_COM1, (SerialPort==1)?MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_COM2, (SerialPort==2)?MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_COM3, (SerialPort==3)?MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_COM4, (SerialPort==4)?MF_CHECKED:MF_UNCHECKED);
}

void BeebWin::ExternUpdateSerialMenu(void) {
	HMENU hMenu = m_hMenu;
	UpdateSerialMenu(hMenu);
}


//Rob
void BeebWin::UpdateEconetMenu(HMENU hMenu) {	
	CheckMenuItem(hMenu, ID_ECONET, (EconetEnabled)?MF_CHECKED:MF_UNCHECKED);
}

void BeebWin::UpdateLEDMenu(HMENU hMenu) {
	// Update the LED Menu
	CheckMenuItem(hMenu,ID_RED_LEDS,(DiscLedColour>0)?MF_UNCHECKED:MF_CHECKED);
	CheckMenuItem(hMenu,ID_GREEN_LEDS,(DiscLedColour>0)?MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(hMenu,ID_SHOW_KBLEDS,(LEDs.ShowKB)?MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(hMenu,ID_SHOW_DISCLEDS,(LEDs.ShowDisc)?MF_CHECKED:MF_UNCHECKED);
}

void BeebWin::UpdateOptiMenu(void) {
	CheckMenuItem(m_hMenu,ID_DOCONLY,(OpCodes==1)?MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(m_hMenu,ID_EXTRAS ,(OpCodes==2)?MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(m_hMenu,ID_FULLSET,(OpCodes==3)?MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(m_hMenu,ID_BHARDWARE,(BHardware==1)?MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(m_hMenu,ID_TSTYLE,(THalfMode==1)?MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(m_hMenu,ID_PSAMPLES,(PartSamples)?MF_CHECKED:MF_UNCHECKED);
}
/***************************************************************************/
void BeebWin::HandleCommand(int MenuId)
{
	char TmpPath[256];
	HMENU hMenu = m_hMenu;
	int prev_palette_type = palette_type;
	int binsize = 0;

	SetSound(MUTED);

	switch (MenuId)
	{
	case IDM_RUNDISC:
		if (ReadDisc(0,hMenu, true))
		{
			m_ShiftBooted = true;
			ResetBeebSystem(MachineType,TubeEnabled,0);
			BeebKeyDown(0, 0); // Shift key
		}
		break;

	case IDM_LOADSTATE:
		RestoreState();
		break;
	case IDM_SAVESTATE:
		SaveState();
		break;

	case IDM_QUICKLOAD:
		QuickLoad();
		break;
	case IDM_QUICKSAVE:
		QuickSave();
		break;

	case IDM_LOADDISC0:
		ReadDisc(0,hMenu, false);
		break;
	case IDM_LOADDISC1:
		ReadDisc(1,hMenu, false);
		break;
	
	case ID_LOADTAPE:
		LoadTape();
		break;

	case IDM_NEWDISC0:
		NewDiscImage(0);
		break;
	case IDM_NEWDISC1:
		NewDiscImage(1);
		break;

	case IDM_EJECTDISC0:
		EjectDiscImage(0);
		break;
	case IDM_EJECTDISC1:
		EjectDiscImage(1);
		break;

	case IDM_WPDISC0:
		ToggleWriteProtect(0);
		break;
	case IDM_WPDISC1:
		ToggleWriteProtect(1);
		break;
	case IDM_WPONLOAD:
		m_WriteProtectOnLoad = 1 - m_WriteProtectOnLoad;
		CheckMenuItem(hMenu, IDM_WPONLOAD, m_WriteProtectOnLoad ? MF_CHECKED : MF_UNCHECKED);
		break;

	case IDM_EDIT_COPY:
		doCopy();
		break;
	case IDM_EDIT_PASTE:
		doPaste();
		break;
	case IDM_EDIT_CRLF:
		m_translateCRLF = !m_translateCRLF;
		CheckMenuItem(hMenu, IDM_EDIT_CRLF, m_translateCRLF ? MF_CHECKED : MF_UNCHECKED);
		break;

	case IDM_DISC_EXPORT_0:
	case IDM_DISC_EXPORT_1:
	case IDM_DISC_EXPORT_2:
	case IDM_DISC_EXPORT_3:
		ExportDiscFiles(MenuId);
		break;

	case IDM_DISC_IMPORT_0:
	case IDM_DISC_IMPORT_1:
	case IDM_DISC_IMPORT_2:
	case IDM_DISC_IMPORT_3:
		ImportDiscFiles(MenuId);
		break;

	case IDM_PRINTER_FILE:
		if (PrinterFile())
		{
			/* If printer is enabled then need to
				disable it before changing file */
			if (PrinterEnabled)
				TogglePrinter();

			/* Add file name to menu */
			char menu_string[256];
			strcpy(menu_string, "File: ");
			strcat(menu_string, m_PrinterFileName);
			ModifyMenu(hMenu, IDM_PRINTER_FILE,
				MF_BYCOMMAND, IDM_PRINTER_FILE,
				menu_string);

			if (MenuId != m_MenuIdPrinterPort)
			{
				CheckMenuItem(hMenu, m_MenuIdPrinterPort, MF_UNCHECKED);
				m_MenuIdPrinterPort = MenuId;
				CheckMenuItem(hMenu, m_MenuIdPrinterPort, MF_CHECKED);
   			}
			TranslatePrinterPort();
		}
		break;
	case IDM_PRINTER_CLIPBOARD:
		if (PrinterEnabled)
			TogglePrinter();

		if (MenuId != m_MenuIdPrinterPort)
		{
			CheckMenuItem(hMenu, m_MenuIdPrinterPort, MF_UNCHECKED);
			m_MenuIdPrinterPort = MenuId;
			CheckMenuItem(hMenu, m_MenuIdPrinterPort, MF_CHECKED);
		}
		TranslatePrinterPort();
		break;
	case IDM_PRINTER_LPT1:
	case IDM_PRINTER_LPT2:
	case IDM_PRINTER_LPT3:
	case IDM_PRINTER_LPT4:
		if (MenuId != m_MenuIdPrinterPort)
		{
			/* If printer is enabled then need to
				disable it before changing file */
			if (PrinterEnabled)
				TogglePrinter();

			CheckMenuItem(hMenu, m_MenuIdPrinterPort, MF_UNCHECKED);
			m_MenuIdPrinterPort = MenuId;
			CheckMenuItem(hMenu, m_MenuIdPrinterPort, MF_CHECKED);
			TranslatePrinterPort();
   		}
		break;
	case IDM_PRINTERONOFF:
		TogglePrinter();
		break;

	case ID_SERIAL:

		if (SerialPortEnabled) { // so disabling..
			if (TouchScreenEnabled)
			{
				TouchScreenClose();
				TouchScreenEnabled = false;
			}
			if (EthernetPortEnabled)
			{
				IP232Close();
				EthernetPortEnabled = false;
	//			IP232custom = false;
	//			IP232localhost = false;
			}
		}
		SerialPortEnabled=1-SerialPortEnabled;

		if (SerialPortEnabled && (IP232custom || IP232localhost))
		{
			EthernetPortEnabled = true;
			if (IP232Open() == false)
			{
				MessageBox(GETHWND,"Could not connect to specified address",WindowTitle,MB_OK|MB_ICONERROR);
				bSerialStateChanged=TRUE;
				UpdateSerialMenu(hMenu);
				SerialPortEnabled=FALSE;
			}
		}
		bSerialStateChanged=TRUE;
		UpdateSerialMenu(hMenu);
		break;

	case ID_TOUCHSCREEN:

		if (TouchScreenEnabled)
		{
			TouchScreenClose();
		}
				
		TouchScreenEnabled = 1 - TouchScreenEnabled;

		if (TouchScreenEnabled)
		{
			// Also switch on analogue mousestick (touch screen uses
			// mousestick position)
			if (m_MenuIdSticks != IDM_AMOUSESTICK)
				HandleCommand(IDM_AMOUSESTICK);

			if (EthernetPortEnabled)
			{
				IP232Close();
				EthernetPortEnabled = false;
			}
			IP232custom = false;
			IP232localhost = false;

			SerialPortEnabled = true;

			Kill_Serial();
			SerialPort = 0;

			bSerialStateChanged=TRUE;
			TouchScreenOpen();
		}
		UpdateSerialMenu(hMenu);
		break;

	case ID_IP232MODE:
		IP232mode = 1 - IP232mode;
		UpdateSerialMenu(hMenu);
		break;

	case ID_IP232RAW:
		IP232raw = 1 - IP232raw;
		UpdateSerialMenu(hMenu);
		break;

	case ID_IP232LOCALHOST:

		if (IP232localhost)
		{
			IP232Close();
		}
				
		IP232localhost = 1 - IP232localhost;

		if (IP232localhost)
		{
			if (TouchScreenEnabled)
			{
				TouchScreenClose();
			}
			if (IP232custom) IP232Close();
			IP232custom = false;
			EthernetPortEnabled = true;
			TouchScreenEnabled = false;

			Kill_Serial();

			SerialPort = 0;
			bSerialStateChanged=TRUE;

			strcpy(IPAddress,"127.0.0.1");
			PortNo = 25232;

			if (SerialPortEnabled) {
				if (IP232Open() == false)
				 {
					MessageBox(GETHWND,"Could not connect to specified address",WindowTitle,MB_OK|MB_ICONERROR);
					bSerialStateChanged=TRUE;
					UpdateSerialMenu(hMenu);
					SerialPortEnabled=FALSE;
				}
			}
		}

		if (IP232localhost == 0 &&  IP232custom == 0)
		{
			EthernetPortEnabled = false;
			IP232Close();
		}

		UpdateSerialMenu(hMenu);
		break;

	case ID_IP232CUSTOM:

		if (IP232custom)
		{
			IP232Close();
		}
				
		IP232custom = 1 - IP232custom;

		if (IP232custom)
		{
			if (TouchScreenEnabled)
			{
				TouchScreenClose();
			}
			if (IP232localhost) IP232Close();
			IP232localhost = false;
			EthernetPortEnabled = true;
			TouchScreenEnabled = false;

			Kill_Serial();

			SerialPort = 0;
			bSerialStateChanged=TRUE;

			strcpy(IPAddress,IP232customip);
			PortNo = IP232customport;
			if (SerialPortEnabled) {
				if (IP232Open() == false)
				 {
					MessageBox(GETHWND,"Could not connect to specified address",WindowTitle,MB_OK|MB_ICONERROR);
					bSerialStateChanged=TRUE;
					UpdateSerialMenu(hMenu);
					SerialPortEnabled=FALSE;
				}
			}
		}

		if (IP232localhost == 0 && IP232custom == 0)
		{
			EthernetPortEnabled = false;
			IP232Close();
		}

		UpdateSerialMenu(hMenu);
		break;

	case ID_COM1:
		SerialPort=1; bSerialStateChanged=TRUE; EthernetPortEnabled = false; IP232custom = false; IP232localhost = false; TouchScreenEnabled = false; IP232Close(); TouchScreenClose(); UpdateSerialMenu(hMenu); break;

	case ID_COM2:
		SerialPort=2; bSerialStateChanged=TRUE; EthernetPortEnabled = false; IP232custom = false; IP232localhost = false; TouchScreenEnabled = false; IP232Close(); TouchScreenClose(); UpdateSerialMenu(hMenu); break;

	case ID_COM3:
		SerialPort=3; bSerialStateChanged=TRUE; EthernetPortEnabled = false; IP232custom = false; IP232localhost = false; TouchScreenEnabled = false; IP232Close(); TouchScreenClose(); UpdateSerialMenu(hMenu); break;

	case ID_COM4:
		SerialPort=4; bSerialStateChanged=TRUE; EthernetPortEnabled = false; IP232custom = false; IP232localhost = false; TouchScreenEnabled = false; IP232Close(); TouchScreenClose(); UpdateSerialMenu(hMenu); break;


	//Rob
	case ID_ECONET:
		EconetEnabled= 1-EconetEnabled;
		if (EconetEnabled)
		{
			// Need hard reset for DNFS to detect econet HW
			ResetBeebSystem(MachineType,TubeEnabled,0);
			EconetStateChanged=TRUE;
		}
		else
		{
			EconetReset();
		}
		UpdateEconetMenu(hMenu);
		break;

	case IDM_DISPGDI:
	case IDM_DISPDDRAW:
	case IDM_DISPDX9:
	{
		int enabled;
		ExitDX();

		m_DisplayRenderer = MenuId;
		SetWindowAttributes(m_isFullScreen);

		if (m_DisplayRenderer != IDM_DISPGDI)
		{
			InitDX();
			enabled = MF_ENABLED;
		}
		else
		{
			enabled = MF_GRAYED;
		}
		UpdateDisplayRendererMenu();
		EnableMenuItem(hMenu, IDM_DXSMOOTHING, enabled);
		EnableMenuItem(hMenu, IDM_DXSMOOTHMODE7ONLY, enabled);
		break;
	}

	case IDM_DXSMOOTHING:
		m_DXSmoothing = !m_DXSmoothing;
		CheckMenuItem(hMenu, IDM_DXSMOOTHING, m_DXSmoothing ? MF_CHECKED : MF_UNCHECKED);
		if (m_DisplayRenderer != IDM_DISPGDI)
		{
			UpdateSmoothing();
		}
		break;

	case IDM_DXSMOOTHMODE7ONLY:
		m_DXSmoothMode7Only = !m_DXSmoothMode7Only;
		CheckMenuItem(hMenu, IDM_DXSMOOTHMODE7ONLY, m_DXSmoothMode7Only ? MF_CHECKED : MF_UNCHECKED);
		if (m_DisplayRenderer != IDM_DISPGDI)
		{
			UpdateSmoothing();
		}
		break;

	case IDM_320X256:
	case IDM_640X512:
	case IDM_800X600:
	case IDM_1024X512:
	case IDM_1024X768:
	case IDM_1280X1024:
	case IDM_1440X1080:
	case IDM_1600X1200:
		{
			if (m_isFullScreen)
				HandleCommand(IDM_FULLSCREEN);
			//CheckMenuItem(hMenu, m_MenuIdWinSize, MF_UNCHECKED);
			m_MenuIdWinSize = MenuId;
			//CheckMenuItem(hMenu, m_MenuIdWinSize, MF_CHECKED);
			TranslateWindowSize();
			SetWindowAttributes(m_isFullScreen);
		}
		break;

	case ID_VIEW_DD_640X480:
	case ID_VIEW_DD_720X576:
	case ID_VIEW_DD_800X600:
	case ID_VIEW_DD_1024X768:
	case ID_VIEW_DD_1280X720:
	case ID_VIEW_DD_1280X768:
	case ID_VIEW_DD_1280X960:
	case ID_VIEW_DD_1280X1024:
	case ID_VIEW_DD_1440X900:
	case ID_VIEW_DD_1600X1200:
	case ID_VIEW_DD_1920X1080:
		{
			CheckMenuItem(hMenu, m_DDFullScreenMode, MF_UNCHECKED);
			m_DDFullScreenMode = MenuId;
			CheckMenuItem(hMenu, m_DDFullScreenMode, MF_CHECKED);
			TranslateDDSize();

			if (m_isFullScreen && m_DisplayRenderer != IDM_DISPGDI)
			{
				SetWindowAttributes(m_isFullScreen);
			}
		}
		break;

	case IDM_FULLSCREEN:
		m_isFullScreen = !m_isFullScreen;
		CheckMenuItem(hMenu, IDM_FULLSCREEN, m_isFullScreen ? MF_CHECKED : MF_UNCHECKED);
		SetWindowAttributes(!m_isFullScreen);
	    break;

	case IDM_MAINTAINASPECTRATIO:
		m_MaintainAspectRatio = !m_MaintainAspectRatio;
		CheckMenuItem(hMenu, IDM_MAINTAINASPECTRATIO, m_MaintainAspectRatio ? MF_CHECKED : MF_UNCHECKED);
		if (m_isFullScreen)
		{
			// Clear unused areas of screen
			RECT rc;
			GetClientRect(m_hWnd, &rc);
			InvalidateRect(m_hWnd, &rc, TRUE);
		}
		break;

	case IDM_SPEEDANDFPS:
		if (m_ShowSpeedAndFPS)
		{
			m_ShowSpeedAndFPS = FALSE;
			CheckMenuItem(hMenu, IDM_SPEEDANDFPS, MF_UNCHECKED);
			SetWindowText(m_hWnd, WindowTitle);
		}
		else
		{
			m_ShowSpeedAndFPS = TRUE;
			CheckMenuItem(hMenu, IDM_SPEEDANDFPS, MF_CHECKED);
		}
		break;

	case IDM_XAUDIO2:
	case IDM_DIRECTSOUND:
		SoundConfig::Selection = MenuId == IDM_XAUDIO2 ? SoundConfig::XAudio2 : SoundConfig::DirectSound;

		if (SoundEnabled)
		{
			SoundReset();
			SoundInit();
		}

#ifdef SPEECH_ENABLED
		if (SpeechDefault)
		{
			tms5220_stop();
			tms5220_start();
		}
#endif

		UpdateSoundStreamerMenu();
		break;

	case IDM_SOUNDONOFF:
		if (SoundDefault)
		{
			CheckMenuItem(hMenu, IDM_SOUNDONOFF, MF_UNCHECKED);
			SoundReset();
			SoundDefault=0;
		}
		else
		{
			SoundInit();
			if (SoundEnabled) {
				CheckMenuItem(hMenu, IDM_SOUNDONOFF, MF_CHECKED);
				SoundDefault=1;
			}
		}
		break;
	case IDM_SOUNDCHIP:
		SoundChipEnabled=1-SoundChipEnabled;
		CheckMenuItem(hMenu, IDM_SOUNDCHIP, SoundChipEnabled?MF_CHECKED:MF_UNCHECKED);
		break;

	case ID_SFX_RELAY:
		RelaySoundEnabled=1-RelaySoundEnabled;
		CheckMenuItem(hMenu,ID_SFX_RELAY,RelaySoundEnabled?MF_CHECKED:MF_UNCHECKED);
		break;

	case ID_SFX_DISCDRIVES:
		DiscDriveSoundEnabled=1-DiscDriveSoundEnabled;
		CheckMenuItem(hMenu,ID_SFX_DISCDRIVES,DiscDriveSoundEnabled?MF_CHECKED:MF_UNCHECKED);
		break;

	case IDM_44100KHZ:
	case IDM_22050KHZ:
	case IDM_11025KHZ:
		if (MenuId != m_MenuIdSampleRate)
		{
			CheckMenuItem(hMenu, m_MenuIdSampleRate, MF_UNCHECKED);
			m_MenuIdSampleRate = MenuId;
			CheckMenuItem(hMenu, m_MenuIdSampleRate, MF_CHECKED);
			TranslateSampleRate();

			if (SoundEnabled)
			{
				SoundReset();
				SoundInit();
			}

#ifdef SPEECH_ENABLED
			if (SpeechDefault)
			{
				tms5220_stop();
				tms5220_start();
			}
#endif
		}
		break;
	
	case IDM_FULLVOLUME:
	case IDM_HIGHVOLUME:
	case IDM_MEDIUMVOLUME:
	case IDM_LOWVOLUME:
		if (MenuId != m_MenuIdVolume)
		{
			CheckMenuItem(hMenu, m_MenuIdVolume, MF_UNCHECKED);
			m_MenuIdVolume = MenuId;
			CheckMenuItem(hMenu, m_MenuIdVolume, MF_CHECKED);
			TranslateVolume();
		}
		break;
	
	/* LRW Added switch individual ROMS Writable ON/OFF */
	case IDM_ALLOWWRITES_ROM0:
	case IDM_ALLOWWRITES_ROM1:
	case IDM_ALLOWWRITES_ROM2:
	case IDM_ALLOWWRITES_ROM3:
	case IDM_ALLOWWRITES_ROM4:
	case IDM_ALLOWWRITES_ROM5:
	case IDM_ALLOWWRITES_ROM6:
	case IDM_ALLOWWRITES_ROM7:
	case IDM_ALLOWWRITES_ROM8:
	case IDM_ALLOWWRITES_ROM9:
	case IDM_ALLOWWRITES_ROMA:
	case IDM_ALLOWWRITES_ROMB:
	case IDM_ALLOWWRITES_ROMC:
	case IDM_ALLOWWRITES_ROMD:
	case IDM_ALLOWWRITES_ROME:
	case IDM_ALLOWWRITES_ROMF:
	{
		int slot = MenuId-IDM_ALLOWWRITES_ROM0;
		RomWritable[slot] = 1-RomWritable[slot];
		CheckMenuItem(hMenu, MenuId, RomWritable[slot] ? MF_CHECKED : MF_UNCHECKED);
		break;				
	}

	case IDM_SWRAMBOARD:
		SWRAMBoardEnabled = 1-SWRAMBoardEnabled;
		CheckMenuItem(hMenu, IDM_SWRAMBOARD, SWRAMBoardEnabled ? MF_CHECKED : MF_UNCHECKED);
		break;

	case IDM_ROMCONFIG:
		EditROMConfig();
		SetRomMenu();
		break;

	case IDM_REALTIME:
	case IDM_FIXEDSPEED100:
	case IDM_FIXEDSPEED50:
	case IDM_FIXEDSPEED10:
	case IDM_FIXEDSPEED5:
	case IDM_FIXEDSPEED2:
	case IDM_FIXEDSPEED1_5:
	case IDM_FIXEDSPEED1_25:
	case IDM_FIXEDSPEED1_1:
	case IDM_FIXEDSPEED0_9:
	case IDM_FIXEDSPEED0_5:
	case IDM_FIXEDSPEED0_75:
	case IDM_FIXEDSPEED0_25:
	case IDM_FIXEDSPEED0_1:
	case IDM_50FPS:
	case IDM_25FPS:
	case IDM_10FPS:
	case IDM_5FPS:
	case IDM_1FPS:
		if (MenuId != m_MenuIdTiming)
		{
			CheckMenuItem(hMenu, m_MenuIdTiming, MF_UNCHECKED);
			m_MenuIdTiming = MenuId;
			CheckMenuItem(hMenu, m_MenuIdTiming, MF_CHECKED);
			TranslateTiming();
		}
		break;

	case IDM_JOYSTICK:
	case IDM_AMOUSESTICK:
	case IDM_DMOUSESTICK:
		/* Disable current selection */
		if (m_MenuIdSticks != 0)
		{
			CheckMenuItem(hMenu, m_MenuIdSticks, MF_UNCHECKED);
			if (m_MenuIdSticks == IDM_JOYSTICK)
			{
				ResetJoystick();
			}
			else /* mousestick */
			{
				AtoDDisable();
			}
		}

		if (MenuId == m_MenuIdSticks)
		{
			/* Joysticks switched off completely */
			m_MenuIdSticks = 0;
		}
		else
		{
			/* Initialise new selection */
			m_MenuIdSticks = MenuId;
			if (m_MenuIdSticks == IDM_JOYSTICK)
			{
				InitJoystick();
			}
			else /* mousestick */
			{
				AtoDEnable();
			}
			if (JoystickEnabled)
				CheckMenuItem(hMenu, m_MenuIdSticks, MF_CHECKED);
			else
				m_MenuIdSticks = 0;
		}
		break;

	case IDM_FREEZEINACTIVE:
		if (m_FreezeWhenInactive)
		{
			m_FreezeWhenInactive = FALSE;
			CheckMenuItem(hMenu, IDM_FREEZEINACTIVE, MF_UNCHECKED);
		}
		else
		{
			m_FreezeWhenInactive = TRUE;
			CheckMenuItem(hMenu, IDM_FREEZEINACTIVE, MF_CHECKED);
		}
		break;

	case IDM_HIDECURSOR:
		if (m_HideCursor)
		{
			m_HideCursor = FALSE;
			CheckMenuItem(hMenu, IDM_HIDECURSOR, MF_UNCHECKED);
		}
		else
		{
			m_HideCursor = TRUE;
			CheckMenuItem(hMenu, IDM_HIDECURSOR, MF_CHECKED);
		}
		break;

	case IDM_IGNOREILLEGALOPS:
		if (IgnoreIllegalInstructions)
		{
			IgnoreIllegalInstructions = FALSE;
			CheckMenuItem(hMenu, IDM_IGNOREILLEGALOPS, MF_UNCHECKED);
		}
		else
		{
			IgnoreIllegalInstructions = TRUE;
			CheckMenuItem(hMenu, IDM_IGNOREILLEGALOPS, MF_CHECKED);
		}
		break;

	case IDM_DEFINEKEYMAP:
		UserKeyboardDialog( m_hWnd );
		break;

	case IDM_LOADKEYMAP:
		LoadUserKeyMap();
		break;

	case IDM_SAVEKEYMAP:
		SaveUserKeyMap();
		break;

	case IDM_USERKYBDMAPPING:
	case IDM_DEFAULTKYBDMAPPING:
	case IDM_LOGICALKYBDMAPPING:
		if (MenuId != m_MenuIdKeyMapping)
		{
			CheckMenuItem(hMenu, m_MenuIdKeyMapping, MF_UNCHECKED);
			m_MenuIdKeyMapping = MenuId;
			CheckMenuItem(hMenu, m_MenuIdKeyMapping, MF_CHECKED);
			TranslateKeyMapping();
		}
		break;

	case IDM_MAPAS:
		if (m_KeyMapAS)
		{
			m_KeyMapAS = FALSE;
			CheckMenuItem(hMenu, IDM_MAPAS, MF_UNCHECKED);
		}
		else
		{
			m_KeyMapAS = TRUE;
			CheckMenuItem(hMenu, IDM_MAPAS, MF_CHECKED);
		}
		break;
	case IDM_MAPFUNCS:
		if (m_KeyMapFunc)
		{
			m_KeyMapFunc = FALSE;
			CheckMenuItem(hMenu, IDM_MAPFUNCS, MF_UNCHECKED);
		}
		else
		{
			m_KeyMapFunc = TRUE;
			CheckMenuItem(hMenu, IDM_MAPFUNCS, MF_CHECKED);
		}
		break;

	case IDM_ABOUT:
		MessageBox(m_hWnd, AboutText, WindowTitle, MB_OK);
		break;

	case IDM_VIEWREADME:
		strcpy(TmpPath, m_AppPath);
		strcat(TmpPath, "Help\\index.html");
		ShellExecute(m_hWnd, NULL, TmpPath, NULL, NULL, SW_SHOWNORMAL);;
		break;

	case IDM_EXIT:
		PostMessage(m_hWnd, WM_CLOSE, 0, 0L);
		break;

	case IDM_SAVE_PREFS:
		SavePreferences(true);
		break;

	case IDM_AUTOSAVE_PREFS_CMOS:
		m_AutoSavePrefsCMOS = !m_AutoSavePrefsCMOS;
		CheckMenuItem(hMenu, IDM_AUTOSAVE_PREFS_CMOS, m_AutoSavePrefsCMOS ? MF_CHECKED : MF_UNCHECKED);
		m_AutoSavePrefsChanged = true;
		break;
	case IDM_AUTOSAVE_PREFS_FOLDERS:
		m_AutoSavePrefsFolders = !m_AutoSavePrefsFolders;
		CheckMenuItem(hMenu, IDM_AUTOSAVE_PREFS_FOLDERS, m_AutoSavePrefsFolders ? MF_CHECKED : MF_UNCHECKED);
		m_AutoSavePrefsChanged = true;
		break;
	case IDM_AUTOSAVE_PREFS_ALL:
		m_AutoSavePrefsAll = !m_AutoSavePrefsAll;
		CheckMenuItem(hMenu, IDM_AUTOSAVE_PREFS_ALL, m_AutoSavePrefsAll ? MF_CHECKED : MF_UNCHECKED);
		m_AutoSavePrefsChanged = true;
		break;
	case IDM_SELECT_USER_DATA_FOLDER:
		SelectUserDataPath();
		break;

	case IDM_AMXONOFF:
		if (AMXMouseEnabled)
		{
			CheckMenuItem(hMenu, IDM_AMXONOFF, MF_UNCHECKED);
			AMXMouseEnabled = FALSE;
		}
		else
		{
			CheckMenuItem(hMenu, IDM_AMXONOFF, MF_CHECKED);
			AMXMouseEnabled = TRUE;
		}
		break;

	case IDM_AMX_LRFORMIDDLE:
		if (AMXLRForMiddle)
		{
			CheckMenuItem(hMenu, IDM_AMX_LRFORMIDDLE, MF_UNCHECKED);
			AMXLRForMiddle = FALSE;
		}
		else
		{
			CheckMenuItem(hMenu, IDM_AMX_LRFORMIDDLE, MF_CHECKED);
			AMXLRForMiddle = TRUE;
		}
		break;

	case IDM_AMX_160X256:
	case IDM_AMX_320X256:
	case IDM_AMX_640X256:
		if (MenuId != m_MenuIdAMXSize)
		{
			CheckMenuItem(hMenu, m_MenuIdAMXSize, MF_UNCHECKED);
			m_MenuIdAMXSize = MenuId;
			CheckMenuItem(hMenu, m_MenuIdAMXSize, MF_CHECKED);
   		}
		TranslateAMX();
		break;

	case IDM_AMX_ADJUSTP50:
	case IDM_AMX_ADJUSTP30:
	case IDM_AMX_ADJUSTP10:
	case IDM_AMX_ADJUSTM10:
	case IDM_AMX_ADJUSTM30:
	case IDM_AMX_ADJUSTM50:
		if (m_MenuIdAMXAdjust != 0)
		{
			CheckMenuItem(hMenu, m_MenuIdAMXAdjust, MF_UNCHECKED);
		}

		if (MenuId != m_MenuIdAMXAdjust)
		{
			m_MenuIdAMXAdjust = MenuId;
			CheckMenuItem(hMenu, m_MenuIdAMXAdjust, MF_CHECKED);
   		}
		else
		{
			m_MenuIdAMXAdjust = 0;
		}
		TranslateAMX();
		break;

	case ID_MONITOR_RGB:
		palette_type = RGB;
		CreateBitmap();
		break;
	case ID_MONITOR_BW:
		palette_type = BW;
		CreateBitmap();
		break;
	case ID_MONITOR_GREEN:
		palette_type = GREEN;
		CreateBitmap();
		break;
	case ID_MONITOR_AMBER:
		palette_type = AMBER;
		CreateBitmap();
		break;

	case IDM_ARM:
		ArmTube=1-ArmTube;
        TubeEnabled = 0;
#ifdef M512COPRO_ENABLED
        Tube186Enabled = 0;
#endif
        TorchTube = 0;
		AcornZ80 = 0;
        CheckMenuItem(hMenu, IDM_TUBE, (TubeEnabled)?MF_CHECKED:MF_UNCHECKED);
#ifdef M512COPRO_ENABLED
        CheckMenuItem(hMenu, IDM_TUBE186, (Tube186Enabled)?MF_CHECKED:MF_UNCHECKED);
#endif
        CheckMenuItem(hMenu, IDM_TORCH, (TorchTube)?MF_CHECKED:MF_UNCHECKED);
        CheckMenuItem(hMenu, IDM_ARM, (ArmTube)?MF_CHECKED:MF_UNCHECKED);
		CheckMenuItem(hMenu, IDM_ACORNZ80, (AcornZ80)?MF_CHECKED:MF_UNCHECKED);
		ResetBeebSystem(MachineType,TubeEnabled,0);
		break;

	case IDM_TUBE:
		TubeEnabled=1-TubeEnabled;
#ifdef M512COPRO_ENABLED
        Tube186Enabled = 0;
#endif
        TorchTube = 0;
        ArmTube = 0;
		AcornZ80 = 0;
        CheckMenuItem(hMenu, IDM_TUBE, (TubeEnabled)?MF_CHECKED:MF_UNCHECKED);
        CheckMenuItem(hMenu, IDM_ARM, (ArmTube)?MF_CHECKED:MF_UNCHECKED);
#ifdef M512COPRO_ENABLED
        CheckMenuItem(hMenu, IDM_TUBE186, (Tube186Enabled)?MF_CHECKED:MF_UNCHECKED);
#endif
        CheckMenuItem(hMenu, IDM_TORCH, (TorchTube)?MF_CHECKED:MF_UNCHECKED);
		CheckMenuItem(hMenu, IDM_ACORNZ80, (AcornZ80)?MF_CHECKED:MF_UNCHECKED);
		ResetBeebSystem(MachineType,TubeEnabled,0);
		break;

#ifdef M512COPRO_ENABLED
	case IDM_TUBE186:
		Tube186Enabled=1-Tube186Enabled;
        TubeEnabled = 0;
        TorchTube = 0;
		AcornZ80 = 0;
        ArmTube = 0;
        CheckMenuItem(hMenu, IDM_ARM, (ArmTube)?MF_CHECKED:MF_UNCHECKED);
        CheckMenuItem(hMenu, IDM_TUBE, (TubeEnabled)?MF_CHECKED:MF_UNCHECKED);
        CheckMenuItem(hMenu, IDM_TUBE186, (Tube186Enabled)?MF_CHECKED:MF_UNCHECKED);
        CheckMenuItem(hMenu, IDM_TORCH, (TorchTube)?MF_CHECKED:MF_UNCHECKED);
		CheckMenuItem(hMenu, IDM_ACORNZ80, (AcornZ80)?MF_CHECKED:MF_UNCHECKED);
		ResetBeebSystem(MachineType,TubeEnabled,0);
		break;
#endif

    case IDM_TORCH:
		TorchTube=1-TorchTube;
		TubeEnabled=0;
#ifdef M512COPRO_ENABLED
		Tube186Enabled=0;
#endif
		AcornZ80 = 0;
        ArmTube = 0;
        CheckMenuItem(hMenu, IDM_ARM, (ArmTube)?MF_CHECKED:MF_UNCHECKED);
        CheckMenuItem(hMenu, IDM_TUBE, (TubeEnabled)?MF_CHECKED:MF_UNCHECKED);
#ifdef M512COPRO_ENABLED
        CheckMenuItem(hMenu, IDM_TUBE186, (Tube186Enabled)?MF_CHECKED:MF_UNCHECKED);
#endif
		CheckMenuItem(hMenu, IDM_TORCH, (TorchTube)?MF_CHECKED:MF_UNCHECKED);
		CheckMenuItem(hMenu, IDM_ACORNZ80, (AcornZ80)?MF_CHECKED:MF_UNCHECKED);
		ResetBeebSystem(MachineType,TubeEnabled,0);
		break;
    
    case IDM_ACORNZ80:
		AcornZ80=1-AcornZ80;
		TubeEnabled=0;
		TorchTube=0;
#ifdef M512COPRO_ENABLED
		Tube186Enabled=0;
#endif
        ArmTube = 0;
        CheckMenuItem(hMenu, IDM_ARM, (ArmTube)?MF_CHECKED:MF_UNCHECKED);
		CheckMenuItem(hMenu, IDM_TUBE, (TubeEnabled)?MF_CHECKED:MF_UNCHECKED);
#ifdef M512COPRO_ENABLED
		CheckMenuItem(hMenu, IDM_TUBE186, (Tube186Enabled)?MF_CHECKED:MF_UNCHECKED);
#endif
		CheckMenuItem(hMenu, IDM_TORCH, (TorchTube)?MF_CHECKED:MF_UNCHECKED);
		CheckMenuItem(hMenu, IDM_ACORNZ80, (AcornZ80)?MF_CHECKED:MF_UNCHECKED);
		ResetBeebSystem(MachineType,TubeEnabled,0);
		break;

    case ID_FILE_RESET:
		ResetBeebSystem(MachineType,TubeEnabled,0);
		break;

	case ID_MODELB:
		if (MachineType!=0)
		{
			ResetBeebSystem(0,EnableTube,1);
			UpdateModelType();
		}
		break;
	case ID_MODELBINT:
		if (MachineType!=1)
		{
			ResetBeebSystem(1,EnableTube,1);
			UpdateModelType();
		}
		break;
	case ID_MODELBP:
		if (MachineType!=2)
		{
			ResetBeebSystem(2,EnableTube,1);
			UpdateModelType();
		}
		break;
	case ID_MASTER128:
		if (MachineType!=3)
		{
			ResetBeebSystem(3,EnableTube,1);
			UpdateModelType();
		}
		break;

	case ID_REWINDTAPE:
		RewindTape();
		break;
	case ID_UNLOCKTAPE:
		UnlockTape=1-UnlockTape;
		SetUnlockTape(UnlockTape);
		CheckMenuItem(hMenu, ID_UNLOCKTAPE, (UnlockTape)?MF_CHECKED:MF_UNCHECKED);
		break;

	case ID_HIDEMENU:
		HideMenuEnabled=1-HideMenuEnabled;
		CheckMenuItem(hMenu, ID_HIDEMENU, (HideMenuEnabled)?MF_CHECKED:MF_UNCHECKED);
		break;

	case ID_RED_LEDS:
		DiscLedColour=0;
		UpdateLEDMenu(hMenu);
		break;
	case ID_GREEN_LEDS:
		DiscLedColour=1;
		UpdateLEDMenu(hMenu);
		break;
	case ID_SHOW_KBLEDS:
		LEDs.ShowKB=!LEDs.ShowKB;
		UpdateLEDMenu(hMenu);
		break;
	case ID_SHOW_DISCLEDS:
		LEDs.ShowDisc=!LEDs.ShowDisc;
		UpdateLEDMenu(hMenu);
		break;

	case ID_FDC_DLL:
		if (MachineType != 3)
			SelectFDC();
		break;
	case ID_8271:
		KillDLLs();
		NativeFDC=TRUE;
		CheckMenuItem(m_hMenu,ID_8271,MF_CHECKED);
		CheckMenuItem(m_hMenu,ID_FDC_DLL,MF_UNCHECKED);
		if (MachineType != 3)
		{
			char CfgName[20];
			sprintf(CfgName, "FDCDLL%d", MachineType);
			PrefsSetStringValue(CfgName,"None");
		}
		break;

	case ID_TAPE_FAST:
		SetTapeSpeed(750);
		SetTapeSpeedMenu();
		break;
	case ID_TAPE_MFAST:
		SetTapeSpeed(1600);
		SetTapeSpeedMenu();
		break;
	case ID_TAPE_MSLOW:
		SetTapeSpeed(3200);
		SetTapeSpeedMenu();
		break;
	case ID_TAPE_NORMAL:
		SetTapeSpeed(5600);
		SetTapeSpeedMenu();
		break;
	case ID_TAPESOUND:
		TapeSoundEnabled=!TapeSoundEnabled;
		CheckMenuItem(m_hMenu,ID_TAPESOUND,(TapeSoundEnabled)?MF_CHECKED:MF_UNCHECKED);
		break;

	case ID_TAPECONTROL:
		if (TapeControlEnabled)
			TapeControlCloseDialog();
		else
			TapeControlOpenDialog(hInst, m_hWnd);
		break;

	case ID_BREAKOUT:
		if (mBreakOutWindow)
			BreakOutCloseDialog();
		else
			BreakOutOpenDialog(hInst, m_hWnd);
		break;

	case ID_UPRM:
		RTC_Enabled = 1 - RTC_Enabled;
		CheckMenuItem(m_hMenu, ID_UPRM, RTC_Enabled ? MF_CHECKED:MF_UNCHECKED);
		break;

	case ID_PBUFF:
		UsePrimaryBuffer=1-UsePrimaryBuffer;
		SetPBuff();
		SoundReset();
		if (SoundDefault) SoundInit();
		break;
	case ID_TSTYLE:
		THalfMode=1-THalfMode;
		UpdateOptiMenu();
		break;

	case ID_DOCONLY:
		OpCodes=1;
		UpdateOptiMenu();
		break;
	case ID_EXTRAS:
		OpCodes=2;
		UpdateOptiMenu();
		break;
	case ID_FULLSET:
		OpCodes=3;
		UpdateOptiMenu();
		break;
	case ID_BHARDWARE:
		BHardware=1-BHardware;
		UpdateOptiMenu();
		break;
	case ID_PSAMPLES:
		PartSamples=1-PartSamples;
		UpdateOptiMenu();
		break;
	case IDM_EXPVOLUME:
		SoundExponentialVolume=1-SoundExponentialVolume;
		CheckMenuItem(m_hMenu, IDM_EXPVOLUME, SoundExponentialVolume ? MF_CHECKED:MF_UNCHECKED);
		break;

	case IDM_SHOWDEBUGGER:
		if (DebugEnabled)
			DebugCloseDialog();
		else
			DebugOpenDialog(hInst, m_hWnd);
		break;

	case IDM_BLUR_OFF:
	case IDM_BLUR_2:
	case IDM_BLUR_4:
	case IDM_BLUR_8:
		if (MenuId != m_MotionBlur)
		{
			CheckMenuItem(hMenu, m_MotionBlur, MF_UNCHECKED);
			m_MotionBlur = MenuId;
			CheckMenuItem(hMenu, m_MotionBlur, MF_CHECKED);
		}
		break;

	case IDM_CAPTURERES1:
	case IDM_CAPTURERES2:
	case IDM_CAPTURERES3:
		if (MenuId != m_MenuIdCaptureResolution)
		{
			CheckMenuItem(hMenu, m_MenuIdCaptureResolution, MF_UNCHECKED);
			m_MenuIdCaptureResolution = MenuId;
			CheckMenuItem(hMenu, m_MenuIdCaptureResolution, MF_CHECKED);
		}
		break;
	case IDM_CAPTUREBMP:
	case IDM_CAPTUREJPEG:
	case IDM_CAPTUREGIF:
	case IDM_CAPTUREPNG:
		if (MenuId != m_MenuIdCaptureFormat)
		{
			CheckMenuItem(hMenu, m_MenuIdCaptureFormat, MF_UNCHECKED);
			m_MenuIdCaptureFormat = MenuId;
			CheckMenuItem(hMenu, m_MenuIdCaptureFormat, MF_CHECKED);
		}
		break;
	case IDM_CAPTURESCREEN:
		// Prompt for file name.  Need to do this in WndProc otherwise
		// dialog does not show in full screen mode.
		if (GetImageFile(m_CaptureFileName))
			CaptureBitmapPending(false);
		break;

	case IDM_VIDEORES1:
	case IDM_VIDEORES2:
	case IDM_VIDEORES3:
		if (MenuId != m_MenuIdAviResolution)
		{
			CheckMenuItem(hMenu, m_MenuIdAviResolution, MF_UNCHECKED);
			m_MenuIdAviResolution = MenuId;
			CheckMenuItem(hMenu, m_MenuIdAviResolution, MF_CHECKED);
		}
		break;
	case IDM_VIDEOSKIP0:
	case IDM_VIDEOSKIP1:
	case IDM_VIDEOSKIP2:
	case IDM_VIDEOSKIP3:
	case IDM_VIDEOSKIP4:
	case IDM_VIDEOSKIP5:
		if (MenuId != m_MenuIdAviSkip)
		{
			CheckMenuItem(hMenu, m_MenuIdAviSkip, MF_UNCHECKED);
			m_MenuIdAviSkip = MenuId;
			CheckMenuItem(hMenu, m_MenuIdAviSkip, MF_CHECKED);
		}
		break;
	case IDM_CAPTUREVIDEO:
		CaptureVideo();
		break;
	case IDM_ENDVIDEO:
		if (aviWriter != NULL)
		{
			delete aviWriter;
			aviWriter = NULL;
		}
		break;

#ifdef SPEECH_ENABLED
	case IDM_SPEECH:
		if (SpeechDefault)
		{
			CheckMenuItem(hMenu, IDM_SPEECH, MF_UNCHECKED);
			tms5220_stop();
			SpeechDefault = 0;
		}
		else
		{
			tms5220_start();
			if (SpeechEnabled)
			{
				CheckMenuItem(hMenu, IDM_SPEECH, MF_CHECKED);
				SpeechDefault = 1;
			}
		}
		break;
#endif

	case ID_TELETEXT:
		TeleTextAdapterEnabled = 1-TeleTextAdapterEnabled;
		TeleTextInit();
		CheckMenuItem(hMenu, ID_TELETEXT, TeleTextAdapterEnabled ? MF_CHECKED:MF_UNCHECKED);
		break;

	case ID_HARDDRIVE:
		SCSIDriveEnabled = 1-SCSIDriveEnabled;
		SCSIReset();
		SASIReset();
		CheckMenuItem(hMenu, ID_HARDDRIVE, SCSIDriveEnabled ? MF_CHECKED:MF_UNCHECKED);
		if (SCSIDriveEnabled) {
			IDEDriveEnabled=0;
			CheckMenuItem(hMenu, ID_IDEDRIVE, IDEDriveEnabled ? MF_CHECKED:MF_UNCHECKED);
			}
		break;

	case ID_IDEDRIVE:
		IDEDriveEnabled = 1-IDEDriveEnabled;
		IDEReset();
		CheckMenuItem(hMenu, ID_IDEDRIVE, IDEDriveEnabled ? MF_CHECKED:MF_UNCHECKED);
		if (IDEDriveEnabled) {
			SCSIDriveEnabled=0;
			CheckMenuItem(hMenu, ID_HARDDRIVE, SCSIDriveEnabled ? MF_CHECKED:MF_UNCHECKED);
			}
		break;

	case ID_FLOPPYDRIVE:
		Disc8271Enabled = 1-Disc8271Enabled;
		Disc1770Enabled = 1-Disc1770Enabled;
		CheckMenuItem(hMenu, ID_FLOPPYDRIVE, Disc8271Enabled ? MF_CHECKED:MF_UNCHECKED);
		break;

    case ID_RTCY2KADJUST:
		RTCY2KAdjust = 1-RTCY2KAdjust;
		RTCInit();
		CheckMenuItem(hMenu, ID_RTCY2KADJUST, RTCY2KAdjust ? MF_CHECKED:MF_UNCHECKED);
		break;

	case IDM_TEXTTOSPEECH:
		m_TextToSpeechEnabled = 1-m_TextToSpeechEnabled;
		InitTextToSpeech();
		break;

	case IDM_TEXTVIEW:
		m_TextViewEnabled = 1-m_TextViewEnabled;
		InitTextView();
		break;

	case IDM_DISABLEKEYSWINDOWS:
	{
		bool reboot = false;
		m_DisableKeysWindows=1-m_DisableKeysWindows;
		UpdateDisableKeysMenu();
		if (m_DisableKeysWindows)
		{
			// Give user warning
			if (MessageBox(m_hWnd,"Disabling the Windows keys will affect the whole PC.\n"
						   "Go ahead and disable the Windows keys?",
						   WindowTitle,MB_YESNO|MB_ICONQUESTION) == IDYES)
			{
				int binsize=sizeof(CFG_DISABLE_WINDOWS_KEYS);
				RegSetBinaryValue(HKEY_LOCAL_MACHINE, CFG_KEYBOARD_LAYOUT,
								  CFG_SCANCODE_MAP, CFG_DISABLE_WINDOWS_KEYS, &binsize);
				reboot = true;
			}
			else
			{
				m_DisableKeysWindows=0;
				UpdateDisableKeysMenu();
			}
		}
		else
		{
			int binsize=0;
			RegSetBinaryValue(HKEY_LOCAL_MACHINE, CFG_KEYBOARD_LAYOUT,
							  CFG_SCANCODE_MAP, CFG_DISABLE_WINDOWS_KEYS, &binsize);
			reboot = true;
		}

		if (reboot)
		{
			// Ask user for reboot
			if (MessageBox(m_hWnd,"Reboot required for key change to\ntake effect. Reboot now?",
						   WindowTitle,MB_YESNO|MB_ICONQUESTION) == IDYES)
			{
				RebootSystem();
			}
		}
		break;
	}
	case IDM_DISABLEKEYSBREAK:
		m_DisableKeysBreak=1-m_DisableKeysBreak;
		UpdateDisableKeysMenu();
		break;
	case IDM_DISABLEKEYSESCAPE:
		m_DisableKeysEscape=1-m_DisableKeysEscape;
		UpdateDisableKeysMenu();
		break;
	case IDM_DISABLEKEYSSHORTCUT:
		m_DisableKeysShortcut=1-m_DisableKeysShortcut;
		UpdateDisableKeysMenu();
		break;
	case IDM_DISABLEKEYSALL:
		if (m_DisableKeysWindows && m_DisableKeysBreak &&
			m_DisableKeysEscape && m_DisableKeysShortcut)
		{
			if (m_DisableKeysWindows)
				HandleCommand(IDM_DISABLEKEYSWINDOWS);
			m_DisableKeysBreak=0;
			m_DisableKeysEscape=0;
			m_DisableKeysShortcut=0;
		}
		else
		{
			if (!m_DisableKeysWindows)
				HandleCommand(IDM_DISABLEKEYSWINDOWS);
			m_DisableKeysBreak=1;
			m_DisableKeysEscape=1;
			m_DisableKeysShortcut=1;
		}
		UpdateDisableKeysMenu();
		break;
	}

	SetSound(UNMUTED);
	if (palette_type != prev_palette_type)
	{
		CreateBitmap();
		UpdateMonitorMenu();
	}
}

void BeebWin::SetPBuff(void) {
	CheckMenuItem(m_hMenu,ID_PBUFF,(UsePrimaryBuffer)?MF_CHECKED:MF_UNCHECKED);
}

void BeebWin::SetSoundMenu(void) {
	CheckMenuItem(m_hMenu,IDM_SOUNDONOFF,(SoundEnabled && SoundDefault)?MF_CHECKED:MF_UNCHECKED);
	SetPBuff();
}

void BeebWin::Activate(BOOL active)
{
	if (active)
		m_frozen = FALSE;
	else if (m_FreezeWhenInactive)
		m_frozen = TRUE;
}

void BeebWin::Focus(BOOL gotit)
{
	if (gotit && m_TextViewEnabled)
	{
		SetFocus(m_hTextView);
	}
}

BOOL BeebWin::IsFrozen(void)
{
	return m_frozen;
}

/*****************************************************************************/
// Parse command line parameters

void BeebWin::ParseCommandLine()
{
	char errstr[200];
	int i;
	int a;
	bool invalid;

	m_CommandLineFileName1[0] = 0;
	m_CommandLineFileName2[0] = 0;

	i = 1;
	while (i < __argc)
	{
		// Params with no arguments
		if (_stricmp(__argv[i], "-DisMenu") == 0)
		{
			DisableMenu = 1;
		}
		else if (_stricmp(__argv[i], "-NoAutoBoot") == 0)
		{
			m_NoAutoBoot = true;
		}
		else if (_stricmp(__argv[i], "-FullScreen") == 0)
		{
			m_startFullScreen = true;
		}
		else if (__argv[i][0] == '-' && i+1 >= __argc)
		{
			sprintf(errstr,"Invalid command line parameter:\n  %s", __argv[i]);
			MessageBox(m_hWnd,errstr,WindowTitle,MB_OK|MB_ICONERROR);
		}
		else // Params with additional arguments
		{
			invalid = false;
			if (_stricmp(__argv[i], "-Data") == 0)
			{
				strcpy(m_UserDataPath, __argv[++i]);

				if (strcmp(m_UserDataPath, "-") == 0)
				{
					// Use app path
					strcpy(m_UserDataPath, m_AppPath);
					strcat(m_UserDataPath, "UserData\\");
				}
				else
				{
					if (m_UserDataPath[strlen(m_UserDataPath)-1] != '\\' &&
						m_UserDataPath[strlen(m_UserDataPath)-1] != '/')
					{
						strcat(m_UserDataPath, "\\");
					}
				}
			}
			else if (_stricmp(__argv[i], "-Prefs") == 0)
			{
				strcpy(m_PrefsFile, __argv[++i]);
			}
			else if (_stricmp(__argv[i], "-Roms") == 0)
			{
				strcpy(RomFile, __argv[++i]);
			}
			else if (_stricmp(__argv[i], "-EcoStn") == 0)
			{
				a = atoi(__argv[++i]);
				if (a < 1 || a > 254)
					invalid = true;
				else
					EconetStationNumber = a;
			}
			else if (_stricmp(__argv[i], "-EcoFF") == 0)
			{
				a = atoi(__argv[++i]);
				if (a < 1)
					invalid = true;
				else
					EconetFlagFillTimeout = a;
			}
			else if (_stricmp(__argv[i], "-KbdCmd") == 0)
			{
				strncpy(m_KbdCmd, __argv[++i], sizeof(m_KbdCmd));
			}
			else if (_stricmp(__argv[i], "-DebugScript") == 0)
			{
				strncpy(m_DebugScript, __argv[++i], sizeof(m_DebugScript));
			}
			else if (__argv[i][0] == '-')
			{
				invalid = true;
				++i;
			}
			else
			{
				// Assume its a file name
				if (m_CommandLineFileName1[0] == 0)
					strncpy(m_CommandLineFileName1, __argv[i], _MAX_PATH);
				else if (m_CommandLineFileName2[0] == 0)
					strncpy(m_CommandLineFileName2, __argv[i], _MAX_PATH);
			}

			if (invalid)
			{
				sprintf(errstr,"Invalid command line parameter:\n  %s %s", __argv[i-1], __argv[i]);
				MessageBox(m_hWnd,errstr,WindowTitle,MB_OK|MB_ICONERROR);
			}
		}

		++i;
	}
}

/*****************************************************************************/
// Check for preference files in the same directory as the file specified
void BeebWin::CheckForLocalPrefs(const char *path, bool bLoadPrefs)
{
	char file[_MAX_PATH];
	char drive[_MAX_DRIVE];
	char dir[_MAX_DIR];
	FILE *fd;

	if (path[0] == 0)
		return;

	_splitpath(path, drive, dir, NULL, NULL);

	// Look for prefs file
	_makepath(file, drive, dir, "Preferences", "cfg");
	fd = fopen(file, "r");
	if (fd != NULL)
	{
		fclose(fd);
		// File exists, use it
		strcpy(m_PrefsFile, file);
		if (bLoadPrefs)
		{
			LoadPreferences();

			// Reinit with new prefs
			SetWindowPos(m_hWnd, HWND_TOP, m_XWinPos, m_YWinPos,
						 0, 0, SWP_NOSIZE);
			HandleCommand(m_DisplayRenderer);
			InitMenu();
			SetWindowText(m_hWnd, WindowTitle);
			if (m_MenuIdSticks == IDM_JOYSTICK)
				InitJoystick();
		}
	}

	// Look for ROMs file
	_makepath(file, drive, dir, "Roms", "cfg");
	fd = fopen(file, "r");
	if (fd != NULL)
	{
		fclose(fd);
		// File exists, use it
		strcpy(RomFile, file);
		if (bLoadPrefs)
		{
			ReadROMFile(RomFile, RomConfig);
			BeebReadRoms();
		}
	}
}

/*****************************************************************************/
// File location of a file passed on command line
	
void BeebWin::FindCommandLineFile(char *CmdLineFile)
{
	bool ssd = false;
	bool dsd = false;
	bool adfs = false;
	bool cont = false;
	char TmpPath[_MAX_PATH];
	char *FileName = NULL;
	bool uef = false;
	bool csw = false;
	bool img = false;
	
	// See if file is readable
	if (CmdLineFile[0] != 0)
	{
		FileName = CmdLineFile;
		strncpy(TmpPath, CmdLineFile, _MAX_PATH);

		// Work out which type of files it is
		char *ext = strrchr(FileName, '.');
		if (ext != NULL)
		{
			cont = true;
			if (_stricmp(ext+1, "ssd") == 0)
				ssd = true;
			else if (_stricmp(ext+1, "dsd") == 0)
				dsd = true;
			else if (_stricmp(ext+1, "adl") == 0)
				adfs = true;
			else if (_stricmp(ext+1, "adf") == 0)
				adfs = true;
			else if (_stricmp(ext+1, "uef") == 0)
				uef = true;
			else if (_stricmp(ext+1, "csw") == 0)
				csw = true;
			else if (_stricmp(ext+1, "img") == 0)
				img = true;
			else
			{
				char errstr[200];
				sprintf(errstr,"Unrecognised file type:\n  %s", FileName);
				MessageBox(m_hWnd,errstr,WindowTitle,MB_OK|MB_ICONERROR);
				cont = false;
			}
		}
	}

	if (cont)
	{
		cont = false;

		FILE *fd = fopen(FileName, "rb");
		if (fd != NULL)
		{
			cont = true;
			fclose(fd);
		}
		else if (uef)
		{
			// Try getting it from BeebState directory
			strcpy(TmpPath, m_UserDataPath);
			strcat(TmpPath, "beebstate/");
			strcat(TmpPath, FileName);
			FILE *fd = fopen(TmpPath, "rb");
			if (fd != NULL)
			{
				cont = true;
				FileName = TmpPath;
				fclose(fd);
			}
			else
			{
				// Try getting it from Tapes directory
				strcpy(TmpPath, m_UserDataPath);
				strcat(TmpPath, "tapes/");
				strcat(TmpPath, FileName);
				FILE *fd = fopen(TmpPath, "rb");
				if (fd != NULL)
				{
					cont = true;
					FileName = TmpPath;
					fclose(fd);
				}
			}
		}
		else if (csw)
		{
			// Try getting it from Tapes directory
			strcpy(TmpPath, m_UserDataPath);
			strcat(TmpPath, "tapes/");
			strcat(TmpPath, FileName);
			FILE *fd = fopen(TmpPath, "rb");
			if (fd != NULL)
			{
				cont = true;
				FileName = TmpPath;
				fclose(fd);
			}
		}
		else
		{
			// Try getting it from DiscIms directory
			strcpy(TmpPath, m_UserDataPath);
			strcat(TmpPath, "discims/");
			strcat(TmpPath, FileName);
			FILE *fd = fopen(TmpPath, "rb");
			if (fd != NULL)
			{
				cont = true;
				FileName = TmpPath;
				fclose(fd);
			}
		}

		if (!cont)
		{
			char errstr[200];
			sprintf(errstr,"Cannot find file:\n  %s", FileName);
			MessageBox(m_hWnd,errstr,WindowTitle,MB_OK|MB_ICONERROR);
			cont = false;
		}
	}

	if (cont)
	{
		PathCanonicalize(CmdLineFile, TmpPath);
	}
	else
	{
		CmdLineFile[0] = 0;
	}
}

/*****************************************************************************/
// Handle a file name passed on command line
	
void BeebWin::HandleCommandLineFile(int drive, char *CmdLineFile)
{
	bool ssd = false;
	bool dsd = false;
	bool adfs = false;
	bool cont = false;
	char *FileName = NULL;
	bool uef = false;
	bool csw = false;
	bool img = false;
	
	if (CmdLineFile[0] != 0)
	{
		FileName = CmdLineFile;

		// Work out which type of files it is
		char *ext = strrchr(FileName, '.');
		if (ext != NULL)
		{
			cont = true;
			if (_stricmp(ext+1, "ssd") == 0)
				ssd = true;
			else if (_stricmp(ext+1, "dsd") == 0)
				dsd = true;
			else if (_stricmp(ext+1, "adl") == 0)
				adfs = true;
			else if (_stricmp(ext+1, "adf") == 0)
				adfs = true;
			else if (_stricmp(ext+1, "uef") == 0)
				uef = true;
			else if (_stricmp(ext+1, "csw") == 0)
				csw = true;
			else if (_stricmp(ext+1, "img") == 0)
				img = true;
			else
			{
				char errstr[200];
				sprintf(errstr,"Unrecognised file type:\n  %s", FileName);
				MessageBox(m_hWnd,errstr,WindowTitle,MB_OK|MB_ICONERROR);
				cont = false;
			}
		}
	}

	if (cont)
	{
		cont = false;

		FILE *fd = fopen(FileName, "rb");
		if (fd != NULL)
		{
			cont = true;
			fclose(fd);
		}

		if (!cont)
		{
			char errstr[200];
			sprintf(errstr,"Cannot find file:\n  %s", FileName);
			MessageBox(m_hWnd,errstr,WindowTitle,MB_OK|MB_ICONERROR);
		}
	}

	if (cont)
	{
		if (uef)
		{
			// Determine if file is a tape or a state file
			bool stateFile = false;
			FILE *fd = fopen(FileName, "rb");
			if (fd != NULL)
			{
				char buf[14];
				fread(buf,14,1,fd);
				if (strcmp(buf,"UEF File!")==0 && buf[12]==0x6c && buf[13]==0x04)
				{
					stateFile = true;
				}
				fclose(fd);
			}

			if (stateFile)
				LoadUEFState(FileName);
			else
				LoadUEF(FileName);

			cont = false;
		}
		else if (csw)
		{
			LoadCSW(FileName);
			cont = false;
		}
	}

	if (cont)
	{
		if (MachineType!=3)
		{
			if (dsd)
			{
				if (NativeFDC)
					LoadSimpleDSDiscImage(FileName, drive, 80);
				else
					Load1770DiscImage(FileName,drive,1,m_hMenu); // 1 = dsd
			}
			if (ssd)
			{
				if (NativeFDC)
					LoadSimpleDiscImage(FileName, drive, 0, 80);
				else
					Load1770DiscImage(FileName,drive,0,m_hMenu); // 0 = ssd
			}
			if (adfs)
			{
				if (!NativeFDC)
					Load1770DiscImage(FileName,drive,2,m_hMenu); // 2 = adfs
				else
					cont = false;  // cannot load adfs with native DFS
			}
			if (img)
			{
				if (NativeFDC)
					LoadSimpleDiscImage(FileName, drive, 0, 80); // Treat like an ssd
				else
					Load1770DiscImage(FileName,drive,3,m_hMenu); // 0 = ssd
			}
		}
		else if (MachineType==3)
		{
			if (dsd)
				Load1770DiscImage(FileName,drive,1,m_hMenu); // 0 = ssd
			if (ssd)
				Load1770DiscImage(FileName,drive,0,m_hMenu); // 1 = dsd
			if (adfs)
				Load1770DiscImage(FileName,drive,2,m_hMenu); // 2 = adfs
			if (img)
				Load1770DiscImage(FileName,drive,3,m_hMenu); // 3 = img
		}
	}

	if (cont && !m_NoAutoBoot && drive == 0)
	{
		// Do a shift + break
		ResetBeebSystem(MachineType,TubeEnabled,0);
		BeebKeyDown(0, 0); // Shift key
		m_ShiftBooted = true;
	}
}

/****************************************************************************/
BOOL BeebWin::RebootSystem(void)
{
	HANDLE hToken; 
	TOKEN_PRIVILEGES tkp; 
	
	if (!OpenProcessToken(GetCurrentProcess(), 
						  TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken)) 
		return( FALSE ); 
	
	LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME, 
						 &tkp.Privileges[0].Luid); 
	
	tkp.PrivilegeCount = 1;  // one privilege to set    
	tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED; 
	AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, 
						  (PTOKEN_PRIVILEGES)NULL, 0); 
	if (GetLastError() != ERROR_SUCCESS) 
		return FALSE; 
	
	if (!ExitWindowsEx(EWX_REBOOT | EWX_FORCE, 
					   SHTDN_REASON_MAJOR_OPERATINGSYSTEM |
					   SHTDN_REASON_MINOR_RECONFIG |
					   SHTDN_REASON_FLAG_PLANNED)) 
		return FALSE; 
	
	return TRUE;
}

/****************************************************************************/
bool BeebWin::CheckUserDataPath()
{
	bool success = true;
	bool copy_user_files = false;
	bool store_user_data_path = false;
	char path[_MAX_PATH];
	char errstr[500];
	DWORD att;

	// Change all '/' to '\'
	for (unsigned int i = 0; i < strlen(m_UserDataPath); ++i)
		if (m_UserDataPath[i] == '/')
			m_UserDataPath[i] = '\\';

	// Check that the folder exists
	att = GetFileAttributes(m_UserDataPath);
	if (att == INVALID_FILE_ATTRIBUTES || !(att & FILE_ATTRIBUTE_DIRECTORY))
	{
		sprintf(errstr, "BeebEm data folder does not exist:\n  %s\n\nCreate the folder?",
				m_UserDataPath);
		if (MessageBox(m_hWnd, errstr, WindowTitle, MB_YESNO|MB_ICONQUESTION) != IDYES)
		{
			// Use data dir installed with BeebEm
			strcpy(m_UserDataPath, m_AppPath);
			strcat(m_UserDataPath, "UserData\\");

			store_user_data_path = true;
		}
		else
		{
			// Create the folder
			strcpy(path, m_UserDataPath);

			char *s = strchr(path, '\\');
			while (s != NULL)
			{
				*s = 0;
				CreateDirectory(path, NULL);
				*s = '\\';
				s = strchr(s+1, '\\');
			}

			att = GetFileAttributes(m_UserDataPath);
			if (att == INVALID_FILE_ATTRIBUTES ||
				!(att & FILE_ATTRIBUTE_DIRECTORY))
			{
				sprintf(errstr, "Failed to create BeebEm data folder:\n  %s", m_UserDataPath);
				MessageBox(m_hWnd, errstr, WindowTitle, MB_OK|MB_ICONERROR);
				success = false;
			}
			else
			{
				copy_user_files = true;
			}
		}
	}
	else
	{
		// Check that essential files are in the user data folder
		sprintf(path, "%sBeebFile", m_UserDataPath);
		att = GetFileAttributes(path);
		if (att == INVALID_FILE_ATTRIBUTES || !(att & FILE_ATTRIBUTE_DIRECTORY))
			copy_user_files = true;
		if (!copy_user_files)
		{
			sprintf(path, "%sBeebState", m_UserDataPath);
			att = GetFileAttributes(path);
			if (att == INVALID_FILE_ATTRIBUTES || !(att & FILE_ATTRIBUTE_DIRECTORY))
				copy_user_files = true;
		}
		if (!copy_user_files)
		{
			sprintf(path, "%sEconet.cfg", m_UserDataPath);
			att = GetFileAttributes(path);
			if (att == INVALID_FILE_ATTRIBUTES)
				copy_user_files = true;
		}
		if (!copy_user_files)
		{
			sprintf(path, "%sAUNMap", m_UserDataPath);
			att = GetFileAttributes(path);
			if (att == INVALID_FILE_ATTRIBUTES)
				copy_user_files = true;
		}
#ifdef SPEECH_ENABLED
		if (!copy_user_files)
		{
			sprintf(path, "%sPhroms.cfg", m_UserDataPath);
			att = GetFileAttributes(path);
			if (att == INVALID_FILE_ATTRIBUTES)
				copy_user_files = true;
		}
#endif
		if (!copy_user_files)
		{
			if (strcmp(RomFile, "Roms.cfg") == 0)
			{
				sprintf(path, "%sRoms.cfg", m_UserDataPath);
				att = GetFileAttributes(path);
				if (att == INVALID_FILE_ATTRIBUTES)
					copy_user_files = true;
			}
		}

		if (copy_user_files)
		{
			sprintf(errstr, "Essential or new files missing from BeebEm data folder:\n  %s"
					"\n\nCopy essential or new files into folder?", m_UserDataPath);
			if (MessageBox(m_hWnd, errstr, WindowTitle, MB_YESNO|MB_ICONQUESTION) != IDYES)
			{
				success = false;
			}
		}
	}

	if (success)
	{
		// Get fully qualified user data path
		char *f;
		if (GetFullPathName(m_UserDataPath, _MAX_PATH, path, &f) != 0)
			strcpy(m_UserDataPath, path);
	}

	if (success && copy_user_files)
	{
		SHFILEOPSTRUCT fileOp = {0};
		fileOp.hwnd = m_hWnd;
		fileOp.wFunc = FO_COPY;
		fileOp.fFlags = 0;

		strcpy(path, m_AppPath);
		strcat(path, "UserData\\*.*");
		path[strlen(path)+1] = 0; // need double 0
		fileOp.pFrom = path;

		m_UserDataPath[strlen(m_UserDataPath)+1] = 0; // need double 0
		fileOp.pTo = m_UserDataPath;

		if (SHFileOperation(&fileOp) || fileOp.fAnyOperationsAborted)
		{
			sprintf(errstr, "Copy failed.  Manually copy files from:\n  %s"
					"\n\nTo BeebEm data folder:\n  %s", path, m_UserDataPath);
			MessageBox(m_hWnd, errstr, WindowTitle, MB_OK|MB_ICONERROR);
			success = false;
		}
		else
		{
			// Wait for copy dialogs to disappear
			Sleep(2000);
		}
	}

	if (success)
	{
		// Check that roms file exists and create its full path
		if (PathIsRelative(RomFile))
		{
			sprintf(path, "%s%s", m_UserDataPath, RomFile);
			strcpy(RomFile, path);
		}
		att = GetFileAttributes(RomFile);
		if (att == INVALID_FILE_ATTRIBUTES)
		{
			sprintf(errstr, "Cannot open ROMs file:\n  %s", RomFile);
			MessageBox(m_hWnd, errstr, WindowTitle, MB_OK|MB_ICONERROR);
			success = false;
		}
	}

	if (success)
	{
		// Fill out full path of prefs file
		if (PathIsRelative(m_PrefsFile))
		{
			sprintf(path, "%s%s", m_UserDataPath, m_PrefsFile);
			strcpy(m_PrefsFile, path);
		}
	}

	if (success && (copy_user_files || store_user_data_path))
	{
		StoreUserDataPath();
	}

	return success;
}

void BeebWin::StoreUserDataPath()
{
	// Store user data path in registry
	RegCreateKey(HKEY_CURRENT_USER, CFG_REG_KEY);
	RegSetStringValue(HKEY_CURRENT_USER, CFG_REG_KEY,
					  "UserDataFolder", m_UserDataPath);
}

/****************************************************************************/
/* Selection of User Data Folder */
static char szExportUserDataPath[MAX_PATH];

int CALLBACK BrowseUserDataCallbackProc(HWND hwnd, UINT uMsg, LPARAM lParam, LPARAM lpData)
{
	switch (uMsg)
	{
	case BFFM_INITIALIZED:
		if (szExportUserDataPath[0])
		{
			SendMessage(hwnd, BFFM_SETEXPANDED, TRUE, (LPARAM)szExportUserDataPath);
			SendMessage(hwnd, BFFM_SETSELECTION, TRUE, (LPARAM)szExportUserDataPath);
		}
		break;
	}
	return 0;
}

void BeebWin::SelectUserDataPath()
{
	char szDisplayName[MAX_PATH];
	char szPathBackup[MAX_PATH];
	BROWSEINFO bi;

	memset(&bi, 0, sizeof(bi));
	bi.hwndOwner = m_hWnd;
	bi.pszDisplayName = szDisplayName;
	bi.lpszTitle = "Select folder for user data files:";
	bi.ulFlags = BIF_EDITBOX | BIF_NEWDIALOGSTYLE;
	bi.lpfn = BrowseUserDataCallbackProc;
	LPITEMIDLIST idList = SHBrowseForFolder(&bi);
	if (idList != NULL)
	{
		if (SHGetPathFromIDList(idList, szExportUserDataPath) == FALSE)
		{
			MessageBox(m_hWnd, "Invalid folder selected", WindowTitle, MB_OK|MB_ICONWARNING);
		}
		else
		{
			strcpy(szPathBackup, m_UserDataPath);
			strcpy(m_UserDataPath, szExportUserDataPath);
			strcat(m_UserDataPath, "\\");

			// Check folder contents
			if (CheckUserDataPath() == false)
			{
				strcpy(m_UserDataPath, szPathBackup);
			}
			else
			{
				// Store the new path
				StoreUserDataPath();

				// Reset prefs and roms file paths
				strcpy(m_PrefsFile, "Preferences.cfg");
				strcpy(RomFile, "Roms.cfg");
				CheckForLocalPrefs(m_UserDataPath, true);

				// Load and apply prefs
				LoadPreferences();
				ApplyPrefs();
			}
		}
		CoTaskMemFree(idList);
	}
}

/****************************************************************************/
void BeebWin::HandleTimer()
{
	int row,col;
	char delay[10];

	// Do nothing if emulation is not running (e.g. if Window is being moved)
	if ((TotalCycles - m_KbdCmdLastCycles) / 2000 < m_KbdCmdDelay)
	{
		SetTimer(m_hWnd, 1, m_KbdCmdDelay, NULL);
		return;
	}
	m_KbdCmdLastCycles = TotalCycles;
	
	// Release previous key press (except shift/control)
	if (m_KbdCmdPress &&
		m_KbdCmdKey != VK_SHIFT && m_KbdCmdKey != VK_CONTROL)
	{
		TranslateKey(m_KbdCmdKey, 1, row, col);
		m_KbdCmdPress = false;
		SetTimer(m_hWnd, 1, m_KbdCmdDelay, NULL);
	}
	else
	{
		m_KbdCmdPos++;
		if (m_KbdCmd[m_KbdCmdPos] == 0)
		{
			KillTimer(m_hWnd, 1);
		}
		else
		{
			m_KbdCmdPress = true;
			
			switch (m_KbdCmd[m_KbdCmdPos])
			{
			case '\\':
				m_KbdCmdPos++;
				switch (m_KbdCmd[m_KbdCmdPos])
				{
				case '\\': m_KbdCmdKey = VK_OEM_5; break;
				case 'n': m_KbdCmdKey = VK_RETURN; break;
				case 's': m_KbdCmdKey = VK_SHIFT; break;
				case 'S': m_KbdCmdKey = VK_SHIFT; m_KbdCmdPress = false; break;
				case 'c': m_KbdCmdKey = VK_CONTROL; break;
				case 'C': m_KbdCmdKey = VK_CONTROL; m_KbdCmdPress = false; break;
				case 'd':
					m_KbdCmdKey = 0;
					m_KbdCmdPos++;
					delay[0] = m_KbdCmd[m_KbdCmdPos];
					m_KbdCmdPos++;
					delay[1] = m_KbdCmd[m_KbdCmdPos];
					m_KbdCmdPos++;
					delay[2] = m_KbdCmd[m_KbdCmdPos];
					m_KbdCmdPos++;
					delay[3] = m_KbdCmd[m_KbdCmdPos];
					delay[4] = 0;
					m_KbdCmdDelay = atoi(delay);
					break;
				default: m_KbdCmdKey = 0; break;
				}
				break;
				
			case '`': m_KbdCmdKey = VK_OEM_8; break;
			case '-': m_KbdCmdKey = VK_OEM_MINUS; break;
			case '=': m_KbdCmdKey = VK_OEM_PLUS; break;
			case '[': m_KbdCmdKey = VK_OEM_4; break;
			case ']': m_KbdCmdKey = VK_OEM_6; break;
			case ';': m_KbdCmdKey = VK_OEM_1; break;
			case '\'': m_KbdCmdKey = VK_OEM_3; break;
			case '#': m_KbdCmdKey = VK_OEM_7; break;
			case ',': m_KbdCmdKey = VK_OEM_COMMA; break;
			case '.': m_KbdCmdKey = VK_OEM_PERIOD; break;
			case '/': m_KbdCmdKey = VK_OEM_2; break;
			default: m_KbdCmdKey = m_KbdCmd[m_KbdCmdPos]; break;
			}
			
			if (m_KbdCmdKey != 0)
			{
				if (m_KbdCmdPress)
					TranslateKey(m_KbdCmdKey, 0, row, col);
				else
					TranslateKey(m_KbdCmdKey, 1, row, col);
			}

			SetTimer(m_hWnd, 1, m_KbdCmdDelay, NULL);
		}
	}
}

/****************************************************************************/
bool BeebWin::RegCreateKey(HKEY hKeyRoot, LPSTR lpSubKey)
{
	bool rc = false;
	HKEY hKeyResult;
	if ((RegCreateKeyEx(hKeyRoot, lpSubKey, 0, NULL, 0, KEY_ALL_ACCESS,
						NULL, &hKeyResult, NULL)) == ERROR_SUCCESS)
	{
		RegCloseKey(hKeyResult);
		rc = true;
	}
	return rc;
}

bool BeebWin::RegGetBinaryValue(
	HKEY hKeyRoot, LPSTR lpSubKey, LPSTR lpValue, PVOID pData, int* pnSize)
{
	bool rc = false;
	HKEY hKeyResult;
	DWORD dwType = REG_BINARY;
	DWORD dwSize = *pnSize;
	LONG lRes = 0;

	if ((RegOpenKeyEx(hKeyRoot, lpSubKey, 0, KEY_ALL_ACCESS, &hKeyResult)) == ERROR_SUCCESS)
	{
		lRes = RegQueryValueEx(hKeyResult, lpValue, NULL, &dwType, (LPBYTE)pData, &dwSize);
		if (lRes == ERROR_SUCCESS && dwType == REG_BINARY)
		{
			*pnSize = dwSize;
			rc = true;
		}
		RegCloseKey(hKeyResult);
	}

	return rc;
}

bool BeebWin::RegSetBinaryValue(
	HKEY hKeyRoot, LPSTR lpSubKey, LPSTR lpValue, PVOID pData, int* pnSize)
{
	bool rc = false;
	HKEY hKeyResult;
    DWORD dwSize = *pnSize;
	LONG  lRes = 0;

	if ((RegOpenKeyEx(hKeyRoot, lpSubKey, 0, KEY_ALL_ACCESS, &hKeyResult)) == ERROR_SUCCESS)
	{
		lRes = RegSetValueEx(hKeyResult, lpValue, 0, REG_BINARY, reinterpret_cast<BYTE*>(pData), dwSize);
		if (lRes == ERROR_SUCCESS)
		{
			*pnSize = dwSize;
			rc = true;
		}
		RegCloseKey(hKeyResult);
	}

	return rc;
}

bool BeebWin::RegGetStringValue(
	HKEY hKeyRoot, LPSTR lpSubKey, LPSTR lpValue, LPSTR pData, DWORD dwSize)
{
	bool rc = false;
	HKEY hKeyResult;
	DWORD dwType = REG_SZ;
	LONG lRes = 0;

	if ((RegOpenKeyEx(hKeyRoot, lpSubKey, 0, KEY_ALL_ACCESS, &hKeyResult)) == ERROR_SUCCESS)
	{
		lRes = RegQueryValueEx(hKeyResult, lpValue, NULL, &dwType, (LPBYTE)pData, &dwSize);
		if (lRes == ERROR_SUCCESS && dwType == REG_SZ)
		{
			rc = true;
		}
		RegCloseKey(hKeyResult);
	}

	return rc;
}

bool BeebWin::RegSetStringValue(
	HKEY hKeyRoot, LPSTR lpSubKey, LPSTR lpValue, LPSTR pData)
{
	bool rc = false;
	HKEY hKeyResult;
    DWORD dwSize = (DWORD)strlen(pData);
	LONG  lRes = 0;

	if ((RegOpenKeyEx(hKeyRoot, lpSubKey, 0, KEY_ALL_ACCESS, &hKeyResult)) == ERROR_SUCCESS)
	{
		lRes = RegSetValueEx(hKeyResult, lpValue, 0, REG_SZ, reinterpret_cast<BYTE*>(pData), dwSize);
		if (lRes == ERROR_SUCCESS)
		{
			rc = true;
		}
		RegCloseKey(hKeyResult);
	}

	return rc;
}
