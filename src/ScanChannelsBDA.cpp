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
#include "FilterGraphTools.h"
#include "LogMessage.h"

#include <ks.h> // Must be included before ksmedia.h
#include <ksmedia.h> // Must be included before bdamedia.h
#include <bdamedia.h>
#include <stdio.h>

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

	m_consoleHandle = m_mpeg2parser.Output()->AddCallback(&m_console);
}

BDAChannelScan::~BDAChannelScan()
{
	m_mpeg2parser.Output()->RemoveCallback(m_consoleHandle);
	CoUninitialize();
}

HRESULT BDAChannelScan::selectCard()
{
	cardList.LoadCards();

	int cardsFound = cardList.cards.size();
	if (cardsFound == 0)
	{
		return (g_log << "Could not find any BDA devices.\n").Write(E_FAIL);
	}

	BDACard *bdaCard = NULL;

	if (cardsFound == 1)
	{
		m_pBDACard = *(cardList.cards.begin());
		return (g_log << "Found BDA device: " << m_pBDACard->tunerDevice.strFriendlyName << "\n").Write(S_OK);
	}

	(g_log << cardsFound << " BDA devices found.\n").Show();

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
			return (g_log << "Exit chosen\n").Write(E_FAIL);
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
				return (g_log << "using " << m_pBDACard->tunerDevice.strFriendlyName << "\n").Write(S_OK);
			}
			id++;
		}
		(g_log << "Weird error. Could not find selected device in list\n").Write();
	}

	return E_FAIL;
}

void BDAChannelScan::AddNetwork(long freq, long band)
{
	if (m_Count < 256)
	{
		(g_log << "Adding network " << freq << ", " << band << "\n").Write();
		m_freq[m_Count] = freq;
		m_band[m_Count] = band;
		m_Count++;
	}
	else
	{
		(g_log << "Cannot add more than 256 networks\n").Write();
	}
}

HRESULT BDAChannelScan::SetupGraph()
{
	HRESULT hr = S_OK;

	if (FAILED(hr = CreateGraph()))
	{
		return (g_log << "Failed to create graph.\n").Show(hr);
	}
	
	if (FAILED(hr = BuildGraph()))
	{
		return (g_log << "Failed to add filters to graph.\n").Show(hr);
	}

	if (FAILED(hr = ConnectGraph()))
	{
		return (g_log << "Failed to connect filters in graph.\n").Show(hr);
	}

	if (FAILED(hr = createConnectionPoint()))
	{
		return (g_log << "Failed to add connection point to TIF.\n").Show(hr);
	}

	if (StartGraph() == FALSE)
	{
		return (g_log << "Failed to start the graph\n").Show(hr);
	}

	return hr;
}

void BDAChannelScan::DestroyGraph()
{
	StopGraph();

	if (m_piConnectionPoint)
		m_piConnectionPoint->Unadvise(m_dwAdviseCookie); 

	m_piConnectionPoint.Release();
	m_piGuideData.Release();

	DisconnectAllPins(m_piGraphBuilder);

	m_pTuningSpace.Release();
	m_piTuner.Release();
	RemoveAllFilters(m_piGraphBuilder);

	//m_pBDANetworkProvider.Release();	//causes an exception
	m_pBDATuner.Release();
	m_pBDADemod.Release();
	m_pBDACapture.Release();
	m_pBDAMpeg2Demux.Release();
	m_pBDATIF.Release();
	m_pBDASecTab.Release();


	if (m_rotEntry)
		RemoveFromRot(m_rotEntry);
	m_piMediaControl.Release();
	m_piGraphBuilder.Release();

}

