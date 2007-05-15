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

#ifndef BEEBWIN_HEADER
#define BEEBWIN_HEADER

#include <string.h>
#include <stdlib.h>
#include <string>
#include <map>
#include <windows.h>
#include <d3dx9.h>
#include <ddraw.h>
#include <sapi.h>
#include "port.h"
#include "video.h"

/* Used in message boxes */
#define GETHWND (mainWin == NULL ? NULL : mainWin->GethWnd())

#define WM_REINITDX (WM_APP+1)

// Registry defs for disabling windows keys
#define CFG_KEYBOARD_LAYOUT "SYSTEM\\CurrentControlSet\\Control\\Keyboard Layout"
#define CFG_SCANCODE_MAP "Scancode Map"
static unsigned char CFG_DISABLE_WINDOWS_KEYS[24] = {
	00,00,00,00,00,00,00,00,03,00,00,00,00,00,0x5B,0xE0,00,00,0x5C,0xE0,00,00,00,00
};

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

typedef std::map<std::string, std::string> PrefsMap;


class BeebWin  {
  
  public:

	enum PaletteType { RGB, BW, AMBER, GREEN } palette_type;

	void Initialise();

	BeebWin();
	~BeebWin();
 
    void UpdateModelType();
	void SetSoundMenu(void);
	void SetPBuff(void);
	void SetImageName(char *DiscName,char Drive,char DType);
	void SetTapeSpeedMenu(void);
	void SetDiscWriteProtects(void);
	void SetRomMenu(void);				// LRW  Added for individual ROM/Ram
	void SelectFDC(void);
	void LoadFDC(char *DLLName, bool save);
	void KillDLLs(void);
	void UpdateLEDMenu(HMENU hMenu);
	void SetDriveControl(unsigned char value);
	unsigned char GetDriveControl(void);
	void doLED(int sx,bool on);
	void updateLines(HDC hDC, int starty, int nlines);
	void updateLines(int starty, int nlines)
		{ updateLines(m_hDC, starty, nlines); };

	void doHorizLine(unsigned long Col, int y, int sx, int width) {
		if (TeletextEnabled) y/=TeletextStyle;
		int d = (y*800)+sx+ScreenAdjust+(TeletextEnabled?36:0);
		if ((d+width)>(500*800)) return;
		if (d<0) return;
		memset(m_screen+d, Col, width);
	};

	void doInvHorizLine(unsigned long Col, int y, int sx, int width) {
		if (TeletextEnabled) y/=TeletextStyle;
		int d = (y*800)+sx+ScreenAdjust+(TeletextEnabled?36:0);
		char *vaddr;
		if ((d+width)>(500*800)) return;
		if (d<0) return;
		vaddr=m_screen+d;
		for (int n=0;n<width;n++) *(vaddr+n)^=Col;
	};

	void doUHorizLine(unsigned long Col, int y, int sx, int width) {
		if (TeletextEnabled) y/=TeletextStyle;
		if (y>500) return;
		memset(m_screen+ (y* 800) + sx, Col, width);
	};

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

	char *imageData(void) {
		return (m_screen+ScreenAdjust>m_screen)?m_screen+ScreenAdjust:m_screen;
	}

	HWND GethWnd() { return m_hWnd; };
	
	void RealizePalette(HDC) {};
	void ResetBeebSystem(unsigned char NewModelType,unsigned char TubeStatus,unsigned char LoadRoms);

	int StartOfFrame(void);
	BOOL UpdateTiming(void);
	void AdjustSpeed(bool up);
	void DisplayTiming(void);
	void DisplayClientAreaText(HDC hdc);
	void ScaleJoystick(unsigned int x, unsigned int y);
	void SetMousestickButton(int button);
	void ScaleMousestick(unsigned int x, unsigned int y);
	void HandleCommand(int MenuId);
	void SetAMXPosition(unsigned int x, unsigned int y);
	void Activate(BOOL);
	void Focus(BOOL);
	BOOL IsFrozen(void);
	void ShowMenu(bool on);
	void TrackPopupMenu(int x, int y);
	bool IsFullScreen() { return m_isFullScreen; }
	void ResetTiming(void);
	int TranslateKey(int, int, int&, int&);
	void ParseCommandLine(void);
	void HandleCommandLineFile(void);
	bool CheckUserDataPath(void);
	void NewTapeImage(char *FileName);
	const char *GetAppPath(void) { return m_AppPath; }
	const char *GetUserDataPath(void) { return m_UserDataPath; }
	void GetDataPath(const char *folder, char *path);
	void QuickLoad(void);
	void QuickSave(void);
	void Speak(const char *text, DWORD flags);
	void SpeakChar(unsigned char c);
	void TextToSpeechKey(WPARAM uParam);
	void TextViewSpeechSync(void);
	void TextViewSyncWithBeebCursor(void);

