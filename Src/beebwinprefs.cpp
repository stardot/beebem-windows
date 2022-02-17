/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2007  Mike Wyatt

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

// BeebWin preferences support

#include <stdio.h>

#include "main.h"
#include "beebwin.h"
#include "beebemrc.h"
#include "6502core.h"
#include "disc8271.h"
#include "disc1770.h"
#include "sysvia.h"
#include "uservia.h"
#include "video.h"
#include "beebsound.h"
#include "SoundStreamer.h"
#include "music5000.h"
#include "beebmem.h"
#include "serial.h"
#include "econet.h"
#include "tube.h"
#include "scsi.h"
#include "ide.h"
#include "z80mem.h"
#include "z80.h"
#include "userkybd.h"
#include "UserPortBreakoutBox.h"
#ifdef SPEECH_ENABLED
#include "speech.h"
#endif
#include "teletext.h"
#include "serialdevices.h"
#include "Arm.h"
#include "SprowCoPro.h"

/* Configuration file strings */
static const char *CFG_VIEW_WIN_SIZE = "WinSize";
static const char *CFG_VIEW_SHOW_FPS = "ShowFSP";
static const char *CFG_VIEW_MONITOR = "Monitor";
static const char *CFG_SOUND_SAMPLE_RATE = "SampleRate";
static const char *CFG_SOUND_VOLUME = "SoundVolume";
static const char *CFG_SOUND_ENABLED = "SoundEnabled";
static const char *CFG_OPTIONS_STICKS = "Sticks";
static const char *CFG_OPTIONS_KEY_MAPPING = "KeyMapping";
static const char *CFG_OPTIONS_USER_KEY_MAP_FILE = "UserKeyMapFile";
static const char *CFG_OPTIONS_FREEZEINACTIVE = "FreezeWhenInactive";
static const char *CFG_OPTIONS_HIDE_CURSOR = "HideCursor";
static const char* CFG_OPTIONS_CAPTURE_MOUSE = "CaptureMouse";
static const char *CFG_SPEED_TIMING = "Timing";
static const char *CFG_AMX_ENABLED = "AMXMouseEnabled";
static const char *CFG_AMX_LRFORMIDDLE = "AMXMouseLRForMiddle";
static const char *CFG_AMX_SIZE = "AMXMouseSize";
static const char *CFG_AMX_ADJUST = "AMXMouseAdjust";
static const char *CFG_PRINTER_ENABLED = "PrinterEnabled";
static const char *CFG_PRINTER_PORT = "PrinterPort";
static const char *CFG_PRINTER_FILE = "PrinterFile";
static const char *CFG_MACHINE_TYPE = "MachineType";
static const char *CFG_TUBE_TYPE = "TubeType";

#define LED_COLOUR_TYPE (LEDByte&4)>>2
#define LED_SHOW_KB (LEDByte&1)
#define LED_SHOW_DISC (LEDByte&2)>>1

extern unsigned char CMOSDefault[64];

