// Port of beebwin to Win32

#ifndef BEEBWIN_HEADER
#define BEEBWIN_HEADER

#include <string.h>

#include "port.h"

typedef union {
	unsigned char data[8];
  EightByteType eightbyte;
} EightUChars;

typedef union {
	unsigned char data[16];
  EightByteType eightbytes[2];
} SixteenUChars;
 
class BeebWin  {

	int DataSize;
  
  public:
	char * m_screen;
	HDC    m_hDC;

	BeebBitmap	beebdisplay;

  private:
  	BOOL InitClass(void);
	void CreateBeebWindow(void);

	
	

  public:
	HWND m_hWnd;
	unsigned char cols[8]; /* Beeb colour lookup */
  	BeebWin();
	~BeebWin();
	char *imageData();
	int bytesPerLine();
	void updateLines(int starty, int nlines);

	void doHorizLine(unsigned long Col, int y, int sx, int width) {
		if (y>255) return;
	  	memset(m_screen+ (y* 640) + sx, Col , width);
  }; /* doHorizLine */

	void doHorizLine(unsigned long Col, int offset, int width) {
		if ((offset+width)<640*256) return;
		memset(m_screen+offset,Col,width);
	}; /* BeebWin::doHorizLine */

	EightUChars *GetLinePtr(int y);
	SixteenUChars *GetLinePtr16(int y);
}; /* BeebWin */

#endif