	unsigned char cols[256];
    HMENU		m_hMenu;
	BOOL		m_frozen;
	char*		m_screen;
	char*		m_screen_blur;
	double		m_RealTimeTarget;
	int			m_ShiftBooted;
	char		m_TextToSpeechEnabled;
	char		m_TextViewEnabled;
	char		m_DisableKeysWindows;
	char		m_DisableKeysBreak;
	char		m_DisableKeysEscape;
	char		m_DisableKeysShortcut;

  private:
	int			m_MenuIdWinSize;
	int			m_XWinSize;
	int			m_YWinSize;
	int			m_XWinPos;
	int			m_YWinPos;
	BOOL		m_ShowSpeedAndFPS;
	int			m_MenuIdSampleRate;
	int			m_MenuIdVolume;
	int			m_DiscTypeSelection;
	int			m_MenuIdTiming;
	int			m_FPSTarget;
	JOYCAPS		m_JoystickCaps;
	int			m_MenuIdSticks;
	BOOL		m_HideCursor;
	BOOL		m_FreezeWhenInactive;
	int			m_MenuIdKeyMapping;
	int			m_KeyMapAS;
	int			m_KeyMapFunc;
	int			m_ShiftPressed;
	int			m_vkeyPressed[256][2][2];
	char		m_AppPath[_MAX_PATH];
	char		m_UserDataPath[_MAX_PATH];
	BOOL		m_WriteProtectDisc[2];
	char		m_WriteProtectOnLoad;
	int			m_MenuIdAMXSize;
	int			m_MenuIdAMXAdjust;
	int			m_AMXXSize;
	int			m_AMXYSize;
	int			m_AMXAdjust;
	int			m_DisplayRenderer;
	int			m_DDFullScreenMode;
	bool		m_isFullScreen;

	HDC 		m_hDC;
	HWND		m_hWnd;
	HGDIOBJ 	m_hOldObj;
	HDC 		m_hDCBitmap;
	HGDIOBJ 	m_hBitmap;
	bmiData 	m_bmi;
	char		m_szTitle[100];

	int			m_ScreenRefreshCount;
	double		m_RelativeSpeed;
	double		m_FramesPerSecond;

	int			m_MenuIdPrinterPort;
	char		m_PrinterFileName[_MAX_PATH];
	char		m_PrinterDevice[_MAX_PATH];

	DWORD		m_LastTickCount;
	DWORD		m_LastStatsTickCount;
	int			m_LastTotalCycles;
	int			m_LastStatsTotalCycles;
	DWORD		m_TickBase;
	int			m_CycleBase;
	int			m_MinFrameCount;
	DWORD		m_LastFPSCount;
	int			m_LastStartY;
	int			m_LastNLines;
	int			m_MotionBlur;
	char 		m_BlurIntensities[8];
	char *		m_CommandLineFileName;

	// AVI vars
	bmiData 	m_Avibmi;
	HBITMAP		m_AviDIB;
	HDC 		m_AviDC;
	char*		m_AviScreen;
	int			m_AviFrameSkip;
	int			m_AviFrameSkipCount;
	int			m_MenuIdAviResolution;
	int			m_MenuIdAviSkip;

	// DirectX stuff
	BOOL					m_DXInit;

	// DirectDraw stuff
	LPDIRECTDRAW			m_DD;			// DirectDraw object
	LPDIRECTDRAW2			m_DD2;			// DirectDraw object
	LPDIRECTDRAWSURFACE		m_DDSPrimary;	// DirectDraw primary surface
	LPDIRECTDRAWSURFACE2	m_DDS2Primary;	// DirectDraw primary surface
	LPDIRECTDRAWSURFACE		m_DDSOne;		// Offscreen surface 1
	LPDIRECTDRAWSURFACE2	m_DDS2One;		// Offscreen surface 1
	BOOL					m_DXSmoothing;
	LPDIRECTDRAWCLIPPER		m_Clipper;		// clipper for primary

