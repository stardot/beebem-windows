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

/* Mike Wyatt and NRM's port to win32 - 07/06/1997 */

#ifndef BEEBWIN_HEADER
#define BEEBWIN_HEADER

#include <stdarg.h>
#include <stdlib.h>
#include <string>
#include <string.h>

#include <windows.h>
#include <d3dx9.h>
#include <ddraw.h>
#include <sapi.h>

#include "DiscType.h"
#include "KeyMap.h"
#include "model.h"
#include "port.h"
#include "preferences.h"
#include "video.h"

// Registry defs for disabling windows keys
#define CFG_KEYBOARD_LAYOUT "SYSTEM\\CurrentControlSet\\Control\\Keyboard Layout"
#define CFG_SCANCODE_MAP "Scancode Map"

extern const char *WindowTitle;

typedef union EightUChars {
	unsigned char data[8];
	EightByteType eightbyte;
} EightUChars;

typedef union SixteenUChars {
	unsigned char data[16];
	EightByteType eightbytes[2];
} SixteenUChars;

typedef struct bmiData
{
	BITMAPINFOHEADER	bmiHeader;
	RGBQUAD			bmiColors[256];
} bmiData;

struct LEDType {
	bool ShiftLock;
	bool CapsLock;
	bool Motor;
	bool Disc0;
	bool Disc1;
	bool HDisc[4];
	bool ShowDisc;
	bool ShowKB;
};
extern struct LEDType LEDs;

enum class LEDColour {
	Red,
	Green
};

extern LEDColour DiscLedColour;

enum TextToSpeechSearchDirection
{
	TTS_FORWARDS,
	TTS_BACKWARDS
};

enum TextToSpeechSearchType
{
	TTS_CHAR,
	TTS_BLANK,
	TTS_NONBLANK,
	TTS_ENDSENTENCE
};

// A structure for our custom vertex type. We added texture coordinates
struct CUSTOMVERTEX
{
	D3DXVECTOR3 position; // The position
	D3DCOLOR	color;	  // The color
	FLOAT		tu, tv;	  // The texture coordinates
};

// Our custom FVF, which describes our custom vertex structure
#define D3DFVF_CUSTOMVERTEX (D3DFVF_XYZ|D3DFVF_DIFFUSE|D3DFVF_TEX1)

enum class MessageType {
	Error,
	Warning,
	Info,
	Question,
	Confirm
};

enum class MessageResult {
	None,
	Yes,
	No,
	OK,
	Cancel
};

enum class PaletteType : char {
	RGB,
	BW,
	Amber,
	Green,
	Last
};

class BeebWin {

public:
	BeebWin();
	~BeebWin();

	bool Initialise();
	void ApplyPrefs();
	void Shutdown();

	static LRESULT CALLBACK WndProc(HWND hWnd,
	                                  UINT nMessage,
	                                  WPARAM wParam,
	                                  LPARAM lParam);

	LRESULT WndProc(UINT nMessage, WPARAM wParam, LPARAM lParam);

	void UpdateModelMenu();
	void SetSoundMenu(void);
	void SetImageName(const char *DiscName, int Drive, DiscType Type);
	void SetTapeSpeedMenu(void);
	void SetRomMenu(); // LRW  Added for individual ROM/RAM
	void UpdateTubeMenu();
	void SelectFDC();
	void LoadFDC(char *DLLName, bool save);
	void KillDLLs(void);
	void UpdateLEDMenu();
	void SetDriveControl(unsigned char value);
	unsigned char GetDriveControl(void);
	void doLED(int sx,bool on);
	void updateLines(HDC hDC, int starty, int nlines);
	void updateLines(int starty, int nlines) {
		updateLines(m_hDC, starty, nlines);
	}

	void doHorizLine(int Colour, int y, int sx, int width) {
		if (TeletextEnabled) y/=TeletextStyle;
		int d = (y*800)+sx+ScreenAdjust+(TeletextEnabled?36:0);
		if ((d+width)>(500*800)) return;
		if (d<0) return;
		memset(m_screen+d, Colour, width);
	}

