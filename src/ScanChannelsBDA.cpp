/**
 *	ScanChannelsBDA.cpp
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
#include "LogMessage.h"

#include <ks.h> // Must be included before ksmedia.h
#include <ksmedia.h> // Must be included before bdamedia.h
#include <bdamedia.h>
#include <stdio.h>

static CLSID CLSID_TSFileSink  = {0x5cdd5c68, 0x80dc, 0x43e1, {0x9e, 0x44, 0xc8, 0x49, 0xca, 0x80, 0x26, 0xe7}};

extern LogMessageWriter lmw;

BDAChannelScan::BDAChannelScan()
{
	CoInitializeEx(NULL, COINIT_MULTITHREADED);

	m_bServiceChanged = FALSE;
	m_bProgramChanged = FALSE;
	m_bScheduleEntryChanged = FALSE;

	m_hGuideDataChangedEvent = CreateEvent(NULL, TRUE, FALSE, "GuideDataChangedEvent");
	m_hServiceChangedMutex = CreateMutex(NULL, FALSE, NULL);

	m_pBDACard = NULL;

	m_rotEntry = 0;

	m_Count = 0;
	m_bScanning = FALSE;
	m_bVerbose = FALSE;
	m_nLockDetectionMode = 0;

	cardList.SetLogCallback(&lmw);
	SetLogCallback(&m_console);
	SetLogCallback(&lmw);
	m_consoleHandle = m_mpeg2parser.Output()->AddCallback(&m_console);
	m_logfileHandle = m_mpeg2parser.Output()->AddCallback(&lmw);
	verbose.AddCallback(&lmw);
}

BDAChannelScan::~BDAChannelScan()
{
	m_mpeg2parser.Output()->RemoveCallback(m_consoleHandle);
	m_mpeg2parser.Output()->RemoveCallback(m_logfileHandle);
	CoUninitialize();
}

HRESULT BDAChannelScan::selectCard()
{
	cardList.LoadCards();

	int cardsFound = cardList.cards.size();
	if (cardsFound == 0)
	{
		return (log << "Could not find any BDA devices.\n").Write(E_FAIL);
	}

	BDACard *bdaCard = NULL;

	if (cardsFound == 1)
	{
		m_pBDACard = *(cardList.cards.begin());
		return (log << "Found BDA device: " << m_pBDACard->tunerDevice.strFriendlyName << "\n").Write(S_OK);
	}

	(log << cardsFound << " BDA devices found.\n").Show();

	std::vector<BDACard *>::iterator it;
	int id, cardID;

	while (TRUE)
	{
		cout << "Please select which device to use." << endl;

		id = 1;
		it = cardList.cards.begin();
		for ( ; it != cardList.cards.end() ; it++ )
		{
			bdaCard = *it;
			printf("%i. %S\n", id, bdaCard->tunerDevice.strFriendlyName);
			id++;
		}
		cout << id << ". Exit" << endl;
		cout << "> ";
		cin >> cardID;

		//Did Exit get chosen?
		if (cardID-1 == cardsFound)
		{
			return (log << "Exit chosen\n").Write(E_FAIL);
		}

		//check for invalid selection
		if ((cardID > cardsFound) || (cardID <= 0))
		{
			cout << "Invalid device selection. Please try again." << endl;
			continue;
		}

		id = 1;
		it = cardList.cards.begin();
		for ( ; it != cardList.cards.end() ; it++ )
		{
			if (id == cardID)
			{
				m_pBDACard = *it;
				return (log << "using " << m_pBDACard->tunerDevice.strFriendlyName << "\n").Write(S_OK);
			}
			id++;
		}
		(log << "Weird error. Could not find selected device in list\n").Write();
	}

	return E_FAIL;
}

void BDAChannelScan::AddNetwork(long freq, long band)
{
	if (m_Count < 256)
	{
		(log << "Adding network " << freq << ", " << band << "\n").Write();
		m_freq[m_Count] = freq;
		m_band[m_Count] = band;
		m_Count++;
	}
	else
	{
		(log << "Cannot add more than 256 networks\n").Write();
	}
}

HRESULT BDAChannelScan::SetupGraph()
{
	HRESULT hr = S_OK;

	if (FAILED(hr = CreateGraph()))
	{
		return (log << "Failed to create graph.\n").Show(hr);
	}
	
	if (FAILED(hr = BuildGraph()))
	{
		return (log << "Failed to add filters to graph.\n").Show(hr);
	}

	if (FAILED(hr = ConnectGraph()))
	{
		return (log << "Failed to connect filters in graph.\n").Show(hr);
	}

	if (FAILED(hr = createConnectionPoint()))
	{
		return (log << "Failed to add connection point to TIF.\n").Show(hr);
	}

	if (StartGraph() == FALSE)
	{
		return (log << "Failed to start the graph\n").Show(hr);
	}

	return hr;
}

HRESULT BDAChannelScan::SetupGraphForTSFileSink(long freq, long band, LPTSTR pFilename)
{
	HRESULT hr = S_OK;

	(log << "Creating graph\n").Show();
	if (FAILED(hr = CreateGraph()))
	{
		return (log << "Failed to create graph.\n").Show(hr);
	}
	
	(log << "Building graph\n").Show();
	if (FAILED(hr = BuildTSFileSinkGraph(freq, band, pFilename)))
	{
		return (log << "Failed to add filters to graph.\n").Show(hr);
	}

	(log << "Connecting \n").Show();
	if (FAILED(hr = ConnectTSFileSinkGraph()))
	{
		return (log << "Failed to connect filters in graph.\n").Show(hr);
	}

	(log << "Starting graph\n").Show();
	if (StartGraph() == FALSE)
	{
		return (log << "Failed to start the graph\n").Show(hr);
	}

	return hr;
}

void BDAChannelScan::DestroyGraph()
{
	StopGraph();

	if (m_piConnectionPoint)
		m_piConnectionPoint->Unadvise(m_dwAdviseCookie); 

	m_piConnectionPoint.Release();
	//m_piGuideData.Release();

	graphTools.DisconnectAllPins(m_piGraphBuilder);

	if (m_pBDACard)
		m_pBDACard->RemoveFilters();

	m_pTuningSpace.Release();
	m_piTuner.Release();
	graphTools.RemoveAllFilters(m_piGraphBuilder);

	m_pBDASecTab.Release();
	m_pBDATIF.Release();
	m_pBDAMpeg2Demux.Release();

	try
	{
		m_pBDANetworkProvider.Release();	//causes an exception. don't know why
		//m_pBDANetworkProvider.Detach();		//so i'll just do this instead
	}
	catch (...)
	{
		m_pBDANetworkProvider.Detach();
	}

	if (m_rotEntry)
		graphTools.RemoveFromRot(m_rotEntry);
	m_piMediaControl.Release();
	m_piGraphBuilder.Release();

}

void BDAChannelScan::DestroyGraphForTSFileSink()
{
	(log << "Stopping graph\n").Show();
	StopGraph();

	(log << "Disconnecting graph\n").Show();
	graphTools.DisconnectAllPins(m_piGraphBuilder);

	(log << "Removing BDA filters\n").Show();
	if (m_pBDACard)
		m_pBDACard->RemoveFilters();

	m_pTuningSpace.Release();
	m_piTuner.Release();

	(log << "Removing remaining filters\n").Show();
	graphTools.RemoveAllFilters(m_piGraphBuilder);

	(log << "Releasing COM objects\n").Show();
	m_pTSFileSink.Release();

	try
	{
		m_pBDANetworkProvider.Release();	//causes an exception. don't know why
		//m_pBDANetworkProvider.Detach();		//so i'll just do this instead
	}
	catch (...)
	{
		m_pBDANetworkProvider.Detach();
	}

	if (m_rotEntry)
		graphTools.RemoveFromRot(m_rotEntry);
	m_piMediaControl.Release();
	m_piGraphBuilder.Release();

}

HRESULT BDAChannelScan::scanNetworks()
{
	HRESULT hr = S_OK;

	(log << "Scanning Networks\n").Write();

	if (FAILED(hr = SetupGraph()))
		return hr;

	for (int count=0 ; count<m_Count ; count++ )
	{
		hr = scanChannel(count+1, m_freq[count], m_band[count]);
		if (hr == S_FALSE)
			(log << "# Nothing found on " << m_freq[count] << "kHz frequency, moving on.\n").Show();
		if (hr == E_FAIL)
			return 1;
	}

	DestroyGraph();

	return hr;
}

HRESULT BDAChannelScan::scanAll()
{
	HRESULT hr = S_OK;

	static int ulFrequencyArray[50] =
	{
		// Band III VHF
		177500,  // 6
		184500,  // 7
		191500,  // 8
		198500,  // 9
		205500,  // 9A
		212500,  // 10
		219500,  // 11
		226500,  // 12

		// Band IV UHF
		529500,  // 28
		536500,  // 29
		543500,  // 30
		550500,  // 31
		557500,  // 32
		564500,  // 33
		571500,  // 34
		578500,  // 35

		// Band V UHF
		585500,  // 36
		592500,  // 37
		599500,  // 38
		606500,  // 39
		613500,  // 40
		620500,  // 41
		627500,  // 42
		634500,  // 43
		641500,  // 44
		648500,  // 45
		655500,  // 46
		662500,  // 47
		669500,  // 48
		676500,  // 49
		683500,  // 50
		690500,  // 51
		697500,  // 52
		704500,  // 53
		711500,  // 54
		718500,  // 55
		725500,  // 56
		732500,  // 57
		739500,  // 58
		746500,  // 59
		753500,  // 60
		760500,  // 61
		767500,  // 62
		774500,  // 63
		781500,  // 64
		788500,  // 65
		795500,  // 66
		802500,  // 67
		809500,  // 68
		816500   // 69
	};

	if (FAILED(hr = SetupGraph()))
		return hr;

	int chNum = 0;
	for (int count=0 ; count<50 ; count++ )
	{
		chNum++;
		hr = scanChannel(chNum, ulFrequencyArray[count], 7);
		if (hr == E_FAIL)
		{
			return (log << "Error locking channel. Aborting\n").Show(E_FAIL);
		}
		if (hr == S_OK)
			continue;

		(log << "# Nothing found on " << ulFrequencyArray[count] << "kHz, trying +125").Show();

		hr = scanChannel(chNum, ulFrequencyArray[count] + 125, 7);
		if (hr == E_FAIL)
		{
			return (log << "\nError locking channel. Aborting\n").Show(E_FAIL);
		}
		if (hr == S_OK)
			continue;

		(log << ", trying -125").Show();

		hr = scanChannel(chNum, ulFrequencyArray[count] - 125, 7);
		if (hr == E_FAIL)
		{
			return (log << "\nError locking channel. Aborting\n").Show(E_FAIL);
		}
		if (hr == S_OK)
			continue;

		(log << ", Nothing found\n").Show();

		chNum--;
	}

	DestroyGraph();

	return hr;
}

HRESULT BDAChannelScan::SignalStatistics(long frequency, long bandwidth)
{
	HRESULT hr;

	if (FAILED(hr = SetupGraph()))
		return hr;

	BOOL bLocked = FALSE;
	BOOL bPresent = FALSE;
	long nStrength = 0;
	long nQuality = 0;

	BOOL bKeyboardEvent = FALSE;
	HANDLE hInput = ::GetStdHandle(STD_INPUT_HANDLE);
	INPUT_RECORD irInBuf[1];
	DWORD numRead = 0;

	(log << "# locking " << frequency << ", " << bandwidth << "\n").Show();
	do
	{
		hr = LockChannel(frequency, bandwidth, bLocked, bPresent, nStrength, nQuality);

		(log << "# signal locked = " << (bLocked ? "Y" : "N")
			 << " present = " << (bPresent ? "Y" : "N")
			 << " stength = " << nStrength
			 << " quality = " << nQuality << "\n").Show();

		if (::WaitForSingleObject(hInput, 500) == WAIT_OBJECT_0)
		{
			ReadConsoleInput(hInput, irInBuf, 1, &numRead);
			if ((numRead == 1) && (irInBuf[0].EventType == KEY_EVENT)) // make sure it's a key event, not a mouse or focus event
				bKeyboardEvent = TRUE;
		}

	} while (!bKeyboardEvent);

	DestroyGraph();

	return hr;
}

HRESULT BDAChannelScan::TestTSFileSink(long freq, long band, LPTSTR pFilename)
{
	HRESULT hr;

	if (FAILED(hr = SetupGraphForTSFileSink(freq, band, pFilename)))
		return hr;

	BOOL bKeyboardEvent = FALSE;
	HANDLE hInput = ::GetStdHandle(STD_INPUT_HANDLE);
	INPUT_RECORD irInBuf[1];
	DWORD numRead = 0;

	do
	{
		if (::WaitForSingleObject(hInput, 1000) == WAIT_OBJECT_0)
		{
			ReadConsoleInput(hInput, irInBuf, 1, &numRead);
 			if (numRead == 1)
			{
				if (irInBuf[0].EventType == KEY_EVENT) // make sure it's a key event, not a mouse or focus event
				{
					if ((irInBuf[0].Event.KeyEvent.bKeyDown == TRUE) &&
						(irInBuf[0].Event.KeyEvent.wVirtualKeyCode != 0))
						bKeyboardEvent = TRUE;
				}
			}
		}
	} while (!bKeyboardEvent);

	DestroyGraphForTSFileSink();

	return hr;
}

void BDAChannelScan::ToggleVerbose()
{
	m_bVerbose = !m_bVerbose;
	if (m_bVerbose)
	{
		m_consoleVerboseHandle = m_mpeg2parser.VerboseOutput()->AddCallback(&m_console);
		m_logfileVerboseHandle = m_mpeg2parser.VerboseOutput()->AddCallback(&lmw);
	}
	else
	{
		m_mpeg2parser.VerboseOutput()->RemoveCallback(m_consoleVerboseHandle);
		m_mpeg2parser.VerboseOutput()->RemoveCallback(m_logfileVerboseHandle);
	}
}

void BDAChannelScan::ToggleLockDetectionMode()
{
	m_nLockDetectionMode++;
	if (m_nLockDetectionMode >= LOCK_MODE_COUNT)
		m_nLockDetectionMode = 0;
}

HRESULT	BDAChannelScan::CreateGraph()
{
	HRESULT hr = S_OK;

	if(FAILED(hr = m_piGraphBuilder.CoCreateInstance(CLSID_FilterGraph)))
	{
		return (log << "Failed to create an instance of the graph builder\n").Show(hr);
	}
	if(FAILED(hr = m_piGraphBuilder->QueryInterface(IID_IMediaControl, reinterpret_cast<void **>(&m_piMediaControl))))
	{
		return (log << "Failed to get Media Control interface\n").Show(hr);
	}

	if(FAILED(hr = graphTools.AddToRot(m_piGraphBuilder, &m_rotEntry)))
	{
		return (log << "Failed adding to ROT\n").Show(hr);
	}
	
	return S_OK;
}


HRESULT	BDAChannelScan::BuildGraph()
{
	HRESULT hr = S_OK;

    CComBSTR bstrNetworkType;
    CLSID CLSIDNetworkType;
	LONG frequency = 0;
	LONG bandwidth = 0;

	// Initialise the tune request
	if (FAILED(hr = graphTools.InitDVBTTuningSpace(m_pTuningSpace)))
	{
		return (log << "Failed to initialise the Tune Request\n").Show(hr);
	}

	// Get the current Network Type clsid
    if (FAILED(hr = m_pTuningSpace->get_NetworkType(&bstrNetworkType)))
	{
		return (log << "Failed to get TuningSpace Network Type\n").Show(hr);
    }

	if (FAILED(hr = CLSIDFromString(bstrNetworkType, &CLSIDNetworkType)))
	{
		return (log << "Couldn't get CLSIDFromString\n").Show(hr);
	}

    // create the network provider based on the clsid obtained from the tuning space
	if (FAILED(hr = graphTools.AddFilter(m_piGraphBuilder, CLSIDNetworkType, &m_pBDANetworkProvider, L"Network Provider")))
	{
		return (log << "Failed to add Network Provider to the graph\n").Show(hr);
	}

	//Create TuneRequest
	CComPtr <ITuneRequest> pTuneRequest;
	if (FAILED(hr = graphTools.CreateDVBTTuneRequest(m_pTuningSpace, pTuneRequest, frequency, bandwidth)))
	{
		return (log << "Failed to create the Tune Request.\n").Show(hr);
	}

	//Apply TuneRequest
	if (FAILED(hr = m_pBDANetworkProvider->QueryInterface(__uuidof(IScanningTuner), reinterpret_cast<void **>(&m_piTuner))))
	{
		return (log << "Failed while interfacing Tuner with Network Provider\n").Show(hr);
	}
	if (FAILED(m_piTuner->put_TuningSpace(m_pTuningSpace)))
	{
		return (log << "Failed to give tuning space to tuner.\n").Show(hr);
	}
	if (FAILED(hr = m_piTuner->put_TuneRequest(pTuneRequest)))
	{
		return (log << "Failed to submit the Tune Tequest to the Network Provider\n").Show(hr);
	}

	//We can now add the rest of the source filters

	if (FAILED(hr = m_pBDACard->AddFilters(m_piGraphBuilder)))
	{
		return hr;
	}
	
	if (FAILED(hr = graphTools.AddFilter(m_piGraphBuilder, CLSID_MPEG2Demultiplexer, &m_pBDAMpeg2Demux, L"BDA MPEG-2 Demultiplexer")))
	{
		return (log << "Failed to add BDA MPEG-2 Demultiplexer to the graph\n").Show(hr);
	}

	if (FAILED(hr = graphTools.AddFilterByName(m_piGraphBuilder, &m_pBDATIF, KSCATEGORY_BDA_TRANSPORT_INFORMATION, L"BDA MPEG2 Transport Information Filter")))
	{
		return (log << "Cannot load TIF\n").Show(hr);
	}

	if (FAILED(hr = graphTools.AddFilterByName(m_piGraphBuilder, &m_pBDASecTab, KSCATEGORY_BDA_TRANSPORT_INFORMATION, L"MPEG-2 Sections and Tables")))
	{
		return (log << "Cannot load MPEG-2 Sections and Tables\n").Show(hr);
	}

	return S_OK;
}


HRESULT	BDAChannelScan::ConnectGraph()
{
	HRESULT hr = S_OK;

	m_pBDACard->Connect(m_pBDANetworkProvider);

	CComPtr <IPin> pCapturePin;
	m_pBDACard->GetCapturePin(&pCapturePin.p);

	CComPtr <IPin> pDemuxPin;
	if (FAILED(hr = graphTools.FindPin(m_pBDAMpeg2Demux, L"MPEG-2 Stream", &pDemuxPin.p, REQUESTED_PINDIR_INPUT)))
	{
		return (log << "Failed to get input pin on Demux\n").Show(hr);
	}
	
	if (FAILED(hr = m_piGraphBuilder->ConnectDirect(pCapturePin, pDemuxPin, NULL)))
	{
		return (log << "Failed to connect Capture filter to BDA Demux\n").Show(hr);
	}

	if (FAILED(hr = graphTools.ConnectFilters(m_piGraphBuilder, m_pBDAMpeg2Demux, m_pBDATIF)))
	{
		return (log << "Failed to connect to BDA Demux to TIF\n").Show(hr);
	}

	if (FAILED(hr = graphTools.ConnectFilters(m_piGraphBuilder, m_pBDAMpeg2Demux, m_pBDASecTab)))
	{
		return (log << "Failed to connect BDA Demux to MPEG-2 Sections and Tables\n").Show(hr);
	}

	return hr;
}

HRESULT	BDAChannelScan::BuildTSFileSinkGraph(long freq, long band, LPTSTR pFilename)
{
	HRESULT hr = S_OK;

    CComBSTR bstrNetworkType;
    CLSID CLSIDNetworkType;
	LONG frequency = freq;
	LONG bandwidth = band;

	// Initialise the tune request
	if (FAILED(hr = graphTools.InitDVBTTuningSpace(m_pTuningSpace)))
	{
		return (log << "Failed to initialise the Tune Request\n").Show(hr);
	}

	// Get the current Network Type clsid
    if (FAILED(hr = m_pTuningSpace->get_NetworkType(&bstrNetworkType)))
	{
		return (log << "Failed to get TuningSpace Network Type\n").Show(hr);
    }

	if (FAILED(hr = CLSIDFromString(bstrNetworkType, &CLSIDNetworkType)))
	{
		return (log << "Couldn't get CLSIDFromString\n").Show(hr);
	}

    // create the network provider based on the clsid obtained from the tuning space
	if (FAILED(hr = graphTools.AddFilter(m_piGraphBuilder, CLSIDNetworkType, &m_pBDANetworkProvider, L"Network Provider")))
	{
		return (log << "Failed to add Network Provider to the graph\n").Show(hr);
	}

	//Create TuneRequest
	CComPtr <ITuneRequest> pTuneRequest;
	if (FAILED(hr = graphTools.CreateDVBTTuneRequest(m_pTuningSpace, pTuneRequest, frequency, bandwidth)))
	{
		return (log << "Failed to create the Tune Request.\n").Show(hr);
	}

	//Apply TuneRequest
	if (FAILED(hr = m_pBDANetworkProvider->QueryInterface(__uuidof(IScanningTuner), reinterpret_cast<void **>(&m_piTuner))))
	{
		return (log << "Failed while interfacing Tuner with Network Provider\n").Show(hr);
	}
	if (FAILED(m_piTuner->put_TuningSpace(m_pTuningSpace)))
	{
		return (log << "Failed to give tuning space to tuner.\n").Show(hr);
	}
	if (FAILED(hr = m_piTuner->put_TuneRequest(pTuneRequest)))
	{
		return (log << "Failed to submit the Tune Tequest to the Network Provider\n").Show(hr);
	}

	//We can now add the rest of the source filters

	if (FAILED(hr = m_pBDACard->AddFilters(m_piGraphBuilder)))
	{
		return hr;
	}
	
	if (FAILED(hr = graphTools.AddFilter(m_piGraphBuilder, CLSID_TSFileSink, &m_pTSFileSink, L"TSFileSink")))
	{
		return (log << "Cannot load TSFileSink filter\n").Show(hr);
	}

	CComQIPtr<IFileSinkFilter> piFileSinkFilter(m_pTSFileSink);
	if (!piFileSinkFilter)
		return (log << "Cannot QI sink filter for IFileSinkFilter interface\n").Show(hr);

	USES_CONVERSION;

	if (FAILED(hr = piFileSinkFilter->SetFileName(T2W(pFilename), NULL)))
	{
		return (log << "Cannot set TSFileSink filename\n").Show(hr);
	}

	return S_OK;
}

HRESULT	BDAChannelScan::ConnectTSFileSinkGraph()
{
	HRESULT hr = S_OK;

	m_pBDACard->Connect(m_pBDANetworkProvider);

	CComPtr <IPin> pCapturePin;
	m_pBDACard->GetCapturePin(&pCapturePin.p);

	CComPtr <IPin> pSinkPin;
	if (FAILED(hr = graphTools.FindPin(m_pTSFileSink, L"In", &pSinkPin.p, REQUESTED_PINDIR_INPUT)))
	{
		return (log << "Failed to get input pin on TSFileSink\n").Show(hr);
	}
	
	if (FAILED(hr = m_piGraphBuilder->ConnectDirect(pCapturePin, pSinkPin, NULL)))
	{
		return (log << "Failed to connect Capture filter to TSFileSink\n").Show(hr);
	}

	return hr;
}


HRESULT	BDAChannelScan::LockChannel(long lFrequency, long lBandwidth, BOOL &locked, BOOL &present, long &strength, long &quality)
{
	HRESULT hr = S_OK;
	CComPtr <ITuneRequest> piTuneRequest;

	if (SUCCEEDED(hr = graphTools.CreateDVBTTuneRequest(m_pTuningSpace, piTuneRequest, lFrequency, lBandwidth)))
	{
		CComQIPtr <ITuner> pTuner(m_pBDANetworkProvider);
		if (!pTuner)
		{
			return (log << "Failed while interfacing Tuner with Network Provider\n").Show(E_FAIL);
		}
		if (FAILED(hr = pTuner->put_TuneRequest(piTuneRequest)))
		{
			return (log << "Failed to submit the Tune Tequest to the Network Provider\n").Show(hr);
		}

		pTuner.Release();
		piTuneRequest.Release();

		//Try testing the signal stats 10 times during a second to allow time for the tuner to lock.
		for (int tests = 0; tests < 10 ; tests++ )
		{
			//Check that channel locked ok.
			if (FAILED(hr = GetSignalStatistics(locked, present, strength, quality)))
				return hr;

			(verbose << "# signal locked = " << (locked ? "Y" : "N")
					 << " present = " << (present ? "Y" : "N")
					 << " stength = " << strength
					 << " quality = " << quality << "\n").Show();
			
			//if ((locked>0) || (present>0) || (quality>0))
			switch (m_nLockDetectionMode)
			{
			case LOCK_MODE_QUALITY:
				if (quality>0)
					return S_OK;
				break;

			case LOCK_MODE_LOCKED:
			default:
				if (locked>0)
					return S_OK;
				break;
			};

			Sleep(100);
		}
		return S_FALSE;
	}

	return hr;
}

HRESULT BDAChannelScan::GetSignalStatistics(BOOL &locked, BOOL &present, long &strength, long &quality)
{
	return m_pBDACard->GetSignalStatistics(locked, present, strength, quality);
}

HRESULT BDAChannelScan::createConnectionPoint()
{
	HRESULT hr;

	/*if (FAILED(hr = m_pBDATIF->QueryInterface(__uuidof(IGuideData), reinterpret_cast<void **>(&m_piGuideData))))
	{
		return (log << "Failed to interface GuideData with TIF\n").Show(hr);
	}*/

	CComPtr <IConnectionPointContainer> piContainer;
	if (FAILED(hr = m_pBDATIF->QueryInterface(__uuidof(IConnectionPointContainer), reinterpret_cast<void **>(&piContainer))))
	{
		return (log << "Failed to interface ConnectionPoint with TIF\n").Show(hr);
	}
	if (FAILED(hr = piContainer->FindConnectionPoint(__uuidof(IGuideDataEvent), &m_piConnectionPoint)))
	{
		return (log << "Failed to find connection point on TIF\n").Show(hr);
	}

	if (FAILED(hr = m_piConnectionPoint->Advise(this, &m_dwAdviseCookie)))
	{
		m_piConnectionPoint.Release();
		return (log << "Failed to Advice connection point of TIF\n").Show(hr);
	}
	
	return S_OK;
}

