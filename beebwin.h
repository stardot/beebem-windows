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

class BeebWin  {
  
  public:

  enum PaletteType { RGB, BW, AMBER, GREEN } palette_type;

	void Initialise();

	unsigned char cols[8];
  
	BeebWin();
	~BeebWin();

	void updateLines(HDC hDC, int starty, int nlines);
	void updateLines(int starty, int nlines)
		{ updateLines(m_hDC, starty, nlines); };

	void doHorizLine(unsigned long Col, int y, int sx, int width) {
		unsigned int tsx;
		float tsx2;
		tsx2=(float)sx*1;
		tsx=(int)tsx2;
		if (y>255) return;
		memset(m_screen+ (y* 800) + tsx, Col , width);
	};

//	void doHorizLine(unsigned long Col, int offset, int width) {
//		unsigned int tsx;
//		if ((offset+width)<640*256) return;
//		tsx=((offset/640)*640)+((offset % 640)/2);
//		memset(m_screen+tsx,Col,width);
//	};

	void SetRomMenu(void);				// LRW  Added for individual ROM/Ram

	EightUChars *GetLinePtr(int y) {
		if(y > 255) y=255;
		return((EightUChars *)(m_screen + ( y * 800 )+ScreenAdjust));
	}

	SixteenUChars *GetLinePtr16(int y) {
		if(y > 255) y=255;
		return((SixteenUChars *)(m_screen + ( y * 800 )+ScreenAdjust));
	}

	char *imageData(void) {
		return m_screen+ScreenAdjust;
	}

	HWND GethWnd() { return m_hWnd; };
	
	void RealizePalette(HDC) {};
	void ResetBeebSystem(unsigned char NewModelType,unsigned char TubeStatus,unsigned char LoadRoms);

	int StartOfFrame(void);
	BOOL UpdateTiming(void);
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
	double		m_RealTimeTarget;
	JOYCAPS		m_JoystickCaps;
	int			m_MenuIdSticks;
	BOOL		m_HideCursor;
	BOOL		m_FreezeWhenInactive;
	int			m_MenuIdKeyMapping;
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

	char*		m_screen;
	HDC 		m_hDC;
	HWND		m_hWnd;
  HMENU   m_hMenu;
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

	BOOL		m_frozen;

	// DirectX stuff
	BOOL					m_DXInit;
	LPDIRECTDRAW			m_DD;			// DirectDraw object
	LPDIRECTDRAW2			m_DD2;			// DirectDraw object
	LPDIRECTDRAWSURFACE		m_DDSPrimary;	// DirectDraw primary surface
	LPDIRECTDRAWSURFACE2	m_DDS2Primary;	// DirectDraw primary surface
	LPDIRECTDRAWSURFACE		m_DDSOne;		// Offscreen surface 1
	LPDIRECTDRAWSURFACE2	m_DDS2One;		// Offscreen surface 1
	BOOL					m_DDS2InVideoRAM;
	LPDIRECTDRAWCLIPPER		m_Clipper;		// clipper for primary

	BOOL InitClass(void);
	void CreateBeebWindow(void);
	void CreateBitmap(void);
	void InitMenu(void);
  void UpdateMonitorMenu();
  void UpdateModelType();
  void UpdateSFXMenu();
	void InitDirectX(void);
	HRESULT InitSurfaces(void);
	void ResetSurfaces(void);
	void GetRomMenu(void);				// LRW  Added for individual ROM/Ram
	void GreyRomMenu(BOOL SetToGrey);	// LRW	Added for individual ROM/Ram
	void TranslateWindowSize(void);
	void TranslateSampleRate(void);
	void TranslateVolume(void);
	void TranslateTiming(void);
	void TranslateKeyMapping(void);
	void ReadDisc(int Drive,HMENU dmenu);
	void LoadTape(void);
	void InitJoystick(void);
	void ResetJoystick(void);
	void RestoreState(void);
	void SaveState(void);
	void NewDiscImage(int Drive);
	void ToggleWriteProtect(int Drive);
	void SavePreferences(void);
	void SetWindowAttributes(bool wasFullScreen);
	void TranslateAMX(void);
	BOOL PrinterFile(void);
	void TogglePrinter(void);
	void TranslatePrinterPort(void);

}; /* BeebWin */

#endif
