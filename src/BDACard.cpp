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
#include "FilterGraphTools.h"
#include "LogMessage.h"

//#include "GlobalFunctions.h"
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
	m_pBDATuner.Release();
	m_pBDADemod.Release();
	m_pBDACapture.Release();
}

HRESULT BDACard::AddFilters(IGraphBuilder* piGraphBuilder)
{
	HRESULT hr;

	if (FAILED(hr = AddFilterByDevicePath(piGraphBuilder, m_pBDATuner.p, tunerDevice.strDevicePath, tunerDevice.strFriendlyName)))
	{
		return (g_log << "Cannot load Tuner Device\n").Show(hr);
	}

	if (demodDevice.bValid)
	{
		if (FAILED(hr = AddFilterByDevicePath(piGraphBuilder, m_pBDADemod.p, demodDevice.strDevicePath, demodDevice.strFriendlyName)))
		{
			return (g_log << "Cannot load Demod Device\n").Show(hr);
		}
	}

	if (captureDevice.bValid)
	{
		if (FAILED(hr = AddFilterByDevicePath(piGraphBuilder, m_pBDACapture.p, captureDevice.strDevicePath, captureDevice.strFriendlyName)))
		{
			return (g_log << "Cannot load Capture Device\n").Show(hr);
		}
	}
	return S_OK;
}

HRESULT BDACard::Connect(IGraphBuilder* piGraphBuilder, IBaseFilter* pSource)
{
	HRESULT hr;

    if (FAILED(hr = ConnectFilters(piGraphBuilder, pSource, m_pBDATuner)))
	{
		return (g_log << "Failed to connect Network Provider to Tuner Filter\n").Show(hr);
	}

	if (captureDevice.bValid)
	{
		if (demodDevice.bValid)
		{
			if (FAILED(hr = ConnectFilters(piGraphBuilder, m_pBDATuner, m_pBDADemod)))
			{
				return (g_log << "Failed to connect Tuner Filter to Demod Filter\n").Show(hr);
			}

			if (FAILED(hr = ConnectFilters(piGraphBuilder, m_pBDADemod, m_pBDACapture)))
			{
				return (g_log << "Failed to connect Demod Filter to Capture Filter\n").Show(hr);
			}

		}
		else
		{
			if (FAILED(hr = ConnectFilters(piGraphBuilder, m_pBDATuner, m_pBDACapture)))
			{
				return (g_log << "Failed to connect Tuner Filter to Capture Filter\n").Show(hr);
			}
		}
		
		if (FAILED(hr = FindPinByMediaType(m_pBDACapture, MEDIATYPE_Stream, KSDATAFORMAT_SUBTYPE_BDA_MPEG2_TRANSPORT, &m_pCapturePin.p, REQUESTED_PINDIR_OUTPUT)))
		{
			//If that failed then try the other mpeg2 type, but this shouldn't happen.
			if (FAILED(hr = FindPinByMediaType(m_pBDACapture, MEDIATYPE_Stream, MEDIASUBTYPE_MPEG2_TRANSPORT, &m_pCapturePin.p, REQUESTED_PINDIR_OUTPUT)))
			{
				return (g_log << "Failed to find Tranport Stream pin on Capture filter\n").Show(hr);
			}
		}
	}
	else if (demodDevice.bValid)
	{
		if (FAILED(hr = ConnectFilters(piGraphBuilder, m_pBDATuner, m_pBDADemod)))
		{
			return (g_log << "Failed to connect Tuner Filter to Demod Filter\n").Show(hr);
		}

		if (FAILED(hr = FindPinByMediaType(m_pBDADemod, MEDIATYPE_Stream, KSDATAFORMAT_SUBTYPE_BDA_MPEG2_TRANSPORT, &m_pCapturePin.p, REQUESTED_PINDIR_OUTPUT)))
		{
			//If that failed then try the other mpeg2 type, but this shouldn't happen.
			if (FAILED(hr = FindPinByMediaType(m_pBDADemod, MEDIATYPE_Stream, MEDIASUBTYPE_MPEG2_TRANSPORT, &m_pCapturePin.p, REQUESTED_PINDIR_OUTPUT)))
			{
				return (g_log << "Failed to find Tranport Stream pin on Capture filter\n").Show(hr);
			}
		}
	}
	else
	{
		if (FAILED(hr = FindPinByMediaType(m_pBDATuner, MEDIATYPE_Stream, KSDATAFORMAT_SUBTYPE_BDA_MPEG2_TRANSPORT, &m_pCapturePin.p, REQUESTED_PINDIR_OUTPUT)))
		{
			//If that failed then try the other mpeg2 type, but this shouldn't happen.
			if (FAILED(hr = FindPinByMediaType(m_pBDATuner, MEDIATYPE_Stream, MEDIASUBTYPE_MPEG2_TRANSPORT, &m_pCapturePin.p, REQUESTED_PINDIR_OUTPUT)))
			{
				return (g_log << "Failed to find Tranport Stream pin on Capture filter\n").Show(hr);
			}
		}
	}
	return S_OK;
}

HRESULT BDACard::GetCapturePin(IPin** pCapturePin)
{
	return m_pCapturePin->QueryInterface(IID_IPin, (void **)pCapturePin);
}

