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

#include <algorithm>
#include <stdio.h>

#include "BeebWin.h"
#include "6502core.h"
#include "Arm.h"
#include "BeebMem.h"
#include "BeebWinPrefs.h"
#include "Disc1770.h"
#include "Disc8271.h"
#include "Econet.h"
#include "FileUtils.h"
#include "Ide.h"
#include "IP232.h"
#include "KeyMap.h"
#include "Main.h"
#include "Music5000.h"
#include "Registry.h"
#include "Resource.h"
#include "Rtc.h"
#include "Scsi.h"
#include "Serial.h"
#include "Sound.h"
#include "SoundStreamer.h"
#include "Speech.h"
#include "SprowCoPro.h"
#include "SysVia.h"
#include "Teletext.h"
#include "Tube.h"
#include "UserPortBreakoutBox.h"
#include "UserPortRTC.h"
#include "UserVia.h"
#include "Video.h"
#include "Z80mem.h"
#include "Z80.h"

/****************************************************************************/

const int PREFERENCES_VERSION = 3;

const char* const MachineTypeStr[] =
{
	"ModelB",
	"IntegraB",
	"BPlus",
	"Master128",
	"MasterET",
	nullptr
};

const char* const TubeDeviceStr[] =
{
	"None",
	"Acorn65C02",
	"Master512CoPro",
	"AcornZ80",
	"TorchZ80",
	"AcornArm",
	"SprowArm",
	nullptr
};

static const char* const TimingTypeStr[] =
{
	"FixedSpeed",
	"FixedFPS",
	nullptr
};

static const char* const DisplayRendererTypeStr[] =
{
	"GDI",
	"DirectDraw",
	"DirectX9",
	nullptr
};

static const char* const DirectXFullScreenModeStr[] =
{
	"ScreenResolution",
	"640x480",
	"720x576",
	"800x600",
	"1024x768",
	"1280x720",
	"1280x1024",
	"1280x768",
	"1280x960",
	"1440x900",
	"1600x1200",
	"1920x1080",
	"2560x1440",
	"3840x2160",
	nullptr
};

static const char* const SoundStreamerTypeStr[] =
{
	"XAudio2",
	"DirectSound",
	nullptr
};

static const char* const JoystickOptionStr[] =
{
	"Disabled",
	"Joystick",
	"AnalogueMouseStick",
	"DigitalMouseStick",
	nullptr
};

static const char* const KeyboardMappingTypeStr[] =
{
	"User",
	"Default",
	"Logical",
	nullptr
};

static const char* const AMXSizeTypeStr[] =
{
	"160x256",
	"320x256",
	"640x256",
	nullptr
};

static const char* const PrinterPortTypeStr[] =
{
	"File",
	"Clipboard",
	"LPT1",
	"LPT2",
	"LPT3",
	"LPT4",
	nullptr
};

static const char* const LEDColourStr[] =
{
	"Red",
	"Green",
	nullptr
};

static const char* const MonitorTypeStr[] =
{
	"RGB",
	"BW",
	"Amber",
	"Green",
	nullptr
};

static const char* const BitmapCaptureResolutionStr[] =
{
	"Display",
	"1280x1024",
	"640x512",
	"320x256",
	nullptr
};

static const char* const BitmapCaptureFormatStr[] =
{
	"BMP",
	"JPEG",
	"GIF",
	"PNG",
	nullptr
};

static const char* const VideoCaptureResolutionStr[] =
{
	"Display",
	"640x512",
	"320x256",
	nullptr
};

static const char* const TeletextSourceTypeStr[] =
{
	"IP",
	"File",
	nullptr
};

/****************************************************************************/

int BeebWin::FindEnum(const std::string& Value, const char* const* Names, int Default)
{
	int Index = Default;
	int i = 0;

	while (Names[i] != nullptr)
	{
		if (stricmp(Value.c_str(), Names[i]) == 0)
		{
			Index = i;
			break;
		}

		i++;
	}

	return Index;
}

/****************************************************************************/

static int Clamp(int Value, int MinValue, int MaxValue)
{
	return std::min(std::max(Value, MinValue), MaxValue);
}

/****************************************************************************/

static int GetPreferencesVersion(const Preferences& Prefs)
{
	int Version;

	std::string VersionStr;

	Prefs.GetStringValue(CFG_PREFERENCES_VERSION, VersionStr);

	if (!VersionStr.empty())
	{
		try
		{
			// Prior to BeebEm 4.20, versions were of the form
			// major.minor, but we only need to take the major number
			// BeebEm v4.20 onwards uses a single number, see
			// PREFERENCES_VERSION.
			//
			// 2.1 (BeebEm v4.15)
			// 1.9 (BeebEm v4.14)
			// 1.8 (BeebEm v4.13)
			// 1.7 (BeebEm v4.12)
			// 1.4 (BeebEm v4.02)
			// 1.2 (BeebEm v3.82)
			// 1.0 (BeebEm v3.7)
			Version = std::stoi(VersionStr);
		}
		catch (std::exception&)
		{
			Version = 0;
		}
	}
	else
	{
		Version = 0;
	}

	return Version;
}

/****************************************************************************/

void BeebWin::LoadPreferences()
{
	Preferences::Result result = m_Preferences.Load(m_PrefsFileName.c_str());

	if (result == Preferences::Result::Failed)
	{
		// No prefs file, will use defaults
		Report(MessageType::Error,
		       "Cannot open preferences file:\n  %s\n\nUsing default preferences",
		       m_PrefsFileName.c_str());
	}
	else if (result == Preferences::Result::InvalidFormat)
	{
		Report(MessageType::Error,
		       "Invalid preferences file:\n  %s\n\nUsing default preferences",
		       m_PrefsFileName.c_str());
	}

	const int Version = GetPreferencesVersion(m_Preferences);

	if (Version > PREFERENCES_VERSION)
	{
		Report(MessageType::Warning,
		       "Using preferences file from a more recent BeebEm version:\n  %s\n\nThis may cause unexpected results",
		       m_PrefsFileName.c_str());
	}

	LoadHardwarePreferences();
	LoadTubePreferences();
	LoadWindowPosPreferences(Version);
	LoadTimingPreferences(Version);
	LoadDisplayPreferences(Version);
	LoadSoundPreferences(Version);
	LoadInputPreferences(Version);
	LoadAMXMousePreferences(Version);
	LoadPrinterPreferences(Version);
	LoadTextToSpeechPreferences();
	LoadUIPreferences(Version);
	LoadTapePreferences(Version);
	LoadSerialPortPreferences(Version);
	LoadTeletextAdapterPreferences(Version);
	LoadCapturePreferences(Version);
	LoadDiskPreferences();
	LoadUserPortRTCPreferences();
	LoadDebugPreferences();
	LoadKeyMapPreferences();
	LoadAutoSavePreferences();
	LoadCMOSPreferences();
	LoadSWRAMPreferences();
	LoadFilePathPreferences();
	LoadUserPortBreakoutPreferences();
}

/****************************************************************************/

void BeebWin::LoadHardwarePreferences()
{
	std::string Value;

	m_Preferences.GetStringValue(CFG_MACHINE_TYPE, Value, MachineTypeStr[0]);

	MachineType = static_cast<Model>(FindEnum(Value, MachineTypeStr, 0));

	if (!m_Preferences.GetBoolValue(CFG_BASIC_HARDWARE_ONLY, BasicHardwareOnly, false))
	{
		m_Preferences.GetBoolValue(CFG_BASIC_HARDWARE_ONLY_OLD, BasicHardwareOnly, false);
	}

	#if ENABLE_SPEECH

	m_Preferences.GetBoolValue(CFG_SPEECH_ENABLED, SpeechEnabled, false);

	#endif

	m_Preferences.GetBoolValue(CFG_ECONET_ENABLED, EconetEnabled, false);

	if (!m_Preferences.GetBinaryValue(CFG_KEYBOARD_LINKS, &KeyboardLinks, sizeof(KeyboardLinks)))
		KeyboardLinks = 0x00;
}

/****************************************************************************/

