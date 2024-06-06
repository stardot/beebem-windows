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
// 11/01/1998: Converted to use DirectX, Mike Wyatt
// 28/12/2004: Econet added Rob O'Donnell. robert@irrelevant.com.
// 26/12/2011: Added IDE Drive to Hardware options, JGH

#include <windows.h>
#include <windowsx.h>
#include <initguid.h>

#pragma warning(push)
#pragma warning(disable: 4091) // ignored on left of 'tagGPFIDL_FLAGS' when no variable is declared
#include <shlobj.h>
#pragma warning(pop)

#include <shlwapi.h>

#include <stdio.h>

#include <algorithm>
#include <array>
#include <string>

#pragma warning(push)
#pragma warning(disable: 4458) // declaration of 'xxx' hides class member
using std::min;
using std::max;
#include <gdiplus.h>
#pragma warning(pop)

#include "BeebWin.h"
#include "6502core.h"
#include "AboutDialog.h"
#include "Arm.h"
#include "AtoDConv.h"
#include "AviWriter.h"
#include "BeebMem.h"
#include "Debug.h"
#include "DebugTrace.h"
#include "Disc1770.h"
#include "Disc8271.h"
#include "DiscInfo.h"
#include "Econet.h" // Rob O'Donnell Christmas 2004.
#include "Ext1770.h"
#include "FolderSelectDialog.h"
#include "FileUtils.h"
#include "Ide.h"
#include "IP232.h"
#include "KeyboardLinksDialog.h"
#include "KeyMap.h"
#include "Log.h"
#include "Main.h"
#include "Master512CoPro.h"
#include "Messages.h"
#include "Music5000.h"
#include "Port.h"
#include "Registry.h"
#include "Resource.h"
#include "RomConfigDialog.h"
#include "Rtc.h"
#include "Sasi.h"
#include "Scsi.h"
#include "Serial.h"
#include "SerialPortDialog.h"
#include "Sound.h"
#include "SoundStreamer.h"
#include "Speech.h"
#include "SprowCoPro.h"
#include "SysVia.h"
#include "TapeControlDialog.h"
#include "Teletext.h"
#include "TeletextDialog.h"
#include "TouchScreen.h"
#include "Tube.h"
#include "UefState.h"
#include "UserKeyboardDialog.h"
#include "UserPortBreakoutBox.h"
#include "UserPortRTC.h"
#include "UserVia.h"
#include "Version.h"
#include "Video.h"
#include "Z80mem.h"
#include "Z80.h"

using namespace Gdiplus;

// Some LED based constants
constexpr int LED_COL_BASE = 64;

static const char *CFG_REG_KEY = "Software\\BeebEm";

static const unsigned char CFG_DISABLE_WINDOWS_KEYS[24] = {
	00,00,00,00,00,00,00,00,03,00,00,00,00,00,0x5B,0xE0,00,00,0x5C,0xE0,00,00,00,00
};

CArm *arm = nullptr;
CSprowCoPro *sprow = nullptr;

LEDType LEDs;

const char *WindowTitle = "BeebEm - BBC Model B / Master Series Emulator";

constexpr int TIMER_KEYBOARD       = 1;
constexpr int TIMER_AUTOBOOT_DELAY = 2;

const char DefaultBlurIntensities[8] = { 100, 88, 75, 62, 50, 38, 25, 12 };

/****************************************************************************/

BeebWin::BeebWin()
{
	// Main window
	m_hWnd = nullptr;
	strcpy(m_szTitle, WindowTitle);
	m_FullScreen = false;
	m_StartFullScreen = false;

	// Menu
	m_hMenu = nullptr;
	m_MenuOn = false;
	m_HideMenuEnabled = false;
	m_DisableMenu = false;

	// Timing
	m_ShowSpeedAndFPS = false;
	m_TimingType = TimingType::FixedSpeed;
	m_TimingSpeed = 100;
	m_RealTimeTarget = 0.0;
	m_CyclesPerSec = 0;
	m_LastTickCount = 0;
	m_LastStatsTickCount = 0;
	m_LastTotalCycles = 0;
	m_LastStatsTotalCycles = 0;
	m_TickBase = 0;
	m_CycleBase = 0;
	m_MinFrameCount = 0;
	m_LastFPSCount = 0;
	m_FPSTarget = 0;
	m_ScreenRefreshCount = 0;
	m_RelativeSpeed = 1.0;
	m_FramesPerSecond = 50.0;

	// Pause / freeze emulation
	m_StartPaused = false;
	m_EmuPaused = false;
	m_WasPaused = false;
	m_FreezeWhenInactive = false;
	m_Frozen = false;

	// Window size
	m_XWinSize = 640;
	m_YWinSize = 512;
	m_XLastWinSize = m_XWinSize;
	m_YLastWinSize = m_YWinSize;
	m_XWinPos = -1;
	m_YWinPos = -1;
	m_XDXSize = 640;
	m_YDXSize = 480;
	m_XRatioAdj = 0.0f;
	m_YRatioAdj = 0.0f;
	m_XRatioCrop = 0.0f;
	m_YRatioCrop = 0.0f;

	// Graphics rendering
	m_hDC = nullptr;
	m_hOldObj = nullptr;
	m_hDCBitmap = nullptr;
	m_hBitmap = nullptr;
	ZeroMemory(&m_bmi, sizeof(m_bmi));
	m_PaletteType = PaletteType::RGB;
	m_screen = nullptr;
	m_screen_blur = nullptr;
	m_LastStartY = 0;
	m_LastNLines = 256;
	m_MotionBlur = 0;
	memcpy(m_BlurIntensities, DefaultBlurIntensities, sizeof(m_BlurIntensities));
	m_MaintainAspectRatio = true;
	m_DisplayRenderer = DisplayRendererType::DirectX9;
	m_CurrentDisplayRenderer = DisplayRendererType::GDI;
	m_DDFullScreenMode = DirectXFullScreenMode::ScreenResolution;
	m_DiscLedColour = LEDColour::Red;

	// DirectX stuff
	m_DXInit = false;
	m_DXResetPending = false;
	m_DXDeviceLost = false;

	// DirectDraw stuff
	m_hInstDDraw = nullptr;
	m_DD = nullptr;
	m_DD2  = nullptr;
	m_DDSPrimary = nullptr;
	m_DDS2Primary = nullptr;
	m_DDSOne = nullptr;
	m_DDS2One = nullptr;
	m_DXSmoothing = true;
	m_DXSmoothMode7Only = false;
	m_Clipper = nullptr;

	// Direct3D9 stuff
	m_pD3D = nullptr;
	m_pd3dDevice = nullptr;
	m_pVB = nullptr;
	m_pTexture = nullptr;
	ZeroMemory(&m_TextureMatrix, sizeof(m_TextureMatrix));

	// Audio
	m_SampleRate = 44100;
	m_SoundVolume = 100;

	// Joystick input
	m_JoystickCaptured = false;
	ZeroMemory(&m_JoystickCaps, sizeof(m_JoystickCaps));
	m_JoystickOption = JoystickOption::Disabled;

	// Mouse capture
	m_HideCursor = false;
	m_CaptureMouse = false;
	m_MouseCaptured = false;
	m_RelMousePos = { 0, 0 };

	// Keyboard input
	m_KeyboardMapping = KeyboardMappingType::Logical;
	m_KeyMapAS = false;
	m_KeyMapFunc = false;
	ZeroMemory(m_UserKeyMapPath, sizeof(m_UserKeyMapPath));
	m_DisableKeysWindows = false;
	m_DisableKeysBreak = false;
	m_DisableKeysEscape = false;
	m_DisableKeysShortcut = false;
	m_ShiftPressed = false;
	m_ShiftBooted = false;

	for (int k = 0; k < 256; ++k)
	{
		m_vkeyPressed[k][0][0] = -1;
		m_vkeyPressed[k][1][0] = -1;
		m_vkeyPressed[k][0][1] = -1;
		m_vkeyPressed[k][1][1] = -1;
	}

	InitKeyMap();

	// File paths

	// Get the applications path - used for non-user files
	char AppPath[_MAX_PATH];
	char AppDrive[_MAX_DRIVE];
	char AppDir[_MAX_DIR];
	GetModuleFileName(nullptr, AppPath, _MAX_PATH);
	_splitpath(AppPath, AppDrive, AppDir, nullptr, nullptr);
	_makepath(m_AppPath, AppDrive, AppDir, nullptr, nullptr);

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

	m_CustomData = false;
	ZeroMemory(m_DiscPath, sizeof(m_DiscPath)); // Set in BeebWin::Initialise()
	m_WriteProtectDisc[0] = !IsDiscWritable(0);
	m_WriteProtectDisc[1] = !IsDiscWritable(1);
	m_WriteProtectOnLoad = true;

	// AMX mouse
	m_AMXSize = AMXSizeType::_320x256;
	m_AMXXSize = 320;
	m_AMXYSize = 256;
	m_AMXAdjust = 30;

	// Preferences
	strcpy(m_PrefsFile, "Preferences.cfg"); // May be overridden by command line parameter.
	m_AutoSavePrefsCMOS = false;
	m_AutoSavePrefsFolders = false;
	m_AutoSavePrefsAll = false;
	m_AutoSavePrefsChanged = false;

	// Clipboard
	m_ClipboardLength = 0;
	m_ClipboardIndex = 0;

	// Printer
	ZeroMemory(m_PrinterBuffer, sizeof(m_PrinterBuffer));
	m_PrinterBufferLen = 0;
	m_TranslateCRLF = true;
	m_PrinterPort = PrinterPortType::Lpt1;
	ZeroMemory(m_PrinterFileName, sizeof(m_PrinterFileName));
	ZeroMemory(m_PrinterDevice, sizeof(m_PrinterDevice));

	// Command line
	ZeroMemory(m_CommandLineFileName1, sizeof(m_CommandLineFileName1));
	ZeroMemory(m_CommandLineFileName2, sizeof(m_CommandLineFileName2));

	// Startup key sequence
	m_KbdCmdPos = -1;
	m_KbdCmdKey = 0;
	m_KbdCmdPress = false;
	m_KbdCmdDelay = 40;
	m_KbdCmdLastCycles = 0;
	m_KeyboardTimerElapsed = false;

	// Disc auto-boot
	m_NoAutoBoot = false;
	m_AutoBootDelay = 0;
	m_AutoBootDisc = false;
	m_BootDiscTimerElapsed = false;

	// ROMs
	ZeroMemory(RomWritePrefs, sizeof(RomWritePrefs));
	strcpy(RomFile, "Roms.cfg"); // May be overridden by command line parameter.

	// Bitmap capture
	m_gdiplusToken = 0;
	m_CaptureBitmapPending = false;
	m_CaptureBitmapAutoFilename = false;
	ZeroMemory(m_CaptureFileName, sizeof(m_CaptureFileName));
	m_BitmapCaptureResolution = BitmapCaptureResolution::_640x512;
	m_BitmapCaptureFormat = BitmapCaptureFormat::Bmp;

	// Video capture
	ZeroMemory(&m_Avibmi, sizeof(m_Avibmi));
	m_AviDIB = nullptr;
	m_AviDC = nullptr;
	m_AviScreen = nullptr;
	m_AviFrameSkip = 1;
	m_AviFrameSkipCount = 0;
	m_AviFrameCount = 0;
	m_VideoCaptureResolution = VideoCaptureResolution::_640x512;

	// Text to speech
	m_TextToSpeechEnabled = false;
	m_hVoiceMenu = nullptr;
	m_SpVoice = nullptr;
	TextToSpeechResetState();
	m_SpeechSpeakPunctuation = false;
	m_SpeechWriteChar = true;
	m_SpeechRate = 0;

	// Text view
	m_hTextView = nullptr;
	m_TextViewEnabled = false;
	m_TextViewPrevWndProc = nullptr;
	ZeroMemory(m_TextViewScreen, sizeof(m_TextViewScreen));

	// Debug
	m_WriteInstructionCounts = false;
}

/****************************************************************************/
bool BeebWin::Initialise()
{
	// Parse command line
	ParseCommandLine();
	FindCommandLineFile(m_CommandLineFileName1);
	FindCommandLineFile(m_CommandLineFileName2);
	CheckForLocalPrefs(m_CommandLineFileName1, false);

	// Check that user data directory exists
	if (!CheckUserDataPath(!m_CustomData))
		return false;

	LoadPreferences();

	// Read disc images path from registry
	if (!RegGetStringValue(HKEY_CURRENT_USER, CFG_REG_KEY, "DiscsPath",
	                       m_DiscPath, _MAX_PATH))
	{
		// Default disc images path to a sub-directory of UserData path
		strcpy(m_DiscPath, m_UserDataPath);

		char DefaultPath[_MAX_PATH];
		m_Preferences.GetStringValue("DiscsPath", DefaultPath);
		GetDataPath(m_DiscPath, DefaultPath);
		strcpy(m_DiscPath, DefaultPath);
	}

	// Override full screen?
	if (m_StartFullScreen)
	{
		m_FullScreen = true;
	}

	if (FAILED(CoInitialize(NULL)))
	{
		Report(MessageType::Error, "Failed to initialise COM");
		return false;
	}

	// Init Windows controls
	INITCOMMONCONTROLSEX cc;
	cc.dwSize = sizeof(cc);
	cc.dwICC = ICC_LISTVIEW_CLASSES;
	InitCommonControlsEx(&cc);

	GdiplusStartupInput gdiplusStartupInput;
	GdiplusStartup(&m_gdiplusToken, &gdiplusStartupInput, NULL);

	WSADATA WsaData;

	if (WSAStartup(MAKEWORD(1, 1), &WsaData) != 0)
	{
		Report(MessageType::Error, "WSA initialisation failed");
		return false;
	}

	InitClass();
	CreateBeebWindow();
	CreateBitmap();

	m_hMenu = GetMenu(m_hWnd);
	m_hDC = GetDC(m_hWnd);

	RomConfig.Load(RomFile);
	ApplyPrefs();

	if (!m_DebugScriptFileName.empty())
	{
		OpenDebugWindow();
		DebugRunScript(m_DebugScriptFileName.c_str());
	}

	if (!m_DebugLabelsFileName.empty())
	{
		if (!DebugLoadSwiftLabels(m_DebugLabelsFileName.c_str()))
		{
			Report(MessageType::Error, "Failed to load symbols file:\n  %s",
			       m_DebugLabelsFileName.c_str());
		}
	}

	char fontFilename[_MAX_PATH];
	strcpy(fontFilename, GetAppPath());
	strcat(fontFilename, "teletext.fnt");

	if (!BuildMode7Font(fontFilename))
	{
		Report(MessageType::Error,
		       "Cannot open Teletext font file:\n  %s",
		       fontFilename);

		return false;
	}

	// Boot file if passed on command line
	HandleCommandLineFile(1, m_CommandLineFileName2);
	HandleCommandLineFile(0, m_CommandLineFileName1);

	if (!m_StartPaused)
	{
		// Schedule first key press if keyboard command supplied
		if (HasKbdCmd())
		{
			SetKeyboardTimer();
		}
	}

	return true;
}

/****************************************************************************/
void BeebWin::ApplyPrefs()
{
	// Set up paths

	if (EconetCfgPath[0] == '\0')
	{
		strcpy(EconetCfgPath, m_UserDataPath);
		strcat(EconetCfgPath, "Econet.cfg");
	}
	else if (PathIsRelative(EconetCfgPath))
	{
		char Filename[_MAX_PATH];
		strcpy(Filename, EconetCfgPath);
		strcpy(EconetCfgPath, m_UserDataPath);
		strcat(EconetCfgPath, Filename);
	}

	if (AUNMapPath[0] == '\0')
	{
		strcpy(AUNMapPath, m_UserDataPath);
		strcat(AUNMapPath, "AUNMap");
	}
	else if (PathIsRelative(AUNMapPath))
	{
		char Filename[_MAX_PATH];
		strcpy(Filename, AUNMapPath);
		strcpy(AUNMapPath, m_UserDataPath);
		strcat(AUNMapPath, Filename);
	}

	strcpy(RomPath, m_UserDataPath);

	char Path[_MAX_PATH];
	m_Preferences.GetStringValue("HardDrivePath", Path);
	GetDataPath(m_UserDataPath, Path);
	strcpy(HardDrivePath, Path);

	// Load key maps
	char KeyMapPath[_MAX_PATH];
	strcpy(KeyMapPath, "Logical.kmap");
	GetDataPath(m_UserDataPath, KeyMapPath);
	ReadKeyMap(KeyMapPath, &LogicalKeyMap);

	strcpy(KeyMapPath, "Default.kmap");
	GetDataPath(m_UserDataPath, KeyMapPath);
	ReadKeyMap(KeyMapPath, &DefaultKeyMap);

	InitMenu();
	ShowMenu(true);

	ExitDX();
	if (m_DisplayRenderer != DisplayRendererType::GDI)
		InitDX();

	InitTextToSpeechVoices();
	InitVoiceMenu();

	if (m_TextToSpeechEnabled)
	{
		m_TextToSpeechEnabled = InitTextToSpeech();
	}

	InitTextView();

	/* Initialise printer */
	if (PrinterEnabled)
		PrinterEnable(m_PrinterDevice);
	else
		PrinterDisable();

	/* Joysticks can only be initialised after the window is created (needs hwnd) */
	if (m_JoystickOption == JoystickOption::Joystick)
		InitJoystick();

	LoadFDC(NULL, true);
	RTCInit();

	SoundReset();
	if (SoundDefault) SoundInit();
	SetSoundMenu();

	// Serial init
	if (SerialPortEnabled)
	{
		if (SerialDestination == SerialType::TouchScreen)
		{
			TouchScreenOpen();
		}
		else if (SerialDestination == SerialType::IP232)
		{
			if (!IP232Open())
			{
				bSerialStateChanged = true;
				SerialPortEnabled = false;
				UpdateSerialMenu();
			}
		}
	}

	ResetBeebSystem(MachineType, true);

	// ROM write flags
	for (int slot = 0; slot < 16; ++slot)
	{
		if (!RomWritePrefs[slot])
			RomWritable[slot] = false;
	}

	SetRomMenu();
}

/****************************************************************************/

BeebWin::~BeebWin()
{
	if (m_DisplayRenderer != DisplayRendererType::GDI)
		ExitDX();

	CloseDX9();

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
	EndVideo();

	if (m_AutoSavePrefsCMOS || m_AutoSavePrefsFolders ||
	    m_AutoSavePrefsAll || m_AutoSavePrefsChanged)
	{
		SavePreferences(m_AutoSavePrefsAll);
	}

	if (SoundEnabled)
		SoundReset();

	Music5000Reset();

	CloseTextToSpeech();
	CloseTextView();

	IP232Close();

	WSACleanup();

	if (m_WriteInstructionCounts) {
		char FileName[_MAX_PATH];
		strcpy(FileName, m_UserDataPath);
		strcat(FileName, "InstructionCounts.txt");

		WriteInstructionCounts(FileName);
	}

	SCSIClose();
	SASIClose();
	IDEClose();

	DestroyArmCoPro();
	DestroySprowCoPro();

	TubeType = Tube::None;
}

/****************************************************************************/

