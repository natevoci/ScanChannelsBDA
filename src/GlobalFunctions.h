/**
 *	GlobalFunctions.h
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

#ifndef GLOBALFUNCTIONS_H
#define GLOBALFUNCTIONS_H

#include "stdafx.h"

typedef struct
{
	double left;
	double top;
	double right;
	double bottom;
} dRECT;

void PStringCopy(char** destString, char* srcString);
void PStringCopy(wchar_t** destString, wchar_t* srcString);
void SetdRect(dRECT* rect, double left, double top, double right, double bottom);
void SetdRectEmpty(dRECT* bounds);

void GetCommandPath(LPWSTR pPath);
void GetCommandExe(LPWSTR pExe);

long wcsToColor(LPWSTR string);

BOOL findchr(char character, LPCSTR strCharSet);
BOOL isWhitespace(char character);
void skipWhitespaces(LPCSTR &str);
LPSTR findEndOfTokenName(LPCSTR str);

BOOL findchr(wchar_t character, LPCWSTR strCharSet);
BOOL isWhitespace(wchar_t character);
void skipWhitespaces(LPCWSTR &str);
LPWSTR findEndOfTokenName(LPCWSTR str);

void strCopy(LPSTR &dest, LPCSTR src, long length);
void strCopy(LPWSTR &dest, LPCWSTR src, long length);
void strCopyA2W(LPWSTR &dest, LPCSTR src, long length);
void strCopyW2A(LPSTR &dest, LPCWSTR src, long length);

void strCopy(LPSTR &dest, LPCSTR src);
void strCopy(LPWSTR &dest, LPCWSTR src);
void strCopyA2W(LPWSTR &dest, LPCSTR src);
void strCopyW2A(LPSTR &dest, LPCWSTR src);

void strCopy(LPSTR &dest, long value);
void strCopy(LPWSTR &dest, long value);

#endif
