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

#include <windows.h>

#define STRSAFE_NO_DEPRECATE
#include <sphelper.h>

#include <assert.h>
#include <stdio.h>

#include "BeebWin.h"
#include "BeebWinPrefs.h"
#include "DebugTrace.h"
#include "Main.h"
#include "Messages.h"
#include "Resource.h"
#include "StringUtils.h"

/****************************************************************************/
bool BeebWin::InitTextToSpeech()
{
	TextToSpeechResetState();

	HRESULT hResult = CoCreateInstance(
		CLSID_SpVoice,
		NULL,
		CLSCTX_ALL,
		IID_ISpVoice,
		(void **)&m_SpVoice
	);

	if (FAILED(hResult))
	{
		Report(MessageType::Error, "Failed to initialise text-to-speech engine");
		return false;
	}

	ISpObjectToken* pToken = TextToSpeechGetSelectedVoiceToken();

	TextToSpeechSetVoice(pToken);

	if (pToken != nullptr)
	{
		pToken->Release();
	}

	m_SpVoice->SetRate(m_SpeechRate);

	return true;
}

void BeebWin::TextToSpeechResetState()
{
	m_SpeechLine = 0;
	m_SpeechCol = 0;
	ZeroMemory(m_SpeechText, sizeof(m_SpeechText));
	ZeroMemory(m_SpeechBuf, sizeof(m_SpeechBuf));
	m_SpeechBufPos = 0;
}

void BeebWin::CloseTextToSpeech()
{
	if (m_SpVoice != nullptr)
	{
		m_SpVoice->Release();
		m_SpVoice = nullptr;
	}
}

bool BeebWin::InitTextToSpeechVoices()
{
	m_TextToSpeechVoices.clear();

	TextToSpeechVoice DefaultVoice;
	DefaultVoice.Id = "";
	DefaultVoice.Description = "&Default";

	m_TextToSpeechVoices.push_back(DefaultVoice);

	IEnumSpObjectTokens *pEnumerator = nullptr;
	ULONG VoiceCount = 0;
	ISpObjectToken* pVoiceToken = nullptr;

	HRESULT hResult = SpEnumTokens(SPCAT_VOICES, nullptr, nullptr, &pEnumerator);

	if (FAILED(hResult))
	{
		goto Exit;
	}

	hResult = pEnumerator->GetCount(&VoiceCount);

	if (FAILED(hResult))
	{
		goto Exit;
	}

	// Only allow 10 entries in the menu
	if (VoiceCount > 10)
	{
		VoiceCount = 10;
	}

	for (ULONG i = 0; i < VoiceCount; i++)
	{
		if (pVoiceToken != nullptr)
		{
			pVoiceToken->Release();
			pVoiceToken = nullptr;
		}

		hResult = pEnumerator->Next(1, &pVoiceToken, nullptr);

		if (FAILED(hResult))
		{
			goto Exit;
		}

		WCHAR* pszDescription = nullptr;
		SpGetDescription(pVoiceToken, &pszDescription);

		WCHAR* pszTokenId = nullptr;
		pVoiceToken->GetId(&pszTokenId);

		if (pszDescription != nullptr && pszTokenId != nullptr)
		{
			TextToSpeechVoice Voice;
			Voice.Id = WStr2Str(pszTokenId);
			Voice.Description = WStr2Str(pszDescription);

			m_TextToSpeechVoices.push_back(Voice);
		}

		if (pszDescription != nullptr)
		{
			CoTaskMemFree(pszDescription);
			pszDescription = nullptr;
		}

		if (pszTokenId != nullptr)
		{
			CoTaskMemFree(pszTokenId);
			pszTokenId = nullptr;
		}
	}

Exit:
	if (pEnumerator != nullptr)
	{
		pEnumerator->Release();
		pEnumerator = nullptr;
	}

	return SUCCEEDED(hResult);
}