void BeebWin::LoadTubePreferences()
{
	std::string Value;

	if (m_Preferences.GetStringValue(CFG_TUBE_TYPE, Value, TubeDeviceStr[0]))
	{
		TubeType = static_cast<TubeDevice>(FindEnum(Value, TubeDeviceStr, 0));
	}
	else
	{
		// For backwards compatibility with BeebEm 4.14 or earlier.
		bool TubeEnabled;
		bool AcornZ80;
		bool TorchTube;
		bool Tube186Enabled;
		bool ArmTube;

		m_Preferences.GetBoolValue(CFG_TUBE_ENABLED_OLD, TubeEnabled, false);
		m_Preferences.GetBoolValue(CFG_TUBE_ACORN_Z80_OLD, AcornZ80, false);
		m_Preferences.GetBoolValue(CFG_TUBE_TORCH_Z80_OLD, TorchTube, false);
		m_Preferences.GetBoolValue(CFG_TUBE_186_OLD, Tube186Enabled, false);
		m_Preferences.GetBoolValue(CFG_TUBE_ARM_OLD, ArmTube, false);

		if (TubeEnabled)
		{
			TubeType = TubeDevice::Acorn65C02;
		}
		else if (AcornZ80)
		{
			TubeType = TubeDevice::AcornZ80;
		}
		else if (TorchTube)
		{
			TubeType = TubeDevice::TorchZ80;
		}
		else if (Tube186Enabled)
		{
			TubeType = TubeDevice::Master512CoPro;
		}
		else if (ArmTube)
		{
			TubeType = TubeDevice::AcornArm;
		}
		else
		{
			TubeType = TubeDevice::None;
		}
	}
}

/****************************************************************************/

void BeebWin::LoadWindowPosPreferences(int Version)
{
	if (Version >= 3)
	{
		m_Preferences.GetDecimalValue(CFG_VIEW_WIN_SIZE_X, m_XWinSize, 640);
		m_Preferences.GetDecimalValue(CFG_VIEW_WIN_SIZE_Y, m_YWinSize, 512);
	}
	else
	{
		DWORD Value;

		m_Preferences.GetDWORDValue(CFG_VIEW_WIN_SIZE, Value, 40006);

		switch (Value)
		{
			case 40005:          m_XWinSize = 320;  m_YWinSize = 256; break;
			case 40006: default: m_XWinSize = 640;  m_YWinSize = 512; break;
			case 40007:          m_XWinSize = 800;  m_YWinSize = 600; break;
			case 40008:          m_XWinSize = 1024; m_YWinSize = 768; break;
			case 40009:          m_XWinSize = 1024; m_YWinSize = 512; break;
			case 40225:          m_XWinSize = 1280; m_YWinSize = 1024; break;
			case 40226:          m_XWinSize = 1440; m_YWinSize = 1080; break;
			case 40227:          m_XWinSize = 1600; m_YWinSize = 1200; break;
			case 40281: // Custom
				m_Preferences.GetDWORDValue(CFG_VIEW_WIN_SIZE_X, Value, 640);
				m_XWinSize = Value;

				m_Preferences.GetDWORDValue(CFG_VIEW_WIN_SIZE_Y, Value, 512);
				m_YWinSize = Value;
				break;
		}
	}

	if (Version >= 3)
	{
		m_Preferences.GetDecimalValue(CFG_VIEW_WINDOW_POS_X, m_XWinPos, -1);
		m_Preferences.GetDecimalValue(CFG_VIEW_WINDOW_POS_Y, m_YWinPos, -1);
	}
	else
	{
		RECT rect;

		if (m_Preferences.GetBinaryValue(CFG_VIEW_WINDOW_POS_OLD, &rect, sizeof(rect)))
		{
			m_XWinPos = rect.left;
			m_YWinPos = rect.top;
		}
		else
		{
			m_XWinPos = -1;
			m_YWinPos = -1;
		}
	}

	// Pos can get corrupted if two BeebEm's exited at same time
	RECT WorkAreaRect;
	SystemParametersInfo(SPI_GETWORKAREA, 0, &WorkAreaRect, 0);

	if (m_XWinPos > WorkAreaRect.right - 80)
		m_XWinPos = -1;

	if (m_YWinPos > WorkAreaRect.bottom - 80)
		m_YWinPos = -1;
}

/****************************************************************************/

void BeebWin::LoadTimingPreferences(int Version)
{
	if (Version >= 3)
	{
		std::string Value;

		m_Preferences.GetStringValue(CFG_SPEED_TIMING, Value, TimingTypeStr[0]);

		m_TimingType = static_cast<TimingType>(FindEnum(Value, TimingTypeStr, 0));

		int Speed;

		m_Preferences.GetDecimalValue(CFG_SPEED, Speed, 100);

		if (m_TimingType == TimingType::FixedFPS)
		{
			switch (Speed)
			{
				case 50:
				case 25:
				case 10:
				case 5:
				case 1:
					m_TimingSpeed = Speed;
					break;

				default:
					m_TimingSpeed = 50;
					break;
			}
		}
		else if (m_TimingType == TimingType::FixedSpeed)
		{
			switch (Speed)
			{
				case 10000:
				case 5000:
				case 1000:
				case 500:
				case 200:
				case 150:
				case 125:
				case 110:
				case 100:
				case 90:
				case 75:
				case 50:
				case 25:
				case 10:
					m_TimingSpeed = Speed;
					break;

				default:
					m_TimingSpeed = 100;
					break;
			}
		}
	}
	else
	{
		DWORD Value;

		// BeebEm 4.19 and earlier stored a menu item ID.
		m_Preferences.GetDWORDValue(CFG_SPEED_TIMING, Value, 40024);

		switch (Value)
		{
			case 40024: default: m_TimingType = TimingType::FixedSpeed; m_TimingSpeed = 100; break;
			case 40025:          m_TimingType = TimingType::FixedFPS;   m_TimingSpeed = 50; break;
			case 40026:          m_TimingType = TimingType::FixedFPS;   m_TimingSpeed = 25; break;
			case 40027:          m_TimingType = TimingType::FixedFPS;   m_TimingSpeed = 10; break;
			case 40028:          m_TimingType = TimingType::FixedFPS;   m_TimingSpeed = 5; break;
			case 40029:          m_TimingType = TimingType::FixedFPS;   m_TimingSpeed = 1; break;
			case 40151:          m_TimingType = TimingType::FixedSpeed; m_TimingSpeed = 10000; break;
			case 40154:          m_TimingType = TimingType::FixedSpeed; m_TimingSpeed = 500; break;
			case 40155:          m_TimingType = TimingType::FixedSpeed; m_TimingSpeed = 200; break;
			case 40156:          m_TimingType = TimingType::FixedSpeed; m_TimingSpeed = 150; break;
			case 40157:          m_TimingType = TimingType::FixedSpeed; m_TimingSpeed = 125; break;
			case 40158:          m_TimingType = TimingType::FixedSpeed; m_TimingSpeed = 110; break;
			case 40159:          m_TimingType = TimingType::FixedSpeed; m_TimingSpeed = 90; break;
			case 40160:          m_TimingType = TimingType::FixedSpeed; m_TimingSpeed = 75; break;
			case 40161:          m_TimingType = TimingType::FixedSpeed; m_TimingSpeed = 5000; break;
			case 40162:          m_TimingType = TimingType::FixedSpeed; m_TimingSpeed = 25; break;
			case 40163:          m_TimingType = TimingType::FixedSpeed; m_TimingSpeed = 1000; break;
			case 40164:          m_TimingType = TimingType::FixedSpeed; m_TimingSpeed = 50; break;
			case 40165:          m_TimingType = TimingType::FixedSpeed; m_TimingSpeed = 10; break;
		}
	}

	TranslateTiming();
}

/****************************************************************************/