	void doInvHorizLine(int Colour, int y, int sx, int width) {
		if (TeletextEnabled) y/=TeletextStyle;
		int d = (y*800)+sx+ScreenAdjust+(TeletextEnabled?36:0);
		char *vaddr;
		if ((d+width)>(500*800)) return;
		if (d<0) return;
		vaddr=m_screen+d;
		for (int n = 0; n < width; n++) *(vaddr+n) ^= Colour;
	}

	void doUHorizLine(int Colour, int y, int sx, int width) {
		if (TeletextEnabled) y/=TeletextStyle;
		if (y>500) return;
		memset(m_screen+ (y* 800) + sx, Colour, width);
	}

	EightUChars *GetLinePtr(int y) {
		int d = (y*800)+ScreenAdjust;
		if (d > (MAX_VIDEO_SCAN_LINES*800))
			return((EightUChars *)(m_screen+(MAX_VIDEO_SCAN_LINES*800)));
		return((EightUChars *)(m_screen + d));
	}

	SixteenUChars *GetLinePtr16(int y) {
		int d = (y*800)+ScreenAdjust;
		if (d > (MAX_VIDEO_SCAN_LINES*800))
			return((SixteenUChars *)(m_screen+(MAX_VIDEO_SCAN_LINES*800)));
		return((SixteenUChars *)(m_screen + d));
	}

	HWND GethWnd() { return m_hWnd; }

	void ResetBeebSystem(Model NewModelType, bool LoadRoms);
	void Break();

	void CreateArmCoPro();
	void DestroyArmCoPro();
	void CreateSprowCoPro();
	void DestroySprowCoPro();

	int StartOfFrame(void);
	bool UpdateTiming();
	void AdjustSpeed(bool up);
	bool ShouldDisplayTiming() const;
	void DisplayTiming();
	void UpdateWindowTitle();
	bool IsWindowMinimized() const;
	void DisplayClientAreaText(HDC hdc);
	void DisplayFDCBoardInfo(HDC hDC, int x, int y);
	void ScaleJoystick(unsigned int x, unsigned int y);
	void SetMousestickButton(int index, bool button);
	void ScaleMousestick(unsigned int x, unsigned int y);
	void HandleCommand(UINT MenuID);
	void SetAMXPosition(unsigned int x, unsigned int y);
	void ChangeAMXPosition(int deltaX, int deltaY);
	void CaptureMouse();
	void ReleaseMouse();
	void Activate(bool Active);
	void Focus(bool Focus);
	void WinSizeChange(WPARAM size, int width, int height);
	void WinPosChange(int x, int y);
	bool IsFrozen();
	void TogglePause();
	bool IsPaused();
	void OpenUserKeyboardDialog();
	void UserKeyboardDialogClosed();
	void ShowMenu(bool on);
	void HideMenu(bool hide);
	void TrackPopupMenu(int x, int y);
	bool IsFullScreen() { return m_isFullScreen; }
	void ResetTiming(void);
	int TranslateKey(int vkey, bool keyUp, int &row, int &col);
	void ParseCommandLine(void);
	void CheckForLocalPrefs(const char *path, bool bLoadPrefs);
	void FindCommandLineFile(char *CmdLineFile);
	void HandleCommandLineFile(int Drive, const char *CmdLineFile);
	void HandleEnvironmentVariables();
	void LoadStartupDisc(int DriveNum, const char *DiscString);
	bool CheckUserDataPath(bool Persist);
	void SelectUserDataPath(void);
	void StoreUserDataPath(void);
	bool NewTapeImage(char *FileName, int Size);
	const char *GetAppPath() const { return m_AppPath; }
	const char *GetUserDataPath() const { return m_UserDataPath; }
	void GetDataPath(const char *folder, char *path);
	void QuickLoad();
	void QuickSave();
	void LoadUEFState(const char *FileName);
	void SaveUEFState(const char *FileName);
	void LoadUEFTape(const char *FileName);
	void LoadCSWTape(const char *FileName);

	void Speak(const char *text, DWORD flags);
	void SpeakChar(unsigned char c);
	void TextToSpeechKey(WPARAM uParam);
	void TextViewSpeechSync(void);
	void TextViewSyncWithBeebCursor(void);
	void HandleTimer(void);
	void doCopy(void);
	void doPaste(void);
	void ClearClipboardBuffer();
	void CopyKey(unsigned char Value);
	void CaptureBitmapPending(bool autoFilename);
	void DoShiftBreak();
	bool HasKbdCmd() const;
	void SetKeyboardTimer();
	void SetBootDiscTimer();
	void KillBootDiscTimer();