void BeebWin::ResetBeebSystem(Model NewModelType, bool LoadRoms)
{
	SoundReset();
	if (SoundDefault)
		SoundInit();

	#if ENABLE_SPEECH

	if (SpeechDefault)
	{
		if (LoadRoms)
		{
			SpeechInit();
		}

		SpeechStop();
		SpeechStart();
	}

	#endif

	SwitchOnSound();
	Music5000Reset();
	if ((Music5000Enabled) && MachineType != Model::MasterET)
		Music5000Init();
	MachineType=NewModelType;
	BeebMemInit(LoadRoms, m_ShiftBooted);
	Init6502core();

	RTCInit();

	if (TubeType == Tube::Acorn65C02)
	{
		Init65C02core();
	}
	else if (TubeType == Tube::Master512CoPro)
	{
		master512CoPro.Reset();
	}
	else if (TubeType == Tube::TorchZ80 || TubeType == Tube::AcornZ80)
	{
		R1Status = 0;
		ResetTube();
		init_z80();
	}
	else if (TubeType == Tube::AcornArm)
	{
		R1Status = 0;
		ResetTube();
		DestroyArmCoPro();
		CreateArmCoPro();
	}
	else if (TubeType == Tube::SprowArm)
	{
		R1Status = 0;
		ResetTube();
		DestroySprowCoPro();
		CreateSprowCoPro();
	}

	SysVIAReset();
	UserVIAReset();
	SerialReset();
	VideoInit();
	SetDiscWriteProtects();
	Disc8271Reset();
	if (EconetEnabled) EconetReset();	//Rob:
	Reset1770();
	AtoDInit();
	SetRomMenu();

	char szDiscName[2][MAX_PATH];
	strcpy(szDiscName[0], DiscInfo[0].FileName);
	strcpy(szDiscName[1], DiscInfo[1].FileName);

	DiscType Type[2];
	Type[0] = DiscInfo[0].Type;
	Type[1] = DiscInfo[1].Type;

	EjectDiscImage(0);
	EjectDiscImage(1);

	if (MachineType != Model::MasterET)
	{
		if (SCSIDriveEnabled) SCSIReset();
		if (SCSIDriveEnabled) SASIReset();
		if (IDEDriveEnabled)  IDEReset();

		TeletextInit();
	}

	if (MachineType == Model::Master128)
	{
		InvertTR00 = false;
	}
	else
	{
		LoadFDC(NULL, false);
	}

	// Keep the disc images loaded

	if (MachineType != Model::Master128 && MachineType != Model::MasterET && NativeFDC)
	{
		// 8271 disc
		if (szDiscName[0][0] != '\0')
		{
			if (Type[0] == DiscType::SSD || Type[0] == DiscType::DSD || Type[0] == DiscType::FSD)
			{
				Load8271DiscImage(szDiscName[0], 0, 80, Type[0]);
			}
		}

		if (szDiscName[1][0] != '\0')
		{
			if (Type[1] == DiscType::SSD || Type[1] == DiscType::DSD || Type[1] == DiscType::FSD)
			{
				Load8271DiscImage(szDiscName[1], 1, 80, Type[1]);
			}
		}
	}

	if ((MachineType != Model::Master128 && !NativeFDC) || (MachineType == Model::Master128))
	{
		// 1770 Disc
		if (szDiscName[0][0] != '\0')
		{
			if (Type[0] != DiscType::FSD)
			{
				Load1770DiscImage(szDiscName[0], 0, Type[0]);
			}
		}

		if (szDiscName[1][0] != '\0')
		{
			if (Type[1] != DiscType::FSD)
			{
				Load1770DiscImage(szDiscName[1], 1, Type[1]);
			}
		}
	}
}

/****************************************************************************/

void BeebWin::Break()
{
	// Must do a reset!
	Init6502core();

	if (TubeType == Tube::Acorn65C02)
	{
		Init65C02core();
	}
	else if (TubeType == Tube::Master512CoPro)
	{
		master512CoPro.Reset();
	}
	else if (TubeType == Tube::AcornZ80 || TubeType == Tube::TorchZ80)
	{
		R1Status = 0;
		ResetTube();
		init_z80();
	}
	else if (TubeType == Tube::AcornArm)
	{
		R1Status = 0;
		ResetTube();
		DestroyArmCoPro();
		CreateArmCoPro();
	}
	else if (TubeType == Tube::SprowArm)
	{
		R1Status = 0;
		ResetTube();
		// We don't want to throw the contents of memory away
		// just tell the co-pro to reset itself.
		sprow->Reset();
	}

	if (MachineType != Model::MasterET)
	{
		Disc8271Reset();
		Reset1770();
	}

	if (EconetEnabled) EconetReset();//Rob

	if (MachineType != Model::MasterET)
	{
		if (SCSIDriveEnabled) SCSIReset();
		if (SCSIDriveEnabled) SASIReset();
		if (IDEDriveEnabled)  IDEReset();
		TeletextInit();

		//SoundChipReset();
		Music5000Reset();

		if (Music5000Enabled)
			Music5000Init();
	}

	#if ENABLE_SPEECH

	if (SpeechDefault)
	{
		SpeechStop();
		SpeechStart();
	}

	#endif

	// Reset IntegraB RTC on Break
	if (MachineType == Model::IntegraB)
	{
		IntegraBRTCReset();
	}
}

/****************************************************************************/

void BeebWin::CreateArmCoPro()
{
	arm = new CArm;

	char ArmROMPath[256];

	strcpy(ArmROMPath, RomPath);
	strcat(ArmROMPath, "BeebFile/ARMeval_100.rom");

	CArm::InitResult Result = arm->init(ArmROMPath);

	switch (Result) {
		case CArm::InitResult::FileNotFound:
			Report(MessageType::Error, "ARM co-processor ROM file not found:\n  %s",
			       ArmROMPath);

			DestroyArmCoPro();

			TubeType = Tube::None;
			UpdateTubeMenu();
			break;

		case CArm::InitResult::Success:
			break;
	}
}

void BeebWin::DestroyArmCoPro()
{
	if (arm)
	{
		delete arm;
		arm = nullptr;
	}
}

void BeebWin::CreateSprowCoPro()
{
	char SprowROMPath[MAX_PATH];

	strcpy(SprowROMPath, RomPath);
	strcat(SprowROMPath, "BeebFile/Sprow.rom");

	sprow = new CSprowCoPro();
	CSprowCoPro::InitResult Result = sprow->Init(SprowROMPath);

	switch (Result) {
		case CSprowCoPro::InitResult::FileNotFound:
			Report(MessageType::Error, "ARM7TDMI co-processor ROM file not found:\n  %s",
			       SprowROMPath);

			DestroySprowCoPro();

			TubeType = Tube::None;
			UpdateTubeMenu();
			break;

		case CSprowCoPro::InitResult::Success:
			break;
	}
}

