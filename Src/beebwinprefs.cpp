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
#include <string>
#include <map>
#include <windows.h>
#include <initguid.h>
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
#include "soundstream.h"
#include "music5000.h"
#include "beebmem.h"
#include "beebemrc.h"
#include "atodconv.h"
#include "userkybd.h"
#include "serial.h"
#include "econet.h"
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
#include "sprowcopro.h"

using namespace std;

/* Configuration file strings */
static char *CFG_VIEW_WIN_SIZE = "WinSize";
static char *CFG_VIEW_SHOW_FPS = "ShowFSP";
static char *CFG_VIEW_MONITOR = "Monitor";
static char *CFG_SOUND_SAMPLE_RATE = "SampleRate";
static char *CFG_SOUND_VOLUME = "SoundVolume";
static char *CFG_SOUND_ENABLED = "SoundEnabled";
static char *CFG_OPTIONS_STICKS = "Sticks";
static char *CFG_OPTIONS_KEY_MAPPING = "KeyMapping";
static char *CFG_OPTIONS_USER_KEY_MAP_FILE = "UserKeyMapFile";
static char *CFG_OPTIONS_FREEZEINACTIVE = "FreezeWhenInactive";
static char *CFG_OPTIONS_HIDE_CURSOR = "HideCursor";
static char *CFG_SPEED_TIMING = "Timing";
static char *CFG_AMX_ENABLED = "AMXMouseEnabled";
static char *CFG_AMX_LRFORMIDDLE = "AMXMouseLRForMiddle";
static char *CFG_AMX_SIZE = "AMXMouseSize";
static char *CFG_AMX_ADJUST = "AMXMouseAdjust";
static char *CFG_PRINTER_ENABLED = "PrinterEnabled";
static char *CFG_PRINTER_PORT = "PrinterPort";
static char *CFG_PRINTER_FILE = "PrinterFile";
static char *CFG_MACHINE_TYPE = "MachineType";

// Token written to start of pref files
#define PREFS_TOKEN "*** BeebEm Preferences ***"

#define MAX_PREFS_LINE_LEN 1024

#define LED_COLOUR_TYPE (LEDByte&4)>>2
#define LED_SHOW_KB (LEDByte&1)
#define LED_SHOW_DISC (LEDByte&2)>>1

extern bool HideMenuEnabled;
extern unsigned char CMOSDefault[64];

