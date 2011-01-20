/****************************************************************
BeebEm - BBC Micro and Master 128 Emulator
Copyright (C) 2006  Mike Wyatt

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

// BeebWin text to speech support

#include <stdio.h>
#include <windows.h>
#include <initguid.h>
#include "main.h"
#include "beebwin.h"
#include "beebemrc.h"

/****************************************************************************/
void BeebWin::InitTextToSpeech(void)
{
	m_SpeechLine = 0;
	m_SpeechCol = 0;
	memset(m_SpeechText, 0, MAX_SPEECH_LINE_LEN+1);
	m_SpeechSpeakPunctuation = false;
	m_SpeechWriteChar = false;
	m_SpeechBufPos = 0;
	memset(m_SpeechBuf, 0, MAX_SPEECH_BUF_LEN+1);
	m_SpeechBufPos = 0;

	if (m_TextToSpeechEnabled && m_SpVoice == NULL)
	{
		HRESULT hr = CoCreateInstance(
			CLSID_SpVoice, NULL, CLSCTX_ALL, IID_ISpVoice, (void **)&m_SpVoice);
		if( FAILED( hr ) )
		{
			m_SpVoice = NULL;
			m_TextToSpeechEnabled = 0;
			MessageBox(m_hWnd,"Failed to initialise text-to-speech engine\n",
					   WindowTitle,MB_OK|MB_ICONERROR);
		}
	}
	CheckMenuItem(m_hMenu, IDM_TEXTTOSPEECH, m_TextToSpeechEnabled ? MF_CHECKED:MF_UNCHECKED);

	if (m_TextToSpeechEnabled)
	{
		m_SpVoice->Speak(L"<SILENCE MSEC='800'/>BeebEm text to speech output enabled",
						 SPF_ASYNC, NULL);
	}
}

void BeebWin::Speak(const char *text, DWORD flags)
{
	if (m_TextToSpeechEnabled)
	{
		int len = MultiByteToWideChar(CP_ACP, 0, text, -1, 0, 0) + 1;
		WCHAR *wtext = (WCHAR*)malloc(len * sizeof(WCHAR));
		MultiByteToWideChar(CP_ACP, 0, text, -1, wtext, len);
		m_SpVoice->Speak(wtext, SPF_ASYNC | SPF_IS_NOT_XML | flags, NULL);
		free(wtext);
	}
}

void BeebWin::SpeakChar(unsigned char c)
{
	if (m_SpeechWriteChar)
	{
		if (isprint(c))
		{
			m_SpeechBuf[m_SpeechBufPos++] = c;
		}

		if (m_SpeechBufPos > 0 &&
			(m_SpeechBufPos == MAX_SPEECH_BUF_LEN ||
			 (c == '.' || c == '!' || c == '?' || c == '!' || c < 32 || c > 126) ))
		{
			m_SpeechBuf[m_SpeechBufPos] = '\0';
			Speak(m_SpeechBuf, 0);
			m_SpeechBufPos = 0;
		}
	}
}

#define IS_ENDSENTENCE(c) (c == '.' || c == '!' || c == '?')