HRESULT BDAChannelScan::scanNetworks()
{
	HRESULT hr = S_OK;

	(g_log << "Scanning Networks\n").Write();

	if (FAILED(hr = SetupGraph()))
		return hr;

	for (int count=0 ; count<m_Count ; count++ )
	{
		hr = scanChannel(count+1, m_freq[count], m_band[count]);
		if (hr == S_FALSE)
			(g_log << "# Nothing found on " << m_freq[count] << "kHz frequency, moving on.\n").Show();
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
			return (g_log << "Error locking channel. Aborting\n").Show(E_FAIL);
		}
		if (hr == S_OK)
			continue;

		(g_log << "# Nothing found on " << ulFrequencyArray[count] << "kHz, trying +125").Show();

		hr = scanChannel(chNum, ulFrequencyArray[count] + 125, 7);
		if (hr == E_FAIL)
		{
			return (g_log << "\nError locking channel. Aborting\n").Show(E_FAIL);
		}
		if (hr == S_OK)
			continue;

		(g_log << ", trying -125").Show();

		hr = scanChannel(chNum, ulFrequencyArray[count] - 125, 7);
		if (hr == E_FAIL)
		{
			return (g_log << "\nError locking channel. Aborting\n").Show(E_FAIL);
		}
		if (hr == S_OK)
			continue;

		(g_log << ", Nothing found\n").Show();

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

	do
	{
		hr = LockChannel(frequency, bandwidth, bLocked, bPresent, nStrength, nQuality);

		switch (hr)
		{
			case S_OK:
				(g_log << "# locked " << frequency << ", " << bandwidth
					<< " signal locked = " << (bLocked ? "Y" : "N")
					<< " present = " << (bPresent ? "Y" : "N")
					<< " stength = " << nStrength
					<< " quality = " << nQuality << "\n").Show();
				break;

			default:
				(g_log << "# no lock " << frequency << ", " << bandwidth
					<< " signal locked = " << (bLocked ? "Y" : "N")
					<< " present = " << (bPresent ? "Y" : "N")
					<< " stength = " << nStrength
					<< " quality = " << nQuality << "\n").Show();
				break;
		}

		if (::WaitForSingleObject(::GetStdHandle(STD_INPUT_HANDLE), 100) == WAIT_OBJECT_0)
			bKeyboardEvent = TRUE;

	} while (!bKeyboardEvent);

	DestroyGraph();

	return hr;
}


void BDAChannelScan::ToggleVerbose()
{
	m_bVerbose = !m_bVerbose;
	if (m_bVerbose)
	{
		m_consoleVerboseHandle = m_mpeg2parser.VerboseOutput()->AddCallback(&m_console);
	}
	else
	{
		m_mpeg2parser.VerboseOutput()->RemoveCallback(m_consoleVerboseHandle);
	}
}