void BeebWin::DestroySprowCoPro()
{
	if (sprow)
	{
		delete sprow;
		sprow = nullptr;
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
		float r = (float)(i & 1);
		float g = (float)((i & 2) != 0);
		float b = (float)((i & 4) != 0);

		if (m_PaletteType != PaletteType::RGB)
		{
			r = g = b = (float) (0.299 * r + 0.587 * g + 0.114 * b);

			switch (m_PaletteType)
			{
			case PaletteType::Amber:
				r *= (float) 1.0;
				g *= (float) 0.8;
				b *= (float) 0.1;
				break;
			case PaletteType::Green:
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

	if (m_hOldObj == NULL)
	{
		Report(MessageType::Error,
		       "Cannot select the screen bitmap\nTry running in a 256 colour mode");
	}
}

/****************************************************************************/
bool BeebWin::InitClass()
{
	WNDCLASS wc;

	// Fill in window class structure with parameters that describe the
	// main window.

	wc.style         = CS_HREDRAW | CS_VREDRAW; // Class style(s).
	wc.lpfnWndProc   = WndProc; // Window Procedure
	wc.cbClsExtra    = 0; // No per-class extra data.
	wc.cbWndExtra    = 0; // No per-window extra data.
	wc.hInstance     = hInst; // Owner of this class
	wc.hIcon         = LoadIcon(hInst, MAKEINTRESOURCE(IDI_BEEBEM));
	wc.hCursor       = LoadCursor(NULL, IDC_ARROW);
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.lpszMenuName  = MAKEINTRESOURCE(IDR_MENU); // Menu from .rc file
	wc.lpszClassName = "BEEBWIN"; // Name to register as

	// Register the window class and return success/failure code.
	return RegisterClass(&wc) != 0;
}

/****************************************************************************/

void BeebWin::CreateBeebWindow()
{
	int x = m_XWinPos;
	int y = m_YWinPos;

	if (x == -1 || y == -1)
	{
		x = CW_USEDEFAULT;
		y = 0;
		m_XWinPos = 0;
		m_YWinPos = 0;
	}

	int nCmdShow = SW_SHOWNORMAL;

	if (m_DisplayRenderer == DisplayRendererType::GDI && m_FullScreen)
	{
		nCmdShow = SW_MAXIMIZE;
	}

	DWORD dwStyle;

	if (m_DisplayRenderer != DisplayRendererType::GDI && m_FullScreen)
	{
		dwStyle = WS_POPUP;
	}
	else
	{
		dwStyle = WS_OVERLAPPEDWINDOW;
	}

	m_hWnd = CreateWindow(
		"BEEBWIN",  // See RegisterClass() call.
		m_szTitle,  // Text for window title bar.
		dwStyle,    // Window style
		x,
		y,
		m_XWinSize, // See SetWindowAttributes()
		m_YWinSize,
		nullptr,    // Overlapped windows have no parent.
		nullptr,    // Use the window class menu.
		hInst,      // This instance owns this window.
		this
	);

	DisableRoundedCorners(m_hWnd);

	ShowWindow(m_hWnd, nCmdShow); // Show the window
	UpdateWindow(m_hWnd); // Sends WM_PAINT message

	SetWindowAttributes(false);
}

/****************************************************************************/

// Windows 11 draws windows with rounded corners by default, so this function
// disables them, so the Beeb video image isn't affected.
//
// See https://stardot.org.uk/forums/viewtopic.php?f=4&t=26874

void BeebWin::DisableRoundedCorners(HWND hWnd)
{
	HMODULE hDwmApi = LoadLibrary("dwmapi.dll");

	if (hDwmApi == nullptr)
	{
		return;
	}

	typedef HRESULT (STDAPICALLTYPE* DWM_SET_WINDOW_ATTRIBUTE)(HWND, DWORD, LPCVOID, DWORD);

	DWM_SET_WINDOW_ATTRIBUTE DwmSetWindowAttribute = reinterpret_cast<DWM_SET_WINDOW_ATTRIBUTE>(
		GetProcAddress(hDwmApi, "DwmSetWindowAttribute")
	);

	if (DwmSetWindowAttribute != nullptr)
	{
		const DWORD DWMWCP_DONOTROUND = 1;
		const DWORD DWMWA_WINDOW_CORNER_PREFERENCE = 33;
		const DWORD CornerPreference = DWMWCP_DONOTROUND;

		DwmSetWindowAttribute(
			hWnd,
			DWMWA_WINDOW_CORNER_PREFERENCE,
			&CornerPreference,
			sizeof(CornerPreference)
		);
	}

	FreeLibrary(hDwmApi);
}

/****************************************************************************/
void BeebWin::FlashWindow()
{
	::FlashWindow(m_hWnd, TRUE);

	MessageBeep(MB_ICONEXCLAMATION);
}

/****************************************************************************/
void BeebWin::ShowMenu(bool on)
{
	if (m_DisableMenu)
	{
		on = false;
	}

	if (on != m_MenuOn)
	{
		SetMenu(m_hWnd, on ? m_hMenu : nullptr);
	}

	m_MenuOn = on;
}

void BeebWin::HideMenu(bool hide)
{
	if (m_HideMenuEnabled)
	{
		ShowMenu(!hide);
	}
}

void BeebWin::TrackPopupMenu(int x, int y)
{
	::TrackPopupMenu(m_hMenu, TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RIGHTBUTTON,
	                 x, y,
	                 0,
	                 m_hWnd,
	                 NULL);
}

/****************************************************************************/

void BeebWin::CheckMenuItem(UINT id, bool checked)
{
	::CheckMenuItem(m_hMenu, id, checked ? MF_CHECKED : MF_UNCHECKED);
}

void BeebWin::CheckMenuRadioItem(UINT FirstID, UINT LastID, UINT SelectedID)
{
	::CheckMenuRadioItem(m_hMenu, FirstID, LastID, SelectedID, MF_BYCOMMAND);
}

void BeebWin::EnableMenuItem(UINT id, bool enabled)
{
	::EnableMenuItem(m_hMenu, id, enabled ? MF_ENABLED : MF_GRAYED);
}

void BeebWin::InitMenu(void)
{
	// File -> Video Options
	UpdateVideoCaptureMenu();

	// File -> Disc Options
	CheckMenuItem(IDM_WRITE_PROTECT_DISC0, m_WriteProtectDisc[0]);
	CheckMenuItem(IDM_WRITE_PROTECT_DISC1, m_WriteProtectDisc[1]);
	CheckMenuItem(IDM_WRITE_PROTECT_ON_LOAD, m_WriteProtectOnLoad);

	// File -> Capture Options
	UpdateBitmapCaptureFormatMenu();
	UpdateBitmapCaptureResolutionMenu();

	// Edit
	CheckMenuItem(IDM_EDIT_CRLF, m_TranslateCRLF);

	// Comms -> Tape Speed
	SetTapeSpeedMenu();

	// Comms
	CheckMenuItem(IDM_UNLOCKTAPE, TapeState.Unlock);
	CheckMenuItem(IDM_PRINTERONOFF, PrinterEnabled);

	// Comms -> Printer
	UpdatePrinterPortMenu();

	std::string MenuString = "File: ";
	MenuString += m_PrinterFileName;
	ModifyMenu(m_hMenu, IDM_PRINTER_FILE, MF_BYCOMMAND, IDM_PRINTER_FILE, MenuString.c_str());

	// Comms -> RS423
	UpdateSerialMenu();

	// View
	UpdateDisplayRendererMenu();

	const bool DirectXEnabled = m_DisplayRenderer != DisplayRendererType::GDI;
	EnableMenuItem(IDM_DXSMOOTHING, DirectXEnabled);
	EnableMenuItem(IDM_DXSMOOTHMODE7ONLY, DirectXEnabled);

	CheckMenuItem(IDM_DXSMOOTHING, m_DXSmoothing);
	CheckMenuItem(IDM_DXSMOOTHMODE7ONLY, m_DXSmoothMode7Only);

	CheckMenuItem(IDM_SPEEDANDFPS, m_ShowSpeedAndFPS);
	CheckMenuItem(IDM_FULLSCREEN, m_FullScreen);
	CheckMenuItem(IDM_MAINTAINASPECTRATIO, m_MaintainAspectRatio);
	UpdateMonitorMenu();
	CheckMenuItem(IDM_HIDEMENU, m_HideMenuEnabled);
	UpdateLEDMenu();
	CheckMenuItem(IDM_TEXTVIEW, m_TextViewEnabled);

	// View -> Window Sizes
	UpdateWindowSizeMenu();

	// View -> DD mode
	UpdateDirectXFullScreenModeMenu();

	// View -> Motion blur
	UpdateMotionBlurMenu();

	// Speed
	UpdateSpeedMenu();
	// Check menu based on m_EmuPaused to take into account -StartPaused arg
	CheckMenuItem(IDM_EMUPAUSED, m_EmuPaused);

	// Sound
	UpdateSoundStreamerMenu();
	SetSoundMenu();

	#if ENABLE_SPEECH
	CheckMenuItem(IDM_SPEECH, SpeechDefault);
	#else
	RemoveMenu(m_hMenu, IDM_SPEECH, MF_BYCOMMAND);
	#endif

	CheckMenuItem(IDM_SOUNDCHIP, SoundChipEnabled);
	UpdateSFXMenu();
	CheckMenuItem(IDM_TAPESOUND, TapeSoundEnabled);
	UpdateSoundSampleRateMenu();
	UpdateSoundVolumeMenu();
	CheckMenuItem(IDM_PART_SAMPLES, PartSamples);
	CheckMenuItem(IDM_EXPVOLUME, SoundExponentialVolume);
	CheckMenuItem(IDM_TEXTTOSPEECH_ENABLE, m_TextToSpeechEnabled);
	CheckMenuItem(IDM_TEXTTOSPEECH_AUTO_SPEAK, m_SpeechWriteChar);
	CheckMenuItem(IDM_TEXTTOSPEECH_SPEAK_PUNCTUATION, m_SpeechSpeakPunctuation);
	EnableMenuItem(IDM_TEXTTOSPEECH_INCREASE_RATE, m_SpeechRate < 10);
	EnableMenuItem(IDM_TEXTTOSPEECH_DECREASE_RATE, m_SpeechRate > -10);

	// AMX
	CheckMenuItem(IDM_AMXONOFF, AMXMouseEnabled);
	CheckMenuItem(IDM_CAPTUREMOUSE, m_CaptureMouse);
	EnableMenuItem(IDM_CAPTUREMOUSE, AMXMouseEnabled);
	CheckMenuItem(IDM_AMX_LRFORMIDDLE, AMXLRForMiddle);
	UpdateAMXSizeMenu();
	UpdateAMXAdjustMenu();

	// Hardware -> Model
	UpdateModelMenu();

	// Hardware
	UpdateTubeMenu();

	SetRomMenu();
	CheckMenuItem(IDM_SWRAMBOARD, SWRAMBoardEnabled);
	UpdateOptiMenu();
	UpdateEconetMenu();
	CheckMenuItem(IDM_TELETEXT, TeletextAdapterEnabled);
	CheckMenuItem(IDM_FLOPPY_DRIVE, Disc8271Enabled);
	CheckMenuItem(IDM_SCSI_HARD_DRIVE, SCSIDriveEnabled);
	CheckMenuItem(IDM_IDE_HARD_DRIVE, IDEDriveEnabled);
	CheckMenuItem(IDM_USER_PORT_RTC_MODULE, UserPortRTCEnabled);

	// Options
	UpdateJoystickMenu();
	CheckMenuItem(IDM_FREEZEINACTIVE, m_FreezeWhenInactive);
	CheckMenuItem(IDM_HIDECURSOR, m_HideCursor);
	UpdateKeyboardMappingMenu();
	CheckMenuItem(IDM_MAPAS, m_KeyMapAS);
	CheckMenuItem(IDM_MAPFUNCS, m_KeyMapFunc);
	UpdateDisableKeysMenu();
	CheckMenuItem(IDM_AUTOSAVE_PREFS_CMOS, m_AutoSavePrefsCMOS);
	CheckMenuItem(IDM_AUTOSAVE_PREFS_FOLDERS, m_AutoSavePrefsFolders);
	CheckMenuItem(IDM_AUTOSAVE_PREFS_ALL, m_AutoSavePrefsAll);
}

/****************************************************************************/

void BeebWin::SetDisplayRenderer(DisplayRendererType DisplayRenderer)
{
	ExitDX();

	m_DisplayRenderer = DisplayRenderer;
	SetWindowAttributes(m_FullScreen);

	bool DirectXEnabled = m_DisplayRenderer != DisplayRendererType::GDI;

	if (DirectXEnabled)
	{
		InitDX();
	}

	UpdateDisplayRendererMenu();
	EnableMenuItem(IDM_DXSMOOTHING, DirectXEnabled);
	EnableMenuItem(IDM_DXSMOOTHMODE7ONLY, DirectXEnabled);
}

void BeebWin::UpdateDisplayRendererMenu()
{
	static const struct { UINT ID; DisplayRendererType DisplayRenderer; } MenuItems[] =
	{
		{ IDM_DISPGDI,   DisplayRendererType::GDI },
		{ IDM_DISPDDRAW, DisplayRendererType::DirectDraw },
		{ IDM_DISPDX9,   DisplayRendererType::DirectX9 }
	};

	UINT SelectedMenuItemID = 0;

	for (int i = 0; i < _countof(MenuItems); i++)
	{
		if (m_DisplayRenderer == MenuItems[i].DisplayRenderer)
		{
			SelectedMenuItemID = MenuItems[i].ID;
			break;
		}
	}

	CheckMenuRadioItem(IDM_DISPGDI, IDM_DISPDX9, SelectedMenuItemID);
}

/****************************************************************************/

void BeebWin::SetSoundStreamer(SoundStreamerType StreamerType)
{
	SelectedSoundStreamer = StreamerType;

	if (SoundEnabled)
	{
		SoundReset();
		SoundInit();
	}

	if (Music5000Enabled && MachineType != Model::MasterET)
	{
		Music5000Reset();
		Music5000Init();
	}

	#if ENABLE_SPEECH

	if (SpeechDefault)
	{
		SpeechStop();
		SpeechStart();
	}

	#endif

	UpdateSoundStreamerMenu();
}

void BeebWin::UpdateSoundStreamerMenu()
{
	CheckMenuRadioItem(
		IDM_XAUDIO2,
		IDM_DIRECTSOUND,
		SelectedSoundStreamer == SoundStreamerType::XAudio2 ? IDM_XAUDIO2 : IDM_DIRECTSOUND
	);
}

/****************************************************************************/

void BeebWin::SetSoundSampleRate(int SampleRate)
{
	if (SampleRate != m_SampleRate)
	{
		m_SampleRate = SampleRate;

		UpdateSoundSampleRateMenu();

		if (SoundEnabled)
		{
			SoundReset();
			SoundInit();
		}

		#if ENABLE_SPEECH

		if (SpeechDefault)
		{
			SpeechStop();
			SpeechStart();
		}

		#endif
	}
}

void BeebWin::UpdateSoundSampleRateMenu()
{
	static const struct { UINT ID; int SampleRate; } MenuItems[] =
	{
		{ IDM_44100KHZ, 44100 },
		{ IDM_22050KHZ, 22050 },
		{ IDM_11025KHZ, 11025 }
	};

	UINT SelectedMenuItemID = 0;

	for (int i = 0; i < _countof(MenuItems); i++)
	{
		if (m_SampleRate == MenuItems[i].SampleRate)
		{
			SelectedMenuItemID = MenuItems[i].ID;
			break;
		}
	}

	CheckMenuRadioItem(IDM_44100KHZ, IDM_11025KHZ, SelectedMenuItemID);
}

/****************************************************************************/

void BeebWin::SetSoundVolume(int Volume)
{
	if (Volume != m_SoundVolume)
	{
		m_SoundVolume = Volume;

		UpdateSoundVolumeMenu();
	}
}

void BeebWin::UpdateSoundVolumeMenu()
{
	static const struct { UINT ID; int Volume; } MenuItems[] =
	{
		{ IDM_FULLVOLUME,   100 },
		{ IDM_HIGHVOLUME,   75 },
		{ IDM_MEDIUMVOLUME, 50 },
		{ IDM_LOWVOLUME,    25 }
	};

	UINT SelectedMenuItemID = 0;

	for (int i = 0; i < _countof(MenuItems); i++)
	{
		if (m_SoundVolume == MenuItems[i].Volume)
		{
			SelectedMenuItemID = MenuItems[i].ID;
			break;
		}
	}

	CheckMenuRadioItem(IDM_FULLVOLUME, IDM_LOWVOLUME, SelectedMenuItemID);
}

/****************************************************************************/

void BeebWin::UpdateMonitorMenu()
{
	static const struct { UINT ID; PaletteType Type; } MenuItems[] =
	{
		{ IDM_MONITOR_RGB,   PaletteType::RGB   },
		{ IDM_MONITOR_BW,    PaletteType::BW    },
		{ IDM_MONITOR_AMBER, PaletteType::Amber },
		{ IDM_MONITOR_GREEN, PaletteType::Green }
	};

	UINT SelectedMenuItemID = 0;

	for (int i = 0; i < _countof(MenuItems); i++)
	{
		if (m_PaletteType == MenuItems[i].Type)
		{
			SelectedMenuItemID = MenuItems[i].ID;
			break;
		}
	}

	CheckMenuRadioItem(IDM_MONITOR_RGB, IDM_MONITOR_GREEN, SelectedMenuItemID);
}

void BeebWin::UpdateModelMenu()
{
	static const struct { UINT ID; Model Type; } MenuItems[] =
	{
		{ IDM_MODELB,     Model::B },
		{ IDM_MODELBINT,  Model::IntegraB },
		{ IDM_MODELBPLUS, Model::BPlus },
		{ IDM_MASTER128,  Model::Master128 },
		{ IDM_MASTER_ET,  Model::MasterET }
	};

	UINT SelectedMenuItemID = 0;

	for (int i = 0; i < _countof(MenuItems); i++)
	{
		if (MachineType == MenuItems[i].Type)
		{
			SelectedMenuItemID = MenuItems[i].ID;
			break;
		}
	}

	CheckMenuRadioItem(IDM_MODELB, IDM_MASTER_ET, SelectedMenuItemID);

	if (MachineType == Model::Master128)
	{
		EnableMenuItem(IDM_FDC_DLL, false);
	}
	else if (MachineType != Model::MasterET)
	{
		EnableMenuItem(IDM_FDC_DLL, true);
	}

	const bool IsMasterET = MachineType == Model::MasterET;
	const bool IsModelB = MachineType == Model::B || MachineType == Model::BPlus || MachineType == Model::IntegraB;

	// File Menu Item
	EnableMenuItem(IDM_RUNDISC, !IsMasterET);
	EnableMenuItem(IDM_LOADDISC0, !IsMasterET);
	EnableMenuItem(IDM_LOADDISC1, !IsMasterET);
	// Disc Options sub-menu
	EnableMenuItem(IDM_NEWDISC0, !IsMasterET);
	EnableMenuItem(IDM_NEWDISC1, !IsMasterET);
	EnableMenuItem(IDM_EJECTDISC0, !IsMasterET);
	EnableMenuItem(IDM_EJECTDISC1, !IsMasterET);
	EnableMenuItem(IDM_WRITE_PROTECT_DISC0, !IsMasterET);
	EnableMenuItem(IDM_WRITE_PROTECT_DISC1, !IsMasterET);
	EnableMenuItem(IDM_WRITE_PROTECT_ON_LOAD, !IsMasterET);

	EnableMenuItem(IDM_LOADTAPE, !IsMasterET);

	// Edit Menu Item
	EnableMenuItem(IDM_DISC_IMPORT_0 , !IsMasterET);
	EnableMenuItem(IDM_DISC_IMPORT_1, !IsMasterET);
	EnableMenuItem(IDM_DISC_IMPORT_2, !IsMasterET);
	EnableMenuItem(IDM_DISC_IMPORT_3, !IsMasterET);
	EnableMenuItem(IDM_DISC_EXPORT_0, !IsMasterET);
	EnableMenuItem(IDM_DISC_EXPORT_1, !IsMasterET);
	EnableMenuItem(IDM_DISC_EXPORT_2, !IsMasterET);
	EnableMenuItem(IDM_DISC_EXPORT_3, !IsMasterET);

	// Comms Menu Item
	// Tape speed sub-menu
	EnableMenuItem(IDM_TAPE_FAST, !IsMasterET);
	EnableMenuItem(IDM_TAPE_MFAST, !IsMasterET);
	EnableMenuItem(IDM_TAPE_MSLOW, !IsMasterET);
	EnableMenuItem(IDM_TAPE_NORMAL, !IsMasterET);

	EnableMenuItem(IDM_REWINDTAPE, !IsMasterET);
	EnableMenuItem(IDM_UNLOCKTAPE, !IsMasterET);
	EnableMenuItem(IDM_TAPECONTROL, !IsMasterET);
	EnableMenuItem(IDM_SERIAL, !IsMasterET);
	EnableMenuItem(IDM_SELECT_SERIAL_DESTINATION, !IsMasterET);

	// View Menu Item
	// LED sub-menu
	EnableMenuItem(IDM_RED_LEDS, !IsMasterET);
	EnableMenuItem(IDM_GREEN_LEDS, !IsMasterET);
	EnableMenuItem(IDM_SHOW_DISCLEDS, !IsMasterET);

	// Sound Menu Item
	EnableMenuItem(IDM_MUSIC5000, !IsMasterET);

	// AMX Menu Item
	EnableMenuItem(IDM_AMXONOFF, !IsMasterET);
	EnableMenuItem(IDM_CAPTUREMOUSE, AMXMouseEnabled && !IsMasterET);
	EnableMenuItem(IDM_AMX_LRFORMIDDLE, !IsMasterET);
	EnableMenuItem(IDM_AMX_LRFORMIDDLE, !IsMasterET);
	EnableMenuItem(IDM_AMX_160X256, !IsMasterET);
	EnableMenuItem(IDM_AMX_320X256, !IsMasterET);
	EnableMenuItem(IDM_AMX_640X256, !IsMasterET);
	EnableMenuItem(IDM_AMX_ADJUSTP50, !IsMasterET);
	EnableMenuItem(IDM_AMX_ADJUSTP30, !IsMasterET);
	EnableMenuItem(IDM_AMX_ADJUSTP10, !IsMasterET);
	EnableMenuItem(IDM_AMX_ADJUSTM10, !IsMasterET);
	EnableMenuItem(IDM_AMX_ADJUSTM30, !IsMasterET);
	EnableMenuItem(IDM_AMX_ADJUSTM50, !IsMasterET);

	// Hardware Menu
	EnableMenuItem(IDM_AMX_ADJUSTM50, !IsMasterET);
	// Model B Floppy Controller sub-menu
	EnableMenuItem(IDM_8271, IsModelB);
	EnableMenuItem(IDM_FDC_DLL, IsModelB);

	EnableMenuItem(IDM_BASIC_HARDWARE_ONLY, !IsMasterET);
	EnableMenuItem(IDM_TELETEXT, !IsMasterET);
	EnableMenuItem(IDM_SELECT_TELETEXT_DATA_SOURCE, !IsMasterET);
	EnableMenuItem(IDM_SET_KEYBOARD_LINKS, IsModelB);
	EnableMenuItem(IDM_FLOPPY_DRIVE, !IsMasterET);
	EnableMenuItem(IDM_SCSI_HARD_DRIVE, !IsMasterET);
	EnableMenuItem(IDM_IDE_HARD_DRIVE, !IsMasterET);
	EnableMenuItem(IDM_SELECT_HARD_DRIVE_FOLDER, !IsMasterET);
	EnableMenuItem(IDM_BREAKOUT, !IsMasterET);
	EnableMenuItem(IDM_USER_PORT_RTC_MODULE, !IsMasterET);

	// Options Menu
	EnableMenuItem(IDM_JOYSTICK, !IsMasterET);
	EnableMenuItem(IDM_ANALOGUE_MOUSESTICK, !IsMasterET);
	EnableMenuItem(IDM_DIGITAL_MOUSESTICK, !IsMasterET);
}

void BeebWin::UpdateSFXMenu()
{
	CheckMenuItem(IDM_SFX_RELAY, RelaySoundEnabled);
	CheckMenuItem(IDM_SFX_DISCDRIVES, DiscDriveSoundEnabled);
}

void BeebWin::UpdateDisableKeysMenu()
{
	CheckMenuItem(IDM_DISABLEKEYSWINDOWS, m_DisableKeysWindows);
	CheckMenuItem(IDM_DISABLEKEYSBREAK, m_DisableKeysBreak);
	CheckMenuItem(IDM_DISABLEKEYSESCAPE, m_DisableKeysEscape);
	CheckMenuItem(IDM_DISABLEKEYSSHORTCUT, m_DisableKeysShortcut);
}

/****************************************************************************/

void BeebWin::UpdateTubeMenu()
{
	static const struct { UINT ID; Tube Type; } MenuItems[] =
	{
		{ IDM_TUBE_NONE,       Tube::None },
		{ IDM_TUBE_ACORN65C02, Tube::Acorn65C02 },
		{ IDM_TUBE_MASTER512,  Tube::Master512CoPro },
		{ IDM_TUBE_ACORNZ80,   Tube::AcornZ80 },
		{ IDM_TUBE_TORCHZ80,   Tube::TorchZ80 },
		{ IDM_TUBE_ACORNARM,   Tube::AcornArm },
		{ IDM_TUBE_SPROWARM,   Tube::SprowArm }
	};

	UINT SelectedMenuItemID = 0;

	for (int i = 0; i < _countof(MenuItems); i++)
	{
		if (TubeType == MenuItems[i].Type)
		{
			SelectedMenuItemID = MenuItems[i].ID;
			break;
		}
	}

	CheckMenuRadioItem(IDM_TUBE_NONE, IDM_TUBE_SPROWARM, SelectedMenuItemID);
}

/****************************************************************************/

void BeebWin::SetRomMenu()
{
	// Set the ROM Titles in the ROM/RAM menu.

	for (int i = 0; i < 16; i++)
	{
		CHAR Title[19];

		Title[0] = '&';
		_itoa(i, &Title[1], 16);
		Title[2] = ' ';

		// Get the Rom Title.
		ReadRomTitle(i, &Title[3], sizeof(Title) - 4);

		if (Title[3] == '\0')
		{
			if (RomBankType[i] == BankType::Ram)
			{
				strcpy(&Title[3], "RAM");
			}
			else
			{
				strcpy(&Title[3], "Empty");
			}
		}

		ModifyMenu(m_hMenu,                  // handle of menu
		           IDM_ALLOWWRITES_ROM0 + i, // menu item to modify
		           MF_BYCOMMAND,
		           IDM_ALLOWWRITES_ROM0 + i, // menu item identifier or pop-up menu handle
		           Title);                   // menu item content

		// Disable ROM and uncheck the ROM/RAM which are NOT writable
		EnableMenuItem(IDM_ALLOWWRITES_ROM0 + i, RomBankType[i] == BankType::Ram);
		CheckMenuItem(IDM_ALLOWWRITES_ROM0 + i, RomBankType[i] == BankType::Ram && RomWritable[i]);
	}
}

/****************************************************************************/

void BeebWin::SetJoystickOption(JoystickOption Option)
{
	// Disable current selection.
	if (m_JoystickOption == JoystickOption::Joystick)
	{
		ResetJoystick();
	}
	else if (m_JoystickOption == JoystickOption::AnalogueMouseStick ||
	         m_JoystickOption == JoystickOption::DigitalMouseStick)
	{
		AtoDDisable();
	}

	// Initialise new selection.
	m_JoystickOption = Option;

	if (m_JoystickOption == JoystickOption::Joystick)
	{
		InitJoystick();
	}
	else if (m_JoystickOption == JoystickOption::AnalogueMouseStick ||
	         m_JoystickOption == JoystickOption::DigitalMouseStick)
	{
		AtoDEnable();
	}

	if (!JoystickEnabled)
	{
		m_JoystickOption = JoystickOption::Disabled;
	}

	UpdateJoystickMenu();
}

void BeebWin::UpdateJoystickMenu()
{
	static const struct { UINT ID; JoystickOption Joystick; } MenuItems[] =
	{
		{ IDM_JOYSTICK_DISABLED,   JoystickOption::Disabled },
		{ IDM_JOYSTICK,            JoystickOption::Joystick },
		{ IDM_ANALOGUE_MOUSESTICK, JoystickOption::AnalogueMouseStick },
		{ IDM_DIGITAL_MOUSESTICK,  JoystickOption::DigitalMouseStick }
	};

	UINT SelectedMenuItemID = 0;

	for (int i = 0; i < _countof(MenuItems); i++)
	{
		if (m_JoystickOption == MenuItems[i].Joystick)
		{
			SelectedMenuItemID = MenuItems[i].ID;
			break;
		}
	}

	CheckMenuRadioItem(IDM_JOYSTICK_DISABLED, IDM_DIGITAL_MOUSESTICK, SelectedMenuItemID);
}

/****************************************************************************/

void BeebWin::InitJoystick()
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
		Report(MessageType::Error, "Joystick is not plugged in");
	}
	else
	{
		Report(MessageType::Error, "Failed to initialise the joystick");
	}
}

/****************************************************************************/
void BeebWin::ScaleJoystick(unsigned int x, unsigned int y)
{
	if (m_JoystickOption == JoystickOption::Joystick)
	{
		/* Scale and reverse the readings */
		JoystickX = (int)((double)(m_JoystickCaps.wXmax - x) * 65535.0 /
		                  (double)(m_JoystickCaps.wXmax - m_JoystickCaps.wXmin));
		JoystickY = (int)((double)(m_JoystickCaps.wYmax - y) * 65535.0 /
		                  (double)(m_JoystickCaps.wYmax - m_JoystickCaps.wYmin));
	}
}

/****************************************************************************/

void BeebWin::ResetJoystick()
{
	// joySetCapture() fails after a joyReleaseCapture() call (not sure why)
	// so leave joystick captured.
	// joyReleaseCapture(JOYSTICKID1);
	AtoDDisable();
}

/****************************************************************************/
void BeebWin::SetMousestickButton(int index, bool button)
{
	if (m_JoystickOption == JoystickOption::AnalogueMouseStick ||
	    m_JoystickOption == JoystickOption::DigitalMouseStick)
	{
		JoystickButton[index] = button;
	}
}

/****************************************************************************/
void BeebWin::ScaleMousestick(unsigned int x, unsigned int y)
{
	static int lastx = 32768;
	static int lasty = 32768;

	if (m_JoystickOption == JoystickOption::AnalogueMouseStick)
	{
		JoystickX = (m_XWinSize - x) * 65535 / m_XWinSize;
		JoystickY = (m_YWinSize - y) * 65535 / m_YWinSize;
	}
	else if (m_JoystickOption == JoystickOption::DigitalMouseStick)
	{
		int dx = x - lastx;
		int dy = y - lasty;

		if (dx > 4) JoystickX = 0;
		if (dx < -4) JoystickX = 65535;

		if (dy > 4) JoystickY = 0;
		if (dy < -4) JoystickY = 65535;

		lastx = x;
		lasty = y;
	}
}

/****************************************************************************/
void BeebWin::SetAMXPosition(unsigned int x, unsigned int y)
{
	if (AMXMouseEnabled && (MachineType != Model::MasterET))
	{
		// Scale the window coords to the beeb screen coords
		AMXTargetX = x * m_AMXXSize * (100 + m_AMXAdjust) / 100 / m_XWinSize;
		AMXTargetY = y * m_AMXYSize * (100 + m_AMXAdjust) / 100 / m_YWinSize;

		AMXMouseMovement();
	}
}

/****************************************************************************/
void BeebWin::ChangeAMXPosition(int deltaX, int deltaY)
{
	if (AMXMouseEnabled && (MachineType != Model::MasterET))
	{
		static int remX = 0;
		static int remY = 0;

		// Scale the window coords to the beeb screen coords
		int bigX = deltaX * m_AMXXSize * (100 + m_AMXAdjust);
		AMXDeltaX += (bigX + remX) / (100 * m_XWinSize);
		remX = (bigX + remX) % (100 * m_XWinSize);

		int bigY = deltaY * m_AMXYSize * (100 + m_AMXAdjust);
		AMXDeltaY += (bigY + remY) / (100 * m_YWinSize);
		remY = (bigY + remY) % (100 * m_YWinSize);

		AMXMouseMovement();
	}
}

/****************************************************************************/

LRESULT CALLBACK BeebWin::WndProc(HWND hWnd,
                                  UINT nMessage,
                                  WPARAM wParam,
                                  LPARAM lParam)
{
	BeebWin* pBeebWin = nullptr;

	if (nMessage == WM_NCCREATE)
	{
		pBeebWin = (BeebWin*)((CREATESTRUCT*)lParam)->lpCreateParams;

		SetWindowLongPtr(hWnd, GWLP_USERDATA, (LONG_PTR)pBeebWin);

		return DefWindowProc(hWnd, nMessage, wParam, lParam);
	}
	else
	{
		pBeebWin = (BeebWin*)GetWindowLongPtr(hWnd, GWLP_USERDATA);
	}

	if (pBeebWin != nullptr)
	{
		return pBeebWin->WndProc(nMessage, wParam, lParam);
	}
	else
	{
		return DefWindowProc(hWnd, nMessage, wParam, lParam);
	}
}

LRESULT BeebWin::WndProc(UINT nMessage, WPARAM wParam, LPARAM lParam)
{
	switch (nMessage)
	{
		case WM_COMMAND: // message: command from application menu
			HandleCommand(LOWORD(wParam));
			break;

		case WM_PAINT: {
			PAINTSTRUCT ps;
			HDC hDC = BeginPaint(m_hWnd, &ps);
			updateLines(hDC, 0, 0);
			EndPaint(m_hWnd, &ps);

			if (m_DXResetPending)
			{
				ResetDX();
			}
			break;
		}

		case WM_SIZE:
			OnSize(wParam, LOWORD(lParam), HIWORD(lParam));
			break;

		case WM_SYSKEYDOWN: {
			// DebugTrace("SysKeyD: %d, 0x%X, 0x%X\n", wParam, wParam, lParam);

			bool AltPressed = (lParam & 0x20000000) != 0; // Check the context code

			if (m_TextToSpeechEnabled &&
			    ((wParam >= VK_NUMPAD0 && wParam <= VK_NUMPAD9) ||
			     wParam == VK_DECIMAL ||
			     wParam == VK_HOME || wParam == VK_END ||
			     wParam == VK_PRIOR || wParam == VK_NEXT ||
			     wParam == VK_UP || wParam == VK_DOWN ||
			     wParam == VK_LEFT || wParam == VK_RIGHT ||
			     wParam == VK_INSERT || wParam == VK_DELETE ||
			     wParam == VK_CLEAR))
			{
				TextToSpeechKey(wParam);
			}
			else if (wParam == VK_OEM_8) // Alt `
			{
				TextViewSyncWithBeebCursor();
			}
			else if (wParam == VK_RETURN && AltPressed) // Alt+Enter
			{
				HandleCommand(IDM_FULLSCREEN);
				break;
			}
			else if (wParam == VK_F4 && AltPressed) // Alt+F4
			{
				HandleCommand(IDM_EXIT);
				break;
			}
			else if (wParam == VK_F5 && AltPressed) // Alt+F5
			{
				HandleCommand(IDM_EMUPAUSED);
				break;
			}

			if (wParam != VK_F10 && wParam != VK_CONTROL)
			{
				break;
			}

			// Fall through...
		}

		case WM_KEYDOWN:
			// DebugTrace("KeyD: %d, 0x%X, 0x%X\n", wParam, wParam, lParam);

			if (m_TextToSpeechEnabled &&
			    ((wParam >= VK_NUMPAD0 && wParam <= VK_NUMPAD9) ||
			     wParam == VK_DECIMAL ||
			     wParam == VK_HOME || wParam == VK_END ||
			     wParam == VK_PRIOR || wParam == VK_NEXT ||
			     wParam == VK_UP || wParam == VK_DOWN ||
			     wParam == VK_LEFT || wParam == VK_RIGHT ||
			     wParam == VK_INSERT || wParam == VK_DELETE ||
			     wParam == VK_CLEAR))
			{
				TextToSpeechKey(wParam);
			}
			else if ((wParam == VK_DIVIDE || wParam == VK_MULTIPLY ||
			          wParam == VK_ADD || wParam == VK_SUBTRACT) &&
			         !m_DisableKeysShortcut)
			{
				// Ignore shortcut key, handled on key up
			}
			else if (wParam == VK_MENU && (GetKeyState(VK_CONTROL) & 0x8000) ||
			         wParam == VK_CONTROL && (GetKeyState(VK_MENU) & 0x8000))
			{
				ReleaseMouse();
			}
			else
			{
				bool bit = false;

				if (userPortBreakoutDialog != nullptr)
				{
					bit = userPortBreakoutDialog->KeyDown((int)wParam);
				}

				if (!bit)
				{
					// Reset shift state if it was set by Run Disc
					if (m_ShiftBooted)
					{
						m_ShiftBooted = false;
						BeebKeyUp(0, 0);
					}

					int row, col;

					TranslateKey((int)wParam, false, row, col);
				}
			}
			break;

		case WM_SYSKEYUP: {
			// Debug("SysKeyU: %d, 0x%X, 0x%X\n", uParam, uParam, lParam);

			bool AltPressed = (lParam & 0x20000000) != 0; // Check the context code

			if ((wParam == 0x35 || wParam == VK_NUMPAD5) && AltPressed)
			{
				CaptureBitmapPending(true);
				break;
			}
			else if (wParam == 0x31 && AltPressed && !m_DisableKeysShortcut)
			{
				QuickSave();
				// Let user know state has been saved
				FlashWindow();
				break;
			}
			else if (wParam == 0x32 && AltPressed && !m_DisableKeysShortcut)
			{
				QuickLoad();
				// Let user know state has been loaded
				FlashWindow();
				break;
			}
			else if (wParam == VK_OEM_PLUS && AltPressed)
			{
				AdjustSpeed(true);
				break;
			}
			else if (wParam == VK_OEM_MINUS && AltPressed)
			{
				AdjustSpeed(false);
				break;
			}
			else if (wParam != VK_F10 && wParam != VK_CONTROL)
			{
				break;
			}

			// Fall through...
		}

		case WM_KEYUP:
			// DebugTrace("KeyU: %d, 0x%X, 0x%X\n", uParam, uParam, lParam);

			if (wParam == VK_DIVIDE && !m_DisableKeysShortcut)
			{
				QuickSave();
				// Let user know state has been saved
				FlashWindow();
			}
			else if (wParam == VK_MULTIPLY && !m_DisableKeysShortcut)
			{
				QuickLoad();
				// Let user know state has been loaded
				FlashWindow();
			}
			else if (wParam == VK_ADD && !m_DisableKeysShortcut)
			{
				AdjustSpeed(true);
			}
			else if (wParam == VK_SUBTRACT && !m_DisableKeysShortcut)
			{
				AdjustSpeed(false);
			}
			else
			{
				bool bit = false;

				if (userPortBreakoutDialog != nullptr)
				{
					bit = userPortBreakoutDialog->KeyUp(static_cast<int>(wParam));
				}

				if (!bit)
				{
					int row, col;

					if (TranslateKey((int)wParam, true, row, col) < 0)
					{
						if (row == -2)
						{
							Break();
						}
						else if (row == -3)
						{
							if (col == -3) SoundTuning += 0.1; // Page Up
							if (col == -4) SoundTuning -= 0.1; // Page Down
						}
					}
				}
			}
			break;

		case WM_ACTIVATE:
			Activate(wParam != WA_INACTIVE);
			break;

		case WM_SETFOCUS:
			Focus(true);
			break;

		case WM_KILLFOCUS:
			Focus(false);
			break;

		case WM_SETCURSOR:
			if (m_HideCursor && LOWORD(lParam) == HTCLIENT)
			{
				SetCursor(nullptr);
				return TRUE;
			}
			else
			{
				return DefWindowProc(m_hWnd, nMessage, wParam, lParam);
			}
			break;

		case MM_JOY1MOVE:
			ScaleJoystick(LOWORD(lParam), HIWORD(lParam));
			break;

		case MM_JOY1BUTTONDOWN:
		case MM_JOY1BUTTONUP:
			JoystickButton[0] = (wParam & (JOY_BUTTON1 | JOY_BUTTON2)) != 0;
			break;

		case WM_INPUT:
			if (m_MouseCaptured)
			{
				UINT dwSize = sizeof(RAWINPUT);
				BYTE Buffer[sizeof(RAWINPUT)];

				GetRawInputData((HRAWINPUT)lParam,
				                RID_INPUT,
				                Buffer,
				                &dwSize,
				                sizeof(RAWINPUTHEADER));

				RAWINPUT* raw = reinterpret_cast<RAWINPUT*>(Buffer);

				if (raw->header.dwType != RIM_TYPEMOUSE)
					break;

				int xDelta = raw->data.mouse.lLastX;
				int yDelta = raw->data.mouse.lLastY;

				m_RelMousePos.x += xDelta;

				if (m_RelMousePos.x < 0)
				{
					m_RelMousePos.x = 0;
				}
				else if (m_RelMousePos.x > m_XWinSize)
				{
					m_RelMousePos.x = m_XWinSize;
				}

				m_RelMousePos.y += yDelta;

				if (m_RelMousePos.y < 0)
				{
					m_RelMousePos.y = 0;
				}
				else if (m_RelMousePos.y > m_YWinSize)
				{
					m_RelMousePos.y = m_YWinSize;
				}

				ScaleMousestick(m_RelMousePos.x,
				                m_RelMousePos.y);

				ChangeAMXPosition(xDelta, yDelta);
			}
			break;

		case WM_MOUSEMOVE:
			if (!m_MouseCaptured)
			{
				int xPos = GET_X_LPARAM(lParam);
				int yPos = GET_Y_LPARAM(lParam);

				ScaleMousestick(xPos, yPos);
				SetAMXPosition(xPos, yPos);

				// Experiment: show menu in full screen when cursor moved to top of window
				HideMenu(yPos > 2);
			}
			break;

		case WM_LBUTTONDOWN:
			if (AMXMouseEnabled && m_CaptureMouse && !m_MouseCaptured)
			{
				CaptureMouse();
			}
			else
			{
				SetMousestickButton(0, (wParam & MK_LBUTTON) != 0);
				AMXButtons |= AMX_LEFT_BUTTON;
			}
			break;

		case WM_LBUTTONUP:
			SetMousestickButton(0, (wParam & MK_LBUTTON) != 0);
			AMXButtons &= ~AMX_LEFT_BUTTON;
			break;

		case WM_MBUTTONDOWN:
			AMXButtons |= AMX_MIDDLE_BUTTON;
			break;

		case WM_MBUTTONUP:
			AMXButtons &= ~AMX_MIDDLE_BUTTON;
			break;

		case WM_RBUTTONDOWN:
			SetMousestickButton(1, (wParam & MK_RBUTTON) != 0);
			AMXButtons |= AMX_RIGHT_BUTTON;
			break;

		case WM_RBUTTONUP:
			SetMousestickButton(1, (wParam & MK_RBUTTON) != 0);
			AMXButtons &= ~AMX_RIGHT_BUTTON;
			break;

		case WM_DESTROY:  // message: window being destroyed
			Shutdown();
			PostQuitMessage(0);
			break;

		case WM_ENTERMENULOOP: // entering menu, must mute directsound
			SetSound(SoundState::Muted);
			break;

		case WM_EXITMENULOOP:
			SetSound(SoundState::Unmuted);
			ResetTiming();
			break;

		case WM_ENTERSIZEMOVE: // Window being moved
			SetSound(SoundState::Muted);
			break;

		case WM_EXITSIZEMOVE:
			SetSound(SoundState::Unmuted);
			break;

		case WM_REINITDX:
			DebugTrace("WM_REINITDX\n");
			ReinitDX();
			break;

		case WM_TIMER:
			if (wParam == TIMER_KEYBOARD)
			{
				HandleTimer();
			}
			else if (wParam == TIMER_AUTOBOOT_DELAY) // Handle timer for automatic disc boot delay
			{
				KillBootDiscTimer();
				DoShiftBreak();
			}
			break;

		case WM_USER_KEYBOARD_DIALOG_CLOSED:
			UserKeyboardDialogClosed();
			break;

		case WM_USER_PORT_BREAKOUT_DIALOG_CLOSED:
			delete userPortBreakoutDialog;
			userPortBreakoutDialog = nullptr;
			break;

		case WM_DIRECTX9_DEVICE_LOST:
			OnDeviceLost();
			break;

		default: // Passes it on if unproccessed
			return DefWindowProc(m_hWnd, nMessage, wParam, lParam);
	}

	return 0;
}

/****************************************************************************/
int BeebWin::TranslateKey(int vkey, bool keyUp, int &row, int &col)
{
	if (vkey < 0 || vkey > 255)
		return -9;

	// Key track of shift state
	if ((*transTable)[vkey][0].row == 0 && (*transTable)[vkey][0].col == 0)
	{
		m_ShiftPressed = !keyUp;
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
		row = (*transTable)[vkey][static_cast<int>(m_ShiftPressed)].row;
		col = (*transTable)[vkey][static_cast<int>(m_ShiftPressed)].col;
		bool needShift = (*transTable)[vkey][static_cast<int>(m_ShiftPressed)].shift;

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
			m_vkeyPressed[vkey][0][static_cast<int>(m_ShiftPressed)] = row;
			m_vkeyPressed[vkey][1][static_cast<int>(m_ShiftPressed)] = col;
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
	if (aviWriter != nullptr)
	{
		if (++m_AviFrameSkipCount > m_AviFrameSkip)
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

		if (++m_AviFrameCount >= 50)
		{
			m_AviFrameCount = 0;
			m_AviFrameSkipCount = 0;
		}
	}

	return FrameNum;
}

void BeebWin::doLED(int sx,bool on) {
	int tsy;
	int colbase = static_cast<int>(m_DiscLedColour) * 2 + LED_COL_BASE; // colour will be 0 for red, 1 for green.
	if (sx<100) colbase=LED_COL_BASE; // Red leds for keyboard always
	if (TeletextEnabled)
		tsy=496;
	else
		tsy=m_LastStartY+m_LastNLines-2;
	doUHorizLine((on ? 1 : 0) + colbase, tsy, sx, 8);
	doUHorizLine((on ? 1 : 0) + colbase, tsy, sx, 8);
}

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
bool BeebWin::UpdateTiming()
{
	bool UpdateScreen = false;

	DWORD TickCount = GetTickCount();

	// Don't do anything if this is the first call or there has
	// been a long pause due to menu commands, or when something
	// wraps
	if (m_LastTickCount == 0 ||
	    TickCount < m_LastTickCount ||
	    (TickCount - m_LastTickCount) > 1000 ||
	    TotalCycles < m_LastTotalCycles)
	{
		ResetTiming();
		return true;
	}

	// Update stats every second
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
		DWORD Ticks = TickCount - m_TickBase;
		int nCycles = (int)((double)(TotalCycles - m_CycleBase) / m_RealTimeTarget);

		if (Ticks <= (DWORD)(nCycles / 2000))
		{
			// Need to slow down, show frame (max 50fps though)
			// and sleep a bit
			if (TickCount >= m_LastFPSCount + 20)
			{
				UpdateScreen = true;
				m_LastFPSCount += 20;
			}
			else
			{
				UpdateScreen = false;
			}

			DWORD SpareTicks = (DWORD)(nCycles / 2000) - Ticks;
			Sleep(SpareTicks);
			m_MinFrameCount = 0;
		}
		else
		{
			// Need to speed up, skip a frame
			UpdateScreen = false;

			// Make sure we show at least one in 100 frames
			++m_MinFrameCount;

			if (m_MinFrameCount >= 100)
			{
				UpdateScreen = true;
				m_MinFrameCount = 0;
			}
		}

		// Move counter bases forward

		while ((TickCount - m_TickBase) > 1000 && (TotalCycles - m_CycleBase) > m_CyclesPerSec)
		{
			m_TickBase += 1000;
			m_CycleBase += m_CyclesPerSec;
		}
	}
	else
	{
		// Fast as possible with a certain frame rate
		if (TickCount >= m_LastFPSCount + (1000 / m_FPSTarget))
		{
			UpdateScreen = true;
			m_LastFPSCount += 1000 / m_FPSTarget;
		}
		else
		{
			UpdateScreen = false;
		}
	}

	m_LastTickCount = TickCount;
	m_LastTotalCycles = TotalCycles;

	return UpdateScreen;
}

/****************************************************************************/

void BeebWin::SetDirectXFullScreenMode(DirectXFullScreenMode Mode)
{
	// Ignore IDM_VIEW_DD_SCREENRES if already in full screen mode
	if ((Mode != DirectXFullScreenMode::ScreenResolution) || !m_FullScreen)
	{
		m_DDFullScreenMode = Mode;

		TranslateDDSize();

		if (m_FullScreen && m_DisplayRenderer != DisplayRendererType::GDI)
		{
			SetWindowAttributes(m_FullScreen);
		}

		UpdateDirectXFullScreenModeMenu();
	}
}

void BeebWin::TranslateDDSize()
{
	switch (m_DDFullScreenMode)
	{
		case DirectXFullScreenMode::_640x480:
			m_XDXSize = 640;
			m_YDXSize = 480;
			break;

		case DirectXFullScreenMode::_720x576:
			m_XDXSize = 720;
			m_YDXSize = 576;
			break;

		case DirectXFullScreenMode::_800x600:
			m_XDXSize = 800;
			m_YDXSize = 600;
			break;

		case DirectXFullScreenMode::_1024x768:
			m_XDXSize = 1024;
			m_YDXSize = 768;
			break;

		case DirectXFullScreenMode::_1280x720:
			m_XDXSize = 1280;
			m_YDXSize = 720;
			break;

		case DirectXFullScreenMode::_1280x768:
			m_XDXSize = 1280;
			m_YDXSize = 768;
			break;

		case DirectXFullScreenMode::_1280x960:
			m_XDXSize = 1280;
			m_YDXSize = 960;
			break;

		case DirectXFullScreenMode::_1280x1024:
			m_XDXSize = 1280;
			m_YDXSize = 1024;
			break;

		case DirectXFullScreenMode::_1440x900:
			m_XDXSize = 1440;
			m_YDXSize = 900;
			break;

		case DirectXFullScreenMode::_1600x1200:
			m_XDXSize = 1600;
			m_YDXSize = 1200;
			break;

		case DirectXFullScreenMode::_1920x1080:
			m_XDXSize = 1920;
			m_YDXSize = 1080;
			break;

		case DirectXFullScreenMode::_2560x1440:
			m_XDXSize = 2560;
			m_YDXSize = 1440;
			break;

		case DirectXFullScreenMode::_3840x2160:
			m_XDXSize = 3840;
			m_YDXSize = 2160;
			break;

		case DirectXFullScreenMode::ScreenResolution:
		default:
			// Pixel size of default monitor
			m_XDXSize = GetSystemMetrics(SM_CXSCREEN);
			m_YDXSize = GetSystemMetrics(SM_CYSCREEN);
			break;
	}
}

void BeebWin::UpdateDirectXFullScreenModeMenu()
{
	static const struct { UINT ID; DirectXFullScreenMode Mode; } MenuItems[] =
	{
		{ IDM_VIEW_DD_SCREENRES, DirectXFullScreenMode::ScreenResolution },
		{ IDM_VIEW_DD_640X480,   DirectXFullScreenMode::_640x480 },
		{ IDM_VIEW_DD_720X576,   DirectXFullScreenMode::_720x576 },
		{ IDM_VIEW_DD_800X600,   DirectXFullScreenMode::_800x600 },
		{ IDM_VIEW_DD_1024X768,  DirectXFullScreenMode::_1024x768 },
		{ IDM_VIEW_DD_1280X720,  DirectXFullScreenMode::_1280x720 },
		{ IDM_VIEW_DD_1280X768,  DirectXFullScreenMode::_1280x768 },
		{ IDM_VIEW_DD_1280X960,  DirectXFullScreenMode::_1280x960 },
		{ IDM_VIEW_DD_1280X1024, DirectXFullScreenMode::_1280x1024 },
		{ IDM_VIEW_DD_1440X900,  DirectXFullScreenMode::_1440x900 },
		{ IDM_VIEW_DD_1600X1200, DirectXFullScreenMode::_1600x1200 },
		{ IDM_VIEW_DD_1920X1080, DirectXFullScreenMode::_1920x1080 },
		{ IDM_VIEW_DD_2560X1440, DirectXFullScreenMode::_2560x1440 },
		{ IDM_VIEW_DD_3840X2160, DirectXFullScreenMode::_3840x2160 }
	};

	UINT SelectedMenuItemID = 0;

	for (int i = 0; i < _countof(MenuItems); i++)
	{
		if (m_DDFullScreenMode == MenuItems[i].Mode)
		{
			SelectedMenuItemID = MenuItems[i].ID;
			break;
		}
	}

	CheckMenuRadioItem(
		IDM_VIEW_DD_SCREENRES,
		IDM_VIEW_DD_3840X2160,
		SelectedMenuItemID
	);
}

void BeebWin::ToggleFullScreen()
{
	m_FullScreen = !m_FullScreen;
	CheckMenuItem(IDM_FULLSCREEN, m_FullScreen);
	SetWindowAttributes(!m_FullScreen);

	if (m_MouseCaptured)
	{
		ReleaseMouse();
	}
}

/****************************************************************************/

void BeebWin::SetWindowSize(int Width, int Height)
{
	if (m_FullScreen)
	{
		HandleCommand(IDM_FULLSCREEN);
	}

	m_XWinSize = Width;
	m_YWinSize = Height;

	m_XLastWinSize = m_XWinSize;
	m_YLastWinSize = m_YWinSize;

	UpdateWindowSizeMenu();

	SetWindowAttributes(m_FullScreen);
}

void BeebWin::UpdateWindowSizeMenu()
{
	static const struct { UINT ID; int Width; int Height; } MenuItems[] =
	{
		{ IDM_320X256,   320, 256 },
		{ IDM_640X512,   640, 512 },
		{ IDM_800X600,   800, 600 },
		{ IDM_1024X768,  1024, 768 },
		{ IDM_1024X512,  1024, 512 },
		{ IDM_1280X1024, 1280, 1024 },
		{ IDM_1440X1080, 1440, 1080 },
		{ IDM_1600X1200, 1600, 1200 }
	};

	int SelectedMenuItemID = 0;

	for (int i = 0; i < _countof(MenuItems); i++)
	{
		if (m_XWinSize == MenuItems[i].Width &&
		    m_YWinSize == MenuItems[i].Height)
		{
			SelectedMenuItemID = MenuItems[i].ID;
			break;
		}
	}

	if (SelectedMenuItemID != 0)
	{
		CheckMenuRadioItem(IDM_320X256, IDM_1600X1200, SelectedMenuItemID);
	}
}

/****************************************************************************/

void BeebWin::UpdateSpeedMenu()
{
	CheckMenuItem(IDM_REALTIME,       m_TimingType == TimingType::FixedSpeed && m_TimingSpeed == 100);
	CheckMenuItem(IDM_50FPS,          m_TimingType == TimingType::FixedFPS && m_TimingSpeed == 50);
	CheckMenuItem(IDM_25FPS,          m_TimingType == TimingType::FixedFPS && m_TimingSpeed == 25);
	CheckMenuItem(IDM_10FPS,          m_TimingType == TimingType::FixedFPS && m_TimingSpeed == 10);
	CheckMenuItem(IDM_5FPS,           m_TimingType == TimingType::FixedFPS && m_TimingSpeed == 5);
	CheckMenuItem(IDM_1FPS,           m_TimingType == TimingType::FixedFPS && m_TimingSpeed == 1);
	CheckMenuItem(IDM_FIXEDSPEED100,  m_TimingType == TimingType::FixedSpeed && m_TimingSpeed == 10000);
	CheckMenuItem(IDM_FIXEDSPEED50,   m_TimingType == TimingType::FixedSpeed && m_TimingSpeed == 5000);
	CheckMenuItem(IDM_FIXEDSPEED10,   m_TimingType == TimingType::FixedSpeed && m_TimingSpeed == 1000);
	CheckMenuItem(IDM_FIXEDSPEED5,    m_TimingType == TimingType::FixedSpeed && m_TimingSpeed == 500);
	CheckMenuItem(IDM_FIXEDSPEED2,    m_TimingType == TimingType::FixedSpeed && m_TimingSpeed == 200);
	CheckMenuItem(IDM_FIXEDSPEED1_5,  m_TimingType == TimingType::FixedSpeed && m_TimingSpeed == 150);
	CheckMenuItem(IDM_FIXEDSPEED1_25, m_TimingType == TimingType::FixedSpeed && m_TimingSpeed == 125);
	CheckMenuItem(IDM_FIXEDSPEED1_1,  m_TimingType == TimingType::FixedSpeed && m_TimingSpeed == 110);
	CheckMenuItem(IDM_FIXEDSPEED0_9,  m_TimingType == TimingType::FixedSpeed && m_TimingSpeed == 90);
	CheckMenuItem(IDM_FIXEDSPEED0_75, m_TimingType == TimingType::FixedSpeed && m_TimingSpeed == 75);
	CheckMenuItem(IDM_FIXEDSPEED0_5,  m_TimingType == TimingType::FixedSpeed && m_TimingSpeed == 50);
	CheckMenuItem(IDM_FIXEDSPEED0_25, m_TimingType == TimingType::FixedSpeed && m_TimingSpeed == 25);
	CheckMenuItem(IDM_FIXEDSPEED0_1,  m_TimingType == TimingType::FixedSpeed && m_TimingSpeed == 10);
}

void BeebWin::TranslateTiming()
{
	switch (m_TimingType)
	{
		case TimingType::FixedSpeed:
			SetRealTimeTarget((double)m_TimingSpeed / 100.0);
			m_FPSTarget = 0;
			break;

		case TimingType::FixedFPS:
			SetRealTimeTarget(0.0);
			m_FPSTarget = m_TimingSpeed;
			break;

		default:
			// Real time
			SetRealTimeTarget(1.0);
			m_FPSTarget = 0;
			break;
	}

	ResetTiming();

	UpdateSpeedMenu();
}

void BeebWin::SetRealTimeTarget(double RealTimeTarget)
{
	m_RealTimeTarget = RealTimeTarget;
	m_CyclesPerSec = (int)(2000000.0 * m_RealTimeTarget);
}

void BeebWin::AdjustSpeed(bool Up)
{
	static const std::array<int, 14> Speeds = {
		10000,
		5000,
		1000,
		500,
		200,
		150,
		125,
		110,
		100,
		90,
		75,
		50,
		25,
		10
	};

	int Index = 0;

	if (m_TimingType == TimingType::FixedFPS)
	{
		Index = 8;
		m_TimingType = TimingType::FixedSpeed;
	}
	else
	{
		auto it = std::find(Speeds.begin(), Speeds.end(), m_TimingSpeed);

		if (it != Speeds.end())
		{
			if (Up && it != Speeds.begin())
			{
				it--;
			}
			else if (!Up && it != Speeds.begin())
			{
				it++;

				if (it == Speeds.end())
				{
					it--;
				}
			}
		}

		m_TimingSpeed = *it;
	}

	TranslateTiming();
}

/****************************************************************************/

void BeebWin::SetKeyboardMapping(KeyboardMappingType KeyboardMapping)
{
	m_KeyboardMapping = KeyboardMapping;

	TranslateKeyMapping();
	UpdateKeyboardMappingMenu();
}

void BeebWin::UpdateKeyboardMappingMenu()
{
	static const struct { UINT ID; KeyboardMappingType Type; } MenuItems[] =
	{
		{ IDM_USERKYBDMAPPING,    KeyboardMappingType::User },
		{ IDM_DEFAULTKYBDMAPPING, KeyboardMappingType::Default },
		{ IDM_LOGICALKYBDMAPPING, KeyboardMappingType::Logical }
	};

	UINT SelectedMenuItemID = 0;

	for (int i = 0; i < _countof(MenuItems); i++)
	{
		if (m_KeyboardMapping == MenuItems[i].Type)
		{
			SelectedMenuItemID = MenuItems[i].ID;
			break;
		}
	}

	CheckMenuRadioItem(IDM_USERKYBDMAPPING, IDM_LOGICALKYBDMAPPING, SelectedMenuItemID);
}

void BeebWin::TranslateKeyMapping()
{
	switch (m_KeyboardMapping)
	{
		case KeyboardMappingType::Logical:
			transTable = &LogicalKeyMap;
			break;

		case KeyboardMappingType::User:
			transTable = &UserKeyMap;
			break;

		case KeyboardMappingType::Default:
		default:
			transTable = &DefaultKeyMap;
			break;

	}
}

/****************************************************************************/

void BeebWin::SetTapeSpeedMenu()
{
	static const struct { UINT ID; int ClockSpeed; } MenuItems[] =
	{
		{ IDM_TAPE_FAST,   750 },
		{ IDM_TAPE_MFAST,  1600 },
		{ IDM_TAPE_MSLOW,  3200 },
		{ IDM_TAPE_NORMAL, 5600 }
	};

	UINT SelectedMenuItemID = 0;

	for (int i = 0; i < _countof(MenuItems); i++)
	{
		if (TapeState.ClockSpeed == MenuItems[i].ClockSpeed)
		{
			SelectedMenuItemID = MenuItems[i].ID;
			break;
		}
	}

	CheckMenuRadioItem(IDM_TAPE_FAST, IDM_TAPE_NORMAL, SelectedMenuItemID);
}

/****************************************************************************/

void BeebWin::SetUnlockTape(bool Unlock)
{
	TapeState.Unlock = Unlock;
	::SetUnlockTape(TapeState.Unlock);

	CheckMenuItem(IDM_UNLOCKTAPE, TapeState.Unlock);

	if (TapeControlEnabled)
	{
		TapeControlSetUnlock(Unlock);
	}
}

/****************************************************************************/

constexpr int ASPECT_RATIO_X = 5;
constexpr int ASPECT_RATIO_Y = 4;

void BeebWin::CalcAspectRatioAdjustment(int DisplayWidth, int DisplayHeight)
{
	m_XRatioAdj = 0.0f;
	m_YRatioAdj = 0.0f;
	m_XRatioCrop = 0.0f;
	m_YRatioCrop = 0.0f;

	if (m_FullScreen)
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
	if (m_FullScreen)
	{
		// Get the monitor that the BeebEm window is on to account for multiple monitors
		if (m_DDFullScreenMode == DirectXFullScreenMode::ScreenResolution)
		{
			HMONITOR monitor = MonitorFromWindow(m_hWnd, MONITOR_DEFAULTTONEAREST);
			MONITORINFO info;
			info.cbSize = sizeof(MONITORINFO);
			GetMonitorInfo(monitor, &info);

			// Get current resolution of the monitor
			m_XDXSize = info.rcMonitor.right - info.rcMonitor.left;
			m_YDXSize = info.rcMonitor.bottom - info.rcMonitor.top;
		}

		if (!wasFullScreen)
		{
			RECT Rect;
			GetWindowRect(m_hWnd, &Rect);

			m_XWinPos = Rect.left;
			m_YWinPos = Rect.top;
		}

		if (m_DisplayRenderer != DisplayRendererType::GDI)
		{
			m_XWinSize = m_XDXSize;
			m_YWinSize = m_YDXSize;
			CalcAspectRatioAdjustment(m_XDXSize, m_YDXSize);

			DWORD dwStyle = GetWindowLong(m_hWnd, GWL_STYLE);
			dwStyle &= ~WS_OVERLAPPEDWINDOW;
			dwStyle |= WS_POPUP;
			SetWindowLong(m_hWnd, GWL_STYLE, dwStyle);

			if (m_DXInit)
			{
				ResetDX();
			}
		}
		else
		{
			CalcAspectRatioAdjustment(m_XWinSize, m_YWinSize);

			DWORD dwStyle = GetWindowLong(m_hWnd, GWL_STYLE);
			dwStyle &= ~WS_POPUP;
			dwStyle |= WS_OVERLAPPEDWINDOW;
			SetWindowLong(m_hWnd, GWL_STYLE, dwStyle);

			ShowWindow(m_hWnd, SW_MAXIMIZE);
		}

		// Experiment: hide menu in full screen
		HideMenu(true);
	}
	else
	{
		CalcAspectRatioAdjustment(0, 0);

		if (wasFullScreen)
			ShowWindow(m_hWnd, SW_RESTORE);

		// Note: Window size gets lost in DDraw mode when DD is reset
		int xs = m_XLastWinSize;
		int ys = m_YLastWinSize;

		if (m_DisplayRenderer != DisplayRendererType::GDI && m_DXInit)
		{
			ResetDX();
		}

		m_XWinSize = xs;
		m_YWinSize = ys;

		DWORD dwStyle = GetWindowLong(m_hWnd, GWL_STYLE);
		dwStyle &= ~WS_POPUP;
		dwStyle |= WS_OVERLAPPEDWINDOW;
		SetWindowLong(m_hWnd, GWL_STYLE, dwStyle);

		RECT Rect{ 0, 0, m_XWinSize, m_YWinSize };
		AdjustWindowRect(&Rect, dwStyle, TRUE);

		SetWindowPos(m_hWnd,
		             HWND_TOP,
		             m_XWinPos,
		             m_YWinPos,
		             Rect.right - Rect.left,
		             Rect.bottom - Rect.top,
		             !wasFullScreen ? SWP_NOMOVE : 0);

		// Experiment: hide menu in full screen
		HideMenu(false);
	}

	// Clear unused areas of screen
	RECT Rect;
	GetClientRect(m_hWnd, &Rect);
	InvalidateRect(m_hWnd, &Rect, TRUE);
}

/*****************************************************************************/

void BeebWin::OnSize(WPARAM ResizeType, int Width, int Height)
{
	if (m_DisplayRenderer == DisplayRendererType::GDI && ResizeType == SIZE_RESTORED && m_FullScreen)
	{
		m_FullScreen = false;
		CheckMenuItem(IDM_FULLSCREEN, false);
	}

	if (!m_FullScreen || m_DisplayRenderer == DisplayRendererType::GDI)
	{
		m_XWinSize = Width;
		m_YWinSize = Height;
		CalcAspectRatioAdjustment(m_XWinSize, m_YWinSize);

		if (ResizeType != SIZE_MINIMIZED && m_DisplayRenderer != DisplayRendererType::GDI && m_DXInit)
		{
			m_DXResetPending = true;
		}
	}

	if (!m_FullScreen)
	{
		m_XLastWinSize = m_XWinSize;
		m_YLastWinSize = m_YWinSize;
	}

	if (m_hTextView != nullptr)
	{
		MoveWindow(m_hTextView, 0, 0, Width, Height, TRUE);
	}
}

/****************************************************************************/

void BeebWin::SetAMXSize(AMXSizeType Size)
{
	m_AMXSize = Size;

	TranslateAMX();
	UpdateAMXSizeMenu();
}

void BeebWin::UpdateAMXSizeMenu()
{
	static const struct { UINT ID; AMXSizeType AMXSize; } MenuItems[] =
	{
		{ IDM_AMX_160X256, AMXSizeType::_160x256 },
		{ IDM_AMX_320X256, AMXSizeType::_320x256 },
		{ IDM_AMX_640X256, AMXSizeType::_640x256 }
	};

	UINT SelectedMenuItemID = 0;

	for (int i = 0; i < _countof(MenuItems); i++)
	{
		if (m_AMXSize == MenuItems[i].AMXSize)
		{
			SelectedMenuItemID = MenuItems[i].ID;
			break;
		}
	}

	CheckMenuRadioItem(IDM_AMX_160X256, IDM_AMX_640X256, SelectedMenuItemID);
}

void BeebWin::TranslateAMX()
{
	switch (m_AMXSize)
	{
		case AMXSizeType::_160x256:
			m_AMXXSize = 160;
			m_AMXYSize = 256;
			break;

		default:
		case AMXSizeType::_320x256:
			m_AMXXSize = 320;
			m_AMXYSize = 256;
			break;

		case AMXSizeType::_640x256:
			m_AMXXSize = 640;
			m_AMXYSize = 256;
			break;
	}
}

void BeebWin::SetAMXAdjust(int Adjust)
{
	if (Adjust == m_AMXAdjust)
	{
		m_AMXAdjust = 0;
	}
	else
	{
		m_AMXAdjust = Adjust;
	}

	UpdateAMXAdjustMenu();
}

void BeebWin::UpdateAMXAdjustMenu()
{
	static const struct { UINT ID; int AMXAdjust; } MenuItems[] =
	{
		{ IDM_AMX_ADJUSTP50, 50 },
		{ IDM_AMX_ADJUSTP30, 30 },
		{ IDM_AMX_ADJUSTP10, 10 },
		{ IDM_AMX_ADJUSTM10, -10 },
		{ IDM_AMX_ADJUSTM10, -10 },
		{ IDM_AMX_ADJUSTM30, -30 },
		{ IDM_AMX_ADJUSTM50, -50 }
	};

	UINT SelectedMenuItemID = 0;

	for (int i = 0; i < _countof(MenuItems); i++)
	{
		if (m_AMXAdjust == MenuItems[i].AMXAdjust)
		{
			SelectedMenuItemID = MenuItems[i].ID;
			break;
		}
	}

	CheckMenuRadioItem(
		IDM_AMX_ADJUSTP50,
		IDM_AMX_ADJUSTM50,
		SelectedMenuItemID
	);
}

void BeebWin::DisableSerial()
{
	/* if (SerialDestination == SerialType::SerialPort)
	{
		SerialClose();
	} */
	if (SerialDestination == SerialType::TouchScreen)
	{
		TouchScreenClose();
	}
	else if (SerialDestination == SerialType::IP232)
	{
		IP232Close();
	}
}

void BeebWin::SelectSerialPort(const char* PortName)
{
	// DisableSerial();
	strcpy(SerialPortName, PortName);
	bSerialStateChanged = true;
	SerialDestination = SerialType::SerialPort;
	UpdateSerialMenu();
}

void BeebWin::UpdateSerialMenu()
{
	CheckMenuItem(IDM_SERIAL, SerialPortEnabled);
}

//Rob
void BeebWin::UpdateEconetMenu()
{
	CheckMenuItem(IDM_ECONET, EconetEnabled);
}

void BeebWin::UpdateLEDMenu()
{
	CheckMenuRadioItem(
		IDM_RED_LEDS,
		IDM_GREEN_LEDS,
		m_DiscLedColour == LEDColour::Red ? IDM_RED_LEDS : IDM_GREEN_LEDS
	);

	CheckMenuItem(IDM_SHOW_KBLEDS, LEDs.ShowKB);
	CheckMenuItem(IDM_SHOW_DISCLEDS, LEDs.ShowDisc);
}

void BeebWin::UpdateOptiMenu()
{
	CheckMenuItem(IDM_BASIC_HARDWARE_ONLY, BasicHardwareOnly);
	CheckMenuItem(IDM_TELETEXTHALFMODE, TeletextHalfMode);
	CheckMenuItem(IDM_PART_SAMPLES, PartSamples);
}

/***************************************************************************/
void BeebWin::HandleCommand(UINT MenuID)
{
	char TmpPath[256];
	PaletteType PrevPaletteType = m_PaletteType;

	SetSound(SoundState::Muted);
	bool StayMuted = false;

	switch (MenuID)
	{
	case IDM_RUNDISC:
		if (ReadDisc(0, true))
		{
			m_ShiftBooted = true;
			ResetBeebSystem(MachineType, false);
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
		ReadDisc(0, false);
		break;

	case IDM_LOADDISC1:
		ReadDisc(1, false);
		break;

	case IDM_LOADTAPE:
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

	case IDM_WRITE_PROTECT_DISC0:
		ToggleWriteProtect(0);
		break;

	case IDM_WRITE_PROTECT_DISC1:
		ToggleWriteProtect(1);
		break;

	case IDM_WRITE_PROTECT_ON_LOAD:
		m_WriteProtectOnLoad = !m_WriteProtectOnLoad;
		CheckMenuItem(IDM_WRITE_PROTECT_ON_LOAD, m_WriteProtectOnLoad);
		break;

	case IDM_EDIT_COPY:
		doCopy();
		break;

	case IDM_EDIT_PASTE:
		doPaste();
		break;

	case IDM_EDIT_CRLF:
		m_TranslateCRLF = !m_TranslateCRLF;
		CheckMenuItem(IDM_EDIT_CRLF, m_TranslateCRLF);
		break;

	case IDM_DISC_EXPORT_0:
	case IDM_DISC_EXPORT_1:
	case IDM_DISC_EXPORT_2:
	case IDM_DISC_EXPORT_3:
		ExportDiscFiles(MenuID);
		break;

	case IDM_DISC_IMPORT_0:
	case IDM_DISC_IMPORT_1:
	case IDM_DISC_IMPORT_2:
	case IDM_DISC_IMPORT_3:
		ImportDiscFiles(MenuID);
		break;

	case IDM_PRINTER_FILE:
		SetPrinterPort(PrinterPortType::File);
		break;

	case IDM_PRINTER_CLIPBOARD:
		SetPrinterPort(PrinterPortType::Clipboard);
		break;

	case IDM_PRINTER_LPT1:
		SetPrinterPort(PrinterPortType::Lpt1);
		break;

	case IDM_PRINTER_LPT2:
		SetPrinterPort(PrinterPortType::Lpt2);
		break;

	case IDM_PRINTER_LPT3:
		SetPrinterPort(PrinterPortType::Lpt3);
		break;

	case IDM_PRINTER_LPT4:
		SetPrinterPort(PrinterPortType::Lpt4);
		break;

	case IDM_PRINTERONOFF:
		TogglePrinter();
		break;

	case IDM_SERIAL:
		if (SerialPortEnabled) // so disabling..
		{
			DisableSerial();
		}

		SerialPortEnabled = !SerialPortEnabled;

		if (SerialPortEnabled && SerialDestination == SerialType::IP232)
		{
			if (!IP232Open())
			{
				bSerialStateChanged = true;
				UpdateSerialMenu();
				SerialPortEnabled = false;
			}
		}

		bSerialStateChanged = true;
		UpdateSerialMenu();
		break;

	case IDM_SELECT_SERIAL_DESTINATION: {
		SerialPortDialog Dialog(hInst,
		                        m_hWnd,
		                        SerialDestination,
		                        SerialPortName,
		                        IP232Address,
		                        IP232Port,
		                        IP232Raw,
		                        IP232Mode);

		if (Dialog.DoModal())
		{
			DisableSerial();

			SerialDestination = Dialog.GetDestination();

			if (SerialDestination == SerialType::SerialPort)
			{
				SelectSerialPort(Dialog.GetSerialPortName().c_str());
			}
			else if (SerialDestination == SerialType::IP232)
			{
				strcpy(IP232Address, Dialog.GetIPAddress().c_str());
				IP232Port = Dialog.GetIPPort();
				IP232Raw = Dialog.GetIP232RawComms();
				IP232Mode = Dialog.GetIP232Handshake();
			}

			if (SerialPortEnabled)
			{
				if (SerialDestination == SerialType::SerialPort)
				{
				}
				else if (SerialDestination == SerialType::TouchScreen)
				{
					// Also switch on analogue mousestick (touch screen uses
					// mousestick position)
					if (m_JoystickOption != JoystickOption::AnalogueMouseStick)
					{
						HandleCommand(IDM_ANALOGUE_MOUSESTICK);
					}

					TouchScreenOpen();
				}
				else if (SerialDestination == SerialType::IP232)
				{
					if (!IP232Open())
					{
						bSerialStateChanged = true;
						SerialPortEnabled = false;
					}
				}
			}

			UpdateSerialMenu();
		}
		break;
	}

	//Rob
	case IDM_ECONET:
		EconetEnabled = !EconetEnabled;
		if (EconetEnabled)
		{
			// Need hard reset for DNFS to detect econet HW
			ResetBeebSystem(MachineType, false);
			EconetStateChanged = true;
		}
		else
		{
			EconetReset();
		}
		UpdateEconetMenu();
		break;

	case IDM_DISPGDI:
		SetDisplayRenderer(DisplayRendererType::GDI);
		break;

	case IDM_DISPDDRAW:
		SetDisplayRenderer(DisplayRendererType::DirectDraw);
		break;

	case IDM_DISPDX9:
		SetDisplayRenderer(DisplayRendererType::DirectX9);
		break;

	case IDM_DXSMOOTHING:
		m_DXSmoothing = !m_DXSmoothing;
		CheckMenuItem(IDM_DXSMOOTHING, m_DXSmoothing);

		if (m_DisplayRenderer != DisplayRendererType::GDI)
		{
			UpdateSmoothing();
		}
		break;

	case IDM_DXSMOOTHMODE7ONLY:
		m_DXSmoothMode7Only = !m_DXSmoothMode7Only;
		CheckMenuItem(IDM_DXSMOOTHMODE7ONLY, m_DXSmoothMode7Only);

		if (m_DisplayRenderer != DisplayRendererType::GDI)
		{
			UpdateSmoothing();
		}
		break;

	case IDM_320X256:
		SetWindowSize(320, 256);
		break;

	case IDM_640X512:
		SetWindowSize(640, 512);
		break;

	case IDM_800X600:
		SetWindowSize(800, 600);
		break;

	case IDM_1024X512:
		SetWindowSize(1024, 512);
		break;

	case IDM_1024X768:
		SetWindowSize(1024, 768);
		break;

	case IDM_1280X1024:
		SetWindowSize(1280, 1024);
		break;

	case IDM_1440X1080:
		SetWindowSize(1440, 1080);
		break;

	case IDM_1600X1200:
		SetWindowSize(1600, 1200);
		break;

	case IDM_VIEW_DD_SCREENRES:
		SetDirectXFullScreenMode(DirectXFullScreenMode::ScreenResolution);
		break;

	case IDM_VIEW_DD_640X480:
		SetDirectXFullScreenMode(DirectXFullScreenMode::_640x480);
		break;

	case IDM_VIEW_DD_720X576:
		SetDirectXFullScreenMode(DirectXFullScreenMode::_720x576);
		break;

	case IDM_VIEW_DD_800X600:
		SetDirectXFullScreenMode(DirectXFullScreenMode::_800x600);
		break;

	case IDM_VIEW_DD_1024X768:
		SetDirectXFullScreenMode(DirectXFullScreenMode::_1024x768);
		break;

	case IDM_VIEW_DD_1280X720:
		SetDirectXFullScreenMode(DirectXFullScreenMode::_1280x720);
		break;

	case IDM_VIEW_DD_1280X768:
		SetDirectXFullScreenMode(DirectXFullScreenMode::_1280x768);
		break;

	case IDM_VIEW_DD_1280X960:
		SetDirectXFullScreenMode(DirectXFullScreenMode::_1280x960);
		break;

	case IDM_VIEW_DD_1280X1024:
		SetDirectXFullScreenMode(DirectXFullScreenMode::_1280x1024);
		break;

	case IDM_VIEW_DD_1440X900:
		SetDirectXFullScreenMode(DirectXFullScreenMode::_1440x900);
		break;

	case IDM_VIEW_DD_1600X1200:
		SetDirectXFullScreenMode(DirectXFullScreenMode::_1600x1200);
		break;

	case IDM_VIEW_DD_1920X1080:
		SetDirectXFullScreenMode(DirectXFullScreenMode::_1920x1080);
		break;

	case IDM_VIEW_DD_2560X1440:
		SetDirectXFullScreenMode(DirectXFullScreenMode::_2560x1440);
		break;

	case IDM_VIEW_DD_3840X2160:
		SetDirectXFullScreenMode(DirectXFullScreenMode::_3840x2160);
		break;

	case IDM_FULLSCREEN:
		ToggleFullScreen();
		break;

	case IDM_MAINTAINASPECTRATIO:
		m_MaintainAspectRatio = !m_MaintainAspectRatio;
		CheckMenuItem(IDM_MAINTAINASPECTRATIO, m_MaintainAspectRatio);
		if (m_FullScreen)
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
			m_ShowSpeedAndFPS = false;
			SetWindowText(m_hWnd, WindowTitle);
		}
		else
		{
			m_ShowSpeedAndFPS = true;
		}

		CheckMenuItem(IDM_SPEEDANDFPS, m_ShowSpeedAndFPS);
		break;

	case IDM_XAUDIO2:
		SetSoundStreamer(SoundStreamerType::XAudio2);
		break;

	case IDM_DIRECTSOUND:
		SetSoundStreamer(SoundStreamerType::DirectSound);
		break;

	case IDM_SOUNDONOFF:
		if (SoundEnabled)
		{
			CheckMenuItem(IDM_SOUNDONOFF, false);
			SoundReset();
			SoundDefault = false;
		}
		else
		{
			SoundInit();
			if (SoundEnabled) {
				CheckMenuItem(IDM_SOUNDONOFF, true);
				SoundDefault = true;
			}
		}
		break;

	case IDM_SOUNDCHIP:
		SoundChipEnabled = !SoundChipEnabled;
		CheckMenuItem(IDM_SOUNDCHIP, SoundChipEnabled);
		break;

	case IDM_SFX_RELAY:
		RelaySoundEnabled = !RelaySoundEnabled;
		CheckMenuItem(IDM_SFX_RELAY, RelaySoundEnabled);
		break;

	case IDM_SFX_DISCDRIVES:
		DiscDriveSoundEnabled = !DiscDriveSoundEnabled;
		CheckMenuItem(IDM_SFX_DISCDRIVES, DiscDriveSoundEnabled);
		break;

	case IDM_44100KHZ:
		SetSoundSampleRate(44100);
		break;

	case IDM_22050KHZ:
		SetSoundSampleRate(22050);
		break;

	case IDM_11025KHZ:
		SetSoundSampleRate(11025);
		break;

	case IDM_FULLVOLUME:
		SetSoundVolume(100);
		break;

	case IDM_HIGHVOLUME:
		SetSoundVolume(75);
		break;

	case IDM_MEDIUMVOLUME:
		SetSoundVolume(50);
		break;

	case IDM_LOWVOLUME:
		SetSoundVolume(25);
		break;

	case IDM_MUSIC5000:
		Music5000Enabled = !Music5000Enabled;
		Music5000Reset();
		if (Music5000Enabled)
			Music5000Init();
		CheckMenuItem(IDM_MUSIC5000, Music5000Enabled);
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
		int slot = MenuID - IDM_ALLOWWRITES_ROM0;
		RomWritable[slot] = !RomWritable[slot];
		CheckMenuItem(MenuID, RomWritable[slot]);
		break;
	}

	case IDM_SWRAMBOARD:
		SWRAMBoardEnabled = !SWRAMBoardEnabled;
		CheckMenuItem(IDM_SWRAMBOARD, SWRAMBoardEnabled);
		break;

	case IDM_ROMCONFIG:
		EditRomConfig();
		SetRomMenu();
		break;

	case IDM_REALTIME:
		m_TimingType = TimingType::FixedSpeed;
		m_TimingSpeed = 100;
		TranslateTiming();
		break;

	case IDM_FIXEDSPEED100:
		m_TimingType = TimingType::FixedSpeed;
		m_TimingSpeed = 10000;
		TranslateTiming();
		break;

	case IDM_FIXEDSPEED50:
		m_TimingType = TimingType::FixedSpeed;
		m_TimingSpeed = 5000;
		TranslateTiming();
		break;

	case IDM_FIXEDSPEED10:
		m_TimingType = TimingType::FixedSpeed;
		m_TimingSpeed = 1000;
		TranslateTiming();
		break;

	case IDM_FIXEDSPEED5:
		m_TimingType = TimingType::FixedSpeed;
		m_TimingSpeed = 500;
		TranslateTiming();
		break;

	case IDM_FIXEDSPEED2:
		m_TimingType = TimingType::FixedSpeed;
		m_TimingSpeed = 200;
		TranslateTiming();
		break;

	case IDM_FIXEDSPEED1_5:
		m_TimingType = TimingType::FixedSpeed;
		m_TimingSpeed = 150;
		TranslateTiming();
		break;

	case IDM_FIXEDSPEED1_25:
		m_TimingType = TimingType::FixedSpeed;
		m_TimingSpeed = 125;
		TranslateTiming();
		break;

	case IDM_FIXEDSPEED1_1:
		m_TimingType = TimingType::FixedSpeed;
		m_TimingSpeed = 110;
		TranslateTiming();
		break;

	case IDM_FIXEDSPEED0_9:
		m_TimingType = TimingType::FixedSpeed;
		m_TimingSpeed = 90;
		TranslateTiming();
		break;

	case IDM_FIXEDSPEED0_75:
		m_TimingType = TimingType::FixedSpeed;
		m_TimingSpeed = 75;
		TranslateTiming();
		break;

	case IDM_FIXEDSPEED0_5:
		m_TimingType = TimingType::FixedSpeed;
		m_TimingSpeed = 50;
		TranslateTiming();
		break;

	case IDM_FIXEDSPEED0_25:
		m_TimingType = TimingType::FixedSpeed;
		m_TimingSpeed = 25;
		TranslateTiming();
		break;

	case IDM_FIXEDSPEED0_1:
		m_TimingType = TimingType::FixedSpeed;
		m_TimingSpeed = 10;
		TranslateTiming();
		break;

	case IDM_50FPS:
		m_TimingType = TimingType::FixedFPS;
		m_TimingSpeed = 50;
		TranslateTiming();
		break;

	case IDM_25FPS:
		m_TimingType = TimingType::FixedFPS;
		m_TimingSpeed = 25;
		TranslateTiming();
		break;

	case IDM_10FPS:
		m_TimingType = TimingType::FixedFPS;
		m_TimingSpeed = 10;
		TranslateTiming();
		break;

	case IDM_5FPS:
		m_TimingType = TimingType::FixedFPS;
		m_TimingSpeed = 5;
		TranslateTiming();
		break;

	case IDM_1FPS:
		m_TimingType = TimingType::FixedFPS;
		m_TimingSpeed = 1;
		TranslateTiming();
		break;

	case IDM_EMUPAUSED:
		TogglePause();
		break;

	case IDM_JOYSTICK_DISABLED:
		SetJoystickOption(JoystickOption::Disabled);
		break;

	case IDM_JOYSTICK:
		SetJoystickOption(JoystickOption::Joystick);
		break;

	case IDM_ANALOGUE_MOUSESTICK:
		SetJoystickOption(JoystickOption::AnalogueMouseStick);
		break;

	case IDM_DIGITAL_MOUSESTICK:
		SetJoystickOption(JoystickOption::DigitalMouseStick);
		break;

	case IDM_FREEZEINACTIVE:
		SetFreezeWhenInactive(!m_FreezeWhenInactive);
		break;

	case IDM_HIDECURSOR:
		m_HideCursor = !m_HideCursor;
		CheckMenuItem(IDM_HIDECURSOR, m_HideCursor);
		break;

	case IDM_DEFINEKEYMAP:
		OpenUserKeyboardDialog();
		StayMuted = true;
		break;

	case IDM_LOADKEYMAP:
		LoadUserKeyMap();
		break;

	case IDM_SAVEKEYMAP:
		SaveUserKeyMap();
		break;

	case IDM_USERKYBDMAPPING:
		SetKeyboardMapping(KeyboardMappingType::User);
		break;

	case IDM_DEFAULTKYBDMAPPING:
		SetKeyboardMapping(KeyboardMappingType::Default);
		break;

	case IDM_LOGICALKYBDMAPPING:
		SetKeyboardMapping(KeyboardMappingType::Logical);
		break;

	case IDM_MAPAS:
		m_KeyMapAS = !m_KeyMapAS;
		CheckMenuItem(IDM_MAPAS, m_KeyMapAS);
		break;

	case IDM_MAPFUNCS:
		m_KeyMapFunc = !m_KeyMapFunc;
		CheckMenuItem(IDM_MAPFUNCS, m_KeyMapFunc);
		break;

	case IDM_ABOUT: {
		AboutDialog Dialog(hInst, m_hWnd);
		Dialog.DoModal();
		break;
	}

	case IDM_VIEWREADME:
		strcpy(TmpPath, m_AppPath);
		strcat(TmpPath, "Help\\index.html");
		ShellExecute(m_hWnd, NULL, TmpPath, NULL, NULL, SW_SHOWNORMAL);
		break;

	case IDM_EXIT:
		PostMessage(m_hWnd, WM_CLOSE, 0, 0);
		break;

	case IDM_SAVE_PREFS:
		SavePreferences(true);
		break;

	case IDM_AUTOSAVE_PREFS_CMOS:
		m_AutoSavePrefsCMOS = !m_AutoSavePrefsCMOS;
		CheckMenuItem(IDM_AUTOSAVE_PREFS_CMOS, m_AutoSavePrefsCMOS);
		m_AutoSavePrefsChanged = true;
		break;

	case IDM_AUTOSAVE_PREFS_FOLDERS:
		m_AutoSavePrefsFolders = !m_AutoSavePrefsFolders;
		CheckMenuItem(IDM_AUTOSAVE_PREFS_FOLDERS, m_AutoSavePrefsFolders);
		m_AutoSavePrefsChanged = true;
		break;

	case IDM_AUTOSAVE_PREFS_ALL:
		m_AutoSavePrefsAll = !m_AutoSavePrefsAll;
		CheckMenuItem(IDM_AUTOSAVE_PREFS_ALL, m_AutoSavePrefsAll);
		m_AutoSavePrefsChanged = true;
		break;

	case IDM_SELECT_USER_DATA_FOLDER:
		SelectUserDataPath();
		break;

	case IDM_AMXONOFF:
		AMXMouseEnabled = !AMXMouseEnabled;
		CheckMenuItem(IDM_AMXONOFF, AMXMouseEnabled);
		EnableMenuItem(IDM_CAPTUREMOUSE, AMXMouseEnabled);
		break;

	case IDM_CAPTUREMOUSE:
		m_CaptureMouse = !m_CaptureMouse;
		CheckMenuItem(IDM_CAPTUREMOUSE, m_CaptureMouse);
		break;

	case IDM_AMX_LRFORMIDDLE:
		AMXLRForMiddle = !AMXLRForMiddle;
		CheckMenuItem(IDM_AMX_LRFORMIDDLE, AMXLRForMiddle);
		break;

	case IDM_AMX_160X256:
		SetAMXSize(AMXSizeType::_160x256);
		break;

	case IDM_AMX_320X256:
		SetAMXSize(AMXSizeType::_320x256);
		break;

	case IDM_AMX_640X256:
		SetAMXSize(AMXSizeType::_640x256);
		break;

	case IDM_AMX_ADJUSTP50:
		SetAMXAdjust(50);
		break;

	case IDM_AMX_ADJUSTP30:
		SetAMXAdjust(30);
		break;

	case IDM_AMX_ADJUSTP10:
		SetAMXAdjust(10);
		break;

	case IDM_AMX_ADJUSTM10:
		SetAMXAdjust(-10);
		break;

	case IDM_AMX_ADJUSTM30:
		SetAMXAdjust(-30);
		break;

	case IDM_AMX_ADJUSTM50:
		SetAMXAdjust(-50);
		break;

	case IDM_MONITOR_RGB:
		m_PaletteType = PaletteType::RGB;
		CreateBitmap();
		break;

	case IDM_MONITOR_BW:
		m_PaletteType = PaletteType::BW;
		CreateBitmap();
		break;

	case IDM_MONITOR_GREEN:
		m_PaletteType = PaletteType::Green;
		CreateBitmap();
		break;

	case IDM_MONITOR_AMBER:
		m_PaletteType = PaletteType::Amber;
		CreateBitmap();
		break;

	case IDM_TUBE_NONE:
		if (TubeType != Tube::None)
		{
			TubeType = Tube::None;
			UpdateTubeMenu();
			ResetBeebSystem(MachineType, false);
		}
		break;

	case IDM_TUBE_ACORN65C02:
		if (TubeType != Tube::Acorn65C02)
		{
			TubeType = Tube::Acorn65C02;
			UpdateTubeMenu();
			ResetBeebSystem(MachineType, false);
		}
		break;

	case IDM_TUBE_MASTER512:
		if (TubeType != Tube::Master512CoPro)
		{
			TubeType = Tube::Master512CoPro;
			UpdateTubeMenu();
			ResetBeebSystem(MachineType, false);
		}
		break;

	case IDM_TUBE_ACORNZ80:
		if (TubeType != Tube::AcornZ80)
		{
			TubeType = Tube::AcornZ80;
			UpdateTubeMenu();
			ResetBeebSystem(MachineType, false);
		}
		break;

	case IDM_TUBE_TORCHZ80:
		if (TubeType != Tube::TorchZ80)
		{
			TubeType = Tube::TorchZ80;
			UpdateTubeMenu();
			ResetBeebSystem(MachineType, false);
		}
		break;

	case IDM_TUBE_ACORNARM:
		if (TubeType != Tube::AcornArm)
		{
			TubeType = Tube::AcornArm;
			UpdateTubeMenu();
			ResetBeebSystem(MachineType, false);
		}
		break;

	case IDM_TUBE_SPROWARM:
		if (TubeType != Tube::SprowArm)
		{
			TubeType = Tube::SprowArm;
			UpdateTubeMenu();
			ResetBeebSystem(MachineType, false);
		}
		break;

	case IDM_FILE_RESET:
		ResetBeebSystem(MachineType, false);
		break;

	case IDM_MODELB:
		if (MachineType != Model::B)
		{
			ResetBeebSystem(Model::B, true);
			UpdateModelMenu();
		}
		break;

	case IDM_MODELBINT:
		if (MachineType != Model::IntegraB)
		{
			ResetBeebSystem(Model::IntegraB, true);
			UpdateModelMenu();
		}
		break;

	case IDM_MODELBPLUS:
		if (MachineType != Model::BPlus)
		{
			ResetBeebSystem(Model::BPlus, true);
			UpdateModelMenu();
		}
		break;

	case IDM_MASTER128:
		if (MachineType != Model::Master128)
		{
			ResetBeebSystem(Model::Master128, true);
			UpdateModelMenu();
		}
		break;

	case IDM_MASTER_ET:
		if (MachineType != Model::MasterET)
		{
			ResetBeebSystem(Model::MasterET, true);
			UpdateModelMenu();
		}
		break;

	case IDM_REWINDTAPE:
		RewindTape();
		break;

	case IDM_UNLOCKTAPE:
		SetUnlockTape(!TapeState.Unlock);
		break;

	case IDM_HIDEMENU:
		m_HideMenuEnabled = !m_HideMenuEnabled;
		CheckMenuItem(IDM_HIDEMENU, m_HideMenuEnabled);
		break;

	case IDM_RED_LEDS:
		m_DiscLedColour = LEDColour::Red;
		UpdateLEDMenu();
		break;

	case IDM_GREEN_LEDS:
		m_DiscLedColour = LEDColour::Green;
		UpdateLEDMenu();
		break;

	case IDM_SHOW_KBLEDS:
		LEDs.ShowKB = !LEDs.ShowKB;
		UpdateLEDMenu();
		break;

	case IDM_SHOW_DISCLEDS:
		LEDs.ShowDisc = !LEDs.ShowDisc;
		UpdateLEDMenu();
		break;

	case IDM_FDC_DLL:
		if (MachineType != Model::Master128 && MachineType != Model::MasterET)
			SelectFDC();
		break;

	case IDM_8271:
		KillDLLs();
		NativeFDC = true;

		CheckMenuItem(IDM_8271, true);
		CheckMenuItem(IDM_FDC_DLL, false);

		if (MachineType != Model::Master128 && MachineType != Model::MasterET)
		{
			char CfgName[20];
			sprintf(CfgName, "FDCDLL%d", static_cast<int>(MachineType));
			m_Preferences.SetStringValue(CfgName, "None");
		}
		break;

	case IDM_TAPE_FAST:
		SetTapeSpeed(750);
		SetTapeSpeedMenu();
		break;

	case IDM_TAPE_MFAST:
		SetTapeSpeed(1600);
		SetTapeSpeedMenu();
		break;

	case IDM_TAPE_MSLOW:
		SetTapeSpeed(3200);
		SetTapeSpeedMenu();
		break;

	case IDM_TAPE_NORMAL:
		SetTapeSpeed(5600);
		SetTapeSpeedMenu();
		break;

	case IDM_TAPESOUND:
		TapeSoundEnabled = !TapeSoundEnabled;
		CheckMenuItem(IDM_TAPESOUND, TapeSoundEnabled);
		break;

	case IDM_TAPECONTROL:
		if (TapeControlEnabled)
		{
			TapeControlCloseDialog();
		}
		else
		{
			TapeControlOpenDialog(hInst, m_hWnd);
		}
		break;

	case IDM_BREAKOUT:
		if (userPortBreakoutDialog != nullptr)
		{
			userPortBreakoutDialog->Close();
			delete userPortBreakoutDialog;
			userPortBreakoutDialog = nullptr;
		}
		else
		{
			userPortBreakoutDialog = new UserPortBreakoutDialog(hInst, m_hWnd);
			userPortBreakoutDialog->Open();
		}
		break;

	case IDM_USER_PORT_RTC_MODULE:
		UserPortRTCEnabled = !UserPortRTCEnabled;
		CheckMenuItem(IDM_USER_PORT_RTC_MODULE, UserPortRTCEnabled);
		break;

	case IDM_TELETEXTHALFMODE:
		TeletextHalfMode = !TeletextHalfMode;
		UpdateOptiMenu();
		break;

	case IDM_BASIC_HARDWARE_ONLY:
		BasicHardwareOnly = !BasicHardwareOnly;
		UpdateOptiMenu();
		break;

	case IDM_PART_SAMPLES:
		PartSamples = !PartSamples;
		UpdateOptiMenu();
		break;

	case IDM_EXPVOLUME:
		SoundExponentialVolume = !SoundExponentialVolume;
		CheckMenuItem(IDM_EXPVOLUME, SoundExponentialVolume);
		break;

	case IDM_SHOWDEBUGGER:
		if (DebugEnabled)
			DebugCloseDialog();
		else
			OpenDebugWindow();
		break;

	case IDM_BLUR_OFF:
		SetMotionBlur(0);
		break;

	case IDM_BLUR_2:
		SetMotionBlur(2);
		break;

	case IDM_BLUR_4:
		SetMotionBlur(4);
		break;

	case IDM_BLUR_8:
		SetMotionBlur(8);
		break;

	case IDM_CAPTURERES_DISPLAY:
		SetBitmapCaptureResolution(BitmapCaptureResolution::Display);
		break;

	case IDM_CAPTURERES_1280:
		SetBitmapCaptureResolution(BitmapCaptureResolution::_1280x1024);
		break;

	case IDM_CAPTURERES_640:
		SetBitmapCaptureResolution(BitmapCaptureResolution::_640x512);
		break;

	case IDM_CAPTURERES_320:
		SetBitmapCaptureResolution(BitmapCaptureResolution::_320x256);
		break;

	case IDM_CAPTUREBMP:
		SetBitmapCaptureFormat(BitmapCaptureFormat::Bmp);
		break;

	case IDM_CAPTUREJPEG:
		SetBitmapCaptureFormat(BitmapCaptureFormat::Jpeg);
		break;

	case IDM_CAPTUREGIF:
		SetBitmapCaptureFormat(BitmapCaptureFormat::Gif);
		break;

	case IDM_CAPTUREPNG:
		SetBitmapCaptureFormat(BitmapCaptureFormat::Png);
		break;

	case IDM_CAPTURESCREEN:
		// Prompt for file name.  Need to do this in WndProc otherwise
		// dialog does not show in full screen mode.
		if (GetImageFile(m_CaptureFileName, sizeof(m_CaptureFileName)))
		{
			CaptureBitmapPending(false);
		}
		break;

	case IDM_VIDEORES1:
		SetVideoCaptureResolution(VideoCaptureResolution::Display);
		break;

	case IDM_VIDEORES2:
		SetVideoCaptureResolution(VideoCaptureResolution::_640x512);
		break;

	case IDM_VIDEORES3:
		SetVideoCaptureResolution(VideoCaptureResolution::_320x256);
		break;

	case IDM_VIDEOSKIP0:
		SetVideoCaptureFrameSkip(0);
		break;

	case IDM_VIDEOSKIP1:
		SetVideoCaptureFrameSkip(1);
		break;

	case IDM_VIDEOSKIP2:
		SetVideoCaptureFrameSkip(2);
		break;

	case IDM_VIDEOSKIP3:
		SetVideoCaptureFrameSkip(3);
		break;

	case IDM_VIDEOSKIP4:
		SetVideoCaptureFrameSkip(4);
		break;

	case IDM_VIDEOSKIP5:
		SetVideoCaptureFrameSkip(5);
		break;

	case IDM_CAPTUREVIDEO:
		CaptureVideo();
		UpdateVideoCaptureMenu();
		break;

	case IDM_ENDVIDEO:
		EndVideo();
		UpdateVideoCaptureMenu();
		break;

	#if ENABLE_SPEECH

	case IDM_SPEECH:
		if (SpeechDefault)
		{
			CheckMenuItem(IDM_SPEECH, false);
			SpeechStop();
			SpeechDefault = false;
		}
		else
		{
			SpeechInit();
			SpeechStart();

			if (SpeechEnabled)
			{
				CheckMenuItem(IDM_SPEECH, true);
				SpeechDefault = true;
			}
		}
		break;

	#endif

	case IDM_TELETEXT:
		TeletextAdapterEnabled = !TeletextAdapterEnabled;
		TeletextInit();
		CheckMenuItem(IDM_TELETEXT, TeletextAdapterEnabled);
		break;

	case IDM_SELECT_TELETEXT_DATA_SOURCE: {
		std::string DiscsPath;
		m_Preferences.GetStringValue("DiscsPath", DiscsPath);

		TeletextDialog Dialog(hInst,
		                      m_hWnd,
		                      TeletextSource,
		                      DiscsPath,
		                      TeletextFileName,
		                      TeletextIP,
		                      TeletextPort);

		if (Dialog.DoModal())
		{
			TeletextSource = Dialog.GetSource();

			for (int i = 0; i < TELETEXT_CHANNEL_COUNT; i++)
			{
				TeletextFileName[i] = Dialog.GetFileName(i);
				TeletextIP[i] = Dialog.GetIPAddress(i);
				TeletextPort[i] = Dialog.GetPort(i);
			}

			TeletextInit();
		}
		break;
	}

	case IDM_SET_KEYBOARD_LINKS: {
		KeyboardLinksDialog Dialog(hInst, m_hWnd, KeyboardLinks);

		if (Dialog.DoModal())
		{
			KeyboardLinks = Dialog.GetValue();
		}
		break;
	}

	case IDM_SCSI_HARD_DRIVE:
		SCSIDriveEnabled = !SCSIDriveEnabled;
		SCSIReset();
		SASIReset();
		CheckMenuItem(IDM_SCSI_HARD_DRIVE, SCSIDriveEnabled);
		if (SCSIDriveEnabled) {
			IDEDriveEnabled = false;
			CheckMenuItem(IDM_IDE_HARD_DRIVE, IDEDriveEnabled);
		}
		break;

	case IDM_IDE_HARD_DRIVE:
		IDEDriveEnabled = !IDEDriveEnabled;
		IDEReset();
		CheckMenuItem(IDM_IDE_HARD_DRIVE, IDEDriveEnabled);
		if (IDEDriveEnabled) {
			SCSIDriveEnabled = false;
			CheckMenuItem(IDM_SCSI_HARD_DRIVE, SCSIDriveEnabled);
		}
		break;

	case IDM_SELECT_HARD_DRIVE_FOLDER:
		SelectHardDriveFolder();
		break;

	case IDM_FLOPPY_DRIVE:
		Disc8271Enabled = !Disc8271Enabled;
		Disc1770Enabled = !Disc1770Enabled;
		CheckMenuItem(IDM_FLOPPY_DRIVE, Disc8271Enabled);
		break;

	case IDM_TEXTTOSPEECH_ENABLE:
		if (m_TextToSpeechEnabled)
		{
			CloseTextToSpeech();
			m_TextToSpeechEnabled = false;
		}
		else
		{
			m_TextToSpeechEnabled = InitTextToSpeech();
		}

		CheckMenuItem(IDM_TEXTTOSPEECH_ENABLE, m_TextToSpeechEnabled);
		break;

	case IDM_TEXTTOSPEECH_AUTO_SPEAK:
		TextToSpeechToggleAutoSpeak();
		CheckMenuItem(IDM_TEXTTOSPEECH_AUTO_SPEAK, m_SpeechWriteChar);
		break;

	case IDM_TEXTTOSPEECH_SPEAK_PUNCTUATION:
		TextToSpeechToggleSpeakPunctuation();
		CheckMenuItem(IDM_TEXTTOSPEECH_SPEAK_PUNCTUATION, m_SpeechSpeakPunctuation);
		break;

	case IDM_TEXTTOSPEECH_INCREASE_RATE:
		TextToSpeechIncreaseRate();
		EnableMenuItem(IDM_TEXTTOSPEECH_INCREASE_RATE, m_SpeechRate < 10);
		EnableMenuItem(IDM_TEXTTOSPEECH_DECREASE_RATE, m_SpeechRate > -10);
		break;

	case IDM_TEXTTOSPEECH_DECREASE_RATE:
		TextToSpeechDecreaseRate();
		EnableMenuItem(IDM_TEXTTOSPEECH_INCREASE_RATE, m_SpeechRate < 10);
		EnableMenuItem(IDM_TEXTTOSPEECH_DECREASE_RATE, m_SpeechRate > -10);
		break;

	case IDM_TEXT_TO_SPEECH_VOICE_BASE:
	case IDM_TEXT_TO_SPEECH_VOICE_BASE + 1:
	case IDM_TEXT_TO_SPEECH_VOICE_BASE + 2:
	case IDM_TEXT_TO_SPEECH_VOICE_BASE + 3:
	case IDM_TEXT_TO_SPEECH_VOICE_BASE + 4:
	case IDM_TEXT_TO_SPEECH_VOICE_BASE + 5:
	case IDM_TEXT_TO_SPEECH_VOICE_BASE + 6:
	case IDM_TEXT_TO_SPEECH_VOICE_BASE + 7:
	case IDM_TEXT_TO_SPEECH_VOICE_BASE + 8:
	case IDM_TEXT_TO_SPEECH_VOICE_BASE + 9:
		TextToSpeechSetVoice(MenuID - IDM_TEXT_TO_SPEECH_VOICE_BASE);
		break;

	case IDM_TEXTVIEW:
		m_TextViewEnabled = !m_TextViewEnabled;
		InitTextView();
		break;

	case IDM_DISABLEKEYSWINDOWS:
	{
		bool reboot = false;
		m_DisableKeysWindows = !m_DisableKeysWindows;
		UpdateDisableKeysMenu();
		if (m_DisableKeysWindows)
		{
			// Give user warning
			if (Report(MessageType::Question,
			           "Disabling the Windows keys will affect the whole PC.\n"
			           "Go ahead and disable the Windows keys?") == MessageResult::Yes)
			{
				int binsize=sizeof(CFG_DISABLE_WINDOWS_KEYS);
				RegSetBinaryValue(HKEY_LOCAL_MACHINE, CFG_KEYBOARD_LAYOUT,
				                  CFG_SCANCODE_MAP, CFG_DISABLE_WINDOWS_KEYS, &binsize);
				reboot = true;
			}
			else
			{
				m_DisableKeysWindows = false;
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
			if (Report(MessageType::Question,
			           "Reboot required for key change to\ntake effect. Reboot now?") == MessageResult::Yes)
			{
				RebootSystem();
			}
		}
		break;
	}

	case IDM_DISABLEKEYSBREAK:
		m_DisableKeysBreak = !m_DisableKeysBreak;
		UpdateDisableKeysMenu();
		break;

	case IDM_DISABLEKEYSESCAPE:
		m_DisableKeysEscape = !m_DisableKeysEscape;
		UpdateDisableKeysMenu();
		break;

	case IDM_DISABLEKEYSSHORTCUT:
		m_DisableKeysShortcut = !m_DisableKeysShortcut;
		UpdateDisableKeysMenu();
		break;

	case IDM_DISABLEKEYSALL:
		if (m_DisableKeysWindows && m_DisableKeysBreak &&
			m_DisableKeysEscape && m_DisableKeysShortcut)
		{
			if (m_DisableKeysWindows)
				HandleCommand(IDM_DISABLEKEYSWINDOWS);
			m_DisableKeysBreak = false;
			m_DisableKeysEscape = false;
			m_DisableKeysShortcut = false;
		}
		else
		{
			if (!m_DisableKeysWindows)
				HandleCommand(IDM_DISABLEKEYSWINDOWS);
			m_DisableKeysBreak = true;
			m_DisableKeysEscape = true;
			m_DisableKeysShortcut = true;
		}
		UpdateDisableKeysMenu();
		break;
	}

	if (!StayMuted)
	{
		SetSound(SoundState::Unmuted);
	}

	if (m_PaletteType != PrevPaletteType)
	{
		CreateBitmap();
		UpdateMonitorMenu();
	}
}

void BeebWin::SetSoundMenu()
{
	CheckMenuItem(IDM_SOUNDONOFF, SoundEnabled);
	CheckMenuItem(IDM_MUSIC5000, Music5000Enabled);
}

void BeebWin::Activate(bool Active)
{
	if (Active)
	{
		m_Frozen = false;
	}
	else if (m_FreezeWhenInactive)
	{
		m_Frozen = true;
	}

	if (Active)
	{
		// Bring debug window to foreground BEHIND main window.
		if (hwndDebug)
		{
			SetWindowPos(hwndDebug, m_hWnd, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOACTIVATE);
			SetWindowPos(m_hWnd, HWND_TOP, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		}
	}

	if (m_MouseCaptured && !Active)
	{
		ReleaseMouse();
	}
}

void BeebWin::Focus(bool Focus)
{
	if (Focus)
	{
		if (m_TextViewEnabled)
		{
			::SetFocus(m_hTextView);
		}
	}
	else
	{
		BeebReleaseAllKeys();
	}
}

bool BeebWin::IsFrozen() const
{
	return m_Frozen;
}

void BeebWin::TogglePause()
{
	m_EmuPaused = !m_EmuPaused;
	CheckMenuItem(IDM_EMUPAUSED, m_EmuPaused);
	if (m_ShowSpeedAndFPS && m_EmuPaused)
	{
		sprintf(m_szTitle, "%s  Paused", WindowTitle);
		SetWindowText(m_hWnd, m_szTitle);
	}

	if (m_EmuPaused)
	{
		KillTimer(m_hWnd, TIMER_KEYBOARD);
		KillTimer(m_hWnd, TIMER_AUTOBOOT_DELAY);
	}
	else
	{
		if (HasKbdCmd() && !m_KeyboardTimerElapsed)
		{
			SetKeyboardTimer();
		}

		if (m_AutoBootDisc && !m_BootDiscTimerElapsed)
		{
			SetBootDiscTimer();
		}
	}
}

bool BeebWin::IsPaused() const
{
	return m_EmuPaused;
}

void BeebWin::SetFreezeWhenInactive(bool State)
{
	m_FreezeWhenInactive = State;
	CheckMenuItem(IDM_FREEZEINACTIVE, m_FreezeWhenInactive);
}

/****************************************************************************/

void BeebWin::EditRomConfig()
{
	RomConfigDialog Dialog(hInst, m_hWnd, RomConfig);

	if (Dialog.DoModal())
	{
		// Copy in new config and read ROMs
		RomConfig = Dialog.GetRomConfig();
		BeebReadRoms();
	}
}

/****************************************************************************/

#ifndef HID_USAGE_PAGE_GENERIC
#define HID_USAGE_PAGE_GENERIC ((USHORT)0x01)
#endif

#ifndef HID_USAGE_GENERIC_MOUSE
#define HID_USAGE_GENERIC_MOUSE ((USHORT)0x02)
#endif

void BeebWin::CaptureMouse()
{
	if (m_MouseCaptured)
		return;

	RAWINPUTDEVICE Rid[1];
	Rid[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
	Rid[0].usUsage = HID_USAGE_GENERIC_MOUSE;
	Rid[0].dwFlags = 0;
	Rid[0].hwndTarget = m_hWnd;
	if (!RegisterRawInputDevices(Rid, 1, sizeof(Rid[0])))
		return;

	// Capture mouse
	m_MouseCaptured = true;
	SetCapture(m_hWnd);

	// Move mouse to window centre and hide cursor
	POINT centre{ m_XWinSize/2, m_YWinSize/2 };
	m_RelMousePos = centre;
	ClientToScreen(m_hWnd, &centre);
	SetCursorPos(centre.x, centre.y);
	ShowCursor(false);

	// Clip cursor to the main window's client area. Coordinates are converted
	// to screen coordinates for ClipCursor()
	RECT clientRect;
	GetClientRect(m_hWnd, &clientRect);
	MapWindowPoints(m_hWnd, nullptr, reinterpret_cast<LPPOINT>(&clientRect), 2);
	ClipCursor(&clientRect);

	// Display info on title bar
	UpdateWindowTitle();
}

void BeebWin::ReleaseMouse()
{
	if (!m_MouseCaptured)
		return;

	// Deregister to stop receiving WM_INPUT messages.
	RAWINPUTDEVICE Rid[1];
	Rid[0].usUsagePage = HID_USAGE_PAGE_GENERIC;
	Rid[0].usUsage = HID_USAGE_GENERIC_MOUSE;
	Rid[0].dwFlags = RIDEV_REMOVE;
	Rid[0].hwndTarget = nullptr;

	RegisterRawInputDevices(Rid, 1, sizeof(Rid[0]));

	ClipCursor(nullptr);

	// Show cursor in the centre of the window
	POINT centre{ m_XWinSize / 2, m_YWinSize / 2 };
	ClientToScreen(m_hWnd, &centre);
	SetCursorPos(centre.x, centre.y);

	// Release mouse and show cursor
	ShowCursor(true);
	ReleaseCapture();

	m_MouseCaptured = false;

	// Restore original window title
	UpdateWindowTitle();
}

void BeebWin::OpenUserKeyboardDialog()
{
	// Pause the emulator if not already paused.

	m_WasPaused = IsPaused();

	if (!m_WasPaused)
	{
		TogglePause();
	}

	UserKeyboardDialog(m_hWnd);
}

void BeebWin::UserKeyboardDialogClosed()
{
	// Restart the emulator if it wasn't paused before the "user keyboard"
	// dialog box was opened.

	if (!m_WasPaused)
	{
		TogglePause();
	}
}

/*****************************************************************************/
// Parse command line parameters

void BeebWin::ParseCommandLine()
{
	bool invalid;

	m_CommandLineFileName1[0] = '\0';
	m_CommandLineFileName2[0] = '\0';

	int i = 1;

	while (i < __argc)
	{
		// Params with no arguments
		if (_stricmp(__argv[i], "-DisMenu") == 0)
		{
			m_DisableMenu = true;
		}
		else if (_stricmp(__argv[i], "-NoAutoBoot") == 0)
		{
			m_NoAutoBoot = true;
		}
		else if (_stricmp(__argv[i], "-StartPaused") == 0)
		{
			m_StartPaused = true;
			m_EmuPaused = true;
		}
		else if (_stricmp(__argv[i], "-FullScreen") == 0)
		{
			m_StartFullScreen = true;
		}
		else if (__argv[i][0] == '-' && i+1 >= __argc)
		{
			Report(MessageType::Error, "Invalid command line parameter:\n  %s",
			       __argv[i]);
		}
		else // Params with additional arguments
		{
			invalid = false;

			const bool Data       = _stricmp(__argv[i], "-Data") == 0;
			const bool CustomData = _stricmp(__argv[i], "-CustomData") == 0;

			if (Data || CustomData)
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
					if (m_UserDataPath[strlen(m_UserDataPath) - 1] != '\\' &&
					    m_UserDataPath[strlen(m_UserDataPath) - 1] != '/')
					{
						strcat(m_UserDataPath, "\\");
					}
				}

				if (CustomData) {
					m_CustomData = true;
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
			else if (_stricmp(__argv[i], "-EconetCfg") == 0)
			{
				strcpy(EconetCfgPath, __argv[++i]);
			}
			else if (_stricmp(__argv[i], "-AUNMap") == 0)
			{
				strcpy(AUNMapPath, __argv[++i]);
			}
			else if (_stricmp(__argv[i], "-EcoStn") == 0)
			{
				int a = atoi(__argv[++i]);

				if (a < 1 || a > 254)
					invalid = true;
				else
					EconetStationID = static_cast<unsigned char>(a);
			}
			else if (_stricmp(__argv[i], "-EcoFF") == 0)
			{
				int a = atoi(__argv[++i]);

				if (a < 1)
					invalid = true;
				else
					EconetFlagFillTimeout = a;
			}
			else if (_stricmp(__argv[i], "-KbdCmd") == 0)
			{
				m_KbdCmd = __argv[++i];
			}
			else if (_stricmp(__argv[i], "-DebugScript") == 0)
			{
				m_DebugScriptFileName = __argv[++i];
			}
			else if (_stricmp(__argv[i], "-DebugLabels") == 0)
			{
				m_DebugLabelsFileName = __argv[++i];
			}
			else if (_stricmp(__argv[i], "-AutoBootDelay") == 0)
			{
				int a = atoi(__argv[++i]);

				if (a < 1)
					invalid = true;
				else
					m_AutoBootDelay = a;
			}
			else if (__argv[i][0] == '-')
			{
				invalid = true;
				++i;
			}
			else
			{
				// Assume it's a file name
				if (m_CommandLineFileName1[0] == '\0')
				{
					strncpy(m_CommandLineFileName1, __argv[i], _MAX_PATH);
				}
				else if (m_CommandLineFileName2[0] == '\0')
				{
					strncpy(m_CommandLineFileName2, __argv[i], _MAX_PATH);
				}
			}

			if (invalid)
			{
				Report(MessageType::Error, "Invalid command line parameter:\n  %s %s",
				       __argv[i-1], __argv[i]);
			}
		}

		++i;
	}
}

/*****************************************************************************/
// Check for preference files in the same directory as the file specified
void BeebWin::CheckForLocalPrefs(const char *path, bool bLoadPrefs)
{
	if (path[0] == '\0')
		return;

	char file[_MAX_PATH];
	char drive[_MAX_DRIVE];
	char dir[_MAX_DIR];

	_splitpath(path, drive, dir, NULL, NULL);

	// Look for prefs file
	_makepath(file, drive, dir, "Preferences", "cfg");

	if (FileExists(file))
	{
		// File exists, use it
		strcpy(m_PrefsFile, file);

		if (bLoadPrefs)
		{
			LoadPreferences();

			// Reinit with new prefs
			SetWindowPos(m_hWnd, HWND_TOP, m_XWinPos, m_YWinPos,
			             0, 0, SWP_NOSIZE);
			SetDisplayRenderer(m_DisplayRenderer);
			InitMenu();
			SetWindowText(m_hWnd, WindowTitle);

			if (m_JoystickOption == JoystickOption::Joystick && MachineType != Model::MasterET)
			{
				InitJoystick();
			}
		}
	}

	// Look for ROMs file
	_makepath(file, drive, dir, "Roms", "cfg");

	if (FileExists(file))
	{
		// File exists, use it
		strcpy(RomFile, file);

		if (bLoadPrefs)
		{
			RomConfig.Load(RomFile);
			BeebReadRoms();
		}
	}
}

enum FileType {
	None,
	SSD,
	DSD,
	ADFS,
	UEF,
	UEFState,
	CSW,
	IMG,
	FSD
};

static FileType GetFileTypeFromExtension(const char* FileName)
{
	FileType Type = FileType::None;

	const char *Ext = strrchr(FileName, '.');

	if (Ext != nullptr)
	{
		if (_stricmp(Ext + 1, "ssd") == 0)
		{
			Type = FileType::SSD;
		}
		else if (_stricmp(Ext + 1, "dsd") == 0)
		{
			Type = FileType::DSD;
		}
		else if (_stricmp(Ext + 1, "adl") == 0)
		{
			Type = FileType::ADFS;
		}
		else if (_stricmp(Ext + 1, "adf") == 0)
		{
			Type = FileType::ADFS;
		}
		else if (_stricmp(Ext + 1, "uef") == 0)
		{
			Type = FileType::UEF;
		}
		else if (_stricmp(Ext + 1, "uefstate") == 0)
		{
			Type = FileType::UEFState;
		}
		else if (_stricmp(Ext + 1, "csw") == 0)
		{
			Type = FileType::CSW;
		}
		else if (_stricmp(Ext + 1, "img") == 0)
		{
			Type = FileType::IMG;
		}
		else if (_stricmp(Ext + 1, "fsd") == 0)
		{
			Type = FileType::FSD;
		}
	}

	return Type;
}

/*****************************************************************************/
// File location of a file passed on command line

void BeebWin::FindCommandLineFile(char *CmdLineFile)
{
	bool cont = false;
	char TmpPath[_MAX_PATH];
	char *FileName = NULL;
	FileType Type = FileType::None;

	// See if file is readable
	if (CmdLineFile[0] != '\0')
	{
		FileName = CmdLineFile;
		strncpy(TmpPath, CmdLineFile, _MAX_PATH);

		// Work out which type of file it is
		Type = GetFileTypeFromExtension(FileName);

		if (Type != FileType::None)
		{
			cont = true;
		}
		else
		{
			Report(MessageType::Error, "Unrecognised file type:\n  %s",
			       FileName);

			cont = false;
		}
	}

	if (cont)
	{
		cont = false;

		if (FileExists(FileName))
		{
			cont = true;
		}
		else if (Type == FileType::UEF)
		{
			// Try getting it from Tapes directory
			strcpy(TmpPath, m_UserDataPath);
			strcat(TmpPath, "Tapes/");
			strcat(TmpPath, FileName);

			if (FileExists(TmpPath))
			{
				cont = true;
				FileName = TmpPath;
			}
		}
		else if (Type == FileType::UEFState)
		{
			strcpy(TmpPath, m_UserDataPath);
			strcat(TmpPath, "BeebState/");
			strcat(TmpPath, FileName);

			if (FileExists(TmpPath))
			{
				cont = true;
				FileName = TmpPath;
			}
		}
		else if (Type == FileType::CSW)
		{
			// Try getting it from Tapes directory
			strcpy(TmpPath, m_UserDataPath);
			strcat(TmpPath, "Tapes/");
			strcat(TmpPath, FileName);

			if (FileExists(TmpPath))
			{
				cont = true;
				FileName = TmpPath;
			}
		}
		else
		{
			// Try getting it from DiscIms directory
			strcpy(TmpPath, m_UserDataPath);
			strcat(TmpPath, "DiscIms/");
			strcat(TmpPath, FileName);

			if (FileExists(TmpPath))
			{
				cont = true;
				FileName = TmpPath;
			}
		}

		if (!cont)
		{
			Report(MessageType::Error, "Cannot find file:\n  %s", FileName);
			cont = false;
		}
	}

	if (cont)
	{
		PathCanonicalize(CmdLineFile, TmpPath);
	}
	else
	{
		CmdLineFile[0] = '\0';
	}
}

/*****************************************************************************/
// Handle a file name passed on command line

void BeebWin::HandleCommandLineFile(int Drive, const char *CmdLineFile)
{
	bool cont = false;
	const char *FileName = NULL;
	FileType Type = FileType::None;

	m_AutoBootDisc = false;

	if (CmdLineFile[0] != '\0')
	{
		FileName = CmdLineFile;

		// Work out which type of files it is
		Type = GetFileTypeFromExtension(FileName);

		if (Type != FileType::None)
		{
			cont = true;
		}
		else
		{
			Report(MessageType::Error, "Unrecognised file type:\n  %s",
			       FileName);

			cont = false;
		}
	}

	if (cont)
	{
		cont = FileExists(FileName);

		if (!cont)
		{
			Report(MessageType::Error, "Cannot find file:\n  %s", FileName);
		}
	}

	if (cont)
	{
		if (Type == FileType::UEFState)
		{
			LoadUEFState(FileName);
		}
		else if (Type == FileType::UEF)
		{
			// Determine if file is a tape or a state file
			if (IsUEFSaveState(FileName))
			{
				LoadUEFState(FileName);
			}
			else
			{
				LoadUEFTape(FileName);
			}

			cont = false;
		}
		else if (Type == FileType::CSW)
		{
			CSWOpen(FileName);
			cont = false;
		}
	}

	if (cont)
	{
		if (MachineType != Model::Master128 && MachineType != Model::MasterET)
		{
			if (Type == FileType::DSD)
			{
				if (NativeFDC)
				{
					Load8271DiscImage(FileName, Drive, 80, DiscType::DSD);
				}
				else
				{
					Load1770DiscImage(FileName, Drive, DiscType::DSD);
				}
			}
			else if (Type == FileType::SSD)
			{
				if (NativeFDC)
				{
					Load8271DiscImage(FileName, Drive, 80, DiscType::SSD);
				}
				else
				{
					Load1770DiscImage(FileName, Drive, DiscType::SSD);
				}
			}
			else if (Type == FileType::ADFS)
			{
				if (!NativeFDC)
				{
					Load1770DiscImage(FileName, Drive, DiscType::ADFS);
				}
				else
				{
					cont = false;  // cannot load adfs with native DFS
				}
			}
			else if (Type == FileType::IMG)
			{
				if (NativeFDC)
				{
					Load8271DiscImage(FileName, Drive, 80, DiscType::SSD); // Treat like an ssd
				}
				else
				{
					Load1770DiscImage(FileName, Drive, DiscType::IMG);
				}
			}
			else if (Type == FileType::FSD)
			{
				if (NativeFDC)
				{
					Load8271DiscImage(FileName, Drive, 80, DiscType::FSD);
				}
				else
				{
					Report(MessageType::Error, "FSD images are only supported with the 8271 FDC");
					return;
				}
			}
		}
		else if (MachineType == Model::Master128)
		{
			if (Type == FileType::FSD)
			{
				Load1770DiscImage(FileName, Drive, DiscType::DSD);
			}
			else if (Type == FileType::SSD)
			{
				Load1770DiscImage(FileName, Drive, DiscType::SSD);
			}
			else if (Type == FileType::ADFS)
			{
				Load1770DiscImage(FileName, Drive, DiscType::ADFS);
			}
			else if (Type == FileType::IMG)
			{
				Load1770DiscImage(FileName, Drive, DiscType::IMG);
			}
			else if (Type == FileType::FSD)
			{
				Report(MessageType::Error, "FSD images are only supported with the 8271 FDC");
				return;
			}
		}

		// Write protect the disc
		if (m_WriteProtectOnLoad)
		{
			SetDiscWriteProtect(Drive, true);
		}
	}

	if (cont && !m_NoAutoBoot && Drive == 0)
	{
		m_AutoBootDisc = true;

		if (!m_StartPaused)
		{
			SetBootDiscTimer();
		}
	}
}

void BeebWin::DoShiftBreak()
{
	// Do a shift + break
	ResetBeebSystem(MachineType, false);
	BeebKeyDown(0, 0); // Shift key
	m_ShiftBooted = true;
}

bool BeebWin::HasKbdCmd() const
{
	return !m_KbdCmd.empty();
}

void BeebWin::SetKeyboardTimer()
{
	SetTimer(m_hWnd, TIMER_KEYBOARD, 1000, NULL);
}

void BeebWin::SetBootDiscTimer()
{
	SetTimer(m_hWnd, TIMER_AUTOBOOT_DELAY, m_AutoBootDelay, NULL);
}

void BeebWin::KillBootDiscTimer()
{
	m_BootDiscTimerElapsed = true;
	KillTimer(m_hWnd, TIMER_AUTOBOOT_DELAY);
}

/****************************************************************************/
bool BeebWin::RebootSystem()
{
	HANDLE hToken;
	TOKEN_PRIVILEGES tkp;

	if (!OpenProcessToken(GetCurrentProcess(),
	                      TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &hToken))
		return false;

	LookupPrivilegeValue(NULL, SE_SHUTDOWN_NAME,
	                     &tkp.Privileges[0].Luid);

	tkp.PrivilegeCount = 1;  // one privilege to set
	tkp.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
	AdjustTokenPrivileges(hToken, FALSE, &tkp, 0, NULL, 0);
	if (GetLastError() != ERROR_SUCCESS)
		return false;

	if (!ExitWindowsEx(EWX_REBOOT | EWX_FORCE,
	                   SHTDN_REASON_MAJOR_OPERATINGSYSTEM |
	                   SHTDN_REASON_MINOR_RECONFIG |
	                   SHTDN_REASON_FLAG_PLANNED))
		return false;

	return true;
}

/****************************************************************************/
bool BeebWin::CheckUserDataPath(bool Persist)
{
	bool success = true;
	bool copy_user_files = false;
	bool store_user_data_path = false;
	char path[_MAX_PATH];

	// Change all '/' to '\'
	for (size_t i = 0; i < strlen(m_UserDataPath); ++i)
		if (m_UserDataPath[i] == '/')
			m_UserDataPath[i] = '\\';

	// Check that the folder exists
	if (!FolderExists(m_UserDataPath))
	{
		if (Report(MessageType::Question,
		           "BeebEm data folder does not exist:\n  %s\n\nCreate the folder?",
		           m_UserDataPath) != MessageResult::Yes)
		{
			// Use data dir installed with BeebEm
			strcpy(m_UserDataPath, m_AppPath);
			strcat(m_UserDataPath, "UserData\\");

			store_user_data_path = true;
		}
		else
		{
			// Create the folder
			int result = SHCreateDirectoryEx(nullptr, m_UserDataPath, nullptr);

			if (result == ERROR_SUCCESS)
			{
				copy_user_files = true;
			}
			else
			{
				Report(MessageType::Error, "Failed to create BeebEm data folder:\n  %s",
				       m_UserDataPath);
				success = false;
			}
		}
	}
	else
	{
		// Check that essential files are in the user data folder
		sprintf(path, "%sBeebFile", m_UserDataPath);

		if (!FolderExists(path))
			copy_user_files = true;

		if (!copy_user_files)
		{
			sprintf(path, "%sBeebState", m_UserDataPath);

			if (!FolderExists(path))
				copy_user_files = true;
		}

		if (!copy_user_files)
		{
			sprintf(path, "%sEconet.cfg", m_UserDataPath);

			if (!FileExists(path))
				copy_user_files = true;
		}

		if (!copy_user_files)
		{
			sprintf(path, "%sAUNMap", m_UserDataPath);

			if (!FileExists(path))
				copy_user_files = true;
		}

		if (!copy_user_files)
		{
			sprintf(path, "%sPhroms.cfg", m_UserDataPath);

			if (!FileExists(path))
				copy_user_files = true;
		}

		if (!copy_user_files)
		{
			if (strcmp(RomFile, "Roms.cfg") == 0)
			{
				sprintf(path, "%sRoms.cfg", m_UserDataPath);

				if (!FileExists(path))
					copy_user_files = true;
			}
		}

		if (copy_user_files)
		{
			if (Report(MessageType::Question,
			           "Essential or new files missing from BeebEm data folder:\n  %s"
			           "\n\nCopy essential or new files into folder?",
			           m_UserDataPath) != MessageResult::Yes)
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
			Report(MessageType::Error, "Copy failed.  Manually copy files from:\n  %s"
			                           "\n\nTo BeebEm data folder:\n  %s",
			       path, m_UserDataPath);
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

		if (!FileExists(RomFile))
		{
			Report(MessageType::Error, "Cannot open ROMs file:\n  %s", RomFile);
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

	if (success && Persist && (copy_user_files || store_user_data_path))
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

// Selection of User Data Folder

void BeebWin::SelectUserDataPath()
{
	std::string PathBackup;

	FolderSelectDialog Dialog(
		m_hWnd,
		"Select folder for user data files:",
		m_UserDataPath
	);

	FolderSelectDialog::Result result = Dialog.DoModal();

	switch (result) {
		case FolderSelectDialog::Result::OK:
			PathBackup = m_UserDataPath;
			strcpy(m_UserDataPath, Dialog.GetFolder().c_str());
			strcat(m_UserDataPath, "\\");

			// Check folder contents
			if (!CheckUserDataPath(true))
			{
				strcpy(m_UserDataPath, PathBackup.c_str());
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
			break;

		case FolderSelectDialog::Result::InvalidFolder:
			Report(MessageType::Warning, "Invalid folder selected");
			break;
	}
}

/****************************************************************************/
void BeebWin::HandleTimer()
{
	int row,col;
	char delay[10];

	m_KeyboardTimerElapsed = true;

	// Do nothing if emulation is not running (e.g. if Window is being moved)
	if ((TotalCycles - m_KbdCmdLastCycles) / 2000 < m_KbdCmdDelay)
	{
		SetTimer(m_hWnd, TIMER_KEYBOARD, m_KbdCmdDelay, NULL);
		return;
	}
	m_KbdCmdLastCycles = TotalCycles;

	// Release previous key press (except shift/control)
	if (m_KbdCmdPress &&
	    m_KbdCmdKey != VK_SHIFT && m_KbdCmdKey != VK_CONTROL)
	{
		TranslateKey(m_KbdCmdKey, true, row, col);
		m_KbdCmdPress = false;
		SetTimer(m_hWnd, TIMER_KEYBOARD, m_KbdCmdDelay, NULL);
	}
	else
	{
		m_KbdCmdPos++;

		if (m_KbdCmd[m_KbdCmdPos] == 0)
		{
			KillTimer(m_hWnd, TIMER_KEYBOARD);
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
					TranslateKey(m_KbdCmdKey, false, row, col);
				else
					TranslateKey(m_KbdCmdKey, true, row, col);
			}

			SetTimer(m_hWnd, TIMER_KEYBOARD, m_KbdCmdDelay, NULL);
		}
	}
}

/****************************************************************************/

void BeebWin::OpenDebugWindow()
{
	SetFreezeWhenInactive(false);
	DebugOpenDialog(hInst, m_hWnd);
}

/****************************************************************************/

MessageResult BeebWin::Report(MessageType type, const char *format, ...)
{
	va_list args;
	va_start(args, format);

	MessageResult Result = ReportV(type, format, args);

	va_end(args);

	return Result;
}

MessageResult BeebWin::ReportV(MessageType type, const char *format, va_list args)
{
	MessageResult Result = MessageResult::None;

	// Calculate required length, +1 is for NUL terminator
	const int length = _vscprintf(format, args) + 1;

	char *buffer = (char*)malloc(length);

	if (buffer != nullptr)
	{
		vsprintf(buffer, format, args);

		UINT Type = 0;

		switch (type)
		{
			case MessageType::Error:
			default:
				Type = MB_ICONERROR;
				break;

			case MessageType::Warning:
				Type = MB_ICONWARNING;
				break;

			case MessageType::Info:
				Type = MB_ICONINFORMATION;
				break;

			case MessageType::Question:
				Type = MB_ICONQUESTION | MB_YESNO;
				break;

			case MessageType::Confirm:
				Type = MB_ICONWARNING | MB_OKCANCEL;
		}

		int ID = MessageBox(m_hWnd, buffer, WindowTitle, Type);

		if (type == MessageType::Question)
		{
			switch (ID)
			{
				case IDYES:
					Result = MessageResult::Yes;
					break;

				case IDNO:
					Result = MessageResult::No;
					break;

				case IDOK:
					Result = MessageResult::OK;
					break;

				case IDCANCEL:
				default:
					Result = MessageResult::Cancel;
					break;
			}
		}

		free(buffer);
	}

	return Result;
}

/****************************************************************************/
