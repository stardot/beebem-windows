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

using namespace std;

/* Configuration file strings */
static char *CFG_VIEW_WIN_SIZE = "WinSize";
static char *CFG_VIEW_SHOW_FPS = "ShowFSP";
static char *CFG_VIEW_MONITOR = "Monitor";
static char *CFG_SOUND_SAMPLE_RATE = "SampleRate";
static char *CFG_SOUND_VOLUME = "Volume";
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

extern unsigned char HideMenuEnabled;
extern char DiscLedColour;
extern const char *WindowTitle;
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

	if(!PrefsGetBinaryValue("MachineType",&MachineType,1))
		MachineType=0;

	if (!PrefsGetBinaryValue("WriteProtectOnLoad", &m_WriteProtectOnLoad,1))
		m_WriteProtectOnLoad=1;

	if (PrefsGetDWORDValue("DisplayRenderer",dword))
		m_DisplayRenderer = dword;
	else
		m_DisplayRenderer = IDM_DISPDX9;

	if (PrefsGetBinaryValue("DXSmoothing",&flag,1))
		m_DXSmoothing = flag;
	else
		m_DXSmoothing = true;

	if (PrefsGetBinaryValue("DXSmoothMode7Only",&flag,1))
		m_DXSmoothMode7Only = flag;
	else
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
	if (PrefsGetBinaryValue("FullScreen",&flag,1))
		m_isFullScreen = (flag != 0);
	else
		m_isFullScreen = false;
	TranslateWindowSize();

	if (PrefsGetBinaryValue("MaintainAspectRatio",&flag,1))
		m_MaintainAspectRatio = (flag != 0);
	else
		m_MaintainAspectRatio = true;

	if (PrefsGetBinaryValue(CFG_VIEW_SHOW_FPS,&flag,1))
		m_ShowSpeedAndFPS = flag;
	else
		m_ShowSpeedAndFPS = 1;

	if (PrefsGetBinaryValue(CFG_VIEW_MONITOR,&flag,1))
		palette_type = (PaletteType)flag;
	else
		palette_type = (PaletteType)0;

	if (!PrefsGetBinaryValue("HideMenuEnabled",&HideMenuEnabled,1))
		HideMenuEnabled=0;

	int LEDByte=0;
	if (!PrefsGetBinaryValue("LED Information",&LEDByte,1))
		LEDByte=0;
	DiscLedColour=LED_COLOUR_TYPE;
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

	if (!PrefsGetBinaryValue("TextViewEnabled",&m_TextViewEnabled,1))
		m_TextViewEnabled=0;

	if (PrefsGetDWORDValue(CFG_SPEED_TIMING,dword))
		m_MenuIdTiming = dword;
	else
		m_MenuIdTiming = IDM_REALTIME;
	TranslateTiming();

	if (!PrefsGetDWORDValue("SoundConfig::Selection", dword))
		dword = 0;
	SoundConfig::Selection = SoundConfig::Option(dword);

	if (!PrefsGetBinaryValue(CFG_SOUND_ENABLED,&SoundDefault,1))
		SoundDefault=1;

	if (!PrefsGetBinaryValue("SoundChipEnabled",&SoundChipEnabled,1))
		SoundChipEnabled=1;

	if (PrefsGetDWORDValue(CFG_SOUND_SAMPLE_RATE,dword))
		m_MenuIdSampleRate = dword;
	else
		m_MenuIdSampleRate = IDM_44100KHZ;
	TranslateSampleRate();

	if (PrefsGetDWORDValue(CFG_SOUND_VOLUME,dword))
		m_MenuIdVolume = dword;
	else
		m_MenuIdVolume = IDM_MEDIUMVOLUME;
	TranslateVolume();

	if (!PrefsGetBinaryValue("RelaySoundEnabled",&RelaySoundEnabled,1))
		RelaySoundEnabled=0;
	if (!PrefsGetBinaryValue("TapeSoundEnabled",&TapeSoundEnabled,1))
		TapeSoundEnabled=0;
	if (!PrefsGetBinaryValue("DiscDriveSoundEnabled",&DiscDriveSoundEnabled,1))
		DiscDriveSoundEnabled=1;
	if (!PrefsGetBinaryValue("UsePrimaryBuffer",&UsePrimaryBuffer,1))
		UsePrimaryBuffer=0;
	if (!PrefsGetBinaryValue("Part Samples",&PartSamples,1))
		PartSamples=1;
	if (!PrefsGetBinaryValue("ExponentialVolume",&SoundExponentialVolume,1))
		SoundExponentialVolume=1;
	if (!PrefsGetBinaryValue("TextToSpeechEnabled",&m_TextToSpeechEnabled,1))
		m_TextToSpeechEnabled=0;

	if (PrefsGetDWORDValue(CFG_OPTIONS_STICKS,dword))
		m_MenuIdSticks = dword;
	else
		m_MenuIdSticks = 0;

	if (PrefsGetBinaryValue(CFG_OPTIONS_FREEZEINACTIVE,&flag,1))
		m_FreezeWhenInactive = flag;
	else
		m_FreezeWhenInactive = 1;

	if (PrefsGetBinaryValue(CFG_OPTIONS_HIDE_CURSOR,&flag,1))
		m_HideCursor = flag;
	else
		m_HideCursor = 0;

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

	if (PrefsGetBinaryValue("KeyMapAS",&flag,1))
		m_KeyMapAS = flag;
	else
		m_KeyMapAS = 0;
	if (PrefsGetBinaryValue("KeyMapFunc",&flag,1))
		m_KeyMapFunc = flag;
	else
		m_KeyMapFunc = 0;
	TranslateKeyMapping();

	if (!PrefsGetBinaryValue("DisableKeysBreak",&m_DisableKeysBreak,1))
		m_DisableKeysBreak=0;
	if (!PrefsGetBinaryValue("DisableKeysEscape",&m_DisableKeysEscape,1))
		m_DisableKeysEscape=0;
	if (!PrefsGetBinaryValue("DisableKeysShortcut",&m_DisableKeysShortcut,1))
		m_DisableKeysShortcut=0;

	if (PrefsGetBinaryValue("AutoSavePrefsCMOS",&flag,1))
		m_AutoSavePrefsCMOS = (flag != 0);
	else
		m_AutoSavePrefsCMOS = false;

	if (PrefsGetBinaryValue("AutoSavePrefsFolders",&flag,1))
		m_AutoSavePrefsFolders = (flag != 0);
	else
		m_AutoSavePrefsFolders = false;

	if (PrefsGetBinaryValue("AutoSavePrefsAll",&flag,1))
		m_AutoSavePrefsAll = (flag != 0);
	else
		m_AutoSavePrefsAll = false;

	sprintf(CfgName, "BitKeys");
	if (PrefsGetBinaryValue(CfgName,keyData,8))
	{
		for (int key=0; key<8; ++key)
			BitKeys[key] = keyData[key];
	}

	if (PrefsGetBinaryValue(CFG_AMX_ENABLED,&flag,1))
		AMXMouseEnabled = flag;
	else
		AMXMouseEnabled = 0;
	if (PrefsGetDWORDValue(CFG_AMX_LRFORMIDDLE,dword))
		AMXLRForMiddle = dword;
	else
		AMXLRForMiddle = 1;
	if (PrefsGetDWORDValue(CFG_AMX_SIZE,dword))
		m_MenuIdAMXSize = dword;
	else
		m_MenuIdAMXSize = IDM_AMX_320X256;
	if (PrefsGetDWORDValue(CFG_AMX_ADJUST,dword))
		m_MenuIdAMXAdjust = dword;
	else
		m_MenuIdAMXAdjust = IDM_AMX_ADJUSTP30;
	TranslateAMX();

	if (PrefsGetBinaryValue(CFG_PRINTER_ENABLED,&flag,1))
		PrinterEnabled = flag;
	else
		PrinterEnabled = 0;
	if (PrefsGetDWORDValue(CFG_PRINTER_PORT,dword))
		m_MenuIdPrinterPort = dword;
	else
		m_MenuIdPrinterPort = IDM_PRINTER_LPT1;
	if (!PrefsGetStringValue(CFG_PRINTER_FILE,m_PrinterFileName))
		m_PrinterFileName[0] = 0;
	TranslatePrinterPort();

	if (!PrefsGetBinaryValue("Tape Clock Speed",&TapeClockSpeed,2))
		TapeClockSpeed=5600;
	if (!PrefsGetBinaryValue("UnlockTape",&flag,1))
		UnlockTape=0;
	else
		UnlockTape=flag;

	if (!PrefsGetBinaryValue("SerialPortEnabled",&SerialPortEnabled,1))
		SerialPortEnabled=0;
	if (!PrefsGetBinaryValue("TouchScreenEnabled",&TouchScreenEnabled,1))
		TouchScreenEnabled=0;