bool BeebWin::TextToSpeechSearch(TextToSpeechSearchDirection dir,
								 TextToSpeechSearchType type)
{
	bool done = false;
	bool found = false;
	int lastNonBlankLine = m_SpeechLine;
	int lastNonBlankCol = m_SpeechCol;

	VideoGetText(m_SpeechText, m_SpeechLine);

	while (!done)
	{
		if (dir == TTS_FORWARDS)
		{
			if (m_SpeechCol < CRTC_HorizontalDisplayed-1)
			{
				++m_SpeechCol;
			}
			else if (m_SpeechLine < CRTC_VerticalDisplayed-1)
			{
				++m_SpeechLine;
				VideoGetText(m_SpeechText, m_SpeechLine);
				m_SpeechCol = 0;
			}
			else
			{
				done = true;
				MessageBeep(-1);
			}
		}
		else // Backwards
		{
			if (m_SpeechCol > 0)
			{
				--m_SpeechCol;
			}
			else if (m_SpeechLine > 0)
			{
				--m_SpeechLine;
				VideoGetText(m_SpeechText, m_SpeechLine);
				m_SpeechCol = (int)(strlen(m_SpeechText) - 1);
				if (m_SpeechCol < 0)
					m_SpeechCol = 0;
			}
			else
			{
				done = true;
				MessageBeep(-1);
			}
		}

		if (m_SpeechText[m_SpeechCol] != ' ')
		{
			lastNonBlankLine = m_SpeechLine;
			lastNonBlankCol = m_SpeechCol;
		}

		if (!done)
		{
			switch (type)
			{
			case TTS_CHAR:
				done = true;
				found = true;
				break;

			case TTS_BLANK:
				if (m_SpeechText[m_SpeechCol] == ' ')
				{
					done = true;
					found = true;
				}
				break;
			case TTS_NONBLANK:
				if (m_SpeechText[m_SpeechCol] != ' ')
				{
					done = true;
					found = true;
				}
				break;
			case TTS_ENDSENTENCE:
				if (IS_ENDSENTENCE(m_SpeechText[m_SpeechCol]))
				{
					done = true;
					found = true;
				}
				break;
			}
		}
	}

	if (!found)
	{
		m_SpeechLine = lastNonBlankLine;
		m_SpeechCol = lastNonBlankCol;
	}

	return found;
}

void BeebWin::TextToSpeechReadChar(void)
{
	char text[MAX_SPEECH_LINE_LEN+1];
	if (m_SpeechText[m_SpeechCol] == ' ')
	{
		strcpy(text, "space");
	}
	else
	{
		text[0] = m_SpeechText[m_SpeechCol];
		text[1] = '\0';
	}
	Speak(text, SPF_PURGEBEFORESPEAK | SPF_NLP_SPEAK_PUNC);
}

void BeebWin::TextToSpeechReadWord(void)
{
	char text[MAX_SPEECH_LINE_LEN+1];
	int i = 0;
	for (int c = m_SpeechCol; m_SpeechText[c] != 0 && m_SpeechText[c] != ' '; ++c)
	{
		text[i++] = m_SpeechText[c];
	}
	text[i] = 0;
	if (i == 0)
		strcpy(text, "blank");
	Speak(text, SPF_PURGEBEFORESPEAK |
		  (m_SpeechSpeakPunctuation ? SPF_NLP_SPEAK_PUNC : 0));
}

void BeebWin::TextToSpeechReadLine(void)
{
	char text[MAX_SPEECH_LINE_LEN+1];
	bool blank = true;
	strcpy(text, m_SpeechText);
	for (int c = 0; text[c] != 0; ++c)
	{
		if (text[c] != ' ')
		{
			blank = false;
			break;
		}
	}
	if (blank)
		strcpy(text, "blank");

	Speak(text, SPF_PURGEBEFORESPEAK |
		  (m_SpeechSpeakPunctuation ? SPF_NLP_SPEAK_PUNC : 0));
}

void BeebWin::TextToSpeechReadSentence(void)
{
	char text[MAX_SPEECH_SENTENCE_LEN+1];
	char textLine[MAX_SPEECH_LINE_LEN+1];
	int line = m_SpeechLine;
	int col = m_SpeechCol;
	bool blank = true;
	bool done = false;
	int i = 0;

	while (!done)
	{
		VideoGetText(textLine, line);
		for (int c = col; textLine[c] != 0; ++c)
		{
			text[i++] = textLine[c];
			if (textLine[c] != ' ')
				blank = false;

			if (IS_ENDSENTENCE(textLine[c]))
			{
				done = true;
				break;
			}
		}
		if (!done)
		{
			if (line < CRTC_VerticalDisplayed-1)
			{
				line++;
				col = 0;
			}
			else
			{
				done = true;
			}
		}
	}
	text[i] = 0;

	if (blank)
		strcpy(text, "blank");

	Speak(text, SPF_PURGEBEFORESPEAK |
		  (m_SpeechSpeakPunctuation ? SPF_NLP_SPEAK_PUNC : 0));
}