static int GetMenuItemPosition(HMENU hMenu, const char *pszMenuItem)
{
	int Index = -1;
	int Count = GetMenuItemCount(hMenu);

	for (int i = 0; i < Count && Index == -1; ++i)
	{
		MENUITEMINFO Info;
		ZeroMemory(&Info, sizeof(Info));
		Info.cbSize = sizeof(Info);
		Info.fMask = MIIM_STRING;

		GetMenuItemInfo(hMenu, i, TRUE, &Info);

		Info.cch += 1;

		char *str = (char*)malloc(Info.cch);
		Info.dwTypeData = str;

		GetMenuItemInfo(hMenu, i, TRUE, &Info);

		if (strcmp(str, pszMenuItem) == 0)
		{
			Index = i; // Found it
		}

		free(str);
	}

	return Index;
}

void BeebWin::InitVoiceMenu()
{
	// The popup menu is destroyed automatically after inserting it
	// into the main menu.
	m_hVoiceMenu = CreatePopupMenu();

	HMENU hSoundMenu = GetSubMenu(m_hMenu, 5);
	assert(hSoundMenu != nullptr);

	int Pos = GetMenuItemPosition(hSoundMenu, "&Text To Speech");
	assert(Pos != -1);

	BOOL Result = InsertMenu(hSoundMenu,
	                         Pos + 1,
	                         MF_BYPOSITION | MF_POPUP | MF_STRING,
	                         (UINT_PTR)m_hVoiceMenu,
	                         "&Voice");

	UINT nMenuItemID = IDM_TEXT_TO_SPEECH_VOICE_BASE;

	for (size_t i = 0; i < m_TextToSpeechVoices.size(); i++)
	{
		Result = AppendMenu(m_hVoiceMenu,
		                    MF_STRING | MF_ENABLED,
		                    nMenuItemID++,
		                    m_TextToSpeechVoices[i].Description.c_str());
	}

	int Index = TextToSpeechGetSelectedVoice();

	TextToSpeechSelectVoiceMenuItem(Index);
}

int BeebWin::TextToSpeechGetSelectedVoice()
{
	char TokenId[_MAX_PATH];
	TokenId[0] = '\0';

	int Index = 0;

	m_Preferences.GetStringValue(CFG_TEXT_TO_SPEECH_VOICE, TokenId);

	for (std::size_t i = 0; i < m_TextToSpeechVoices.size(); i++)
	{
		if (m_TextToSpeechVoices[i].Id == TokenId)
		{
			Index = static_cast<int>(i);
			break;
		}
	}

	return Index;
}

// Returns the token for the selected voice, or nullptr to use the default voice.

ISpObjectToken* BeebWin::TextToSpeechGetSelectedVoiceToken()
{
	ISpObjectToken* pToken = nullptr;

	int Index = TextToSpeechGetSelectedVoice();

	if (Index > 0)
	{
		SpGetTokenFromId(Str2WStr(m_TextToSpeechVoices[Index].Id).c_str(), &pToken);
	}

	return pToken;
}

void BeebWin::TextToSpeechSetVoice(int Index)
{
	ISpObjectToken *pToken = nullptr;

	if (Index >= 0 && Index < (int)m_TextToSpeechVoices.size())
	{
		std::wstring TokenId = Str2WStr(m_TextToSpeechVoices[Index].Id);

		SpGetTokenFromId(TokenId.c_str(), &pToken);

		m_Preferences.SetStringValue(CFG_TEXT_TO_SPEECH_VOICE, m_TextToSpeechVoices[Index].Id.c_str());
	}
	else
	{
		m_Preferences.SetStringValue(CFG_TEXT_TO_SPEECH_VOICE, ""); // Default
	}

	TextToSpeechSelectVoiceMenuItem(Index);

	TextToSpeechSetVoice(pToken);

	if (pToken != nullptr)
	{
		pToken->Release();
	}
}

void BeebWin::TextToSpeechSetVoice(ISpObjectToken* pToken)
{
	if (m_SpVoice != nullptr)
	{
		m_SpVoice->SetVoice(pToken);

		m_SpVoice->Speak(L"<SILENCE MSEC='800'/>BeebEm text to speech output enabled",
		                 SPF_ASYNC, nullptr);
	}
}