void BeebWin::LoadPreferences()
{
	Preferences::Result result = m_Preferences.Load(m_PrefsFile);

	if (result == Preferences::Result::Failed) {
		// No prefs file, will use defaults
		Report(MessageType::Error,
		       "Cannot open preferences file:\n  %s\n\nUsing default preferences",
		       m_PrefsFile);
	}
	else if (result == Preferences::Result::InvalidFormat) {
		Report(MessageType::Error,
		       "Invalid preferences file:\n  %s\n\nUsing default preferences",
		       m_PrefsFile);
	}

	char path[_MAX_PATH];
	char keyData[256];
	unsigned char flag;
	DWORD dword;

	// Remove obsolete prefs
	m_Preferences.EraseValue("UserKeyMapRow");
	m_Preferences.EraseValue("UserKeyMapCol");
	m_Preferences.EraseValue("ShowBottomCursorLine");
	m_Preferences.EraseValue("Volume");
	m_Preferences.EraseValue("UsePrimaryBuffer");

	MachineType = Model::B;

	unsigned char type = 0;

	if (m_Preferences.GetBinaryValue(CFG_MACHINE_TYPE, &type, 1))
	{
		if (type < static_cast<unsigned char>(Model::Last))
		{
			MachineType = static_cast<Model>(type);
		}
	}

	if (!m_Preferences.GetBoolValue("WriteProtectOnLoad", m_WriteProtectOnLoad))
		m_WriteProtectOnLoad = true;

	if (m_Preferences.GetDWORDValue("DisplayRenderer", dword))
		m_DisplayRenderer = dword;
	else
		m_DisplayRenderer = IDM_DISPDX9;

	if (!m_Preferences.GetBoolValue("DXSmoothing", m_DXSmoothing))
		m_DXSmoothing = true;

	if (!m_Preferences.GetBoolValue("DXSmoothMode7Only", m_DXSmoothMode7Only))
		m_DXSmoothMode7Only = false;

	if (m_Preferences.GetDWORDValue("DDFullScreenMode", dword))
		m_DDFullScreenMode = dword;
	else
		m_DDFullScreenMode = ID_VIEW_DD_SCREENRES;
	TranslateDDSize();

	if (m_Preferences.GetDWORDValue(CFG_VIEW_WIN_SIZE, dword))
		m_MenuIdWinSize = dword;
	else
		m_MenuIdWinSize = IDM_640X512;
	if (m_MenuIdWinSize == IDM_CUSTOMWINSIZE)
	{
		if (m_Preferences.GetDWORDValue("WinSizeX", dword))
			m_XWinSize = dword;
		else
			m_XWinSize = 640;
		if (m_Preferences.GetDWORDValue("WinSizeY", dword))
			m_YWinSize = dword;
		else
			m_YWinSize = 512;
	}

	if (!m_Preferences.GetBoolValue("FullScreen", m_isFullScreen))
		m_isFullScreen = false;

	TranslateWindowSize();

	if (!m_Preferences.GetBoolValue("MaintainAspectRatio", m_MaintainAspectRatio))
		m_MaintainAspectRatio = true;

	if (!m_Preferences.GetBoolValue(CFG_VIEW_SHOW_FPS, m_ShowSpeedAndFPS))
		m_ShowSpeedAndFPS = true;

	m_PaletteType = PaletteType::RGB;

	if (m_Preferences.GetBinaryValue(CFG_VIEW_MONITOR, &flag, 1))
	{
		if (flag < static_cast<unsigned char>(PaletteType::Last))
		{
			m_PaletteType = static_cast<PaletteType>(flag);
		}
	}

	if (!m_Preferences.GetBoolValue("HideMenuEnabled", m_HideMenuEnabled))
		m_HideMenuEnabled = false;

	unsigned char LEDByte = 0;
	if (!m_Preferences.GetBinaryValue("LED Information", &LEDByte, 1))
		LEDByte=0;
	DiscLedColour=static_cast<LEDColour>(LED_COLOUR_TYPE);
	LEDs.ShowDisc=(LED_SHOW_DISC != 0);
	LEDs.ShowKB=LED_SHOW_KB;

	if (m_Preferences.GetDWORDValue("MotionBlur", dword))
		m_MotionBlur = dword;
	else
		m_MotionBlur = IDM_BLUR_OFF;

	if (!m_Preferences.GetBinaryValue("MotionBlurIntensities", m_BlurIntensities, 8))
	{
		m_BlurIntensities[0]=100;
		m_BlurIntensities[1]=88;
		m_BlurIntensities[2]=75;
		m_BlurIntensities[3]=62;
		m_BlurIntensities[4]=50;
		m_BlurIntensities[5]=38;
		m_BlurIntensities[6]=25;
		m_BlurIntensities[7]=12;
	}

	if (!m_Preferences.GetBoolValue("TextViewEnabled", m_TextViewEnabled))
		m_TextViewEnabled = false;

	if (m_Preferences.GetDWORDValue(CFG_SPEED_TIMING, dword))
		m_MenuIdTiming = dword;
	else
		m_MenuIdTiming = IDM_REALTIME;
	TranslateTiming();

	if (!m_Preferences.GetDWORDValue("SoundConfig::Selection", dword))
		dword = 0;
	SoundConfig::Selection = SoundConfig::Option(dword);

	if (!m_Preferences.GetBoolValue(CFG_SOUND_ENABLED, SoundDefault))
		SoundDefault = true;

	if (!m_Preferences.GetBoolValue("SoundChipEnabled", SoundChipEnabled))
		SoundChipEnabled = true;

	if (m_Preferences.GetDWORDValue(CFG_SOUND_SAMPLE_RATE, dword))
		m_MenuIdSampleRate = dword;
	else
		m_MenuIdSampleRate = IDM_44100KHZ;
	TranslateSampleRate();

	if (m_Preferences.GetDWORDValue(CFG_SOUND_VOLUME, dword))
		m_MenuIdVolume = dword;
	else
		m_MenuIdVolume = IDM_FULLVOLUME;
	TranslateVolume();

	if (!m_Preferences.GetBoolValue("RelaySoundEnabled", RelaySoundEnabled))
		RelaySoundEnabled = false;
	if (!m_Preferences.GetBoolValue("TapeSoundEnabled", TapeSoundEnabled))
		TapeSoundEnabled = false;
	if (!m_Preferences.GetBoolValue("DiscDriveSoundEnabled", DiscDriveSoundEnabled))
		DiscDriveSoundEnabled = true;
	if (!m_Preferences.GetBoolValue("Part Samples", PartSamples))
		PartSamples = true;
	if (!m_Preferences.GetBoolValue("ExponentialVolume", SoundExponentialVolume))
		SoundExponentialVolume = true;
	if (!m_Preferences.GetBoolValue("TextToSpeechEnabled", m_TextToSpeechEnabled))
		m_TextToSpeechEnabled = false;

	if (!m_Preferences.GetBoolValue("Music5000Enabled", Music5000Enabled))
		Music5000Enabled = false;

	if (m_Preferences.GetDWORDValue(CFG_OPTIONS_STICKS, dword))
		m_MenuIdSticks = dword;
	else
		m_MenuIdSticks = 0;

	if (!m_Preferences.GetBoolValue(CFG_OPTIONS_FREEZEINACTIVE, m_FreezeWhenInactive))
		m_FreezeWhenInactive = true;

	if (!m_Preferences.GetBoolValue(CFG_OPTIONS_HIDE_CURSOR, m_HideCursor))
		m_HideCursor = false;

	if (!m_Preferences.GetBoolValue(CFG_OPTIONS_CAPTURE_MOUSE, m_CaptureMouse))
		m_CaptureMouse = false;

	if (m_Preferences.GetDWORDValue(CFG_OPTIONS_KEY_MAPPING,dword))
		m_MenuIdKeyMapping = dword;
	else
		m_MenuIdKeyMapping = IDM_LOGICALKYBDMAPPING;

	bool readDefault = true;
	if (m_Preferences.GetStringValue(CFG_OPTIONS_USER_KEY_MAP_FILE, m_UserKeyMapPath))
	{
		strcpy(path, m_UserKeyMapPath);
		GetDataPath(m_UserDataPath, path);
		if (ReadKeyMap(path, &UserKeymap))
			readDefault = false;
	}
	if (readDefault)
	{
		strcpy(m_UserKeyMapPath, "DefaultUser.kmap");
		strcpy(path, m_UserKeyMapPath);
		GetDataPath(m_UserDataPath, path);
		ReadKeyMap(path, &UserKeymap);
	}

	if (!m_Preferences.GetBoolValue("KeyMapAS", m_KeyMapAS))
		m_KeyMapAS = false;

	if (!m_Preferences.GetBoolValue("KeyMapFunc", m_KeyMapFunc))
		m_KeyMapFunc = false;

	TranslateKeyMapping();

	if (!m_Preferences.GetBoolValue("DisableKeysBreak", m_DisableKeysBreak))
		m_DisableKeysBreak = false;

	if (!m_Preferences.GetBoolValue("DisableKeysEscape", m_DisableKeysEscape))
		m_DisableKeysEscape = false;

	if (!m_Preferences.GetBoolValue("DisableKeysShortcut", m_DisableKeysShortcut))
		m_DisableKeysShortcut = false;

	if (!m_Preferences.GetBoolValue("AutoSavePrefsCMOS", m_AutoSavePrefsCMOS))
		m_AutoSavePrefsCMOS = false;

	if (!m_Preferences.GetBoolValue("AutoSavePrefsFolders", m_AutoSavePrefsFolders))
		m_AutoSavePrefsFolders = false;

	if (!m_Preferences.GetBoolValue("AutoSavePrefsAll", m_AutoSavePrefsAll))
		m_AutoSavePrefsAll = false;

	if (m_Preferences.GetBinaryValue("BitKeys", keyData, 8))
	{
		for (int key=0; key<8; ++key)
			BitKeys[key] = keyData[key];
	}

	if (!m_Preferences.GetBoolValue(CFG_AMX_ENABLED, AMXMouseEnabled))
		AMXMouseEnabled = false;

	if (m_Preferences.GetDWORDValue(CFG_AMX_LRFORMIDDLE, dword))
		AMXLRForMiddle = dword != 0;
	else
		AMXLRForMiddle = true;
	if (m_Preferences.GetDWORDValue(CFG_AMX_SIZE, dword))
		m_MenuIdAMXSize = dword;
	else
		m_MenuIdAMXSize = IDM_AMX_320X256;
	if (m_Preferences.GetDWORDValue(CFG_AMX_ADJUST, dword))
		m_MenuIdAMXAdjust = dword;
	else
		m_MenuIdAMXAdjust = IDM_AMX_ADJUSTP30;
	TranslateAMX();

	if (!m_Preferences.GetBoolValue(CFG_PRINTER_ENABLED, PrinterEnabled))
		PrinterEnabled = false;
	if (m_Preferences.GetDWORDValue(CFG_PRINTER_PORT, dword))
		m_MenuIdPrinterPort = dword;
	else
		m_MenuIdPrinterPort = IDM_PRINTER_LPT1;
	if (!m_Preferences.GetStringValue(CFG_PRINTER_FILE, m_PrinterFileName))
		m_PrinterFileName[0] = 0;
	TranslatePrinterPort();

	if (!m_Preferences.GetBinaryValue("Tape Clock Speed", &TapeClockSpeed, 2))
		TapeClockSpeed=5600;
	if (!m_Preferences.GetBoolValue("UnlockTape", UnlockTape))
		UnlockTape = false;

	if (!m_Preferences.GetBoolValue("SerialPortEnabled", SerialPortEnabled))
		SerialPortEnabled = false;
	if (!m_Preferences.GetBoolValue("TouchScreenEnabled", TouchScreenEnabled))
		TouchScreenEnabled = false;
//	if (!m_Preferences.GetBoolValue("EthernetPortEnabled", EthernetPortEnabled))
//		EthernetPortEnabled = false;
	if (!m_Preferences.GetBoolValue("IP232localhost", IP232localhost))
		IP232localhost = false;
	if (!m_Preferences.GetBoolValue("IP232custom", IP232custom))
		IP232custom = false;

	EthernetPortEnabled = IP232localhost || IP232custom;

	if (!m_Preferences.GetBoolValue("IP232mode", IP232mode))
		IP232mode = false;
	if (!m_Preferences.GetBoolValue("IP232raw", IP232raw))
		IP232raw = false;
	if (m_Preferences.GetDWORDValue("IP232customport",dword))
		IP232customport = dword;
	else
		IP232customport = 25232;
	m_customport = IP232customport;
	if (m_Preferences.GetStringValue("IP232customip", m_customip))
		strcpy(IP232customip, m_customip);
	else
		IP232customip[0] = 0;

	if (!m_Preferences.GetBinaryValue("SerialPort", &SerialPort, 1))
		SerialPort=2;

	if (!m_Preferences.GetBoolValue("EconetEnabled", EconetEnabled))
		EconetEnabled = false;

#ifdef SPEECH_ENABLED
	if (!m_Preferences.GetBoolValue("SpeechEnabled", SpeechDefault))
		SpeechDefault = false;
#endif

	if (!m_Preferences.GetBinaryValue("SWRAMWritable", RomWritePrefs, 16))
	{
		for (int slot = 0; slot < 16; ++slot)
			RomWritePrefs[slot] = 1;
	}

	if (!m_Preferences.GetBoolValue("SWRAMBoard", SWRAMBoardEnabled))
		SWRAMBoardEnabled = false;

	if (m_Preferences.GetBinaryValue(CFG_TUBE_TYPE, &type, 1))
	{
		TubeType = static_cast<Tube>(type);
	}
	else
	{
		// For backwards compatibility with BeebEm 4.14 or earlier:
		unsigned char TubeEnabled = 0;
		unsigned char AcornZ80 = 0;
		unsigned char TorchTube = 0;
		unsigned char Tube186Enabled = 0;
		unsigned char ArmTube = 0;

		m_Preferences.GetBinaryValue("TubeEnabled", &TubeEnabled, 1);
		m_Preferences.GetBinaryValue("AcornZ80", &AcornZ80, 1);
		m_Preferences.GetBinaryValue("TorchTube", &TorchTube, 1);
		m_Preferences.GetBinaryValue("Tube186Enabled", &Tube186Enabled, 1);
		m_Preferences.GetBinaryValue("ArmTube", &ArmTube, 1);

		if (TubeEnabled)
		{
			TubeType = Tube::Acorn65C02;
		}
		else if (AcornZ80)
		{
			TubeType = Tube::AcornZ80;
		}
		else if (TorchTube)
		{
			TubeType = Tube::TorchZ80;
		}
		else if (Tube186Enabled)
		{
			TubeType = Tube::Master512CoPro;
		}
		else if (ArmTube)
		{
			TubeType = Tube::AcornArm;
		}
		else
		{
			TubeType = Tube::None;
		}
	}

	if (!m_Preferences.GetBoolValue("Basic Hardware", BasicHardwareOnly))
		BasicHardwareOnly = false;
	if (!m_Preferences.GetBoolValue("Teletext Half Mode", TeletextHalfMode))
		TeletextHalfMode = false;

	if (!m_Preferences.GetBoolValue("TeletextAdapterEnabled", TeletextAdapterEnabled))
		TeletextAdapterEnabled = false;
	if (!m_Preferences.GetBoolValue("TeletextLocalhost", TeletextLocalhost))
		TeletextLocalhost = false;
	if (!m_Preferences.GetBoolValue("TeletextCustom", TeletextCustom))
		TeletextCustom = false;
	if (!(TeletextLocalhost || TeletextCustom))
		TeletextFiles = true; // default to Files

	char key[20];
	for (int ch=0; ch<4; ch++)
	{
		sprintf(key, "TeletextCustomPort%d", ch);
		if (m_Preferences.GetDWORDValue(key,dword))
			TeletextCustomPort[ch] = (u_short)dword;
		else
			TeletextCustomPort[ch] = (u_short)(19761 + ch);
		sprintf(key, "TeletextCustomIP%d", ch);
		if (m_Preferences.GetStringValue(key, keyData))
			strncpy(TeletextCustomIP[ch], keyData, 16);
		else
			strcpy(TeletextCustomIP[ch], "127.0.0.1");
	}

	if (!m_Preferences.GetBoolValue("FloppyDriveEnabled", Disc8271Enabled))
		Disc8271Enabled = true;
	Disc1770Enabled = Disc8271Enabled;

	if (!m_Preferences.GetBoolValue("SCSIDriveEnabled", SCSIDriveEnabled))
		SCSIDriveEnabled = false;

	if (!m_Preferences.GetBoolValue("IDEDriveEnabled", IDEDriveEnabled))
		IDEDriveEnabled = false;

	if (!m_Preferences.GetBoolValue("RTCEnabled", RTC_Enabled))
		RTC_Enabled = false;

	if (!m_Preferences.GetBoolValue("RTCY2KAdjust", RTCY2KAdjust))
		RTCY2KAdjust = true;

	if (m_Preferences.GetDWORDValue("CaptureResolution", dword))
		m_MenuIdAviResolution = dword;
	else
		m_MenuIdAviResolution = IDM_VIDEORES2;

	if (m_Preferences.GetDWORDValue("FrameSkip", dword))
		m_MenuIdAviSkip = dword;
	else
		m_MenuIdAviSkip = IDM_VIDEOSKIP1;

	if (m_Preferences.GetDWORDValue("BitmapCaptureResolution", dword))
		m_MenuIdCaptureResolution = dword;
	else
		m_MenuIdCaptureResolution = IDM_CAPTURERES_640;

	if (m_Preferences.GetDWORDValue("BitmapCaptureFormat", dword))
		m_MenuIdCaptureFormat = dword;
	else
		m_MenuIdCaptureFormat = IDM_CAPTUREBMP;

	RECT rect;
	if (m_Preferences.GetBinaryValue("WindowPos", &rect, sizeof(rect)))
	{
		m_XWinPos = rect.left;
		m_YWinPos = rect.top;

		// Pos can get corrupted if two BeebEm's exited at same time
		RECT scrrect;
		SystemParametersInfo(SPI_GETWORKAREA, 0, (PVOID)&scrrect, 0);
		if (m_XWinPos < 0)
			m_XWinPos = 0;
		if (m_XWinPos > scrrect.right - 80)
			m_XWinPos = 0;
		if (m_YWinPos < 0)
			m_YWinPos = 0;
		if (m_YWinPos > scrrect.bottom - 80)
			m_YWinPos = 0;
	}
	else
	{
		m_XWinPos = -1;
		m_YWinPos = -1;
	}

	// CMOS RAM now in prefs file
	if (!m_Preferences.GetBinaryValue("CMOSRam", &CMOSRAM[14], 50))
		memcpy(&CMOSRAM[14], CMOSDefault, 50);

	// Set FDC defaults if not already set
	for (int machine = 0; machine < 3; ++machine)
	{
		char CfgName[256];
		sprintf(CfgName, "FDCDLL%d", machine);

		if (!m_Preferences.HasValue(CfgName))
		{
			// Default B+ to Acorn FDC
			if (machine == static_cast<int>(Model::BPlus))
				strcpy(path, "Hardware\\Acorn1770.dll");
			else
				strcpy(path, "None");
			m_Preferences.SetStringValue(CfgName, path);
		}
	}

	// Set file path defaults
	if (!m_Preferences.HasValue("DiscsPath")) {
		m_Preferences.SetStringValue("DiscsPath", "DiscIms");
	}

	if (!m_Preferences.HasValue("DiscsFilter")) {
		m_Preferences.SetDWORDValue("DiscsFilter", 0);
	}

	if (!m_Preferences.HasValue("TapesPath")) {
		m_Preferences.SetStringValue("TapesPath", "Tapes");
	}

	if (!m_Preferences.HasValue("StatesPath")) {
		m_Preferences.SetStringValue("StatesPath", "BeebState");
	}

	if (!m_Preferences.HasValue("AVIPath")) {
		m_Preferences.SetStringValue("AVIPath", "");
	}

	if (!m_Preferences.HasValue("ImagePath")) {
		m_Preferences.SetStringValue("ImagePath", "");
	}

	if (!m_Preferences.HasValue("HardDrivePath")) {
		m_Preferences.SetStringValue("HardDrivePath", "DiscIms");
	}

	// Update prefs version
	m_Preferences.SetStringValue("PrefsVersion", "2.1");

	// Windows key enable/disable still comes from registry
	int binsize = 24;
	if (RegGetBinaryValue(HKEY_LOCAL_MACHINE,CFG_KEYBOARD_LAYOUT,
						  CFG_SCANCODE_MAP,keyData,&binsize) && binsize==24)
		m_DisableKeysWindows = true;
	else
		m_DisableKeysWindows = false;

	if (!m_Preferences.GetBoolValue("WriteInstructionCounts", m_WriteInstructionCounts))
		m_WriteInstructionCounts = false;
}