void BeebWin::LoadDisplayPreferences(int Version)
{
	if (Version >= 3)
	{
		std::string Value;

		m_Preferences.GetStringValue(CFG_DISPLAY_RENDERER, Value, DisplayRendererTypeStr[2]);

		m_DisplayRenderer = static_cast<DisplayRendererType>(FindEnum(Value, DisplayRendererTypeStr, 2));
	}
	else
	{
		DWORD Value;

		// BeebEm 4.19 and earlier stored a menu item ID.
		m_Preferences.GetDWORDValue(CFG_DISPLAY_RENDERER, Value, 40219);

		switch (Value)
		{
			case 40217:          m_DisplayRenderer = DisplayRendererType::GDI; break;
			case 40218:          m_DisplayRenderer = DisplayRendererType::DirectDraw; break;
			case 40219: default: m_DisplayRenderer = DisplayRendererType::DirectX9; break;
		}
	}

	m_Preferences.GetBoolValue(CFG_FULL_SCREEN, m_FullScreen, false);

	if (Version >= 3)
	{
		std::string Value;

		m_Preferences.GetStringValue(CFG_DX_FULL_SCREEN_MODE, Value, DirectXFullScreenModeStr[0]);

		m_DDFullScreenMode = static_cast<DirectXFullScreenMode>(FindEnum(Value, DirectXFullScreenModeStr, 0));
	}
	else
	{
		DWORD Value;

		// BeebEm 4.19 and earlier stored a menu item ID.
		m_Preferences.GetDWORDValue(CFG_DX_FULL_SCREEN_MODE_OLD, Value, 40102);

		switch (Value)
		{
			case 40102: default: m_DDFullScreenMode = DirectXFullScreenMode::ScreenResolution; break;
			case 40099:          m_DDFullScreenMode = DirectXFullScreenMode::_640x480; break;
			case 40279:          m_DDFullScreenMode = DirectXFullScreenMode::_720x576; break;
			case 40280:          m_DDFullScreenMode = DirectXFullScreenMode::_800x600; break;
			case 40100:          m_DDFullScreenMode = DirectXFullScreenMode::_1024x768; break;
			case 40288:          m_DDFullScreenMode = DirectXFullScreenMode::_1280x720; break;
			case 40101:          m_DDFullScreenMode = DirectXFullScreenMode::_1280x1024; break;
			case 40221:          m_DDFullScreenMode = DirectXFullScreenMode::_1280x768; break;
			case 40222:          m_DDFullScreenMode = DirectXFullScreenMode::_1280x960; break;
			case 40223:          m_DDFullScreenMode = DirectXFullScreenMode::_1440x900; break;
			case 40224:          m_DDFullScreenMode = DirectXFullScreenMode::_1600x1200; break;
			case 40289:          m_DDFullScreenMode = DirectXFullScreenMode::_1920x1080; break;
			case 40294:          m_DDFullScreenMode = DirectXFullScreenMode::_2560x1440; break;
			case 40295:          m_DDFullScreenMode = DirectXFullScreenMode::_3840x2160; break;
		}
	}

	TranslateDDSize();

	m_Preferences.GetBoolValue(CFG_MAINTAIN_ASPECT_RATIO, m_MaintainAspectRatio, true);

	m_Preferences.GetBoolValue(CFG_DX_SMOOTHING, m_DXSmoothing, true);

	m_Preferences.GetBoolValue(CFG_DX_SMOOTH_MODE7_ONLY, m_DXSmoothMode7Only, false);

	if (!m_Preferences.GetBoolValue(CFG_TELETEXT_HALF_MODE, TeletextHalfMode, false))
	{
		m_Preferences.GetBoolValue(CFG_TELETEXT_HALF_MODE_OLD, TeletextHalfMode, false);
	}

	if (Version >= 3)
	{
		int Value;

		m_Preferences.GetDecimalValue(CFG_MOTION_BLUR, Value, 0);

		switch (Value)
		{
			case 0:
			case 2:
			case 4:
			case 8:
				m_MotionBlur = Value;
				break;

			default:
				m_MotionBlur = 0;
				break;
		}
	}
	else
	{
		DWORD Value;

		// BeebEm 4.19 and earlier stored a menu item ID.
		m_Preferences.GetDWORDValue(CFG_MOTION_BLUR, Value, 40177);

		switch (Value)
		{
			case 40177: default: m_MotionBlur = 0; break;
			case 40178:          m_MotionBlur = 2; break;
			case 40179:          m_MotionBlur = 4; break;
			case 40180:          m_MotionBlur = 8; break;
		}
	}

	if (!m_Preferences.GetBinaryValue(CFG_MOTION_BLUR_INTENSITIES, m_BlurIntensities, 8))
	{
		memcpy(m_BlurIntensities, DefaultBlurIntensities, sizeof(m_BlurIntensities));
	}
}

/****************************************************************************/

void BeebWin::LoadSoundPreferences(int Version)
{
	if (Version >= 3)
	{
		std::string Value;

		m_Preferences.GetStringValue(CFG_SOUND_STREAMER, Value, SoundStreamerTypeStr[0]);

		SelectedSoundStreamer = static_cast<SoundStreamerType>(FindEnum(Value, SoundStreamerTypeStr, 0));
	}
	else
	{
		DWORD Value;
		m_Preferences.GetDWORDValue(CFG_SOUND_STREAMER_OLD, Value, 0);

		switch (Value)
		{
			case 0:
			default:
				SelectedSoundStreamer = SoundStreamerType::XAudio2;
				break;

			case 1:
				SelectedSoundStreamer = SoundStreamerType::DirectSound;
				break;
		}
	}

	if (Version >= 3)
	{
		int Value;

		m_Preferences.GetDecimalValue(CFG_SOUND_SAMPLE_RATE, Value, 44100);

		switch (Value)
		{
			case 11025:
			case 22050:
			case 44100:
				SoundSampleRate = Value;
				break;

			default:
				SoundSampleRate = 44100;
				break;
		}
	}
	else
	{
		DWORD Value;

		// BeebEm 4.19 and earlier stored a menu item ID.
		m_Preferences.GetDWORDValue(CFG_SOUND_SAMPLE_RATE, Value, 40014);

		switch (Value)
		{
			case 40016:          SoundSampleRate = 11025; break;
			case 40015:          SoundSampleRate = 22050; break;
			case 40014: default: SoundSampleRate = 44100; break;
		}
	}

	if (Version >= 3)
	{
		int Value;

		m_Preferences.GetDecimalValue(CFG_SOUND_VOLUME, Value, 0100);

		switch (Value)
		{
			case 25:
			case 50:
			case 75:
			case 100:
				SoundVolume = Value;
				break;

			default:
				SoundVolume = 100;
				break;
		}
	}
	else
	{
		DWORD Value;

		// BeebEm 4.19 and earlier stored a menu item ID.
		m_Preferences.GetDWORDValue(CFG_SOUND_VOLUME, Value, 40021);

		switch (Value)
		{
			case 40017:          SoundVolume = 75; break;
			case 40018:          SoundVolume = 50; break;
			case 40019:          SoundVolume = 25; break;
			case 40021: default: SoundVolume = 100; break;
		}
	}

	m_Preferences.GetBoolValue(CFG_SOUND_ENABLED, SoundDefault, true);
	m_Preferences.GetBoolValue(CFG_SOUND_CHIP_ENABLED, SoundChipEnabled, true);
	m_Preferences.GetBoolValue(CFG_SOUND_EXPONENTIAL_VOLUME, SoundExponentialVolume, true);
	m_Preferences.GetBoolValue(CFG_RELAY_SOUND_ENABLED, RelaySoundEnabled, false);
	m_Preferences.GetBoolValue(CFG_TAPE_SOUND_ENABLED, TapeSoundEnabled, false);
	m_Preferences.GetBoolValue(CFG_DISC_SOUND_ENABLED, DiscDriveSoundEnabled, true);

	if (!m_Preferences.GetBoolValue(CFG_SOUND_PART_SAMPLES, PartSamples, true))
	{
		m_Preferences.GetBoolValue(CFG_SOUND_PART_SAMPLES_OLD, PartSamples, true);
	}

	m_Preferences.GetBoolValue(CFG_MUSIC5000_ENABLED, Music5000Enabled, false);
}

/****************************************************************************/

void BeebWin::LoadInputPreferences(int Version)
{
	if (Version >= 3)
	{
		std::string Value;

		m_Preferences.GetStringValue(CFG_OPTIONS_STICKS, Value, JoystickOptionStr[0]);

		m_JoystickOption = static_cast<JoystickOption>(FindEnum(Value, JoystickOptionStr, 0));
	}
	else
	{
		DWORD Value;

		// BeebEm 4.19 and earlier stored a menu item ID.
		m_Preferences.GetDWORDValue(CFG_OPTIONS_STICKS, Value, 0);

		switch (Value)
		{
			case 40030:          m_JoystickOption = JoystickOption::Joystick; break;
			case 40205:          m_JoystickOption = JoystickOption::AnalogueMouseStick; break;
			case 40206:          m_JoystickOption = JoystickOption::DigitalMouseStick; break;
			case 0:     default: m_JoystickOption = JoystickOption::Disabled; break;
		}
	}

	if (Version >= 3)
	{
		std::string Value;

		m_Preferences.GetStringValue(CFG_OPTIONS_KEY_MAPPING, Value, KeyboardMappingTypeStr[2]);

		m_KeyboardMapping = static_cast<KeyboardMappingType>(FindEnum(Value, KeyboardMappingTypeStr, 2));
	}
	else
	{
		DWORD Value;

		// BeebEm 4.19 and earlier stored a menu item ID.
		m_Preferences.GetDWORDValue(CFG_OPTIONS_KEY_MAPPING, Value, 2);

		switch (Value)
		{
			case 40060:          m_KeyboardMapping = KeyboardMappingType::User; break;
			case 40034:          m_KeyboardMapping = KeyboardMappingType::Default; break;
			case 40035: default: m_KeyboardMapping = KeyboardMappingType::Logical; break;
		}
	}

	bool ReadDefault = true;
	char Path[MAX_PATH];

	if (m_Preferences.GetStringValue(CFG_OPTIONS_USER_KEY_MAP_FILE, m_UserKeyMapPath))
	{
		strcpy(Path, m_UserKeyMapPath);
		GetDataPath(m_UserDataPath, Path);
		if (ReadKeyMap(Path, &UserKeyMap))
			ReadDefault = false;
	}

	if (ReadDefault)
	{
		strcpy(m_UserKeyMapPath, "DefaultUser.kmap");
		strcpy(Path, m_UserKeyMapPath);
		GetDataPath(m_UserDataPath, Path);
		ReadKeyMap(Path, &UserKeyMap);
	}
}