	void SaveEmuUEF(FILE *SUEF);
	void LoadEmuUEF(FILE *SUEF,int Version);

	bool InitClass();
	void UpdateOptiMenu();
	void CreateBeebWindow(void);
	void DisableRoundedCorners(HWND hWnd);
	void FlashWindow();
	void CreateBitmap(void);
	void InitMenu();
	void UpdateMonitorMenu();
	void DisableSerial();
	void SelectSerialPort(const char *PortName);
	void UpdateSerialMenu();
	void UpdateEconetMenu();

	void UpdateSFXMenu();
	void UpdateDisableKeysMenu();
	void UpdateDisplayRendererMenu();

	void UpdateSoundStreamerMenu();

	void CheckMenuItem(UINT id, bool checked);
	void EnableMenuItem(UINT id, bool enabled);

	// DirectX - calls DDraw or DX9 fn
	void InitDX();
	void ResetDX();
	void ReinitDX();
	void ExitDX();
	void UpdateSmoothing();

	// DirectDraw
	HRESULT InitDirectDraw();
	HRESULT InitSurfaces();
	void ResetSurfaces();

	// DirectX9
	HRESULT InitDX9();
	void ExitDX9();
	void RenderDX9();

	void TranslateWindowSize(void);
	void TranslateDDSize(void);
	void CalcAspectRatioAdjustment(int DisplayWidth, int DisplayHeight);
	void TranslateSampleRate(void);
	void TranslateVolume(void);
	void TranslateTiming();
	void SetRealTimeTarget(double RealTimeTarget);
	void TranslateKeyMapping(void);
	bool ReadDisc(int Drive, bool bCheckForPrefs);
	void Load1770DiscImage(const char *FileName, int Drive, DiscType Type);
	void LoadTape(void);
	void InitJoystick(void);
	void ResetJoystick(void);
	void RestoreState(void);
	void SaveState(void);
	void NewDiscImage(int Drive);
	void CreateDiscImage(const char *FileName, int DriveNum, int Heads, int Tracks);
	void EjectDiscImage(int Drive);
	void ExportDiscFiles(int menuId);
	void ImportDiscFiles(int menuId);
	void SelectHardDriveFolder();
	void ToggleWriteProtect(int Drive);
	void SetDiscWriteProtect(int Drive, bool WriteProtect);
	void SetDiscWriteProtects();
	void SetWindowAttributes(bool wasFullScreen);
	void TranslateAMX(void);
	bool PrinterFile();
	void TogglePrinter(void);
	void TranslatePrinterPort(void);

	// AVI recording
	void CaptureVideo();
	void EndVideo();

	void CaptureBitmap(int SourceX,
	                   int SourceY,
	                   int SourceWidth,
	                   int SourceHeight,
	                   bool Teletext);
	bool GetImageFile(char *FileName, int Size);
	bool GetImageEncoderClsid(const WCHAR *mimeType, CLSID *encoderClsid);

	void InitTextToSpeech();
	bool TextToSpeechSearch(TextToSpeechSearchDirection dir,
	                        TextToSpeechSearchType type);
	void TextToSpeechReadChar();
	void TextToSpeechReadWord();
	void TextToSpeechReadLine();
	void TextToSpeechReadSentence();
	void TextToSpeechReadScreen();

	void InitTextView();
	void TextView();
	void TextViewSetCursorPos(int line, int col);

	bool RebootSystem();
	void LoadUserKeyMap(void);
	void SaveUserKeyMap(void);

	MessageResult Report(MessageType type, const char *format, ...);
	MessageResult ReportV(MessageType type, const char *format, va_list args);

	bool RegCreateKey(HKEY hKeyRoot, LPCSTR lpSubKey);
	bool RegGetBinaryValue(HKEY hKeyRoot, LPCSTR lpSubKey, LPCSTR lpValue, void* pData, int* pnSize);
	bool RegSetBinaryValue(HKEY hKeyRoot, LPCSTR lpSubKey, LPCSTR lpValue, const void* pData, int* pnSize);
	bool RegGetStringValue(HKEY hKeyRoot, LPCSTR lpSubKey, LPCSTR lpValue, LPSTR pData, DWORD dwSize);
	bool RegSetStringValue(HKEY hKeyRoot, LPCSTR lpSubKey, LPCSTR lpValue, LPCSTR pData);
	void EditROMConfig(void);

