/**
 *	LogMessage.h
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



#ifndef LOGMESSAGE_H
#define LOGMESSAGE_H

#include "StdAfx.h"
#include <vector>

using namespace std;

class LogMessageCallback
{
public:
	LogMessageCallback();
	virtual ~LogMessageCallback();
	int GetHandle();
	virtual void Write(LPSTR pStr) {};
	virtual void Show(LPSTR pStr) {};
	virtual void Clear() {};
private:
	int m_handle;
};

class LogMessage
{
public:
	LogMessage();
	virtual ~LogMessage();

	int AddCallback(LogMessageCallback *callback);
	void RemoveCallback(int handle);

	int Show();
	int Show(int returnValue);

	int Write();
	int Write(int returnValue);

	void ClearFile();

	void LogVersionNumber();

	void writef(char *sz,...);
	void showf(char *sz,...);

	LogMessage& operator<< (const int& val);
	LogMessage& operator<< (const double& val);
	LogMessage& operator<< (const __int64& val);

	LogMessage& operator<< (const char& val);
	LogMessage& operator<< (const wchar_t& val);

	LogMessage& operator<< (const LPSTR& val);
	LogMessage& operator<< (const LPWSTR& val);

	LogMessage& operator<< (const LPCSTR& val);
	LogMessage& operator<< (const LPCWSTR& val);

private:
	void WriteLogMessage();

	char m_str[8192];

	int callbackHandleID;
	vector<LogMessageCallback *> callbacks;
};


extern LogMessage g_log;

#endif