//	if (!PrefsGetBinaryValue("EthernetPortEnabled",&EthernetPortEnabled,1))
//		EthernetPortEnabled=false;
	if (!PrefsGetBinaryValue("IP232localhost",&IP232localhost,1))
		IP232localhost=0;
	if (!PrefsGetBinaryValue("IP232custom",&IP232custom,1))
		IP232custom=0;
		
	EthernetPortEnabled = (IP232localhost || IP232custom);

	if (!PrefsGetBinaryValue("IP232mode",&IP232mode,1))
		IP232mode=0;
	if (!PrefsGetBinaryValue("IP232raw",&IP232raw,1))
		IP232raw=0;
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

	if (!PrefsGetBinaryValue("EconetEnabled",&EconetEnabled,1))
		EconetEnabled=0;

#ifdef SPEECH_ENABLED
	if (!PrefsGetBinaryValue("SpeechEnabled",&flag,1))
		SpeechDefault=0;
	else
		SpeechDefault=flag;
#endif

	if (!PrefsGetBinaryValue("SWRAMWritable",RomWritePrefs,16))
	{
		for (int slot = 0; slot < 16; ++slot)
			RomWritePrefs[slot] = 1;
	}

	if (!PrefsGetBinaryValue("SWRAMBoard",&SWRAMBoardEnabled,1))
		SWRAMBoardEnabled=0;

	if (!PrefsGetBinaryValue("ArmTube",&ArmTube,1))
		ArmTube=0;

	if (!PrefsGetBinaryValue("TorchTube",&TorchTube,1))
		TorchTube=0;

	if (!PrefsGetBinaryValue("AcornZ80",&AcornZ80,1))
		AcornZ80=0;

	if (!PrefsGetBinaryValue("TubeEnabled",&TubeEnabled,1))
		TubeEnabled=0;

