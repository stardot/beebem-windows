// BeebBitmap.h
// A simple class to hold on to a bitmap & Paint it

typedef struct header
{
  BITMAPINFOHEADER  bmiHeader;
  RGBQUAD           aColors[256];
} header;

typedef struct pal
{
  WORD Version;
  WORD NumberOfEntries;
  PALETTEENTRY aEntries[256];
} pal;

class BeebBitmap {
	public:
		char * 	m_bitmapbits;
	
	private:
		HDC			m_hDCBitmap;	
		HANDLE		m_hBitmap;
		header		m_bmi;
		BITMAPINFO	*m_lpbmi;
	public:
		BeebBitmap();	
		
		~BeebBitmap() {	}		

		void RealizePalette(HDC);
		char * BitmapPointer(void)
			{ return m_bitmapbits; }
		
		void Blit(HDC hDestDC, int srcy, int size);
};
