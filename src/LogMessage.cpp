/**
 *	LogMessage.cpp
 *	Copyright (C) 2004 Nate
 *
 *	This file is part of DigitalWatch, a free DTV watching and recording
 *	program for the VisionPlus DVB-T.
 *
 *	DigitalWatch is free software; you can redistribute it and/or modify
 *	it under the terms of the GNU General Public License as published by
 *	the Free Software Foundation; either version 2 of the License, or
 *	(at your option) any later version.
 *
 *	DigitalWatch is distributed in the hope that it will be useful,
 *	but WITHOUT ANY WARRANTY; without even the implied warranty of
 *	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *	GNU General Public License for more details.
 *
 *	You should have received a copy of the GNU General Public License
 *	along with DigitalWatch; if not, write to the Free Software
 *	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "LogMessage.h"
#include "GlobalFunctions.h"

#include <fstream>
using namespace std;

LogMessageCallback::LogMessageCallback()
{
	static int handle = 1;
	m_handle = handle++;
}

LogMessageCallback::~LogMessageCallback()
{
}

int LogMessageCallback::GetHandle()
{
	return m_handle;
}


//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

LogMessage::LogMessage()
{
	m_str[0] = 0;
}

LogMessage::~LogMessage()
{
	if (m_str[0] != 0)
		WriteLogMessage();

	vector<LogMessageCallback *>::iterator it;
	for ( it = callbacks.begin() ; it != callbacks.end() ; it++ )
	{
		delete *it;
		callbacks.erase(it);
	}
}

int LogMessage::AddCallback(LogMessageCallback *callback)
{
	callbacks.push_back(callback);
	return callback->GetHandle();
}

void LogMessage::RemoveCallback(int handle)
{
	vector<LogMessageCallback *>::iterator it = callbacks.begin();
	while ( it != callbacks.end() )
	{
		if ((*it)->GetHandle() == handle)
		{
			callbacks.erase(it);
			continue;
		}
		it++;
	}
}

void LogMessage::WriteLogMessage()
{
	USES_CONVERSION;

	if (m_str[0] != 0)
	{
		vector<LogMessageCallback *>::iterator it;
		for ( it = callbacks.begin() ; it != callbacks.end() ; it++ )
		{
			(*it)->Write((LPSTR)&m_str);
		}
		m_str[0] = 0;
	}
}

int LogMessage::Write()
{
	return Write(FALSE);
}

int LogMessage::Write(int returnValue)
{
	if (m_str[0] != 0)
		WriteLogMessage();

	return returnValue;
}

int LogMessage::Show()
{
	return Show(FALSE);
}

int LogMessage::Show(int returnValue)
{
	USES_CONVERSION;

	if (m_str[0] != 0)
	{
		vector<LogMessageCallback *>::iterator it;
		for ( it = callbacks.begin() ; it != callbacks.end() ; it++ )
		{
			(*it)->Show((LPSTR)&m_str);
			(*it)->Write((LPSTR)&m_str);
		}
		//WriteLogMessage();
		m_str[0] = 0;
	}

	return returnValue;
}

void LogMessage::ClearFile()
{
	vector<LogMessageCallback *>::iterator it;
	for ( it = callbacks.begin() ; it != callbacks.end() ; it++ )
	{
		(*it)->Clear();
	}
}

void LogMessage::LogVersionNumber()
{
	USES_CONVERSION;
	wchar_t filename[MAX_PATH];
	GetCommandExe((LPWSTR)&filename);

	DWORD zeroHandle, size = 0;
	while (TRUE)
	{
		size = GetFileVersionInfoSize(W2T((LPWSTR)&filename), &zeroHandle);
		if (size == 0)
			break;

		LPVOID pBlock = (LPVOID) new char[size];
		if (!GetFileVersionInfo(W2T((LPWSTR)&filename), zeroHandle, size, pBlock))
		{
			delete[] pBlock;
			break;
		}

		struct LANGANDCODEPAGE
		{
			WORD wLanguage;
			WORD wCodePage;
		} *lpTranslate;
		UINT uLen = 0;

		if (!VerQueryValue(pBlock, TEXT("\\VarFileInfo\\Translation"), (LPVOID*)&lpTranslate, &uLen) || (uLen == 0))
		{
			delete[] pBlock;
			break;
		}

		LPWSTR SubBlock = new wchar_t[256];
		swprintf(SubBlock, L"\\StringFileInfo\\%04x%04x\\FileVersion", lpTranslate[0].wLanguage, lpTranslate[0].wCodePage);

		LPSTR lpBuffer;
		UINT dwBytes = 0;
		if (!VerQueryValue(pBlock, W2T(SubBlock), (LPVOID*)&lpBuffer, &dwBytes))
		{
			delete[] pBlock;
			delete[] SubBlock;
			break;
		}
		((*this) << "Build - v" << lpBuffer << "\n").Write();

		delete[] SubBlock;
		delete[] pBlock;
		break;
	}
	if (size == 0)
		((*this) << "Error reading version number\n").Write();
}

void LogMessage::writef(char *sz,...)
{
    char buf[8192];

    va_list va;
    va_start(va, sz);
    vsprintf((char *)&buf, sz, va);
    va_end(va);

	sprintf((char*)m_str, "%s%s", m_str, buf);

	Write(FALSE);
}

void LogMessage::showf(char *sz,...)
{
    char buf[8192];

    va_list va;
    va_start(va, sz);
    vsprintf((char *)&buf, sz, va);
    va_end(va);

	sprintf((char*)m_str, "%s%s", m_str, buf);

	Show(FALSE);
}

//Numbers
LogMessage& LogMessage::operator<< (const int& val)
{
	sprintf((char*)m_str, "%s%i", m_str, val);
	return *this;
}
LogMessage& LogMessage::operator<< (const double& val)
{
	sprintf((char*)m_str, "%s%f", m_str, val);
	return *this;
}
LogMessage& LogMessage::operator<< (const __int64& val)
{
	sprintf((char*)m_str, "%s%i", m_str, val);
	return *this;
}

//Characters
LogMessage& LogMessage::operator<< (const char& val)
{
	sprintf((char*)m_str, "%s%c", m_str, val);
	return *this;
}
LogMessage& LogMessage::operator<< (const wchar_t& val)
{
	sprintf((char*)m_str, "%s%C", m_str, val);
	return *this;
}

//Strings
LogMessage& LogMessage::operator<< (const LPSTR& val)
{
	sprintf((char*)m_str, "%s%s", m_str, val);
	return *this;
}
LogMessage& LogMessage::operator<< (const LPWSTR& val)
{
	sprintf((char*)m_str, "%s%S", m_str, val);
	return *this;
}
LogMessage& LogMessage::operator<< (const LPCSTR& val)
{
	sprintf((char*)m_str, "%s%s", m_str, val);
	return *this;
}
LogMessage& LogMessage::operator<< (const LPCWSTR& val)
{
	sprintf((char*)m_str, "%s%S", m_str, val);
	return *this;
}