#ifdef M512COPRO_ENABLED
	if (!PrefsGetBinaryValue("Tube186Enabled",&Tube186Enabled,1))
		Tube186Enabled=0;
#endif
    
	if (!PrefsGetBinaryValue("OpCodes",&OpCodes,1))
		OpCodes=2;
	if (!PrefsGetBinaryValue("Basic Hardware",&BHardware,1))
		BHardware=0;
	if (!PrefsGetBinaryValue("Teletext Half Mode",&THalfMode,1))
		THalfMode=0;

	if (!PrefsGetBinaryValue("TeleTextAdapterEnabled",&TeleTextAdapterEnabled,1))
		TeleTextAdapterEnabled=0;

	if (!PrefsGetBinaryValue("FloppyDriveEnabled",&Disc8271Enabled,1))
		Disc8271Enabled=1;
	Disc1770Enabled=Disc8271Enabled;

	if (!PrefsGetBinaryValue("SCSIDriveEnabled",&SCSIDriveEnabled,1))
		SCSIDriveEnabled=0;

	if (!PrefsGetBinaryValue("IDEDriveEnabled",&IDEDriveEnabled,1))
		IDEDriveEnabled=0;

	if (!PrefsGetBinaryValue("RTCEnabled",&flag,1))
		RTC_Enabled=0;
	else
		RTC_Enabled=flag;

	if (!PrefsGetBinaryValue("RTCY2KAdjust",&RTCY2KAdjust,1))
		RTCY2KAdjust=1;

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
	PrefsSetStringValue("PrefsVersion", "1.9");

	// Windows key enable/disable still comes from registry
	int binsize = 24;
	if (RegGetBinaryValue(HKEY_LOCAL_MACHINE,CFG_KEYBOARD_LAYOUT,
						  CFG_SCANCODE_MAP,keyData,&binsize) && binsize==24)
		m_DisableKeysWindows=1;
	else
		m_DisableKeysWindows=0;
}

