/**
 *	ParseLine.h
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

#ifndef PARSELINE_H
#define PARSELINE_H

#include "StdAfx.h"

typedef struct PARSELINE_FUNCTION_tag
{
	LPWSTR Function;
	LPWSTR FunctionName;
	LPWSTR* Parameter;
	long ParameterCount;
} PARSELINE_FUNCTION;

class ParseLine  
{
public:

	ParseLine();
	virtual ~ParseLine();

	BOOL Parse(LPWSTR line);

	BOOL HasRHS();
	PARSELINE_FUNCTION LHS;
	PARSELINE_FUNCTION RHS;

	LPWSTR GetErrorMessage();
	long GetErrorPosition();


private:
	BOOL ParseFunction(LPWSTR &strStart, BOOL bRHS);
	void AddParameter(PARSELINE_FUNCTION &LHS, LPCWSTR src, long length);

	BOOL SetErrorMessage(LPCWSTR message, long position);

	BOOL m_bHasRHS;

	LPWSTR m_strLine;
	LPWSTR m_strCurr;

	LPWSTR m_strErrorMessage;
	long m_nErrorPosition;
};

#endif