void BeebWin::TextToSpeechSelectVoiceMenuItem(int Index)
{
	size_t LastMenuItemID = m_TextToSpeechVoices.size() > 0 ?
	                        IDM_TEXT_TO_SPEECH_VOICE_BASE + m_TextToSpeechVoices.size() - 1 :
	                        IDM_TEXT_TO_SPEECH_VOICE_BASE;

	::CheckMenuRadioItem(m_hVoiceMenu,
	                     IDM_TEXT_TO_SPEECH_VOICE_BASE,
	                     static_cast<UINT>(LastMenuItemID),
	                     IDM_TEXT_TO_SPEECH_VOICE_BASE + Index,
	                     MF_BYCOMMAND);
}

void BeebWin::Speak(const char *text, DWORD flags)
{
	if (m_SpVoice != nullptr)
	{
		m_SpVoice->Speak(Str2WStr(text).c_str(), SPF_ASYNC | SPF_IS_NOT_XML | flags, nullptr);
	}
}

static constexpr bool IsEndSentence(char c)
{
	return c == '.' || c == '!' || c == '?';
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
		     IsEndSentence(c) ||
		     c < 32 || c > 126))
		{
			m_SpeechBuf[m_SpeechBufPos] = '\0';
			Speak(m_SpeechBuf, 0);
			m_SpeechBufPos = 0;
		}
	}
}

void BeebWin::TextToSpeechClearBuffer()
{
	if (m_SpVoice != nullptr)
	{
		m_SpeechBufPos = 0;

		// Stop speaking.
		m_SpVoice->Speak(nullptr, SPF_PURGEBEFORESPEAK, nullptr);
	}
}

// Toggle text to speech for all text writes through WRCHV

void BeebWin::TextToSpeechToggleAutoSpeak()
{
	m_SpeechWriteChar = !m_SpeechWriteChar;

	if (m_SpeechWriteChar)
	{
		Speak("Writes to screen enabled.", SPF_PURGEBEFORESPEAK);
	}
	else
	{
		Speak("Writes to screen disabled.", SPF_PURGEBEFORESPEAK);
	}

	m_SpeechBufPos = 0;
}

// Toggle speaking of punctuation

void BeebWin::TextToSpeechToggleSpeakPunctuation()
{
	m_SpeechSpeakPunctuation = !m_SpeechSpeakPunctuation;

	if (m_SpeechSpeakPunctuation)
	{
		Speak("Speak punctuation enabled.", SPF_PURGEBEFORESPEAK);
	}
	else
	{
		Speak("Speak punctuation disabled.", SPF_PURGEBEFORESPEAK);
	}
}

void BeebWin::TextToSpeechIncreaseRate()
{
	if (m_SpeechRate < 10)
	{
		m_SpeechRate++;
		m_SpVoice->SetRate(m_SpeechRate);

		char Text[20];
		sprintf(Text, "Rate %d", m_SpeechRate);

		Speak(Text, SPF_PURGEBEFORESPEAK);
	}
}

void BeebWin::TextToSpeechDecreaseRate()
{
	if (m_SpeechRate > -10)
	{
		m_SpeechRate--;
		m_SpVoice->SetRate(m_SpeechRate);

		char Text[20];
		sprintf(Text, "Rate %d", m_SpeechRate);

		Speak(Text, SPF_PURGEBEFORESPEAK);
	}
}

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
				MessageBeep(0xFFFFFFFF);
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
				MessageBeep(0xFFFFFFFF);
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
				if (IsEndSentence(m_SpeechText[m_SpeechCol]))
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

void BeebWin::TextToSpeechReadChar()
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

void BeebWin::TextToSpeechReadWord()
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

