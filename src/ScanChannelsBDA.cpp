/**
 *	ScanChannelsBDA.cpp
 *	Copyright (C) 2004 BionicDonkey
 *  Copyright (C) 2004 Nate
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

	m_Count = 0;
	m_bScanning = FALSE;
}

BDAChannelScan::~BDAChannelScan()
{
	RemoveFromRot(m_rotEntry);
	m_piConnectionPoint->Unadvise(m_dwAdviseCookie); 

	m_pBDANetworkProvider.Release();
	m_pBDATuner.Release();
	m_pBDADemod.Release();
	m_pBDACapture.Release();
	m_pBDAMpeg2Demux.Release();
	m_pBDATIF.Release();
	m_pBDASecTab.Release();
	
	m_pTuningSpace.Release();
	m_piGuideData.Release();
	m_piConnectionPoint.Release();
	m_piGraphBuilder.Release();
	m_piMediaControl.Release();
	m_piTuner.Release();

	CoUninitialize();
}

HRESULT BDAChannelScan::selectCard()
{
	cardList.LoadCards();

	int cardsFound = cardList.cards.size();
	if (cardsFound == 0)
	{
		cout << "Could not find any BDA devices." << endl;
		return E_FAIL;
	}

	BDACard *bdaCard = NULL;

	if (cardsFound == 1)
	{
		m_pBDACard = *(cardList.cards.begin());
		printf("Found BDA device: %S", m_pBDACard->tunerDevice.strFriendlyName);
		return S_OK;
	}

	cout << cardsFound << " BDA devices found." << endl;

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
			//cout << id << ". " << bdaCard->tunerDevice.strFriendlyName << endl;
			printf("%i. %S\n", id, bdaCard->tunerDevice.strFriendlyName);
			id++;
		}
		cout << id << ". Exit" << endl;
		cout << "> ";
		cin >> cardID;

		//Did Exit get chosen?
		if (cardID-1 == cardsFound)
			return S_FALSE;

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
				return S_OK;
			}
			id++;
		}
		cout << "Weird error. Could not find selected device in list" << endl;
	}

	return S_FALSE;
}

void BDAChannelScan::AddNetwork(long freq, long band)
{
	if (m_Count < 256)
	{
		m_freq[m_Count] = freq;
		m_band[m_Count] = band;
		m_Count++;
	}
}

HRESULT BDAChannelScan::scanNetworks()
{
	HRESULT hr = S_OK;

	for (int count=0 ; count<m_Count ; count++ )
	{
		hr = scanChannel(count+1, m_freq[count], m_band[count]);
		if (hr == S_FALSE)
			cout << "# Nothing found on " << m_freq[count] << "kHz frequency, moving on." << endl;
		if (hr == E_FAIL)
			return 1;
	}

	return hr;
}

HRESULT BDAChannelScan::scanAll()
{
	HRESULT hr = S_OK;

	static ULONG ulFrequencyArray[50] =
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

	int chNum = 0;
	for (int count=0 ; count<50 ; count++ )
	{
		chNum++;
		hr = scanChannel(chNum, ulFrequencyArray[count], 7);
		if (hr == E_FAIL)
		{
			cout << "Error locking channel. Aborting" << endl;
			return E_FAIL;
		}
		if (hr == S_OK)
			continue;

		cout << "# Nothing found on " << ulFrequencyArray[count] << "kHz, trying +125";

		hr = scanChannel(chNum, ulFrequencyArray[count] + 125, 7);
		if (hr == E_FAIL)
		{
			cout << "Error locking channel. Aborting" << endl;
			return E_FAIL;
		}
		if (hr == S_OK)
			continue;

		cout << ", trying -125";

		hr = scanChannel(chNum, ulFrequencyArray[count] - 125, 7);
		if (hr == E_FAIL)
		{
			cout << "Error locking channel. Aborting" << endl;
			return E_FAIL;
		}
		if (hr == S_OK)
			continue;

		cout << ", Nothing found" << endl;

		chNum--;
	}


	return hr;
}


HRESULT	BDAChannelScan::CreateGraph()
{
	HRESULT hr = S_OK;

	if(FAILED(hr = m_piGraphBuilder.CoCreateInstance(CLSID_FilterGraph)))
	{
		cout << "Failed to create an instance of the graph builder" << endl;
		return 1;
	}
	if(FAILED(hr = m_piGraphBuilder->QueryInterface(IID_IMediaControl, reinterpret_cast<void **>(&m_piMediaControl))))
	{
		cout << "Failed to get Media Control interface" << endl;
		return 1;
	}

	if(FAILED(hr = AddToRot(m_piGraphBuilder, &m_rotEntry)))
	{
		cout << "Failed adding to ROT" << endl;
	}
	
	return hr;
}


HRESULT	BDAChannelScan::BuildGraph()
{
	HRESULT hr = S_OK;

    CComBSTR bstrNetworkType;
    CLSID CLSIDNetworkType;
	LONG frequency = 0;
	LONG bandwidth = 0;

	// Initialise the tune request
	if (FAILED(hr = this->InitialiseTuningSpace()))
	{
		cout << "Failed to initialise the Tune Request" << endl;
		return FALSE;
	}

	// Get the current Network Type clsid
    if (FAILED(hr = m_pTuningSpace->get_NetworkType(&bstrNetworkType)))
	{
        cout << "Failed to get TuningSpace Network Type" << endl;
        return hr;
    }

	if (FAILED(hr = CLSIDFromString(bstrNetworkType, &CLSIDNetworkType)))
	{
		cout << "Couldn't get CLSIDFromString" << endl;
		return hr;
	}

    // create the network provider based on the clsid obtained from the tuning space
	if (FAILED(hr = AddFilter(m_piGraphBuilder, CLSIDNetworkType, m_pBDANetworkProvider.p, L"Network Provider")))
	{
		cout << "Failed to add Network Provider to the graph" << endl;
		return FALSE;
	}

	//Create TuneRequest
	CComPtr <ITuneRequest> pTuneRequest;
	if (FAILED(hr = this->newRequest(frequency, bandwidth, pTuneRequest.p)))
	{
		cout << "Failed to create the Tune Request." << endl;
		return FALSE;
	}

	//Apply TuneRequest
	if(SUCCEEDED(hr = m_pBDANetworkProvider->QueryInterface(__uuidof(IScanningTuner), reinterpret_cast<void **>(&m_piTuner))))
	{
		if(FAILED(m_piTuner->put_TuningSpace(m_pTuningSpace)))
		{
			cout << "Failed to give tuning space to tuner." << endl;
			return E_FAIL;
		}
		if (FAILED(hr = m_piTuner->put_TuneRequest(pTuneRequest)))
		{
			cout << "Failed to submit the Tune Tequest to the Network Provider" << endl;
			return E_FAIL;
		}
	}
	else
	{
		cout << "Failed while interfacing Tuner with Network Provider" << endl;
		return E_FAIL;
	}

	//We can now add the rest of the source filters
	
	if (FAILED(hr = AddFilterByDevicePath(m_piGraphBuilder, m_pBDATuner.p, m_pBDACard->tunerDevice.strDevicePath, m_pBDACard->tunerDevice.strFriendlyName)))
	{
		cout << "Cannot load Tuner Device" << endl;
		return FALSE;
	}

	if (m_pBDACard->bUsesDemod)
	{
		if (FAILED(hr = AddFilterByDevicePath(m_piGraphBuilder, m_pBDADemod.p, m_pBDACard->demodDevice.strDevicePath, m_pBDACard->demodDevice.strFriendlyName)))
		{
			cout << "Cannot load Demod Device" << endl;
			return FALSE;
		}
	}

	if (FAILED(hr = AddFilterByDevicePath(m_piGraphBuilder, m_pBDACapture.p, m_pBDACard->captureDevice.strDevicePath, m_pBDACard->captureDevice.strFriendlyName)))
	{
		cout << "Cannot load Capture Device" << endl;
		return FALSE;
	}

	if (FAILED(hr = AddFilter(m_piGraphBuilder, CLSID_MPEG2Demultiplexer, m_pBDAMpeg2Demux.p, L"BDA MPEG-2 Demultiplexer")))
	{
		cout << "Failed to add BDA MPEG-2 Demultiplexer to the graph" << endl;
		return FALSE;
	}

	if (FAILED(hr = AddFilterByName(m_piGraphBuilder, m_pBDATIF.p, KSCATEGORY_BDA_TRANSPORT_INFORMATION, L"BDA MPEG2 Transport Information Filter")))
	{
		cout << "Cannot load TIF" << endl;
		return FALSE;
	}

	if (FAILED(hr = AddFilterByName(m_piGraphBuilder, m_pBDASecTab.p, KSCATEGORY_BDA_TRANSPORT_INFORMATION, L"MPEG-2 Sections and Tables")))
	{
		cout << "Cannot load MPEG-2 Sections and Tables" << endl;
		return FALSE;
	}


    if (FAILED(hr = ConnectFilters(m_piGraphBuilder, m_pBDANetworkProvider, m_pBDATuner)))
	{
		cout << "Failed to connect Network Provider to Tuner Filter" << endl;
		return E_FAIL;
	}

	if (m_pBDACard->bUsesDemod)
	{
		if (FAILED(hr = ConnectFilters(m_piGraphBuilder, m_pBDATuner, m_pBDADemod)))
		{
			cout << "Failed to connect Tuner Filter to Demod Filter" << endl;
			return E_FAIL;
		}

		if (FAILED(hr = ConnectFilters(m_piGraphBuilder, m_pBDADemod, m_pBDACapture)))
		{
			cout << "Failed to connect Demod Filter to Capture Filter" << endl;
			return E_FAIL;
		}
	}
	else
	{
		if (FAILED(hr = ConnectFilters(m_piGraphBuilder, m_pBDATuner, m_pBDACapture)))
		{
			cout << "Failed to connect Tuner Filter to Capture Filter" << endl;
			return E_FAIL;
		}
	}

	CComPtr <IPin> pCapturePin;
	if (FAILED(hr = FindPinByMediaType(m_pBDACapture, MEDIATYPE_Stream, KSDATAFORMAT_SUBTYPE_BDA_MPEG2_TRANSPORT, &pCapturePin.p, REQUESTED_PINDIR_OUTPUT)))
	{
		//If that failed then try the other mpeg2 type, but this shouldn't happen.
		if (FAILED(hr = FindPinByMediaType(m_pBDACapture, MEDIATYPE_Stream, MEDIASUBTYPE_MPEG2_TRANSPORT, &pCapturePin.p, REQUESTED_PINDIR_OUTPUT)))
		{
			cout << "Failed to find Tranport Stream pin on Capture filter" << endl;
			return E_FAIL;
		}
	}

	CComPtr <IPin> pDemuxPin;
	if (FAILED(hr = FindPin(m_pBDAMpeg2Demux, L"MPEG-2 Stream", &pDemuxPin.p, REQUESTED_PINDIR_INPUT)))
	{
		cout << "Failed to get input pin on Demux" << endl;
		return E_FAIL;
	}
	
	if (FAILED(hr = m_piGraphBuilder->ConnectDirect(pCapturePin, pDemuxPin, NULL)))
	{
		cout << "Failed to connect Capture filter to BDA Demux" << endl;
		return E_FAIL;
	}

/*	if (FAILED(hr = ConnectFilters(m_piGraphBuilder, m_pBDACapture, m_pBDAMpeg2Demux)))
	{
		cout << "Failed to connect Infinite Pin Tee Filter to BDA Demux" << endl;
		return E_FAIL;
	}*/

	if (FAILED(hr = ConnectFilters(m_piGraphBuilder, m_pBDAMpeg2Demux, m_pBDATIF)))
	{
		cout << "Failed to connect to BDA Demux to TIF" << endl;
		return E_FAIL;
	}

	if (FAILED(hr = ConnectFilters(m_piGraphBuilder, m_pBDAMpeg2Demux, m_pBDASecTab)))
	{
		cout << "Failed to connect BDA Demux to MPEG-2 Sections and Tables" << endl;
		return E_FAIL;
	}

	m_mpeg2parser.SetFilter(m_pBDASecTab);

	return hr;
}

