/****************************************************************************/
/*                               Beebem                                     */
/*                               ------                                     */
/* This program may be distributed freely within the following restrictions:*/
/*                                                                          */
/* 1) You may not charge for this program or for any part of it.            */
/* 2) This copyright message must be distributed with all copies.           */
/* 3) This program must be distributed complete with source code.  Binary   */
/*    only distribution is not permitted.                                   */
/* 4) The author offers no warrenties, or guarentees etc. - you use it at   */
/*    your own risk.  If it messes something up or destroys your computer   */
/*    thats YOUR problem.                                                   */
/* 5) You may use small sections of code from this program in your own      */
/*    applications - but you must acknowledge its use.  If you plan to use  */
/*    large sections then please ask the author.                            */
/*                                                                          */
/* If you do not agree with any of the above then please do not use this    */
/* program.                                                                 */
/****************************************************************************/
/* Mike Wyatt and NRM's port to win32 - 7/6/97 */
/* Conveted to use DirectX - Mike Wyatt 11/1/98 */
// Econet added Rob O'Donnell. robert@irrelevant.com. 28/12/2004.

#include <stdio.h>
#include <windows.h>
#include <initguid.h>
#include <shlobj.h>
#include "main.h"
#include "beebwin.h"
#include "port.h"
#include "6502core.h"
#include "cRegistry.h"
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
#include "z80mem.h"
#include "z80.h"
#include "userkybd.h"
#include "speech.h"
#include "teletext.h"
#include "avi.h"
#include "csw.h"
#include "serialdevices.h"
#include "Arm.h"

// some LED based macros
#define LED_COL_BASE 64

// Registry access
extern cRegistry SysReg;

void i86_main(void);

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
	"BBC Micro Model B Plus (128)\nAcorn Master 128\nAcorn 65C02 Second Processor\n"
	"Torch Z80 Second Processor\nMaster 512 Second Processor\nAcorn Z80 Second Processor\n"
	"ARM Second Processor\n\n"
	"Version 3.82, March 2008";

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
	m_UserKeyMapPath[0] = 0;
	m_hBitmap = m_hOldObj = m_hDCBitmap = NULL;
	m_screen = m_screen_blur = NULL;
	m_ScreenRefreshCount = 0;
	m_RelativeSpeed = 1;
	m_FramesPerSecond = 50;
	strcpy(m_szTitle, WindowTitle);
	m_AviDC = NULL;
	m_AviDIB = NULL;
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
	m_KbdCmd[0] = 0;
	m_KbdCmdPos = -1;
	m_KbdCmdPress = false;

	/* Get the applications path - used for non-user files */
	char app_path[_MAX_PATH];
	char app_drive[_MAX_DRIVE];
	char app_dir[_MAX_DIR];
	GetModuleFileName(NULL, app_path, _MAX_PATH);
	_splitpath(app_path, app_drive, app_dir, NULL, NULL);
	_makepath(m_AppPath, app_drive, app_dir, NULL, NULL);

	// Default user data path to a sub-directory in My Docs
	if (SHGetFolderPath(NULL, CSIDL_PERSONAL, NULL, SHGFP_TYPE_CURRENT, m_UserDataPath) == NOERROR)
	{
		strcat(m_UserDataPath, "\\BeebEm\\");
	}
	else
	{
		// Default to user data dir installed with BeebEm
		strcpy(m_UserDataPath, m_AppPath);
		strcat(m_UserDataPath, "UserData\\");
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

	// Check that user data directory exists
	if (CheckUserDataPath() == false)
		exit(-1);

	// Set up paths
	strcpy(EconetCfgPath, m_UserDataPath);
	strcpy(RomPath, m_UserDataPath);

	// Load key maps
	char keymap[_MAX_PATH];
	strcpy(keymap, "Logical.kmap");
	GetDataPath(m_UserDataPath, keymap);
	ReadKeyMap(keymap, &logicalMapping);
	strcpy(keymap, "Default.kmap");
	GetDataPath(m_UserDataPath, keymap);
	ReadKeyMap(keymap, &defaultMapping);

	LoadPreferences();
	TouchScreenOpen();

	if (FAILED(CoInitialize(NULL)))
		MessageBox(m_hWnd,"Failed to initialise COM\n",WindowTitle,MB_OK|MB_ICONERROR);

	InitClass();
	CreateBeebWindow(); 
	CreateBitmap();

	m_hMenu = GetMenu(m_hWnd);
	InitMenu();
	m_hDC = GetDC(m_hWnd);

	// Will hide menu if necessary
	ShowMenu(TRUE);

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

	UpdateModelType();
	UpdateSFXMenu();
	UpdateLEDMenu(m_hMenu);
	MenuOn=TRUE;
	LoadFDC(NULL, true);
	SetTapeSpeedMenu();
	UpdateOptiMenu();

	SoundReset();
	if (SoundDefault) SoundInit();
	SetSoundMenu();
	if (SpeechDefault)
		tms5220_start();

	ResetBeebSystem(MachineType,TubeEnabled,1); 

	// Boot file if passed on command line
	HandleCommandLineFile();

	// Schedule first key press if keyboard command supplied
	if (m_KbdCmd[0] != 0)
		SetTimer(m_hWnd, 1, 1000, NULL);
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
}