/****************************************************************************/

void BeebWin::LoadAMXMousePreferences(int Version)
{
	m_Preferences.GetBoolValue(CFG_OPTIONS_CAPTURE_MOUSE, m_CaptureMouse, false);
	m_Preferences.GetBoolValue(CFG_AMX_ENABLED, AMXMouseEnabled, false);
	m_Preferences.GetBoolValue(CFG_AMX_LRFORMIDDLE, AMXLRForMiddle, true);

	if (Version >= 3)
	{
		std::string Value;

		m_Preferences.GetStringValue(CFG_AMX_SIZE, Value, AMXSizeTypeStr[1]);

		m_AMXSize = static_cast<AMXSizeType>(FindEnum(Value, AMXSizeTypeStr, 1));
	}
	else
	{
		DWORD Value;

		// BeebEm 4.19 and earlier stored a menu item ID.
		m_Preferences.GetDWORDValue(CFG_AMX_SIZE, Value, 1);

		switch (Value)
		{
			case 40069:          m_AMXSize = AMXSizeType::_160x256; break;
			case 40070: default: m_AMXSize = AMXSizeType::_320x256; break;
			case 40071:          m_AMXSize = AMXSizeType::_640x256; break;
		}
	}

	if (Version >= 3)
	{
		int Value;

		m_Preferences.GetDecimalValue(CFG_AMX_ADJUST, Value, 30);

		switch (Value)
		{
			case 50:
			case 30:
			case 10:
			case -10:
			case -30:
			case -50:
				m_AMXAdjust = Value;
				break;

			default:
				m_AMXAdjust = 30;
				break;
		}
	}
	else
	{
		DWORD Value;

		// BeebEm 4.19 and earlier stored a menu item ID.
		m_Preferences.GetDWORDValue(CFG_AMX_ADJUST, Value, 40073);

		switch (Value)
		{
			case 40072:          m_AMXAdjust = 50; break;
			case 40073: default: m_AMXAdjust = 30; break;
			case 40074:          m_AMXAdjust = 10; break;
			case 40075:          m_AMXAdjust = -10; break;
			case 40076:          m_AMXAdjust = -30; break;
			case 40077:          m_AMXAdjust = -50; break;
		}
	}
}

/****************************************************************************/

void BeebWin::LoadPrinterPreferences(int Version)
{
	m_Preferences.GetBoolValue(CFG_PRINTER_ENABLED, PrinterEnabled, false);

	if (Version >= 3)
	{
		std::string Value;

		m_Preferences.GetStringValue(CFG_PRINTER_PORT, Value, PrinterPortTypeStr[1]);

		m_PrinterPort = static_cast<PrinterPortType>(FindEnum(Value, PrinterPortTypeStr, 1));
	}
	else
	{
		DWORD Value;

		// BeebEm 4.19 and earlier stored a menu item ID.
		m_Preferences.GetDWORDValue(CFG_PRINTER_PORT, Value, 40082);

		switch (Value)
		{
			case 40081:          m_PrinterPort = PrinterPortType::File; break;
			case 40244: default: m_PrinterPort = PrinterPortType::Clipboard; break;
			case 40082:          m_PrinterPort = PrinterPortType::Lpt1; break;
			case 40083:          m_PrinterPort = PrinterPortType::Lpt2; break;
			case 40084:          m_PrinterPort = PrinterPortType::Lpt3; break;
			case 40085:          m_PrinterPort = PrinterPortType::Lpt4; break;
		}
	}

	if (!m_Preferences.GetStringValue(CFG_PRINTER_FILE, m_PrinterFileName))
	{
		m_PrinterFileName.clear();
	}

	TranslatePrinterPort();
}

/****************************************************************************/

void BeebWin::LoadTextToSpeechPreferences()
{
	m_Preferences.GetBoolValue(CFG_TEXT_TO_SPEECH_ENABLED, m_TextToSpeechEnabled, false);
	m_Preferences.GetBoolValue(CFG_TEXT_TO_SPEECH_AUTO_SPEAK, m_SpeechWriteChar, true);
	m_Preferences.GetBoolValue(CFG_TEXT_TO_SPEECH_PUNCTUATION, m_SpeechSpeakPunctuation, false);
	m_Preferences.GetDecimalValue(CFG_TEXT_TO_SPEECH_RATE, m_SpeechRate, 0);

	m_SpeechRate = Clamp(m_SpeechRate, -10, 10);
}

/****************************************************************************/

void BeebWin::LoadUIPreferences(int Version)
{
	if (!m_Preferences.GetBoolValue(CFG_SHOW_FPS, m_ShowSpeedAndFPS, true))
	{
		m_Preferences.GetBoolValue(CFG_SHOW_FPS_OLD, m_ShowSpeedAndFPS, true);
	}

	if (Version >= 3)
	{
		std::string Value;

		m_Preferences.GetStringValue(CFG_DISC_LED_COLOUR, Value, LEDColourStr[0]);

		m_DiscLedColour = static_cast<LEDColour>(FindEnum(Value, LEDColourStr, 0));

		m_Preferences.GetBoolValue(CFG_SHOW_KEYBOARD_LEDS, LEDs.ShowKB, false);
		m_Preferences.GetBoolValue(CFG_SHOW_DISC_LEDS, LEDs.ShowDisc, false);
	}
	else
	{
		unsigned char LEDByte = 0;

		if (m_Preferences.GetBinaryValue(CFG_LED_INFORMATION_OLD, &LEDByte, 1))
		{
			m_DiscLedColour = ((LEDByte & 0x04) != 0) ? LEDColour::Green : LEDColour::Red;
			LEDs.ShowDisc = ((LEDByte & 0x02) != 0) && MachineType != Model::MasterET;
			LEDs.ShowKB = (LEDByte & 0x01) != 0;
		}
	}

	m_Preferences.GetBoolValue(CFG_HIDE_MENU_ENABLED, m_HideMenuEnabled, false);

	if (Version >= 3)
	{
		std::string Value;

		m_Preferences.GetStringValue(CFG_VIEW_MONITOR, Value, MonitorTypeStr[0]);

		m_MonitorType = static_cast<MonitorType>(FindEnum(Value, MonitorTypeStr, 0));
	}
	else
	{
		int Value;

		m_Preferences.GetDecimalValue(CFG_VIEW_MONITOR, Value, 0);

		switch (Value)
		{
			case 0: default: m_MonitorType = MonitorType::RGB; break;
			case 1:          m_MonitorType = MonitorType::BW; break;
			case 2:          m_MonitorType = MonitorType::Amber; break;
			case 3:          m_MonitorType = MonitorType::Green; break;
		}
	}

	m_Preferences.GetBoolValue(CFG_OPTIONS_HIDE_CURSOR, m_HideCursor, false);
	m_Preferences.GetBoolValue(CFG_OPTIONS_FREEZEINACTIVE, m_FreezeWhenInactive, true);
	m_Preferences.GetBoolValue(CFG_TEXT_VIEW_ENABLED, m_TextViewEnabled, false);
}

/****************************************************************************/

void BeebWin::LoadTapePreferences(int Version)
{
	if (Version >= 3)
	{
		m_Preferences.GetDecimalValue(CFG_TAPE_CLOCK_SPEED, TapeState.ClockSpeed, 5600);
	}
	else
	{
		if (!m_Preferences.GetBinaryValue(CFG_TAPE_CLOCK_SPEED_OLD, &TapeState.ClockSpeed, 2))
		{
			TapeState.ClockSpeed = 5600;
		}
	}

	m_Preferences.GetBoolValue(CFG_UNLOCK_TAPE, TapeState.Unlock, false);
}