void BeebWin::TextToSpeechReadScreen(void)
{
	char text[MAX_SPEECH_SCREEN_LEN+1];
	bool blank = true;
	int i = 0;

	for (m_SpeechLine = 0; m_SpeechLine < CRTC_VerticalDisplayed-1; ++m_SpeechLine)
	{
		VideoGetText(m_SpeechText, m_SpeechLine);
		for (int c = 0; m_SpeechText[c] != 0; ++c)
		{
			text[i++] = m_SpeechText[c];
			if (m_SpeechText[c] != ' ')
				blank = false;
		}
	}
	text[i] = 0;

	if (blank)
		strcpy(text, "blank");

	Speak(text, SPF_PURGEBEFORESPEAK |
		  (m_SpeechSpeakPunctuation ? SPF_NLP_SPEAK_PUNC : 0));
}

void BeebWin::TextToSpeechKey(WPARAM uParam)
{
	char text[MAX_SPEECH_LINE_LEN+1];
	bool found;

	if (!TeletextEnabled &&
		uParam != VK_NUMPAD0 && uParam != VK_INSERT &&
		uParam != VK_END && uParam != VK_NUMPAD1)
	{
		Speak("Not in text mode.", SPF_PURGEBEFORESPEAK);
	}
	else
	{
		int s = GetKeyState(VK_MENU);
		bool altPressed = (GetKeyState(VK_MENU) < 0);
		bool insPressed = (GetKeyState(VK_INSERT) < 0) || (GetKeyState(VK_NUMPAD0) < 0);

		// Check that current position is still on screen
		if (m_SpeechCol >= CRTC_HorizontalDisplayed)
			m_SpeechCol = CRTC_HorizontalDisplayed-1;
		if (m_SpeechLine >= CRTC_VerticalDisplayed)
			m_SpeechLine = CRTC_VerticalDisplayed;

		VideoGetText(m_SpeechText, m_SpeechLine);

		// Key assignments are based on the keys used by the 
		// Freedom Scientific JAWS package.
		switch (uParam)
		{
		case VK_CLEAR:
		case VK_HOME:
		case VK_NUMPAD5:
		case VK_NUMPAD7:
			if (insPressed)
			{
				// Speak current word
				if (m_SpeechText[m_SpeechCol] != ' ')
					TextToSpeechSearch(TTS_BACKWARDS, TTS_BLANK);
				if (m_SpeechText[m_SpeechCol] == ' ')
					TextToSpeechSearch(TTS_FORWARDS, TTS_NONBLANK);

				TextToSpeechReadWord();
			}
			else if (altPressed)
			{
				// Speak current sentence
				found = TextToSpeechSearch(TTS_BACKWARDS, TTS_ENDSENTENCE);
				if (found || m_SpeechText[m_SpeechCol] == ' ')
					TextToSpeechSearch(TTS_FORWARDS, TTS_NONBLANK);

				TextToSpeechReadSentence();
			}
			else
			{
				// Speak current character
				TextToSpeechReadChar();
			}
			break;

		case VK_LEFT:
		case VK_NUMPAD4:
			if (insPressed)
			{
				// Speak previous word
				if (m_SpeechText[m_SpeechCol] != ' ')
					TextToSpeechSearch(TTS_BACKWARDS, TTS_BLANK);
				TextToSpeechSearch(TTS_BACKWARDS, TTS_NONBLANK);

				// Now speak current word
				TextToSpeechKey(VK_CLEAR);
			}
			else
			{
				// Speak previous character
				TextToSpeechSearch(TTS_BACKWARDS, TTS_CHAR);
				TextToSpeechReadChar();
			}
			break;

		case VK_RIGHT:
		case VK_NUMPAD6:
			if (insPressed)
			{
				// Speak next word
				if (m_SpeechText[m_SpeechCol] != ' ')
					TextToSpeechSearch(TTS_FORWARDS, TTS_BLANK);

				// Now speak current word
				TextToSpeechKey(VK_CLEAR);
			}
			else
			{
				// Speak next character
				TextToSpeechSearch(TTS_FORWARDS, TTS_CHAR);
				TextToSpeechReadChar();
			}
			break;

		case VK_UP:
		case VK_NUMPAD8:
			if (insPressed)
			{
				// Read current line
				TextToSpeechReadLine();
			}
			else if (altPressed)
			{
				// Read previous sentence
				TextToSpeechSearch(TTS_BACKWARDS, TTS_ENDSENTENCE);

				// Now speak current sentence
				TextToSpeechKey(VK_CLEAR);
			}
			else
			{
				// Read previous line
				if (m_SpeechLine > 0)
				{
					--m_SpeechLine;
					VideoGetText(m_SpeechText, m_SpeechLine);
					TextToSpeechReadLine();
				}
				else
				{
					MessageBeep(-1);
				}
			}
			break;

		case VK_DOWN:
		case VK_NUMPAD2:
			if (insPressed)
			{
				// Read entire screen
				TextToSpeechReadScreen();
			}
			else if (altPressed)
			{
				// Read next sentence
				found = TextToSpeechSearch(TTS_FORWARDS, TTS_ENDSENTENCE);
				if (found)
					TextToSpeechSearch(TTS_FORWARDS, TTS_NONBLANK);

				TextToSpeechReadSentence();
			}
			else
			{
				// Read next line
				if (m_SpeechLine < CRTC_VerticalDisplayed-1)
				{
					++m_SpeechLine;
					VideoGetText(m_SpeechText, m_SpeechLine);
					TextToSpeechReadLine();
				}
				else
				{
					MessageBeep(-1);
				}
			}
			break;

		case VK_PRIOR:
		case VK_NUMPAD9:
			if (insPressed)
			{
				// Output current cursor location
				sprintf(text, "Line %d column %d", m_SpeechLine, m_SpeechCol);
				Speak(text, SPF_PURGEBEFORESPEAK);
			}
			else
			{
				// Go to top of screen
				m_SpeechLine = 0;
				m_SpeechCol = 0;
			}
			break;

		case VK_END:
		case VK_NUMPAD1:
			if (insPressed)
			{
				// Toggle text to speech for all text writes through WRCHV
				m_SpeechWriteChar = !m_SpeechWriteChar;
				if (m_SpeechWriteChar)
					Speak("Writes to screen enabled.", SPF_PURGEBEFORESPEAK);
				else
					Speak("Writes to screen disabled.", SPF_PURGEBEFORESPEAK);
				m_SpeechBufPos = 0;
			}
			else
			{
				// Flush buffered speech
				SpeakChar(13);
			}
			break;

		case VK_NEXT:
		case VK_NUMPAD3:
			if (insPressed)
			{
				// Toggle speaking of punctuation
				m_SpeechSpeakPunctuation = !m_SpeechSpeakPunctuation;
				if (m_SpeechSpeakPunctuation)
					Speak("Speak puntuation enabled.", SPF_PURGEBEFORESPEAK);
				else
					Speak("Speak puntuation disabled.", SPF_PURGEBEFORESPEAK);
			}
			else
			{
				// Go to bottom of screen
				m_SpeechLine = CRTC_VerticalDisplayed-1;
				m_SpeechCol = 0;
			}
			break;
		}
	}
}