STDMETHODIMP_(ULONG) BDAChannelScan::AddRef(void)
{
	return ++m_ulRef; // NOT thread-safe
}

STDMETHODIMP_(ULONG) BDAChannelScan::Release(void)
{
	--m_ulRef; // NOT thread-safe
	if (m_ulRef == 0)
	{
		delete this;
		return 0; // Can't return the member of a deleted object.
	}
	else
	{
		return m_ulRef;
	}
}

STDMETHODIMP BDAChannelScan::QueryInterface(REFIID iid, void **ppv)
{
	*ppv = NULL;
	if (iid == IID_IUnknown || iid == IID_IGuideDataEvent)
	{
		*ppv = static_cast<IGuideDataEvent *>(this);
	}

	if (*ppv)
	{
		AddRef();
		return S_OK;
	}
	else
		return E_NOINTERFACE;
}

STDMETHODIMP BDAChannelScan::ServiceChanged(VARIANT varServiceDescriptionID)
{
	DWORD dwWaitResult; 
    dwWaitResult = WaitForSingleObject(m_hServiceChangedMutex, 1000);
	if (dwWaitResult == WAIT_OBJECT_0)
	{
		try
		{
			if (m_bScanning)
				m_mpeg2parser.StartMpeg2DataScan();
		}
		catch(...)
		{
		} 
		ReleaseMutex(m_hServiceChangedMutex);
	}

	return S_OK;
}

