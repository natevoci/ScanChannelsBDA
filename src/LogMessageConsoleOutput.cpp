// LogMessageConsoleOutput.cpp: implementation of the LogMessageConsoleOutput class.
//
//////////////////////////////////////////////////////////////////////

#include "LogMessageConsoleOutput.h"
#include <iostream>

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

LogMessageConsoleOutput::LogMessageConsoleOutput()
{

}

LogMessageConsoleOutput::~LogMessageConsoleOutput()
{

}

void LogMessageConsoleOutput::Show(LPWSTR pStr)
{
	USES_CONVERSION;
	cout << W2A(pStr);
}

