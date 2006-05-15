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
#include <windows.h>
#include <ddraw.h>
#include "port.h"
#include "video.h"

/* Used in message boxes */
#define GETHWND (mainWin == NULL ? NULL : mainWin->GethWnd())

typedef union {
	unsigned char data[8];
  EightByteType eightbyte;
} EightUChars;

typedef union {
	unsigned char data[16];
  EightByteType eightbytes[2];
} SixteenUChars;
 
typedef struct
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
	bool ShowDisc;
	bool ShowKB;
};
extern struct LEDType LEDs;

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
	void ScaleJoystick(unsigned int x, unsigned int y);
	void SetMousestickButton(int button);
	void ScaleMousestick(unsigned int x, unsigned int y);
	void HandleCommand(int MenuId);
	void SetAMXPosition(unsigned int x, unsigned int y);
	void Focus(BOOL);
	BOOL IsFrozen(void);
	void ShowMenu(bool on);
	void TrackPopupMenu(int x, int y);
	bool IsFullScreen() { return m_isFullScreen; }
	void SaveOnExit(void);
	void ResetTiming(void);
	int TranslateKey(int, int, int&, int&);
	void ParseCommandLine(void);
	void HandleCommandLineFile(void);
	void NewTapeImage(char *FileName);
	const char *GetAppPath(void) { return m_AppPath; }
	void QuickLoad(void);
	void QuickSave(void);

	unsigned char cols[256];
    HMENU		m_hMenu;
	BOOL		m_frozen;
	char*		m_screen;
	char*		m_screen_blur;
	double		m_RealTimeTarget;
	int			m_ShiftBooted;

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
	BOOL		m_WriteProtectDisc[2];
	int			m_MenuIdAMXSize;
	int			m_MenuIdAMXAdjust;
	int			m_AMXXSize;
	int			m_AMXYSize;
	int			m_AMXAdjust;
	BOOL		m_DirectDrawEnabled;
	int     m_DDFullScreenMode;
	bool    m_isFullScreen;
	bool    m_isDD32;

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
	LPDIRECTDRAW			m_DD;			// DirectDraw object
	LPDIRECTDRAW2			m_DD2;			// DirectDraw object
	LPDIRECTDRAWSURFACE		m_DDSPrimary;	// DirectDraw primary surface
	LPDIRECTDRAWSURFACE2	m_DDS2Primary;	// DirectDraw primary surface
	LPDIRECTDRAWSURFACE		m_DDSOne;		// Offscreen surface 1
	LPDIRECTDRAWSURFACE2	m_DDS2One;		// Offscreen surface 1
	LPDIRECTDRAWSURFACE     m_BackBuffer;   // Full Screen Back Buffer
	LPDIRECTDRAWSURFACE2	m_BackBuffer2;  // DD2 of the above
	BOOL					m_DDS2InVideoRAM;
	LPDIRECTDRAWCLIPPER		m_Clipper;		// clipper for primary

	BOOL InitClass(void);
	void UpdateOptiMenu(void);
	void CreateBeebWindow(void);
	void CreateBitmap(void);
	void InitMenu(void);
	void UpdateMonitorMenu();
	void UpdateSerialMenu(HMENU hMenu);
	void UpdateEconetMenu(HMENU hMenu);
	void UpdateSFXMenu();
	void InitDirectX(void);
	HRESULT InitSurfaces(void);
	void ResetSurfaces(void);
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
	void ToggleWriteProtect(int Drive);
	void LoadPreferences(void);
	void SavePreferences(void);
	void SetWindowAttributes(bool wasFullScreen);
	void TranslateAMX(void);
	BOOL PrinterFile(void);
	void TogglePrinter(void);
	void TranslatePrinterPort(void);
	void SaveWindowPos(void);
	void CaptureVideo(void);

}; /* BeebWin */

void SaveEmuUEF(FILE *SUEF);
void LoadEmuUEF(FILE *SUEF,int Version);
#endif
