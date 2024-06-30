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

#ifndef BEEBWIN_PREFS_HEADER
#define BEEBWIN_PREFS_HEADER

/****************************************************************************/

// Configuration file strings

const char* const CFG_PREFERENCES_VERSION          = "PrefsVersion";

// Hardware
const char* const CFG_MACHINE_TYPE                 = "MachineType";
const char* const CFG_BASIC_HARDWARE_ONLY          = "BasicHardwareOnly";
const char* const CFG_BASIC_HARDWARE_ONLY_OLD      = "Basic Hardware"; // For BeebEm v4.19 and earlier.
const char* const CFG_SPEECH_ENABLED               = "SpeechEnabled";
const char* const CFG_ECONET_ENABLED               = "EconetEnabled";
const char* const CFG_KEYBOARD_LINKS               = "KeyboardLinks";

// Second processors
const char* const CFG_TUBE_TYPE                    = "TubeType";
const char* const CFG_TUBE_ENABLED_OLD             = "TubeEnabled"; // For BeebEm 4.14 or earlier.
const char* const CFG_TUBE_ACORN_Z80_OLD           = "AcornZ80"; // For BeebEm 4.14 or earlier.
const char* const CFG_TUBE_TORCH_Z80_OLD           = "TorchTube"; // For BeebEm 4.14 or earlier.
const char* const CFG_TUBE_186_OLD                 = "Tube186Enabled"; // For BeebEm 4.14 or earlier.
const char* const CFG_TUBE_ARM_OLD                 = "ArmTube"; // For BeebEm 4.14 or earlier.

// Window size and position
const char* const CFG_VIEW_WIN_SIZE                = "WinSize";
const char* const CFG_VIEW_WIN_SIZE_X              = "WinSizeX";
const char* const CFG_VIEW_WIN_SIZE_Y              = "WinSizeY";
const char* const CFG_VIEW_WINDOW_POS_X            = "WinPosX";
const char* const CFG_VIEW_WINDOW_POS_Y            = "WinPosY";
const char* const CFG_VIEW_WINDOW_POS_OLD          = "WindowPos";

// Emulation speed
const char* const CFG_SPEED_TIMING                 = "Timing";
const char* const CFG_SPEED                        = "Speed";

// Display
const char* const CFG_DISPLAY_RENDERER             = "DisplayRenderer";
const char* const CFG_FULL_SCREEN                  = "FullScreen";
const char* const CFG_DX_FULL_SCREEN_MODE          = "DirectXFullScreenMode";
const char* const CFG_DX_FULL_SCREEN_MODE_OLD      = "DDFullScreenMode"; // For BeebEm v4.19 and earlier.
const char* const CFG_MAINTAIN_ASPECT_RATIO        = "MaintainAspectRatio";
const char* const CFG_DX_SMOOTHING                 = "DXSmoothing";
const char* const CFG_DX_SMOOTH_MODE7_ONLY         = "DXSmoothMode7Only";
const char* const CFG_TELETEXT_HALF_MODE           = "TeletextHalfMode";
const char* const CFG_TELETEXT_HALF_MODE_OLD       = "Teletext Half Mode"; // For BeebEm v4.19 and earlier.
const char* const CFG_MOTION_BLUR                  = "MotionBlur";
const char* const CFG_MOTION_BLUR_INTENSITIES      = "MotionBlurIntensities";

// Sound
const char* const CFG_SOUND_STREAMER               = "SoundStreamer";
const char* const CFG_SOUND_STREAMER_OLD           = "SoundConfig::Selection"; // For BeebEm v4.19 and earlier.
const char* const CFG_SOUND_SAMPLE_RATE            = "SampleRate";
const char* const CFG_SOUND_VOLUME                 = "SoundVolume";
const char* const CFG_SOUND_EXPONENTIAL_VOLUME     = "ExponentialVolume";
const char* const CFG_SOUND_ENABLED                = "SoundEnabled";
const char* const CFG_SOUND_CHIP_ENABLED           = "SoundChipEnabled";
const char* const CFG_RELAY_SOUND_ENABLED          = "RelaySoundEnabled";
const char* const CFG_TAPE_SOUND_ENABLED           = "TapeSoundEnabled";
const char* const CFG_DISC_SOUND_ENABLED           = "DiscDriveSoundEnabled";
const char* const CFG_SOUND_PART_SAMPLES           = "PartSamples";
const char* const CFG_SOUND_PART_SAMPLES_OLD       = "Part Samples"; // For BeebEm v4.19 and earlier.
const char* const CFG_MUSIC5000_ENABLED            = "Music5000Enabled";