void BeebWin::LoadPreferences()
{
	FILE *fd;
	char buf[MAX_PREFS_LINE_LEN];
	char *val;
	char CfgName[256];
	char path[_MAX_PATH];
	char keyData[256];
	unsigned char flag;
	DWORD dword;

	fd = fopen(m_PrefsFile, "r");
	if (fd == NULL)
	{
		// No prefs file, will use defaults
		char errstr[500];
		sprintf(errstr, "Cannot open preferences file:\n  %s\n\nUsing default preferences", m_PrefsFile);
		MessageBox(m_hWnd, errstr, WindowTitle, MB_OK|MB_ICONERROR);
	}
	else if (fgets(buf, MAX_PREFS_LINE_LEN-1, fd) != NULL)
	{
		if (strcmp(buf, PREFS_TOKEN "\n") != 0)
		{
			char errstr[500];
			sprintf(errstr, "Invalid preferences file:\n  %s\n\nUsing default preferences", m_PrefsFile);
			MessageBox(m_hWnd, errstr, WindowTitle, MB_OK|MB_ICONERROR);
		}
		else
		{
			while (fgets(buf, MAX_PREFS_LINE_LEN-1, fd) != NULL)
			{
				val = strchr(buf, '=');
				if (val)
				{
					*val = 0;
					++val;
					if (val[strlen(val)-1] == '\n')
						val[strlen(val)-1] = 0;
					m_Prefs[buf] = val;
				}
			}
		}
		fclose(fd);
	}

	// Remove obsolete prefs
	m_Prefs.erase("UserKeyMapRow");
	m_Prefs.erase("UserKeyMapCol");
	m_Prefs.erase("ShowBottomCursorLine");
	m_Prefs.erase("Volume");
	m_Prefs.erase("UsePrimaryBuffer");

	unsigned char type = 0;
	if(!PrefsGetBinaryValue("MachineType", &type, 1))
		MachineType = Model::B;
	else
		MachineType = static_cast<Model>(type);

	if (!PrefsGetBoolValue("WriteProtectOnLoad", m_WriteProtectOnLoad))
		m_WriteProtectOnLoad = true;

	if (PrefsGetDWORDValue("DisplayRenderer",dword))
		m_DisplayRenderer = dword;
	else
		m_DisplayRenderer = IDM_DISPDX9;

	if (!PrefsGetBoolValue("DXSmoothing", m_DXSmoothing))
		m_DXSmoothing = true;

	if (!PrefsGetBoolValue("DXSmoothMode7Only", m_DXSmoothMode7Only))
		m_DXSmoothMode7Only = false;

	if (PrefsGetDWORDValue("DDFullScreenMode",dword))
		m_DDFullScreenMode = dword;
	else
		m_DDFullScreenMode = ID_VIEW_DD_640X480;
	TranslateDDSize();

	if (PrefsGetDWORDValue(CFG_VIEW_WIN_SIZE,dword))
		m_MenuIdWinSize = dword;
	else
		m_MenuIdWinSize = IDM_640X512;
	if (m_MenuIdWinSize == IDM_CUSTOMWINSIZE)
	{
		if (PrefsGetDWORDValue("WinSizeX",dword))
			m_XWinSize = dword;
		else
			m_XWinSize = 640;
		if (PrefsGetDWORDValue("WinSizeY",dword))
			m_YWinSize = dword;
		else
			m_YWinSize = 512;
	}

	if (!PrefsGetBoolValue("FullScreen", m_isFullScreen))
		m_isFullScreen = false;

	TranslateWindowSize();

	if (!PrefsGetBoolValue("MaintainAspectRatio", m_MaintainAspectRatio))
		m_MaintainAspectRatio = true;

	if (!PrefsGetBoolValue(CFG_VIEW_SHOW_FPS, m_ShowSpeedAndFPS))
		m_ShowSpeedAndFPS = true;

	if (PrefsGetBinaryValue(CFG_VIEW_MONITOR,&flag,1))
		palette_type = (PaletteType)flag;
	else
		palette_type = (PaletteType)0;

	if (!PrefsGetBoolValue("HideMenuEnabled", HideMenuEnabled))
		HideMenuEnabled = false;

	unsigned char LEDByte = 0;
	if (!PrefsGetBinaryValue("LED Information",&LEDByte,1))
		LEDByte=0;
	DiscLedColour=static_cast<LEDColour>(LED_COLOUR_TYPE);
	LEDs.ShowDisc=(LED_SHOW_DISC != 0);
	LEDs.ShowKB=LED_SHOW_KB;

	if (PrefsGetDWORDValue("MotionBlur",dword))
		m_MotionBlur = dword;
	else
		m_MotionBlur = IDM_BLUR_OFF;

	if (!PrefsGetBinaryValue("MotionBlurIntensities",m_BlurIntensities,8))
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

	if (!PrefsGetBoolValue("TextViewEnabled",m_TextViewEnabled))
		m_TextViewEnabled = false;

	if (PrefsGetDWORDValue(CFG_SPEED_TIMING,dword))
		m_MenuIdTiming = dword;
	else
		m_MenuIdTiming = IDM_REALTIME;
	TranslateTiming();

	if (!PrefsGetDWORDValue("SoundConfig::Selection", dword))
		dword = 0;
	SoundConfig::Selection = SoundConfig::Option(dword);

	if (!PrefsGetBoolValue(CFG_SOUND_ENABLED, SoundDefault))
		SoundDefault = true;

	if (!PrefsGetBoolValue("SoundChipEnabled", SoundChipEnabled))
		SoundChipEnabled = true;

	if (PrefsGetDWORDValue(CFG_SOUND_SAMPLE_RATE,dword))
		m_MenuIdSampleRate = dword;
	else
		m_MenuIdSampleRate = IDM_44100KHZ;
	TranslateSampleRate();

	if (PrefsGetDWORDValue(CFG_SOUND_VOLUME,dword))
		m_MenuIdVolume = dword;
	else
		m_MenuIdVolume = IDM_FULLVOLUME;
	TranslateVolume();

	if (!PrefsGetBoolValue("RelaySoundEnabled", RelaySoundEnabled))
		RelaySoundEnabled = false;
	if (!PrefsGetBoolValue("TapeSoundEnabled", TapeSoundEnabled))
		TapeSoundEnabled = false;
	if (!PrefsGetBoolValue("DiscDriveSoundEnabled", DiscDriveSoundEnabled))
		DiscDriveSoundEnabled = true;
	if (!PrefsGetBoolValue("Part Samples", PartSamples))
		PartSamples = true;
	if (!PrefsGetBoolValue("ExponentialVolume", SoundExponentialVolume))
		SoundExponentialVolume = true;
	if (!PrefsGetBoolValue("TextToSpeechEnabled", m_TextToSpeechEnabled))
		m_TextToSpeechEnabled = false;

	if (!PrefsGetBoolValue("Music5000Enabled",Music5000Enabled))
		Music5000Enabled = false;

	if (PrefsGetDWORDValue(CFG_OPTIONS_STICKS,dword))
		m_MenuIdSticks = dword;
	else
		m_MenuIdSticks = 0;

	if (!PrefsGetBoolValue(CFG_OPTIONS_FREEZEINACTIVE, m_FreezeWhenInactive))
		m_FreezeWhenInactive = true;

	if (!PrefsGetBoolValue(CFG_OPTIONS_HIDE_CURSOR, m_HideCursor))
		m_HideCursor = false;

	if (PrefsGetDWORDValue(CFG_OPTIONS_KEY_MAPPING,dword))
		m_MenuIdKeyMapping = dword;
	else
		m_MenuIdKeyMapping = IDM_LOGICALKYBDMAPPING;

	bool readDefault = true;
	if (PrefsGetStringValue(CFG_OPTIONS_USER_KEY_MAP_FILE,m_UserKeyMapPath))
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

	if (!PrefsGetBoolValue("KeyMapAS", m_KeyMapAS))
		m_KeyMapAS = false;

	if (!PrefsGetBoolValue("KeyMapFunc", m_KeyMapFunc))
		m_KeyMapFunc = false;

	TranslateKeyMapping();

	if (!PrefsGetBoolValue("DisableKeysBreak", m_DisableKeysBreak))
		m_DisableKeysBreak = false;

	if (!PrefsGetBoolValue("DisableKeysEscape", m_DisableKeysEscape))
		m_DisableKeysEscape = false;

	if (!PrefsGetBoolValue("DisableKeysShortcut", m_DisableKeysShortcut))
		m_DisableKeysShortcut = false;

	if (!PrefsGetBoolValue("AutoSavePrefsCMOS", m_AutoSavePrefsCMOS))
		m_AutoSavePrefsCMOS = false;

	if (!PrefsGetBoolValue("AutoSavePrefsFolders", m_AutoSavePrefsFolders))
		m_AutoSavePrefsFolders = false;

	if (!PrefsGetBoolValue("AutoSavePrefsAll", m_AutoSavePrefsAll))
		m_AutoSavePrefsAll = false;

	sprintf(CfgName, "BitKeys");
	if (PrefsGetBinaryValue(CfgName,keyData,8))
	{
		for (int key=0; key<8; ++key)
			BitKeys[key] = keyData[key];
	}

	if (!PrefsGetBoolValue(CFG_AMX_ENABLED, AMXMouseEnabled))
		AMXMouseEnabled = false;

	if (PrefsGetDWORDValue(CFG_AMX_LRFORMIDDLE, dword))
		AMXLRForMiddle = dword != 0;
	else
		AMXLRForMiddle = true;
	if (PrefsGetDWORDValue(CFG_AMX_SIZE,dword))
		m_MenuIdAMXSize = dword;
	else
		m_MenuIdAMXSize = IDM_AMX_320X256;
	if (PrefsGetDWORDValue(CFG_AMX_ADJUST,dword))
		m_MenuIdAMXAdjust = dword;
	else
		m_MenuIdAMXAdjust = IDM_AMX_ADJUSTP30;
	TranslateAMX();

	if (PrefsGetBoolValue(CFG_PRINTER_ENABLED, PrinterEnabled))
		PrinterEnabled = false;
	if (PrefsGetDWORDValue(CFG_PRINTER_PORT,dword))
		m_MenuIdPrinterPort = dword;
	else
		m_MenuIdPrinterPort = IDM_PRINTER_LPT1;
	if (!PrefsGetStringValue(CFG_PRINTER_FILE,m_PrinterFileName))
		m_PrinterFileName[0] = 0;
	TranslatePrinterPort();

	if (!PrefsGetBinaryValue("Tape Clock Speed",&TapeClockSpeed,2))
		TapeClockSpeed=5600;
	if (!PrefsGetBoolValue("UnlockTape", UnlockTape))
		UnlockTape = false;

	if (!PrefsGetBoolValue("SerialPortEnabled", SerialPortEnabled))
		SerialPortEnabled = false;
	if (!PrefsGetBoolValue("TouchScreenEnabled", TouchScreenEnabled))
		TouchScreenEnabled = false;
//	if (!PrefsGetBoolValue("EthernetPortEnabled", EthernetPortEnabled))
//		EthernetPortEnabled = false;
	if (!PrefsGetBoolValue("IP232localhost", IP232localhost))
		IP232localhost = false;
	if (!PrefsGetBoolValue("IP232custom", IP232custom))
		IP232custom = false;
		
	EthernetPortEnabled = IP232localhost || IP232custom;

	if (!PrefsGetBoolValue("IP232mode", IP232mode))
		IP232mode = false;
	if (!PrefsGetBoolValue("IP232raw", IP232raw))
		IP232raw = false;
	if (PrefsGetDWORDValue("IP232customport",dword))
		IP232customport = dword;
	else
		IP232customport = 25232;
	m_customport = IP232customport;
	if (PrefsGetStringValue("IP232customip",m_customip))
		strcpy(IP232customip, m_customip);
	else 
		IP232customip[0] = 0;

	if (!PrefsGetBinaryValue("SerialPort",&SerialPort,1))
		SerialPort=2;

	if (!PrefsGetBoolValue("EconetEnabled", EconetEnabled))
		EconetEnabled = false;

#ifdef SPEECH_ENABLED
	if (!PrefsGetBoolValue("SpeechEnabled", SpeechDefault))
		SpeechDefault = false;
#endif

	if (!PrefsGetBinaryValue("SWRAMWritable",RomWritePrefs,16))
	{
		for (int slot = 0; slot < 16; ++slot)
			RomWritePrefs[slot] = 1;
	}

	if (!PrefsGetBoolValue("SWRAMBoard", SWRAMBoardEnabled))
		SWRAMBoardEnabled = false;

	if (!PrefsGetBoolValue("ArmTube", ArmTube))
		ArmTube = false;

	if (!PrefsGetBoolValue("ArmCoProTube", ArmCoProTube))
		ArmCoProTube = false;

	if (!PrefsGetBoolValue("TorchTube", TorchTube))
		TorchTube = false;

	if (!PrefsGetBoolValue("AcornZ80", AcornZ80))
		AcornZ80 = false;

	if (!PrefsGetBoolValue("TubeEnabled", TubeEnabled))
		TubeEnabled = false;

#ifdef M512COPRO_ENABLED
	if (!PrefsGetBoolValue("Tube186Enabled", Tube186Enabled))
		Tube186Enabled = false;
#endif
    
	if (!PrefsGetBinaryValue("OpCodes",&OpCodes,1))
		OpCodes=2;
	if (!PrefsGetBoolValue("Basic Hardware", BHardware))
		BHardware = false;
	if (!PrefsGetBoolValue("Teletext Half Mode", THalfMode))
		THalfMode = false;

	if (!PrefsGetBoolValue("TeleTextAdapterEnabled", TeleTextAdapterEnabled))
		TeleTextAdapterEnabled = false;

	if (!PrefsGetBoolValue("FloppyDriveEnabled", Disc8271Enabled))
		Disc8271Enabled = true;
	Disc1770Enabled = Disc8271Enabled;

	if (!PrefsGetBoolValue("SCSIDriveEnabled", SCSIDriveEnabled))
		SCSIDriveEnabled = false;

	if (!PrefsGetBoolValue("IDEDriveEnabled", IDEDriveEnabled))
		IDEDriveEnabled = false;

	if (!PrefsGetBoolValue("RTCEnabled", RTC_Enabled))
		RTC_Enabled = false;

	if (!PrefsGetBoolValue("RTCY2KAdjust", RTCY2KAdjust))
		RTCY2KAdjust = true;

	if (PrefsGetDWORDValue("CaptureResolution",dword))
		m_MenuIdAviResolution = dword;
	else
		m_MenuIdAviResolution = IDM_VIDEORES2;

	if (PrefsGetDWORDValue("FrameSkip",dword))
		m_MenuIdAviSkip = dword;
	else
		m_MenuIdAviSkip = IDM_VIDEOSKIP1;

	if (PrefsGetDWORDValue("BitmapCaptureResolution",dword))
		m_MenuIdCaptureResolution = dword;
	else
		m_MenuIdCaptureResolution = IDM_CAPTURERES3;

	if (PrefsGetDWORDValue("BitmapCaptureFormat",dword))
		m_MenuIdCaptureFormat = dword;
	else
		m_MenuIdCaptureFormat = IDM_CAPTUREBMP;

	RECT rect;
	if (PrefsGetBinaryValue("WindowPos",&rect,sizeof(rect)))
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
	if (!PrefsGetBinaryValue("CMOSRam",&CMOSRAM[14],50))
		memcpy(&CMOSRAM[14], CMOSDefault, 50);

	// Set FDC defaults if not already set
	for (int machine = 0; machine < 3; ++machine)
	{
		sprintf(CfgName, "FDCDLL%d", machine);
		if (m_Prefs.find(CfgName) == m_Prefs.end())
		{
			// Default B+ to Acorn FDC
			if (machine == 2)
				strcpy(path, "Hardware\\Acorn1770.dll");
			else
				strcpy(path, "None");
			PrefsSetStringValue(CfgName, path);
		}
	}

	// Set file path defaults
	if (m_Prefs.find("DiscsPath") == m_Prefs.end())
	{
		PrefsSetStringValue("DiscsPath", "DiscIms");
	}
	if (m_Prefs.find("DiscsFilter") == m_Prefs.end())
	{
		PrefsSetDWORDValue("DiscsFilter",0);
	}
	if (m_Prefs.find("TapesPath") == m_Prefs.end())
	{
		PrefsSetStringValue("TapesPath", "Tapes");
	}
	if (m_Prefs.find("StatesPath") == m_Prefs.end())
	{
		PrefsSetStringValue("StatesPath", "BeebState");
	}
	if (m_Prefs.find("AVIPath") == m_Prefs.end())
	{
		PrefsSetStringValue("AVIPath", "");
	}
	if (m_Prefs.find("ImagePath") == m_Prefs.end())
	{
		PrefsSetStringValue("ImagePath", "");
	}

	// Update prefs version
	PrefsSetStringValue("PrefsVersion", "2.1");

	// Windows key enable/disable still comes from registry
	int binsize = 24;
	if (RegGetBinaryValue(HKEY_LOCAL_MACHINE,CFG_KEYBOARD_LAYOUT,
						  CFG_SCANCODE_MAP,keyData,&binsize) && binsize==24)
		m_DisableKeysWindows = true;
	else
		m_DisableKeysWindows = false;
}