HRESULT	BDAChannelScan::CreateGraph()
{
	HRESULT hr = S_OK;

	if(FAILED(hr = m_piGraphBuilder.CoCreateInstance(CLSID_FilterGraph)))
	{
		return (g_log << "Failed to create an instance of the graph builder\n").Show(hr);
	}
	if(FAILED(hr = m_piGraphBuilder->QueryInterface(IID_IMediaControl, reinterpret_cast<void **>(&m_piMediaControl))))
	{
		return (g_log << "Failed to get Media Control interface\n").Show(hr);
	}

	if(FAILED(hr = AddToRot(m_piGraphBuilder, &m_rotEntry)))
	{
		return (g_log << "Failed adding to ROT\n").Show(hr);
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
	if (FAILED(hr = InitDVBTTuningSpace(m_pTuningSpace)))
	{
		return (g_log << "Failed to initialise the Tune Request\n").Show(hr);
	}

	// Get the current Network Type clsid
    if (FAILED(hr = m_pTuningSpace->get_NetworkType(&bstrNetworkType)))
	{
		return (g_log << "Failed to get TuningSpace Network Type\n").Show(hr);
    }

	if (FAILED(hr = CLSIDFromString(bstrNetworkType, &CLSIDNetworkType)))
	{
		return (g_log << "Couldn't get CLSIDFromString\n").Show(hr);
	}

    // create the network provider based on the clsid obtained from the tuning space
	if (FAILED(hr = AddFilter(m_piGraphBuilder, CLSIDNetworkType, m_pBDANetworkProvider.p, L"Network Provider")))
	{
		return (g_log << "Failed to add Network Provider to the graph\n").Show(hr);
	}

	//Create TuneRequest
	CComPtr <ITuneRequest> pTuneRequest;
	if (FAILED(hr = SubmitDVBTTuneRequest(m_pTuningSpace, pTuneRequest, frequency, bandwidth)))
	{
		return (g_log << "Failed to create the Tune Request.\n").Show(hr);
	}

	//Apply TuneRequest
	if (FAILED(hr = m_pBDANetworkProvider->QueryInterface(__uuidof(IScanningTuner), reinterpret_cast<void **>(&m_piTuner))))
	{
		return (g_log << "Failed while interfacing Tuner with Network Provider\n").Show(hr);
	}
	if (FAILED(m_piTuner->put_TuningSpace(m_pTuningSpace)))
	{
		return (g_log << "Failed to give tuning space to tuner.\n").Show(hr);
	}
	if (FAILED(hr = m_piTuner->put_TuneRequest(pTuneRequest)))
	{
		return (g_log << "Failed to submit the Tune Tequest to the Network Provider\n").Show(hr);
	}

	//We can now add the rest of the source filters
	
	if (FAILED(hr = AddFilterByDevicePath(m_piGraphBuilder, m_pBDATuner.p, m_pBDACard->tunerDevice.strDevicePath, m_pBDACard->tunerDevice.strFriendlyName)))
	{
		return (g_log << "Cannot load Tuner Device\n").Show(hr);
	}

	if (m_pBDACard->demodDevice.bValid)
	{
		if (FAILED(hr = AddFilterByDevicePath(m_piGraphBuilder, m_pBDADemod.p, m_pBDACard->demodDevice.strDevicePath, m_pBDACard->demodDevice.strFriendlyName)))
		{
			return (g_log << "Cannot load Demod Device\n").Show(hr);
		}
	}

	if (m_pBDACard->captureDevice.bValid)
	{
		if (FAILED(hr = AddFilterByDevicePath(m_piGraphBuilder, m_pBDACapture.p, m_pBDACard->captureDevice.strDevicePath, m_pBDACard->captureDevice.strFriendlyName)))
		{
			return (g_log << "Cannot load Capture Device\n").Show(hr);
		}
	}

	if (FAILED(hr = AddFilter(m_piGraphBuilder, CLSID_MPEG2Demultiplexer, m_pBDAMpeg2Demux.p, L"BDA MPEG-2 Demultiplexer")))
	{
		return (g_log << "Failed to add BDA MPEG-2 Demultiplexer to the graph\n").Show(hr);
	}

	if (FAILED(hr = AddFilterByName(m_piGraphBuilder, m_pBDATIF.p, KSCATEGORY_BDA_TRANSPORT_INFORMATION, L"BDA MPEG2 Transport Information Filter")))
	{
		return (g_log << "Cannot load TIF\n").Show(hr);
	}

	if (FAILED(hr = AddFilterByName(m_piGraphBuilder, m_pBDASecTab.p, KSCATEGORY_BDA_TRANSPORT_INFORMATION, L"MPEG-2 Sections and Tables")))
	{
		return (g_log << "Cannot load MPEG-2 Sections and Tables\n").Show(hr);
	}

	return S_OK;
}


HRESULT	BDAChannelScan::ConnectGraph()
{
	HRESULT hr = S_OK;

    if (FAILED(hr = ConnectFilters(m_piGraphBuilder, m_pBDANetworkProvider, m_pBDATuner)))
	{
		return (g_log << "Failed to connect Network Provider to Tuner Filter\n").Show(hr);
	}

	CComPtr <IPin> pCapturePin;

	if (m_pBDACard->captureDevice.bValid)
	{
		if (m_pBDACard->demodDevice.bValid)
		{
			if (FAILED(hr = ConnectFilters(m_piGraphBuilder, m_pBDATuner, m_pBDADemod)))
			{
				return (g_log << "Failed to connect Tuner Filter to Demod Filter\n").Show(hr);
			}

			if (FAILED(hr = ConnectFilters(m_piGraphBuilder, m_pBDADemod, m_pBDACapture)))
			{
				return (g_log << "Failed to connect Demod Filter to Capture Filter\n").Show(hr);
			}

		}
		else
		{
			if (FAILED(hr = ConnectFilters(m_piGraphBuilder, m_pBDATuner, m_pBDACapture)))
			{
				return (g_log << "Failed to connect Tuner Filter to Capture Filter\n").Show(hr);
			}
		}
		
		if (FAILED(hr = FindPinByMediaType(m_pBDACapture, MEDIATYPE_Stream, KSDATAFORMAT_SUBTYPE_BDA_MPEG2_TRANSPORT, &pCapturePin.p, REQUESTED_PINDIR_OUTPUT)))
		{
			//If that failed then try the other mpeg2 type, but this shouldn't happen.
			if (FAILED(hr = FindPinByMediaType(m_pBDACapture, MEDIATYPE_Stream, MEDIASUBTYPE_MPEG2_TRANSPORT, &pCapturePin.p, REQUESTED_PINDIR_OUTPUT)))
			{
				return (g_log << "Failed to find Tranport Stream pin on Capture filter\n").Show(hr);
			}
		}
	}
	else if (m_pBDACard->demodDevice.bValid)
	{
		if (FAILED(hr = ConnectFilters(m_piGraphBuilder, m_pBDATuner, m_pBDADemod)))
		{
			return (g_log << "Failed to connect Tuner Filter to Demod Filter\n").Show(hr);
		}

		if (FAILED(hr = FindPinByMediaType(m_pBDADemod, MEDIATYPE_Stream, KSDATAFORMAT_SUBTYPE_BDA_MPEG2_TRANSPORT, &pCapturePin.p, REQUESTED_PINDIR_OUTPUT)))
		{
			//If that failed then try the other mpeg2 type, but this shouldn't happen.
			if (FAILED(hr = FindPinByMediaType(m_pBDADemod, MEDIATYPE_Stream, MEDIASUBTYPE_MPEG2_TRANSPORT, &pCapturePin.p, REQUESTED_PINDIR_OUTPUT)))
			{
				return (g_log << "Failed to find Tranport Stream pin on Capture filter\n").Show(hr);
			}
		}
	}
	else
	{
		if (FAILED(hr = FindPinByMediaType(m_pBDATuner, MEDIATYPE_Stream, KSDATAFORMAT_SUBTYPE_BDA_MPEG2_TRANSPORT, &pCapturePin.p, REQUESTED_PINDIR_OUTPUT)))
		{
			//If that failed then try the other mpeg2 type, but this shouldn't happen.
			if (FAILED(hr = FindPinByMediaType(m_pBDATuner, MEDIATYPE_Stream, MEDIASUBTYPE_MPEG2_TRANSPORT, &pCapturePin.p, REQUESTED_PINDIR_OUTPUT)))
			{
				return (g_log << "Failed to find Tranport Stream pin on Capture filter\n").Show(hr);
			}
		}
	}

	CComPtr <IPin> pDemuxPin;
	if (FAILED(hr = FindPin(m_pBDAMpeg2Demux, L"MPEG-2 Stream", &pDemuxPin.p, REQUESTED_PINDIR_INPUT)))
	{
		return (g_log << "Failed to get input pin on Demux\n").Show(hr);
	}
	
	if (FAILED(hr = m_piGraphBuilder->ConnectDirect(pCapturePin, pDemuxPin, NULL)))
	{
		return (g_log << "Failed to connect Capture filter to BDA Demux\n").Show(hr);
	}

	if (FAILED(hr = ConnectFilters(m_piGraphBuilder, m_pBDAMpeg2Demux, m_pBDATIF)))
	{
		return (g_log << "Failed to connect to BDA Demux to TIF\n").Show(hr);
	}

	if (FAILED(hr = ConnectFilters(m_piGraphBuilder, m_pBDAMpeg2Demux, m_pBDASecTab)))
	{
		return (g_log << "Failed to connect BDA Demux to MPEG-2 Sections and Tables\n").Show(hr);
	}

	return hr;
}

HRESULT	BDAChannelScan::LockChannel(long lFrequency, long lBandwidth, BOOL &locked, BOOL &present, long &strength, long &quality)
{
	HRESULT hr = S_OK;
	CComPtr <ITuneRequest> piTuneRequest;

	if (SUCCEEDED(hr = SubmitDVBTTuneRequest(m_pTuningSpace, piTuneRequest, lFrequency, lBandwidth)))
	{
		CComQIPtr <ITuner> pTuner(m_pBDANetworkProvider);
		if (!pTuner)
		{
			return (g_log << "Failed while interfacing Tuner with Network Provider\n").Show(E_FAIL);
		}
		if (FAILED(hr = pTuner->put_TuneRequest(piTuneRequest)))
		{
			return (g_log << "Failed to submit the Tune Tequest to the Network Provider\n").Show(hr);
		}

		pTuner.Release();
		piTuneRequest.Release();


		//Check that channel locked ok.

		//Get IID_IBDA_Topology
		CComPtr <IBDA_Topology> bdaNetTop;
		if (FAILED(hr = m_pBDATuner.QueryInterface(&bdaNetTop)))
		{
			return (g_log << "Cannot Find IID_IBDA_Topology\n").Show(hr);
		}

		ULONG NodeTypes;
		ULONG NodeType[32];
		ULONG Interfaces;
		GUID Interface[32];
		CComPtr <IUnknown> iNode;

		long longVal;
		longVal = strength = quality = 0;
		BYTE byteVal;
		byteVal = locked = present = 0;

		if (FAILED(hr = bdaNetTop->GetNodeTypes(&NodeTypes, 32, NodeType)))
		{
			return (g_log << "Cannot get node types\n").Show(hr);
		}

		for ( int i=0 ; i<NodeTypes ; i++ )
		{
			hr = bdaNetTop->GetNodeInterfaces(NodeType[i], &Interfaces, 32, Interface);
			if (hr == S_OK)
			{
				for ( int j=0 ; j<Interfaces ; j++ )
				{
					if (Interface[j] == IID_IBDA_SignalStatistics)
					{
						hr = bdaNetTop->GetControlNode(0, 1, NodeType[i], &iNode);
						if (hr == S_OK)
						{
							CComPtr <IBDA_SignalStatistics> pSigStats;

							hr = iNode.QueryInterface(&pSigStats);
							if (hr == S_OK)
							{
								if (SUCCEEDED(hr = pSigStats->get_SignalStrength(&longVal)))
									strength = longVal;

								if (SUCCEEDED(hr = pSigStats->get_SignalQuality(&longVal)))
									quality = longVal;

								if (SUCCEEDED(hr = pSigStats->get_SignalLocked(&byteVal)))
									locked = byteVal;

								if (SUCCEEDED(hr = pSigStats->get_SignalPresent(&byteVal)))
									present = byteVal;

								pSigStats.Release();
							}
							iNode.Release();
						}
						break;
					}
				}
			}
		}

		bdaNetTop.Release();
		
		if ((locked>0) || (present>0))
		{
			return S_OK;
		}
		return E_FAIL;
	}

	return hr;
}

HRESULT BDAChannelScan::createConnectionPoint()
{
	HRESULT hr;

	if (FAILED(hr = m_pBDATIF->QueryInterface(__uuidof(IGuideData), reinterpret_cast<void **>(&m_piGuideData))))
	{
		return (g_log << "Failed to interface GuideData with TIF\n").Show(hr);
	}

	CComPtr <IConnectionPointContainer> piContainer;
	if (FAILED(hr = m_pBDATIF->QueryInterface(__uuidof(IConnectionPointContainer), reinterpret_cast<void **>(&piContainer))))
	{
		return (g_log << "Failed to interface ConnectionPoint with TIF\n").Show(hr);
	}
	if (FAILED(hr = piContainer->FindConnectionPoint(__uuidof(IGuideDataEvent), &m_piConnectionPoint)))
	{
		return (g_log << "Failed to find connection point on TIF\n").Show(hr);
	}

	if (FAILED(hr = m_piConnectionPoint->Advise(this, &m_dwAdviseCookie)))
	{
		m_piConnectionPoint.Release();
		return (g_log << "Failed to Advice connection point of TIF\n").Show(hr);
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
			(g_log << "# locked " << frequency << ", " << bandwidth
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