/****************************************************************************/
static WNDPROC TextViewPrevWndProc;
LRESULT TextViewWndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	bool keypress = false;

	switch (msg)
	{
	case WM_KEYDOWN:
	case WM_KEYUP:
	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
		SendMessage(mainWin->GethWnd(), msg, wParam, lParam);
		keypress = true;
		break;
	}
	LRESULT lr = CallWindowProc(TextViewPrevWndProc, hWnd, msg, wParam, lParam);

	if (keypress)
		mainWin->TextViewSpeechSync();

	return lr;
}

void BeebWin::InitTextView(void)
{
	if (m_hTextView != NULL)
	{
		DestroyWindow(m_hTextView);
		m_hTextView = NULL;
	}
	m_TextViewScreen[0] = 0;

	if (m_TextViewEnabled)
	{
		RECT rc;
		GetClientRect(m_hWnd, &rc);
		m_hTextView = CreateWindow("EDIT", "TextView",
			WS_CHILD | WS_VISIBLE | ES_READONLY | ES_MULTILINE | ES_LEFT,
			0, 0, rc.right, rc.bottom, m_hWnd,
			NULL, hInst, NULL);
		if (m_hTextView == NULL)
		{
			m_TextViewEnabled = 0;
		}
		else
		{
			SendMessage(m_hTextView, WM_SETFONT,
						(WPARAM)GetStockObject(ANSI_FIXED_FONT), 0);

			LONG_PTR lPtr = GetWindowLongPtr(m_hTextView, GWLP_WNDPROC);
			TextViewPrevWndProc = (WNDPROC)lPtr;
			lPtr = reinterpret_cast<LONG_PTR>(TextViewWndProc);
			#pragma warning (disable:4244)
			SetWindowLongPtr(m_hTextView, GWLP_WNDPROC, lPtr);
			SetFocus(m_hTextView);
		}
	}

	CheckMenuItem(m_hMenu, IDM_TEXTVIEW, m_TextViewEnabled ? MF_CHECKED:MF_UNCHECKED);
}

