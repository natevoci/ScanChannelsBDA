/**
 *	BDACardCollection.cpp
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

//this is a file from DigitalWatch 2 that i've hacked up to work here.

#include "BDACardCollection.h"
#include "StdAfx.h"
#include "ParseLine.h"

#include "GlobalFunctions.h"
#include "FilterGraphTools.h"
#include <dshow.h>
#include <ks.h> // Must be included before ksmedia.h
#include <ksmedia.h> // Must be included before bdamedia.h
#include <bdatypes.h> // Must be included before bdamedia.h
#include <bdamedia.h>
#include "FileWriter.h"
#include "FileReader.h"

BDACardCollection::BDACardCollection()
{
	m_filename = NULL;
}

BDACardCollection::~BDACardCollection()
{
	std::vector<BDACard *>::iterator it = cards.begin();
	for ( ; it != cards.end() ; it++ )
	{
		delete *it;
	}
	cards.clear();
}

BOOL BDACardCollection::LoadCards()
{
	if (m_filename)
		delete m_filename;
	m_filename = NULL;

	return LoadCardsFromHardware();
}

BOOL BDACardCollection::LoadCards(LPWSTR filename)
{
	USES_CONVERSION;
	strCopy(m_filename, filename);

	LoadCardsFromFile();

	return LoadCardsFromHardware();
}

BOOL BDACardCollection::LoadCardsFromFile()
{
	FileReader file;
	if (file.Open(m_filename) == FALSE)
		//return (g_log << "Could not open cards file: " << m_filename).Write();
		return FALSE;

	//(g_log << "Loading BDA DVB-T Cards file: " << m_filename).Write();

	try
	{
		int line = 0;
		LPWSTR pBuff = NULL;
		LPWSTR pCurr;
		BDACard* currCard = NULL;

		while (file.ReadLine(pBuff))
		{
			line++;

			pCurr = pBuff;
			skipWhitespaces(pCurr);
			if ((pCurr[0] == '\0') || (pCurr[0] == '#'))
				continue;

			ParseLine parseLine;
			if (parseLine.Parse(pBuff) == FALSE)
				//return (g_log << "Parse Error in " << m_filename << ": Line " << line << ":" << parseLine.GetErrorPosition() << "\n" << parseLine.GetErrorMessage()).Show();
				return FALSE;

			if (parseLine.HasRHS())
				//return (g_log << "Parse Error in " << m_filename << ": Line " << line << "\nEquals not valid for this command").Show();
				return FALSE;

			if (parseLine.LHS.ParameterCount < 1)
				//return (g_log << "Parse Error in " << m_filename << ": Line " << line << "\nMissing expected parameter").Show();
				return FALSE;

			if (parseLine.LHS.ParameterCount > 1)
				//return (g_log << "Parse Error in " << m_filename << ": Line " << line << "\nToo many parameters").Show();
				return FALSE;

			pCurr = parseLine.LHS.FunctionName;

			if (_wcsicmp(pCurr, L"TunerDevice") == 0)
			{
				pCurr = parseLine.LHS.Parameter[0];

				AddCardToList(currCard);

				currCard = new BDACard();
				strCopy(currCard->tunerDevice.strDevicePath, pCurr);
				continue;
			}
			if (_wcsicmp(pCurr, L"TunerName") == 0)
			{
				if (!currCard)
					//return (g_log << "Parse Error in " << m_filename << ": Line " << line << "TunerName function must come after a TunerDevice function.\n").Write();
					return FALSE;

				pCurr = parseLine.LHS.Parameter[0];
				strCopy(currCard->tunerDevice.strFriendlyName, pCurr);
				continue;
			}
			if (_wcsicmp(pCurr, L"CaptureDevice") == 0)
			{
				if (!currCard)
					//return (g_log << "Parse Error in " << m_filename << ": Line " << line << "TunerName function must come after a TunerDevice function.\n").Write();
					return FALSE;

				pCurr = parseLine.LHS.Parameter[0];
				strCopy(currCard->captureDevice.strDevicePath, pCurr);
				continue;
			}
			if (_wcsicmp(pCurr, L"CaptureName") == 0)
			{
				if (!currCard)
					//return (g_log << "Parse Error in " << m_filename << ": Line " << line << "TunerName function must come after a TunerDevice function.\n").Write();
					return FALSE;

				pCurr = parseLine.LHS.Parameter[0];
				strCopy(currCard->captureDevice.strFriendlyName, pCurr);
				continue;
			}
			if (_wcsicmp(pCurr, L"Active") == 0)
			{
				if (!currCard)
					//return (g_log << "Parse Error in " << m_filename << ": Line " << line << "TunerName function must come after a TunerDevice function.\n").Write();
					return FALSE;

				pCurr = parseLine.LHS.Parameter[0];
				currCard->bActive = (pCurr[0] != '0');
				continue;
			}

			//return (g_log << "Parse Error in " << m_filename << ": Line " << line << ":unexpected term '" << pCurr << "'").Show();
			return FALSE;
		}

		AddCardToList(currCard);
	}	   
	catch (LPWSTR str)
	{
		//(g_log << "Error caught reading cards file: " << str).Show();
		cout << "Error caught reading cards file: " << str << endl;
		file.Close();
		return FALSE;
	}
	file.Close();

	return TRUE;
}

void BDACardCollection::AddCardToList(BDACard* currCard)
{
	//check if currCard is present and check that all it's values got set before pushing it into the cards vector
	if (currCard)
	{
		if ((currCard->tunerDevice.strDevicePath == NULL) ||
			(currCard->tunerDevice.strFriendlyName == NULL) ||
			(currCard->captureDevice.strDevicePath == NULL) ||
			(currCard->captureDevice.strFriendlyName == NULL))
		{
			//(g_log << "Error in " << m_filename << ": Cannot add device without setting both tuner and capture name and device paths").Write();
			return;
		}
		cards.push_back(currCard);
		//(g_log << "  " << currCard->tunerDevice.strFriendlyName << " [" << (currCard->bActive ? "Active" : "Not Active") << "] - " << currCard->tunerDevice.strDevicePath).Write();
		//(g_log << "    " << currCard->captureDevice.strFriendlyName << " - " << currCard->captureDevice.strDevicePath).Write();
	}
}

BOOL BDACardCollection::SaveCards(LPWSTR filename)
{
	USES_CONVERSION;

	FileWriter file;
	if (filename)
	{
		if (file.Open(filename) == FALSE)
			//return (g_log << "Could not open cards file for writing: " << filename).Show();
			return FALSE;
	}
	else
	{
		if (file.Open(m_filename) == FALSE)
			//return (g_log << "Could not open cards file for writing: " << m_filename).Show();
			return FALSE;
	}

	//(g_log << "Saving BDA DVB-T Cards file: " << m_filename).Write();

	try
	{
		file << "# DigitalWatch - BDA Cards File" << file.EOL;
		file << "#" << file.EOL;
		file << file.EOL;

		BDACard *currCard;
		std::vector<BDACard *>::iterator it = cards.begin();
		for ( ; it != cards.end() ; it++ )
		{
			currCard = *it;
			file << "TunerDevice(\"" << W2A(currCard->tunerDevice.strDevicePath) << "\")" << file.EOL;
			file << "  TunerName(\"" << W2A(currCard->tunerDevice.strFriendlyName) << "\")" << file.EOL;
			file << "  CaptureDevice(\"" << W2A(currCard->captureDevice.strDevicePath) << "\")" << file.EOL;
			file << "  CaptureName(\"" << W2A(currCard->captureDevice.strFriendlyName) << "\")" << file.EOL;
			file << "  Active(" << (currCard->bActive ? 1 : 0) << ")" << file.EOL;
			file << file.EOL;
		}
	}
	catch (LPWSTR str)
	{
		//(g_log << str).Show();
		cout << "Error caught saving cards file: " << str << endl;
		file.Close();
		return FALSE;
	}
	file.Close();
	return TRUE;
}

BOOL BDACardCollection::LoadCardsFromHardware()
{
	HRESULT hr = S_OK;

	//(g_log << "Checking for new BDA DVB-T Cards").Write();

	DirectShowSystemDevice* pTunerDevice;
	DirectShowSystemDeviceEnumerator enumerator(KSCATEGORY_BDA_NETWORK_TUNER);

	while (enumerator.Next(&pTunerDevice) == S_OK)
	{
		BDACard *bdaCard = NULL;

		std::vector<BDACard *>::iterator it = cards.begin();
		for ( ; it != cards.end() ; it++ )
		{
			bdaCard = *it;
			if (_wcsicmp(pTunerDevice->strDevicePath, bdaCard->tunerDevice.strDevicePath) == 0)
				break;
			bdaCard = NULL;
		}

		if (bdaCard)
		{
			bdaCard->bDetected = TRUE;
			//verify tuner can connect to the capture filter.
		}
		else
		{
			DirectShowSystemDevice* pDemodDevice;
			DirectShowSystemDevice* pCaptureDevice;

			if (FindCaptureDevice(pTunerDevice, &pDemodDevice, &pCaptureDevice))
			{
				bdaCard = new BDACard();
				bdaCard->tunerDevice = *pTunerDevice;

				if (pDemodDevice)
					bdaCard->demodDevice = *pDemodDevice;
				if (pCaptureDevice)
					bdaCard->captureDevice = *pCaptureDevice;

				cards.push_back(bdaCard);
			}
		}

		delete pTunerDevice;
		pTunerDevice = NULL;
	}

	if (cards.size() == 0)
		//return (g_log << "No cards found").Show();
		return FALSE;

	return TRUE;
}

BOOL BDACardCollection::FindCaptureDevice(DirectShowSystemDevice* pTunerDevice, DirectShowSystemDevice** ppDemodDevice, DirectShowSystemDevice** ppCaptureDevice)
{
	HRESULT hr;
	CComPtr <IGraphBuilder> piGraphBuilder;
	CComPtr <IBaseFilter> piBDANetworkProvider;
	CComPtr <ITuningSpace> piTuningSpace;
	CComPtr <ITuneRequest> pTuneRequest;
	CComPtr <IBaseFilter> piBDATuner;
	CComPtr <IBaseFilter> piBDADemod;
	CComPtr <IBaseFilter> piBDACapture;

	BOOL bRemoveNP = FALSE;
	BOOL bRemoveTuner = FALSE;
	BOOL bFoundDemod = FALSE;
	BOOL bFoundCapture = FALSE;

	do
	{
		//--- Create Graph ---
		if (FAILED(hr = piGraphBuilder.CoCreateInstance(CLSID_FilterGraph, NULL, CLSCTX_INPROC_SERVER)))
		{
			//(g_log << "Failed Creating DVB-T Graph Builder").Write();
			break;
		}

		//--- Initialise Tune Request ---
		if (FAILED(hr = InitDVBTTuningSpace(piTuningSpace)))
		{
			//(g_log << "Failed to initialise the Tune Request").Write();
			break;
		}

		//--- Get NetworkType CLSID ---
		CComBSTR bstrNetworkType;
		CLSID CLSIDNetworkType;
		if (FAILED(hr = piTuningSpace->get_NetworkType(&bstrNetworkType)))
		{
			//(g_log << "Failed to get TuningSpace Network Type").Write();
			break;
		}

		if (FAILED(hr = CLSIDFromString(bstrNetworkType, &CLSIDNetworkType)))
		{
			//(g_log << "Could not convert Network Type to CLSID").Write();
			break;
		}

		//--- Create Network Provider ---
		if (FAILED(hr = AddFilter(piGraphBuilder.p, CLSIDNetworkType, piBDANetworkProvider.p, L"Network Provider")))
		{
			//(g_log << "Failed to add Network Provider to the graph").Write();
			break;
		}
		bRemoveNP = TRUE;

		//--- Create TuneRequest ---
		if (FAILED(hr = SubmitDVBTTuneRequest(piTuningSpace, pTuneRequest, -1, -1)))
		{
			//(g_log << "Failed to create the Tune Request.").Write();
			break;
		}

		//--- Apply TuneRequest ---
		CComQIPtr <ITuner> pTuner(piBDANetworkProvider);
		if (!pTuner)
		{
			//(g_log << "Failed while interfacing Tuner with Network Provider").Write();
			break;
		}

		hr = pTuner->put_TuneRequest(pTuneRequest);
		pTuner.Release();
		if (FAILED(hr))
		{
			//(g_log << "Failed to submit the Tune Tequest to the Network Provider").Write();
			break;
		}

		//--- We can now add and connect the tuner filter ---
		
		if (FAILED(hr = AddFilterByDevicePath(piGraphBuilder, piBDATuner.p, pTunerDevice->strDevicePath, pTunerDevice->strFriendlyName)))
		{
			//(g_log << "Cannot load Tuner Device").Write();
			break;
		}
		bRemoveTuner = TRUE;
    
		if (FAILED(hr = ConnectFilters(piGraphBuilder, piBDANetworkProvider, piBDATuner)))
		{
			//(g_log << "Failed to connect Network Provider to Tuner Filter").Write();
			break;
		}

		DirectShowSystemDeviceEnumerator enumerator(KSCATEGORY_BDA_RECEIVER_COMPONENT);
		*ppDemodDevice = NULL;
		while (hr = enumerator.Next(ppDemodDevice) == S_OK)
		{
			if (FAILED(hr = AddFilterByDevicePath(piGraphBuilder, piBDADemod.p, (*ppDemodDevice)->strDevicePath, (*ppDemodDevice)->strFriendlyName)))
			{
				break;
			}

			if (SUCCEEDED(hr = ConnectFilters(piGraphBuilder, piBDATuner, piBDADemod)))
			{
				bFoundDemod = TRUE;
				break;
			}

			//(g_log << "Connection failed, trying next card").Write();
			piBDADemod.Release();
			delete *ppDemodDevice;
			*ppDemodDevice = NULL;
		}

		if (bFoundDemod)
		{
			*ppCaptureDevice = NULL;
			while (hr = enumerator.Next(ppCaptureDevice) == S_OK)
			{
				if (FAILED(hr = AddFilterByDevicePath(piGraphBuilder, piBDACapture.p, (*ppCaptureDevice)->strDevicePath, (*ppCaptureDevice)->strFriendlyName)))
				{
					break;
				}

				if (SUCCEEDED(hr = ConnectFilters(piGraphBuilder, piBDADemod, piBDACapture)))
				{
					bFoundCapture = TRUE;
					break;
				}

				//(g_log << "Connection failed, trying next card").Write();
				piBDACapture.Release();
				delete *ppCaptureDevice;
				*ppCaptureDevice = NULL;
			}
		}

		//if (hr != S_OK)
			//(g_log << "No Cards Left").Write();
	} while (FALSE);

	DisconnectAllPins(piGraphBuilder);
	if (bFoundCapture)
		piGraphBuilder->RemoveFilter(piBDACapture);
	if (bFoundDemod)
		piGraphBuilder->RemoveFilter(piBDADemod);
	if (bRemoveTuner)
		piGraphBuilder->RemoveFilter(piBDATuner);
	if (bRemoveNP)
		piGraphBuilder->RemoveFilter(piBDANetworkProvider);

	piBDACapture.Release();
	piBDADemod.Release();
	piBDATuner.Release();
	piBDANetworkProvider.Release();

	pTuneRequest.Release();
	piTuningSpace.Release();
	piGraphBuilder.Release();

	if (!bFoundCapture)
	{
		*ppCaptureDevice = *ppDemodDevice;
		*ppDemodDevice = NULL;
	}

	return (bFoundDemod);
}