	// Direct3D9 stuff
	LPDIRECT3D9             m_pD3D;
	LPDIRECT3DDEVICE9       m_pd3dDevice;
	LPDIRECT3DVERTEXBUFFER9 m_pVB;
	LPDIRECT3DTEXTURE9      m_pTexture;
	D3DXMATRIX              m_TextureMatrix;

	// Text to speech variables
	ISpVoice *m_SpVoice;
	int m_SpeechLine;
	int m_SpeechCol;
	static const int MAX_SPEECH_LINE_LEN = 128;
	static const int MAX_SPEECH_SENTENCE_LEN = 128*25;
	static const int MAX_SPEECH_SCREEN_LEN = 128*32;
	char m_SpeechText[MAX_SPEECH_LINE_LEN+1];
	bool m_SpeechSpeakPunctuation;
	bool m_SpeechWriteChar;
	static const int MAX_SPEECH_BUF_LEN = 160;
	char m_SpeechBuf[MAX_SPEECH_BUF_LEN+1];
	int m_SpeechBufPos;

	// Text view variables
	HWND m_hTextView;
	static const int MAX_TEXTVIEW_SCREEN_LEN = 128*32;
	char m_TextViewScreen[MAX_TEXTVIEW_SCREEN_LEN+1];

	BOOL InitClass(void);
	void UpdateOptiMenu(void);
	void CreateBeebWindow(void);
	void CreateBitmap(void);
	void InitMenu(void);
	void UpdateMonitorMenu();
	void UpdateSerialMenu(HMENU hMenu);
	void UpdateEconetMenu(HMENU hMenu);
	void UpdateSFXMenu();
	void UpdateDisableKeysMenu();
	void UpdateDisplayRendererMenu(void);

	// DirectX - calls DDraw or DX9 fn
	void InitDX(void);
	void ResetDX(void);
  public:
	void ReinitDX(void);
  private:
	void ExitDX(void);

	// DirectDraw
	HRESULT InitDirectDraw(void);
	HRESULT InitSurfaces(void);
	void ResetSurfaces(void);

	// DirectX9
	HRESULT InitDX9(void);
	void ExitDX9(void);
	void RenderDX9(void);

	void GetRomMenu(void);				// LRW  Added for individual ROM/Ram
	void TranslateWindowSize(void);
	void TranslateSampleRate(void);
	void TranslateVolume(void);
	void TranslateTiming(void);
	void TranslateKeyMapping(void);
	int ReadDisc(int Drive,HMENU dmenu);
	void LoadTape(void);
	void InitJoystick(void);
	void ResetJoystick(void);
	void RestoreState(void);
	void SaveState(void);
	void NewDiscImage(int Drive);
	void EjectDiscImage(int Drive);
	void ToggleWriteProtect(int Drive);
	void SetWindowAttributes(bool wasFullScreen);
	void TranslateAMX(void);
	BOOL PrinterFile(void);
	void TogglePrinter(void);
	void TranslatePrinterPort(void);
	void CaptureVideo(void);
	void InitTextToSpeech(void);
	bool TextToSpeechSearch(TextToSpeechSearchDirection dir,
							TextToSpeechSearchType type);
	void TextToSpeechReadChar(void);
	void TextToSpeechReadWord(void);
	void TextToSpeechReadLine(void);
	void TextToSpeechReadSentence(void);
	void TextToSpeechReadScreen(void);
	void InitTextView(void);
	void TextView(void);
	void TextViewSetCursorPos(int line, int col);
	BOOL RebootSystem(void);

	// Preferences
	PrefsMap m_Prefs;
	char m_PrefsFile[_MAX_PATH];
	void LoadPreferences(void);
	void SavePreferences(void);
	bool PrefsGetBinaryValue(const char *id, void *bin, int binsize);
	void PrefsSetBinaryValue(const char *id, void *bin, int binsize);
	bool PrefsGetStringValue(const char *id, char *str);
	void PrefsSetStringValue(const char *id, const char *str);
	bool PrefsGetDWORDValue(const char *id, DWORD &dw);
	void PrefsSetDWORDValue(const char *id, DWORD dw);

}; /* BeebWin */

void SaveEmuUEF(FILE *SUEF);
void LoadEmuUEF(FILE *SUEF,int Version);
#endif
