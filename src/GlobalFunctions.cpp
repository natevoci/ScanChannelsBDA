/**
 *	GlobalFunctions.cpp
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

//this is a file from DigitalWatch 2.

#include "StdAfx.h"
#include "GlobalFunctions.h"

void PStringCopy(char** destString, char* srcString)
{
	if (*destString)
		delete *destString;
	*destString = new char[strlen(srcString)+1];
	strcpy(*destString, srcString);
}

void PStringCopy(wchar_t** destString, wchar_t* srcString)
{
	if (*destString)
		delete *destString;
	*destString = new wchar_t[wcslen(srcString)+1];
	wcscpy(*destString, srcString);
}

void SetdRect(dRECT* rect, double left, double top, double right, double bottom)
{
	rect->top = top;
	rect->left = left;
	rect->right = right;
	rect->bottom = bottom;
}

void SetdRectEmpty(dRECT* rect)
{
	rect->top = 0;
	rect->left = 0;
	rect->right = 0;
	rect->bottom = 0;
}

void GetCommandPath(LPWSTR pPath)
{
	USES_CONVERSION;

	LPWSTR cmdLine = A2W(GetCommandLine());
	LPWSTR pCurr;

	if (cmdLine[0] == '"')
	{
		cmdLine++;
		pCurr = wcschr(cmdLine, '"');	//the +1 is because it starts with a quote
		if (pCurr <= 0) //cannot find closing quote
			return;
		pCurr[0] = '\0';

	}
	pCurr = wcsrchr(cmdLine, '\\');
	if (pCurr <= 0)
	{
		pPath[0] = 0;
		return;
	}
	pCurr[1] = '\0';

	wcscpy(pPath, cmdLine);
}

void GetCommandExe(LPWSTR pExe)
{
	USES_CONVERSION;

	LPWSTR cmdLine = A2W(GetCommandLine());
	LPWSTR pCurr;

	if (cmdLine[0] == '"')
	{
		cmdLine++;
		pCurr = wcschr(cmdLine, '"');	//the +1 is because it starts with a quote
		if (pCurr <= 0) //cannot find closing quote
			return;
		pCurr[0] = '\0';

	}
	wcscpy(pExe, cmdLine);
}

BOOL findchr(char character, LPCSTR strCharSet)
{
	int length = strlen(strCharSet);
	for ( int i=0 ; i<length ; i++ )
	{
		if (strCharSet[i] == character)
			return TRUE;
	}
	return FALSE;
}

BOOL isWhitespace(char character)
{
	return ((character == ' ') ||
			(character == '\t'));
}

void skipWhitespaces(LPSTR &str)
{
	while (isWhitespace(str[0]))
		str++;
}

LPSTR findEndOfTokenName(LPCSTR str)
{
	int length = strlen(str);
	for ( int i=0 ; i<=length ; i++ )
	{
		if (((str[i] < 'A') || (str[i] > 'Z')) &&
			((str[i] < 'a') || (str[i] > 'z')) &&
			((str[i] < '0') || (str[i] > '9')) &&
			(str[i] != '_')
		   )
		   return (LPSTR)&str[i];
	}
	//Should never get to here because the for loop includes the '\0' character
	return NULL;
}

BOOL findchr(wchar_t character, LPCWSTR strCharSet)
{
	int length = wcslen(strCharSet);
	for ( int i=0 ; i<length ; i++ )
	{
		if (strCharSet[i] == character)
			return TRUE;
	}
	return FALSE;
}

BOOL isWhitespace(wchar_t character)
{
	return ((character == ' ') ||
			(character == '\t'));
}

void skipWhitespaces(LPWSTR &str)
{
	while (isWhitespace(str[0]))
		str++;
}

LPWSTR findEndOfTokenName(LPCWSTR str)
{
	int length = wcslen(str);
	for ( int i=0 ; i<=length ; i++ )
	{
		if (((str[i] < 'A') || (str[i] > 'Z')) &&
			((str[i] < 'a') || (str[i] > 'z')) &&
			((str[i] < '0') || (str[i] > '9')) &&
			(str[i] != '_')
		   )
		   return (LPWSTR)&str[i];
	}
	//Should never get to here because the for loop includes the '\0' character
	return NULL;
}

void strCopy(LPSTR &dest, LPCSTR src, long length)
{
	if (dest)
		delete[] dest;
	if (length < 0)
		length = strlen(src);
	dest = new char[length + 1];
	memcpy(dest, src, length);
	dest[length] = 0;
}

void strCopy(LPWSTR &dest, LPCWSTR src, long length)
{
	if (dest)
		delete[] dest;
	if (length < 0)
		length = wcslen(src);
	dest = new wchar_t[length + 1];
	memcpy(dest, src, length*2);
	dest[length] = 0;
}

void strCopyA2W(LPWSTR &dest, LPCSTR src, long length)
{
	if (dest)
		delete[] dest;
	if (length < 0)
		length = strlen(src);
	dest = new wchar_t[length + 1];
	mbstowcs(dest, src, length);
	dest[length] = 0;
}

void strCopyW2A(LPSTR &dest, LPCWSTR src, long length)
{
	if (dest)
		delete[] dest;
	if (length < 0)
		length = wcslen(src);
	dest = new char[length + 1];
	wcstombs(dest, src, length);
}

