/**
 *	BDACardCollection.h
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

#ifndef BDACARD_H
#define BDACARD_H

#include "StdAfx.h"
#include "SystemDeviceEnumerator.h"
#include "LogMessage.h"
#include "XMLDocument.h"
#include "FilterGraphTools.h"

class BDACard : public LogMessageCaller
{
public:
	BDACard();
	virtual ~BDACard();

	virtual void SetLogCallback(LogMessageCallback *callback);

	HRESULT LoadFromXML(XMLElement *pElement);
	HRESULT SaveToXML(XMLElement *pElement);

	DirectShowSystemDevice tunerDevice;
	DirectShowSystemDevice demodDevice;
	DirectShowSystemDevice captureDevice;

	HRESULT AddFilters(IGraphBuilder* piGraphBuilder);
	HRESULT Connect(IBaseFilter* pSource);
	HRESULT GetCapturePin(IPin** pCapturePin);

	HRESULT RemoveFilters();

	HRESULT GetSignalStatistics(BOOL &locked, BOOL &present, long &strength, long &quality);

	BOOL bActive;
	BOOL bNew;
	long nDetected;
	int index;

private:
	CComPtr <IGraphBuilder> m_piGraphBuilder;

	CComPtr <IBaseFilter> m_pBDATuner;
	CComPtr <IBaseFilter> m_pBDADemod;
	CComPtr <IBaseFilter> m_pBDACapture;

	CComPtr <IPin> m_pCapturePin;

	FilterGraphTools graphTools;
};

#endif