/****************************************************************************/
void BeebWin::SavePreferences(bool saveAll)
{
	int LEDByte=0;
	char CfgName[256];
	unsigned char flag;
	char keyData[256];
	DWORD dword;

	if (saveAll)
	{
		PrefsSetBinaryValue("MachineType",&MachineType,1);
		PrefsSetBinaryValue("WriteProtectOnLoad",&m_WriteProtectOnLoad,1);
		PrefsSetDWORDValue("DisplayRenderer",m_DisplayRenderer);
		flag = m_DXSmoothing;
		PrefsSetBinaryValue("DXSmoothing",&flag,1);
		flag = m_DXSmoothMode7Only;
		PrefsSetBinaryValue("DXSmoothMode7Only",&flag,1);
		PrefsSetDWORDValue("DDFullScreenMode",m_DDFullScreenMode);
		PrefsSetDWORDValue(CFG_VIEW_WIN_SIZE,m_MenuIdWinSize);
		PrefsSetDWORDValue("WinSizeX",m_XLastWinSize);
		PrefsSetDWORDValue("WinSizeY",m_YLastWinSize);
		flag = m_isFullScreen;
		PrefsSetBinaryValue("FullScreen",&flag,1);
		flag = m_MaintainAspectRatio;
		PrefsSetBinaryValue("MaintainAspectRatio",&flag,1);
		flag = m_ShowSpeedAndFPS;
		PrefsSetBinaryValue(CFG_VIEW_SHOW_FPS,&flag,1);
		flag = palette_type;
		PrefsSetBinaryValue(CFG_VIEW_MONITOR,&flag,1);
		PrefsSetBinaryValue("HideMenuEnabled",&HideMenuEnabled,1);
		LEDByte=(DiscLedColour<<2)|((LEDs.ShowDisc?1:0)<<1)|(LEDs.ShowKB?1:0);
		PrefsSetBinaryValue("LED Information",&LEDByte,1);
		flag = m_MotionBlur;
		PrefsSetDWORDValue( "MotionBlur", m_MotionBlur);
		PrefsSetBinaryValue("MotionBlurIntensities",m_BlurIntensities,8);
		PrefsSetBinaryValue("TextViewEnabled",&m_TextViewEnabled,1);

		PrefsSetDWORDValue( CFG_SPEED_TIMING, m_MenuIdTiming);

		PrefsSetDWORDValue("SoundConfig::Selection", SoundConfig::Selection);
		PrefsSetBinaryValue(CFG_SOUND_ENABLED,&SoundDefault,1);
		flag = SoundChipEnabled;
		PrefsSetBinaryValue("SoundChipEnabled",&flag,1);
		PrefsSetDWORDValue( CFG_SOUND_SAMPLE_RATE, m_MenuIdSampleRate);
		PrefsSetDWORDValue( CFG_SOUND_VOLUME, m_MenuIdVolume);
		PrefsSetBinaryValue("RelaySoundEnabled",&RelaySoundEnabled,1);
		PrefsSetBinaryValue("TapeSoundEnabled",&TapeSoundEnabled,1);
		PrefsSetBinaryValue("DiscDriveSoundEnabled",&DiscDriveSoundEnabled,1);
		PrefsSetBinaryValue("UsePrimaryBuffer",&UsePrimaryBuffer,1);
		PrefsSetBinaryValue("Part Samples",&PartSamples,1);
		PrefsSetBinaryValue("ExponentialVolume",&SoundExponentialVolume,1);
		PrefsSetBinaryValue("TextToSpeechEnabled",&m_TextToSpeechEnabled,1);

		PrefsSetDWORDValue( CFG_OPTIONS_STICKS, m_MenuIdSticks);
		flag = m_FreezeWhenInactive;
		PrefsSetBinaryValue(CFG_OPTIONS_FREEZEINACTIVE,&flag,1);
		flag = m_HideCursor;
		PrefsSetBinaryValue(CFG_OPTIONS_HIDE_CURSOR,&flag,1);
		PrefsSetDWORDValue( CFG_OPTIONS_KEY_MAPPING, m_MenuIdKeyMapping);
		flag = m_KeyMapAS;
		PrefsSetBinaryValue("KeyMapAS",&flag,1);
		flag = m_KeyMapFunc;
		PrefsSetBinaryValue("KeyMapFunc",&flag,1);

		PrefsSetBinaryValue("DisableKeysBreak",&m_DisableKeysBreak,1);
		PrefsSetBinaryValue("DisableKeysEscape",&m_DisableKeysEscape,1);
		PrefsSetBinaryValue("DisableKeysShortcut",&m_DisableKeysShortcut,1);

		for (int key=0; key<8; ++key)
			keyData[key] = BitKeys[key];
		sprintf(CfgName, "BitKeys");
		PrefsSetBinaryValue(CfgName,keyData,8);

		PrefsSetStringValue(CFG_OPTIONS_USER_KEY_MAP_FILE, m_UserKeyMapPath);

		flag = AMXMouseEnabled;
		PrefsSetBinaryValue(CFG_AMX_ENABLED,&flag,1);
		PrefsSetDWORDValue( CFG_AMX_LRFORMIDDLE, AMXLRForMiddle);
		PrefsSetDWORDValue( CFG_AMX_SIZE, m_MenuIdAMXSize);
		PrefsSetDWORDValue( CFG_AMX_ADJUST, m_MenuIdAMXAdjust);

		flag = PrinterEnabled;
		PrefsSetBinaryValue(CFG_PRINTER_ENABLED,&flag,1);
		PrefsSetDWORDValue( CFG_PRINTER_PORT, m_MenuIdPrinterPort);
		PrefsSetStringValue( CFG_PRINTER_FILE, m_PrinterFileName);

		PrefsSetBinaryValue("Tape Clock Speed",&TapeClockSpeed,2);
		flag=UnlockTape;
		PrefsSetBinaryValue("UnlockTape",&flag,1);
		PrefsSetBinaryValue("SerialPortEnabled",&SerialPortEnabled,1);
		PrefsSetBinaryValue("TouchScreenEnabled",&TouchScreenEnabled,1);
//		PrefsSetBinaryValue("EthernetPortEnabled",&EthernetPortEnabled,1);
		PrefsSetBinaryValue("IP232localhost",&IP232localhost,1);
		PrefsSetBinaryValue("IP232custom",&IP232custom,1);
		dword = IP232customport;
		PrefsSetBinaryValue("IP232mode",&IP232mode,1);
		PrefsSetBinaryValue("IP232raw",&IP232raw,1);
		PrefsSetDWORDValue("IP232customport", dword);
		PrefsSetStringValue("IP232customip", m_customip);

		PrefsSetBinaryValue("SerialPort",&SerialPort,1);

		PrefsSetBinaryValue("EconetEnabled",&EconetEnabled,1); //Rob
#ifdef SPEECH_ENABLED
		flag=SpeechDefault;
		PrefsSetBinaryValue("SpeechEnabled",&flag,1);
#endif

		for (int slot = 0; slot < 16; ++slot)
			RomWritePrefs[slot] = RomWritable[slot];
		PrefsSetBinaryValue("SWRAMWritable",RomWritePrefs,16);
		PrefsSetBinaryValue("SWRAMBoard",&SWRAMBoardEnabled,1);

		PrefsSetBinaryValue("ArmTube",&ArmTube,1);
		PrefsSetBinaryValue("TorchTube",&TorchTube,1);
		PrefsSetBinaryValue("AcornZ80",&AcornZ80,1);
		PrefsSetBinaryValue("TubeEnabled",&TubeEnabled,1);
#ifdef M512COPRO_ENABLED
		PrefsSetBinaryValue("Tube186Enabled",&Tube186Enabled,1);
#endif

		PrefsSetBinaryValue("OpCodes",&OpCodes,1);
		PrefsSetBinaryValue("Basic Hardware",&BHardware,1);
		PrefsSetBinaryValue("Teletext Half Mode",&THalfMode,1);
		PrefsSetBinaryValue("TeleTextAdapterEnabled",&TeleTextAdapterEnabled,1);
		PrefsSetBinaryValue("FloppyDriveEnabled",&Disc8271Enabled,1);
		PrefsSetBinaryValue("SCSIDriveEnabled",&SCSIDriveEnabled,1);
		PrefsSetBinaryValue("IDEDriveEnabled",&IDEDriveEnabled,1);
		flag = RTC_Enabled;
		PrefsSetBinaryValue("RTCEnabled",&flag,1);
		PrefsSetBinaryValue("RTCY2KAdjust",&RTCY2KAdjust,1);

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

	flag = m_AutoSavePrefsCMOS;
	PrefsSetBinaryValue("AutoSavePrefsCMOS",&flag,1);
	flag = m_AutoSavePrefsFolders;
	PrefsSetBinaryValue("AutoSavePrefsFolders",&flag,1);
	flag = m_AutoSavePrefsAll;
	PrefsSetBinaryValue("AutoSavePrefsAll",&flag,1);

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

		PrefsMap::iterator ii;
		for (ii = m_Prefs.begin(); ii != m_Prefs.end(); ii++)
		{
			pair<string, string> pref = *ii;
			fprintf(fd, "%s=%s\n", pref.first.c_str(), pref.second.c_str());
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
		strcpy(val, (*ii).second.c_str());
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
		strcpy(str, (*ii).second.c_str());
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
		sscanf((*ii).second.c_str(), "%x", &dw);
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