/****************************************************************************/
void BeebWin::SavePreferences(bool saveAll)
{
	unsigned char LEDByte = 0;
	char CfgName[256];
	unsigned char flag;
	char keyData[256];
	DWORD dword;

	if (saveAll)
	{
		PrefsSetBinaryValue("MachineType",&MachineType,1);
		PrefsSetBoolValue("WriteProtectOnLoad", m_WriteProtectOnLoad);
		PrefsSetDWORDValue("DisplayRenderer",m_DisplayRenderer);
		PrefsSetBoolValue("DXSmoothing", m_DXSmoothing);
		PrefsSetBoolValue("DXSmoothMode7Only", m_DXSmoothMode7Only);
		PrefsSetDWORDValue("DDFullScreenMode",m_DDFullScreenMode);
		PrefsSetDWORDValue(CFG_VIEW_WIN_SIZE,m_MenuIdWinSize);
		PrefsSetDWORDValue("WinSizeX",m_XLastWinSize);
		PrefsSetDWORDValue("WinSizeY",m_YLastWinSize);
		PrefsSetBoolValue("FullScreen", m_isFullScreen);
		PrefsSetBoolValue("MaintainAspectRatio", m_MaintainAspectRatio);
		PrefsSetBoolValue(CFG_VIEW_SHOW_FPS, m_ShowSpeedAndFPS);
		flag = palette_type;
		PrefsSetBinaryValue(CFG_VIEW_MONITOR,&flag,1);
		PrefsSetBoolValue("HideMenuEnabled", HideMenuEnabled);
		LEDByte=(static_cast<int>(DiscLedColour) << 2) | ((LEDs.ShowDisc ? 1 : 0) << 1) | (LEDs.ShowKB ? 1 : 0);
		PrefsSetBinaryValue("LED Information",&LEDByte,1);
		flag = m_MotionBlur;
		PrefsSetDWORDValue( "MotionBlur", m_MotionBlur);
		PrefsSetBinaryValue("MotionBlurIntensities",m_BlurIntensities,8);
		PrefsSetBoolValue("TextViewEnabled", m_TextViewEnabled);

		PrefsSetDWORDValue( CFG_SPEED_TIMING, m_MenuIdTiming);

		PrefsSetDWORDValue("SoundConfig::Selection", SoundConfig::Selection);
		PrefsSetBoolValue(CFG_SOUND_ENABLED, SoundDefault);
		PrefsSetBoolValue("SoundChipEnabled", SoundChipEnabled);
		PrefsSetDWORDValue( CFG_SOUND_SAMPLE_RATE, m_MenuIdSampleRate);
		PrefsSetDWORDValue( CFG_SOUND_VOLUME, m_MenuIdVolume);
		PrefsSetBoolValue("RelaySoundEnabled", RelaySoundEnabled);
		PrefsSetBoolValue("TapeSoundEnabled", TapeSoundEnabled);
		PrefsSetBoolValue("DiscDriveSoundEnabled", DiscDriveSoundEnabled);
		PrefsSetBoolValue("Part Samples", PartSamples);
		PrefsSetBoolValue("ExponentialVolume", SoundExponentialVolume);
		PrefsSetBoolValue("TextToSpeechEnabled", m_TextToSpeechEnabled);
		PrefsSetBoolValue("Music5000Enabled", Music5000Enabled);

		PrefsSetDWORDValue( CFG_OPTIONS_STICKS, m_MenuIdSticks);
		PrefsSetBoolValue(CFG_OPTIONS_FREEZEINACTIVE, m_FreezeWhenInactive);
		PrefsSetBoolValue(CFG_OPTIONS_HIDE_CURSOR, m_HideCursor);
		PrefsSetDWORDValue( CFG_OPTIONS_KEY_MAPPING, m_MenuIdKeyMapping);
		PrefsSetBoolValue("KeyMapAS", m_KeyMapAS);
		flag = m_KeyMapFunc;
		PrefsSetBinaryValue("KeyMapFunc",&flag,1);

		PrefsSetBoolValue("DisableKeysBreak", m_DisableKeysBreak);
		PrefsSetBoolValue("DisableKeysEscape", m_DisableKeysEscape);
		PrefsSetBoolValue("DisableKeysShortcut", m_DisableKeysShortcut);

		for (int key=0; key<8; ++key)
			keyData[key] = BitKeys[key];
		sprintf(CfgName, "BitKeys");
		PrefsSetBinaryValue(CfgName,keyData,8);

		PrefsSetStringValue(CFG_OPTIONS_USER_KEY_MAP_FILE, m_UserKeyMapPath);

		PrefsSetBoolValue(CFG_AMX_ENABLED, AMXMouseEnabled);
		PrefsSetDWORDValue(CFG_AMX_LRFORMIDDLE, AMXLRForMiddle);
		PrefsSetDWORDValue(CFG_AMX_SIZE, m_MenuIdAMXSize);
		PrefsSetDWORDValue(CFG_AMX_ADJUST, m_MenuIdAMXAdjust);

		PrefsSetBoolValue(CFG_PRINTER_ENABLED, PrinterEnabled);
		PrefsSetDWORDValue(CFG_PRINTER_PORT, m_MenuIdPrinterPort);
		PrefsSetStringValue(CFG_PRINTER_FILE, m_PrinterFileName);

		PrefsSetBinaryValue("Tape Clock Speed",&TapeClockSpeed,2);
		PrefsSetBoolValue("UnlockTape", UnlockTape);
		PrefsSetBoolValue("SerialPortEnabled", SerialPortEnabled);
		PrefsSetBoolValue("TouchScreenEnabled", TouchScreenEnabled);
//		PrefsSetBoolValue("EthernetPortEnabled", EthernetPortEnabled);
		PrefsSetBoolValue("IP232localhost", IP232localhost);
		PrefsSetBoolValue("IP232custom", IP232custom);
		dword = IP232customport;
		PrefsSetBoolValue("IP232mode", IP232mode);
		PrefsSetBoolValue("IP232raw", IP232raw);
		PrefsSetDWORDValue("IP232customport", dword);
		PrefsSetStringValue("IP232customip", m_customip);

		PrefsSetBinaryValue("SerialPort",&SerialPort,1);

		PrefsSetBoolValue("EconetEnabled", EconetEnabled); //Rob
#ifdef SPEECH_ENABLED
		PrefsSetBoolValue("SpeechEnabled", SpeechDefault);
#endif

		for (int slot = 0; slot < 16; ++slot)
			RomWritePrefs[slot] = RomWritable[slot];
		PrefsSetBinaryValue("SWRAMWritable",RomWritePrefs,16);
		PrefsSetBoolValue("SWRAMBoard", SWRAMBoardEnabled);

		PrefsSetBoolValue("ArmTube", ArmTube);
		PrefsSetBoolValue("ArmCoProTube", ArmCoProTube);
		PrefsSetBoolValue("TorchTube", TorchTube);
		PrefsSetBoolValue("AcornZ80", AcornZ80);
		PrefsSetBoolValue("TubeEnabled", TubeEnabled);
#ifdef M512COPRO_ENABLED
		PrefsSetBoolValue("Tube186Enabled", Tube186Enabled);
#endif

		PrefsSetBinaryValue("OpCodes",&OpCodes,1);
		PrefsSetBoolValue("Basic Hardware", BHardware);
		PrefsSetBoolValue("Teletext Half Mode", THalfMode);
		PrefsSetBoolValue("TeleTextAdapterEnabled", TeleTextAdapterEnabled);
		PrefsSetBoolValue("FloppyDriveEnabled", Disc8271Enabled);
		PrefsSetBoolValue("SCSIDriveEnabled", SCSIDriveEnabled);
		PrefsSetBoolValue("IDEDriveEnabled", IDEDriveEnabled);
		PrefsSetBoolValue("RTCEnabled", RTC_Enabled);
		PrefsSetBoolValue("RTCY2KAdjust", RTCY2KAdjust);

		PrefsSetDWORDValue("CaptureResolution",m_MenuIdAviResolution);
		PrefsSetDWORDValue("FrameSkip",m_MenuIdAviSkip);

		PrefsSetDWORDValue("BitmapCaptureResolution",m_MenuIdCaptureResolution);
		PrefsSetDWORDValue("BitmapCaptureFormat",m_MenuIdCaptureFormat);

		RECT rect;
		GetWindowRect(m_hWnd,&rect);
		PrefsSetBinaryValue("WindowPos",&rect,sizeof(rect));
	}

	// CMOS RAM now in prefs file
	if (saveAll || m_AutoSavePrefsCMOS)
	{
		PrefsSetBinaryValue("CMOSRam",&CMOSRAM[14],50);
	}

	PrefsSetBoolValue("AutoSavePrefsCMOS", m_AutoSavePrefsCMOS);
	PrefsSetBoolValue("AutoSavePrefsFolders", m_AutoSavePrefsFolders);
	PrefsSetBoolValue("AutoSavePrefsAll", m_AutoSavePrefsAll);

	// Write the file
	FILE *fd;

	fd = fopen(m_PrefsFile, "w");
	if (fd == NULL)
	{
		char errstr[500];
		sprintf(errstr, "Failed to write preferences file:\n  %s", m_PrefsFile);
		MessageBox(m_hWnd, errstr, WindowTitle, MB_OK|MB_ICONERROR);
	}
	else
	{
		fprintf(fd, PREFS_TOKEN "\n\n");

		for (PrefsMap::iterator ii = m_Prefs.begin(); ii != m_Prefs.end(); ii++)
		{
			fprintf(fd, "%s=%s\n", ii->first.c_str(), ii->second.c_str());
		}
		fclose(fd);

		m_AutoSavePrefsChanged = false;
	}
}