	// Preferences
	void LoadPreferences();
	void SavePreferences(bool saveAll);

	// Main window
	HWND m_hWnd;
	char m_szTitle[256];
	bool m_isFullScreen;
	bool m_startFullScreen;

	// Menu
	HMENU m_hMenu;
	bool m_MenuOn;
	bool m_HideMenuEnabled;
	bool m_DisableMenu;

	// Timing
	bool m_ShowSpeedAndFPS;
	UINT m_MenuIDTiming;
	double m_RealTimeTarget;
	int m_CyclesPerSec;
	DWORD m_LastTickCount;
	DWORD m_LastStatsTickCount;
	int m_LastTotalCycles;
	int m_LastStatsTotalCycles;
	DWORD m_TickBase;
	int m_CycleBase;
	int m_MinFrameCount;
	DWORD m_LastFPSCount;
	int m_FPSTarget;
	int m_ScreenRefreshCount;
	double m_RelativeSpeed;
	double m_FramesPerSecond;

	// Pause / freeze emulation
	bool m_StartPaused;
	bool m_EmuPaused;
	bool m_WasPaused;

	bool m_FreezeWhenInactive;
	bool m_Frozen;

	// Window size
	UINT m_MenuIDWinSize;
	int m_XWinSize;
	int m_YWinSize;
	int m_XLastWinSize;
	int m_YLastWinSize;
	int m_XWinPos;
	int m_YWinPos;
	int m_XDXSize;
	int m_YDXSize;
	int m_XScrSize;
	int m_YScrSize;
	int m_XWinBorder;
	int m_YWinBorder;
	float m_XRatioAdj;
	float m_YRatioAdj;
	float m_XRatioCrop;
	float m_YRatioCrop;

	// Graphics rendering
	HDC m_hDC;
	HGDIOBJ m_hOldObj;
	HDC m_hDCBitmap;
	HGDIOBJ m_hBitmap;
	bmiData m_bmi;
	PaletteType m_PaletteType;
	char* m_screen;
	char* m_screen_blur;
	int m_LastStartY;
	int m_LastNLines;
	UINT m_MenuIDMotionBlur;
	char m_BlurIntensities[8];
	bool m_MaintainAspectRatio;
	int m_DisplayRenderer;
	int m_CurrentDisplayRenderer;
	int m_DDFullScreenMode;

	// DirectX stuff
	bool m_DXInit;
	bool m_DXResetPending;

	// DirectDraw stuff
	LPDIRECTDRAW m_DD; // DirectDraw object
	LPDIRECTDRAW2 m_DD2; // DirectDraw object
	LPDIRECTDRAWSURFACE m_DDSPrimary; // DirectDraw primary surface
	LPDIRECTDRAWSURFACE2 m_DDS2Primary; // DirectDraw primary surface
	LPDIRECTDRAWSURFACE m_DDSOne; // Offscreen surface 1
	LPDIRECTDRAWSURFACE2 m_DDS2One; // Offscreen surface 1
	bool m_DXSmoothing;
	bool m_DXSmoothMode7Only;
	LPDIRECTDRAWCLIPPER m_Clipper; // clipper for primary

	// Direct3D9 stuff
	LPDIRECT3D9 m_pD3D;
	LPDIRECT3DDEVICE9 m_pd3dDevice;
	LPDIRECT3DVERTEXBUFFER9 m_pVB;
	LPDIRECT3DTEXTURE9 m_pTexture;
	D3DXMATRIX m_TextureMatrix;

	// Audio
	UINT m_MenuIDSampleRate;
	UINT m_MenuIDVolume;

	// Joystick input
	bool m_JoystickCaptured;
	JOYCAPS m_JoystickCaps;
	UINT m_MenuIDSticks;

	// Mouse capture
	bool m_HideCursor;
	bool m_CaptureMouse;
	bool m_MouseCaptured;
	POINT m_RelMousePos;

