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

#include "BDACard.h"
#include "StdAfx.h"
#include "GlobalFunctions.h"

//#include <dshow.h>
#include <ks.h> // Must be included before ksmedia.h
#include <ksmedia.h> // Must be included before bdamedia.h
#include <bdatypes.h> // Must be included before bdamedia.h
#include <bdamedia.h>

BDACard::BDACard()
{
	bActive = FALSE;
	bNew = FALSE;
	bDetected = FALSE;
}

BDACard::~BDACard()
{
	m_pCapturePin.Release();
	RemoveFilters();
	m_piGraphBuilder.Release();
}

void BDACard::SetLogCallback(LogMessageCallback *callback)
{
	LogMessageCaller::SetLogCallback(callback);

	tunerDevice.SetLogCallback(callback);
	demodDevice.SetLogCallback(callback);
	captureDevice.SetLogCallback(callback);
	graphTools.SetLogCallback(callback);
}

HRESULT BDACard::LoadFromXML(XMLElement *pElement)
{
	XMLAttribute *attr;
	attr = pElement->Attributes.Item(L"active");
	bActive = (attr) && (attr->value[0] == '1');

	XMLElement *device;
	device = pElement->Elements.Item(L"Tuner");
	if (device == NULL)
	{
		return (log << "Cannot add device without setting tuner name and device path\n").Write(E_FAIL);
	}

	attr = device->Attributes.Item(L"device");
	strCopy(tunerDevice.strDevicePath, (attr ? attr->value : L""));
	attr = device->Attributes.Item(L"name");
	strCopy(tunerDevice.strFriendlyName, (attr ? attr->value : L""));
	tunerDevice.bValid = TRUE;
	(log << "Card - " << (bActive ? "Active" : "Not Active") << "\n").Write();
	LogMessageIndent indent(&log);
	(log << tunerDevice.strFriendlyName << "\n").Write();
	(log << "  " << tunerDevice.strDevicePath << "\n").Write();

	device = pElement->Elements.Item(L"Demod");
	if (device != NULL)
	{
		attr = device->Attributes.Item(L"device");
		strCopy(demodDevice.strDevicePath, (attr ? attr->value : L""));
		attr = device->Attributes.Item(L"name");
		strCopy(demodDevice.strFriendlyName, (attr ? attr->value : L""));
		demodDevice.bValid = TRUE;
		(log << demodDevice.strFriendlyName << "\n").Write();
		(log << "  " << demodDevice.strDevicePath << "\n").Write();
	}

	device = pElement->Elements.Item(L"Capture");
	if (device != NULL)
	{
		attr = device->Attributes.Item(L"device");
		strCopy(captureDevice.strDevicePath, (attr ? attr->value : L""));
		attr = device->Attributes.Item(L"name");
		strCopy(captureDevice.strFriendlyName, (attr ? attr->value : L""));
		captureDevice.bValid = TRUE;
		(log << captureDevice.strFriendlyName << "\n").Write();
		(log << "  " << captureDevice.strDevicePath << "\n").Write();
	}

	return S_OK;
}

HRESULT BDACard::SaveToXML(XMLElement *pElement)
{
	pElement->Attributes.Add(new XMLAttribute(L"active", (bActive ? L"1" : L"0")));
	pElement->Attributes.Add(new XMLAttribute(L"detected", (bDetected ? L"1" : L"0")));

	if (tunerDevice.bValid)
	{
		XMLElement *device;
		device = new XMLElement(L"Tuner");
		device->Attributes.Add(new XMLAttribute(L"device", tunerDevice.strDevicePath));
		device->Attributes.Add(new XMLAttribute(L"name", tunerDevice.strFriendlyName));
		pElement->Elements.Add(device);
	}
	if (demodDevice.bValid)
	{
		XMLElement *device;
		device = new XMLElement(L"Demod");
		device->Attributes.Add(new XMLAttribute(L"device", demodDevice.strDevicePath));
		device->Attributes.Add(new XMLAttribute(L"name", demodDevice.strFriendlyName));
		pElement->Elements.Add(device);
	}
	if (captureDevice.bValid)
	{
		XMLElement *device;
		device = new XMLElement(L"Capture");
		device->Attributes.Add(new XMLAttribute(L"device", captureDevice.strDevicePath));
		device->Attributes.Add(new XMLAttribute(L"name", captureDevice.strFriendlyName));
		pElement->Elements.Add(device);
	}

	return S_OK;
}

HRESULT BDACard::AddFilters(IGraphBuilder* piGraphBuilder)
{
	HRESULT hr;

	m_piGraphBuilder = piGraphBuilder;

	if FAILED(hr = graphTools.AddFilterByDevicePath(m_piGraphBuilder, &m_pBDATuner, tunerDevice.strDevicePath, tunerDevice.strFriendlyName))
	{
		return (log << "Cannot load Tuner Device: " << hr << "\n").Show(hr);
	}

	if (demodDevice.bValid)
	{
		if FAILED(hr = graphTools.AddFilterByDevicePath(m_piGraphBuilder, &m_pBDADemod, demodDevice.strDevicePath, demodDevice.strFriendlyName))
		{
			return (log << "Cannot load Demod Device: " << hr << "\n").Show(hr);
		}
	}

	if (captureDevice.bValid)
	{
		if FAILED(hr = graphTools.AddFilterByDevicePath(m_piGraphBuilder, &m_pBDACapture, captureDevice.strDevicePath, captureDevice.strFriendlyName))
		{
			return (log << "Cannot load Capture Device: " << hr << "\n").Show(hr);
		}
	}
	return S_OK;
}