void BeebWin::TextToSpeechReadLine()
{
	char text[MAX_SPEECH_LINE_LEN+1];
	bool blank = true;
	strcpy(text, m_SpeechText);
	for (int c = 0; text[c] != '\0'; ++c)
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

void BeebWin::TextToSpeechReadSentence()
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

			if (IsEndSentence(textLine[c]))
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

void BeebWin::TextToSpeechReadScreen()
{
	char text[MAX_SPEECH_SCREEN_LEN+1];
	bool blank = true;
	int i = 0;

	for (m_SpeechLine = 0; m_SpeechLine < CRTC_VerticalDisplayed; ++m_SpeechLine)
	{
		VideoGetText(m_SpeechText, m_SpeechLine);

		for (int c = 0; m_SpeechText[c] != '\0'; ++c)
		{
			text[i++] = m_SpeechText[c];

			if (m_SpeechText[c] != ' ')
				blank = false;
		}
	}

	text[i] = '\0';

	if (blank)
		strcpy(text, "blank");

	Speak(text, SPF_PURGEBEFORESPEAK |
	      (m_SpeechSpeakPunctuation ? SPF_NLP_SPEAK_PUNC : 0));
}

void BeebWin::TextToSpeechKey(WPARAM wParam)
{
	char text[MAX_SPEECH_LINE_LEN+1];

	if (!TeletextEnabled &&
	    wParam != VK_NUMPAD0 && wParam != VK_INSERT &&
	    wParam != VK_END && wParam != VK_NUMPAD1)
	{
		Speak("Not in text mode.", SPF_PURGEBEFORESPEAK);
	}
	else
	{
		bool AltPressed = GetKeyState(VK_MENU) < 0;
		bool InsPressed = (GetKeyState(VK_INSERT) < 0) || (GetKeyState(VK_NUMPAD0) < 0);
		bool RightCtrlPressed = GetKeyState(VK_RCONTROL) < 0;

		// Check that current position is still on screen
		if (m_SpeechCol >= CRTC_HorizontalDisplayed)
			m_SpeechCol = CRTC_HorizontalDisplayed-1;
		if (m_SpeechLine >= CRTC_VerticalDisplayed)
			m_SpeechLine = CRTC_VerticalDisplayed;

		VideoGetText(m_SpeechText, m_SpeechLine);

		// Key assignments are based on the keys used by the
		// Freedom Scientific JAWS package.
		switch (wParam)
		{
		case VK_CLEAR:
		case VK_HOME:
		case VK_NUMPAD5:
		case VK_NUMPAD7:
			if (InsPressed || RightCtrlPressed)
			{
				// Speak current word
				if (m_SpeechText[m_SpeechCol] != ' ')
					TextToSpeechSearch(TTS_BACKWARDS, TTS_BLANK);
				if (m_SpeechText[m_SpeechCol] == ' ')
					TextToSpeechSearch(TTS_FORWARDS, TTS_NONBLANK);

				TextToSpeechReadWord();
			}
			else if (AltPressed)
			{
				// Speak current sentence
				bool Found = TextToSpeechSearch(TTS_BACKWARDS, TTS_ENDSENTENCE);
				if (Found || m_SpeechText[m_SpeechCol] == ' ')
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
			if (InsPressed || RightCtrlPressed)
			{
				// Speak previous word
				if (m_SpeechText[m_SpeechCol] != ' ')
					TextToSpeechSearch(TTS_BACKWARDS, TTS_BLANK);
				TextToSpeechSearch(TTS_BACKWARDS, TTS_NONBLANK);

				// Now speak current word
				TextToSpeechKey(VK_CLEAR);
			}
			else if (AltPressed)
			{
				TextToSpeechClearBuffer();
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
			if (InsPressed || RightCtrlPressed)
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
			if (InsPressed || RightCtrlPressed)
			{
				// Read current line
				TextToSpeechReadLine();
			}
			else if (AltPressed)
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
					MessageBeep(0xFFFFFFFF);
				}
			}
			break;

		case VK_DOWN:
		case VK_NUMPAD2:
			if (InsPressed || RightCtrlPressed)
			{
				// Read entire screen
				TextToSpeechReadScreen();
			}
			else if (AltPressed)
			{
				// Read next sentence
				bool Found = TextToSpeechSearch(TTS_FORWARDS, TTS_ENDSENTENCE);
				if (Found)
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
					MessageBeep(0xFFFFFFFF);
				}
			}
			break;

		case VK_PRIOR:
		case VK_NUMPAD9:
			if (InsPressed || RightCtrlPressed)
			{
				// Output current cursor location
				sprintf(text, "Line %d column %d", m_SpeechLine, m_SpeechCol);
				Speak(text, SPF_PURGEBEFORESPEAK);
			}
			else if (AltPressed)
			{
				TextToSpeechDecreaseRate();
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
			if (InsPressed || RightCtrlPressed)
			{
				TextToSpeechToggleAutoSpeak();
			}
			else
			{
				// Flush buffered speech
				SpeakChar(13);
			}
			break;

		case VK_NEXT:
		case VK_NUMPAD3:
			if (InsPressed || RightCtrlPressed)
			{
				TextToSpeechToggleSpeakPunctuation();
			}
			else if (AltPressed)
			{
				TextToSpeechIncreaseRate();
			}
			else
			{
				// Go to bottom of screen
				m_SpeechLine = CRTC_VerticalDisplayed - 1;
				m_SpeechCol = 0;
			}
			break;
		}
	}
}

/****************************************************************************/

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

	LRESULT Result = CallWindowProc(mainWin->m_TextViewPrevWndProc, hWnd, msg, wParam, lParam);

	if (keypress)
		mainWin->TextViewSpeechSync();

	return Result;
}

