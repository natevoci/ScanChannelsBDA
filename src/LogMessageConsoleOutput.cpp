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

void LogMessageConsoleOutput::Show(LPSTR pStr)
{
	cout << pStr;
}