/****************************************************************************/

void BeebWin::LoadSerialPortPreferences(int Version)
{
	if (m_Preferences.GetStringValue(CFG_SERIAL_PORT, m_SerialPort))
	{
		// For backwards compatibility with Preferences.cfg files from
		// BeebEm 4.18 and earlier, which stored the port number as a
		// binary value
		if (m_SerialPort.size() == 2 &&
		    isxdigit(m_SerialPort[0]) &&
		    isxdigit(m_SerialPort[1]))
		{
			int Port;
			sscanf(m_SerialPort.c_str(), "%x", &Port);

			char PortName[20];
			sprintf(PortName, "COM%d", Port);
			m_SerialPort = PortName;
		}
	}
	else
	{
		m_SerialPort = "COM2";
	}

	m_Preferences.GetBoolValue(CFG_SERIAL_PORT_ENABLED, SerialPortEnabled, false);

	bool TouchScreenEnabled;

	m_Preferences.GetBoolValue(CFG_TOUCH_SCREEN_ENABLED, TouchScreenEnabled, false);

	bool IP232Enabled;
	bool IP232localhost;
	bool IP232custom;

	m_Preferences.GetBoolValue(CFG_IP232_ENABLED, IP232Enabled, false);
	m_Preferences.GetBoolValue(CFG_IP232_LOCALHOST_OLD, IP232localhost, false);
	m_Preferences.GetBoolValue(CFG_IP232_CUSTOM_OLD, IP232custom, false);

	if (TouchScreenEnabled)
	{
		SerialDestination = SerialType::TouchScreen;
	}
	else if (IP232Enabled || IP232localhost || IP232custom)
	{
		SerialDestination = SerialType::IP232;
	}
	else
	{
		SerialDestination = SerialType::SerialPort;
	}

	char IPAddress[MAX_PATH];

	if (m_Preferences.GetStringValue(CFG_IP232_ADDRESS, IPAddress))
	{
		strcpy(IP232Address, IPAddress);
	}
	else if (m_Preferences.GetStringValue(CFG_IP232_CUSTOM_IP_OLD, IPAddress) && IP232custom)
	{
		strcpy(IP232Address, IPAddress);
	}
	else
	{
		strcpy(IP232Address, "127.0.0.1");
	}

	if (Version >= 3)
	{
		int Value;

		// From BeebEm 4.20, port numbers are stored in decimal.
		m_Preferences.GetDecimalValue(CFG_IP232_PORT, Value, 25232);

		if (Value >= 0 && Value <= 65535)
		{
			IP232Port = Value;
		}
		else
		{
			Value = 25232;
		}
	}
	else
	{
		DWORD Value;

		if (m_Preferences.GetDWORDValue(CFG_IP232_PORT, Value))
		{
			IP232Port = Value;
		}
		else if (m_Preferences.GetDWORDValue(CFG_IP232_CUSTOM_PORT_OLD, Value) && IP232custom)
		{
			IP232Port = Value;
		}
		else
		{
			IP232Port = 25232;
		}
	}

	if (!m_Preferences.GetBoolValue(CFG_IP232_HANDSHAKE, IP232Handshake, false))
	{
		m_Preferences.GetBoolValue(CFG_IP232_HANDSHAKE_OLD, IP232Handshake, false);
	}

	if (!m_Preferences.GetBoolValue(CFG_IP232_RAW, IP232Raw, false))
	{
		m_Preferences.GetBoolValue(CFG_IP232_RAW_OLD, IP232Raw, false);
	}
}

/****************************************************************************/

void BeebWin::LoadTeletextAdapterPreferences(int Version)
{
	m_Preferences.GetBoolValue(CFG_TELETEXT_ADAPTER_ENABLED, TeletextAdapterEnabled, false);

	std::string SourceValue;

	if (m_Preferences.GetStringValue(CFG_TELETEXT_ADAPTER_SOURCE, SourceValue, TeletextSourceTypeStr[0]))
	{
		TeletextSource = static_cast<TeletextSourceType>(FindEnum(SourceValue, TeletextSourceTypeStr, 0));
	}
	else
	{
		bool TeletextLocalhost;
		bool TeletextCustom;

		m_Preferences.GetBoolValue(CFG_TELETEXT_LOCALHOST_OLD, TeletextLocalhost, false);
		m_Preferences.GetBoolValue(CFG_TELETEXT_CUSTOM_IP_OLD, TeletextCustom, false);

		if (!(TeletextLocalhost || TeletextCustom))
		{
			TeletextSource = TeletextSourceType::File;
		}
		else
		{
			TeletextSource = TeletextSourceType::IP;
		}
	}

	char key[20];

	for (int ch = 0; ch < TELETEXT_CHANNEL_COUNT; ch++)
	{
		sprintf(key, CFG_TELETEXT_FILE, ch);

		if (!m_Preferences.GetStringValue(key, TeletextFileName[ch]))
		{
			char DiscsPath[MAX_PATH];
			m_Preferences.GetStringValue("DiscsPath", DiscsPath);
			GetDataPath(m_UserDataPath, DiscsPath);

			char TeletextFile[MAX_PATH];
			MakeFileName(TeletextFile, MAX_PATH, DiscsPath, "txt%d.dat", ch);

			TeletextFileName[ch] = TeletextFile;
		}

		std::string TeletextIPAddress;

		if (Version >= 3)
		{
			sprintf(key, CFG_TELETEXT_IP, ch);

			if (m_Preferences.GetStringValue(key, TeletextIPAddress))
			{
				TeletextIP[ch] = TeletextIPAddress;
			}
			else
			{
				TeletextIP[ch] = "127.0.0.1";
			}
		}
		else
		{
			sprintf(key, CFG_TELETEXT_IP_OLD, ch);

			if (m_Preferences.GetStringValue(key, TeletextIPAddress))
			{
				TeletextIP[ch] = TeletextIPAddress;
			}
			else
			{
				TeletextIP[ch] = "127.0.0.1";
			}
		}

		if (Version >= 3)
		{
			int Value;

			sprintf(key, CFG_TELETEXT_PORT, ch);

			// From BeebEm 4.20, port numbers are stored in decimal.
			m_Preferences.GetDecimalValue(key, Value, TELETEXT_BASE_PORT + ch);

			if (Value >= 0 && Value <= 65535)
			{
				TeletextPort[ch] = (u_short)Value;
			}
			else
			{
				TeletextPort[ch] = (u_short)(TELETEXT_BASE_PORT + ch);
			}
		}
		else
		{
			DWORD Value;

			sprintf(key, CFG_TELETEXT_PORT_OLD, ch);

			m_Preferences.GetDWORDValue(key, Value, TELETEXT_BASE_PORT + ch);

			if (Value >= 0 && Value <= 65535)
			{
				TeletextPort[ch] = (u_short)Value;
			}
			else
			{
				TeletextPort[ch] = (u_short)(TELETEXT_BASE_PORT + ch);
			}
		}
	}
}

/****************************************************************************/