void BeebWin::TextView(void)
{
	char text[MAX_SPEECH_LINE_LEN+1];
	char screen[MAX_TEXTVIEW_SCREEN_LEN+1];
	char *s = screen;
	int line;

	int selpos = (int)SendMessage(m_hTextView, EM_GETSEL, 0, 0) & 0xffff;

	if (TeletextEnabled)
	{
		for (line = 0; line < CRTC_VerticalDisplayed; ++line)
		{
			VideoGetText(text, line);
			strcpy(s, text);
			strcat(s, "\r\n");
			s += strlen(s);
		}
	}
	else
	{
		strcpy(screen, "Not in text mode.");
	}

	if (strcmp(m_TextViewScreen, screen) != 0)
	{
		strcpy(m_TextViewScreen, screen);
		SendMessage(m_hTextView, WM_SETTEXT, 0, (LPARAM)m_TextViewScreen);
		SendMessage(m_hTextView, EM_SETSEL, selpos, selpos);
		TextViewSpeechSync();
	}
}

void BeebWin::TextViewSpeechSync(void)
{
	if (m_TextToSpeechEnabled)
		TextViewSetCursorPos(m_SpeechLine, m_SpeechCol);
}

void BeebWin::TextViewSetCursorPos(int line, int col)
{
	int selpos = (CRTC_HorizontalDisplayed+2) * line + col;
	SendMessage(m_hTextView, EM_SETSEL, selpos, selpos);
}

void BeebWin::TextViewSyncWithBeebCursor(void)
{
	// Move speech cursor to Beeb's cursor pos
	int CurAddr = CRTC_CursorPosLow+(((CRTC_CursorPosHigh ^ 0x20) + 0x74 & 0xff)<<8);
	int ScrAddr = CRTC_ScreenStartLow+(((CRTC_ScreenStartHigh ^ 0x20) + 0x74 & 0xff)<<8);
	int RelAddr = CurAddr - ScrAddr;
	m_SpeechLine = RelAddr / CRTC_HorizontalDisplayed;
	m_SpeechCol = RelAddr % CRTC_HorizontalDisplayed;
	if (m_TextViewEnabled)
		TextViewSetCursorPos(m_SpeechLine, m_SpeechCol);
}
