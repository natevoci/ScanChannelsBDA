/**
 *	ParseLine.cpp
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

#include "ParseLine.h"
#include "GlobalFunctions.h"

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

ParseLine::ParseLine()
{
	m_strLine = NULL;
	m_strCurr = NULL;
	m_strErrorMessage = NULL;
	m_nErrorPosition = 0;
	m_bHasRHS = FALSE;
	ZeroMemory(&LHS, sizeof(PARSELINE_FUNCTION));
	ZeroMemory(&RHS, sizeof(PARSELINE_FUNCTION));
}

ParseLine::~ParseLine()
{
	if (m_strLine)
		delete[] m_strLine;
	if (m_strErrorMessage)
		delete[] m_strErrorMessage;
	
	if (LHS.FunctionName)
		delete[] LHS.FunctionName;
	if (LHS.ParameterCount > 0)
	{
		for (int i=0 ; i<LHS.ParameterCount ; i++ )
			delete[] LHS.Parameter[i];
		delete[] LHS.Parameter;
	}
}

BOOL ParseLine::Parse(LPWSTR line)
{
	if (line[0] == '\0')
		return FALSE;

	if (m_strLine)
		delete[] m_strLine;
	if (m_strErrorMessage)
		delete[] m_strErrorMessage;

	strCopy(m_strLine, line);
	LPWSTR strCurr = m_strLine;

	return ParseFunction(strCurr, 0);
}

BOOL ParseLine::ParseFunction(LPWSTR &strCurr, BOOL bRHS)
{
	BOOL bResult = FALSE;

	skipWhitespaces(strCurr);

	LPWSTR startOfFunction = strCurr;

	LPWSTR endOfToken = findEndOfTokenName(strCurr);
	if (endOfToken == strCurr)
		return SetErrorMessage(L"Invalid character found", strCurr-m_strLine);

	if (bRHS)
		strCopy(RHS.FunctionName, strCurr, endOfToken - strCurr);
	else
		strCopy(LHS.FunctionName, strCurr, endOfToken - strCurr);

	strCurr = endOfToken;
	skipWhitespaces(strCurr);

	if (strCurr[0] == '(')
	{
		strCurr++;
		skipWhitespaces(strCurr);
		if (strCurr[0] != ')')
		{
			strCurr--;
			do
			{
				strCurr++;
				skipWhitespaces(strCurr);
				
				if (strCurr[0] == '"')
				{
					strCurr++;
					LPWSTR nextChr = strCurr;
					do
					{
						if (nextChr[0] == '\\')
						{
							if (nextChr[1] == '\0')
								return SetErrorMessage(L"Line continuation by using a backslash is not supported.", strCurr-m_strLine);
							nextChr += 2;
						}
						nextChr = wcspbrk(nextChr, L"\\\"");
						if (nextChr == NULL)
							return SetErrorMessage(L"Cannot find end of string", strCurr-m_strLine);
					} while (nextChr[0] != '"');

					if (bRHS)
						AddParameter(RHS, strCurr, nextChr - strCurr);
					else
						AddParameter(LHS, strCurr, nextChr - strCurr);

					strCurr = nextChr+1;
				}
				else
				{
					LPWSTR nextChr = wcspbrk(strCurr, L",)");
					if (nextChr == NULL)
						return SetErrorMessage(L"Cannot find next ',' or ')' in function", strCurr-m_strLine);

					for (nextChr-- ; ((nextChr > strCurr) && (isWhitespace(nextChr[0]))) ; nextChr--);
					if (nextChr >= strCurr)
						nextChr++;

					if (nextChr <= strCurr)
						return SetErrorMessage(L"Missing parameter", strCurr-m_strLine);

					if (bRHS)
						AddParameter(RHS, strCurr, nextChr - strCurr);
					else
						AddParameter(LHS, strCurr, nextChr - strCurr);

					strCurr = nextChr;
				}
				skipWhitespaces(strCurr);
			}
			while (strCurr[0] == ',');
		}

		if (strCurr[0] == '\0')
			return SetErrorMessage(L"End of line found. Expecting ',' or ')'", strCurr-m_strLine);
		if (strCurr[0] != ')')
			return SetErrorMessage(L"Invalid character found. Expecting ',' or ')'", strCurr-m_strLine);

		strCurr++;
		bResult = TRUE;
	}

	if (bRHS)
		strCopy(RHS.Function, startOfFunction, strCurr-startOfFunction);
	else
		strCopy(LHS.Function, startOfFunction, strCurr-startOfFunction);

	skipWhitespaces(strCurr);

	if (strCurr[0] == '=')
	{
		if (bRHS)
			return SetErrorMessage(L"Cannot assign twice. Unexpected '=' found", strCurr-m_strLine);

		m_bHasRHS = TRUE;

		strCurr++;
		LPWSTR endOfFunction = strCurr;
		return ParseFunction(endOfFunction, TRUE);
	}

	if (strCurr[0] == '\0')
		return TRUE;
	else if (strCurr[0] == '#')
		return TRUE;
	else
		return SetErrorMessage(L"Invalid character found. Expecting '=' or '('", strCurr-m_strLine);
}

void ParseLine::AddParameter(PARSELINE_FUNCTION &LHS, LPCWSTR src, long length)
{
	LPWSTR *newParams = new LPWSTR[LHS.ParameterCount+1];

	for (int i=0 ; i<LHS.ParameterCount ; i++)
		newParams[i] = LHS.Parameter[i];

	newParams[LHS.ParameterCount] = NULL;
	strCopy(newParams[LHS.ParameterCount], src, length);

	if (LHS.Parameter)
		delete[] LHS.Parameter;

	LHS.Parameter = newParams;
	LHS.ParameterCount++;
}

BOOL ParseLine::HasRHS()
{
	return m_bHasRHS;
}

LPWSTR ParseLine::GetErrorMessage()
{
	return m_strErrorMessage;
}

long ParseLine::GetErrorPosition()
{
	return m_nErrorPosition;
}

BOOL ParseLine::SetErrorMessage(LPCWSTR message, long position)
{
	strCopy(m_strErrorMessage, message);
	m_nErrorPosition = position;
	return FALSE;
}