STDMETHODIMP BDAChannelScan::ProgramChanged(VARIANT varProgramDescriptionID)
{
	return S_OK;
}
STDMETHODIMP BDAChannelScan::ScheduleEntryChanged(VARIANT varScheduleEntryDescriptionID)
{
	return S_OK;
}
STDMETHODIMP BDAChannelScan::GuideDataAcquired()
{
	return S_OK;
}
STDMETHODIMP BDAChannelScan::ProgramDeleted(VARIANT stVariant)
{
	return S_OK;
}
STDMETHODIMP BDAChannelScan::ServiceDeleted(VARIANT stVariant)
{
	return S_OK;
}
STDMETHODIMP BDAChannelScan::ScheduleDeleted(VARIANT stVariant)
{
	return S_OK;
}

BOOL BDAChannelScan::StartGraph()
{
	m_piMediaControl->Run();

	return TRUE;
}

BOOL BDAChannelScan::StopGraph()
{
	if (m_piMediaControl)
		m_piMediaControl->Stop();

	return TRUE;
}

HRESULT BDAChannelScan::scanChannel(long channelNumber, long frequency, long bandwidth)
{
	ResetEvent(m_hGuideDataChangedEvent);
	m_mpeg2parser.Reset();
	m_mpeg2parser.SetFilter(m_pBDASecTab);
	m_mpeg2parser.SetNetworkNumber(channelNumber);
	
	m_bScanning = TRUE;

	BOOL bLocked = FALSE;
	BOOL bPresent = FALSE;
	long nStrength = 0;
	long nQuality = 0;

	HRESULT hr = LockChannel(frequency, bandwidth, bLocked, bPresent, nStrength, nQuality);

	switch (hr)
	{
		case S_OK:
			(log << "# locked " << frequency << ", " << bandwidth
				<< " signal locked = " << (bLocked ? "Y" : "N")
				<< " present = " << (bPresent ? "Y" : "N")
				<< " stength = " << nStrength
				<< " quality = " << nQuality << "\n").Show();

			switch (m_mpeg2parser.WaitForScanToFinish())
			{
			case WAIT_OBJECT_0:
				m_mpeg2parser.PrintDigitalWatchChannelsIni();
				break;
			case WAIT_TIMEOUT:
				printf("# scan timed out\n");
				break;
			default:
				printf("# scan failed\n");
				break;
			};
			break;

		default:
			break;
	}

	m_mpeg2parser.ReleaseFilter();

	m_bScanning = FALSE;
	return hr;
}