/****************************************************************************/
void BeebWin::SavePreferences(bool saveAll)
{
	unsigned char LEDByte = 0;
	unsigned char flag;
	char keyData[256];

	if (saveAll)
	{
		m_Preferences.SetBinaryValue(CFG_MACHINE_TYPE, &MachineType, 1);
		m_Preferences.SetBoolValue("WriteProtectOnLoad", m_WriteProtectOnLoad);
		m_Preferences.SetDWORDValue("DisplayRenderer", m_DisplayRenderer);
		m_Preferences.SetBoolValue("DXSmoothing", m_DXSmoothing);
		m_Preferences.SetBoolValue("DXSmoothMode7Only", m_DXSmoothMode7Only);
		m_Preferences.SetDWORDValue("DDFullScreenMode", m_DDFullScreenMode);
		m_Preferences.SetDWORDValue(CFG_VIEW_WIN_SIZE, m_MenuIdWinSize);
		m_Preferences.SetDWORDValue("WinSizeX", m_XLastWinSize);
		m_Preferences.SetDWORDValue("WinSizeY", m_YLastWinSize);
		m_Preferences.SetBoolValue("FullScreen", m_isFullScreen);
		m_Preferences.SetBoolValue("MaintainAspectRatio", m_MaintainAspectRatio);
		m_Preferences.SetBoolValue(CFG_VIEW_SHOW_FPS, m_ShowSpeedAndFPS);
		m_Preferences.SetBinaryValue(CFG_VIEW_MONITOR, &m_PaletteType, 1);
		m_Preferences.SetBoolValue("HideMenuEnabled", m_HideMenuEnabled);
		LEDByte = static_cast<unsigned char>(
			(DiscLedColour == LEDColour::Green ? 4 : 0) |
			(LEDs.ShowDisc ? 2 : 0) |
			(LEDs.ShowKB ? 1 : 0)
		);
		m_Preferences.SetBinaryValue("LED Information", &LEDByte, 1);
		m_Preferences.SetDWORDValue("MotionBlur", m_MotionBlur);
		m_Preferences.SetBinaryValue("MotionBlurIntensities", m_BlurIntensities, 8);
		m_Preferences.SetBoolValue("TextViewEnabled", m_TextViewEnabled);

		m_Preferences.SetDWORDValue(CFG_SPEED_TIMING, m_MenuIdTiming);

		m_Preferences.SetDWORDValue("SoundConfig::Selection", SoundConfig::Selection);
		m_Preferences.SetBoolValue(CFG_SOUND_ENABLED, SoundDefault);
		m_Preferences.SetBoolValue("SoundChipEnabled", SoundChipEnabled);
		m_Preferences.SetDWORDValue(CFG_SOUND_SAMPLE_RATE, m_MenuIdSampleRate);
		m_Preferences.SetDWORDValue(CFG_SOUND_VOLUME, m_MenuIdVolume);
		m_Preferences.SetBoolValue("RelaySoundEnabled", RelaySoundEnabled);
		m_Preferences.SetBoolValue("TapeSoundEnabled", TapeSoundEnabled);
		m_Preferences.SetBoolValue("DiscDriveSoundEnabled", DiscDriveSoundEnabled);
		m_Preferences.SetBoolValue("Part Samples", PartSamples);
		m_Preferences.SetBoolValue("ExponentialVolume", SoundExponentialVolume);
		m_Preferences.SetBoolValue("TextToSpeechEnabled", m_TextToSpeechEnabled);
		m_Preferences.SetBoolValue("Music5000Enabled", Music5000Enabled);

		m_Preferences.SetDWORDValue( CFG_OPTIONS_STICKS, m_MenuIdSticks);
		m_Preferences.SetBoolValue(CFG_OPTIONS_FREEZEINACTIVE, m_FreezeWhenInactive);
		m_Preferences.SetBoolValue(CFG_OPTIONS_HIDE_CURSOR, m_HideCursor);
		m_Preferences.SetBoolValue(CFG_OPTIONS_CAPTURE_MOUSE, m_CaptureMouse);
		m_Preferences.SetDWORDValue( CFG_OPTIONS_KEY_MAPPING, m_MenuIdKeyMapping);
		m_Preferences.SetBoolValue("KeyMapAS", m_KeyMapAS);
		flag = m_KeyMapFunc;
		m_Preferences.SetBinaryValue("KeyMapFunc", &flag, 1);

		m_Preferences.SetBoolValue("DisableKeysBreak", m_DisableKeysBreak);
		m_Preferences.SetBoolValue("DisableKeysEscape", m_DisableKeysEscape);
		m_Preferences.SetBoolValue("DisableKeysShortcut", m_DisableKeysShortcut);

		for (int key = 0; key < 8; ++key)
		{
			keyData[key] = static_cast<char>(BitKeys[key]);
		}

		m_Preferences.SetBinaryValue("BitKeys", keyData, 8);

		m_Preferences.SetStringValue(CFG_OPTIONS_USER_KEY_MAP_FILE, m_UserKeyMapPath);

		m_Preferences.SetBoolValue(CFG_AMX_ENABLED, AMXMouseEnabled);
		m_Preferences.SetDWORDValue(CFG_AMX_LRFORMIDDLE, AMXLRForMiddle);
		m_Preferences.SetDWORDValue(CFG_AMX_SIZE, m_MenuIdAMXSize);
		m_Preferences.SetDWORDValue(CFG_AMX_ADJUST, m_MenuIdAMXAdjust);

		m_Preferences.SetBoolValue(CFG_PRINTER_ENABLED, PrinterEnabled);
		m_Preferences.SetDWORDValue(CFG_PRINTER_PORT, m_MenuIdPrinterPort);
		m_Preferences.SetStringValue(CFG_PRINTER_FILE, m_PrinterFileName);

		m_Preferences.SetBinaryValue("Tape Clock Speed", &TapeClockSpeed, 2);
		m_Preferences.SetBoolValue("UnlockTape", UnlockTape);
		m_Preferences.SetBoolValue("SerialPortEnabled", SerialPortEnabled);
		m_Preferences.SetBoolValue("TouchScreenEnabled", TouchScreenEnabled);
		// m_Preferences.SetBoolValue("EthernetPortEnabled", EthernetPortEnabled);
		m_Preferences.SetBoolValue("IP232localhost", IP232localhost);
		m_Preferences.SetBoolValue("IP232custom", IP232custom);
		m_Preferences.SetBoolValue("IP232mode", IP232mode);
		m_Preferences.SetBoolValue("IP232raw", IP232raw);
		m_Preferences.SetDWORDValue("IP232customport", IP232customport);
		m_Preferences.SetStringValue("IP232customip", m_customip);

		m_Preferences.SetBinaryValue("SerialPort", &SerialPort, 1);

		m_Preferences.SetBoolValue("EconetEnabled", EconetEnabled); // Rob
#ifdef SPEECH_ENABLED
		m_Preferences.SetBoolValue("SpeechEnabled", SpeechDefault);
#endif

		for (int slot = 0; slot < 16; ++slot)
			RomWritePrefs[slot] = RomWritable[slot];
		m_Preferences.SetBinaryValue("SWRAMWritable", RomWritePrefs, 16);
		m_Preferences.SetBoolValue("SWRAMBoard", SWRAMBoardEnabled);

		m_Preferences.SetBinaryValue(CFG_TUBE_TYPE, &TubeType, 1);

		m_Preferences.SetBoolValue("Basic Hardware", BasicHardwareOnly);
		m_Preferences.SetBoolValue("Teletext Half Mode", TeletextHalfMode);
		m_Preferences.SetBoolValue("TeletextAdapterEnabled", TeletextAdapterEnabled);
		m_Preferences.SetBoolValue("TeletextLocalhost", TeletextLocalhost);
		m_Preferences.SetBoolValue("TeletextCustom", TeletextCustom);
		char key[20];
		for (int ch=0; ch<4; ch++)
		{
			sprintf(key, "TeletextCustomPort%d", ch);
			m_Preferences.SetDWORDValue(key, TeletextCustomPort[ch]);
			sprintf(key, "TeletextCustomIP%d", ch);
			m_Preferences.SetStringValue(key, TeletextCustomIP[ch]);
		}

		m_Preferences.SetBoolValue("FloppyDriveEnabled", Disc8271Enabled);
		m_Preferences.SetBoolValue("SCSIDriveEnabled", SCSIDriveEnabled);
		m_Preferences.SetBoolValue("IDEDriveEnabled", IDEDriveEnabled);
		m_Preferences.SetBoolValue("RTCEnabled", RTC_Enabled);
		m_Preferences.SetBoolValue("RTCY2KAdjust", RTCY2KAdjust);

		m_Preferences.SetDWORDValue("CaptureResolution", m_MenuIdAviResolution);
		m_Preferences.SetDWORDValue("FrameSkip", m_MenuIdAviSkip);

		m_Preferences.SetDWORDValue("BitmapCaptureResolution", m_MenuIdCaptureResolution);
		m_Preferences.SetDWORDValue("BitmapCaptureFormat", m_MenuIdCaptureFormat);

		RECT rect;
		GetWindowRect(m_hWnd,&rect);
		m_Preferences.SetBinaryValue("WindowPos", &rect, sizeof(rect));
	}

	// CMOS RAM now in prefs file
	if (saveAll || m_AutoSavePrefsCMOS)
	{
		m_Preferences.SetBinaryValue("CMOSRam", &CMOSRAM[14], 50);
	}

	m_Preferences.SetBoolValue("AutoSavePrefsCMOS", m_AutoSavePrefsCMOS);
	m_Preferences.SetBoolValue("AutoSavePrefsFolders", m_AutoSavePrefsFolders);
	m_Preferences.SetBoolValue("AutoSavePrefsAll", m_AutoSavePrefsAll);

	m_Preferences.SetBoolValue("WriteInstructionCounts", m_WriteInstructionCounts);

	if (m_Preferences.Save(m_PrefsFile) == Preferences::Result::Success) {
		m_AutoSavePrefsChanged = false;
	}
	else {
		Report(MessageType::Error,
		       "Failed to write preferences file:\n  %s",
		       m_PrefsFile);
	}
}
