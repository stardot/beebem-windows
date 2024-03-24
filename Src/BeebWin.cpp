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
#include "Resource.h"
#include "RomConfigDialog.h"
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
#include "TouchScreen.h"
#include "Tube.h"
#include "UefState.h"
#include "UserKeyboardDialog.h"
#include "UserPortBreakoutBox.h"
#include "UserVia.h"
#include "Version.h"
#include "Video.h"
#include "Z80mem.h"
#include "Z80.h"

using namespace Gdiplus;

constexpr UINT WIN_STYLE = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX | WS_SIZEBOX;

// Some LED based constants
constexpr int LED_COL_BASE = 64;

static const char *CFG_REG_KEY = "Software\\BeebEm";

static const unsigned char CFG_DISABLE_WINDOWS_KEYS[24] = {
	00,00,00,00,00,00,00,00,03,00,00,00,00,00,0x5B,0xE0,00,00,0x5C,0xE0,00,00,00,00
};

CArm *arm = nullptr;
CSprowCoPro *sprow = nullptr;

LEDType LEDs;

LEDColour DiscLedColour = LEDColour::Red;

const char *WindowTitle = "BeebEm - BBC Model B / Master 128 Emulator";

/****************************************************************************/
BeebWin::BeebWin()
{
	m_hWnd = nullptr;
	m_DXInit = false;
	m_LastStartY = 0;
	m_LastNLines = 256;
	m_LastTickCount = 0;
	m_KeyMapAS = false;
	m_KeyMapFunc = false;
	m_ShiftPressed = 0;
	m_ShiftBooted = false;

	for (int k = 0; k < 256; ++k)
	{
		m_vkeyPressed[k][0][0] = -1;
		m_vkeyPressed[k][1][0] = -1;
		m_vkeyPressed[k][0][1] = -1;
		m_vkeyPressed[k][1][1] = -1;
	}

	m_DisableKeysWindows = false;
	m_DisableKeysBreak = false;
	m_DisableKeysEscape = false;
	m_DisableKeysShortcut = false;
	memset(m_UserKeyMapPath, 0, sizeof(m_UserKeyMapPath));
	m_hBitmap = m_hOldObj = m_hDCBitmap = NULL;
	m_screen = m_screen_blur = NULL;
	m_ScreenRefreshCount = 0;
	m_RelativeSpeed = 1;
	m_FramesPerSecond = 50;
	strcpy(m_szTitle, WindowTitle);
	m_AviDC = nullptr;
	m_AviDIB = nullptr;
	m_CaptureBitmapPending = false;
	m_SpVoice = nullptr;
	m_hTextView = nullptr;
	m_TextViewPrevWndProc = nullptr;
	m_Frozen = false;
	m_WriteProtectDisc[0] = !IsDiscWritable(0);
	m_WriteProtectDisc[1] = !IsDiscWritable(1);
	m_AutoSavePrefsCMOS = false;
	m_AutoSavePrefsFolders = false;
	m_AutoSavePrefsAll = false;
	m_AutoSavePrefsChanged = false;
	m_KbdCmdPos = -1;
	m_KbdCmdPress = false;
	m_KbdCmdDelay = 40;
	m_KbdCmdLastCycles = 0;
	m_NoAutoBoot = false;
	m_AutoBootDelay = 0;
	m_EmuPaused = false;
	m_StartPaused = false;
	m_WasPaused = false;
	m_KeyboardTimerElapsed = false;
	m_BootDiscTimerElapsed = false;
	memset(m_ClipboardBuffer, 0, sizeof(m_ClipboardBuffer));
	m_ClipboardLength = 0;
	m_ClipboardIndex = 0;
	m_printerbufferlen = 0;
	m_translateCRLF = true;
	m_CurrentDisplayRenderer = 0;
	m_DXSmoothing = true;
	m_DXSmoothMode7Only = false;
	m_DXResetPending = false;

	m_JoystickCaptured = false;
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
	m_HideMenuEnabled = false;
	m_DisableMenu = false;
	m_MenuOn = true;
	m_PaletteType = PaletteType::RGB;
	m_WriteInstructionCounts = false;
	m_CaptureMouse = false;
	m_MouseCaptured = false;

	m_TextToSpeechEnabled = false;
	m_hVoiceMenu = nullptr;
	m_SpVoice = nullptr;
	m_SpeechRate = 0;
	TextToSpeechResetState();

	InitKeyMap();

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

	m_CustomData = false;

	// Set default files, may be overridden by command line parameters.
	strcpy(m_PrefsFile, "Preferences.cfg");
	strcpy(RomFile, "Roms.cfg");
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
	if (m_startFullScreen)
	{
		m_isFullScreen = true;
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

	ReadROMConfigFile(RomFile, RomConfig);
	ApplyPrefs();

	if (!m_DebugScriptFileName.empty())
	{
		DebugOpenDialog(hInst, m_hWnd);
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
	if (m_DisplayRenderer != IDM_DISPGDI)
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
	if (m_MenuIDSticks == IDM_JOYSTICK)
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
	if (Music5000Enabled)
		Music5000Init();
	MachineType=NewModelType;
	BeebMemInit(LoadRoms, m_ShiftBooted);
	Init6502core();

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

	if (SCSIDriveEnabled) SCSIReset();
	if (SCSIDriveEnabled) SASIReset();
	if (IDEDriveEnabled)  IDEReset();
	TeletextInit();
	if (MachineType == Model::Master128) {
		InvertTR00 = false;
	}
	else {
		LoadFDC(NULL, false);
	}

	// Keep the disc images loaded

	if (MachineType != Model::Master128 && NativeFDC)
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

	Disc8271Reset();
	Reset1770();
	if (EconetEnabled) EconetReset();//Rob
	if (SCSIDriveEnabled) SCSIReset();
	if (SCSIDriveEnabled) SASIReset();
	if (IDEDriveEnabled)  IDEReset();
	TeletextInit();
	//SoundChipReset();
	Music5000Reset();
	if (Music5000Enabled)
		Music5000Init();

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
		RTCReset();
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
void BeebWin::CreateBeebWindow(void)
{
	DWORD style;
	int show = SW_SHOW;

	int x = m_XWinPos;
	int y = m_YWinPos;

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
		"BEEBWIN",  // See RegisterClass() call.
		m_szTitle,  // Text for window title bar.
		style,
		x, y,
		m_XWinSize + m_XWinBorder,
		m_YWinSize + m_YWinBorder,
		nullptr,    // Overlapped windows have no parent.
		nullptr,    // Use the window class menu.
		hInst,      // This instance owns this window.
		this
	);

	DisableRoundedCorners(m_hWnd);

	ShowWindow(m_hWnd, show); // Show the window
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

void BeebWin::EnableMenuItem(UINT id, bool enabled)
{
	::EnableMenuItem(m_hMenu, id, enabled ? MF_ENABLED : MF_GRAYED);
}

void BeebWin::InitMenu(void)
{
	char menu_string[256];

	// File -> Video Options
	CheckMenuItem(IDM_VIDEORES1, false);
	CheckMenuItem(IDM_VIDEORES2, false);
	CheckMenuItem(IDM_VIDEORES3, false);
	CheckMenuItem(IDM_VIDEOSKIP0, false);
	CheckMenuItem(IDM_VIDEOSKIP1, false);
	CheckMenuItem(IDM_VIDEOSKIP2, false);
	CheckMenuItem(IDM_VIDEOSKIP3, false);
	CheckMenuItem(IDM_VIDEOSKIP4, false);
	CheckMenuItem(IDM_VIDEOSKIP5, false);
	CheckMenuItem(m_MenuIDAviResolution, true);
	CheckMenuItem(m_MenuIDAviSkip, true);

	// File -> Disc Options
	CheckMenuItem(IDM_WRITE_PROTECT_DISC0, m_WriteProtectDisc[0]);
	CheckMenuItem(IDM_WRITE_PROTECT_DISC1, m_WriteProtectDisc[1]);
	CheckMenuItem(IDM_WRITE_PROTECT_ON_LOAD, m_WriteProtectOnLoad);

	// File -> Capture Options
	CheckMenuItem(IDM_CAPTURERES_DISPLAY, false);
	CheckMenuItem(IDM_CAPTURERES_1280, false);
	CheckMenuItem(IDM_CAPTURERES_640, false);
	CheckMenuItem(IDM_CAPTURERES_320, false);
	CheckMenuItem(IDM_CAPTUREBMP, false);
	CheckMenuItem(IDM_CAPTUREJPEG, false);
	CheckMenuItem(IDM_CAPTUREGIF, false);
	CheckMenuItem(IDM_CAPTUREPNG, false);
	CheckMenuItem(m_MenuIDCaptureResolution, true);
	CheckMenuItem(m_MenuIDCaptureFormat, true);

	// Edit
	CheckMenuItem(IDM_EDIT_CRLF, m_translateCRLF);

	// Comms -> Tape Speed
	SetTapeSpeedMenu();

	// Comms
	CheckMenuItem(ID_UNLOCKTAPE, TapeState.Unlock);
	CheckMenuItem(IDM_PRINTERONOFF, PrinterEnabled);

	// Comms -> Printer
	CheckMenuItem(IDM_PRINTER_FILE, false);
	CheckMenuItem(IDM_PRINTER_LPT1, false);
	CheckMenuItem(IDM_PRINTER_LPT2, false);
	CheckMenuItem(IDM_PRINTER_LPT3, false);
	CheckMenuItem(IDM_PRINTER_LPT4, false);
	CheckMenuItem(IDM_PRINTER_COM1, false);
	CheckMenuItem(IDM_PRINTER_COM2, false);
	CheckMenuItem(IDM_PRINTER_COM3, false);
	CheckMenuItem(IDM_PRINTER_COM4, false);
	CheckMenuItem(m_MenuIDPrinterPort, true);
	strcpy(menu_string, "File: ");
	strcat(menu_string, m_PrinterFileName);
	ModifyMenu(m_hMenu, IDM_PRINTER_FILE, MF_BYCOMMAND, IDM_PRINTER_FILE, menu_string);

	// Comms -> RS423
	UpdateSerialMenu();

	// View
	UpdateDisplayRendererMenu();

	const bool DirectXEnabled = m_DisplayRenderer != IDM_DISPGDI;
	EnableMenuItem(IDM_DXSMOOTHING, DirectXEnabled);
	EnableMenuItem(IDM_DXSMOOTHMODE7ONLY, DirectXEnabled);

	CheckMenuItem(IDM_DXSMOOTHING, m_DXSmoothing);
	CheckMenuItem(IDM_DXSMOOTHMODE7ONLY, m_DXSmoothMode7Only);

	CheckMenuItem(IDM_SPEEDANDFPS, m_ShowSpeedAndFPS);
	CheckMenuItem(IDM_FULLSCREEN, m_isFullScreen);
	CheckMenuItem(IDM_MAINTAINASPECTRATIO, m_MaintainAspectRatio);
	UpdateMonitorMenu();
	CheckMenuItem(ID_HIDEMENU, m_HideMenuEnabled);
	UpdateLEDMenu();
	CheckMenuItem(IDM_TEXTVIEW, m_TextViewEnabled);

	// View -> Win size
	CheckMenuItem(IDM_320X256, false);
	CheckMenuItem(IDM_640X512, false);
	CheckMenuItem(IDM_800X600, false);
	CheckMenuItem(IDM_1024X768, false);
	CheckMenuItem(IDM_1024X512, false);
	CheckMenuItem(IDM_1280X1024, false);
	CheckMenuItem(IDM_1440X1080, false);
	CheckMenuItem(IDM_1600X1200, false);
	// CheckMenuItem(m_MenuIdWinSize, true);

	// View -> DD mode
	CheckMenuItem(ID_VIEW_DD_SCREENRES, false);
	CheckMenuItem(ID_VIEW_DD_640X480, false);
	CheckMenuItem(ID_VIEW_DD_720X576, false);
	CheckMenuItem(ID_VIEW_DD_800X600, false);
	CheckMenuItem(ID_VIEW_DD_1024X768, false);
	CheckMenuItem(ID_VIEW_DD_1280X720, false);
	CheckMenuItem(ID_VIEW_DD_1280X1024, false);
	CheckMenuItem(ID_VIEW_DD_1280X768, false);
	CheckMenuItem(ID_VIEW_DD_1280X960, false);
	CheckMenuItem(ID_VIEW_DD_1440X900, false);
	CheckMenuItem(ID_VIEW_DD_1600X1200, false);
	CheckMenuItem(ID_VIEW_DD_1920X1080, false);
	CheckMenuItem(ID_VIEW_DD_2560X1440, false);
	CheckMenuItem(ID_VIEW_DD_3840X2160, false);
	CheckMenuItem(m_DDFullScreenMode, true);

	// View -> Motion blur
	CheckMenuItem(IDM_BLUR_OFF, false);
	CheckMenuItem(IDM_BLUR_2, false);
	CheckMenuItem(IDM_BLUR_4, false);
	CheckMenuItem(IDM_BLUR_8, false);
	CheckMenuItem(m_MenuIDMotionBlur, true);

	// Speed
	CheckMenuItem(IDM_REALTIME, false);
	CheckMenuItem(IDM_FIXEDSPEED100, false);
	CheckMenuItem(IDM_FIXEDSPEED50, false);
	CheckMenuItem(IDM_FIXEDSPEED10, false);
	CheckMenuItem(IDM_FIXEDSPEED5, false);
	CheckMenuItem(IDM_FIXEDSPEED2, false);
	CheckMenuItem(IDM_FIXEDSPEED1_5, false);
	CheckMenuItem(IDM_FIXEDSPEED1_25, false);
	CheckMenuItem(IDM_FIXEDSPEED1_1, false);
	CheckMenuItem(IDM_FIXEDSPEED0_9, false);
	CheckMenuItem(IDM_FIXEDSPEED0_5, false);
	CheckMenuItem(IDM_FIXEDSPEED0_75, false);
	CheckMenuItem(IDM_FIXEDSPEED0_25, false);
	CheckMenuItem(IDM_FIXEDSPEED0_1, false);
	CheckMenuItem(IDM_50FPS, false);
	CheckMenuItem(IDM_25FPS, false);
	CheckMenuItem(IDM_10FPS, false);
	CheckMenuItem(IDM_5FPS, false);
	CheckMenuItem(IDM_1FPS, false);
	CheckMenuItem(m_MenuIDTiming, true);
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
	CheckMenuItem(ID_TAPESOUND, TapeSoundEnabled);
	CheckMenuItem(IDM_44100KHZ, false);
	CheckMenuItem(IDM_22050KHZ, false);
	CheckMenuItem(IDM_11025KHZ, false);
	CheckMenuItem(m_MenuIDSampleRate, true);
	CheckMenuItem(IDM_HIGHVOLUME, false);
	CheckMenuItem(IDM_MEDIUMVOLUME, false);
	CheckMenuItem(IDM_LOWVOLUME, false);
	CheckMenuItem(IDM_FULLVOLUME, false);
	CheckMenuItem(m_MenuIDVolume, true);
	CheckMenuItem(ID_PSAMPLES, PartSamples);
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
	CheckMenuItem(IDM_AMX_320X256, false);
	CheckMenuItem(IDM_AMX_640X256, false);
	CheckMenuItem(IDM_AMX_160X256, false);
	CheckMenuItem(m_MenuIDAMXSize, true);
	CheckMenuItem(IDM_AMX_ADJUSTP50, false);
	CheckMenuItem(IDM_AMX_ADJUSTP30, false);
	CheckMenuItem(IDM_AMX_ADJUSTP10, false);
	CheckMenuItem(IDM_AMX_ADJUSTM10, false);
	CheckMenuItem(IDM_AMX_ADJUSTM30, false);
	CheckMenuItem(IDM_AMX_ADJUSTM50, false);
	if (m_MenuIDAMXAdjust != 0)
		CheckMenuItem(m_MenuIDAMXAdjust, true);

	// Hardware -> Model
	UpdateModelMenu();

	// Hardware
	UpdateTubeMenu();

	SetRomMenu();
	CheckMenuItem(IDM_SWRAMBOARD, SWRAMBoardEnabled);
	UpdateOptiMenu();
	UpdateEconetMenu();
	CheckMenuItem(ID_TELETEXT, TeletextAdapterEnabled);
	CheckMenuItem(ID_TELETEXTFILES, TeletextFiles);
	CheckMenuItem(ID_TELETEXTLOCALHOST, TeletextLocalhost);
	CheckMenuItem(ID_TELETEXTCUSTOM, TeletextCustom);
	CheckMenuItem(ID_FLOPPYDRIVE, Disc8271Enabled);
	CheckMenuItem(ID_HARDDRIVE, SCSIDriveEnabled);
	CheckMenuItem(ID_IDEDRIVE, IDEDriveEnabled);
	CheckMenuItem(ID_USER_PORT_RTC_MODULE, RTC_Enabled);

	// Options
	CheckMenuItem(IDM_JOYSTICK, false);
	CheckMenuItem(IDM_ANALOGUE_MOUSESTICK, false);
	CheckMenuItem(IDM_DIGITAL_MOUSESTICK, false);
	if (m_MenuIDSticks != 0)
		CheckMenuItem(m_MenuIDSticks, true);
	CheckMenuItem(IDM_FREEZEINACTIVE, m_FreezeWhenInactive);
	CheckMenuItem(IDM_HIDECURSOR, m_HideCursor);
	CheckMenuItem(IDM_DEFAULTKYBDMAPPING, false);
	CheckMenuItem(IDM_LOGICALKYBDMAPPING, false);
	CheckMenuItem(IDM_USERKYBDMAPPING, false);
	CheckMenuItem(m_MenuIDKeyMapping, true);
	CheckMenuItem(IDM_MAPAS, m_KeyMapAS);
	CheckMenuItem(IDM_MAPFUNCS, m_KeyMapFunc);
	UpdateDisableKeysMenu();
	CheckMenuItem(IDM_AUTOSAVE_PREFS_CMOS, m_AutoSavePrefsCMOS);
	CheckMenuItem(IDM_AUTOSAVE_PREFS_FOLDERS, m_AutoSavePrefsFolders);
	CheckMenuItem(IDM_AUTOSAVE_PREFS_ALL, m_AutoSavePrefsAll);
}

void BeebWin::UpdateDisplayRendererMenu()
{
	CheckMenuItem(IDM_DISPGDI, m_DisplayRenderer == IDM_DISPGDI);
	CheckMenuItem(IDM_DISPDDRAW, m_DisplayRenderer == IDM_DISPDDRAW);
	CheckMenuItem(IDM_DISPDX9, m_DisplayRenderer == IDM_DISPDX9);
}

void BeebWin::UpdateSoundStreamerMenu()
{
	CheckMenuItem(IDM_XAUDIO2, SoundConfig::Selection == SoundConfig::XAudio2);
	CheckMenuItem(IDM_DIRECTSOUND, SoundConfig::Selection == SoundConfig::DirectSound);
}

void BeebWin::UpdateMonitorMenu()
{
	CheckMenuItem(ID_MONITOR_RGB, m_PaletteType == PaletteType::RGB);
	CheckMenuItem(ID_MONITOR_BW, m_PaletteType == PaletteType::BW);
	CheckMenuItem(ID_MONITOR_GREEN, m_PaletteType == PaletteType::Green);
	CheckMenuItem(ID_MONITOR_AMBER, m_PaletteType == PaletteType::Amber);
}

void BeebWin::UpdateModelMenu()
{
	static const std::map<Model, UINT> ModelMenuItems{
		{ Model::B,         ID_MODELB },
		{ Model::IntegraB,  ID_MODELBINT },
		{ Model::BPlus,     ID_MODELBPLUS },
		{ Model::Master128, ID_MASTER128 },
	};

	UINT SelectedMenuItem = ModelMenuItems.find(MachineType)->second;

	CheckMenuRadioItem(
		m_hMenu,
		ID_MODELB,
		ID_MASTER128,
		SelectedMenuItem,
		MF_BYCOMMAND
	);

	if (MachineType == Model::Master128) {
		EnableMenuItem(ID_FDC_DLL, false);
	}
	else {
		EnableMenuItem(ID_FDC_DLL, true);
	}
}

void BeebWin::UpdateSFXMenu() {
	CheckMenuItem(ID_SFX_RELAY, RelaySoundEnabled);
	CheckMenuItem(ID_SFX_DISCDRIVES, DiscDriveSoundEnabled);
}

void BeebWin::UpdateDisableKeysMenu() {
	CheckMenuItem(IDM_DISABLEKEYSWINDOWS, m_DisableKeysWindows);
	CheckMenuItem(IDM_DISABLEKEYSBREAK, m_DisableKeysBreak);
	CheckMenuItem(IDM_DISABLEKEYSESCAPE, m_DisableKeysEscape);
	CheckMenuItem(IDM_DISABLEKEYSSHORTCUT, m_DisableKeysShortcut);
}

/****************************************************************************/

void BeebWin::UpdateTubeMenu()
{
	static const std::map<Tube, UINT> TubeMenuItems{
		{ Tube::None,           IDM_TUBE_NONE },
		{ Tube::Acorn65C02,     IDM_TUBE_ACORN65C02 },
		{ Tube::Master512CoPro, IDM_TUBE_MASTER512 },
		{ Tube::AcornZ80,       IDM_TUBE_ACORNZ80 },
		{ Tube::TorchZ80,       IDM_TUBE_TORCHZ80 },
		{ Tube::AcornArm,       IDM_TUBE_ACORNARM },
		{ Tube::SprowArm,       IDM_TUBE_SPROWARM }
	};

	UINT SelectedMenuItem = TubeMenuItems.find(TubeType)->second;

	CheckMenuRadioItem(
		m_hMenu,
		IDM_TUBE_NONE,
		IDM_TUBE_SPROWARM,
		SelectedMenuItem,
		MF_BYCOMMAND
	);
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
	if (m_MenuIDSticks == IDM_JOYSTICK)
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
void BeebWin::SetMousestickButton(int index, bool button)
{
	if (m_MenuIDSticks == IDM_ANALOGUE_MOUSESTICK ||
	    m_MenuIDSticks == IDM_DIGITAL_MOUSESTICK)
	{
		JoystickButton[index] = button;
	}
}

/****************************************************************************/
void BeebWin::ScaleMousestick(unsigned int x, unsigned int y)
{
	static int lastx = 32768;
	static int lasty = 32768;

	if (m_MenuIDSticks == IDM_ANALOGUE_MOUSESTICK)
	{
		JoystickX = (m_XWinSize - x) * 65535 / m_XWinSize;
		JoystickY = (m_YWinSize - y) * 65535 / m_YWinSize;
	}
	else if (m_MenuIDSticks == IDM_DIGITAL_MOUSESTICK)
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
	if (AMXMouseEnabled)
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
	if (AMXMouseEnabled)
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
			{
				int wmId = LOWORD(wParam);
				HandleCommand(wmId);
			}
			break;

		case WM_PAINT:
			{
				PAINTSTRUCT ps;
				HDC hDC = BeginPaint(m_hWnd, &ps);
				updateLines(hDC, 0, 0);
				EndPaint(m_hWnd, &ps);

				if (m_DXResetPending)
				{
					ResetDX();
				}
			}
			break;

		case WM_SIZE:
			WinSizeChange(wParam, LOWORD(lParam), HIWORD(lParam));
			break;

		case WM_MOVE:
			WinPosChange(LOWORD(lParam), HIWORD(lParam));
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
			ReinitDX();
			break;

		case WM_TIMER:
			if (wParam == 1)
			{
				HandleTimer();
			}
			else if (wParam == 2) // Handle timer for automatic disc boot delay
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
	int colbase = static_cast<int>(DiscLedColour) * 2 + LED_COL_BASE; // colour will be 0 for red, 1 for green.
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
	case ID_VIEW_DD_2560X1440:
		m_XDXSize = 2560;
		m_YDXSize = 1440;
		break;
	case ID_VIEW_DD_3840X2160:
		m_XDXSize = 3840;
		m_YDXSize = 2160;
		break;
	case ID_VIEW_DD_SCREENRES:
		// Pixel size of default monitor
		m_XDXSize = GetSystemMetrics(SM_CXSCREEN);
		m_YDXSize = GetSystemMetrics(SM_CYSCREEN);
		break;
	}
}

/****************************************************************************/
void BeebWin::TranslateWindowSize(void)
{
	switch (m_MenuIDWinSize)
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

	m_MenuIDWinSize = IDM_CUSTOMWINSIZE;
}

/****************************************************************************/
void BeebWin::TranslateSampleRate(void)
{
	switch (m_MenuIDSampleRate)
	{
	default:
	case IDM_44100KHZ:
		SoundSampleRate = 44100;
		break;

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
	switch (m_MenuIDVolume)
	{
	default:
	case IDM_FULLVOLUME:
		SoundVolume = 100;
		break;

	case IDM_HIGHVOLUME:
		SoundVolume = 75;
		break;

	case IDM_MEDIUMVOLUME:
		SoundVolume = 50;
		break;

	case IDM_LOWVOLUME:
		SoundVolume = 25;
		break;
	}
}

/****************************************************************************/

void BeebWin::TranslateTiming()
{
	m_FPSTarget = 0;
	SetRealTimeTarget(1.0);

	if (m_MenuIDTiming == IDM_3QSPEED)
		m_MenuIDTiming = IDM_FIXEDSPEED0_75;
	if (m_MenuIDTiming == IDM_HALFSPEED)
		m_MenuIDTiming = IDM_FIXEDSPEED0_5;

	switch (m_MenuIDTiming)
	{
	default:
	case IDM_REALTIME:
		SetRealTimeTarget(1.0);
		m_FPSTarget = 0;
		break;

	case IDM_FIXEDSPEED100:
		SetRealTimeTarget(100.0);
		m_FPSTarget = 0;
		break;

	case IDM_FIXEDSPEED50:
		SetRealTimeTarget(50.0);
		m_FPSTarget = 0;
		break;

	case IDM_FIXEDSPEED10:
		SetRealTimeTarget(10.0);
		m_FPSTarget = 0;
		break;

	case IDM_FIXEDSPEED5:
		SetRealTimeTarget(5.0);
		m_FPSTarget = 0;
		break;

	case IDM_FIXEDSPEED2:
		SetRealTimeTarget(2.0);
		m_FPSTarget = 0;
		break;

	case IDM_FIXEDSPEED1_5:
		SetRealTimeTarget(1.5);
		m_FPSTarget = 0;
		break;

	case IDM_FIXEDSPEED1_25:
		SetRealTimeTarget(1.25);
		m_FPSTarget = 0;
		break;

	case IDM_FIXEDSPEED1_1:
		SetRealTimeTarget(1.1);
		m_FPSTarget = 0;
		break;

	case IDM_FIXEDSPEED0_9:
		SetRealTimeTarget(0.9);
		m_FPSTarget = 0;
		break;

	case IDM_FIXEDSPEED0_5:
		SetRealTimeTarget(0.5);
		m_FPSTarget = 0;
		break;

	case IDM_FIXEDSPEED0_75:
		SetRealTimeTarget(0.75);
		m_FPSTarget = 0;
		break;

	case IDM_FIXEDSPEED0_25:
		SetRealTimeTarget(0.25);
		m_FPSTarget = 0;
		break;

	case IDM_FIXEDSPEED0_1:
		SetRealTimeTarget(0.1);
		m_FPSTarget = 0;
		break;

	case IDM_50FPS:
		m_FPSTarget = 50;
		SetRealTimeTarget(0.0);
		break;

	case IDM_25FPS:
		m_FPSTarget = 25;
		SetRealTimeTarget(0.0);
		break;

	case IDM_10FPS:
		m_FPSTarget = 10;
		SetRealTimeTarget(0.0);
		break;

	case IDM_5FPS:
		m_FPSTarget = 5;
		SetRealTimeTarget(0.0);
		break;

	case IDM_1FPS:
		m_FPSTarget = 1;
		SetRealTimeTarget(0.0);
		break;
	}

	ResetTiming();
}

void BeebWin::SetRealTimeTarget(double RealTimeTarget)
{
	m_RealTimeTarget = RealTimeTarget;
	m_CyclesPerSec = (int)(2000000.0 * m_RealTimeTarget);
}

void BeebWin::AdjustSpeed(bool up)
{
	static const UINT speeds[] = {
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
		0
	};

	int s = 0;
	UINT MenuItemID = m_MenuIDTiming;

	while (speeds[s] != 0 && speeds[s] != m_MenuIDTiming)
		s++;

	if (speeds[s] == 0)
	{
		MenuItemID = IDM_REALTIME;
	}
	else if (up)
	{
		if (s > 0)
			MenuItemID = speeds[s - 1];
	}
	else
	{
		if (speeds[s + 1] != 0)
			MenuItemID = speeds[s + 1];
	}

	if (MenuItemID != m_MenuIDTiming)
	{
		CheckMenuItem(m_MenuIDTiming, false);
		m_MenuIDTiming = MenuItemID;
		CheckMenuItem(m_MenuIDTiming, true);
		TranslateTiming();
	}
}

/****************************************************************************/
void BeebWin::TranslateKeyMapping(void)
{
	switch (m_MenuIDKeyMapping)
	{
	default:
	case IDM_DEFAULTKYBDMAPPING:
		transTable = &DefaultKeyMap;
		break;

	case IDM_LOGICALKYBDMAPPING:
		transTable = &LogicalKeyMap;
		break;

	case IDM_USERKYBDMAPPING:
		transTable = &UserKeyMap;
		break;
	}
}

/****************************************************************************/

void BeebWin::SetTapeSpeedMenu()
{
	CheckMenuItem(ID_TAPE_FAST, TapeState.ClockSpeed == 750);
	CheckMenuItem(ID_TAPE_MFAST, TapeState.ClockSpeed == 1600);
	CheckMenuItem(ID_TAPE_MSLOW, TapeState.ClockSpeed == 3200);
	CheckMenuItem(ID_TAPE_NORMAL, TapeState.ClockSpeed == 5600);
}

/****************************************************************************/

void BeebWin::SetUnlockTape(bool Unlock)
{
	TapeState.Unlock = Unlock;
	::SetUnlockTape(TapeState.Unlock);

	CheckMenuItem(ID_UNLOCKTAPE, TapeState.Unlock);

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
		// Get the monitor that the BeebEm window is on to account for multiple monitors
		if (m_DDFullScreenMode == ID_VIEW_DD_SCREENRES)
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
			GetWindowRect(m_hWnd, &wndrect);
			m_XWinPos = wndrect.left;
			m_YWinPos = wndrect.top;
		}

		if (m_DisplayRenderer != IDM_DISPGDI)
		{
			m_XWinSize = m_XDXSize;
			m_YWinSize = m_YDXSize;
			CalcAspectRatioAdjustment(m_XDXSize, m_YDXSize);

			style = GetWindowLong(m_hWnd, GWL_STYLE);
			style &= ~WIN_STYLE;
			style |= WS_POPUP;
			SetWindowLong(m_hWnd, GWL_STYLE, style);

			if (m_DXInit)
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

		if (m_DisplayRenderer != IDM_DISPGDI && m_DXInit)
		{
			ResetDX();
		}

		m_XWinSize = xs;
		m_YWinSize = ys;

		style = GetWindowLong(m_hWnd, GWL_STYLE);
		style &= ~WS_POPUP;
		style |= WIN_STYLE;
		SetWindowLong(m_hWnd, GWL_STYLE, style);

		SetWindowPos(m_hWnd,
		             HWND_TOP,
		             m_XWinPos,
		             m_YWinPos,
		             m_XWinSize + m_XWinBorder,
		             m_YWinSize + m_YWinBorder,
		             !wasFullScreen ? SWP_NOMOVE : 0);

		// Experiment: hide menu in full screen
		HideMenu(false);
	}

	// Clear unused areas of screen
	RECT rc;
	GetClientRect(m_hWnd, &rc);
	InvalidateRect(m_hWnd, &rc, TRUE);
}

/*****************************************************************************/
void BeebWin::WinSizeChange(WPARAM size, int width, int height)
{
	if (m_DisplayRenderer == IDM_DISPGDI && size == SIZE_RESTORED && m_isFullScreen)
	{
		m_isFullScreen = false;
		CheckMenuItem(IDM_FULLSCREEN, false);
	}

	if (!m_isFullScreen || m_DisplayRenderer == IDM_DISPGDI)
	{
		m_XWinSize = width;
		m_YWinSize = height;
		CalcAspectRatioAdjustment(m_XWinSize, m_YWinSize);

		if (size != SIZE_MINIMIZED && m_DisplayRenderer != IDM_DISPGDI && m_DXInit)
		{
			m_DXResetPending = true;
		}
	}

	if (!m_isFullScreen)
	{
		m_XLastWinSize = m_XWinSize;
		m_YLastWinSize = m_YWinSize;
	}

	if (m_hTextView)
	{
		MoveWindow(m_hTextView, 0, 0, width, height, TRUE);
	}
}

/****************************************************************************/
void BeebWin::WinPosChange(int /* x */, int /* y */)
{
	// DebugTrace("WM_MOVE %d, %d (%d, %d)\n", x, y, m_XWinPos, m_YWinPos);
}

/****************************************************************************/
void BeebWin::TranslateAMX(void)
{
	switch (m_MenuIDAMXSize)
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

	switch (m_MenuIDAMXAdjust)
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
	CheckMenuItem(ID_SERIAL, SerialPortEnabled);
}

//Rob
void BeebWin::UpdateEconetMenu()
{
	CheckMenuItem(ID_ECONET, EconetEnabled);
}

void BeebWin::UpdateLEDMenu()
{
	// Update the LED Menu
	CheckMenuItem(ID_RED_LEDS, DiscLedColour == LEDColour::Red);
	CheckMenuItem(ID_GREEN_LEDS, DiscLedColour == LEDColour::Green);
	CheckMenuItem(ID_SHOW_KBLEDS, LEDs.ShowKB);
	CheckMenuItem(ID_SHOW_DISCLEDS, LEDs.ShowDisc);
}

void BeebWin::UpdateOptiMenu()
{
	CheckMenuItem(ID_BASIC_HARDWARE_ONLY, BasicHardwareOnly);
	CheckMenuItem(ID_TELETEXTHALFMODE, TeletextHalfMode);
	CheckMenuItem(ID_PSAMPLES, PartSamples);
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
		m_translateCRLF = !m_translateCRLF;
		CheckMenuItem(IDM_EDIT_CRLF, m_translateCRLF);
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
			ModifyMenu(m_hMenu, IDM_PRINTER_FILE,
				MF_BYCOMMAND, IDM_PRINTER_FILE,
				menu_string);

			if (MenuID != m_MenuIDPrinterPort)
			{
				CheckMenuItem(m_MenuIDPrinterPort, false);
				m_MenuIDPrinterPort = MenuID;
				CheckMenuItem(m_MenuIDPrinterPort, true);
			}
			TranslatePrinterPort();
		}
		break;

	case IDM_PRINTER_CLIPBOARD:
		if (PrinterEnabled)
			TogglePrinter();

		if (MenuID != m_MenuIDPrinterPort)
		{
			CheckMenuItem(m_MenuIDPrinterPort, false);
			m_MenuIDPrinterPort = MenuID;
			CheckMenuItem(m_MenuIDPrinterPort, true);
		}
		TranslatePrinterPort();
		break;

	case IDM_PRINTER_LPT1:
	case IDM_PRINTER_LPT2:
	case IDM_PRINTER_LPT3:
	case IDM_PRINTER_LPT4:
		if (MenuID != m_MenuIDPrinterPort)
		{
			/* If printer is enabled then need to
				disable it before changing file */
			if (PrinterEnabled)
				TogglePrinter();

			CheckMenuItem(m_MenuIDPrinterPort, false);
			m_MenuIDPrinterPort = MenuID;
			CheckMenuItem(m_MenuIDPrinterPort, true);
			TranslatePrinterPort();
		}
		break;

	case IDM_PRINTERONOFF:
		TogglePrinter();
		break;

	case ID_SERIAL:
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

	case ID_SELECT_SERIAL_DESTINATION: {
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
					if (m_MenuIDSticks != IDM_ANALOGUE_MOUSESTICK)
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
	case ID_ECONET:
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
	case IDM_DISPDDRAW:
	case IDM_DISPDX9:
	{
		ExitDX();

		m_DisplayRenderer = MenuID;
		SetWindowAttributes(m_isFullScreen);

		bool DirectXEnabled = m_DisplayRenderer != IDM_DISPGDI;

		if (DirectXEnabled)
		{
			InitDX();
		}

		UpdateDisplayRendererMenu();
		EnableMenuItem(IDM_DXSMOOTHING, DirectXEnabled);
		EnableMenuItem(IDM_DXSMOOTHMODE7ONLY, DirectXEnabled);
		break;
	}

	case IDM_DXSMOOTHING:
		m_DXSmoothing = !m_DXSmoothing;
		CheckMenuItem(IDM_DXSMOOTHING, m_DXSmoothing);
		if (m_DisplayRenderer != IDM_DISPGDI)
		{
			UpdateSmoothing();
		}
		break;

	case IDM_DXSMOOTHMODE7ONLY:
		m_DXSmoothMode7Only = !m_DXSmoothMode7Only;
		CheckMenuItem(IDM_DXSMOOTHMODE7ONLY, m_DXSmoothMode7Only);
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
			// CheckMenuItem(m_MenuIdWinSize, false);
			m_MenuIDWinSize = MenuID;
			// CheckMenuItem(m_MenuIdWinSize, true);
			TranslateWindowSize();
			SetWindowAttributes(m_isFullScreen);
		}
		break;

	case ID_VIEW_DD_SCREENRES:
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
	case ID_VIEW_DD_2560X1440:
	case ID_VIEW_DD_3840X2160:
		// Ignore ID_VIEW_DD_SCREENRES if already in full screen mode
		if ((MenuID != ID_VIEW_DD_SCREENRES) || !m_isFullScreen)
		{
			CheckMenuItem(m_DDFullScreenMode, false);
			m_DDFullScreenMode = MenuID;
			CheckMenuItem(m_DDFullScreenMode, true);
			TranslateDDSize();

			if (m_isFullScreen && m_DisplayRenderer != IDM_DISPGDI)
			{
				SetWindowAttributes(m_isFullScreen);
			}
		}
		break;

	case IDM_FULLSCREEN:
		m_isFullScreen = !m_isFullScreen;
		CheckMenuItem(IDM_FULLSCREEN, m_isFullScreen);
		SetWindowAttributes(!m_isFullScreen);
		if (m_MouseCaptured)
			ReleaseMouse();
		break;

	case IDM_MAINTAINASPECTRATIO:
		m_MaintainAspectRatio = !m_MaintainAspectRatio;
		CheckMenuItem(IDM_MAINTAINASPECTRATIO, m_MaintainAspectRatio);
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
	case IDM_DIRECTSOUND:
		SoundConfig::Selection = MenuID == IDM_XAUDIO2 ? SoundConfig::XAudio2 : SoundConfig::DirectSound;

		if (SoundEnabled)
		{
			SoundReset();
			SoundInit();
		}

		if (Music5000Enabled)
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

	case ID_SFX_RELAY:
		RelaySoundEnabled = !RelaySoundEnabled;
		CheckMenuItem(ID_SFX_RELAY, RelaySoundEnabled);
		break;

	case ID_SFX_DISCDRIVES:
		DiscDriveSoundEnabled = !DiscDriveSoundEnabled;
		CheckMenuItem(ID_SFX_DISCDRIVES, DiscDriveSoundEnabled);
		break;

	case IDM_44100KHZ:
	case IDM_22050KHZ:
	case IDM_11025KHZ:
		if (MenuID != m_MenuIDSampleRate)
		{
			CheckMenuItem(m_MenuIDSampleRate, false);
			m_MenuIDSampleRate = MenuID;
			CheckMenuItem(m_MenuIDSampleRate, true);
			TranslateSampleRate();

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
		break;

	case IDM_FULLVOLUME:
	case IDM_HIGHVOLUME:
	case IDM_MEDIUMVOLUME:
	case IDM_LOWVOLUME:
		if (MenuID != m_MenuIDVolume)
		{
			CheckMenuItem(m_MenuIDVolume, false);
			m_MenuIDVolume = MenuID;
			CheckMenuItem(m_MenuIDVolume, true);
			TranslateVolume();
		}
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
		if (MenuID != m_MenuIDTiming)
		{
			CheckMenuItem(m_MenuIDTiming, false);
			m_MenuIDTiming = MenuID;
			CheckMenuItem(m_MenuIDTiming, true);
			TranslateTiming();
		}
		break;

	case IDM_EMUPAUSED:
		TogglePause();
		break;

	case IDM_JOYSTICK:
	case IDM_ANALOGUE_MOUSESTICK:
	case IDM_DIGITAL_MOUSESTICK:
		/* Disable current selection */
		if (m_MenuIDSticks != 0)
		{
			CheckMenuItem(m_MenuIDSticks, false);

			if (m_MenuIDSticks == IDM_JOYSTICK)
			{
				ResetJoystick();
			}
			else /* mousestick */
			{
				AtoDDisable();
			}
		}

		if (MenuID == m_MenuIDSticks)
		{
			/* Joysticks switched off completely */
			m_MenuIDSticks = 0;
		}
		else
		{
			/* Initialise new selection */
			m_MenuIDSticks = MenuID;

			if (m_MenuIDSticks == IDM_JOYSTICK)
			{
				InitJoystick();
			}
			else /* mousestick */
			{
				AtoDEnable();
			}

			if (JoystickEnabled)
				CheckMenuItem(m_MenuIDSticks, true);
			else
				m_MenuIDSticks = 0;
		}
		break;

	case IDM_FREEZEINACTIVE:
		m_FreezeWhenInactive = !m_FreezeWhenInactive;
		CheckMenuItem(IDM_FREEZEINACTIVE, m_FreezeWhenInactive);
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
	case IDM_DEFAULTKYBDMAPPING:
	case IDM_LOGICALKYBDMAPPING:
		if (MenuID != m_MenuIDKeyMapping)
		{
			CheckMenuItem(m_MenuIDKeyMapping, false);
			m_MenuIDKeyMapping = MenuID;
			CheckMenuItem(m_MenuIDKeyMapping, true);
			TranslateKeyMapping();
		}
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
		PostMessage(m_hWnd, WM_CLOSE, 0, 0L);
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
	case IDM_AMX_320X256:
	case IDM_AMX_640X256:
		if (MenuID != m_MenuIDAMXSize)
		{
			CheckMenuItem(m_MenuIDAMXSize, false);
			m_MenuIDAMXSize = MenuID;
			CheckMenuItem(m_MenuIDAMXSize, true);
		}
		TranslateAMX();
		break;

	case IDM_AMX_ADJUSTP50:
	case IDM_AMX_ADJUSTP30:
	case IDM_AMX_ADJUSTP10:
	case IDM_AMX_ADJUSTM10:
	case IDM_AMX_ADJUSTM30:
	case IDM_AMX_ADJUSTM50:
		if (m_MenuIDAMXAdjust != 0)
		{
			CheckMenuItem(m_MenuIDAMXAdjust, false);
		}

		if (MenuID != m_MenuIDAMXAdjust)
		{
			m_MenuIDAMXAdjust = MenuID;
			CheckMenuItem(m_MenuIDAMXAdjust, true);
		}
		else
		{
			m_MenuIDAMXAdjust = 0;
		}
		TranslateAMX();
		break;

	case ID_MONITOR_RGB:
		m_PaletteType = PaletteType::RGB;
		CreateBitmap();
		break;

	case ID_MONITOR_BW:
		m_PaletteType = PaletteType::BW;
		CreateBitmap();
		break;

	case ID_MONITOR_GREEN:
		m_PaletteType = PaletteType::Green;
		CreateBitmap();
		break;

	case ID_MONITOR_AMBER:
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

	case ID_FILE_RESET:
		ResetBeebSystem(MachineType, false);
		break;

	case ID_MODELB:
		if (MachineType != Model::B)
		{
			ResetBeebSystem(Model::B, true);
			UpdateModelMenu();
		}
		break;

	case ID_MODELBINT:
		if (MachineType != Model::IntegraB)
		{
			ResetBeebSystem(Model::IntegraB, true);
			UpdateModelMenu();
		}
		break;

	case ID_MODELBPLUS:
		if (MachineType != Model::BPlus)
		{
			ResetBeebSystem(Model::BPlus, true);
			UpdateModelMenu();
		}
		break;

	case ID_MASTER128:
		if (MachineType != Model::Master128)
		{
			ResetBeebSystem(Model::Master128, true);
			UpdateModelMenu();
		}
		break;

	case ID_REWINDTAPE:
		RewindTape();
		break;

	case ID_UNLOCKTAPE:
		SetUnlockTape(!TapeState.Unlock);
		break;

	case ID_HIDEMENU:
		m_HideMenuEnabled = !m_HideMenuEnabled;
		CheckMenuItem(ID_HIDEMENU, m_HideMenuEnabled);
		break;

	case ID_RED_LEDS:
		DiscLedColour = LEDColour::Red;
		UpdateLEDMenu();
		break;

	case ID_GREEN_LEDS:
		DiscLedColour = LEDColour::Green;
		UpdateLEDMenu();
		break;

	case ID_SHOW_KBLEDS:
		LEDs.ShowKB = !LEDs.ShowKB;
		UpdateLEDMenu();
		break;

	case ID_SHOW_DISCLEDS:
		LEDs.ShowDisc = !LEDs.ShowDisc;
		UpdateLEDMenu();
		break;

	case ID_FDC_DLL:
		if (MachineType != Model::Master128)
			SelectFDC();
		break;

	case ID_8271:
		KillDLLs();
		NativeFDC = true;

		CheckMenuItem(ID_8271, true);
		CheckMenuItem(ID_FDC_DLL, false);

		if (MachineType != Model::Master128)
		{
			char CfgName[20];
			sprintf(CfgName, "FDCDLL%d", static_cast<int>(MachineType));
			m_Preferences.SetStringValue(CfgName, "None");
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
		TapeSoundEnabled = !TapeSoundEnabled;
		CheckMenuItem(ID_TAPESOUND, TapeSoundEnabled);
		break;

	case ID_TAPECONTROL:
		if (TapeControlEnabled)
		{
			TapeControlCloseDialog();
		}
		else
		{
			TapeControlOpenDialog(hInst, m_hWnd);
		}
		break;

	case ID_BREAKOUT:
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

	case ID_USER_PORT_RTC_MODULE:
		RTC_Enabled = !RTC_Enabled;
		CheckMenuItem(ID_USER_PORT_RTC_MODULE, RTC_Enabled);
		break;

	case ID_TELETEXTHALFMODE:
		TeletextHalfMode = !TeletextHalfMode;
		UpdateOptiMenu();
		break;

	case ID_BASIC_HARDWARE_ONLY:
		BasicHardwareOnly = !BasicHardwareOnly;
		UpdateOptiMenu();
		break;

	case ID_PSAMPLES:
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
			DebugOpenDialog(hInst, m_hWnd);
		break;

	case IDM_BLUR_OFF:
	case IDM_BLUR_2:
	case IDM_BLUR_4:
	case IDM_BLUR_8:
		if (MenuID != m_MenuIDMotionBlur)
		{
			CheckMenuItem(m_MenuIDMotionBlur, false);
			m_MenuIDMotionBlur = MenuID;
			CheckMenuItem(m_MenuIDMotionBlur, true);
		}
		break;

	case IDM_CAPTURERES_DISPLAY:
	case IDM_CAPTURERES_1280:
	case IDM_CAPTURERES_640:
	case IDM_CAPTURERES_320:
		if (MenuID != m_MenuIDCaptureResolution)
		{
			CheckMenuItem(m_MenuIDCaptureResolution, false);
			m_MenuIDCaptureResolution = MenuID;
			CheckMenuItem(m_MenuIDCaptureResolution, true);
		}
		break;

	case IDM_CAPTUREBMP:
	case IDM_CAPTUREJPEG:
	case IDM_CAPTUREGIF:
	case IDM_CAPTUREPNG:
		if (MenuID != m_MenuIDCaptureFormat)
		{
			CheckMenuItem(m_MenuIDCaptureFormat, false);
			m_MenuIDCaptureFormat = MenuID;
			CheckMenuItem(m_MenuIDCaptureFormat, true);
		}
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
	case IDM_VIDEORES2:
	case IDM_VIDEORES3:
		if (MenuID != m_MenuIDAviResolution)
		{
			CheckMenuItem(m_MenuIDAviResolution, false);
			m_MenuIDAviResolution = MenuID;
			CheckMenuItem(m_MenuIDAviResolution, true);
		}
		break;

	case IDM_VIDEOSKIP0:
	case IDM_VIDEOSKIP1:
	case IDM_VIDEOSKIP2:
	case IDM_VIDEOSKIP3:
	case IDM_VIDEOSKIP4:
	case IDM_VIDEOSKIP5:
		if (MenuID != m_MenuIDAviSkip)
		{
			CheckMenuItem(m_MenuIDAviSkip, false);
			m_MenuIDAviSkip = MenuID;
			CheckMenuItem(m_MenuIDAviSkip, true);
		}
		break;

	case IDM_CAPTUREVIDEO:
		CaptureVideo();
		break;

	case IDM_ENDVIDEO:
		EndVideo();
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

	case ID_TELETEXT:
		TeletextAdapterEnabled = !TeletextAdapterEnabled;
		TeletextInit();
		CheckMenuItem(ID_TELETEXT, TeletextAdapterEnabled);
		break;

	case ID_TELETEXTFILES:
		if (!TeletextFiles)
		{
			TeletextFiles = true;
			TeletextLocalhost = false;
			TeletextCustom = false;
			TeletextInit();
		}
		CheckMenuItem(ID_TELETEXTFILES, TeletextFiles);
		CheckMenuItem(ID_TELETEXTLOCALHOST, TeletextLocalhost);
		CheckMenuItem(ID_TELETEXTCUSTOM, TeletextCustom);
		break;

	case ID_TELETEXTLOCALHOST:
		if (!TeletextLocalhost)
		{
			TeletextFiles = false;
			TeletextLocalhost = true;
			TeletextCustom = false;
			TeletextInit();
		}
		CheckMenuItem(ID_TELETEXTFILES, TeletextFiles);
		CheckMenuItem(ID_TELETEXTLOCALHOST, TeletextLocalhost);
		CheckMenuItem(ID_TELETEXTCUSTOM, TeletextCustom);
		break;

	case ID_TELETEXTCUSTOM:
		if (!TeletextCustom)
		{
			TeletextFiles = false;
			TeletextLocalhost = false;
			TeletextCustom = true;
			for (int ch=0; ch<4; ch++)
			{
				strcpy(TeletextIP[ch],TeletextCustomIP[ch]);
				TeletextPort[ch] = TeletextCustomPort[ch];
			}
			TeletextInit();
		}
		CheckMenuItem(ID_TELETEXTFILES, TeletextFiles);
		CheckMenuItem(ID_TELETEXTLOCALHOST, TeletextLocalhost);
		CheckMenuItem(ID_TELETEXTCUSTOM, TeletextCustom);
		break;

	case IDM_SET_KEYBOARD_LINKS: {
		KeyboardLinksDialog Dialog(hInst, m_hWnd, KeyboardLinks);

		if (Dialog.DoModal())
		{
			KeyboardLinks = Dialog.GetValue();
		}
		break;
	}

	case ID_HARDDRIVE:
		SCSIDriveEnabled = !SCSIDriveEnabled;
		SCSIReset();
		SASIReset();
		CheckMenuItem(ID_HARDDRIVE, SCSIDriveEnabled);
		if (SCSIDriveEnabled) {
			IDEDriveEnabled = false;
			CheckMenuItem(ID_IDEDRIVE, IDEDriveEnabled);
		}
		break;

	case ID_IDEDRIVE:
		IDEDriveEnabled = !IDEDriveEnabled;
		IDEReset();
		CheckMenuItem(ID_IDEDRIVE, IDEDriveEnabled);
		if (IDEDriveEnabled) {
			SCSIDriveEnabled = false;
			CheckMenuItem(ID_HARDDRIVE, SCSIDriveEnabled);
		}
		break;

	case IDM_SELECT_HARD_DRIVE_FOLDER:
		SelectHardDriveFolder();
		break;

	case ID_FLOPPYDRIVE:
		Disc8271Enabled = !Disc8271Enabled;
		Disc1770Enabled = !Disc1770Enabled;
		CheckMenuItem(ID_FLOPPYDRIVE, Disc8271Enabled);
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

	case ID_TEXT_TO_SPEECH_VOICE_BASE:
	case ID_TEXT_TO_SPEECH_VOICE_BASE + 1:
	case ID_TEXT_TO_SPEECH_VOICE_BASE + 2:
	case ID_TEXT_TO_SPEECH_VOICE_BASE + 3:
	case ID_TEXT_TO_SPEECH_VOICE_BASE + 4:
	case ID_TEXT_TO_SPEECH_VOICE_BASE + 5:
	case ID_TEXT_TO_SPEECH_VOICE_BASE + 6:
	case ID_TEXT_TO_SPEECH_VOICE_BASE + 7:
	case ID_TEXT_TO_SPEECH_VOICE_BASE + 8:
	case ID_TEXT_TO_SPEECH_VOICE_BASE + 9:
		TextToSpeechSetVoice(MenuID - ID_TEXT_TO_SPEECH_VOICE_BASE);
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

bool BeebWin::IsFrozen()
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
		KillTimer(m_hWnd, 1);
		KillTimer(m_hWnd, 2);
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

bool BeebWin::IsPaused()
{
	return m_EmuPaused;
}

/****************************************************************************/

void BeebWin::EditRomConfig()
{
	RomConfigDialog Dialog(hInst, m_hWnd, RomConfig);

	if (Dialog.DoModal())
	{
		// Copy in new config and read ROMs
		memcpy(RomConfig, Dialog.GetRomConfig(), sizeof(ROMConfigFile));
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
			m_startFullScreen = true;
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
			HandleCommand(m_DisplayRenderer);
			InitMenu();
			SetWindowText(m_hWnd, WindowTitle);
			if (m_MenuIDSticks == IDM_JOYSTICK)
				InitJoystick();
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
			ReadROMConfigFile(RomFile, RomConfig);
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
		if (MachineType != Model::Master128)
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
		else // Model::Master128
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
	SetTimer(m_hWnd, 1, 1000, NULL);
}

void BeebWin::SetBootDiscTimer()
{
	SetTimer(m_hWnd, 2, m_AutoBootDelay, NULL);
}

void BeebWin::KillBootDiscTimer()
{
	m_BootDiscTimerElapsed = true;
	KillTimer(m_hWnd, 2);
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
		SetTimer(m_hWnd, 1, m_KbdCmdDelay, NULL);
		return;
	}
	m_KbdCmdLastCycles = TotalCycles;

	// Release previous key press (except shift/control)
	if (m_KbdCmdPress &&
	    m_KbdCmdKey != VK_SHIFT && m_KbdCmdKey != VK_CONTROL)
	{
		TranslateKey(m_KbdCmdKey, true, row, col);
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
					TranslateKey(m_KbdCmdKey, false, row, col);
				else
					TranslateKey(m_KbdCmdKey, true, row, col);
			}

			SetTimer(m_hWnd, 1, m_KbdCmdDelay, NULL);
		}
	}
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
bool BeebWin::RegCreateKey(HKEY hKeyRoot, LPCSTR lpSubKey)
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

bool BeebWin::RegGetBinaryValue(HKEY hKeyRoot, LPCSTR lpSubKey, LPCSTR lpValue,
                                void* pData, int* pnSize)
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

bool BeebWin::RegSetBinaryValue(HKEY hKeyRoot, LPCSTR lpSubKey, LPCSTR lpValue,
                                const void* pData, int* pnSize)
{
	bool rc = false;
	HKEY hKeyResult;
	DWORD dwSize = *pnSize;
	LONG  lRes = 0;

	if ((RegOpenKeyEx(hKeyRoot, lpSubKey, 0, KEY_ALL_ACCESS, &hKeyResult)) == ERROR_SUCCESS)
	{
		lRes = RegSetValueEx(hKeyResult, lpValue, 0, REG_BINARY, reinterpret_cast<const BYTE*>(pData), dwSize);
		if (lRes == ERROR_SUCCESS)
		{
			*pnSize = dwSize;
			rc = true;
		}
		RegCloseKey(hKeyResult);
	}

	return rc;
}

bool BeebWin::RegGetStringValue(HKEY hKeyRoot, LPCSTR lpSubKey, LPCSTR lpValue,
                                LPSTR pData, DWORD dwSize)
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

bool BeebWin::RegSetStringValue(HKEY hKeyRoot, LPCSTR lpSubKey, LPCSTR lpValue,
                                LPCSTR pData)
{
	bool rc = false;
	HKEY hKeyResult;
	DWORD dwSize = (DWORD)strlen(pData);
	LONG  lRes = 0;

	if ((RegOpenKeyEx(hKeyRoot, lpSubKey, 0, KEY_ALL_ACCESS, &hKeyResult)) == ERROR_SUCCESS)
	{
		lRes = RegSetValueEx(hKeyResult, lpValue, 0, REG_SZ, reinterpret_cast<const BYTE*>(pData), dwSize);
		if (lRes == ERROR_SUCCESS)
		{
			rc = true;
		}
		RegCloseKey(hKeyResult);
	}

	return rc;
}