void BeebWin::InitTextView()
{
	CloseTextView();

	m_TextViewScreen[0] = '\0';

	if (m_TextViewEnabled)
	{
		RECT rc;
		GetClientRect(m_hWnd, &rc);

		m_hTextView = CreateWindow("EDIT", "TextView",
		                           WS_CHILD | WS_VISIBLE | ES_READONLY | ES_MULTILINE | ES_LEFT,
		                           0, 0, rc.right, rc.bottom,
		                           m_hWnd,
		                           NULL,
		                           hInst,
		                           NULL);

		if (m_hTextView == NULL)
		{
			m_TextViewEnabled = false;
		}
		else
		{
			SendMessage(m_hTextView, WM_SETFONT,
			            (WPARAM)GetStockObject(ANSI_FIXED_FONT), 0);

			m_TextViewPrevWndProc = (WNDPROC)GetWindowLongPtr(m_hTextView, GWLP_WNDPROC);
			SetWindowLongPtr(m_hTextView, GWLP_WNDPROC, (LONG_PTR)TextViewWndProc);

			SetFocus(m_hTextView);
		}
	}

	CheckMenuItem(IDM_TEXTVIEW, m_TextViewEnabled);
}

void BeebWin::CloseTextView()
{
	if (m_hTextView != nullptr)
	{
		DestroyWindow(m_hTextView);
		m_hTextView = nullptr;
		m_TextViewPrevWndProc = nullptr;
	}
}

void BeebWin::TextView()
{
	char text[MAX_SPEECH_LINE_LEN+1];
	char screen[MAX_TEXTVIEW_SCREEN_LEN+1];
	char *s = screen;

	int selpos = (int)SendMessage(m_hTextView, EM_GETSEL, 0, 0) & 0xffff;

	if (TeletextEnabled)
	{
		for (int line = 0; line < CRTC_VerticalDisplayed; ++line)
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

void BeebWin::TextViewSpeechSync()
{
	if (m_TextToSpeechEnabled)
		TextViewSetCursorPos(m_SpeechLine, m_SpeechCol);
}

void BeebWin::TextViewSetCursorPos(int line, int col)
{
	int Pos = (CRTC_HorizontalDisplayed + 2) * line + col;
	SendMessage(m_hTextView, EM_SETSEL, Pos, Pos);
}

void BeebWin::TextViewSyncWithBeebCursor()
{
	// Move speech cursor to Beeb's cursor pos
	int CurAddr = CRTC_CursorPosLow + (((CRTC_CursorPosHigh ^ 0x20) + 0x74 & 0xff) << 8);
	int ScrAddr = CRTC_ScreenStartLow + (((CRTC_ScreenStartHigh ^ 0x20) + 0x74 & 0xff) << 8);
	int RelAddr = CurAddr - ScrAddr;

	m_SpeechLine = RelAddr / CRTC_HorizontalDisplayed;
	m_SpeechCol = RelAddr % CRTC_HorizontalDisplayed;

	if (m_TextViewEnabled)
		TextViewSetCursorPos(m_SpeechLine, m_SpeechCol);
}