// Keyboard and joystick
const char* const CFG_OPTIONS_STICKS               = "Sticks";
const char* const CFG_OPTIONS_KEY_MAPPING          = "KeyMapping";
const char* const CFG_OPTIONS_USER_KEY_MAP_FILE    = "UserKeyMapFile";

// AMX mouse
const char* const CFG_OPTIONS_CAPTURE_MOUSE        = "CaptureMouse";
const char* const CFG_AMX_ENABLED                  = "AMXMouseEnabled";
const char* const CFG_AMX_LRFORMIDDLE              = "AMXMouseLRForMiddle";
const char* const CFG_AMX_SIZE                     = "AMXMouseSize";
const char* const CFG_AMX_ADJUST                   = "AMXMouseAdjust";

// Printer
const char* const CFG_PRINTER_ENABLED              = "PrinterEnabled";
const char* const CFG_PRINTER_PORT                 = "PrinterPort";
const char* const CFG_PRINTER_FILE                 = "PrinterFile";

// Text to speech
const char* const CFG_TEXT_TO_SPEECH_ENABLED       = "TextToSpeechEnabled";
const char* const CFG_TEXT_TO_SPEECH_AUTO_SPEAK    = "TextToSpeechAutoSpeak";
const char* const CFG_TEXT_TO_SPEECH_PUNCTUATION   = "TextToSpeechSpeakPunctuation";
const char* const CFG_TEXT_TO_SPEECH_RATE          = "TextToSpeechRate";
const char* const CFG_TEXT_TO_SPEECH_VOICE         = "TextToSpeechVoice";

// UI
const char* const CFG_SHOW_FPS                     = "ShowFPS";
const char* const CFG_SHOW_FPS_OLD                 = "ShowFSP"; // For BeebEm v4.18 and earlier.
const char* const CFG_SHOW_KEYBOARD_LEDS           = "ShowKeyboardLEDs";
const char* const CFG_SHOW_DISC_LEDS               = "ShowDiscLEDs";
const char* const CFG_DISC_LED_COLOUR              = "DiscLEDColour";
const char* const CFG_LED_INFORMATION_OLD          = "LED Information"; // For BeebEm v4.19 and earlier.
const char* const CFG_HIDE_MENU_ENABLED            = "HideMenuEnabled";
const char* const CFG_VIEW_MONITOR                 = "Monitor";
const char* const CFG_OPTIONS_HIDE_CURSOR          = "HideCursor";
const char* const CFG_OPTIONS_FREEZEINACTIVE       = "FreezeWhenInactive";
const char* const CFG_TEXT_VIEW_ENABLED            = "TextViewEnabled";

// Tape
const char* const CFG_TAPE_CLOCK_SPEED             = "TapeClockSpeed";
const char* const CFG_TAPE_CLOCK_SPEED_OLD         = "Tape Clock Speed"; // For BeebEm v4.19 and earlier.
const char* const CFG_UNLOCK_TAPE                  = "UnlockTape";

// Serial port
const char* const CFG_SERIAL_PORT                  = "SerialPort";
const char* const CFG_SERIAL_PORT_ENABLED          = "SerialPortEnabled";
const char* const CFG_TOUCH_SCREEN_ENABLED         = "TouchScreenEnabled";
const char* const CFG_IP232_ENABLED                = "IP232Enabled";
const char* const CFG_IP232_LOCALHOST_OLD          = "IP232localhost"; // For BeebEm v4.18 and earlier.
const char* const CFG_IP232_CUSTOM_OLD             = "IP232custom"; // For BeebEm v4.18 and earlier.
const char* const CFG_IP232_HANDSHAKE              = "IP232Handshake";
const char* const CFG_IP232_HANDSHAKE_OLD          = "IP232mode"; // For BeebEm v4.18 and earlier.
const char* const CFG_IP232_RAW                    = "IP232Raw";
const char* const CFG_IP232_RAW_OLD                = "IP232raw"; // For BeebEm v4.18 and earlier.
const char* const CFG_IP232_ADDRESS                = "IP232Address";
const char* const CFG_IP232_CUSTOM_IP_OLD          = "IP232customip"; // For BeebEm v4.18 and earlier.
const char* const CFG_IP232_PORT                   = "IP232Port";
const char* const CFG_IP232_CUSTOM_PORT_OLD        = "IP232customport"; // For BeebEm v4.18 and earlier.

