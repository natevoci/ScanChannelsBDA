/**
 *	ScanChannelsBDA.h
 *	Copyright (C) 2004 BionicDonkey
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

#ifndef BDASCANCHANNELS_H
#define BDASCANCHANNELS_H

#include "stdafx.h"
#include "Mpeg2DataParser.h"
#include "BDACardCollection.h"

#include <bdatif.h>

class BDAChannelScan : IGuideDataEvent
{
public:
	BDAChannelScan();
	~BDAChannelScan();
	
	HRESULT CreateGraph();
	HRESULT	BuildGraph();
	HRESULT	LockChannel(long lFrequency, long lBandwidth, BOOL &locked, BOOL &present, long &strength, long &quality);
	HRESULT	newRequest(long lFrequency, long lBandwidth, ITuneRequest* &pExTuneRequest);
	HRESULT	InitialiseTuningSpace();

	HRESULT selectCard();
	void AddNetwork(long freq, long band);
	HRESULT scanNetworks();
	HRESULT scanAll();
	//HRESULT scanOne();

	BOOL IsVerbose() { return m_bVerbose; }
	void ToggleVerbose() { m_bVerbose = !m_bVerbose; }

	BOOL StartGraph();
	BOOL StopGraph();

	STDMETHODIMP_(ULONG) AddRef(void);
	STDMETHODIMP_(ULONG) Release(void);
	STDMETHODIMP QueryInterface(REFIID, void **);
	
	STDMETHODIMP ProgramChanged(VARIANT varProgramDescriptionID);
	STDMETHODIMP ServiceChanged(VARIANT varServiceDescriptionID);
	STDMETHODIMP ScheduleEntryChanged(VARIANT varScheduleEntryDescriptionID);
	STDMETHODIMP GuideDataAcquired();
	STDMETHODIMP ProgramDeleted(VARIANT varProgramDescriptionID);
	STDMETHODIMP ServiceDeleted(VARIANT varServiceDescriptionID);
	STDMETHODIMP ScheduleDeleted(VARIANT varScheduleEntryDescriptionID);

	HRESULT createConnectionPoint();


private:	
	HRESULT scanChannel(long channelNumber, long frequency, long bandwidth);
	HRESULT scanChannels();

	CComPtr <IBaseFilter> m_pBDANetworkProvider;
	CComPtr <IBaseFilter> m_pBDATuner;
	CComPtr <IBaseFilter> m_pBDADemod;
	CComPtr <IBaseFilter> m_pBDACapture;
	CComPtr <IBaseFilter> m_pBDAMpeg2Demux;
	CComPtr <IBaseFilter> m_pBDATIF;
	CComPtr <IBaseFilter> m_pBDASecTab;
	
	CComPtr <ITuningSpace> m_pTuningSpace;
	CComPtr <IScanningTuner> m_piTuner;
	CComPtr <IGuideData> m_piGuideData;
	CComPtr <IConnectionPoint> m_piConnectionPoint;
	CComPtr <IGraphBuilder> m_piGraphBuilder;
	CComPtr <IMediaControl> m_piMediaControl;

	Mpeg2DataParser m_mpeg2parser;

	BDACardCollection cardList;
	BDACard *m_pBDACard;

	DWORD m_dwAdviseCookie;
	DWORD m_rotEntry;

	ULONG m_ulRef;

	BOOL m_bServiceChanged;
	BOOL m_bProgramChanged;
	BOOL m_bScheduleEntryChanged;

	HANDLE m_hGuideDataChangedEvent;
	HANDLE m_hServiceChangedMutex;

	BOOL m_bScanning;
	BOOL m_bVerbose;

	long m_freq[256];
	long m_band[256];
	long m_Count;
};

#endif