/****************************************************************************/
void BeebWin::ResetBeebSystem(unsigned char NewModelType,unsigned char TubeStatus,unsigned char LoadRoms) 
{
	SwitchOnCycles=0; // Reset delay
	SoundChipReset();
	SwitchOnSound();
	EnableTube=TubeStatus;
	MachineType=NewModelType;
	BeebMemInit(LoadRoms,m_ShiftBooted);
	Init6502core();
	if (EnableTube) Init65C02core();
	if (Tube186Enabled) i86_main();
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
	Disc8271_reset();
	if (EconetEnabled) EconetReset();	//Rob:
	Reset1770();
	AtoDReset();
	SetRomMenu();
	FreeDiscImage(0);
	// Keep the disc images loaded
	FreeDiscImage(1);
	Close1770Disc(0);
	Close1770Disc(1);
	if (HardDriveEnabled) SCSIReset();
	if (HardDriveEnabled) SASIReset();
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
	{
		RECT scrrect;
		SystemParametersInfo(SPI_GETWORKAREA, 0, (PVOID)&scrrect, 0);
		x = scrrect.left;
		y = scrrect.top;
	}

	if (m_DisplayRenderer != IDM_DISPGDI && m_isFullScreen)
	{
		style = WS_POPUP;
	}
	else
	{
		style = WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX;
	}

	m_hWnd = CreateWindow(
				"BEEBWIN",				// See RegisterClass() call.
				m_szTitle, 		// Text for window title bar.
				style,
				x, y,
				m_XWinSize + GetSystemMetrics(SM_CXFIXEDFRAME) * 2,
				m_YWinSize + GetSystemMetrics(SM_CYFIXEDFRAME) * 2
					+ GetSystemMetrics(SM_CYMENUSIZE)
					+ GetSystemMetrics(SM_CYCAPTION)
					+ 1,
				NULL,					// Overlapped windows have no parent.
				NULL,				 // Use the window class menu.
				hInst,			 // This instance owns this window.
				NULL				 // We don't use any data in our WM_CREATE
		); 

	ShowWindow(m_hWnd, SW_SHOW); // Show the window
	UpdateWindow(m_hWnd);		  // Sends WM_PAINT message
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

	CheckMenuItem(hMenu, m_MenuIdAviResolution, MF_CHECKED);
	CheckMenuItem(hMenu, m_MenuIdAviSkip, MF_CHECKED);
	CheckMenuItem(hMenu, IDM_SPEEDANDFPS, m_ShowSpeedAndFPS ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hMenu, m_MenuIdWinSize, MF_CHECKED);
	CheckMenuItem(hMenu, IDM_FULLSCREEN, m_isFullScreen ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hMenu, m_MenuIdSampleRate, MF_CHECKED);
	CheckMenuItem(hMenu, m_MenuIdVolume, MF_CHECKED);
	CheckMenuItem(hMenu, m_MenuIdTiming, MF_CHECKED);
	if (m_MenuIdSticks != 0)
		CheckMenuItem(hMenu, m_MenuIdSticks, MF_CHECKED);
	CheckMenuItem(hMenu, IDM_FREEZEINACTIVE, m_FreezeWhenInactive ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_HIDECURSOR, m_HideCursor ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_IGNOREILLEGALOPS,
					IgnoreIllegalInstructions ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hMenu, m_MenuIdKeyMapping, MF_CHECKED);
	CheckMenuItem(hMenu, IDM_MAPAS, m_KeyMapAS ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_MAPFUNCS, m_KeyMapFunc ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_WPDISC0, m_WriteProtectDisc[0] ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_WPDISC1, m_WriteProtectDisc[1] ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_WPONLOAD, m_WriteProtectOnLoad ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_AMXONOFF, AMXMouseEnabled ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_AMX_LRFORMIDDLE, AMXLRForMiddle ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hMenu, m_MenuIdAMXSize, MF_CHECKED);
	if (m_MenuIdAMXAdjust != 0)
		CheckMenuItem(hMenu, m_MenuIdAMXAdjust, MF_CHECKED);
	CheckMenuItem(hMenu, IDM_PRINTERONOFF, PrinterEnabled ? MF_CHECKED : MF_UNCHECKED);
	strcpy(menu_string, "File: ");
	strcat(menu_string, m_PrinterFileName);
	ModifyMenu(hMenu, IDM_PRINTER_FILE, MF_BYCOMMAND, IDM_PRINTER_FILE, menu_string);
	CheckMenuItem(hMenu, m_MenuIdPrinterPort, MF_CHECKED);

	UpdateDisplayRendererMenu();
	CheckMenuItem(hMenu, IDM_DXSMOOTHING, m_DXSmoothing ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hMenu, m_DDFullScreenMode, MF_CHECKED);
	CheckMenuItem(hMenu, IDM_TEXTVIEW, m_TextViewEnabled ? MF_CHECKED:MF_UNCHECKED);

	CheckMenuItem(hMenu, ID_SERIAL, SerialPortEnabled ? MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_TOUCHSCREEN, TouchScreenEnabled ? MF_CHECKED:MF_UNCHECKED);

	CheckMenuItem(hMenu, ID_COM1, (SerialPort==1)? MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_COM2, (SerialPort==2)? MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_COM3, (SerialPort==3)? MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_COM4, (SerialPort==4)? MF_CHECKED:MF_UNCHECKED);

	CheckMenuItem(hMenu, ID_HIDEMENU, HideMenuEnabled ? MF_CHECKED:MF_UNCHECKED);

	CheckMenuItem(m_hMenu,ID_TAPESOUND,(TapeSoundEnabled)?MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(m_hMenu,IDM_SOUNDCHIP,(SoundChipEnabled)?MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(m_hMenu,ID_PSAMPLES,(PartSamples)?MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(m_hMenu, IDM_EXPVOLUME, SoundExponentialVolume ? MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_TEXTTOSPEECH, m_TextToSpeechEnabled ? MF_CHECKED:MF_UNCHECKED);

	CheckMenuItem(m_hMenu, IDM_TUBE, (TubeEnabled)?MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(m_hMenu, IDM_TUBE186, (Tube186Enabled)?MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(m_hMenu, IDM_TORCH, (TorchTube)?MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(m_hMenu, IDM_ACORNZ80, (AcornZ80)?MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(m_hMenu, IDM_ARM, (ArmTube)?MF_CHECKED:MF_UNCHECKED);

	CheckMenuItem(m_hMenu,ID_UNLOCKTAPE,(UnlockTape)?MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(m_hMenu,m_MotionBlur,MF_CHECKED);
	CheckMenuItem(hMenu, ID_ECONET, EconetEnabled ? MF_CHECKED:MF_UNCHECKED);	//Rob
	CheckMenuItem(hMenu, ID_TELETEXT, TeleTextAdapterEnabled ? MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_HARDDRIVE, HardDriveEnabled ? MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_UPRM, RTC_Enabled ? MF_CHECKED:MF_UNCHECKED);

	CheckMenuItem(hMenu, IDM_SPEECH, SpeechDefault ? MF_CHECKED:MF_UNCHECKED);

	CheckMenuItem(hMenu, IDM_AUTOSAVE_PREFS_CMOS, m_AutoSavePrefsCMOS ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_AUTOSAVE_PREFS_FOLDERS, m_AutoSavePrefsFolders ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(hMenu, IDM_AUTOSAVE_PREFS_ALL, m_AutoSavePrefsAll ? MF_CHECKED : MF_UNCHECKED);

	UpdateMonitorMenu();
	UpdateDisableKeysMenu();

	/* Initialise the ROM Menu. */
	SetRomMenu();

	SetSoundMenu();
}

void BeebWin::UpdateDisplayRendererMenu() {
	CheckMenuItem(m_hMenu, IDM_DISPGDI,
				  m_DisplayRenderer == IDM_DISPGDI ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(m_hMenu, IDM_DISPDDRAW,
				  m_DisplayRenderer == IDM_DISPDDRAW ? MF_CHECKED : MF_UNCHECKED);
	CheckMenuItem(m_hMenu, IDM_DISPDX9,
				  m_DisplayRenderer == IDM_DISPDX9 ? MF_CHECKED : MF_UNCHECKED);
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
			strcpy( &Title[3], "Empty" );

		ModifyMenu( hMenu,	// handle of menu 
					IDM_ALLOWWRITES_ROM0 + i,
					MF_BYCOMMAND,	// menu item to modify
				//	MF_STRING,	// menu item flags 
					IDM_ALLOWWRITES_ROM0 + i,	// menu item identifier or pop-up menu handle
					Title		// menu item content 
					);

		/* LRW Now uncheck the Roms which are NOT writable, that have already been loaded. */
		CheckMenuItem(hMenu, IDM_ALLOWWRITES_ROM0 + i, RomWritable[i] ? MF_CHECKED : MF_UNCHECKED );
	}
}

/****************************************************************************/
void BeebWin::GetRomMenu(void)
{
	HMENU hMenu = m_hMenu;

	for (int i=0; i<16; ++i)
	  /* LRW Now uncheck the Roms as NOT writable, that have already been loaded. */
	  RomWritable[i] = ( GetMenuState(hMenu, IDM_ALLOWWRITES_ROM0 + i, MF_BYCOMMAND ) & MF_CHECKED );
}

/****************************************************************************/
void BeebWin::InitJoystick(void)
{
	MMRESULT mmresult;

	/* Get joystick updates 10 times a second */
	mmresult = joySetCapture(m_hWnd, JOYSTICKID1, 100, FALSE);
	if (mmresult == JOYERR_NOERROR)
		mmresult = joyGetDevCaps(JOYSTICKID1, &m_JoystickCaps, sizeof(JOYCAPS));

	if (mmresult == JOYERR_NOERROR)
	{
		AtoDInit();
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
	/* Scale and reverse the readings */
	JoystickX = (int)((double)(m_JoystickCaps.wXmax - x) * 65535.0 /
						(double)(m_JoystickCaps.wXmax - m_JoystickCaps.wXmin));
	JoystickY = (int)((double)(m_JoystickCaps.wYmax - y) * 65535.0 /
						(double)(m_JoystickCaps.wYmax - m_JoystickCaps.wYmin));
}

/****************************************************************************/
void BeebWin::ResetJoystick(void)
{
	joyReleaseCapture(JOYSTICKID1);
	AtoDReset();
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
	} else if (m_MenuIdSticks == IDM_DMOUSESTICK)		// Digital mouse stick
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
			}
			break;

		case WM_SYSKEYDOWN:
			//{char txt[100];sprintf(txt,"SysKeyD: %d, 0x%X\n", uParam, uParam);OutputDebugString(txt);}
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

			if (uParam != VK_F10 && uParam != VK_CONTROL)
				break;
		case WM_KEYDOWN:
			//{char txt[100];sprintf(txt,"KeyD: %d, 0x%X\n", uParam, uParam);OutputDebugString(txt);}
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
			//{char txt[100];sprintf(txt,"SysKeyU: %d, 0x%X\n", uParam, uParam);OutputDebugString(txt);}
			if (uParam != VK_F10 && uParam != VK_CONTROL)
				break;
		case WM_KEYUP:
			//{char txt[100];sprintf(txt,"KeyU: %d, 0x%X\n", uParam, uParam);OutputDebugString(txt);}
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
							if (Tube186Enabled) i86_main();
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
							if (HardDriveEnabled) SCSIReset();
							if (HardDriveEnabled) SASIReset();
							TeleTextInit();
							//SoundChipReset();
						}
						else if(row==-3)
						{
							if (col==-3) SoundTuning+=0.1; // Page Up
							if (col==-4) SoundTuning-=0.1; // Page Down
						}
						else if(row==-4)
						{
							mainWin->AdjustSpeed(col == 0);
						}
					}
				}
			}
			break;					  

		case WM_ACTIVATE:
			if (mainWin)
				mainWin->Activate(uParam != WA_INACTIVE);
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

		case WM_REINITDX:
			mainWin->ReinitDX();
			break;

		case WM_TIMER:
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
void BeebWin::TranslateWindowSize(void)
{
	if (m_isFullScreen)
	{
		if (m_DisplayRenderer != IDM_DISPGDI)
		{
			switch (m_DDFullScreenMode)
			{
			case ID_VIEW_DD_640X480:
			  m_XWinSize = 640;
			  m_YWinSize = 480;
			  break;
			case ID_VIEW_DD_1024X768:
			  m_XWinSize = 1024;
			  m_YWinSize = 768;
			  break;
			case ID_VIEW_DD_1280X768:
			  m_XWinSize = 1280;
			  m_YWinSize = 768;
			  break;
			case ID_VIEW_DD_1280X960:
			  m_XWinSize = 1280;
			  m_YWinSize = 960;
			  break;
			case ID_VIEW_DD_1280X1024:
			  m_XWinSize = 1280;
			  m_YWinSize = 1024;
			  break;
			case ID_VIEW_DD_1440X900:
			  m_XWinSize = 1440;
			  m_YWinSize = 900;
			  break;
			case ID_VIEW_DD_1600X1200:
			  m_XWinSize = 1600;
			  m_YWinSize = 1200;
			  break;
			}
		}
		else
		{
			RECT scrrect;
			SystemParametersInfo(SPI_GETWORKAREA, 0, (PVOID)&scrrect, 0);
			m_XWinSize = scrrect.right - scrrect.left - GetSystemMetrics(SM_CXFIXEDFRAME) * 2;
			m_YWinSize = scrrect.bottom - scrrect.top - GetSystemMetrics(SM_CYFIXEDFRAME) * 2
					- GetSystemMetrics(SM_CYMENUSIZE) - GetSystemMetrics(SM_CYCAPTION);
		}
	}
	else
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
	  }
	}
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
void BeebWin::SetWindowAttributes(bool wasFullScreen)
{
	RECT wndrect;
	RECT scrrect;
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
			style = GetWindowLong(m_hWnd, GWL_STYLE);
			style &= ~(WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX);
			style |= WS_POPUP;
			SetWindowLong(m_hWnd, GWL_STYLE, style);

			if (m_DXInit == TRUE)
			{
				ResetDX();
			}
		}
		else
		{
			style = GetWindowLong(m_hWnd, GWL_STYLE);
			style &= ~WS_POPUP;
			style |= WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX;
			SetWindowLong(m_hWnd, GWL_STYLE, style);

			SystemParametersInfo(SPI_GETWORKAREA, 0, (PVOID)&scrrect, 0);
			SetWindowPos(m_hWnd, HWND_TOP, scrrect.left, scrrect.top,
				m_XWinSize + GetSystemMetrics(SM_CXFIXEDFRAME) * 2,
				m_YWinSize + GetSystemMetrics(SM_CYFIXEDFRAME) * 2
					+ GetSystemMetrics(SM_CYMENUSIZE)
					+ GetSystemMetrics(SM_CYCAPTION)
					+ 1,
				0);
		}

		// Experiment: hide menu in full screen
		if (HideMenuEnabled)
			ShowMenu(false);
	}
	else
	{
		if (m_DisplayRenderer != IDM_DISPGDI && m_DXInit == TRUE)
		{
			ResetDX();
		}

		style = GetWindowLong(m_hWnd, GWL_STYLE);
		style &= ~WS_POPUP;
		style |= WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX;
		SetWindowLong(m_hWnd, GWL_STYLE, style);

		SetWindowPos(m_hWnd, HWND_TOP, m_XWinPos, m_YWinPos,
			m_XWinSize + GetSystemMetrics(SM_CXFIXEDFRAME) * 2,
			m_YWinSize + GetSystemMetrics(SM_CYFIXEDFRAME) * 2
				+ GetSystemMetrics(SM_CYMENUSIZE)
				+ GetSystemMetrics(SM_CYCAPTION)
				+ 1,
			!wasFullScreen ? SWP_NOMOVE : 0);

		// Experiment: hide menu in full screen
		if (HideMenuEnabled)
			ShowMenu(true);
	}
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
	CheckMenuItem(hMenu, ID_COM1, (SerialPort==1)?MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_COM2, (SerialPort==2)?MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_COM3, (SerialPort==3)?MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(hMenu, ID_COM4, (SerialPort==4)?MF_CHECKED:MF_UNCHECKED);
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
	CheckMenuItem(m_hMenu,ID_1773,(SBSize==1)?MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(m_hMenu,ID_443,(SBSize==0)?MF_CHECKED:MF_UNCHECKED);
	CheckMenuItem(m_hMenu,ID_PSAMPLES,(PartSamples)?MF_CHECKED:MF_UNCHECKED);
}
/***************************************************************************/
void BeebWin::HandleCommand(int MenuId)
{
	char TmpPath[256];
	HMENU hMenu = m_hMenu;
	int prev_palette_type = palette_type;

	SetSound(MUTED);

	switch (MenuId)
	{
	case IDM_RUNDISC:
		if (ReadDisc(0,hMenu))
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
		ReadDisc(0,hMenu);
		break;
	case IDM_LOADDISC1:
		ReadDisc(1,hMenu);
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
		if (TouchScreenEnabled)
		{
			TouchScreenClose();
			TouchScreenEnabled = false;
		}

		SerialPortEnabled=1-SerialPortEnabled;
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

			SerialPortEnabled = true;

			Kill_Serial();

			SerialPort = 0;
			bSerialStateChanged=TRUE;
			TouchScreenOpen();
		}
		UpdateSerialMenu(hMenu);
		break;

	case ID_COM1:
		SerialPort=1; bSerialStateChanged=TRUE; TouchScreenEnabled = false; TouchScreenClose(); UpdateSerialMenu(hMenu); break;

	case ID_COM2:
		SerialPort=2; bSerialStateChanged=TRUE; TouchScreenEnabled = false; TouchScreenClose(); UpdateSerialMenu(hMenu); break;

	case ID_COM3:
		SerialPort=3; bSerialStateChanged=TRUE; TouchScreenEnabled = false; TouchScreenClose(); UpdateSerialMenu(hMenu); break;

	case ID_COM4:
		SerialPort=4; bSerialStateChanged=TRUE; TouchScreenEnabled = false; TouchScreenClose(); UpdateSerialMenu(hMenu); break;


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
		if (m_DisplayRenderer != IDM_DISPGDI)
		{
			ExitDX();
		}

		m_DisplayRenderer = MenuId;
		TranslateWindowSize();
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
		break;
	}

	case IDM_DXSMOOTHING:
		m_DXSmoothing = !m_DXSmoothing;
		CheckMenuItem(hMenu, IDM_DXSMOOTHING, m_DXSmoothing ? MF_CHECKED : MF_UNCHECKED);
		if (m_DisplayRenderer != IDM_DISPGDI)
		{
			ResetDX();
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
			CheckMenuItem(hMenu, m_MenuIdWinSize, MF_UNCHECKED);
			m_MenuIdWinSize = MenuId;
			CheckMenuItem(hMenu, m_MenuIdWinSize, MF_CHECKED);
			TranslateWindowSize();
			SetWindowAttributes(m_isFullScreen);
		}
		break;

	case ID_VIEW_DD_640X480:
	case ID_VIEW_DD_1024X768:
	case ID_VIEW_DD_1280X768:
	case ID_VIEW_DD_1280X960:
	case ID_VIEW_DD_1280X1024:
	case ID_VIEW_DD_1440X900:
	case ID_VIEW_DD_1600X1200:
		{
		if (!m_isFullScreen)
			HandleCommand(IDM_FULLSCREEN);
			if (m_DisplayRenderer == IDM_DISPGDI) 
				// Should not happen since the items are grayed out, but anyway...
				HandleCommand(IDM_DISPDX9);

			CheckMenuItem(hMenu, m_DDFullScreenMode, MF_UNCHECKED);
			m_DDFullScreenMode = MenuId;
			CheckMenuItem(hMenu, m_DDFullScreenMode, MF_CHECKED);
			TranslateWindowSize();
			SetWindowAttributes(m_isFullScreen);
		}
		break;

	case IDM_FULLSCREEN:
		m_isFullScreen = !m_isFullScreen;
		CheckMenuItem(hMenu, IDM_FULLSCREEN, m_isFullScreen ? MF_CHECKED : MF_UNCHECKED);
		TranslateWindowSize();
		SetWindowAttributes(!m_isFullScreen);
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

			if (SpeechDefault)
			{
				tms5220_stop();
				tms5220_start();
			}
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

		CheckMenuItem(hMenu,  MenuId, RomWritable[( MenuId-IDM_ALLOWWRITES_ROM0)] ? MF_UNCHECKED : MF_CHECKED );
		GetRomMenu();	// Update the Rom/Ram state for all the roms.
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
				AtoDReset();
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
				AtoDInit();
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
		strcat(TmpPath, "README.txt");
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
        Tube186Enabled = 0;
        TorchTube = 0;
		AcornZ80 = 0;
        CheckMenuItem(hMenu, IDM_TUBE, (TubeEnabled)?MF_CHECKED:MF_UNCHECKED);
        CheckMenuItem(hMenu, IDM_TUBE186, (Tube186Enabled)?MF_CHECKED:MF_UNCHECKED);
        CheckMenuItem(hMenu, IDM_TORCH, (TorchTube)?MF_CHECKED:MF_UNCHECKED);
        CheckMenuItem(hMenu, IDM_ARM, (ArmTube)?MF_CHECKED:MF_UNCHECKED);
		CheckMenuItem(hMenu, IDM_ACORNZ80, (AcornZ80)?MF_CHECKED:MF_UNCHECKED);
		ResetBeebSystem(MachineType,TubeEnabled,0);
		break;

	case IDM_TUBE:
		TubeEnabled=1-TubeEnabled;
        Tube186Enabled = 0;
        TorchTube = 0;
        ArmTube = 0;
		AcornZ80 = 0;
        CheckMenuItem(hMenu, IDM_TUBE, (TubeEnabled)?MF_CHECKED:MF_UNCHECKED);
        CheckMenuItem(hMenu, IDM_ARM, (ArmTube)?MF_CHECKED:MF_UNCHECKED);
        CheckMenuItem(hMenu, IDM_TUBE186, (Tube186Enabled)?MF_CHECKED:MF_UNCHECKED);
        CheckMenuItem(hMenu, IDM_TORCH, (TorchTube)?MF_CHECKED:MF_UNCHECKED);
		CheckMenuItem(hMenu, IDM_ACORNZ80, (AcornZ80)?MF_CHECKED:MF_UNCHECKED);
		ResetBeebSystem(MachineType,TubeEnabled,0);
		break;

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

    case IDM_TORCH:
		TorchTube=1-TorchTube;
		TubeEnabled=0;
		Tube186Enabled=0;
		AcornZ80 = 0;
        ArmTube = 0;
        CheckMenuItem(hMenu, IDM_ARM, (ArmTube)?MF_CHECKED:MF_UNCHECKED);
        CheckMenuItem(hMenu, IDM_TUBE, (TubeEnabled)?MF_CHECKED:MF_UNCHECKED);
        CheckMenuItem(hMenu, IDM_TUBE186, (Tube186Enabled)?MF_CHECKED:MF_UNCHECKED);
		CheckMenuItem(hMenu, IDM_TORCH, (TorchTube)?MF_CHECKED:MF_UNCHECKED);
		CheckMenuItem(hMenu, IDM_ACORNZ80, (AcornZ80)?MF_CHECKED:MF_UNCHECKED);
		ResetBeebSystem(MachineType,TubeEnabled,0);
		break;
    
    case IDM_ACORNZ80:
		AcornZ80=1-AcornZ80;
		TubeEnabled=0;
		TorchTube=0;
		Tube186Enabled=0;
        ArmTube = 0;
        CheckMenuItem(hMenu, IDM_ARM, (ArmTube)?MF_CHECKED:MF_UNCHECKED);
		CheckMenuItem(hMenu, IDM_TUBE, (TubeEnabled)?MF_CHECKED:MF_UNCHECKED);
		CheckMenuItem(hMenu, IDM_TUBE186, (Tube186Enabled)?MF_CHECKED:MF_UNCHECKED);
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
	case ID_1773:
		SBSize=1;
		UpdateOptiMenu();
		break;
	case ID_443:
		SBSize=0;
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

	case ID_TELETEXT:
		TeleTextAdapterEnabled = 1-TeleTextAdapterEnabled;
		TeleTextInit();
		CheckMenuItem(hMenu, ID_TELETEXT, TeleTextAdapterEnabled ? MF_CHECKED:MF_UNCHECKED);
		break;

	case ID_HARDDRIVE:
		HardDriveEnabled = 1-HardDriveEnabled;
		SCSIReset();
		SASIReset();
		CheckMenuItem(hMenu, ID_HARDDRIVE, HardDriveEnabled ? MF_CHECKED:MF_UNCHECKED);
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
				SysReg.SetBinaryValue(HKEY_LOCAL_MACHINE, CFG_KEYBOARD_LAYOUT,
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
			// Delete does not work?
			// SysReg.DeleteKey(HKEY_LOCAL_MACHINE, CFG_KEYBOARD_LAYOUT "\\" CFG_SCANCODE_MAP);
			int binsize=0;
			SysReg.SetBinaryValue(HKEY_LOCAL_MACHINE, CFG_KEYBOARD_LAYOUT,
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
	CheckMenuItem(m_hMenu,IDM_SOUNDONOFF,(SoundEnabled)?MF_CHECKED:MF_UNCHECKED);
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

	m_CommandLineFileName = NULL;

	i = 1;
	while (i < __argc)
	{
		if (_stricmp(__argv[i], "-DisMenu") == 0)
		{
			DisableMenu = 1;
		}
		else if (__argv[i][0] == '-' && i+1 >= __argc)
		{
			sprintf(errstr,"Invalid command line parameter:\n  %s", __argv[i]);
			MessageBox(m_hWnd,errstr,WindowTitle,MB_OK|MB_ICONERROR);
		}
		else
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
				strncpy(m_KbdCmd, __argv[++i], 1024);
			}
			else if (__argv[i][0] == '-')
			{
				invalid = true;
				++i;
			}
			else
			{
				// Assume its a file name
				m_CommandLineFileName = __argv[i];
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
// Handle a file name passed on command line
	
void BeebWin::HandleCommandLineFile()
{
	bool ssd = false;
	bool dsd = false;
	bool adfs = false;
	bool cont = false;
	char TmpPath[_MAX_PATH];
	char *FileName = NULL;
	bool uef = false;
	bool csw = false;
	
	// See if file is readable
	if (m_CommandLineFileName != NULL)
	{
		FileName = m_CommandLineFileName;

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
					LoadSimpleDSDiscImage(FileName, 0, 80);
				else
					Load1770DiscImage(FileName,0,1,m_hMenu); // 1 = dsd
			}
			if (ssd)
			{
				if (NativeFDC)
					LoadSimpleDiscImage(FileName, 0, 0, 80);
				else
					Load1770DiscImage(FileName,0,0,m_hMenu); // 0 = ssd
			}
			if (adfs)
			{
				if (!NativeFDC)
					Load1770DiscImage(FileName,0,2,m_hMenu); // 2 = adfs
				else
					cont = false;  // cannot load adfs with native DFS
			}
		}
		else if (MachineType==3)
		{
			if (dsd)
				Load1770DiscImage(FileName,0,1,m_hMenu); // 0 = ssd
			if (ssd)
				Load1770DiscImage(FileName,0,0,m_hMenu); // 1 = dsd
			if (adfs)
				Load1770DiscImage(FileName,0,2,m_hMenu); // 2 = adfs
		}
	}

	if (cont)
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
			sprintf(path, "%sPhroms.cfg", m_UserDataPath);
			att = GetFileAttributes(path);
			if (att == INVALID_FILE_ATTRIBUTES)
				copy_user_files = true;
		}
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
			sprintf(errstr, "Essential files missing from BeebEm data folder:\n  %s"
					"\n\nCopy essential files into folder?", m_UserDataPath);
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
		// Check that user specified roms file exists
		if (strcmp(RomFile, "Roms.cfg") != 0)
		{
			sprintf(path, "%s%s", m_UserDataPath, RomFile);
			att = GetFileAttributes(path);
			if (att == INVALID_FILE_ATTRIBUTES)
			{
				sprintf(errstr, "Cannot open ROMs file:\n  %s", path);
				MessageBox(m_hWnd, errstr, WindowTitle, MB_OK|MB_ICONERROR);
				success = false;
			}
		}
	}

	return success;
}

/****************************************************************************/
void BeebWin::HandleTimer()
{
	int row,col;
	
	// Release previous key press (except shift/control)
	if (m_KbdCmdPress &&
		m_KbdCmdKey != VK_SHIFT && m_KbdCmdKey != VK_CONTROL)
	{
		TranslateKey(m_KbdCmdKey, 1, row, col);
		m_KbdCmdPress = false;
		SetTimer(m_hWnd, 1, 40, NULL);
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
			
			if (m_KbdCmdPress)
				TranslateKey(m_KbdCmdKey, 0, row, col);
			else
				TranslateKey(m_KbdCmdKey, 1, row, col);
			
			SetTimer(m_hWnd, 1, 40, NULL);
		}
	}
}