HRESULT BDACard::RemoveFilters()
{
	m_pCapturePin.Release();

	if (m_pBDACapture)
	{
		m_piGraphBuilder->RemoveFilter(m_pBDACapture);
		m_pBDACapture.Release();
	}
	if (m_pBDADemod)
	{
		m_piGraphBuilder->RemoveFilter(m_pBDADemod);
		m_pBDADemod.Release();
	}
	if (m_pBDATuner)
	{
		m_piGraphBuilder->RemoveFilter(m_pBDATuner);
		m_pBDATuner.Release();
	}

	return S_OK;
}

HRESULT BDACard::Connect(IBaseFilter* pSource)
{
	HRESULT hr;

	if (!m_piGraphBuilder)
		return E_FAIL;

    if FAILED(hr = graphTools.ConnectFilters(m_piGraphBuilder, pSource, m_pBDATuner))
	{
		return (log << "Failed to connect Network Provider to Tuner Filter: " << hr << "\n").Show(hr);
	}

	IBaseFilter *piLastFilter = NULL;

	if (captureDevice.bValid)
	{
		if (demodDevice.bValid)
		{
			if FAILED(hr = graphTools.ConnectFilters(m_piGraphBuilder, m_pBDATuner, m_pBDADemod))
			{
				return (log << "Failed to connect Tuner Filter to Demod Filter: " << hr << "\n").Show(hr);
			}

			if FAILED(hr = graphTools.ConnectFilters(m_piGraphBuilder, m_pBDADemod, m_pBDACapture))
			{
				return (log << "Failed to connect Demod Filter to Capture Filter: " << hr << "\n").Show(hr);
			}

		}
		else
		{
			if FAILED(hr = graphTools.ConnectFilters(m_piGraphBuilder, m_pBDATuner, m_pBDACapture))
			{
				return (log << "Failed to connect Tuner Filter to Capture Filter: " << hr << "\n").Show(hr);
			}
		}

		piLastFilter = m_pBDACapture;	
	}
	else if (demodDevice.bValid)
	{
		if FAILED(hr = graphTools.ConnectFilters(m_piGraphBuilder, m_pBDATuner, m_pBDADemod))
		{
			return (log << "Failed to connect Tuner Filter to Demod Filter: " << hr << "\n").Show(hr);
		}

		piLastFilter = m_pBDADemod;
	}
	else
	{
		piLastFilter = m_pBDATuner;
	}

	m_pCapturePin.Release();

	if FAILED(hr = graphTools.FindPinByMediaType(piLastFilter, MEDIATYPE_Stream, KSDATAFORMAT_SUBTYPE_BDA_MPEG2_TRANSPORT, &m_pCapturePin, REQUESTED_PINDIR_OUTPUT))
	{
		//If that failed then try the other mpeg2 type, but this shouldn't happen.
		if FAILED(hr = graphTools.FindPinByMediaType(piLastFilter, MEDIATYPE_Stream, MEDIASUBTYPE_MPEG2_TRANSPORT, &m_pCapturePin, REQUESTED_PINDIR_OUTPUT))
		{
			return (log << "Failed to find Tranport Stream pin on last filter: " << hr << "\n").Show(hr);
		}
	}

	return S_OK;
}

HRESULT BDACard::GetCapturePin(IPin** pCapturePin)
{
	if (!m_pCapturePin)
		return E_POINTER;
	*pCapturePin = m_pCapturePin;
	(*pCapturePin)->AddRef();
	return S_OK;
}

HRESULT BDACard::GetSignalStatistics(BOOL &locked, BOOL &present, long &strength, long &quality)
{
	HRESULT hr;

	//Get IID_IBDA_Topology
	CComPtr <IBDA_Topology> bdaNetTop;
	if (FAILED(hr = m_pBDATuner.QueryInterface(&bdaNetTop)))
	{
		return (log << "Cannot Find IID_IBDA_Topology\n").Show(hr);
	}

	ULONG NodeTypes;
	ULONG NodeType[32];
	//ULONG Interfaces;
	//GUID Interface[32];
	CComPtr <IUnknown> iNode;

	long longVal;
	longVal = strength = quality = 0;
	BYTE byteVal;
	byteVal = locked = present = 0;

	if (FAILED(hr = bdaNetTop->GetNodeTypes(&NodeTypes, 32, NodeType)))
	{
		return (log << "Cannot get node types\n").Show(hr);
	}

	for ( int i=0 ; i<NodeTypes ; i++ )
	{
		hr = bdaNetTop->GetControlNode(0, 1, NodeType[i], &iNode);
		if (hr == S_OK)
		{
			CComPtr <IBDA_SignalStatistics> pSigStats;

			hr = iNode.QueryInterface(&pSigStats);
			if (hr == S_OK)
			{
				longVal = 0;
				if (SUCCEEDED(hr = pSigStats->get_SignalStrength(&longVal)))
					strength = longVal;

				longVal = 0;
				if (SUCCEEDED(hr = pSigStats->get_SignalQuality(&longVal)))
					quality = longVal;

				byteVal = 0;
				if (SUCCEEDED(hr = pSigStats->get_SignalLocked(&byteVal)))
					locked = byteVal;

				byteVal = 0;
				if (SUCCEEDED(hr = pSigStats->get_SignalPresent(&byteVal)))
					present = byteVal;

				pSigStats.Release();
			}
			iNode.Release();
		}
	}
	bdaNetTop.Release();

	return S_OK;
}