/****************************************************************************/
bool BeebWin::PrefsGetBinaryValue(const char *id, void *bin, int binsize)
{
	bool found = true;
	PrefsMap::iterator ii = m_Prefs.find(id);
	if (ii != m_Prefs.end())
	{
		char val[MAX_PREFS_LINE_LEN];
		strcpy(val, ii->second.c_str());
		if (strlen(val) == binsize * 2)
		{
			unsigned char *binc = (unsigned char *)bin;
			int x;
			char hx[3];
			hx[2] = 0;
			for (int b = 0; b < binsize; ++b)
			{
				hx[0] = val[b*2];
				hx[1] = val[b*2+1];
				sscanf(hx, "%x", &x);
				binc[b] = x;
			}
		}
		else
		{
			found = false;
		}
	}
	else
	{
		found = false;
	}
	return found;
}
void BeebWin::PrefsSetBinaryValue(const char *id, void *bin, int binsize)
{
	char hx[MAX_PREFS_LINE_LEN];
	unsigned char *binc = (unsigned char *)bin;
	for (int b = 0; b < binsize; ++b)
		sprintf(hx+b*2, "%02x", (int)binc[b]);
	m_Prefs[id] = hx;
}
bool BeebWin::PrefsGetStringValue(const char *id, char *str)
{
	bool found = true;
	PrefsMap::iterator ii = m_Prefs.find(id);
	if (ii != m_Prefs.end())
		strcpy(str, ii->second.c_str());
	else
		found = false;
	return found;
}
void BeebWin::PrefsSetStringValue(const char *id, const char *str)
{
	m_Prefs[id] = str;
}
bool BeebWin::PrefsGetDWORDValue(const char *id, DWORD &dw)
{
	bool found = true;
	PrefsMap::iterator ii = m_Prefs.find(id);
	if (ii != m_Prefs.end())
		sscanf(ii->second.c_str(), "%x", &dw);
	else
		found = false;
	return found;
}
void BeebWin::PrefsSetDWORDValue(const char *id, DWORD dw)
{
	char hx[MAX_PREFS_LINE_LEN];
	sprintf(hx, "%08x", dw);
	m_Prefs[id] = hx;
}

bool BeebWin::PrefsGetBoolValue(const char *id, bool &b)
{
	unsigned char c = 0;
	bool found = PrefsGetBinaryValue(id, &c, sizeof(c));

	b = c != 0;

	return found;
}

void BeebWin::PrefsSetBoolValue(const char *id, bool b)
{
	unsigned char c = b;
	PrefsSetBinaryValue(id, &c, sizeof(c));
}