void BeebWin::LoadCapturePreferences(int Version)
{
	if (Version >= 3)
	{
		std::string Value;

		m_Preferences.GetStringValue(CFG_BITMAP_CAPTURE_RESOLUTION, Value, BitmapCaptureResolutionStr[2]);

		m_BitmapCaptureResolution = static_cast<BitmapCaptureResolution>(FindEnum(Value, BitmapCaptureResolutionStr, 2));
	}
	else
	{
		DWORD Value;

		// BeebEm 4.19 and earlier stored a menu item ID.
		m_Preferences.GetDWORDValue(CFG_BITMAP_CAPTURE_RESOLUTION, Value, 2);

		switch (Value)
		{
			case 40262:          m_BitmapCaptureResolution = BitmapCaptureResolution::Display; break;
			case 40263:          m_BitmapCaptureResolution = BitmapCaptureResolution::_1280x1024; break;
			case 40264: default: m_BitmapCaptureResolution = BitmapCaptureResolution::_640x512; break;
			case 40265:          m_BitmapCaptureResolution = BitmapCaptureResolution::_320x256; break;
		}
	}

	if (Version >= 3)
	{
		std::string Value;

		m_Preferences.GetStringValue(CFG_BITMAP_CAPTURE_FORMAT, Value, BitmapCaptureFormatStr[0]);

		m_BitmapCaptureFormat = static_cast<BitmapCaptureFormat>(FindEnum(Value, BitmapCaptureFormatStr, 0));
	}
	else
	{
		DWORD Value;

		// BeebEm 4.19 and earlier stored a menu item ID.
		m_Preferences.GetDWORDValue(CFG_BITMAP_CAPTURE_FORMAT, Value, 40266);

		switch (Value)
		{
			case 40266: default: m_BitmapCaptureFormat = BitmapCaptureFormat::Bmp; break;
			case 40267:          m_BitmapCaptureFormat = BitmapCaptureFormat::Jpeg; break;
			case 40268:          m_BitmapCaptureFormat = BitmapCaptureFormat::Gif; break;
			case 40269:          m_BitmapCaptureFormat = BitmapCaptureFormat::Png; break;
		}
	}

	if (Version >= 3)
	{
		std::string Value;

		m_Preferences.GetStringValue(CFG_VIDEO_CAPTURE_RESOLUTION, Value, VideoCaptureResolutionStr[1]);

		m_VideoCaptureResolution = static_cast<VideoCaptureResolution>(FindEnum(Value, VideoCaptureResolutionStr, 1));
	}
	else
	{
		DWORD Value;

		// BeebEm 4.19 and earlier stored a menu item ID.
		m_Preferences.GetDWORDValue(CFG_VIDEO_CAPTURE_RESOLUTION, Value, 40186);

		switch (Value)
		{
			case 40185:          m_VideoCaptureResolution = VideoCaptureResolution::Display;
			case 40186: default: m_VideoCaptureResolution = VideoCaptureResolution::_640x512;
			case 40187:          m_VideoCaptureResolution = VideoCaptureResolution::_320x256;
		}
	}

	if (Version >= 3)
	{
		int Value;

		m_Preferences.GetDecimalValue(CFG_VIDEO_CAPTURE_FRAME_SKIP, Value, 1);

		if (Value >= 0 && Value <= 5)
		{
			m_AviFrameSkip = Value;
		}
		else
		{
			m_AviFrameSkip = 1;
		}
	}
	else
	{
		DWORD Value;

		// BeebEm 4.19 and earlier stored a menu item ID.
		m_Preferences.GetDWORDValue(CFG_VIDEO_CAPTURE_FRAME_SKIP_OLD, Value, 40189);

		switch (Value)
		{
			case 40188:          m_AviFrameSkip = 0; break;
			case 40189: default: m_AviFrameSkip = 1; break;
			case 40190:          m_AviFrameSkip = 2; break;
			case 40191:          m_AviFrameSkip = 3; break;
			case 40192:          m_AviFrameSkip = 4; break;
			case 40193:          m_AviFrameSkip = 5; break;
		}
	}
}

/****************************************************************************/

void BeebWin::LoadDiskPreferences()
{
	m_Preferences.GetBoolValue(CFG_WRITE_PROTECT_ON_LOAD, m_WriteProtectOnLoad, true);
	m_Preferences.GetBoolValue(CFG_FLOPPY_DRIVE_ENABLED, Disc8271Enabled, true);

	Disc1770Enabled = Disc8271Enabled;

	m_Preferences.GetBoolValue(CFG_SCSI_DRIVE_ENABLED, SCSIDriveEnabled, false);
	m_Preferences.GetBoolValue(CFG_IDE_DRIVE_ENABLED, IDEDriveEnabled, false);
}

/****************************************************************************/

void BeebWin::LoadUserPortRTCPreferences()
{
	if (!m_Preferences.GetBoolValue(CFG_USER_PORT_RTC_ENABLED, UserPortRTCEnabled, false))
	{
		m_Preferences.GetBoolValue(CFG_USER_PORT_RTC_ENABLED_OLD, UserPortRTCEnabled, false);
	}

	if (!m_Preferences.GetBinaryValue(CFG_USER_PORT_RTC_REGISTERS, UserPortRTCRegisters, sizeof(UserPortRTCRegisters)))
	{
		ZeroMemory(UserPortRTCRegisters, sizeof(UserPortRTCRegisters));
	}
}

/****************************************************************************/

void BeebWin::LoadDebugPreferences()
{
	m_Preferences.GetBoolValue(CFG_WRITE_INSTRUCTION_COUNTS, m_WriteInstructionCounts, false);
}

/****************************************************************************/

void BeebWin::LoadKeyMapPreferences()
{
	m_Preferences.GetBoolValue(CFG_KEY_MAP_AS, m_KeyMapAS, false);
	m_Preferences.GetBoolValue(CFG_KEY_MAP_FUNC, m_KeyMapFunc, false);

	TranslateKeyMapping();

	m_Preferences.GetBoolValue(CFG_DISABLE_KEYS_BREAK, m_DisableKeysBreak, false);
	m_Preferences.GetBoolValue(CFG_DISABLE_KEYS_ESCAPE, m_DisableKeysEscape, false);
	m_Preferences.GetBoolValue(CFG_DISABLE_KEYS_SHORTCUT, m_DisableKeysShortcut, false);

	// Windows key enable/disable still comes from registry
	int Size = 24;
	unsigned char KeyData[24];

	if (RegGetBinaryValue(HKEY_LOCAL_MACHINE, CFG_KEYBOARD_LAYOUT,
	                      CFG_SCANCODE_MAP, KeyData, &Size) && Size == 24)
	{
		m_DisableKeysWindows = true;
	}
	else
	{
		m_DisableKeysWindows = false;
	}
}

/****************************************************************************/

void BeebWin::LoadAutoSavePreferences()
{
	m_Preferences.GetBoolValue(CFG_AUTO_SAVE_PREFS_CMOS, m_AutoSavePrefsCMOS, false);
	m_Preferences.GetBoolValue(CFG_AUTO_SAVE_PREFS_FOLDERS, m_AutoSavePrefsFolders, false);
	m_Preferences.GetBoolValue(CFG_AUTO_SAVE_PREFS_ALL, m_AutoSavePrefsAll, false);
}

/****************************************************************************/

void BeebWin::LoadCMOSPreferences()
{
	// CMOS RAM now in prefs file
	unsigned char CMOSData[50];

	if (m_Preferences.GetBinaryValue(CFG_CMOS_MASTER128, CMOSData, 50))
	{
		RTCSetCMOSData(Model::Master128, CMOSData, 50);
	}
	else
	{
		RTCSetCMOSDefaults(Model::Master128);
	}

	if (m_Preferences.GetBinaryValue(CFG_CMOS_MASTER_ET, CMOSData, 50))
	{
		RTCSetCMOSData(Model::MasterET, CMOSData, 50);
	}
	else
	{
		RTCSetCMOSDefaults(Model::MasterET);
	}
}

/****************************************************************************/

void BeebWin::LoadSWRAMPreferences()
{
	if (!m_Preferences.GetBinaryValue(CFG_SWRAM_WRITABLE, RomWritePrefs, 16))
	{
		for (int slot = 0; slot < 16; ++slot)
			RomWritePrefs[slot] = true;
	}

	m_Preferences.GetBoolValue(CFG_SWRAM_BOARD_ENABLED, SWRAMBoardEnabled, false);
}

/****************************************************************************/

void BeebWin::LoadFilePathPreferences()
{
	// Set file path defaults
	if (!m_Preferences.HasValue(CFG_DISCS_PATH)) {
		m_Preferences.SetStringValue(CFG_DISCS_PATH, "DiscIms");
	}

	if (!m_Preferences.HasValue(CFG_DISCS_FILTER)) {
		m_Preferences.SetDecimalValue(CFG_DISCS_FILTER, 0);
	}

	if (!m_Preferences.HasValue(CFG_TAPES_PATH)) {
		m_Preferences.SetStringValue(CFG_TAPES_PATH, "Tapes");
	}

	if (!m_Preferences.HasValue(CFG_STATES_PATH)) {
		m_Preferences.SetStringValue(CFG_STATES_PATH, "BeebState");
	}

	if (!m_Preferences.HasValue(CFG_AVI_PATH)) {
		m_Preferences.SetStringValue(CFG_AVI_PATH, "");
	}

	if (!m_Preferences.HasValue(CFG_IMAGE_PATH)) {
		m_Preferences.SetStringValue(CFG_IMAGE_PATH, "");
	}

	if (!m_Preferences.HasValue(CFG_HARD_DRIVE_PATH)) {
		m_Preferences.SetStringValue(CFG_HARD_DRIVE_PATH, "DiscIms");
	}

	// Set FDC defaults if not already set
	for (int machine = 0; machine < static_cast<int>(Model::Master128); ++machine)
	{
		char CfgName[256];
		sprintf(CfgName, CFG_FDC_DLL, machine);

		if (!m_Preferences.HasValue(CfgName))
		{
			// Default B+ to Acorn FDC
			if (machine == static_cast<int>(Model::BPlus))
			{
				m_Preferences.SetStringValue(CfgName, "Hardware\\Acorn1770.dll");
			}
			else
			{
				m_Preferences.SetStringValue(CfgName, "None");
			}
		}
	}
}