HRESULT	BDAChannelScan::LockChannel(long lFrequency, long lBandwidth, long &strength, long &quality)
{
	HRESULT hr = S_OK;
	CComPtr <ITuneRequest> piTuneRequest;

	if (SUCCEEDED(hr = this->newRequest(lFrequency, lBandwidth, piTuneRequest.p)))
	{
		CComQIPtr <ITuner> pTuner(m_pBDANetworkProvider);
		if (!pTuner)
		{
			cout << "Failed while interfacing Tuner with Network Provider" << endl;
			return E_FAIL;
		}
		if (FAILED(hr = pTuner->put_TuneRequest(piTuneRequest)))
		{
			cout << "Failed to submit the Tune Tequest to the Network Provider" << endl;
			return S_FALSE;
		}

		pTuner.Release();
		piTuneRequest.Release();


		//Check that channel locked ok.

		//Get IID_IBDA_Topology
		CComPtr <IBDA_Topology> bdaNetTop;
		if (FAILED(hr = m_pBDATuner.QueryInterface(&bdaNetTop)))
		{
			cout << "Cannot Find IID_IBDA_Topology" << endl;
			return E_FAIL;
		}

		ULONG NodeTypes;
		ULONG NodeType[32];
		ULONG Interfaces;
		GUID Interface[32];
		CComPtr <IUnknown> iNode;

		long longVal;
		longVal = strength = quality = 0;
		BYTE byteVal, locked, present;
		byteVal = locked = present = 0;

		if (FAILED(hr = bdaNetTop->GetNodeTypes(&NodeTypes, 32, NodeType)))
		{
			cout << "Cannot get node types" << endl;
			return E_FAIL;
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
		
		if ((locked>0) || (quality>0))
		{
			return S_OK;
		}
		return S_FALSE;
	}

	return hr;
}

HRESULT BDAChannelScan::createConnectionPoint()
{
	HRESULT hr;

	if(FAILED(hr = m_pBDATIF->QueryInterface(__uuidof(IGuideData), reinterpret_cast<void **>(&m_piGuideData))))
	{
		cout << "Failed to interface GuideData with TIF" << endl;
		return E_FAIL;
	}

	CComPtr <IConnectionPointContainer> piContainer;
	if(SUCCEEDED(hr = m_pBDATIF->QueryInterface(__uuidof(IConnectionPointContainer), reinterpret_cast<void **>(&piContainer))))
	{
		if(SUCCEEDED(hr = piContainer->FindConnectionPoint(__uuidof(IGuideDataEvent), &m_piConnectionPoint)))
		{
			hr = m_piConnectionPoint->Advise(this, &m_dwAdviseCookie);
			piContainer.Release();
		}
	}
	else
	{
		cout << "Failed to interface ConnectionPoint with TIF" << endl;
		return E_FAIL;
	}
	return S_OK;
}

HRESULT BDAChannelScan::newRequest(LONG lFrequency, LONG lBandwidth, ITuneRequest* &pExTuneRequest)
{
	HRESULT hr = S_OK;

	if (m_pTuningSpace == NULL)
	{
        cout << "Tuning Space is NULL" << endl;
        return E_FAIL;
	}

	//Get an interface to the TuningSpace
	CComQIPtr <IDVBTuningSpace2> piDVBTuningSpace(m_pTuningSpace);
    if (!piDVBTuningSpace)
	{
        cout << "Can't Query Interface for an IDVBTuningSpace2" << endl;
        return E_FAIL;
    }

	//Get new TuneRequest from tuning space
	//CComPtr <ITuneRequest> pTuneRequest;
	hr = piDVBTuningSpace->CreateTuneRequest(&pExTuneRequest);
	piDVBTuningSpace.Release();
	if (FAILED(hr))
	{
		cout << "Failed to create Tune Request" << endl;
		return E_FAIL;
	}

	//Get interface to the TuneRequest
	CComQIPtr <IDVBTuneRequest> piDVBTuneRequest(pExTuneRequest);
	if (!piDVBTuneRequest)
	{
		pExTuneRequest->Release();
		cout << "Can't Query Interface for an IDVBTuneRequest." << endl;
		return E_FAIL;
	}

	//
	// Start
	//
	CComPtr <IDVBTLocator> pDVBTLocator;
	hr = pDVBTLocator.CoCreateInstance(CLSID_DVBTLocator);
	switch (hr)
	{ 
	case REGDB_E_CLASSNOTREG:
		pExTuneRequest->Release();
		piDVBTuneRequest.Release();
		cout << "The DVBTLocator class isn't registered in the registration database." << endl;
		return E_FAIL;
	case CLASS_E_NOAGGREGATION:
		pExTuneRequest->Release();
		piDVBTuneRequest.Release();
		cout << "The DVBTLocator class can't be created as part of an aggregate." << endl;
		return E_FAIL;
	}

	if (FAILED(hr = pDVBTLocator->put_CarrierFrequency(lFrequency)))
	{
		pDVBTLocator.Release();
		pExTuneRequest->Release();
		piDVBTuneRequest.Release();
		cout << "Can't set Frequency on Locator." << endl;
		return E_FAIL;
	}
	if(FAILED(hr = pDVBTLocator->put_SymbolRate(lBandwidth)))
	{
		pDVBTLocator.Release();
		pExTuneRequest->Release();
		piDVBTuneRequest.Release();
		cout << "Can't set Bandwidth on Locator." << endl;
		return E_FAIL;
	}
	//
	//End
	//

	piDVBTuneRequest->put_ONID(-1);
	piDVBTuneRequest->put_TSID(-1);
	piDVBTuneRequest->put_SID(-1);

	// Bind the locator to the tune request.

	if(FAILED(hr = piDVBTuneRequest->put_Locator(pDVBTLocator)))
	{
		pDVBTLocator.Release();
		pExTuneRequest->Release();
		piDVBTuneRequest.Release();
		cout << "Cannot put the locator on DVB Tune Request" << endl;
		return E_FAIL;
	}

	//hr = pTuneRequest.QueryInterface(pExTuneRequest);

	pDVBTLocator.Release();
	piDVBTuneRequest.Release();

	return hr;
}

HRESULT BDAChannelScan::InitialiseTuningSpace()
{

	HRESULT hr = S_OK;

	m_pTuningSpace.CoCreateInstance(CLSID_DVBTuningSpace);
	CComQIPtr <IDVBTuningSpace2> piDVBTuningSpace(m_pTuningSpace);
	if (!piDVBTuningSpace)
	{
		m_pTuningSpace.Release();
		return E_FAIL;
	}
	if (FAILED(hr = piDVBTuningSpace->put_SystemType(DVB_Terrestrial)))
	{
		piDVBTuningSpace.Release();
		m_pTuningSpace.Release();
		return E_FAIL;
	}
	CComBSTR bstrNetworkType = "{216C62DF-6D7F-4E9A-8571-05F14EDB766A}";
	if (FAILED(hr = piDVBTuningSpace->put_NetworkType(bstrNetworkType)))
	{
		piDVBTuningSpace.Release();
		m_pTuningSpace.Release();
		return E_FAIL;
	}
	piDVBTuningSpace.Release();

	return hr;
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
	m_piMediaControl->Stop();

	return TRUE;
}

HRESULT BDAChannelScan::scanChannel(long channelNumber, long frequency, long bandwidth)
{
	ResetEvent(m_hGuideDataChangedEvent);
	m_mpeg2parser.Reset();
	m_mpeg2parser.SetNetworkNumber(channelNumber);
	m_bScanning = TRUE;

	long nStrength = 0;
	long nQuality = 0;

	HRESULT hr = LockChannel(frequency, bandwidth, nStrength, nQuality);

	switch (hr)
	{
		case S_OK:
			printf("# locked %ld, %ld signal strength = %ld quality = %ld\n",
					frequency, bandwidth, nStrength, nQuality);
			m_mpeg2parser.WaitForScanToFinish(INFINITE);
			break;

		default:
			printf("# no lock %ld, %ld signal strength = %ld quality = %ld\n",
					frequency, bandwidth, nStrength, nQuality);
			break;
	}

	m_bScanning = FALSE;
	return hr;
}

HRESULT BDAChannelScan::scanChannels()
{
	HRESULT hr = S_OK;

	if(!m_piTuner)
	{
		cout << "Tuner is NULL" << endl;
		return E_FAIL;
	}

	//if(FAILED(hr = m_piTuner->AutoProgram())) {
	//	cout << "AutoProgram failed." << endl;
	//	return E_FAIL;
	//}

	while(SUCCEEDED(hr))
	{
		ResetEvent(m_hGuideDataChangedEvent);
		hr = m_piTuner->SeekDown();
		//if (SUCCEEDED(hr))
			//getServiceProperties();
	}

	return hr;
}