// Teletext adapter
const char* const CFG_TELETEXT_ADAPTER_ENABLED     = "TeletextAdapterEnabled";
const char* const CFG_TELETEXT_ADAPTER_SOURCE      = "TeletextAdapterSource";
const char* const CFG_TELETEXT_FILE                = "TeletextFile%d";
const char* const CFG_TELETEXT_PORT                = "TeletextPort%d";
const char* const CFG_TELETEXT_PORT_OLD            = "TeletextCustomPort%d"; // For BeebEm v4.19 and earlier.
const char* const CFG_TELETEXT_IP                  = "TeletextIP%d";
const char* const CFG_TELETEXT_IP_OLD              = "TeletextCustomIP%d"; // For BeebEm v4.19 and earlier.
const char* const CFG_TELETEXT_LOCALHOST_OLD       = "TeletextLocalhost"; // For BeebEm v4.19 and earlier.
const char* const CFG_TELETEXT_CUSTOM_IP_OLD       = "TeletextCustom"; // For BeebEm v4.19 and earlier.

// Image and video capture
const char* const CFG_BITMAP_CAPTURE_RESOLUTION    = "BitmapCaptureResolution";
const char* const CFG_BITMAP_CAPTURE_FORMAT        = "BitmapCaptureFormat";
const char* const CFG_VIDEO_CAPTURE_RESOLUTION     = "CaptureResolution";
const char* const CFG_VIDEO_CAPTURE_FRAME_SKIP     = "CaptureFrameSkip";
const char* const CFG_VIDEO_CAPTURE_FRAME_SKIP_OLD = "FrameSkip"; // For BeebEm v4.19 and earlier.

// Floppy and hard drives
const char* const CFG_WRITE_PROTECT_ON_LOAD        = "WriteProtectOnLoad";
const char* const CFG_FLOPPY_DRIVE_ENABLED         = "FloppyDriveEnabled";
const char* const CFG_SCSI_DRIVE_ENABLED           = "SCSIDriveEnabled";
const char* const CFG_IDE_DRIVE_ENABLED            = "IDEDriveEnabled";

// User port RTC
const char* const CFG_USER_PORT_RTC_ENABLED        = "UserPortRTCEnabled";
const char* const CFG_USER_PORT_RTC_ENABLED_OLD    = "RTCEnabled"; // For BeebEm v4.19 and earlier.
const char* const CFG_USER_PORT_RTC_REGISTERS      = "UserPortRTCRegisters";

// Debug
const char* const CFG_WRITE_INSTRUCTION_COUNTS     = "WriteInstructionCounts";

// Key mappings
const char* const CFG_KEY_MAP_AS                   = "KeyMapAS";
const char* const CFG_KEY_MAP_FUNC                 = "KeyMapFunc";
const char* const CFG_DISABLE_KEYS_BREAK           = "DisableKeysBreak";
const char* const CFG_DISABLE_KEYS_ESCAPE          = "DisableKeysEscape";
const char* const CFG_DISABLE_KEYS_SHORTCUT        = "DisableKeysShortcut";

// Preferences auto-save
const char* const CFG_AUTO_SAVE_PREFS_CMOS         = "AutoSavePrefsCMOS";
const char* const CFG_AUTO_SAVE_PREFS_FOLDERS      = "AutoSavePrefsFolders";
const char* const CFG_AUTO_SAVE_PREFS_ALL          = "AutoSavePrefsAll";

// CMOS
const char* const CFG_CMOS_MASTER128               = "CMOSRam";
const char* const CFG_CMOS_MASTER_ET               = "CMOSRamMasterET";

// Sideways RAM
const char* const CFG_SWRAM_WRITABLE               = "SWRAMWritable";
const char* const CFG_SWRAM_BOARD_ENABLED          = "SWRAMBoard";

// File paths
const char* const CFG_DISCS_PATH                   = "DiscsPath";
const char* const CFG_DISCS_FILTER                 = "DiscsFilter";
const char* const CFG_TAPES_PATH                   = "TapesPath";
const char* const CFG_STATES_PATH                  = "StatesPath";
const char* const CFG_AVI_PATH                     = "AVIPath";
const char* const CFG_IMAGE_PATH                   = "ImagePath";
const char* const CFG_HARD_DRIVE_PATH              = "HardDrivePath";
const char* const CFG_EXPORT_PATH                  = "ExportPath";
const char* const CFG_FDC_DLL                      = "FDCDLL%d";

// User port breakout box
const char* const CFG_BIT_KEYS                     = "BitKeys";

#endif