	// Keyboard input
	UINT m_MenuIDKeyMapping;
	bool m_KeyMapAS;
	bool m_KeyMapFunc;
	char m_UserKeyMapPath[_MAX_PATH];
	bool m_DisableKeysWindows;
	bool m_DisableKeysBreak;
	bool m_DisableKeysEscape;
	bool m_DisableKeysShortcut;
	bool m_ShiftPressed;
	bool m_ShiftBooted;
	int m_vkeyPressed[256][2][2];

	// File paths
	char m_AppPath[_MAX_PATH];
	char m_UserDataPath[_MAX_PATH];
	bool m_CustomData;
	char m_DiscPath[_MAX_PATH]; // JGH
	bool m_WriteProtectDisc[2];
	bool m_WriteProtectOnLoad;

	// AMX mouse
	UINT m_MenuIDAMXSize;
	UINT m_MenuIDAMXAdjust;
	int m_AMXXSize;
	int m_AMXYSize;
	int m_AMXAdjust;

	// Preferences
	char m_PrefsFile[_MAX_PATH];
	Preferences m_Preferences;
	bool m_AutoSavePrefsCMOS;
	bool m_AutoSavePrefsFolders;
	bool m_AutoSavePrefsAll;
	bool m_AutoSavePrefsChanged;

	// Clipboard
	static const int ClipboardBufferSize = 32768;
	char m_ClipboardBuffer[ClipboardBufferSize];
	int m_ClipboardLength;
	int m_ClipboardIndex;

	// Printer
	char m_printerbuffer[1024 * 1024];
	int m_printerbufferlen;
	bool m_translateCRLF;

	UINT m_MenuIDPrinterPort;
	char m_PrinterFileName[_MAX_PATH];
	char m_PrinterDevice[_MAX_PATH];

	// Command line
	char m_CommandLineFileName1[_MAX_PATH];
	char m_CommandLineFileName2[_MAX_PATH];
	std::string m_DebugScriptFileName;
	std::string m_DebugLabelsFileName;

	// Startup key sequence
	std::string m_KbdCmd;
	int m_KbdCmdPos;
	int m_KbdCmdKey;
	bool m_KbdCmdPress;
	int m_KbdCmdDelay;
	int m_KbdCmdLastCycles;
	bool m_KeyboardTimerElapsed;

	// Disc auto-boot
	bool m_NoAutoBoot;
	int m_AutoBootDelay;
	bool m_AutoBootDisc;
	bool m_BootDiscTimerElapsed;

	// ROMs
	bool RomWritePrefs[16];

	// Bitmap capture
	ULONG_PTR m_gdiplusToken;
	bool m_CaptureBitmapPending;
	bool m_CaptureBitmapAutoFilename;
	char m_CaptureFileName[MAX_PATH];
	UINT m_MenuIDCaptureResolution;
	UINT m_MenuIDCaptureFormat;

	// AVI vars
	bmiData m_Avibmi;
	HBITMAP m_AviDIB;
	HDC m_AviDC;
	char* m_AviScreen;
	int m_AviFrameSkip;
	int m_AviFrameSkipCount;
	int m_AviFrameCount;
	UINT m_MenuIDAviResolution;
	UINT m_MenuIDAviSkip;

	// Text to speech variables
	bool m_TextToSpeechEnabled;
	ISpVoice *m_SpVoice;
	int m_SpeechLine;
	int m_SpeechCol;
	static const int MAX_SPEECH_LINE_LEN = 128;
	static const int MAX_SPEECH_SENTENCE_LEN = 128 * 25;
	static const int MAX_SPEECH_SCREEN_LEN = 128 * 32;
	char m_SpeechText[MAX_SPEECH_LINE_LEN + 1];
	bool m_SpeechSpeakPunctuation;
	bool m_SpeechWriteChar;
	static const int MAX_SPEECH_BUF_LEN = 160;
	char m_SpeechBuf[MAX_SPEECH_BUF_LEN + 1];
	int m_SpeechBufPos;
	int m_SpeechRate;

	// Text view variables
	HWND m_hTextView;
	bool m_TextViewEnabled;
	static const int MAX_TEXTVIEW_SCREEN_LEN = 128 * 32;
	char m_TextViewScreen[MAX_TEXTVIEW_SCREEN_LEN + 1];

	// Debug
	bool m_WriteInstructionCounts;
};

#endif