/****************************************************************************/

void BeebWin::LoadUserPortBreakoutPreferences()
{
	char Keys[8];

	if (m_Preferences.GetBinaryValue(CFG_BIT_KEYS, Keys, 8))
	{
		for (int i = 0; i < 8; i++)
		{
			BitKeys[i] = Keys[i];
		}
	}
}

/****************************************************************************/

void BeebWin::SavePreferences(bool saveAll)
{
	if (saveAll)
	{
		// Update prefs version
		m_Preferences.SetDecimalValue(CFG_PREFERENCES_VERSION, PREFERENCES_VERSION);

		// Remove obsolete prefs
		m_Preferences.EraseValue("UserKeyMapRow");
		m_Preferences.EraseValue("UserKeyMapCol");
		m_Preferences.EraseValue("ShowBottomCursorLine");
		m_Preferences.EraseValue("Volume");
		m_Preferences.EraseValue("UsePrimaryBuffer");
		m_Preferences.EraseValue("OpCodes");

		// Hardware
		m_Preferences.SetStringValue(CFG_MACHINE_TYPE, MachineTypeStr[(int)MachineType]);
		m_Preferences.SetBoolValue(CFG_BASIC_HARDWARE_ONLY, BasicHardwareOnly);
		m_Preferences.EraseValue(CFG_BASIC_HARDWARE_ONLY_OLD);

		#if ENABLE_SPEECH
		m_Preferences.SetBoolValue(CFG_SPEECH_ENABLED, SpeechEnabled);
		#else
		m_Preferences.SetBoolValue(CFG_SPEECH_ENABLED, false);
		#endif

		m_Preferences.SetBoolValue(CFG_ECONET_ENABLED, EconetEnabled); // Rob
		m_Preferences.SetBinaryValue(CFG_KEYBOARD_LINKS, &KeyboardLinks, sizeof(KeyboardLinks));

		// Second processors
		m_Preferences.SetDecimalValue(CFG_TUBE_TYPE, (int)TubeType);
		m_Preferences.EraseValue(CFG_TUBE_ENABLED_OLD);
		m_Preferences.EraseValue(CFG_TUBE_ACORN_Z80_OLD);
		m_Preferences.EraseValue(CFG_TUBE_TORCH_Z80_OLD);
		m_Preferences.EraseValue(CFG_TUBE_186_OLD);
		m_Preferences.EraseValue(CFG_TUBE_ARM_OLD);

		// Window size and position
		m_Preferences.SetDecimalValue(CFG_VIEW_WIN_SIZE_X, m_XLastWinSize);
		m_Preferences.SetDecimalValue(CFG_VIEW_WIN_SIZE_Y, m_YLastWinSize);
		m_Preferences.EraseValue(CFG_VIEW_WIN_SIZE);

		RECT Rect;
		GetWindowRect(m_hWnd, &Rect);

		m_Preferences.SetDecimalValue(CFG_VIEW_WINDOW_POS_X, Rect.left);
		m_Preferences.SetDecimalValue(CFG_VIEW_WINDOW_POS_Y, Rect.top);

		// Emulation speed
		m_Preferences.SetStringValue(CFG_SPEED_TIMING, TimingTypeStr[(int)m_TimingType]);
		m_Preferences.SetDecimalValue(CFG_SPEED, m_TimingSpeed);

		// Display
		m_Preferences.SetStringValue(CFG_DISPLAY_RENDERER, DisplayRendererTypeStr[(int)m_DisplayRenderer]);
		m_Preferences.SetBoolValue(CFG_FULL_SCREEN, m_FullScreen);
		m_Preferences.SetStringValue(CFG_DX_FULL_SCREEN_MODE, DirectXFullScreenModeStr[(int)m_DDFullScreenMode]);
		m_Preferences.EraseValue(CFG_DX_FULL_SCREEN_MODE_OLD);
		m_Preferences.SetBoolValue(CFG_MAINTAIN_ASPECT_RATIO, m_MaintainAspectRatio);
		m_Preferences.SetBoolValue(CFG_DX_SMOOTHING, m_DXSmoothing);
		m_Preferences.SetBoolValue(CFG_DX_SMOOTH_MODE7_ONLY, m_DXSmoothMode7Only);
		m_Preferences.SetBoolValue(CFG_TELETEXT_HALF_MODE, TeletextHalfMode);
		m_Preferences.EraseValue(CFG_TELETEXT_HALF_MODE_OLD);
		m_Preferences.SetDecimalValue(CFG_MOTION_BLUR, (int)m_MotionBlur);
		m_Preferences.SetBinaryValue(CFG_MOTION_BLUR_INTENSITIES, m_BlurIntensities, 8);

		// Sound
		m_Preferences.SetStringValue(CFG_SOUND_STREAMER, SoundStreamerTypeStr[(int)SelectedSoundStreamer]);
		m_Preferences.EraseValue(CFG_SOUND_STREAMER_OLD);
		m_Preferences.SetDecimalValue(CFG_SOUND_SAMPLE_RATE, SoundSampleRate);
		m_Preferences.SetDecimalValue(CFG_SOUND_VOLUME, SoundVolume);
		m_Preferences.SetBoolValue(CFG_SOUND_EXPONENTIAL_VOLUME, SoundExponentialVolume);
		m_Preferences.SetBoolValue(CFG_SOUND_ENABLED, SoundDefault);
		m_Preferences.SetBoolValue(CFG_SOUND_CHIP_ENABLED, SoundChipEnabled);
		m_Preferences.SetBoolValue(CFG_RELAY_SOUND_ENABLED, RelaySoundEnabled);
		m_Preferences.SetBoolValue(CFG_TAPE_SOUND_ENABLED, TapeSoundEnabled);
		m_Preferences.SetBoolValue(CFG_DISC_SOUND_ENABLED, DiscDriveSoundEnabled);
		m_Preferences.SetBoolValue(CFG_SOUND_PART_SAMPLES, PartSamples);
		m_Preferences.EraseValue(CFG_SOUND_PART_SAMPLES_OLD);
		m_Preferences.SetBoolValue(CFG_MUSIC5000_ENABLED, Music5000Enabled);

		// Keyboard and joystick
		m_Preferences.SetStringValue(CFG_OPTIONS_STICKS, JoystickOptionStr[(int)m_JoystickOption]);
		m_Preferences.SetStringValue(CFG_OPTIONS_KEY_MAPPING, KeyboardMappingTypeStr[(int)m_KeyboardMapping]);
		m_Preferences.SetStringValue(CFG_OPTIONS_USER_KEY_MAP_FILE, m_UserKeyMapPath);

		// AMX mouse
		m_Preferences.SetBoolValue(CFG_OPTIONS_CAPTURE_MOUSE, m_CaptureMouse);
		m_Preferences.SetBoolValue(CFG_AMX_ENABLED, AMXMouseEnabled);
		m_Preferences.SetBoolValue(CFG_AMX_LRFORMIDDLE, AMXLRForMiddle);
		m_Preferences.SetStringValue(CFG_AMX_SIZE, AMXSizeTypeStr[(int)m_AMXSize]);
		m_Preferences.SetDecimalValue(CFG_AMX_ADJUST, m_AMXAdjust);

		// Printer
		m_Preferences.SetBoolValue(CFG_PRINTER_ENABLED, PrinterEnabled);
		m_Preferences.SetStringValue(CFG_PRINTER_PORT, PrinterPortTypeStr[(int)m_PrinterPort]);
		m_Preferences.SetStringValue(CFG_PRINTER_FILE, m_PrinterFileName);

		// Text to speech
		m_Preferences.SetBoolValue(CFG_TEXT_TO_SPEECH_ENABLED, m_TextToSpeechEnabled);
		m_Preferences.SetBoolValue(CFG_TEXT_TO_SPEECH_AUTO_SPEAK, m_SpeechWriteChar);
		m_Preferences.SetBoolValue(CFG_TEXT_TO_SPEECH_PUNCTUATION, m_SpeechSpeakPunctuation);
		m_Preferences.SetDecimalValue(CFG_TEXT_TO_SPEECH_RATE, m_SpeechRate);

		// UI
		m_Preferences.SetBoolValue(CFG_SHOW_FPS, m_ShowSpeedAndFPS);
		m_Preferences.EraseValue(CFG_SHOW_FPS_OLD);
		m_Preferences.SetBoolValue(CFG_SHOW_KEYBOARD_LEDS, LEDs.ShowKB);
		m_Preferences.SetBoolValue(CFG_SHOW_DISC_LEDS, LEDs.ShowDisc);
		m_Preferences.SetDecimalValue(CFG_DISC_LED_COLOUR, (int)m_DiscLedColour);
		m_Preferences.EraseValue(CFG_LED_INFORMATION_OLD);
		m_Preferences.SetBoolValue(CFG_HIDE_MENU_ENABLED, m_HideMenuEnabled);
		m_Preferences.SetStringValue(CFG_VIEW_MONITOR, MonitorTypeStr[(int)m_MonitorType]);
		m_Preferences.SetBoolValue(CFG_OPTIONS_HIDE_CURSOR, m_HideCursor);
		m_Preferences.SetBoolValue(CFG_OPTIONS_FREEZEINACTIVE, m_FreezeWhenInactive);
		m_Preferences.SetBoolValue(CFG_TEXT_VIEW_ENABLED, m_TextViewEnabled);

		// Tape
		m_Preferences.SetDecimalValue(CFG_TAPE_CLOCK_SPEED, TapeState.ClockSpeed);
		m_Preferences.EraseValue(CFG_TAPE_CLOCK_SPEED_OLD);
		m_Preferences.SetBoolValue(CFG_UNLOCK_TAPE, TapeState.Unlock);

		// Serial port
		m_Preferences.SetStringValue(CFG_SERIAL_PORT, m_SerialPort);
		m_Preferences.SetBoolValue(CFG_SERIAL_PORT_ENABLED, SerialPortEnabled);
		m_Preferences.SetBoolValue(CFG_TOUCH_SCREEN_ENABLED, SerialDestination == SerialType::TouchScreen);
		m_Preferences.SetBoolValue(CFG_IP232_ENABLED, SerialDestination == SerialType::IP232);
		m_Preferences.EraseValue(CFG_IP232_LOCALHOST_OLD);
		m_Preferences.EraseValue(CFG_IP232_CUSTOM_OLD);
		m_Preferences.SetBoolValue(CFG_IP232_HANDSHAKE, IP232Handshake);
		m_Preferences.EraseValue(CFG_IP232_HANDSHAKE_OLD);
		m_Preferences.SetBoolValue(CFG_IP232_RAW, IP232Raw);
		m_Preferences.EraseValue(CFG_IP232_RAW_OLD);
		m_Preferences.SetStringValue(CFG_IP232_ADDRESS, IP232Address);
		m_Preferences.EraseValue(CFG_IP232_CUSTOM_IP_OLD);
		m_Preferences.SetDecimalValue(CFG_IP232_PORT, IP232Port);
		m_Preferences.EraseValue(CFG_IP232_CUSTOM_PORT_OLD);

		// Teletext adapter
		m_Preferences.SetBoolValue(CFG_TELETEXT_ADAPTER_ENABLED, TeletextAdapterEnabled);
		m_Preferences.SetStringValue(CFG_TELETEXT_ADAPTER_SOURCE, TeletextSourceTypeStr[(int)TeletextSource]);

		char key[20];

		for (int ch = 0; ch < TELETEXT_CHANNEL_COUNT; ch++)
		{
			sprintf(key, CFG_TELETEXT_FILE, ch);
			m_Preferences.SetStringValue(key, TeletextFileName[ch]);
			sprintf(key, CFG_TELETEXT_PORT, ch);
			m_Preferences.SetDecimalValue(key, TeletextPort[ch]);
			sprintf(key, CFG_TELETEXT_PORT_OLD, ch);
			m_Preferences.EraseValue(key);
			sprintf(key, CFG_TELETEXT_IP, ch);
			m_Preferences.SetStringValue(key, TeletextIP[ch]);
			sprintf(key, CFG_TELETEXT_IP_OLD, ch);
			m_Preferences.EraseValue(key);
		}

		m_Preferences.EraseValue(CFG_TELETEXT_LOCALHOST_OLD);
		m_Preferences.EraseValue(CFG_TELETEXT_CUSTOM_IP_OLD);

		// Image and video capture
		m_Preferences.SetStringValue(CFG_BITMAP_CAPTURE_RESOLUTION, BitmapCaptureResolutionStr[(int)m_BitmapCaptureResolution]);
		m_Preferences.SetStringValue(CFG_BITMAP_CAPTURE_FORMAT, BitmapCaptureFormatStr[(int)m_BitmapCaptureFormat]);
		m_Preferences.SetStringValue(CFG_VIDEO_CAPTURE_RESOLUTION, VideoCaptureResolutionStr[(int)m_VideoCaptureResolution]);
		m_Preferences.SetDecimalValue(CFG_VIDEO_CAPTURE_FRAME_SKIP, m_AviFrameSkip);
		m_Preferences.EraseValue(CFG_VIDEO_CAPTURE_FRAME_SKIP_OLD);

		// Floppy and hard drives
		m_Preferences.SetBoolValue(CFG_WRITE_PROTECT_ON_LOAD, m_WriteProtectOnLoad);
		m_Preferences.SetBoolValue(CFG_FLOPPY_DRIVE_ENABLED, Disc8271Enabled);
		m_Preferences.SetBoolValue(CFG_SCSI_DRIVE_ENABLED, SCSIDriveEnabled);
		m_Preferences.SetBoolValue(CFG_IDE_DRIVE_ENABLED, IDEDriveEnabled);

		// User port RTC
		m_Preferences.SetBoolValue(CFG_USER_PORT_RTC_ENABLED, UserPortRTCEnabled);
		m_Preferences.EraseValue(CFG_USER_PORT_RTC_ENABLED_OLD);

		// Debug
		m_Preferences.SetBoolValue(CFG_WRITE_INSTRUCTION_COUNTS, m_WriteInstructionCounts);

		// Key mappings
		m_Preferences.SetBoolValue(CFG_KEY_MAP_AS, m_KeyMapAS);
		m_Preferences.SetBoolValue(CFG_KEY_MAP_FUNC, m_KeyMapFunc);
		m_Preferences.SetBoolValue(CFG_DISABLE_KEYS_BREAK, m_DisableKeysBreak);
		m_Preferences.SetBoolValue(CFG_DISABLE_KEYS_ESCAPE, m_DisableKeysEscape);
		m_Preferences.SetBoolValue(CFG_DISABLE_KEYS_SHORTCUT, m_DisableKeysShortcut);

		// Sideways RAM
		for (int slot = 0; slot < 16; ++slot)
			RomWritePrefs[slot] = RomWritable[slot];
		m_Preferences.SetBinaryValue(CFG_SWRAM_WRITABLE, RomWritePrefs, 16);
		m_Preferences.SetBoolValue(CFG_SWRAM_BOARD_ENABLED, SWRAMBoardEnabled);

		// User port breakout box
		char KeyData[256];

		for (int Key = 0; Key < 8; Key++)
		{
			KeyData[Key] = static_cast<char>(BitKeys[Key]);
		}

		m_Preferences.SetBinaryValue(CFG_BIT_KEYS, KeyData, 8);
	}

	// CMOS RAM now in prefs file
	if (saveAll || m_AutoSavePrefsCMOS)
	{
		// CMOS
		m_Preferences.SetBinaryValue(CFG_CMOS_MASTER128, RTCGetCMOSData(Model::Master128), 50);
		m_Preferences.SetBinaryValue(CFG_CMOS_MASTER_ET, RTCGetCMOSData(Model::MasterET), 50);

		m_Preferences.SetBinaryValue(CFG_USER_PORT_RTC_REGISTERS, UserPortRTCRegisters, sizeof(UserPortRTCRegisters));
	}

	// Preferences auto-save
	m_Preferences.SetBoolValue(CFG_AUTO_SAVE_PREFS_CMOS, m_AutoSavePrefsCMOS);
	m_Preferences.SetBoolValue(CFG_AUTO_SAVE_PREFS_FOLDERS, m_AutoSavePrefsFolders);
	m_Preferences.SetBoolValue(CFG_AUTO_SAVE_PREFS_ALL, m_AutoSavePrefsAll);

	if (m_Preferences.Save(m_PrefsFileName.c_str()) == Preferences::Result::Success)
	{
		m_AutoSavePrefsChanged = false;
	}
	else
	{
		Report(MessageType::Error,
		       "Failed to write preferences file:\n  %s",
		       m_PrefsFileName.c_str());
	}
}
