/**
 *	LogMessageWriter.cpp
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

#include "LogMessageWriter.h"
#include "GlobalFunctions.h"

#include <fstream>
using namespace std;

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

LogMessageWriter::LogMessageWriter()
{
	m_logFilename = NULL;
}

LogMessageWriter::~LogMessageWriter()
{
	if (m_logFilename)
		delete[] m_logFilename;
}

void LogMessageWriter::SetFilename(LPWSTR filename)
{
	if ((wcslen(filename) > 2) &&
		((filename[1] == ':') ||
		 (filename[0] == '\\' && filename[1] == '\\')
		)
	   )
	{
		strCopy(m_logFilename, filename);
	}
	else
	{
		LPWSTR str = new wchar_t[MAX_PATH];
		GetCommandPath(str);
		swprintf(str, L"%s%s", str, filename);
		strCopy(m_logFilename, str);
		delete[] str;
	}
}

void LogMessageWriter::Write(LPSTR pStr)
{
	USES_CONVERSION;
	if (m_logFilename)
	{
		ofstream file;
		file.open(W2A(m_logFilename), ios::app);

		if (file.is_open() == 1)
		{
			file << pStr;
			file.close();
		}
	}
}

void LogMessageWriter::Clear()
{
	USES_CONVERSION;
	if (m_logFilename)
	{
		ofstream file;
		file.open(W2A(m_logFilename));

		if (file.is_open() == 1)
		{
			file.close();
		}
	}
}

