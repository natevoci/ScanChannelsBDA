/**
 *	FileWriter.h
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

#ifndef FILEWRITER_H
#define FILEWRITER_H

#include "StdAfx.h"

class FileWriter  
{
public:
	FileWriter();
	virtual ~FileWriter();

	BOOL Open(LPCWSTR filename);
	void Close();

	FileWriter& operator<< (const int& val);
	FileWriter& operator<< (const double& val);
	FileWriter& operator<< (const __int64& val);

	FileWriter& operator<< (const char& val);
	FileWriter& operator<< (const wchar_t& val);

	FileWriter& operator<< (const LPSTR& val);
	FileWriter& operator<< (const LPWSTR& val);

	FileWriter& operator<< (const LPCSTR& val);
	FileWriter& operator<< (const LPCWSTR& val);

	struct FileWriterEOL
	{
	} EOL;

	FileWriter& operator<< (const FileWriterEOL& val);

private:
	HANDLE m_hFile;

};

#endif
