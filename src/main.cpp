/**
 *	main.cpp
 *	Copyright (C) 2004 BionicDonkey
 *  Copyright (C) 2004 Nate
 *  Copyright (C) 2004 JoeyBloggs
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

#include "stdafx.h"
#include "ScanChannelsBDA.h"
#include "FilterGraphTools.h"
#include "LogMessage.h"
#include "LogMessageWriter.h"
#include "LogMessageConsoleOutput.h"
#include "GlobalFunctions.h"

LogMessage g_log;
LogMessageConsoleOutput consOut;
LogMessageWriter lmw;

HRESULT ShowMenu();
HRESULT GetChannelInfo(long &freq, long &band);
HRESULT GetFileName(LPTSTR pFilename);

int	_tmain(int argc, _TCHAR* argv[])
{
	HRESULT hr;

	lmw.SetFilename(L"ScanChannelsBDA.log");
	int writeHandle = g_log.AddCallback(&lmw);

	int consoleHandle = g_log.AddCallback(&consOut);

	g_log.ClearFile();
	(g_log << "-----------------------\n").Write();
	(g_log << "ScanChannelBDA starting\n").Write();
	g_log.LogVersionNumber();

	hr = ShowMenu();

	g_log.RemoveCallback(writeHandle);
	g_log.RemoveCallback(consoleHandle);

	return 0;
}

HRESULT ShowMenu()
{
	HRESULT hr = S_OK;

	BOOL bExit = FALSE ;
	char strMenuSelect[80];
	int iMenuSelect = 0;

	cout << "This program was designed to scan for channels on BDA DVB-T devices." << endl;
	cout << endl;
	cout << "Usage:" << endl;
	cout << "  Use 'Add Network' to add each frequency for your area." << endl;
	cout << "  eg." << endl;
	cout << "    Please Select Menu(1,2,5,6,10):1" << endl;
	cout << "    Frequency (KHz):177500" << endl;
	cout << "    BandWidth (Mbps):7" << endl;
	cout << endl;
	cout << "  Then use option 2 to begin scanning" << endl;
	cout << endl;
	cout << "DigitalWatch Notes:" << endl;
	cout << "  To create the channels.ini entries for DigitalWatch use option 3 to" << endl;
	cout << "  generate the channel listing." << endl;
	cout << "  Once finished you can cut and paste the entries into your channels.ini file." << endl;
	cout << "  - If a channel has multiple audio pids then it will be listed multiple times." << endl;
	cout << "  - If an AC3 PID is detected then an A will be prefixed to the audio pid." << endl;
	cout << "  - If an AC3 PID is detected and is not the first pid for the program then the" << endl;
	cout << "    name will be suffixed with AC3" << endl << endl << endl;



	BDAChannelScan* cBDAChannelScan = new BDAChannelScan();
	cBDAChannelScan->AddRef();

	if (FAILED(hr = cBDAChannelScan->selectCard()))
	{
		return 1;
	}
	if (hr != S_OK)
		return 0;

	bExit = FALSE;
	while (bExit == FALSE)
	{
		cout << endl;
		cout << "1. Add Network" << endl;
		cout << "2. Scan Networks" << endl;
		cout << "3. Generate Channels.ini" << endl;
		cout << "4. Signal Statistics" << endl;
		cout << "5. Scan all Australian frequencies"  << endl;
		cout << "6. Turn Verbose output " << (cBDAChannelScan->IsVerbose() ? "Off" : "On") << endl;
		cout << "8. Test TSFileSink " << endl;
		cout << "9. Toggle Lock Detection : ";
		
		switch (cBDAChannelScan->LockDetectionMode())
		{
		case LOCK_MODE_LOCKED:
			cout << "Using (Signal Locked != 0)" << endl; break;
		case LOCK_MODE_QUALITY:
			cout << "Using (Quality > 0)" << endl; break;
		default:
			cout << "Unknown Mode" << endl; break;
		};
		
		cout << "10.Exit"		 << endl;
		cout << endl;
		cout << "Please Select Menu(1,2,3,4,5,6,8,9,10):";

		cin >> strMenuSelect;

		iMenuSelect = 0;
		for (int i=0 ; (strMenuSelect[i]!='\0' && i<80) ; i++ )
		{
			if ((strMenuSelect[i] < '0') || (strMenuSelect[i] > '9'))
			{
				iMenuSelect = -1;
				break;
			}
			iMenuSelect *= 10;
			iMenuSelect += strMenuSelect[i] - '0';
		}

		switch (iMenuSelect)
		{
		case -1:
			cout << "Invalid number, Please Select again" << endl;  
			continue;

		case 1:
			long freq, band;
			GetChannelInfo(freq, band);
			cBDAChannelScan->AddNetwork(freq, band);
			break;

		case 2:
			cBDAChannelScan->scanNetworks(FALSE);
			break;

		case 3:
			cBDAChannelScan->scanNetworks(TRUE);
			break;

		case 4:
			GetChannelInfo(freq, band);
			cBDAChannelScan->SignalStatistics(freq, band);
			break;

		case 5:
			cBDAChannelScan->scanAll();
			break;

		case 6:
			cBDAChannelScan->ToggleVerbose();
			break;

		case 8:
			{
				GetChannelInfo(freq, band);
				LPTSTR pFilename = new TCHAR[MAX_PATH];
				
				if (GetFileName(pFilename) == S_OK)
					cBDAChannelScan->TestTSFileSink(freq, band, pFilename);

				delete[] pFilename;
			}
			break;

		case 9:
			cBDAChannelScan->ToggleLockDetectionMode();
			break;

		case 10:
			bExit = TRUE;
			break;

		default:
			cout << "Error selecting, Please Select again" << endl;  
			continue;
		}
	}


	cBDAChannelScan->StopGraph();

	cBDAChannelScan->Release();

	return S_OK;
}

HRESULT GetChannelInfo(long &freq, long &band)
{
	freq = 0;
	band = 0;
	cout << "Frequency (KHz):";
	cin >> freq;
	while (freq <= 0)
	{
		cout << "invalid number. Please try again:";
		cin >> freq;
	}

	cout << "BandWidth (Mbps):";
	cin >> band;
	while (band <= 0)
	{
		cout << "invalid number. Please try again:";
		cin >> band;
	}

	return S_OK;
}

HRESULT GetFileName(LPTSTR pFilename)
{
	pFilename[0] = '\0';

    // Setup the OPENFILENAME structure
    OPENFILENAME ofn = { sizeof(OPENFILENAME), NULL, NULL,
                         TEXT("Timeshift Buffer Files\0*.tsbuffer\0All Files\0*.*\0\0"), NULL,
                         0, 1,
						 pFilename, MAX_PATH,
						 NULL, 0,
						 NULL,
                         TEXT("Save As"),
                         OFN_OVERWRITEPROMPT|OFN_HIDEREADONLY, 0, 0,
                         TEXT(".tsbuffer"), 0, NULL, NULL };

    // Display the SaveFileName dialog.
    if( GetSaveFileName( &ofn ) == FALSE )
		return S_FALSE;

	return S_OK;
}
