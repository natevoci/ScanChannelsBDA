/**
 *	FilterGraphTools.h
 *	Copyright (C) 2003-2004 Nate
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

#ifndef FILTERGRAPHTOOLS_H
#define FILTERGRAPHTOOLS_H

//#pragma warning (disable : 4312)
//#pragma warning (disable : 4995)

#include <dshow.h>
#include <bdatif.h>
//#include <ObjBase.h>

#ifndef HELPER_RELEASE
#define HELPER_RELEASE(x) { if (x) x->Release(); x = NULL; }
#endif

typedef enum _RequestedPinDirection
{
	REQUESTED_PINDIR_INPUT    = PINDIR_INPUT,
	REQUESTED_PINDIR_OUTPUT   = PINDIR_OUTPUT,
	REQUESTED_PINDIR_ANY      = PINDIR_OUTPUT + 1
} REQUESTED_PIN_DIRECTION;

HRESULT AddFilter(IGraphBuilder* piGraphBuilder, REFCLSID rclsid, IBaseFilter* &pFilter, LPCWSTR pName, BOOL bSilent = FALSE);
HRESULT AddFilterByName(IGraphBuilder* piGraphBuilder, IBaseFilter* &pFilter, CLSID clsidDeviceClass, LPCWSTR friendlyName);
HRESULT AddFilterByDevicePath(IGraphBuilder* piGraphBuilder, IBaseFilter* &pFilter, LPCWSTR pDevicePath, LPCWSTR pName);

HRESULT EnumPins(IBaseFilter* source);

HRESULT FindPin(IBaseFilter* source, LPCWSTR Id, IPin **pin, REQUESTED_PIN_DIRECTION eRequestedPinDir = REQUESTED_PINDIR_ANY);
HRESULT FindPinByMediaType(IBaseFilter* source, GUID majortype, GUID subtype, IPin **pin, REQUESTED_PIN_DIRECTION eRequestedPinDir = REQUESTED_PINDIR_ANY);
HRESULT FindFirstFreePin(IBaseFilter* source, PIN_DIRECTION pinDirection, IPin **pin);

HRESULT FindFilter(IGraphBuilder* piGraphBuilder, LPCWSTR Id, IBaseFilter **filter);
HRESULT FindFilter(IGraphBuilder* piGraphBuilder, CLSID rclsid, IBaseFilter **filter);

HRESULT ConnectPins   (IGraphBuilder* piGraphBuilder, IBaseFilter* source, LPCWSTR sourcePinName, IBaseFilter* dest, LPCWSTR destPinName);
HRESULT ConnectFilters(IGraphBuilder* piGraphBuilder, IBaseFilter* pFilterUpstream, IBaseFilter* pFilterDownstream);
HRESULT RenderPin     (IGraphBuilder* piGraphBuilder, IBaseFilter* source, LPCWSTR pinName);

HRESULT DisconnectAllPins(IGraphBuilder* piGraphBuilder);
HRESULT RemoveAllFilters(IGraphBuilder* piGraphBuilder);

HRESULT GetOverlayMixer(IGraphBuilder* piGraphBuilder, IBaseFilter **filter);
HRESULT GetOverlayMixerInputPin(IGraphBuilder* piGraphBuilder, LPCWSTR pinName, IPin **pin);

ULONG HelperRelease(IUnknown* piUnknown);
ULONG HelperFullRelease(IUnknown* piUnknown);
HRESULT AddToRot(IUnknown *pUnkGraph, DWORD *pdwRegister);
void RemoveFromRot(DWORD pdwRegister);

//BDA functions
HRESULT InitDVBTTuningSpace(CComPtr<ITuningSpace> &piTuningSpace);
HRESULT SubmitDVBTTuneRequest(CComPtr<ITuningSpace> piTuningSpace, CComPtr<ITuneRequest> &pExTuneRequest, long frequency, long bandwidth);

#endif
